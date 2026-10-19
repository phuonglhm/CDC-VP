//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DWbb_bcm41
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DWbb_bcm41.h"

mipi_4_rx_dphy_ns_DWbb_bcm41::mipi_4_rx_dphy_ns_DWbb_bcm41(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_data_s_int);
    sensitive << data_s;
    SC_METHOD(assign_data_d);
    sensitive << data_d_int;
    U_SYNC.reset(new mipi_4_rx_dphy_ns_DWbb_bcm21("U_SYNC"));
    U_SYNC->clk_d(clk_d);
    U_SYNC->rst_d_n(rst_d_n);
    U_SYNC->data_s(data_s_int);
    U_SYNC->data_d(data_d_int);
}

void mipi_4_rx_dphy_ns_DWbb_bcm41::assign_data_s_int() {
    vl::wr(data_s_int, vl::rd(data_s) ^ vl::logic<1>(0x0ULL));
}

void mipi_4_rx_dphy_ns_DWbb_bcm41::assign_data_d() {
    vl::wr(data_d, vl::rd(data_d_int) ^ vl::logic<1>(0x0ULL));
}
