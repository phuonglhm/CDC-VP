//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYLPTX_core_keys
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYLPTX_core_keys.h"

mipi_4_rx_dphy_ns_DPHYLPTX_core_keys::mipi_4_rx_dphy_ns_DPHYLPTX_core_keys(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vph << gd;
    SC_METHOD(proc_always_159897);
    sensitive << supply_ok << C << D << A;
}

void mipi_4_rx_dphy_ns_DPHYLPTX_core_keys::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vph), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHYLPTX_core_keys::proc_always_159897() {
    if (vl::rd(supply_ok).test()) {
        if (vl::land(vl::ceq(vl::rd(C), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(D), vl::logic<1>(0x1ULL))).test()) {
            vl::wr(Z, vl::logic<1>(0x0ULL));
        } else if (vl::land(vl::ceq(vl::rd(A), vl::logic<1>(0x0ULL)), vl::ceq(vl::rd(D), vl::logic<1>(0x0ULL))).test()) {
            vl::wr(Z, vl::logic<1>(0x1ULL));
        } else {
            vl::wr(Z, vl::logic<1>());
        }
    } else {
        vl::wr(Z, vl::logic<1>::all_z());
    }
}
