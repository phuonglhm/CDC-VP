//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_gen_sync_rst
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_gen_sync_rst.h"

mipi_4_rx_dphy_ns_gen_sync_rst::mipi_4_rx_dphy_ns_gen_sync_rst(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_int_clk);
    sensitive << clk;
    SC_METHOD(assign_int_rst);
    sensitive << rst;
    SC_METHOD(assign_srst);
    sensitive << reset_async_ff << syncrst;
    SC_METHOD(proc_always_909);
    sensitive << int_clk.posedge_event() << int_rst.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_928);
    sensitive << int_clk.posedge_event();
    dont_initialize();
}

void mipi_4_rx_dphy_ns_gen_sync_rst::assign_int_clk() {
    vl::wr(int_clk, vl::mux(vl::logic<32>(0x1ULL).truth(), vl::rd(clk), ~vl::rd(clk)));
}

void mipi_4_rx_dphy_ns_gen_sync_rst::assign_int_rst() {
    vl::wr(int_rst, vl::mux(vl::logic<32>(0x0ULL).truth(), ~vl::rd(rst), vl::rd(rst)));
}

void mipi_4_rx_dphy_ns_gen_sync_rst::assign_srst() {
    vl::wr(srst, vl::mux(vl::logic<32>(0x0ULL).truth(), vl::lor(vl::rd(reset_async_ff), vl::ror_(vl::rd(syncrst))), vl::land(vl::rd(reset_async_ff), vl::rand_(vl::rd(syncrst)))));
}

void mipi_4_rx_dphy_ns_gen_sync_rst::proc_always_909() {
    if (vl::rd(int_rst).truth_val() == 0) {
        vl::wr(reset_async_ff, vl::logic<1>(0x0ULL));
    } else {
        vl::wr(reset_async_ff, vl::logic<1>(0x1ULL));
    }
}

void mipi_4_rx_dphy_ns_gen_sync_rst::proc_always_928() {
    vl::wr(syncrst, vl::cat(vl::rd(syncrst).slice<2>(0), vl::rd(reset_async_ff)));
}
