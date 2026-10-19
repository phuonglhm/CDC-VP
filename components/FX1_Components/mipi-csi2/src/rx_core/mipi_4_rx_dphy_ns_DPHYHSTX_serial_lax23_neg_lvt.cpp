//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSTX_serial_lax23_neg_lvt
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSTX_serial_lax23_neg_lvt.h"

mipi_4_rx_dphy_ns_DPHYHSTX_serial_lax23_neg_lvt::mipi_4_rx_dphy_ns_DPHYHSTX_serial_lax23_neg_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vp << gd;
    SC_METHOD(assign_qb);
    sensitive << n1;
    SC_METHOD(proc_always_158966);
    sensitive << supply_ok << ck << n1 << d;
}

void mipi_4_rx_dphy_ns_DPHYHSTX_serial_lax23_neg_lvt::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHYHSTX_serial_lax23_neg_lvt::assign_qb() {
    vl::wr(qb, ~vl::rd(n1));
}

void mipi_4_rx_dphy_ns_DPHYHSTX_serial_lax23_neg_lvt::proc_always_158966() {
    if (vl::rd(supply_ok).test()) {
        if (vl::rd(ck).test()) {
            vl::wr(n1, vl::rd(n1));
        } else {
            vl::wr(n1, vl::rd(d));
        }
    } else {
        vl::wr(n1, vl::logic<1>());
    }
}
