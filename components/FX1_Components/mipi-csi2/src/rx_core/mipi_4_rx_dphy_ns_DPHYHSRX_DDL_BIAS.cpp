//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BIAS
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BIAS.h"

mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BIAS::mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BIAS(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(drive_bp);
    sensitive << bp_r;
    SC_METHOD(assign_supply_ok);
    sensitive << vp << gd;
    SC_METHOD(assign_enable);
    sensitive << pon << ponz << tie_high << bias_ddl << pon_hv;
    SC_METHOD(proc_always_154783);
    sensitive << supply_ok << enable;
    SC_METHOD(proc_always_154793);
    sensitive << supply_ok << enable;
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BIAS::drive_bp() { vl::wr(bp, vl::rd(bp_r)); }

// nfinhv  NCDM( .B(gd), .D(net433), .G(pon_hv), .S(net428) );
// spfinlvt  PM5( .B(vp), .D(bp), .G(bp), .S(vp) );
// spfinlvt  PM6( .B(vp), .D(bn), .G(bp), .S(vp) );
// snfinlvt  NM( .B(gd), .D(net428), .G(net428), .S(gd) );
// snfinlvt  NM0( .B(gd), .D(net356), .G(net428), .S(gd) );
// snfinlvt  NM1( .B(gd), .D(net354), .G(net428), .S(gd) );
// snfinlvt  NM2( .B(gd), .D(net352), .G(net428), .S(gd) );
// snfinlvt  NM3( .B(gd), .D(net350), .G(net428), .S(gd) );
// snfinlvt  NM4( .B(gd), .D(net348), .G(net428), .S(gd) );
// snfinlvt  NM5( .B(gd), .D(net343), .G(net428), .S(gd) );
// snfinlvt  NM6( .B(gd), .D(bn), .G(bn), .S(gd) );
// nfinlvt  N73( .B(gd), .D(bn), .G(ponz), .S(gd) );
// nfinlvt  NK0( .B(gd), .D(bp), .G(tie_high), .S(net356) );
// nfinlvt  NK1( .B(gd), .D(bp), .G(tune[0]), .S(net354) );
// nfinlvt  NK2( .B(gd), .D(bp), .G(tune[1]), .S(net352) );
// nfinlvt  NK3( .B(gd), .D(bp), .G(tune[2]), .S(net350) );
// nfinlvt  NK4( .B(gd), .D(bp), .G(tune[3]), .S(net348) );
// nfinlvt  NK5( .B(gd), .D(bp), .G(tune[4]), .S(net343) );
// nfinlvt  NONZ1( .B(gd), .D(net428), .G(ponz), .S(gd) );
// pfinlvt  PON( .B(vp), .D(bp), .G(pon), .S(vp) );
// rppwl  RCDM( .b(gd), .in(bias_ddl), .out(net433) );
// // Instances of basic/opin/symbol
void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BIAS::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BIAS::assign_enable() {
    vl::wr(enable, vl::land(vl::land(vl::land(vl::land(vl::ceq(vl::rd(pon), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(ponz), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(tie_high), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(bias_ddl), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(pon_hv), vl::logic<1>(0x1ULL))));
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BIAS::proc_always_154783() {
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)).test()) {
        if (vl::ceq(vl::rd(enable), vl::logic<1>(0x1ULL)).test()) {
            vl::wr(bp_r, vl::logic<1>(0x0ULL));
        } else {
            vl::wr(bp_r, vl::logic<1>(0x1ULL));
        }
    } else {
        vl::wr(bp_r, vl::logic<1>::all_z());
    }
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BIAS::proc_always_154793() {
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)).test()) {
        if (vl::ceq(vl::rd(enable), vl::logic<1>(0x1ULL)).test()) {
            vl::wr(bn, vl::logic<1>(0x1ULL));
        } else {
            vl::wr(bn, vl::logic<1>(0x0ULL));
        }
    } else {
        vl::wr(bp_r, vl::logic<1>::all_z());
    }
}
