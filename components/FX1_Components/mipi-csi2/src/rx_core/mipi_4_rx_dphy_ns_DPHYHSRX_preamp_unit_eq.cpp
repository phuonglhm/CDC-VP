//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit_eq
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit_eq.h"

mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit_eq::mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit_eq(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vph << gd;
    SC_METHOD(assign_bias_en);
    sensitive << ibias_l << ibias_r;
    SC_THREAD(proc_always_158015);
    set_stack_size(0x20000);
    SC_THREAD(proc_always_158027);
    set_stack_size(0x20000);
}

// wire  net983;
// wire  net984;
// pfinlvt  P242( .B(ibias_l), .D(ibias_l), .G(ibias_l), .S(ibias_l) );
// pfinlvt  P243( .B(ibias_r), .D(ibias_r), .G(ibias_r), .S(ibias_r) );
// pfinlvt  PM( .B(ibias_r), .D(voutp), .G(vim), .S(ibias_r) );
// pfinlvt  PP( .B(ibias_l), .D(voutm), .G(vip), .S(ibias_l) );
// mcap_234_234_h_v  CEQ_0( .first(ibias_r), .second(ibias_l) );
// nfinhv  NKEY_BYPASS( .B(gd), .D(ibias_r), .G(rx_eq_bcal_bypass),
// .S(ibias_l) );
// nfinhv  NKEY_EQ0( .B(gd), .D(ibias_r), .G(rx_eq_bcal_hz[2]), .S(net983)
// );
// nfinhv  NKEY_EQ1( .B(gd), .D(net984), .G(rx_eq_bcal_hz[1]), .S(ibias_l)
// );
// nfinhv  NKEY_EQ2( .B(gd), .D(net984), .G(rx_eq_bcal_hz[0]), .S(net983)
// );
// rppwl  REQ0( .b(vp), .in(ibias_r), .out(net983) );
// rppwl  REQ1( .b(vp), .in(net984), .out(ibias_l) );
// rppwl  REQ2( .b(vp), .in(net983), .out(net984) );
// rppwl  RLM( .b(vp), .in(voutm), .out(gd) );
// rppwl  RLP( .b(vp), .in(voutp), .out(gd) );
// // Instances of basic/ipin/symbol
void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit_eq::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vph), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit_eq::assign_bias_en() {
    vl::wr(bias_en, vl::land(vl::ceq(vl::rd(ibias_l), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(ibias_r), vl::logic<1>(0x1ULL))));
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit_eq::proc_always_158015() {
    for (;;) {
        wait(supply_ok.value_changed_event() | bias_en.value_changed_event() | vip.value_changed_event());
        if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)).test()) {
            if (vl::ceq(vl::rd(bias_en), vl::logic<1>(0x1ULL)).test()) {
                wait(vl::dly(0.001, TU_, TP_));
                vl::wr(voutp, vl::rd(vip));
            } else {
                vl::wr(voutp, vl::logic<1>());
            }
        } else {
            vl::wr(voutp, vl::logic<1>::all_z());
        }
    }
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit_eq::proc_always_158027() {
    for (;;) {
        wait(supply_ok.value_changed_event() | bias_en.value_changed_event() | vim.value_changed_event());
        if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)).test()) {
            if (vl::ceq(vl::rd(bias_en), vl::logic<1>(0x1ULL)).test()) {
                wait(vl::dly(0.001, TU_, TP_));
                vl::wr(voutm, vl::rd(vim));
            } else {
                vl::wr(voutm, vl::logic<1>());
            }
        } else {
            vl::wr(voutm, vl::logic<1>::all_z());
        }
    }
}
