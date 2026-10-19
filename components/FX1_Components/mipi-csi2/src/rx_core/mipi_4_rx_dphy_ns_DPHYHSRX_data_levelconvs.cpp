//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs.h"

mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs::mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_atb_sw);
    sensitive << atb_sw;
    SC_METHOD(split_data_eq);
    sensitive << data_eq;
    SC_METHOD(split_lb_prog);
    sensitive << lb_prog;
    SC_METHOD(merge_atb_swhv);
    sensitive << atb_swhv_b[0] << atb_swhv_b[1] << atb_swhv_b[2] << atb_swhv_b[3];
    SC_METHOD(merge_atb_swhvz);
    sensitive << atb_swhvz_b[0] << atb_swhvz_b[1] << atb_swhvz_b[2] << atb_swhvz_b[3];
    SC_METHOD(merge_data_eq_hv);
    sensitive << data_eq_hv_b[0] << data_eq_hv_b[1] << data_eq_hv_b[2];
    SC_METHOD(merge_data_eq_hvz);
    sensitive << data_eq_hvz_b[0] << data_eq_hvz_b[1] << data_eq_hvz_b[2];
    SC_METHOD(merge_lb_prog_hv);
    sensitive << lb_prog_hv_b[0] << lb_prog_hv_b[1] << lb_prog_hv_b[2] << lb_prog_hv_b[3];
    lv2hvctrl.reset(new mipi_4_rx_dphy_ns_LEVELCONV_vp_det_soc("lv2hvctrl"));
    lv2hvctrl->A_in(enlogic);
    lv2hvctrl->Z(enlogic_hv);
    lv2hvctrl->Z_neg(enlogic_hvz);
    lv2hvctrl->gd(gd);
    lv2hvctrl->vph(vph);
    lv2hvctrl->vss(vss);
    I46.reset(new mipi_4_rx_dphy_ns_tieLowHigh_HV("I46"));
    I46->high(tie_high_hv);
    I46->low(tie_low_hv);
    I46->gd(gd);
    I46->vph(vph);
    ref.reset(new mipi_4_rx_dphy_ns_LEVELCONV_bias("ref"));
    ref->onz_hv(enlogic_hvz);
    ref->tie_low_hv(tie_low_hv);
    ref->vblvhv(vblvhv);
    ref->gd(gd);
    ref->vph(vph);
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
    I58_2.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I58_2"));
    I58_2->A_in(data_eq_b[2]);
    I58_2->onz_hv(enlogic_hvz);
    I58_2->tie_low_hv(tie_low_hv);
    I58_2->vblvhv(vblvhv);
    I58_2->Z(data_eq_hv_b[2]);
    I58_2->Z_neg(data_eq_hvz_b[2]);
    I58_2->gd(gd);
    I58_2->vdd(vdd);
    I58_2->vph(vph);
    I58_2->vss(vss);
    I58_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I58_1"));
    I58_1->A_in(data_eq_b[1]);
    I58_1->onz_hv(enlogic_hvz);
    I58_1->tie_low_hv(tie_low_hv);
    I58_1->vblvhv(vblvhv);
    I58_1->Z(data_eq_hv_b[1]);
    I58_1->Z_neg(data_eq_hvz_b[1]);
    I58_1->gd(gd);
    I58_1->vdd(vdd);
    I58_1->vph(vph);
    I58_1->vss(vss);
    I58_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I58_0"));
    I58_0->A_in(data_eq_b[0]);
    I58_0->onz_hv(enlogic_hvz);
    I58_0->tie_low_hv(tie_low_hv);
    I58_0->vblvhv(vblvhv);
    I58_0->Z(data_eq_hv_b[0]);
    I58_0->Z_neg(data_eq_hvz_b[0]);
    I58_0->gd(gd);
    I58_0->vdd(vdd);
    I58_0->vph(vph);
    I58_0->vss(vss);
    I62_4.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I62_4"));
    I62_4->A_in(enterm);
    I62_4->onz_hv(enlogic_hvz);
    I62_4->tie_low_hv(tie_low_hv);
    I62_4->vblvhv(vblvhv);
    I62_4->Z(entermhv_int);
    I62_4->Z_neg(entermhvz);
    I62_4->gd(gd);
    I62_4->vdd(vdd);
    I62_4->vph(vph);
    I62_4->vss(vss);
    I62_3.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I62_3"));
    I62_3->A_in(pon);
    I62_3->onz_hv(enlogic_hvz);
    I62_3->tie_low_hv(tie_low_hv);
    I62_3->vblvhv(vblvhv);
    I62_3->Z(ponhv);
    I62_3->Z_neg(ponhvz);
    I62_3->gd(gd);
    I62_3->vdd(vdd);
    I62_3->vph(vph);
    I62_3->vss(vss);
    I62_2.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I62_2"));
    I62_2->A_in(ena);
    I62_2->onz_hv(enlogic_hvz);
    I62_2->tie_low_hv(tie_low_hv);
    I62_2->vblvhv(vblvhv);
    I62_2->Z(enahv);
    I62_2->Z_neg(enahvz);
    I62_2->gd(gd);
    I62_2->vdd(vdd);
    I62_2->vph(vph);
    I62_2->vss(vss);
    I62_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I62_1"));
    I62_1->A_in(feed_int_clk);
    I62_1->onz_hv(enlogic_hvz);
    I62_1->tie_low_hv(tie_low_hv);
    I62_1->vblvhv(vblvhv);
    I62_1->Z(feed_int_clk_hv);
    I62_1->Z_neg(feed_int_clk_hvz);
    I62_1->gd(gd);
    I62_1->vdd(vdd);
    I62_1->vph(vph);
    I62_1->vss(vss);
    I62_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I62_0"));
    I62_0->A_in(offset_cal_en);
    I62_0->onz_hv(enlogic_hvz);
    I62_0->tie_low_hv(tie_low_hv);
    I62_0->vblvhv(vblvhv);
    I62_0->Z(offset_cal_en_hv);
    I62_0->Z_neg(offset_cal_en_hvz);
    I62_0->gd(gd);
    I62_0->vdd(vdd);
    I62_0->vph(vph);
    I62_0->vss(vss);
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
    I83_3->Z_neg(net160[3]);
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
    I83_2->Z_neg(net160[2]);
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
    I83_1->Z_neg(net160[1]);
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
    I83_0->Z_neg(net160[0]);
    I83_0->gd(gd);
    I83_0->vdd(vdd);
    I83_0->vph(vph);
    I83_0->vss(vss);
    I73.reset(new mipi_4_rx_dphy_ns_sctg_inv16x("I73"));
    I73->a(net190);
    I73->z(entermhv);
    I73->gd(gd);
    I73->vph(vph);
    I72.reset(new mipi_4_rx_dphy_ns_sctg_inv8x("I72"));
    I72->a(entermhv_int);
    I72->z(net190);
    I72->gd(gd);
    I72->vph(vph);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs::split_atb_sw() {
    const vl::logic<4> v = vl::rd(atb_sw);
    atb_sw_b[0].write(v.bit(0).sl());
    atb_sw_b[1].write(v.bit(1).sl());
    atb_sw_b[2].write(v.bit(2).sl());
    atb_sw_b[3].write(v.bit(3).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs::split_data_eq() {
    const vl::logic<3> v = vl::rd(data_eq);
    data_eq_b[0].write(v.bit(0).sl());
    data_eq_b[1].write(v.bit(1).sl());
    data_eq_b[2].write(v.bit(2).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs::split_lb_prog() {
    const vl::logic<4> v = vl::rd(lb_prog);
    lb_prog_b[0].write(v.bit(0).sl());
    lb_prog_b[1].write(v.bit(1).sl());
    lb_prog_b[2].write(v.bit(2).sl());
    lb_prog_b[3].write(v.bit(3).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs::merge_atb_swhv() {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(atb_swhv_b[0]));
    v.set_bit(1, vl::rd(atb_swhv_b[1]));
    v.set_bit(2, vl::rd(atb_swhv_b[2]));
    v.set_bit(3, vl::rd(atb_swhv_b[3]));
    vl::wr(atb_swhv, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs::merge_atb_swhvz() {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(atb_swhvz_b[0]));
    v.set_bit(1, vl::rd(atb_swhvz_b[1]));
    v.set_bit(2, vl::rd(atb_swhvz_b[2]));
    v.set_bit(3, vl::rd(atb_swhvz_b[3]));
    vl::wr(atb_swhvz, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs::merge_data_eq_hv() {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(data_eq_hv_b[0]));
    v.set_bit(1, vl::rd(data_eq_hv_b[1]));
    v.set_bit(2, vl::rd(data_eq_hv_b[2]));
    vl::wr(data_eq_hv, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs::merge_data_eq_hvz() {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(data_eq_hvz_b[0]));
    v.set_bit(1, vl::rd(data_eq_hvz_b[1]));
    v.set_bit(2, vl::rd(data_eq_hvz_b[2]));
    vl::wr(data_eq_hvz, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs::merge_lb_prog_hv() {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(lb_prog_hv_b[0]));
    v.set_bit(1, vl::rd(lb_prog_hv_b[1]));
    v.set_bit(2, vl::rd(lb_prog_hv_b[2]));
    v.set_bit(3, vl::rd(lb_prog_hv_b[3]));
    vl::wr(lb_prog_hv, v);
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs::rd_atb_sw() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(atb_sw_b[0]));
    v.set_bit(1, vl::rd(atb_sw_b[1]));
    v.set_bit(2, vl::rd(atb_sw_b[2]));
    v.set_bit(3, vl::rd(atb_sw_b[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs::rd_atb_swhv() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(atb_swhv_b[0]));
    v.set_bit(1, vl::rd(atb_swhv_b[1]));
    v.set_bit(2, vl::rd(atb_swhv_b[2]));
    v.set_bit(3, vl::rd(atb_swhv_b[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs::rd_atb_swhvz() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(atb_swhvz_b[0]));
    v.set_bit(1, vl::rd(atb_swhvz_b[1]));
    v.set_bit(2, vl::rd(atb_swhvz_b[2]));
    v.set_bit(3, vl::rd(atb_swhvz_b[3]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs::rd_data_eq() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(data_eq_b[0]));
    v.set_bit(1, vl::rd(data_eq_b[1]));
    v.set_bit(2, vl::rd(data_eq_b[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs::rd_data_eq_hv() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(data_eq_hv_b[0]));
    v.set_bit(1, vl::rd(data_eq_hv_b[1]));
    v.set_bit(2, vl::rd(data_eq_hv_b[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs::rd_data_eq_hvz() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(data_eq_hvz_b[0]));
    v.set_bit(1, vl::rd(data_eq_hvz_b[1]));
    v.set_bit(2, vl::rd(data_eq_hvz_b[2]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs::rd_lb_prog() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(lb_prog_b[0]));
    v.set_bit(1, vl::rd(lb_prog_b[1]));
    v.set_bit(2, vl::rd(lb_prog_b[2]));
    v.set_bit(3, vl::rd(lb_prog_b[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs::rd_lb_prog_hv() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(lb_prog_hv_b[0]));
    v.set_bit(1, vl::rd(lb_prog_hv_b[1]));
    v.set_bit(2, vl::rd(lb_prog_hv_b[2]));
    v.set_bit(3, vl::rd(lb_prog_hv_b[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs::rd_net160() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(net160[0]));
    v.set_bit(1, vl::rd(net160[1]));
    v.set_bit(2, vl::rd(net160[2]));
    v.set_bit(3, vl::rd(net160[3]));
    return v;
}
