//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_sc_tie_hilow_lvt
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_sc_tie_hilow_lvt.h"

mipi_4_rx_dphy_ns_sc_tie_hilow_lvt::mipi_4_rx_dphy_ns_sc_tie_hilow_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_soft_low);
    sensitive << gd;
    SC_METHOD(assign_soft_high);
    sensitive << vp;
    inv_high.reset(new mipi_4_rx_dphy_ns_sc_invx8_lvt("inv_high"));
    inv_high->a(soft_low);
    inv_high->z(tie_high);
    inv_high->gd(gd);
    inv_high->vp(vp);
    inv_low.reset(new mipi_4_rx_dphy_ns_sc_invx8_lvt("inv_low"));
    inv_low->a(soft_high);
    inv_low->z(tie_low);
    inv_low->gd(gd);
    inv_low->vp(vp);
}

void mipi_4_rx_dphy_ns_sc_tie_hilow_lvt::assign_soft_low() {
    vl::wr(soft_low, vl::rd(gd));
}

void mipi_4_rx_dphy_ns_sc_tie_hilow_lvt::assign_soft_high() {
    vl::wr(soft_high, vl::rd(vp));
}
