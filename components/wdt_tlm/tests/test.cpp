#include "test.h"

#include "watchdog.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {

std::string hex32(uint32_t value)
{
    std::ostringstream os;
    os << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
    return os.str();
}

} // namespace

Testbench::Testbench(sc_core::sc_module_name name, sc_core::sc_time tick_period)
    : sc_core::sc_module(name)
    , initiator_socket("initiator_socket")
    , irq("irq")
    , reset_i("reset_i")
    , m_tick_period(tick_period)
    , m_errors(0)
{
    SC_THREAD(run);
}

bool Testbench::passed() const
{
    return m_errors == 0;
}

void Testbench::run()
{
    wait(sc_core::SC_ZERO_TIME);

    std::cout << "\n[TB] SP805 Watchdog LT functional tests begin\n";
    std::cout << "[TB] tick_period = " << m_tick_period << "\n\n";

    unsigned int errors_before = m_errors;
    std::cout << "[TB] Test 1: load=5, INTEN -> first timeout asserts IRQ\n";
    write32(watchdog::WDOG_LOAD, 5);
    write32(watchdog::WDOG_CONTROL, watchdog::CTRL_INTEN);
    wait_ticks(6);
    settle_outputs();
    expect_signal("IRQ after first timeout", irq.read(), true);
    expect_signal("RESET after first timeout", reset_i.read(), false);
    expect_eq("WdogRIS", read32(watchdog::WDOG_RIS), 1);
    expect_eq("WdogMIS", read32(watchdog::WDOG_MIS), 1);
    print_test_result("Test 1", errors_before);

    errors_before = m_errors;
    std::cout << "\n[TB] Test 2: WdogIntClr clears IRQ and reloads counter\n";
    write32(watchdog::WDOG_INTCLR, 0xABCD1234u);
    settle_outputs();
    expect_signal("IRQ after WdogIntClr", irq.read(), false);
    expect_eq("WdogRIS after WdogIntClr", read32(watchdog::WDOG_RIS), 0);
    expect_eq("WdogMIS after WdogIntClr", read32(watchdog::WDOG_MIS), 0);
    expect_eq("WdogValue after WdogIntClr", read32(watchdog::WDOG_VALUE), 5);
    print_test_result("Test 2", errors_before);

    errors_before = m_errors;
    std::cout << "\n[TB] Test 3: INTEN alone asserts IRQ; RESEN only affects the next unserviced timeout\n";
    wait_ticks(6);
    settle_outputs();
    expect_signal("IRQ after first timeout with INTEN", irq.read(), true);
    expect_signal("RESET after first timeout with INTEN", reset_i.read(), false);
    expect_eq("WdogRIS after first timeout with INTEN", read32(watchdog::WDOG_RIS), 1);
    expect_eq("WdogMIS after first timeout with INTEN", read32(watchdog::WDOG_MIS), 1);

    write32(watchdog::WDOG_CONTROL, watchdog::CTRL_INTEN | watchdog::CTRL_RESEN);
    settle_outputs();
    expect_signal("IRQ after enabling RESEN", irq.read(), true);
    expect_signal("RESET after enabling RESEN", reset_i.read(), false);
    expect_eq("WdogRIS after enabling RESEN", read32(watchdog::WDOG_RIS), 1);
    expect_eq("WdogMIS after enabling RESEN", read32(watchdog::WDOG_MIS), 1);

    wait_ticks(6);
    settle_outputs();
    expect_signal("RESET after second timeout", reset_i.read(), true);
    const uint32_t stopped_value = read32(watchdog::WDOG_VALUE);
    wait_ticks(2);
    settle_outputs();
    expect_eq("counter stopped after RESET", read32(watchdog::WDOG_VALUE), stopped_value);
    print_test_result("Test 3", errors_before);

    errors_before = m_errors;
    std::cout << "\n[TB] Test 4: WdogLock blocks writes except unlock key\n";
    write32(watchdog::WDOG_LOAD, 0x22);
    expect_eq("WdogLoad before lock", read32(watchdog::WDOG_LOAD), 0x22);
    write32(watchdog::WDOG_LOCK, 0);
    expect_eq("WdogLock locked status", read32(watchdog::WDOG_LOCK), 1);
    write32(watchdog::WDOG_LOAD, 0x12345678u);
    expect_eq("WdogLoad write ignored while locked", read32(watchdog::WDOG_LOAD), 0x22);
    write32(watchdog::WDOG_LOCK, watchdog::LOCK_UNLOCK_VALUE);
    expect_eq("WdogLock unlocked status", read32(watchdog::WDOG_LOCK), 0);
    write32(watchdog::WDOG_LOAD, 0x12345678u);
    expect_eq("WdogLoad write accepted after unlock", read32(watchdog::WDOG_LOAD), 0x12345678u);
    print_test_result("Test 4", errors_before);

    errors_before = m_errors;
    std::cout << "\n[TB] Test 5: ID registers read expected SP805 values\n";
    expect_eq("WdogPeriphID0", read32(watchdog::WDOG_PERIPHID0), watchdog::PERIPHID0_VALUE);
    expect_eq("WdogPeriphID1", read32(watchdog::WDOG_PERIPHID1), watchdog::PERIPHID1_VALUE);
    expect_eq("WdogPeriphID2", read32(watchdog::WDOG_PERIPHID2), watchdog::PERIPHID2_VALUE);
    expect_eq("WdogPeriphID3", read32(watchdog::WDOG_PERIPHID3), watchdog::PERIPHID3_VALUE);
    expect_eq("WdogPCellID0", read32(watchdog::WDOG_PCELLID0), watchdog::PCELLID0_VALUE);
    expect_eq("WdogPCellID1", read32(watchdog::WDOG_PCELLID1), watchdog::PCELLID1_VALUE);
    expect_eq("WdogPCellID2", read32(watchdog::WDOG_PCELLID2), watchdog::PCELLID2_VALUE);
    expect_eq("WdogPCellID3", read32(watchdog::WDOG_PCELLID3), watchdog::PCELLID3_VALUE);
    print_test_result("Test 5", errors_before);

    std::cout << "\n[TB] Result: " << (passed() ? "PASS" : "FAIL")
              << " (" << m_errors << " error(s))\n";

    sc_core::sc_stop();
}

