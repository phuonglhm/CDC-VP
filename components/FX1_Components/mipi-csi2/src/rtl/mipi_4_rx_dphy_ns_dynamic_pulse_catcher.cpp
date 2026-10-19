//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_dynamic_pulse_catcher
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_dynamic_pulse_catcher.h"

mipi_4_rx_dphy_ns_dynamic_pulse_catcher::mipi_4_rx_dphy_ns_dynamic_pulse_catcher(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_int_clk);
    sensitive << clk;
    SC_METHOD(assign_int_rst);
    sensitive << rst;
    U1.reset(new mipi_4_rx_dphy_ns_DWbb_bcm35("U1"));
    U1->clk_s(pulse_in);
    U1->clk_d(int_clk);
    U1->rst_d_n(int_rst);
    U1->event_d(pulse_out);
}

// reg pulse_sync2;
void mipi_4_rx_dphy_ns_dynamic_pulse_catcher::assign_int_clk() {
    vl::wr(int_clk, vl::mux(vl::logic<32>(0x1ULL).truth(), vl::rd(clk), ~vl::rd(clk)));
}

void mipi_4_rx_dphy_ns_dynamic_pulse_catcher::assign_int_rst() {
    vl::wr(int_rst, vl::mux(vl::logic<32>(0x1ULL).truth(), ~vl::rd(rst), vl::rd(rst)));
}
