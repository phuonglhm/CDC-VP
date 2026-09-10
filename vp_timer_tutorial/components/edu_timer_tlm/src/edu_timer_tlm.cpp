#include "edu_timer_tlm.h"

#include <cstring>

namespace tutorial {

edu_timer_tlm::edu_timer_tlm(sc_core::sc_module_name name,
                             sc_core::sc_time tick_period,
                             sc_core::sc_time access_latency)
    : sc_core::sc_module(name)
    , socket("socket")
    , reset_n("reset_n")
    , irq("irq")
    , tick_period_(tick_period)
    , access_latency_(access_latency)
{
    socket.register_b_transport(this, &edu_timer_tlm::b_transport);
    socket.register_transport_dbg(this, &edu_timer_tlm::transport_dbg);
    socket.register_get_direct_mem_ptr(this, &edu_timer_tlm::get_direct_mem_ptr);
    irq.initialize(false);
    SC_THREAD(timer_thread);
    SC_METHOD(drive_irq);
    sensitive << irq_changed_;
    dont_initialize();
}

std::uint32_t edu_timer_tlm::byte_mask(const tlm::tlm_generic_payload& trans)
{
    const unsigned char* enables = trans.get_byte_enable_ptr();
    if (enables == nullptr) {
        return 0xFFFF'FFFFu;
    }

    const unsigned enable_len = trans.get_byte_enable_length();
    if (enable_len == 0) {
        // access() rejects this payload before byte_mask() runs; the guard
        // keeps the helper safe if it is reused elsewhere.
        return 0;
    }
    std::uint32_t mask = 0;
    for (unsigned byte = 0; byte < 4; ++byte) {
        if (enables[byte % enable_len] != 0) {
            mask |= 0xFFu << (byte * 8u);
        }
    }
    return mask;
}

void edu_timer_tlm::copy_read_data(tlm::tlm_generic_payload& trans,
                                   std::uint32_t value)
{
    unsigned char bytes[4]{};
    std::memcpy(bytes, &value, sizeof(value));

    unsigned char* data = trans.get_data_ptr();
    const unsigned char* enables = trans.get_byte_enable_ptr();
    if (enables == nullptr) {
        std::memcpy(data, bytes, sizeof(bytes));
        return;
    }

    const unsigned enable_len = trans.get_byte_enable_length();
    if (enable_len == 0) {
        return;
    }
    for (unsigned byte = 0; byte < 4; ++byte) {
        if (enables[byte % enable_len] != 0) {
            data[byte] = bytes[byte];
        }
    }
}

tlm::tlm_response_status
edu_timer_tlm::access(tlm::tlm_generic_payload& trans)
{
    const std::uint64_t address = trans.get_address();
    const unsigned length = trans.get_data_length();
    const unsigned width = trans.get_streaming_width();

    if (trans.get_data_ptr() == nullptr || length != 4) {
        return tlm::TLM_GENERIC_ERROR_RESPONSE;
    }
    if ((address & 0x3u) != 0) {
        return tlm::TLM_ADDRESS_ERROR_RESPONSE;
    }
    if (width != 0 && width < length) {
        return tlm::TLM_BURST_ERROR_RESPONSE;
    }
    if (trans.get_byte_enable_ptr() != nullptr &&
        trans.get_byte_enable_length() == 0) {
        return tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE;
    }

    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        std::uint32_t result = 0;
        switch (address) {
        case kCtrlOffset:   result = ctrl_; break;
        case kLoadOffset:   result = load_; break;
        case kValueOffset:  result = value_; break;
        case kStatusOffset: result = status_; break;
        case kIdOffset:     result = kIdValue; break;
        default: return tlm::TLM_ADDRESS_ERROR_RESPONSE;
        }
        copy_read_data(trans, result);
        return tlm::TLM_OK_RESPONSE;
    }

    if (trans.get_command() != tlm::TLM_WRITE_COMMAND) {
        return tlm::TLM_COMMAND_ERROR_RESPONSE;
    }

    std::uint32_t incoming = 0;
    std::memcpy(&incoming, trans.get_data_ptr(), sizeof(incoming));
    const std::uint32_t mask = byte_mask(trans);

