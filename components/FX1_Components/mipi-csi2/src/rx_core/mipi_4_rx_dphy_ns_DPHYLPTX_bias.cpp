//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYLPTX_bias
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYLPTX_bias.h"

mipi_4_rx_dphy_ns_DPHYLPTX_bias::mipi_4_rx_dphy_ns_DPHYLPTX_bias(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vph << gd << ibias;
    SC_METHOD(proc_always_159848);
    sensitive << supply_ok << pon << vref1200m;
}

void mipi_4_rx_dphy_ns_DPHYLPTX_bias::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::land(vl::ceq(vl::rd(vph), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(ibias), vl::logic<1>(0x1ULL))));
}

void mipi_4_rx_dphy_ns_DPHYLPTX_bias::proc_always_159848() {
    if (vl::rd(supply_ok).test()) {
        if (vl::land(vl::ceq(vl::rd(pon), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(vref1200m), vl::logic<1>(0x1ULL))).test()) {
            vl::wr(biasn, vl::logic<1>(0x1ULL));
            vl::wr(biasn_fb, vl::logic<1>(0x0ULL));
            vl::wr(biasp, vl::logic<1>(0x0ULL));
            vl::wr(cascn, vl::logic<1>(0x1ULL));
            vl::wr(cascp, vl::logic<1>(0x0ULL));
        } else {
            vl::wr(biasn, vl::logic<1>(0x0ULL));
            vl::wr(biasn_fb, vl::logic<1>(0x0ULL));
            vl::wr(cascp, vl::logic<1>(0x1ULL));
        }
    } else {
        vl::wr(biasn, vl::logic<1>::all_z());
        vl::wr(biasn_fb, vl::logic<1>::all_z());
        vl::wr(biasp, vl::logic<1>::all_z());
        vl::wr(cascn, vl::logic<1>::all_z());
        vl::wr(cascp, vl::logic<1>::all_z());
    }
}
