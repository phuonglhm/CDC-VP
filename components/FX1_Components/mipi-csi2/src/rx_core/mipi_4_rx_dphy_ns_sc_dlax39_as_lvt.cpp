//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_sc_dlax39_as_lvt
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_sc_dlax39_as_lvt.h"

mipi_4_rx_dphy_ns_sc_dlax39_as_lvt::mipi_4_rx_dphy_ns_sc_dlax39_as_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_ck_int);
    sensitive << ck_p << ck_m;
    SC_METHOD(assign_supply_ok);
    sensitive << vp << gd;
    SC_METHOD(assign_qb);
    sensitive << nb;
    SC_METHOD(proc_initial_161837);
    SC_METHOD(proc_always_161841);
    sensitive << supply_ok << pwr_off << ck_int << d << nb;
}

// wire  k0;
// wire  net213;
// wire  d_int;
// wire  net192;
// wire  ck_m_int;
// wire  net195;
// wire  ck_p_int;
// alias1  I8( .MINUS(ck_m), .PLUS(ck_m_int) );
// alias1  I9( .MINUS(ck_p), .PLUS(ck_p_int) );
// alias1  I10( .MINUS(d), .PLUS(d_int) );
// va_suck  dlax39_as_a( .clk(ck_m_int), .in_h(d_int), .in_s(d_int) );
// va_suck  dlax39_as_b( .clk(ck_p_int), .in_h(d_int), .in_s(d_int) );
// wire_cap  I121( .w(nb) );
// wire_cap  I913( .w(k0) );
// nfinlvt  M6( .B(gd), .D(d), .G(ck_p), .S(nb) );
// nfinlvt  M29( .B(gd), .D(qb), .G(nb), .S(gd) );
// nfinlvt  M40( .B(gd), .D(k0), .G(nb), .S(gd) );
// nfinlvt  M43( .B(gd), .D(net195), .G(k0), .S(gd) );
// nfinlvt  N5( .B(gd), .D(nb), .G(ck_m), .S(net195) );
// nfinlvt  N7( .B(gd), .D(nb), .G(pwr_off), .S(gd) );
// pfinlvt  M28( .B(vp), .D(d), .G(ck_m), .S(nb) );
// pfinlvt  M30( .B(vp), .D(qb), .G(nb), .S(vp) );
// pfinlvt  M42( .B(vp), .D(k0), .G(nb), .S(vp) );
// pfinlvt  M44( .B(vp), .D(net192), .G(k0), .S(net213) );
// pfinlvt  P4( .B(vp), .D(nb), .G(ck_p), .S(net192) );
// pfinlvt  P8( .B(vp), .D(net213), .G(pwr_off), .S(vp) );
// va_diff_clk_chk  diff_chk_dlax39_as( .clk_m(ck_m_int), .clk_p(ck_p_int)
// , .vp(vp) );
void mipi_4_rx_dphy_ns_sc_dlax39_as_lvt::assign_ck_int() {
    vl::wr(ck_int, vl::rd(ck_p) & (~vl::rd(ck_m)));
}

void mipi_4_rx_dphy_ns_sc_dlax39_as_lvt::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_sc_dlax39_as_lvt::assign_qb() {
    vl::wr(qb, ~vl::rd(nb));
}

void mipi_4_rx_dphy_ns_sc_dlax39_as_lvt::proc_initial_161837() {
    vl::logic<1> nb_sh = vl::rd(nb);
    nb_sh = vl::random().ext<1>();
    vl::wr(nb, nb_sh);
}

void mipi_4_rx_dphy_ns_sc_dlax39_as_lvt::proc_always_161841() {
    vl::logic<1> nb_sh_2 = vl::rd(nb);
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)).test()) {
        if (vl::ceq(vl::rd(pwr_off), vl::logic<1>(0x1ULL)).test()) {
            nb_sh_2 = vl::logic<1>(0x0ULL);
            vl::wr(nb, nb_sh_2);
        } else if (vl::ceq(vl::rd(ck_int), vl::logic<1>(0x1ULL)).test()) {
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
