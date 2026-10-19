//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_sc_syncx10_ar_lvt_speedup
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_DPHYPLL_sc_syncx10_ar_lvt_speedup.h"

mipi_pll_dphy_DPHYPLL_sc_syncx10_ar_lvt_speedup::mipi_pll_dphy_DPHYPLL_sc_syncx10_ar_lvt_speedup(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vp << gd;
    SC_METHOD(assign_qb);
    sensitive << n3;
    SC_METHOD(proc_always_2375);
    sensitive << ck << d << reset_b << supply_ok;
    SC_METHOD(proc_always_2387);
    sensitive << ck << n1 << reset_b << supply_ok;
}

void mipi_pll_dphy_DPHYPLL_sc_syncx10_ar_lvt_speedup::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_pll_dphy_DPHYPLL_sc_syncx10_ar_lvt_speedup::assign_qb() {
    vl::wr(qb, ~vl::rd(n3));
}

void mipi_pll_dphy_DPHYPLL_sc_syncx10_ar_lvt_speedup::proc_always_2375() {
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x0ULL)).test()) {
        vl::wr(n1, vl::random().ext<1>());
    } else {
        if (vl::rd(reset_b).truth_val() == 0) {
            vl::wr(n1, vl::logic<1>(0x0ULL));
        } else if (vl::rd(ck).truth_val() == 0) {
            vl::wr(n1, ~vl::rd(d));
        }
    }
}

void mipi_pll_dphy_DPHYPLL_sc_syncx10_ar_lvt_speedup::proc_always_2387() {
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x0ULL)).test()) {
        vl::wr(n3, vl::random().ext<1>());
    } else {
        if (vl::rd(reset_b).truth_val() == 0) {
            vl::wr(n3, vl::logic<1>(0x1ULL));
        } else if (vl::rd(ck).test()) {
            vl::wr(n3, ~vl::rd(n1));
        }
    }
}
