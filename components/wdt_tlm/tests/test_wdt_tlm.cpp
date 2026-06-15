#include <systemc>
#include <wdt_tlm.h>

int sc_main(int argc, char* argv[])
{
    sc_core::sc_time tick_period(10, sc_core::SC_NS);
    cdc::components::wdt_tlm dut("wdt_tlm", tick_period);

    sc_core::sc_signal<bool> irq_sig("irq_sig");
    sc_core::sc_signal<bool> reset_sig("reset_sig");

    dut.irq(irq_sig);
    dut.reset_o(reset_sig);

    sc_core::sc_start(100, sc_core::SC_NS);

    return 0;
}
