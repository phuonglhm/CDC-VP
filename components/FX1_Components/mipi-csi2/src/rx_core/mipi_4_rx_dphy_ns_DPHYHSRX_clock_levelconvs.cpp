//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_clock_levelconvs
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_clock_levelconvs.h"

mipi_4_rx_dphy_ns_DPHYHSRX_clock_levelconvs::mipi_4_rx_dphy_ns_DPHYHSRX_clock_levelconvs(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_atb_sw);
    sensitive << atb_sw;
    SC_METHOD(split_lb_prog);
    sensitive << lb_prog;
    SC_METHOD(merge_atb_swhv);
    sensitive << atb_swhv_b[0] << atb_swhv_b[1] << atb_swhv_b[2] << atb_swhv_b[3];
    SC_METHOD(merge_atb_swhvz);
    sensitive << atb_swhvz_b[0] << atb_swhvz_b[1] << atb_swhvz_b[2] << atb_swhvz_b[3];
    SC_METHOD(merge_lb_prog_hv);
    sensitive << lb_prog_hv_b[0] << lb_prog_hv_b[1] << lb_prog_hv_b[2] << lb_prog_hv_b[3];
    I72.reset(new mipi_4_rx_dphy_ns_sctg_inv8x("I72"));
    I72->a(entermhv_int);
    I72->z(net142);
    I72->gd(gd);
    I72->vph(vph);
    ref.reset(new mipi_4_rx_dphy_ns_LEVELCONV_bias("ref"));
    ref->onz_hv(enlogic_hvz);
    ref->tie_low_hv(tie_low_hv);
    ref->vblvhv(vblvhv);
    ref->gd(gd);
    ref->vph(vph);
    lv2hvctrl.reset(new mipi_4_rx_dphy_ns_LEVELCONV_vp_det_soc("lv2hvctrl"));
    lv2hvctrl->A_in(enlogic);
    lv2hvctrl->Z(enlogic_hv);
    lv2hvctrl->Z_neg(enlogic_hvz);
    lv2hvctrl->gd(gd);
    lv2hvctrl->vph(vph);
    lv2hvctrl->vss(vss);
    I48_3.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I48_3"));
    I48_3->A_in(atb_sw_b[3]);
    I48_3->onz_hv(enlogic_hvz);
    I48_3->tie_low_hv(tie_low_hv);
    I48_3->vblvhv(vblvhv);
    I48_3->Z(atb_swhv_b[3]);
    I48_3->Z_neg(atb_swhvz_b[3]);
    I48_3->gd(gd);
    I48_3->vdd(vdd);
    I48_3->vph(vph);
    I48_3->vss(vss);
    I48_2.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I48_2"));
    I48_2->A_in(atb_sw_b[2]);
    I48_2->onz_hv(enlogic_hvz);
    I48_2->tie_low_hv(tie_low_hv);
    I48_2->vblvhv(vblvhv);
    I48_2->Z(atb_swhv_b[2]);
    I48_2->Z_neg(atb_swhvz_b[2]);
    I48_2->gd(gd);
    I48_2->vdd(vdd);
    I48_2->vph(vph);
    I48_2->vss(vss);
    I48_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I48_1"));
    I48_1->A_in(atb_sw_b[1]);
    I48_1->onz_hv(enlogic_hvz);
    I48_1->tie_low_hv(tie_low_hv);
    I48_1->vblvhv(vblvhv);
    I48_1->Z(atb_swhv_b[1]);
    I48_1->Z_neg(atb_swhvz_b[1]);
    I48_1->gd(gd);
    I48_1->vdd(vdd);
    I48_1->vph(vph);
    I48_1->vss(vss);
    I48_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I48_0"));
    I48_0->A_in(atb_sw_b[0]);
    I48_0->onz_hv(enlogic_hvz);
    I48_0->tie_low_hv(tie_low_hv);
    I48_0->vblvhv(vblvhv);
    I48_0->Z(atb_swhv_b[0]);
    I48_0->Z_neg(atb_swhvz_b[0]);
    I48_0->gd(gd);
    I48_0->vdd(vdd);
    I48_0->vph(vph);
    I48_0->vss(vss);
    I66.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I66"));
    I66->A_in(osc_en);
    I66->onz_hv(enlogic_hvz);
    I66->tie_low_hv(tie_low_hv);
    I66->vblvhv(vblvhv);
    I66->Z(osc_enhv);
    I66->Z_neg(osc_enhvz);
    I66->gd(gd);
    I66->vdd(vdd);
    I66->vph(vph);
    I66->vss(vss);
    I82_2.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I82_2"));
    I82_2->A_in(lb_res_dp_en);
    I82_2->onz_hv(enlogic_hvz);
    I82_2->tie_low_hv(tie_low_hv);
    I82_2->vblvhv(vblvhv);
    I82_2->Z(lb_res_dp_en_hv);
    I82_2->Z_neg(lb_res_dp_enz_hv);
    I82_2->gd(gd);
    I82_2->vdd(vdd);
    I82_2->vph(vph);
    I82_2->vss(vss);
    I82_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I82_1"));
    I82_1->A_in(lb_res_dn_en);
    I82_1->onz_hv(enlogic_hvz);
    I82_1->tie_low_hv(tie_low_hv);
    I82_1->vblvhv(vblvhv);
    I82_1->Z(lb_res_dn_en_hv);
    I82_1->Z_neg(lb_res_dn_enz_hv);
    I82_1->gd(gd);
    I82_1->vdd(vdd);
    I82_1->vph(vph);
    I82_1->vss(vss);
    I82_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I82_0"));
    I82_0->A_in(lb_res_cm_en);
    I82_0->onz_hv(enlogic_hvz);
    I82_0->tie_low_hv(tie_low_hv);
    I82_0->vblvhv(vblvhv);
    I82_0->Z(lb_res_cm_en_hv);
    I82_0->Z_neg(lb_res_cm_enz_hv);
    I82_0->gd(gd);
    I82_0->vdd(vdd);
    I82_0->vph(vph);
    I82_0->vss(vss);
    I83_3.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I83_3"));
    I83_3->A_in(lb_prog_b[3]);
    I83_3->onz_hv(enlogic_hvz);
    I83_3->tie_low_hv(tie_low_hv);
    I83_3->vblvhv(vblvhv);
    I83_3->Z(lb_prog_hv_b[3]);
    I83_3->Z_neg(net127[3]);
    I83_3->gd(gd);
    I83_3->vdd(vdd);
    I83_3->vph(vph);
    I83_3->vss(vss);
    I83_2.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I83_2"));
    I83_2->A_in(lb_prog_b[2]);
    I83_2->onz_hv(enlogic_hvz);
    I83_2->tie_low_hv(tie_low_hv);
    I83_2->vblvhv(vblvhv);
    I83_2->Z(lb_prog_hv_b[2]);
    I83_2->Z_neg(net127[2]);
    I83_2->gd(gd);
    I83_2->vdd(vdd);
    I83_2->vph(vph);
    I83_2->vss(vss);
    I83_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I83_1"));
    I83_1->A_in(lb_prog_b[1]);
    I83_1->onz_hv(enlogic_hvz);
    I83_1->tie_low_hv(tie_low_hv);
    I83_1->vblvhv(vblvhv);
    I83_1->Z(lb_prog_hv_b[1]);
    I83_1->Z_neg(net127[1]);
    I83_1->gd(gd);
    I83_1->vdd(vdd);
    I83_1->vph(vph);
    I83_1->vss(vss);
    I83_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I83_0"));
    I83_0->A_in(lb_prog_b[0]);
    I83_0->onz_hv(enlogic_hvz);
    I83_0->tie_low_hv(tie_low_hv);
    I83_0->vblvhv(vblvhv);
    I83_0->Z(lb_prog_hv_b[0]);
    I83_0->Z_neg(net127[0]);
    I83_0->gd(gd);
    I83_0->vdd(vdd);
    I83_0->vph(vph);
    I83_0->vss(vss);
    lv2hv_5.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("lv2hv_5"));
    lv2hv_5->A_in(enterm);
    lv2hv_5->onz_hv(enlogic_hvz);
    lv2hv_5->tie_low_hv(tie_low_hv);
    lv2hv_5->vblvhv(vblvhv);
    lv2hv_5->Z(entermhv_int);
    lv2hv_5->Z_neg(entermhvz);
    lv2hv_5->gd(gd);
    lv2hv_5->vdd(vdd);
    lv2hv_5->vph(vph);
    lv2hv_5->vss(vss);
    lv2hv_4.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("lv2hv_4"));
    lv2hv_4->A_in(pon);
    lv2hv_4->onz_hv(enlogic_hvz);
    lv2hv_4->tie_low_hv(tie_low_hv);
    lv2hv_4->vblvhv(vblvhv);
    lv2hv_4->Z(ponhv);
    lv2hv_4->Z_neg(ponhvz);
    lv2hv_4->gd(gd);
    lv2hv_4->vdd(vdd);
    lv2hv_4->vph(vph);
    lv2hv_4->vss(vss);
    lv2hv_3.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("lv2hv_3"));
    lv2hv_3->A_in(ena);
    lv2hv_3->onz_hv(enlogic_hvz);
    lv2hv_3->tie_low_hv(tie_low_hv);
    lv2hv_3->vblvhv(vblvhv);
    lv2hv_3->Z(enahv);
    lv2hv_3->Z_neg(enahvz);
    lv2hv_3->gd(gd);
    lv2hv_3->vdd(vdd);
    lv2hv_3->vph(vph);
    lv2hv_3->vss(vss);
    lv2hv_2.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("lv2hv_2"));
    lv2hv_2->A_in(feed_int_clk);
    lv2hv_2->onz_hv(enlogic_hvz);
    lv2hv_2->tie_low_hv(tie_low_hv);
    lv2hv_2->vblvhv(vblvhv);
    lv2hv_2->Z(feed_int_clk_hv);
    lv2hv_2->Z_neg(feed_int_clk_hvz);
    lv2hv_2->gd(gd);
    lv2hv_2->vdd(vdd);
    lv2hv_2->vph(vph);
    lv2hv_2->vss(vss);
    lv2hv_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("lv2hv_1"));
    lv2hv_1->A_in(offset_cal_en);
    lv2hv_1->onz_hv(enlogic_hvz);
    lv2hv_1->tie_low_hv(tie_low_hv);
    lv2hv_1->vblvhv(vblvhv);
    lv2hv_1->Z(offset_cal_en_hv);
    lv2hv_1->Z_neg(offset_cal_en_hvz);
    lv2hv_1->gd(gd);
    lv2hv_1->vdd(vdd);
    lv2hv_1->vph(vph);
    lv2hv_1->vss(vss);
    lv2hv_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("lv2hv_0"));
    lv2hv_0->A_in(rxhs_rx_en200mode);
    lv2hv_0->onz_hv(enlogic_hvz);
    lv2hv_0->tie_low_hv(tie_low_hv);
    lv2hv_0->vblvhv(vblvhv);
    lv2hv_0->Z(rxhs_rx_en200mode_hv);
    lv2hv_0->Z_neg(rxhs_rx_en200mode_hvz);
    lv2hv_0->gd(gd);
    lv2hv_0->vdd(vdd);
    lv2hv_0->vph(vph);
    lv2hv_0->vss(vss);
    I46.reset(new mipi_4_rx_dphy_ns_tieLowHigh_HV("I46"));
    I46->high(tie_high_hv);
    I46->low(tie_low_hv);
    I46->gd(gd);
    I46->vph(vph);
    I73.reset(new mipi_4_rx_dphy_ns_sctg_inv16x("I73"));
    I73->a(net142);
    I73->z(entermhv);
    I73->gd(gd);
    I73->vph(vph);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_clock_levelconvs::split_atb_sw() {
    const vl::logic<4> v = vl::rd(atb_sw);
    atb_sw_b[0].write(v.bit(0).sl());
    atb_sw_b[1].write(v.bit(1).sl());
    atb_sw_b[2].write(v.bit(2).sl());
    atb_sw_b[3].write(v.bit(3).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_clock_levelconvs::split_lb_prog() {
    const vl::logic<4> v = vl::rd(lb_prog);
    lb_prog_b[0].write(v.bit(0).sl());
    lb_prog_b[1].write(v.bit(1).sl());
    lb_prog_b[2].write(v.bit(2).sl());
    lb_prog_b[3].write(v.bit(3).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_clock_levelconvs::merge_atb_swhv() {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(atb_swhv_b[0]));
    v.set_bit(1, vl::rd(atb_swhv_b[1]));
    v.set_bit(2, vl::rd(atb_swhv_b[2]));
    v.set_bit(3, vl::rd(atb_swhv_b[3]));
    vl::wr(atb_swhv, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_clock_levelconvs::merge_atb_swhvz() {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(atb_swhvz_b[0]));
    v.set_bit(1, vl::rd(atb_swhvz_b[1]));
    v.set_bit(2, vl::rd(atb_swhvz_b[2]));
    v.set_bit(3, vl::rd(atb_swhvz_b[3]));
    vl::wr(atb_swhvz, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_clock_levelconvs::merge_lb_prog_hv() {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(lb_prog_hv_b[0]));
    v.set_bit(1, vl::rd(lb_prog_hv_b[1]));
    v.set_bit(2, vl::rd(lb_prog_hv_b[2]));
    v.set_bit(3, vl::rd(lb_prog_hv_b[3]));
    vl::wr(lb_prog_hv, v);
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_clock_levelconvs::rd_atb_sw() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(atb_sw_b[0]));
    v.set_bit(1, vl::rd(atb_sw_b[1]));
    v.set_bit(2, vl::rd(atb_sw_b[2]));
    v.set_bit(3, vl::rd(atb_sw_b[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_clock_levelconvs::rd_atb_swhv() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(atb_swhv_b[0]));
    v.set_bit(1, vl::rd(atb_swhv_b[1]));
    v.set_bit(2, vl::rd(atb_swhv_b[2]));
    v.set_bit(3, vl::rd(atb_swhv_b[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_clock_levelconvs::rd_atb_swhvz() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(atb_swhvz_b[0]));
    v.set_bit(1, vl::rd(atb_swhvz_b[1]));
    v.set_bit(2, vl::rd(atb_swhvz_b[2]));
    v.set_bit(3, vl::rd(atb_swhvz_b[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_clock_levelconvs::rd_lb_prog() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(lb_prog_b[0]));
    v.set_bit(1, vl::rd(lb_prog_b[1]));
    v.set_bit(2, vl::rd(lb_prog_b[2]));
    v.set_bit(3, vl::rd(lb_prog_b[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_clock_levelconvs::rd_lb_prog_hv() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(lb_prog_hv_b[0]));
    v.set_bit(1, vl::rd(lb_prog_hv_b[1]));
    v.set_bit(2, vl::rd(lb_prog_hv_b[2]));
    v.set_bit(3, vl::rd(lb_prog_hv_b[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_clock_levelconvs::rd_net127() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(net127[0]));
    v.set_bit(1, vl::rd(net127[1]));
    v.set_bit(2, vl::rd(net127[2]));
    v.set_bit(3, vl::rd(net127[3]));
    return v;
}
