//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_sc_syncx10_as_lvt
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_sc_syncx10_as_lvt.h"

mipi_pll_dphy_sc_syncx10_as_lvt::mipi_pll_dphy_sc_syncx10_as_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vp << gd;
    SC_METHOD(assign_qb);
    sensitive << n3;
    SC_METHOD(proc_initial_1009);
    SC_METHOD(proc_initial_1010);
    SC_METHOD(proc_always_1012);
    sensitive << ck << d << set << supply_ok;
    SC_METHOD(proc_always_1024);
    sensitive << ck << n1 << set << supply_ok;
}

void mipi_pll_dphy_sc_syncx10_as_lvt::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_pll_dphy_sc_syncx10_as_lvt::assign_qb() {
    vl::wr(qb, ~vl::rd(n3));
}

void mipi_pll_dphy_sc_syncx10_as_lvt::proc_initial_1009() {
    vl::logic<1> n3_sh = vl::rd(n3);
    n3_sh = vl::logic<1>(0x0ULL);
    vl::wr(n3, n3_sh);
}

void mipi_pll_dphy_sc_syncx10_as_lvt::proc_initial_1010() {
    vl::logic<1> n1_sh = vl::rd(n1);
    n1_sh = vl::logic<1>(0x1ULL);
    vl::wr(n1, n1_sh);
}

void mipi_pll_dphy_sc_syncx10_as_lvt::proc_always_1012() {
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x0ULL)).test()) {
        vl::wr(n1, vl::random().ext<1>());
    } else {
        if (vl::rd(set).test()) {
            vl::wr(n1, vl::logic<1>(0x1ULL));
        } else if (vl::rd(ck).truth_val() == 0) {
            vl::wr(n1, ~vl::rd(d));
        }
    }
}

void mipi_pll_dphy_sc_syncx10_as_lvt::proc_always_1024() {
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x0ULL)).test()) {
        vl::wr(n3, vl::logic<1>(0x0ULL));
    } else {
        if (vl::rd(set).test()) {
            vl::wr(n3, vl::logic<1>(0x0ULL));
        } else if (vl::rd(ck).test()) {
            vl::wr(n3, ~vl::rd(n1));
        }
    }
}
