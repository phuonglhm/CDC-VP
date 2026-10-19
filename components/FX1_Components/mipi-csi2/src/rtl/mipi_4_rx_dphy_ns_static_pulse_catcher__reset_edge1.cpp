//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge1
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge1.h"

mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge1::mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_int_clk);
    sensitive << clk;
    SC_METHOD(assign_int_rst);
    sensitive << rst;
    U1.reset(new mipi_4_rx_dphy_ns_DWbb_bcm41("U1"));
    U1->clk_d(int_clk);
    U1->rst_d_n(int_rst);
    U1->data_s(pulse_in_ff);
    U1->data_d(pulse_out);
    SC_METHOD(proc_always_2421);
    sensitive << clk_cap.pos() << pulse_in.pos() << rst_cap.neg();
    dont_initialize();
}

void mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge1::assign_int_clk() {
    vl::wr(int_clk, vl::mux(vl::logic<32>(0x1ULL).truth(), vl::rd(clk), ~vl::rd(clk)));
}

void mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge1::assign_int_rst() {
    vl::wr(int_rst, vl::mux(vl::logic<32>(0x1ULL).truth(), ~vl::rd(rst), vl::rd(rst)));
}

// Changed pulse_in_ff simulation model to be set/reset level sensitive
// The block is now both edge sensitive to evaluate the effect each signal (set/reset)
// produces when releasing the other
void mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge1::proc_always_2421() {
    if (vl::rd(rst_cap).truth_val() == 0) {
        vl::wr(pulse_in_ff, vl::logic<1>(0x0ULL));
    } else if (vl::rd(pulse_in).test()) {
        vl::wr(pulse_in_ff, vl::logic<1>(0x1ULL));
    } else {
        vl::wr(pulse_in_ff, vl::logic<1>(0x0ULL));
    }
}
