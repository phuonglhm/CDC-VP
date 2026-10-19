//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_LEVELCONV_vph_det
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_LEVELCONV_vph_det.h"

mipi_pll_dphy_LEVELCONV_vph_det::mipi_pll_dphy_LEVELCONV_vph_det(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(gate_buf1);
    sensitive << A_in << tie_low_hv << vp << gd << tie_low_lv;
    SC_METHOD(gate_buf2);
    sensitive << Z << vp << gd;
}

// spfinlvt  M4_0( .B(vp), .D(gate), .G(tie_low_lv), .S(a[0]) );
// spfinlvt  M4_1( .B(vp), .D(a[0]), .G(tie_low_lv), .S(a[1]) );
// spfinlvt  M4_2( .B(vp), .D(a[1]), .G(tie_low_lv), .S(a[2]) );
// spfinlvt  M4_3( .B(vp), .D(a[2]), .G(tie_low_lv), .S(a[3]) );
// spfinlvt  M4_4( .B(vp), .D(a[3]), .G(tie_low_lv), .S(a[4]) );
// spfinlvt  M4_5( .B(vp), .D(a[4]), .G(tie_low_lv), .S(a[5]) );
// spfinlvt  M4_6( .B(vp), .D(a[5]), .G(tie_low_lv), .S(a[6]) );
// spfinlvt  M4_7( .B(vp), .D(a[6]), .G(tie_low_lv), .S(a[7]) );
// spfinlvt  M4_8( .B(vp), .D(a[7]), .G(tie_low_lv), .S(a[8]) );
// spfinlvt  M4_9( .B(vp), .D(a[8]), .G(tie_low_lv), .S(a[9]) );
// spfinlvt  M4_10( .B(vp), .D(a[9]), .G(tie_low_lv), .S(a[10]) );
// spfinlvt  M4_11( .B(vp), .D(a[10]), .G(tie_low_lv), .S(a[11]) );
// spfinlvt  M4_12( .B(vp), .D(a[11]), .G(tie_low_lv), .S(a[12]) );
// spfinlvt  M4_13( .B(vp), .D(a[12]), .G(tie_low_lv), .S(a[13]) );
// spfinlvt  M4_14( .B(vp), .D(a[13]), .G(tie_low_lv), .S(a[14]) );
// spfinlvt  M4_15( .B(vp), .D(a[14]), .G(tie_low_lv), .S(a[15]) );
// spfinlvt  M4_16( .B(vp), .D(a[15]), .G(tie_low_lv), .S(a[16]) );
// spfinlvt  M4_17( .B(vp), .D(a[16]), .G(tie_low_lv), .S(a[17]) );
// spfinlvt  M4_18( .B(vp), .D(a[17]), .G(tie_low_lv), .S(a[18]) );
// spfinlvt  M4_19( .B(vp), .D(a[18]), .G(tie_low_lv), .S(vp) );
// snfinhv  M3( .D(gate), .G(inr), .S(gd), .B(gd) );
// rppwl  R0( .b(gd), .in(A_in), .out(net227) );
// rppwl  R1( .b(gd), .in(net227), .out(inr) );
// nfinhv  ESD( .B(gd), .D(inr), .G(tie_low_hv), .S(gd) );
// pfinlvt  M13( .B(vp), .D(Z), .G(gate), .S(vp) );
// pfinlvt  M14( .B(vp), .D(Z_neg), .G(Z), .S(vp) );
// pfinlvt  MPhyst( .B(vp), .D(a[7]), .G(Z), .S(vp) );
// nfinlvt  M16( .B(gd), .D(Z), .G(gate), .S(gd) );
// nfinlvt  M17( .B(gd), .D(Z_neg), .G(Z), .S(gd) );
void mipi_pll_dphy_LEVELCONV_vph_det::gate_buf1() {
    const vl::logic<1> v = vl::bufif1_(vl::rd(A_in), vl::land(vl::land(vl::land(vl::eq(vl::rd(tie_low_hv), vl::logic<1>(0x0ULL)), vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(tie_low_lv), vl::logic<1>(0x0ULL))));
    vl::wr(Z, v);
}

void mipi_pll_dphy_LEVELCONV_vph_det::gate_buf2() {
    const vl::logic<1> v = vl::bufif1_(~vl::rd(Z), vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
    vl::wr(Z_neg, v);
}