void Testbench::wait_ticks(unsigned int ticks)
{
    sc_core::sc_time duration = m_tick_period;
    duration *= static_cast<double>(ticks);
    wait(duration);
}

void Testbench::settle_outputs()
{
    wait(sc_core::SC_ZERO_TIME);
    wait(sc_core::SC_ZERO_TIME);
}

uint32_t Testbench::read32(uint32_t addr)
{
    uint32_t value = 0;
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    trans.set_data_length(sizeof(value));
    trans.set_streaming_width(sizeof(value));
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);
    wait(delay);

    if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
        ++m_errors;
        std::cout << "[TB][FAIL] read " << hex32(addr)
                  << " response=" << trans.get_response_string() << '\n';
    }

    std::cout << sc_core::sc_time_stamp() << " [TB] READ  "
              << hex32(addr) << " -> " << hex32(value) << '\n';
    return value;
}

void Testbench::write32(uint32_t addr, uint32_t data)
{
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(sizeof(data));
    trans.set_streaming_width(sizeof(data));
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);
    wait(delay);

    if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
        ++m_errors;
        std::cout << "[TB][FAIL] write " << hex32(addr)
                  << " response=" << trans.get_response_string() << '\n';
    }

    std::cout << sc_core::sc_time_stamp() << " [TB] WRITE "
              << hex32(addr) << " <= " << hex32(data) << '\n';
}

void Testbench::expect_eq(const std::string& label, uint32_t actual, uint32_t expected)
{
    if (actual != expected) {
        ++m_errors;
        std::cout << "[TB][FAIL] " << label << ": expected "
                  << hex32(expected) << ", got " << hex32(actual) << '\n';
        return;
    }

    std::cout << "[TB][PASS] " << label << " = " << hex32(actual) << '\n';
}

void Testbench::expect_signal(const std::string& label, bool actual, bool expected)
{
    if (actual != expected) {
        ++m_errors;
        std::cout << "[TB][FAIL] " << label << ": expected "
                  << expected << ", got " << actual << '\n';
        return;
    }

    std::cout << "[TB][PASS] " << label << " = " << actual << '\n';
}

void Testbench::print_test_result(const std::string& name, unsigned int errors_before) const
{
    std::cout << "[TB] " << name << ' '
              << (m_errors == errors_before ? "PASS" : "FAIL") << '\n';
}
