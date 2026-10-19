//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_sc_dlax23_as_lvt
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_sc_dlax23_as_lvt.h"

mipi_4_rx_dphy_ns_sc_dlax23_as_lvt::mipi_4_rx_dphy_ns_sc_dlax23_as_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_ck_int);
    sensitive << ck_p << ck_m;
    SC_METHOD(assign_supply_ok);
    sensitive << vp << gd;
    SC_METHOD(assign_qb);
    sensitive << nb;
    SC_METHOD(proc_initial_161694);
    SC_METHOD(proc_always_161698);
    sensitive << supply_ok << pwr_off << ck_int << d << nb;
}

void mipi_4_rx_dphy_ns_sc_dlax23_as_lvt::assign_ck_int() {
    vl::wr(ck_int, vl::rd(ck_p) & (~vl::rd(ck_m)));
}

void mipi_4_rx_dphy_ns_sc_dlax23_as_lvt::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_sc_dlax23_as_lvt::assign_qb() {
    vl::wr(qb, ~vl::rd(nb));
}

void mipi_4_rx_dphy_ns_sc_dlax23_as_lvt::proc_initial_161694() {
    vl::logic<1> nb_sh = vl::rd(nb);
    nb_sh = vl::random().ext<1>();
    vl::wr(nb, nb_sh);
}

void mipi_4_rx_dphy_ns_sc_dlax23_as_lvt::proc_always_161698() {
    vl::logic<1> nb_sh_2 = vl::rd(nb);
    if (vl::rd(supply_ok).test()) {
        if (vl::rd(pwr_off).test()) {
            nb_sh_2 = vl::logic<1>(0x0ULL);
            vl::wr(nb, nb_sh_2);
        } else if (vl::rd(ck_int).test()) {
            nb_sh_2 = vl::rd(d);
            vl::wr(nb, nb_sh_2);
        } else {
            nb_sh_2 = nb_sh_2;
            vl::wr(nb, nb_sh_2);
        }
    } else {
        nb_sh_2 = vl::logic<1>();
        vl::wr(nb, nb_sh_2);
    }
}
