//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_sc_syncx10_as_lvt
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_sc_syncx10_as_lvt.h"

mipi_4_rx_dphy_ns_sc_syncx10_as_lvt::mipi_4_rx_dphy_ns_sc_syncx10_as_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vp << gd;
    SC_METHOD(assign_n2);
    sensitive << n1;
    SC_METHOD(assign_qb);
    sensitive << n3;
    SC_METHOD(proc_initial_162567);
    SC_METHOD(proc_initial_162568);
    SC_METHOD(proc_always_162572);
    sensitive << supply_ok << ck << set << n1 << d;
    SC_METHOD(proc_always_162592);
    sensitive << supply_ok << ck << n2 << set << n3;
}

void mipi_4_rx_dphy_ns_sc_syncx10_as_lvt::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_sc_syncx10_as_lvt::assign_n2() {
    vl::wr(n2, ~vl::rd(n1));
}

void mipi_4_rx_dphy_ns_sc_syncx10_as_lvt::assign_qb() {
    vl::wr(qb, ~vl::rd(n3));
}

// wire  n3;
// wire  kp;
// wire  net481;
// wire  net348;
// wire  net349;
// wire  ckb;
// wire  d_n;
// wire  net371;
// wire  net372;
// wire  reset_b;
// wire  ff;
// wire  kp1;
// wire  net474;
// wire  n1;
// va_suck  syncx10_as_a( .clk(ckb), .in_h(d_n), .in_s(d_n) );
// va_suck  syncx10_as_b( .clk(ckb), .in_h(n2), .in_s(n2) );
// wire_cap  I121( .w(n1) );
// wire_cap  I123( .w(n3) );
// wire_cap  I224( .w(d_n) );
// wire_cap  I225( .w(n2) );
// wire_cap  I226( .w(ckb) );
// wire_cap  I252( .w(ff) );
// wire_cap  I254( .w(kp) );
// wire_cap  I255( .w(reset_b) );
// wire_cap  I256( .w(kp1) );
// nfinlvt  N0( .B(gd), .D(ckb), .G(ck), .S(gd) );
// nfinlvt  N1( .B(gd), .D(ff), .G(reset_b), .S(gd) );
// nfinlvt  N4( .B(gd), .D(net481), .G(reset_b), .S(gd) );
// nfinlvt  N6( .B(gd), .D(d_n), .G(ckb), .S(n1) );
// nfinlvt  N18b( .B(gd), .D(reset_b), .G(set), .S(gd) );
// nfinlvt  N29( .B(gd), .D(n2), .G(n1), .S(gd) );
// nfinlvt  N31( .B(gd), .D(qb), .G(n3), .S(gd) );
// nfinlvt  N32( .B(gd), .D(n2), .G(ck), .S(n3) );
// nfinlvt  N91( .B(gd), .D(n1), .G(kp), .S(net348) );
// nfinlvt  N91b( .B(gd), .D(kp), .G(set), .S(gd) );
// nfinlvt  N95( .B(gd), .D(kp), .G(n1), .S(gd) );
// nfinlvt  N98( .B(gd), .D(n3), .G(kp1), .S(net371) );
// nfinlvt  N99( .B(gd), .D(net348), .G(ck), .S(gd) );
// nfinlvt  N101( .B(gd), .D(kp1), .G(n3), .S(net481) );
// nfinlvt  N102( .B(gd), .D(net371), .G(ckb), .S(gd) );
void mipi_4_rx_dphy_ns_sc_syncx10_as_lvt::proc_initial_162567() {
    vl::logic<1> n3_sh = vl::rd(n3);
    n3_sh = vl::random().ext<1>();
    vl::wr(n3, n3_sh);
}

void mipi_4_rx_dphy_ns_sc_syncx10_as_lvt::proc_initial_162568() {
    vl::logic<1> n1_sh = vl::rd(n1);
    n1_sh = vl::random().ext<1>();
    vl::wr(n1, n1_sh);
}

void mipi_4_rx_dphy_ns_sc_syncx10_as_lvt::proc_always_162572() {
    if (vl::rd(supply_ok).test()) {
        if (vl::rd(ck).test()) {
            if (vl::rd(set).test()) {
                vl::wr(n1, vl::logic<1>(0x1ULL));
            } else {
                vl::wr(n1, vl::rd(n1));
            }
        } else {
            if (vl::rd(set).test()) {
                vl::wr(n1, vl::logic<1>(0x1ULL));
            } else {
                vl::wr(n1, ~vl::rd(d));
            }
        }
    } else {
        vl::wr(n1, vl::logic<1>());
    }
}

void mipi_4_rx_dphy_ns_sc_syncx10_as_lvt::proc_always_162592() {
    if (vl::rd(supply_ok).test()) {
        if (vl::rd(ck).test()) {
            vl::wr(n3, vl::rd(n2));
        } else {
            if (vl::rd(set).test()) {
                vl::wr(n3, vl::logic<1>(0x0ULL));
            } else {
                vl::wr(n3, vl::rd(n3));
            }
        }
    } else {
        vl::wr(n3, vl::logic<1>());
    }
}
