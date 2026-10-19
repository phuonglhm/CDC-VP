//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_sc_srlax12_lvt
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_sc_srlax12_lvt.h"

mipi_4_rx_dphy_ns_sc_srlax12_lvt::mipi_4_rx_dphy_ns_sc_srlax12_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vp << gd;
    SC_METHOD(assign_z);
    sensitive << nb;
    SC_METHOD(proc_always_162402);
    sensitive << supply_ok << reset_n << set << nb;
}

void mipi_4_rx_dphy_ns_sc_srlax12_lvt::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_sc_srlax12_lvt::assign_z() {
    vl::wr(z, ~vl::rd(nb));
}

void mipi_4_rx_dphy_ns_sc_srlax12_lvt::proc_always_162402() {
    vl::logic<1> nb_sh = vl::rd(nb);
    if (vl::rd(supply_ok).test()) {
        if (vl::rd(reset_n).truth_val() == 0) {
            nb_sh = vl::logic<1>(0x1ULL);
            vl::wr(nb, nb_sh);
        } else if (vl::rd(set).test()) {
            nb_sh = vl::logic<1>(0x0ULL);
            vl::wr(nb, nb_sh);
        } else {
            nb_sh = nb_sh;
            vl::wr(nb, nb_sh);
        }
    } else {
        nb_sh = vl::logic<1>();
        vl::wr(nb, nb_sh);
    }
}
