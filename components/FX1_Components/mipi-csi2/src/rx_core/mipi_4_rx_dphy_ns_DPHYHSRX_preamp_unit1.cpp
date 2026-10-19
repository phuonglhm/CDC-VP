//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit1
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit1.h"

mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit1::mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vp << gd;
    SC_METHOD(assign_enable);
    sensitive << g;
    SC_METHOD(proc_always_157833);
    sensitive << supply_ok << enable << ibias << vip;
    SC_METHOD(proc_always_157847);
    sensitive << supply_ok << enable << ibias << vim;
}

// wire  net971;
// wire  net972;
// wire  net973;
// wire  net974;
// wire  net975;
// wire  net976;
// wire  net977;
// wire  net978;
// wire  net979;
// wire  net966;
// wire  net967;
// wire  net968;
// wire  net970;
// wire  net969;
// rppwl  RLM0_0( .b(vp), .in(net970), .out(net976) );
// rppwl  RLM0_1( .b(vp), .in(voutm), .out(net970) );
// rppwl  RLM1_0( .b(vp), .in(net971), .out(net975) );
// rppwl  RLM1_1( .b(vp), .in(voutm), .out(net971) );
// rppwl  RLM2_0( .b(vp), .in(net972), .out(net974) );
// rppwl  RLM2_1( .b(vp), .in(voutm), .out(net972) );
// rppwl  RLM3( .b(vp), .in(voutm), .out(net973) );
// rppwl  RLP0_0( .b(vp), .in(net969), .out(net977) );
// rppwl  RLP0_1( .b(vp), .in(voutp), .out(net969) );
// rppwl  RLP1_0( .b(vp), .in(net968), .out(net978) );
// rppwl  RLP1_1( .b(vp), .in(voutp), .out(net968) );
// rppwl  RLP2_0( .b(vp), .in(net967), .out(net979) );
// rppwl  RLP2_1( .b(vp), .in(voutp), .out(net967) );
// rppwl  RLP3( .b(vp), .in(voutp), .out(net966) );
// pfinlvt  P231( .B(vp), .D(ibias), .G(ibias), .S(ibias) );
// pfinlvt  P232( .B(vp), .D(ibias), .G(ibias), .S(ibias) );
// pfinlvt  PM( .B(vp), .D(voutp), .G(vim), .S(ibias) );
// pfinlvt  PP( .B(vp), .D(voutm), .G(vip), .S(ibias) );
// nfinlvt  N230( .B(gd), .D(gd), .G(gd), .S(gd) );
// nfinlvt  N233( .B(gd), .D(net974), .G(g[2]), .S(gd) );
// nfinlvt  N234( .B(gd), .D(net979), .G(g[2]), .S(gd) );
// nfinlvt  NMK0( .B(gd), .D(net976), .G(g[0]), .S(gd) );
// nfinlvt  NMK1( .B(gd), .D(net975), .G(g[1]), .S(gd) );
// nfinlvt  NMK3( .B(gd), .D(net973), .G(g[3]), .S(gd) );
// nfinlvt  NPK0( .B(gd), .D(net977), .G(g[0]), .S(gd) );
// nfinlvt  NPK1( .B(gd), .D(net978), .G(g[1]), .S(gd) );
void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit1::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit1::assign_enable() {
    vl::wr(enable, vl::ror_(vl::rd(g)));
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit1::proc_always_157833() {
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)).test()) {
        if (vl::ceq(vl::rd(enable), vl::logic<1>(0x1ULL)).test()) {
            if (vl::ceq(vl::rd(ibias), vl::logic<1>(0x1ULL)).test()) {
                vl::wr(voutm, ~vl::rd(vip));
            } else {
                vl::wr(voutm, vl::logic<1>(0x0ULL));
            }
        } else {
            vl::wr(voutm, vl::logic<1>::all_z());
        }
    } else {
        vl::wr(voutm, vl::logic<1>::all_z());
    }
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit1::proc_always_157847() {
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)).test()) {
        if (vl::ceq(vl::rd(enable), vl::logic<1>(0x1ULL)).test()) {
            if (vl::ceq(vl::rd(ibias), vl::logic<1>(0x1ULL)).test()) {
                vl::wr(voutp, ~vl::rd(vim));
            } else {
                vl::wr(voutp, vl::logic<1>(0x0ULL));
            }
        } else {
            vl::wr(voutp, vl::logic<1>::all_z());
        }
    } else {
        vl::wr(voutp, vl::logic<1>::all_z());
    }
}
