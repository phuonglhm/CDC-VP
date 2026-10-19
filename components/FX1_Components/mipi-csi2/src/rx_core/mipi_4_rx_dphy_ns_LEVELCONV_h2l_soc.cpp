//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_LEVELCONV_h2l_soc
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_LEVELCONV_h2l_soc.h"

mipi_4_rx_dphy_ns_LEVELCONV_h2l_soc::mipi_4_rx_dphy_ns_LEVELCONV_h2l_soc(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vdd << vph << vss << gd;
    SC_METHOD(proc_always_161156);
    sensitive << supply_ok << onz_lv_soc << tie_low_lv_soc << A_in;
}

// wire  inz;
// wire  net309;
// wire  net283;
// wire  net284;
// wire  y;
// wire  yz;
// pfinlvt  M4( .B(vdd), .D(y), .G(yz), .S(vdd) );
// pfinlvt  M29( .B(vdd), .D(yz), .G(y), .S(vdd) );
// pfinlvt  M32( .B(vdd), .D(Z_neg), .G(y), .S(vdd) );
// pfinlvt  M35( .B(vdd), .D(Z), .G(yz), .S(vdd) );
// pfinlvt  dummy2_1( .B(vdd), .D(vdd), .G(vdd), .S(vdd) );
// pfinlvt  dummy2_0( .B(vdd), .D(vdd), .G(vdd), .S(vdd) );
// rppwl  R39( .b(vph), .in(y), .out(net284) );
// rppwl  XR8( .b(vph), .in(yz), .out(net283) );
// nfinlvt  M2( .B(vss), .D(y), .G(onz_lv_soc), .S(vss) );
// nfinlvt  M30( .B(vss), .D(Z_neg), .G(y), .S(vss) );
// nfinlvt  M34( .B(vss), .D(Z), .G(yz), .S(vss) );
// nfinlvt  M36( .B(vss), .D(yz), .G(tie_low_lv_soc), .S(vss) );
// nfinlvt  dummy3_1( .B(vss), .D(vss), .G(vss), .S(vss) );
// nfinlvt  dummy3_0( .B(vss), .D(vss), .G(vss), .S(vss) );
// nfinlvt  dummy4_1( .B(vss), .D(vss), .G(vss), .S(vss) );
// nfinlvt  dummy4_0( .B(vss), .D(vss), .G(vss), .S(vss) );
// nfinhv  M3( .B(gd), .D(inz), .G(A_in), .S(gd) );
// nfinhv  M25( .B(gd), .D(net284), .G(inz), .S(net309) );
// nfinhv  M26( .B(gd), .D(net283), .G(A_in), .S(net309) );
// nfinhv  N47( .B(gd), .D(net309), .G(on_hv), .S(gd) );
// pfinhv  M5( .B(vph), .D(inz), .G(A_in), .S(vph) );
// pfinhv  dummy1( .B(vph), .D(vph), .G(vph), .S(vph) );
void mipi_4_rx_dphy_ns_LEVELCONV_h2l_soc::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::land(vl::land(vl::ceq(vl::rd(vdd), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(vph), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(vss), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_LEVELCONV_h2l_soc::proc_always_161156() {
    if (vl::rd(supply_ok).test()) {
        if (vl::land(vl::ceq(vl::rd(onz_lv_soc), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(tie_low_lv_soc), vl::logic<1>(0x1ULL))).test()) {
            vl::wr(Z_neg, vl::logic<1>(0x1ULL));
            vl::wr(Z, vl::logic<1>(0x1ULL));
        } else if (vl::land(vl::ceq(vl::rd(onz_lv_soc), vl::logic<1>(0x0ULL)), vl::ceq(vl::rd(tie_low_lv_soc), vl::logic<1>(0x1ULL))).test()) {
            vl::wr(Z_neg, vl::logic<1>(0x0ULL));
            vl::wr(Z, vl::logic<1>(0x1ULL));
        } else if (vl::land(vl::ceq(vl::rd(onz_lv_soc), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(tie_low_lv_soc), vl::logic<1>(0x0ULL))).test()) {
            vl::wr(Z_neg, vl::logic<1>(0x1ULL));
            vl::wr(Z, vl::logic<1>(0x0ULL));
        } else {
            vl::wr(Z, vl::rd(A_in));
            vl::wr(Z_neg, ~vl::rd(A_in));
        }
    } else {
        vl::wr(Z, vl::logic<1>::all_z());
    }
    vl::wr(Z_neg, vl::logic<1>::all_z());
}
