#include "test.h"
#include "watchdog.h"

#include <systemc>

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    const sc_core::sc_time tick_period(10, sc_core::SC_NS);

    watchdog dut("watchdog", tick_period);
    Testbench tb("testbench", tick_period);

    sc_core::sc_signal<bool> irq_sig("irq_sig");
    sc_core::sc_signal<bool> reset_sig("reset_sig");

    tb.initiator_socket.bind(dut.target_socket);
    dut.irq(irq_sig);
    dut.reset_o(reset_sig);
    tb.irq(irq_sig);
    tb.reset_i(reset_sig);

    sc_core::sc_trace_file* vcd = sc_core::sc_create_vcd_trace_file("watchdog_wave");
    vcd->set_time_unit(1, sc_core::SC_NS);
    sc_core::sc_trace(vcd, irq_sig, "watchdog.irq");
    sc_core::sc_trace(vcd, reset_sig, "watchdog.reset");
    dut.trace(vcd);

    sc_core::sc_start();

    sc_core::sc_close_vcd_trace_file(vcd);

    return tb.passed() ? 0 : 1;
}
