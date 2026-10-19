//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_slicer_comp1a_ptype
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_slicer_comp1a_ptype.h"

mipi_4_rx_dphy_ns_DPHYHSRX_slicer_comp1a_ptype::mipi_4_rx_dphy_ns_DPHYHSRX_slicer_comp1a_ptype(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vp << gd;
    SC_METHOD(proc_initial_158170);
    SC_METHOD(proc_always_158176);
    sensitive << clk << vi_m << vi_p << vi_m_prev << vi_p_prev;
    SC_METHOD(proc_always_158191);
    sensitive << supply_ok << clk << vi_m;
    SC_METHOD(proc_always_158201);
    sensitive << supply_ok << clk << vi_p;
}

// wire  src;
// wire  na_p;
// wire  na_m;
// nfinlvt  N0( .B(gd), .D(vom), .G(vop), .S(gd) );
// nfinlvt  N1( .B(gd), .D(vop), .G(vom), .S(gd) );
// nfinlvt  N3( .B(gd), .D(vop), .G(clk), .S(gd) );
// nfinlvt  N4( .B(gd), .D(vom), .G(clk), .S(gd) );
// nfinlvt  N5( .B(gd), .D(na_p), .G(clk), .S(gd) );
// nfinlvt  N6( .B(gd), .D(na_m), .G(clk), .S(gd) );
// wire_cap  I41( .w(na_p) );
// wire_cap  I42( .w(na_m) );
// wire_cap  I43( .w(src) );
// pfinlvt  P0( .B(vp), .D(vop), .G(vom), .S(na_p) );
// pfinlvt  P1( .B(vp), .D(vom), .G(vop), .S(na_m) );
// pfinlvt  P13( .B(vp), .D(na_p), .G(vi_m), .S(src) );
// pfinlvt  P22( .B(vp), .D(src), .G(clk), .S(vp) );
// pfinlvt  P31( .B(vp), .D(na_m), .G(vi_p), .S(src) );
void mipi_4_rx_dphy_ns_DPHYHSRX_slicer_comp1a_ptype::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHYHSRX_slicer_comp1a_ptype::proc_initial_158170() {
    vl::logic<1> vi_m_prev_sh = vl::rd(vi_m_prev);
    vl::logic<1> vi_p_prev_sh = vl::rd(vi_p_prev);
    vl::logic<1> violation_sh = vl::rd(violation);
    violation_sh = vl::logic<1>(0x0ULL);
    vl::wr(violation, violation_sh);
    vi_m_prev_sh = vl::logic<1>(0x0ULL);
    vl::wr(vi_m_prev, vi_m_prev_sh);
    vi_p_prev_sh = vl::logic<1>(0x0ULL);
    vl::wr(vi_p_prev, vi_p_prev_sh);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_slicer_comp1a_ptype::proc_always_158176() {
    vl::logic<1> vi_m_prev_sh_2 = vl::rd(vi_m_prev);
    vl::logic<1> vi_p_prev_sh_2 = vl::rd(vi_p_prev);
    vl::logic<1> violation_sh_2 = vl::rd(violation);
    if (vl::ceq(vl::rd(clk), vl::logic<1>(0x1ULL)).test()) {
        violation_sh_2 = vl::logic<1>(0x0ULL);
        vl::wr(violation, violation_sh_2);
        vi_m_prev_sh_2 = vl::rd(vi_m);
        vl::wr(vi_m_prev, vi_m_prev_sh_2);
        vi_p_prev_sh_2 = vl::rd(vi_p);
        vl::wr(vi_p_prev, vi_p_prev_sh_2);
    } else {
        if (vl::lor(vl::cne(vi_m_prev_sh_2, vl::rd(vi_m)), vl::cne(vi_p_prev_sh_2, vl::rd(vi_p))).test()) {
            vl::wr(violation, vl::logic<1>(0x1ULL));
        } else {
            vl::wr(violation, vl::logic<1>(0x0ULL));
        }
    }
}

void mipi_4_rx_dphy_ns_DPHYHSRX_slicer_comp1a_ptype::proc_always_158191() {
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)).test()) {
        if (vl::ceq(vl::rd(clk), vl::logic<1>(0x1ULL)).test()) {
            vl::wr(vom, vl::rd(vi_m));
        } else {
            vl::wr(vom, vl::logic<1>(0x0ULL));
        }
    } else {
        vl::wr(vom, vl::logic<1>::all_z());
    }
}

void mipi_4_rx_dphy_ns_DPHYHSRX_slicer_comp1a_ptype::proc_always_158201() {
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)).test()) {
        if (vl::ceq(vl::rd(clk), vl::logic<1>(0x1ULL)).test()) {
            vl::wr(vop, vl::rd(vi_p));
        } else {
            vl::wr(vop, vl::logic<1>(0x0ULL));
        }
    } else {
        vl::wr(vop, vl::logic<1>::all_z());
    }
}