    switch (address) {
    case kCtrlOffset:
        ctrl_ = ((ctrl_ & ~mask) | (incoming & mask)) &
                (kCtrlEnable | kCtrlPeriodic | kCtrlIrqEnable);
        state_changed_.notify(sc_core::SC_ZERO_TIME);
        update_irq();
        return tlm::TLM_OK_RESPONSE;

    case kLoadOffset:
        load_ = (load_ & ~mask) | (incoming & mask);
        value_ = load_;
        state_changed_.notify(sc_core::SC_ZERO_TIME);
        return tlm::TLM_OK_RESPONSE;

    case kStatusOffset:
        // STATUS is W1C.  Disabled byte lanes cannot clear state.
        status_ &= ~(incoming & mask & kStatusPending);
        update_irq();
        state_changed_.notify(sc_core::SC_ZERO_TIME);
        return tlm::TLM_OK_RESPONSE;

    case kValueOffset:
    case kIdOffset:
        return tlm::TLM_COMMAND_ERROR_RESPONSE;

    default:
        return tlm::TLM_ADDRESS_ERROR_RESPONSE;
    }
}

void edu_timer_tlm::b_transport(tlm::tlm_generic_payload& trans,
                                sc_core::sc_time& delay)
{
    // Loosely-timed convention: annotate time; the initiator decides when to
    // synchronize.  Do not call wait() inside this target callback.
    delay += access_latency_;
    trans.set_dmi_allowed(false);
    trans.set_response_status(access(trans));
}

unsigned int edu_timer_tlm::transport_dbg(tlm::tlm_generic_payload& trans)
{
    const tlm::tlm_response_status response = access(trans);
    trans.set_response_status(response);
    return response == tlm::TLM_OK_RESPONSE ? trans.get_data_length() : 0;
}

bool edu_timer_tlm::get_direct_mem_ptr(tlm::tlm_generic_payload&,
                                       tlm::tlm_dmi&)
{
    // Deliberate policy: MMIO registers have read/write side effects and
    // time-dependent VALUE/STATUS state.  Granting raw pointer access would
    // bypass those semantics.  DMI belongs on RAM/ROM in this platform.
    return false;
}

void edu_timer_tlm::reset_registers()
{
    ctrl_ = 0;
    load_ = 0;
    value_ = 0;
    status_ = 0;
    update_irq();
}

void edu_timer_tlm::update_irq()
{
    // b_transport executes in the initiator's process context.  Writing the
    // port directly here and also from timer_thread would make SystemC report
    // multiple writers.  Funnel all physical pin writes through drive_irq().
    irq_changed_.notify(sc_core::SC_ZERO_TIME);
}

void edu_timer_tlm::drive_irq()
{
    irq.write((status_ & kStatusPending) != 0 &&
              (ctrl_ & kCtrlIrqEnable) != 0);
}

void edu_timer_tlm::timer_thread()
{
    // The countdown runs on an absolute tick deadline rather than on a plain
    // wait(tick_period_).  A software access notifies state_changed_, which
    // ends the wait early; treating that early return as a tick expiry would
    // let every driver access (an IRQ acknowledge, for instance) steal one
    // tick from the countdown and silently shorten the timer period.
    sc_core::sc_time next_tick = sc_core::SC_ZERO_TIME;
    bool armed = false;

    while (true) {
        if (!reset_n.read()) {
            reset_registers();
            armed = false;
            wait(reset_n.posedge_event());
            continue;
        }

        if ((ctrl_ & kCtrlEnable) == 0) {
            armed = false;
            wait(state_changed_ | reset_n.negedge_event());
            continue;
        }

        if (!armed) {
            // Disabled -> enabled starts a fresh tick period.
            next_tick = sc_core::sc_time_stamp() + tick_period_;
            armed = true;
        }

        const sc_core::sc_time now = sc_core::sc_time_stamp();
        wait(next_tick > now ? next_tick - now : sc_core::SC_ZERO_TIME,
             state_changed_ | reset_n.negedge_event());

        if (!reset_n.read()) {
            reset_registers();
            armed = false;
            continue;
        }
        if ((ctrl_ & kCtrlEnable) == 0) {
            armed = false;
            continue;
        }
        if (sc_core::sc_time_stamp() < next_tick) {
            // Woken by a register write, not by the tick.  Keep the deadline
            // so the period stays exact; writing LOAD reloads VALUE but does
            // not re-phase the tick.
            continue;
        }

        next_tick += tick_period_;

        if (value_ > 0) {
            --value_;
        }
        if (value_ == 0) {
            status_ |= kStatusPending;
            update_irq();

            // PERIODIC with LOAD == 0 has no meaningful period, so it falls
            // back to one-shot and clears ENABLE.
            if ((ctrl_ & kCtrlPeriodic) != 0 && load_ != 0) {
                value_ = load_;
            } else {
                ctrl_ &= ~kCtrlEnable;
            }
        }
    }
}

} // namespace tutorial
