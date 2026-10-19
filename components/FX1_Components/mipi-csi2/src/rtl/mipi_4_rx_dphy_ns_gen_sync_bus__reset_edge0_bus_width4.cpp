//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge0_bus_width4
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge0_bus_width4.h"

mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge0_bus_width4::mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge0_bus_width4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    tie0__drv.o(tie0_);
    SC_METHOD(assign_int_rst);
    sensitive << rst;
    SC_METHOD(assign_data_stable);
    sensitive << data_sync << data_sync2;
    U1.reset(new mipi_4_rx_dphy_ns_DWbb_bcm41_scan__WIDTH4_RST_VAL0("U1"));
    U1->clk_d(clk_inv);
    U1->rst_d_n(int_rst);
    U1->data_s(data_in);
    U1->data_d(data_sync);
    U1->scanmode(tie0_);
    SC_METHOD(proc_always_813);
    sensitive << clk_inv.pos() << int_rst.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_823);
    sensitive << clk.pos() << int_rst.negedge_event();
    dont_initialize();
}

void mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge0_bus_width4::assign_int_rst() {
    vl::wr(int_rst, vl::mux(vl::logic<32>(0x0ULL).truth(), ~vl::rd(rst), vl::rd(rst)));
}

void mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge0_bus_width4::assign_data_stable() {
    vl::wr(data_stable, vl::mux(vl::eq(vl::rd(data_sync), vl::rd(data_sync2)), vl::logic<1>(0x1ULL), vl::logic<1>(0x0ULL)));
}

void mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge0_bus_width4::proc_always_813() {
    if (vl::rd(int_rst).truth_val() == 0) {
        vl::wr(data_sync2, vl::logic<4>(0x0ULL));
    } else {
        vl::wr(data_sync2, vl::rd(data_sync));
    }
}

void mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge0_bus_width4::proc_always_823() {
    if (vl::rd(int_rst).truth_val() == 0) {
        vl::wr(data_out, vl::logic<4>(0x0ULL));
    } else if (vl::rd(data_stable).test()) {
        vl::wr(data_out, vl::rd(data_sync2));
    } else {
        vl::wr(data_out, vl::rd(data_out));
    }
}
