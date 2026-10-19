//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DWbb_bcm41_scan__WIDTH1_RST_VAL0
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DWbb_bcm41_scan__WIDTH1_RST_VAL0.h"

mipi_4_rx_dphy_ns_DWbb_bcm41_scan__WIDTH1_RST_VAL0::mipi_4_rx_dphy_ns_DWbb_bcm41_scan__WIDTH1_RST_VAL0(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_data_s_int);
    sensitive << data_s;
    SC_METHOD(assign_data_d);
    sensitive << data_d_int;
    U_SYNC.reset(new mipi_4_rx_dphy_ns_DWbb_bcm21_scan__WIDTH1("U_SYNC"));
    U_SYNC->clk_d(clk_d);
    U_SYNC->rst_d_n(rst_d_n);
    U_SYNC->data_s(data_s_int);
    U_SYNC->data_d(data_d_int);
    U_SYNC->scanmode(scanmode);
}

void mipi_4_rx_dphy_ns_DWbb_bcm41_scan__WIDTH1_RST_VAL0::assign_data_s_int() {
    vl::wr(data_s_int, vl::rd(data_s) ^ vl::logic<1>(0x0ULL));
}

void mipi_4_rx_dphy_ns_DWbb_bcm41_scan__WIDTH1_RST_VAL0::assign_data_d() {
    vl::wr(data_d, vl::rd(data_d_int) ^ vl::logic<1>(0x0ULL));
}
