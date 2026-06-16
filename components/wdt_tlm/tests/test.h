#ifndef TEST_H
#define TEST_H

#include <cstdint>
#include <string>

#include <systemc>
#include <tlm_utils/simple_initiator_socket.h>

class Testbench : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<Testbench> initiator_socket;
    sc_core::sc_in<bool> reset_n;
    sc_core::sc_in<bool> irq;
    sc_core::sc_in<bool> reset_i;

    SC_HAS_PROCESS(Testbench);

    Testbench(sc_core::sc_module_name name, sc_core::sc_time tick_period);

    bool passed() const;

private:
    sc_core::sc_time m_tick_period;
    unsigned int m_errors;

    void run();
    void wait_ticks(unsigned int ticks);
    void settle_outputs();

    uint32_t read32(uint32_t addr);
    void write32(uint32_t addr, uint32_t data);

    void expect_eq(const std::string& label, uint32_t actual, uint32_t expected);
    void expect_signal(const std::string& label, bool actual, bool expected);
    void print_test_result(const std::string& name, unsigned int errors_before) const;
};

#endif
