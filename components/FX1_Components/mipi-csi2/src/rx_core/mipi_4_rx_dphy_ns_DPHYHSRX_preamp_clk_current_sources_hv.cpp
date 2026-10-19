//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk_current_sources_hv
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk_current_sources_hv.h"

mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk_current_sources_hv::mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk_current_sources_hv(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_lbkeyhv);
    sensitive << lbkeyhv;
    SC_METHOD(gate_ihv_int_buf0);
    sensitive << vph << onhv;
    SC_METHOD(gate_ihv_int_buf1);
    sensitive << vph << onhv;
    I0_CALL.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("I0_CALL"));
    I0_CALL->bias_h(ihv_int);
    I0_CALL->on_hz(keyhv);
    I0_CALL->o(ibias_cal);
    I0_CALL->vph(vph);
    I0_CALR.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("I0_CALR"));
    I0_CALR->bias_h(ihv_int);
    I0_CALR->on_hz(keyhv);
    I0_CALR->o(ibias_cal);
    I0_CALR->vph(vph);
    I22_7.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("I22_7"));
    I22_7->bias_h(ihv_int);
    I22_7->on_hz(lbkeyhv_b[3]);
    I22_7->o(ibias_lb);
    I22_7->vph(vph);
    I22_6.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("I22_6"));
    I22_6->bias_h(ihv_int);
    I22_6->on_hz(lbkeyhv_b[3]);
    I22_6->o(ibias_lb);
    I22_6->vph(vph);
    I22_5.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("I22_5"));
    I22_5->bias_h(ihv_int);
    I22_5->on_hz(lbkeyhv_b[3]);
    I22_5->o(ibias_lb);
    I22_5->vph(vph);
    I22_4.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("I22_4"));
    I22_4->bias_h(ihv_int);
    I22_4->on_hz(lbkeyhv_b[3]);
    I22_4->o(ibias_lb);
    I22_4->vph(vph);
    I22_3.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("I22_3"));
    I22_3->bias_h(ihv_int);
    I22_3->on_hz(lbkeyhv_b[3]);
    I22_3->o(ibias_lb);
    I22_3->vph(vph);
    I22_2.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("I22_2"));
    I22_2->bias_h(ihv_int);
    I22_2->on_hz(lbkeyhv_b[3]);
    I22_2->o(ibias_lb);
    I22_2->vph(vph);
    I22_1.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("I22_1"));
    I22_1->bias_h(ihv_int);
    I22_1->on_hz(lbkeyhv_b[3]);
    I22_1->o(ibias_lb);
    I22_1->vph(vph);
    I22_0.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("I22_0"));
    I22_0->bias_h(ihv_int);
    I22_0->on_hz(lbkeyhv_b[3]);
    I22_0->o(ibias_lb);
    I22_0->vph(vph);
    I23_1.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("I23_1"));
    I23_1->bias_h(ihv_int);
    I23_1->on_hz(lbkeyhv_b[1]);
    I23_1->o(ibias_lb);
    I23_1->vph(vph);
    I23_0.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("I23_0"));
    I23_0->bias_h(ihv_int);
    I23_0->on_hz(lbkeyhv_b[1]);
    I23_0->o(ibias_lb);
    I23_0->vph(vph);
    I24_3.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("I24_3"));
    I24_3->bias_h(ihv_int);
    I24_3->on_hz(lbkeyhv_b[2]);
    I24_3->o(ibias_lb);
    I24_3->vph(vph);
    I24_2.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("I24_2"));
    I24_2->bias_h(ihv_int);
    I24_2->on_hz(lbkeyhv_b[2]);
    I24_2->o(ibias_lb);
    I24_2->vph(vph);
    I24_1.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("I24_1"));
    I24_1->bias_h(ihv_int);
    I24_1->on_hz(lbkeyhv_b[2]);
    I24_1->o(ibias_lb);
    I24_1->vph(vph);
    I24_0.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("I24_0"));
    I24_0->bias_h(ihv_int);
    I24_0->on_hz(lbkeyhv_b[2]);
    I24_0->o(ibias_lb);
    I24_0->vph(vph);
    I25.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("I25"));
    I25->bias_h(ihv_int);
    I25->on_hz(lbkeyhv_b[0]);
    I25->o(ibias_lb);
    I25->vph(vph);
    IEQL_9.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("IEQL_9"));
    IEQL_9->bias_h(ihv_int);
    IEQL_9->on_hz(keyhv);
    IEQL_9->o(ibias);
    IEQL_9->vph(vph);
    IEQL_8.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("IEQL_8"));
    IEQL_8->bias_h(ihv_int);
    IEQL_8->on_hz(keyhv);
    IEQL_8->o(ibias);
    IEQL_8->vph(vph);
    IEQL_7.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("IEQL_7"));
    IEQL_7->bias_h(ihv_int);
    IEQL_7->on_hz(keyhv);
    IEQL_7->o(ibias);
    IEQL_7->vph(vph);
    IEQL_6.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("IEQL_6"));
    IEQL_6->bias_h(ihv_int);
    IEQL_6->on_hz(keyhv);
    IEQL_6->o(ibias);
    IEQL_6->vph(vph);
    IEQL_5.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("IEQL_5"));
    IEQL_5->bias_h(ihv_int);
    IEQL_5->on_hz(keyhv);
    IEQL_5->o(ibias);
    IEQL_5->vph(vph);
    IEQL_4.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("IEQL_4"));
    IEQL_4->bias_h(ihv_int);
    IEQL_4->on_hz(keyhv);
    IEQL_4->o(ibias);
    IEQL_4->vph(vph);
    IEQL_3.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("IEQL_3"));
    IEQL_3->bias_h(ihv_int);
    IEQL_3->on_hz(keyhv);
    IEQL_3->o(ibias);
    IEQL_3->vph(vph);
    IEQL_2.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("IEQL_2"));
    IEQL_2->bias_h(ihv_int);
    IEQL_2->on_hz(keyhv);
    IEQL_2->o(ibias);
    IEQL_2->vph(vph);
    IEQL_1.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("IEQL_1"));
    IEQL_1->bias_h(ihv_int);
    IEQL_1->on_hz(keyhv);
    IEQL_1->o(ibias);
    IEQL_1->vph(vph);
    IEQL_0.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv("IEQL_0"));
    IEQL_0->bias_h(ihv_int);
    IEQL_0->on_hz(keyhv);
    IEQL_0->o(ibias);
    IEQL_0->vph(vph);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk_current_sources_hv::split_lbkeyhv() {
    const vl::logic<4> v = vl::rd(lbkeyhv);
    lbkeyhv_b[0].write(v.bit(0).sl());
    lbkeyhv_b[1].write(v.bit(1).sl());
    lbkeyhv_b[2].write(v.bit(2).sl());
    lbkeyhv_b[3].write(v.bit(3).sl());
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk_current_sources_hv::rd_lbkeyhv() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(lbkeyhv_b[0]));
    v.set_bit(1, vl::rd(lbkeyhv_b[1]));
    v.set_bit(2, vl::rd(lbkeyhv_b[2]));
    v.set_bit(3, vl::rd(lbkeyhv_b[3]));
    return v;
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk_current_sources_hv::gate_ihv_int_buf0() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x0ULL), vl::land(vl::ceq(vl::rd(vph), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(onhv), vl::logic<1>(0x0ULL))));
    vl::wr(ihv_int, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk_current_sources_hv::gate_ihv_int_buf1() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x1ULL), vl::land(vl::ceq(vl::rd(vph), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(onhv), vl::logic<1>(0x1ULL))));
    vl::wr(ihv_int, v);
}
