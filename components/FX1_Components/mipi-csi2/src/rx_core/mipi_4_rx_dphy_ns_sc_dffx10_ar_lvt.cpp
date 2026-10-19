//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_sc_dffx10_ar_lvt
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_sc_dffx10_ar_lvt.h"

mipi_4_rx_dphy_ns_sc_dffx10_ar_lvt::mipi_4_rx_dphy_ns_sc_dffx10_ar_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vp << gd;
    SC_METHOD(assign_ck_int);
    sensitive << ck << ckb;
    SC_METHOD(assign_set);
    sensitive << reset_b;
    SC_METHOD(assign_n2);
    sensitive << n1;
    SC_METHOD(assign_qb);
    sensitive << n3;
    SC_METHOD(proc_initial_161521);
    SC_METHOD(proc_initial_161522);
    SC_METHOD(proc_always_161528);
    sensitive << supply_ok << set << ck_int << n1 << d;
    SC_METHOD(proc_always_161541);
    sensitive << supply_ok << set << ck_int << n2 << n3;
}

void mipi_4_rx_dphy_ns_sc_dffx10_ar_lvt::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_sc_dffx10_ar_lvt::assign_ck_int() {
    vl::wr(ck_int, vl::rd(ck) & (~vl::rd(ckb)));
}

void mipi_4_rx_dphy_ns_sc_dffx10_ar_lvt::assign_set() {
    vl::wr(set, ~vl::rd(reset_b));
}

void mipi_4_rx_dphy_ns_sc_dffx10_ar_lvt::assign_n2() {
    vl::wr(n2, ~vl::rd(n1));
}

void mipi_4_rx_dphy_ns_sc_dffx10_ar_lvt::assign_qb() {
    vl::wr(qb, ~vl::rd(n3));
}

void mipi_4_rx_dphy_ns_sc_dffx10_ar_lvt::proc_initial_161521() {
    vl::logic<1> n3_sh = vl::rd(n3);
    n3_sh = vl::random().ext<1>();
    vl::wr(n3, n3_sh);
}

void mipi_4_rx_dphy_ns_sc_dffx10_ar_lvt::proc_initial_161522() {
    vl::logic<1> n1_sh = vl::rd(n1);
    n1_sh = vl::random().ext<1>();
    vl::wr(n1, n1_sh);
}

void mipi_4_rx_dphy_ns_sc_dffx10_ar_lvt::proc_always_161528() {
    if (vl::rd(supply_ok).test()) {
        if (vl::rd(set).test()) {
            vl::wr(n1, vl::logic<1>(0x0ULL));
        } else if (vl::rd(ck_int).test()) {
            vl::wr(n1, vl::rd(n1));
        } else {
            vl::wr(n1, ~vl::rd(d));
        }
    } else {
        vl::wr(n1, vl::logic<1>());
    }
}

void mipi_4_rx_dphy_ns_sc_dffx10_ar_lvt::proc_always_161541() {
    if (vl::rd(supply_ok).test()) {
        if (vl::rd(set).test()) {
            vl::wr(n3, vl::logic<1>(0x1ULL));
        } else if (vl::rd(ck_int).test()) {
            vl::wr(n3, vl::rd(n2));
        } else {
            vl::wr(n3, vl::rd(n3));
        }
    } else {
        vl::wr(n3, vl::logic<1>());
    }
}
