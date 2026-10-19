//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_sc_ffx23_lvt
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_sc_ffx23_lvt.h"

mipi_4_rx_dphy_ns_sc_ffx23_lvt::mipi_4_rx_dphy_ns_sc_ffx23_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vp << gd;
    SC_METHOD(assign_q);
    sensitive << n3;
    SC_METHOD(proc_initial_161879);
    SC_METHOD(proc_initial_161880);
    SC_METHOD(proc_always_161882);
    sensitive << supply_ok << ck << d << n1;
    SC_METHOD(proc_always_161894);
    sensitive << supply_ok << ck << n1 << n3;
}

void mipi_4_rx_dphy_ns_sc_ffx23_lvt::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_sc_ffx23_lvt::assign_q() {
    vl::wr(q, ~vl::rd(n3));
}

void mipi_4_rx_dphy_ns_sc_ffx23_lvt::proc_initial_161879() {
    vl::logic<1> n3_sh = vl::rd(n3);
    n3_sh = vl::random().ext<1>();
    vl::wr(n3, n3_sh);
}

void mipi_4_rx_dphy_ns_sc_ffx23_lvt::proc_initial_161880() {
    vl::logic<1> n1_sh = vl::rd(n1);
    n1_sh = vl::random().ext<1>();
    vl::wr(n1, n1_sh);
}

void mipi_4_rx_dphy_ns_sc_ffx23_lvt::proc_always_161882() {
    vl::logic<1> n1_sh_2 = vl::rd(n1);
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x0ULL)).test()) {
        n1_sh_2 = vl::logic<1>();
        vl::wr(n1, n1_sh_2);
    } else {
        if (vl::rd(ck).truth_val() == 0) {
            n1_sh_2 = vl::rd(d);
            vl::wr(n1, n1_sh_2);
        } else {
            n1_sh_2 = n1_sh_2;
            vl::wr(n1, n1_sh_2);
        }
    }
}

void mipi_4_rx_dphy_ns_sc_ffx23_lvt::proc_always_161894() {
    vl::logic<1> n3_sh_2 = vl::rd(n3);
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x0ULL)).test()) {
        n3_sh_2 = vl::logic<1>();
        vl::wr(n3, n3_sh_2);
    } else {
        if (vl::rd(ck).test()) {
            n3_sh_2 = ~vl::rd(n1);
            vl::wr(n3, n3_sh_2);
        } else {
            n3_sh_2 = n3_sh_2;
            vl::wr(n3, n3_sh_2);
        }
    }
}
