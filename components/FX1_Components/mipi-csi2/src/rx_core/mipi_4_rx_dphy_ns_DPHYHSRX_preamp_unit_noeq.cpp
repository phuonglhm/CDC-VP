//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit_noeq
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit_noeq.h"

mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit_noeq::mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit_noeq(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vph << gd;
    SC_METHOD(proc_always_158071);
    sensitive << supply_ok << ibias << vip;
    SC_METHOD(proc_always_158081);
    sensitive << supply_ok << ibias << vim;
}

// pfinlvt  P242( .B(ibias), .D(ibias), .G(ibias), .S(ibias) );
// pfinlvt  P243( .B(ibias), .D(ibias), .G(ibias), .S(ibias) );
// pfinlvt  PM( .B(ibias), .D(voutp), .G(vim), .S(ibias) );
// pfinlvt  PP( .B(ibias), .D(voutm), .G(vip), .S(ibias) );
// rppwl  RLM( .b(vp), .in(voutm), .out(gd) );
// rppwl  RLP( .b(vp), .in(voutp), .out(gd) );
void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit_noeq::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vph), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit_noeq::proc_always_158071() {
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)).test()) {
        if (vl::ceq(vl::rd(ibias), vl::logic<1>(0x1ULL)).test()) {
            vl::wr(voutp, vl::rd(vip));
        } else {
            vl::wr(voutp, vl::logic<1>());
        }
    } else {
        vl::wr(voutp, vl::logic<1>::all_z());
    }
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit_noeq::proc_always_158081() {
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)).test()) {
        if (vl::ceq(vl::rd(ibias), vl::logic<1>(0x1ULL)).test()) {
            vl::wr(voutm, vl::rd(vim));
        } else {
            vl::wr(voutm, vl::logic<1>());
        }
    } else {
        vl::wr(voutm, vl::logic<1>::all_z());
    }
}
