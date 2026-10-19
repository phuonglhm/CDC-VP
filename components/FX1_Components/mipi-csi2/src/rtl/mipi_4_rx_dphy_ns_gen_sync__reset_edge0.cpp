//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_gen_sync__reset_edge0
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_gen_sync__reset_edge0.h"

mipi_4_rx_dphy_ns_gen_sync__reset_edge0::mipi_4_rx_dphy_ns_gen_sync__reset_edge0(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_int_clk);
    sensitive << clk;
    SC_METHOD(assign_int_rst);
    sensitive << rst;
    U1.reset(new mipi_4_rx_dphy_ns_DWbb_bcm41_scan__WIDTH1_RST_VAL0("U1"));
    U1->clk_d(int_clk);
    U1->rst_d_n(int_rst);
    U1->data_s(data_in);
    U1->data_d(data_out);
    U1->scanmode(scanmode);
    SC_METHOD(proc_always_712);
    sensitive << int_clk.posedge_event() << int_rst.negedge_event();
    dont_initialize();
}

void mipi_4_rx_dphy_ns_gen_sync__reset_edge0::assign_int_clk() {
    vl::wr(int_clk, vl::mux(vl::logic<32>(0x1ULL).truth(), vl::rd(clk), ~vl::rd(clk)));
}

void mipi_4_rx_dphy_ns_gen_sync__reset_edge0::assign_int_rst() {
    vl::wr(int_rst, vl::mux(vl::logic<32>(0x0ULL).truth(), ~vl::rd(rst), vl::rd(rst)));
}

void mipi_4_rx_dphy_ns_gen_sync__reset_edge0::proc_always_712() {
    if (vl::rd(int_rst).truth_val() == 0) {
        vl::wr(data_out_dly, vl::logic<1>(0x0ULL));
    } else {
        vl::wr(data_out_dly, vl::rd(data_out));
    }
}
