//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYCB_preamp_bias
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYCB_preamp_bias.h"

mipi_4_rx_dphy_ns_DPHYCB_preamp_bias::mipi_4_rx_dphy_ns_DPHYCB_preamp_bias(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(drive_ibias_d2c_bias);
    sensitive << ibias_d2c_bias_r;
    SC_METHOD(drive_ihv);
    sensitive << ihv_r;
    SC_METHOD(drive_ilv);
    sensitive << ilv_r;
    SC_METHOD(assign_supply_ok);
    sensitive << vph << gd;
    SC_METHOD(assign_enable);
    sensitive << rx_pre_pon_h << rx_pre_pon_hz;
    SC_METHOD(proc_initial_151641);
    SC_THREAD(proc_always_151647);
    set_stack_size(0x20000);
}

void mipi_4_rx_dphy_ns_DPHYCB_preamp_bias::drive_ibias_d2c_bias() { vl::wr(ibias_d2c_bias, vl::rd(ibias_d2c_bias_r)); }

void mipi_4_rx_dphy_ns_DPHYCB_preamp_bias::drive_ihv() { vl::wr(ihv, vl::rd(ihv_r)); }

void mipi_4_rx_dphy_ns_DPHYCB_preamp_bias::drive_ilv() { vl::wr(ilv, vl::rd(ilv_r)); }

// wire  [0:4] net206;
// wire  vref200;
// wire  [0:4] net209;
// wire  [6:0] vref;
// wire  net248;
// wire  net250;
// wire  vbp_cgm;
// wire  net272;
// wire  net361;
// wire  net362;
// wire  net363;
// wire  net364;
// wire  net365;
// wire  vbp_ccm;
// DPHYCB_preamp_bias_const_cm_opamp  I31( .inm(vref200), .inp(vfb_in),
// .on(rx_pre_pon), .onz(rx_pre_ponz), .vbp(vbp_cgm), .vcn(vref[5]),
// .o(vbp_ccm), .gd(gd), .vp(vp) );
// rppwl  R19( .b(vp), .in(vfb_out), .out(net250) );
// rppwl  R20( .b(vp), .in(net272), .out(net248) );
// DPHYCB_preamp_bias_const_gm  gm( .onhv(rx_pre_pon_h),
// .onhvz(rx_pre_pon_hz), .startz(rx_pre_pon_hz), .vbn_in(net364),
// .vbn_out(net364), .vcn(net365), .gd(gd), .vph(vph) );
// nfinhv  N19_4( .B(gd), .D(ihv_cal[4]), .G(net365), .S(net206[0]) );
// nfinhv  N19_3( .B(gd), .D(ihv_cal[3]), .G(net365), .S(net206[1]) );
// nfinhv  N19_2( .B(gd), .D(ihv_cal[2]), .G(net365), .S(net206[2]) );
// nfinhv  N19_1( .B(gd), .D(ihv_cal[1]), .G(net365), .S(net206[3]) );
// nfinhv  N19_0( .B(gd), .D(ihv_cal[0]), .G(net365), .S(net206[4]) );
// nfinhv  N21_4( .B(gd), .D(ihv[4]), .G(net365), .S(net209[0]) );
// nfinhv  N21_3( .B(gd), .D(ihv[3]), .G(net365), .S(net209[1]) );
// nfinhv  N21_2( .B(gd), .D(ihv[2]), .G(net365), .S(net209[2]) );
// nfinhv  N21_1( .B(gd), .D(ihv[1]), .G(net365), .S(net209[3]) );
// nfinhv  N21_0( .B(gd), .D(ihv[0]), .G(net365), .S(net209[4]) );
// snfinhv  N18( .D(vbp_cgm), .G(net364), .S(gd), .B(gd) );
// snfinhv  N20_4( .D(ilv[4]), .G(net364), .S(gd), .B(gd) );
// snfinhv  N20_3( .D(ilv[3]), .G(net364), .S(gd), .B(gd) );
// snfinhv  N20_2( .D(ilv[2]), .G(net364), .S(gd), .B(gd) );
// snfinhv  N20_1( .D(ilv[1]), .G(net364), .S(gd), .B(gd) );
// snfinhv  N20_0( .D(ilv[0]), .G(net364), .S(gd), .B(gd) );
// snfinhv  N22_4( .D(net209[0]), .G(net364), .S(gd), .B(gd) );
// snfinhv  N22_3( .D(net209[1]), .G(net364), .S(gd), .B(gd) );
void mipi_4_rx_dphy_ns_DPHYCB_preamp_bias::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vph), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHYCB_preamp_bias::assign_enable() {
    vl::wr(enable, vl::land(vl::ceq(vl::rd(rx_pre_pon_h), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(rx_pre_pon_hz), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHYCB_preamp_bias::proc_initial_151641() {
    vl::wr(ibias_d2c_bias_r, vl::logic<5>(0x0ULL));
    vl::wr(ihv_r, vl::logic<5>(0x0ULL));
    vl::wr(ilv_r, vl::logic<5>(0x0ULL));
}

void mipi_4_rx_dphy_ns_DPHYCB_preamp_bias::proc_always_151647() {
    for (;;) {
        wait(supply_ok.value_changed_event() | enable.value_changed_event());
        if (vl::rd(supply_ok).test()) {
            if (vl::rd(enable).test()) {
                wait(vl::dly(1000.0, TU_, TP_));
                vl::wr(ibias_d2c_bias_r, vl::logic<5>(0x1fULL));
                vl::wr(ihv_r, vl::logic<5>(0x1fULL));
                vl::wr(ilv_r, vl::logic<5>(0x1fULL));
            } else {
                vl::wr(ibias_d2c_bias_r, vl::logic<5>(0x0ULL));
                vl::wr(ihv_r, vl::logic<5>(0x0ULL));
                vl::wr(ilv_r, vl::logic<5>(0x0ULL));
            }
        } else {
            vl::wr(ibias_d2c_bias_r, vl::logic<5>::all_z());
            vl::wr(ihv_r, vl::logic<5>::all_z());
            vl::wr(ilv_r, vl::logic<5>::all_z());
        }
    }
}
