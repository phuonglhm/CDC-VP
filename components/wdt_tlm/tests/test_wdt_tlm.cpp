//Author: QuanNH107
//Verified by: HoangV11


#include <systemc>

#include "test.h"

#include <wdt_tlm.h>

int sc_main(int argc, char* argv[])
{
    sc_core::sc_time tick_period(10, sc_core::SC_NS);
    cdc::components::wdt_tlm dut("wdt_tlm", tick_period);
    Testbench tb("tb", tick_period);

    sc_core::sc_signal<bool> rst_n_sig("rst_n_sig");
    sc_core::sc_signal<bool> irq_sig("irq_sig");
    sc_core::sc_signal<bool> reset_sig("reset_sig");

    tb.initiator_socket.bind(dut.target_socket);
    tb.reset_n(rst_n_sig);
    tb.irq(irq_sig);
    tb.reset_i(reset_sig);

    dut.reset_n(rst_n_sig);
    dut.irq(irq_sig);
    dut.reset_o(reset_sig);

    rst_n_sig.write(false);
    sc_core::sc_start(10, sc_core::SC_NS);
    rst_n_sig.write(true);
    sc_core::sc_start();

    return tb.passed() ? 0 : 1;
}
