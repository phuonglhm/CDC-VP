//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_sc_dlax23_ar_lvt
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_sc_dlax23_ar_lvt.h"

mipi_pll_dphy_sc_dlax23_ar_lvt::mipi_pll_dphy_sc_dlax23_ar_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_ck);
    sensitive << ck_p << ck_m;
    SC_METHOD(assign_supply_ok);
    sensitive << vp << gd;
    SC_METHOD(assign_qb);
    sensitive << nb;
    SC_METHOD(proc_initial_413);
    SC_METHOD(proc_always_417);
    sensitive << ck << d << pwr_on << vp << gd;
}

void mipi_pll_dphy_sc_dlax23_ar_lvt::assign_ck() {
    vl::wr(ck, vl::rd(ck_p) & (~vl::rd(ck_m)));
}

void mipi_pll_dphy_sc_dlax23_ar_lvt::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_pll_dphy_sc_dlax23_ar_lvt::assign_qb() {
    vl::wr(qb, ~vl::rd(nb));
}

void mipi_pll_dphy_sc_dlax23_ar_lvt::proc_initial_413() {
    vl::logic<1> nb_sh = vl::rd(nb);
    nb_sh = vl::random().ext<1>();
    vl::wr(nb, nb_sh);
}

void mipi_pll_dphy_sc_dlax23_ar_lvt::proc_always_417() {
    vl::logic<1> nb_sh_2 = vl::rd(nb);
    if (vl::rd(supply_ok).test()) {
        if (vl::rd(ck).test()) {
            nb_sh_2 = vl::rd(d);
            vl::wr(nb, nb_sh_2);
        } else if (vl::rd(pwr_on).truth_val() == 0) {
            nb_sh_2 = vl::logic<1>(0x1ULL);
            vl::wr(nb, nb_sh_2);
        } else {
            nb_sh_2 = nb_sh_2;
            vl::wr(nb, nb_sh_2);
        }
    } else {
        nb_sh_2 = vl::random().ext<1>();
        vl::wr(nb, nb_sh_2);
    }
}
