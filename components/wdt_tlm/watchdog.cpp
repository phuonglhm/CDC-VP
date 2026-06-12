#include "watchdog.h"

#include <cstring>
#include <iomanip>
#include <iostream>

namespace {

uint32_t load_u32_le(const unsigned char* data)
{
    uint32_t value = 0;
    std::memcpy(&value, data, sizeof(value));
    return value;
}

void store_u32_le(unsigned char* data, uint32_t value)
{
    std::memcpy(data, &value, sizeof(value));
}

} // namespace

watchdog::watchdog(sc_core::sc_module_name name, sc_core::sc_time tick_period)
    : sc_core::sc_module(name)
    , target_socket("target_socket")
    , irq("irq")
    , reset_o("reset_o")
    , m_load(0xFFFFFFFFu)
    , m_counter(0xFFFFFFFFu)
    , m_control(0)
    , m_ris(0)
    , m_mis(0)
    , m_locked(false)
    , m_reset_asserted(false)
    , m_irq_level(false)
    , m_reset_level(false)
    , m_tick_period(tick_period)
{
    target_socket.register_b_transport(this, &watchdog::b_transport);

    SC_THREAD(counter_thread);
    SC_METHOD(drive_outputs);
    sensitive << m_output_changed;
}

void watchdog::trace(sc_core::sc_trace_file* tf) const
{
    if (tf == nullptr) {
        return;
    }

    sc_core::sc_trace(tf, m_load, "watchdog.wdog_load");
    sc_core::sc_trace(tf, m_counter, "watchdog.wdog_value");
    sc_core::sc_trace(tf, m_control, "watchdog.wdog_control");
    sc_core::sc_trace(tf, m_ris, "watchdog.wdog_ris");
    sc_core::sc_trace(tf, m_mis, "watchdog.wdog_mis");
    sc_core::sc_trace(tf, m_locked, "watchdog.locked");
    sc_core::sc_trace(tf, m_reset_asserted, "watchdog.reset_asserted");
    sc_core::sc_trace(tf, m_irq_level, "watchdog.irq_level");
    sc_core::sc_trace(tf, m_reset_level, "watchdog.reset_level");
}

void watchdog::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    (void)delay;

    if (trans.get_byte_enable_ptr() != nullptr) {
        trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        return;
    }

    if (trans.get_data_length() < sizeof(uint32_t)) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }

    const uint32_t addr = static_cast<uint32_t>(trans.get_address() & 0xFFFu);
    unsigned char* data = trans.get_data_ptr();

    if (data == nullptr) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    switch (trans.get_command()) {
    case tlm::TLM_READ_COMMAND:
        store_u32_le(data, read_reg(addr));
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        break;

    case tlm::TLM_WRITE_COMMAND:
        write_reg(addr, load_u32_le(data));
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        break;

    default:
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        break;
    }
}

uint32_t watchdog::read_reg(uint32_t addr)
{
    update_outputs();

    switch (addr) {
    case WDOG_LOAD:
        return m_load;
    case WDOG_VALUE:
        return m_counter;
    case WDOG_CONTROL:
        return m_control;
    case WDOG_INTCLR:
        return 0;
    case WDOG_RIS:
        return m_ris & RIS_ASSERTED;
    case WDOG_MIS:
        return m_mis & RIS_ASSERTED;
    case WDOG_LOCK:
        return m_locked ? 1u : 0u;
    case WDOG_PERIPHID0:
        return PERIPHID0_VALUE;
    case WDOG_PERIPHID1:
        return PERIPHID1_VALUE;
    case WDOG_PERIPHID2:
        return PERIPHID2_VALUE;
    case WDOG_PERIPHID3:
        return PERIPHID3_VALUE;
    case WDOG_PCELLID0:
        return PCELLID0_VALUE;
    case WDOG_PCELLID1:
        return PCELLID1_VALUE;
    case WDOG_PCELLID2:
        return PCELLID2_VALUE;
    case WDOG_PCELLID3:
        return PCELLID3_VALUE;
    default:
        return 0;
    }
}

