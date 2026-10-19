//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_sc_syncx10_ar_lvt
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_sc_syncx10_ar_lvt.h"

mipi_pll_dphy_sc_syncx10_ar_lvt::mipi_pll_dphy_sc_syncx10_ar_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vp << gd;
    SC_METHOD(assign_qb);
    sensitive << n3;
    SC_METHOD(proc_always_918);
    sensitive << ck << d << reset_b << supply_ok;
    SC_METHOD(proc_always_930);
    sensitive << ck << n1 << reset_b << supply_ok;
}

void mipi_pll_dphy_sc_syncx10_ar_lvt::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_pll_dphy_sc_syncx10_ar_lvt::assign_qb() {
    vl::wr(qb, ~vl::rd(n3));
}

void mipi_pll_dphy_sc_syncx10_ar_lvt::proc_always_918() {
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

void mipi_pll_dphy_sc_syncx10_ar_lvt::proc_always_930() {
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