void watchdog::write_reg(uint32_t addr, uint32_t data)
{
    if (addr == WDOG_LOCK) {
        m_locked = (data != LOCK_UNLOCK_VALUE);
        std::cout << sc_core::sc_time_stamp() << " [WDOG] WdogLock <= 0x"
                  << std::hex << std::setw(8) << std::setfill('0') << data
                  << std::dec << " -> " << (m_locked ? "locked" : "unlocked")
                  << std::setfill(' ') << '\n';
        return;
    }

    if (m_locked) {
        std::cout << sc_core::sc_time_stamp() << " [WDOG] ignored write while locked, addr=0x"
                  << std::hex << std::setw(3) << std::setfill('0') << addr
                  << " data=0x" << std::setw(8) << data << std::dec
                  << std::setfill(' ') << '\n';
        return;
    }

    switch (addr) {
    case WDOG_LOAD:
        m_load = data;
        reload_counter();
        std::cout << sc_core::sc_time_stamp() << " [WDOG] WdogLoad <= "
                  << m_load << ", counter reloaded\n";
        break;

    case WDOG_CONTROL: {
        const bool was_enabled = interrupt_enabled();
        m_control = data & CONTROL_WRITABLE_MASK;

        if (!was_enabled && interrupt_enabled() && !m_reset_asserted) {
            reload_counter();
        }

        update_outputs();
        std::cout << sc_core::sc_time_stamp() << " [WDOG] WdogControl <= 0x"
                  << std::hex << m_control << std::dec
                  << " (INTEN=" << interrupt_enabled()
                  << ", RESEN=" << reset_enabled() << ")\n";
        break;
    }

    case WDOG_INTCLR:
        clear_interrupt();
        std::cout << sc_core::sc_time_stamp()
                  << " [WDOG] WdogIntClr write: IRQ cleared, counter reloaded\n";
        break;

    case WDOG_VALUE:
    case WDOG_RIS:
    case WDOG_MIS:
    case WDOG_PERIPHID0:
    case WDOG_PERIPHID1:
    case WDOG_PERIPHID2:
    case WDOG_PERIPHID3:
    case WDOG_PCELLID0:
    case WDOG_PCELLID1:
    case WDOG_PCELLID2:
    case WDOG_PCELLID3:
        std::cout << sc_core::sc_time_stamp() << " [WDOG] ignored write to read-only addr=0x"
                  << std::hex << addr << std::dec << '\n';
        break;

    default:
        std::cout << sc_core::sc_time_stamp() << " [WDOG] ignored write to unmapped addr=0x"
                  << std::hex << addr << std::dec << '\n';
        break;
    }
}

void watchdog::counter_thread()
{
    while (true) {
        wait(m_tick_period);

        if (!interrupt_enabled() || m_reset_asserted) {
            continue;
        }

        if (m_counter == 0) {
            handle_timeout();
        } else {
            --m_counter;
        }
    }
}

void watchdog::handle_timeout()
{
    if ((m_ris & RIS_ASSERTED) != 0) {
        if (reset_enabled()) {
            m_reset_asserted = true;
            update_outputs();
            std::cout << sc_core::sc_time_stamp()
                      << " [WDOG] second timeout: RESET asserted, counter stopped\n";
            return;
        }

        reload_counter();
        std::cout << sc_core::sc_time_stamp()
                  << " [WDOG] timeout while IRQ pending, RESEN=0: counter reloaded\n";
        return;
    }

    m_ris = RIS_ASSERTED;
    update_outputs();
    reload_counter();

    std::cout << sc_core::sc_time_stamp()
              << " [WDOG] first timeout: IRQ asserted, counter reloaded\n";
}

void watchdog::drive_outputs()
{
    irq.write(m_irq_level);
    reset_o.write(m_reset_level);
}

void watchdog::reload_counter()
{
    m_counter = m_load;
}

void watchdog::clear_interrupt()
{
    m_ris = 0;
    reload_counter();
    update_outputs();
}

void watchdog::update_outputs()
{
    m_mis = ((m_ris & RIS_ASSERTED) != 0 && interrupt_enabled()) ? RIS_ASSERTED : 0;
    m_irq_level = (m_mis & RIS_ASSERTED) != 0;
    m_reset_level = m_reset_asserted;
    m_output_changed.notify(sc_core::SC_ZERO_TIME);
}

bool watchdog::interrupt_enabled() const
{
    return (m_control & CTRL_INTEN) != 0;
}

bool watchdog::reset_enabled() const
{
    return (m_control & CTRL_RESEN) != 0;
}
