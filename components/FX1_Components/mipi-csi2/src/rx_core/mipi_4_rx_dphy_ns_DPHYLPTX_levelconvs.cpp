//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYLPTX_levelconvs
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYLPTX_levelconvs.h"

mipi_4_rx_dphy_ns_DPHYLPTX_levelconvs::mipi_4_rx_dphy_ns_DPHYLPTX_levelconvs(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_din);
    sensitive << din;
    SC_METHOD(split_eniocont);
    sensitive << eniocont;
    SC_METHOD(merge_dinhv);
    sensitive << dinhv_b[0] << dinhv_b[1];
    SC_METHOD(merge_dinhvz);
    sensitive << dinhvz_b[0] << dinhvz_b[1];
    SC_METHOD(merge_enioconthv);
    sensitive << enioconthv_b[0] << enioconthv_b[1];
    SC_METHOD(merge_enioconthvz);
    sensitive << enioconthvz_b[0] << enioconthvz_b[1];
    ref.reset(new mipi_4_rx_dphy_ns_LEVELCONV_bias("ref"));
    ref->onz_hv(enlogic_hvz);
    ref->tie_low_hv(tie_low_hv);
    ref->vblvhv(vblvhv);
    ref->gd(gd);
    ref->vph(vph);
    lv2hvctrl_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_vp_det_soc("lv2hvctrl_1"));
    lv2hvctrl_1->A_in(enlogic);
    lv2hvctrl_1->Z(enlogic_hv);
    lv2hvctrl_1->Z_neg(enlogic_hvz);
    lv2hvctrl_1->gd(gd);
    lv2hvctrl_1->vph(vph);
    lv2hvctrl_1->vss(vss);
    lv2hvctrl_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_vp_det_soc("lv2hvctrl_0"));
    lv2hvctrl_0->A_in(enpulldwn);
    lv2hvctrl_0->Z(enpulldwnhv);
    lv2hvctrl_0->Z_neg(enpulldwnhvz);
    lv2hvctrl_0->gd(gd);
    lv2hvctrl_0->vph(vph);
    lv2hvctrl_0->vss(vss);
    lv2hv_6.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("lv2hv_6"));
    lv2hv_6->A_in(enulp);
    lv2hv_6->onz_hv(enlogic_hvz);
    lv2hv_6->tie_low_hv(tie_low_hv);
    lv2hv_6->vblvhv(vblvhv);
    lv2hv_6->Z(enulphv);
    lv2hv_6->Z_neg(enulphvz);
    lv2hv_6->gd(gd);
    lv2hv_6->vdd(vdd);
    lv2hv_6->vph(vph);
    lv2hv_6->vss(vss);
    lv2hv_5.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("lv2hv_5"));
    lv2hv_5->A_in(pon);
    lv2hv_5->onz_hv(enlogic_hvz);
    lv2hv_5->tie_low_hv(tie_low_hv);
    lv2hv_5->vblvhv(vblvhv);
    lv2hv_5->Z(ponhv);
    lv2hv_5->Z_neg(ponhvz);
    lv2hv_5->gd(gd);
    lv2hv_5->vdd(vdd);
    lv2hv_5->vph(vph);
    lv2hv_5->vss(vss);
    lv2hv_4.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("lv2hv_4"));
    lv2hv_4->A_in(ena);
    lv2hv_4->onz_hv(enlogic_hvz);
    lv2hv_4->tie_low_hv(tie_low_hv);
    lv2hv_4->vblvhv(vblvhv);
    lv2hv_4->Z(enahv);
    lv2hv_4->Z_neg(enahvz);
    lv2hv_4->gd(gd);
    lv2hv_4->vdd(vdd);
    lv2hv_4->vph(vph);
    lv2hv_4->vss(vss);
    lv2hv_3.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("lv2hv_3"));
    lv2hv_3->A_in(din_b[1]);
    lv2hv_3->onz_hv(enlogic_hvz);
    lv2hv_3->tie_low_hv(tie_low_hv);
    lv2hv_3->vblvhv(vblvhv);
    lv2hv_3->Z(dinhv_b[1]);
    lv2hv_3->Z_neg(dinhvz_b[1]);
    lv2hv_3->gd(gd);
    lv2hv_3->vdd(vdd);
    lv2hv_3->vph(vph);
    lv2hv_3->vss(vss);
    lv2hv_2.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("lv2hv_2"));
    lv2hv_2->A_in(din_b[0]);
    lv2hv_2->onz_hv(enlogic_hvz);
    lv2hv_2->tie_low_hv(tie_low_hv);
    lv2hv_2->vblvhv(vblvhv);
    lv2hv_2->Z(dinhv_b[0]);
    lv2hv_2->Z_neg(dinhvz_b[0]);
    lv2hv_2->gd(gd);
    lv2hv_2->vdd(vdd);
    lv2hv_2->vph(vph);
    lv2hv_2->vss(vss);
    lv2hv_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("lv2hv_1"));
    lv2hv_1->A_in(eniocont_b[1]);
    lv2hv_1->onz_hv(enlogic_hvz);
    lv2hv_1->tie_low_hv(tie_low_hv);
    lv2hv_1->vblvhv(vblvhv);
    lv2hv_1->Z(enioconthv_b[1]);
    lv2hv_1->Z_neg(enioconthvz_b[1]);
    lv2hv_1->gd(gd);
    lv2hv_1->vdd(vdd);
    lv2hv_1->vph(vph);
    lv2hv_1->vss(vss);
    lv2hv_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("lv2hv_0"));
    lv2hv_0->A_in(eniocont_b[0]);
    lv2hv_0->onz_hv(enlogic_hvz);
    lv2hv_0->tie_low_hv(tie_low_hv);
    lv2hv_0->vblvhv(vblvhv);
    lv2hv_0->Z(enioconthv_b[0]);
    lv2hv_0->Z_neg(enioconthvz_b[0]);
    lv2hv_0->gd(gd);
    lv2hv_0->vdd(vdd);
    lv2hv_0->vph(vph);
    lv2hv_0->vss(vss);
    lv2hv_7.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("lv2hv_7"));
    lv2hv_7->A_in(atb_lptx1200_on);
    lv2hv_7->onz_hv(enlogic_hvz);
    lv2hv_7->tie_low_hv(tie_low_hv);
    lv2hv_7->vblvhv(vblvhv);
    lv2hv_7->Z(atb_lptx1200_on_hv);
    lv2hv_7->Z_neg(atb_lptx1200_on_hvz);
    lv2hv_7->gd(gd);
    lv2hv_7->vdd(vdd);
    lv2hv_7->vph(vph);
    lv2hv_7->vss(vss);
    sr_bypas.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("sr_bypas"));
    sr_bypas->A_in(sr_bypass);
    sr_bypas->onz_hv(enlogic_hvz);
    sr_bypas->tie_low_hv(tie_low_hv);
    sr_bypas->vblvhv(vblvhv);
    sr_bypas->Z(sr_bypasshv);
    sr_bypas->Z_neg(net126);
    sr_bypas->gd(gd);
    sr_bypas->vdd(vdd);
    sr_bypas->vph(vph);
    sr_bypas->vss(vss);
    I46.reset(new mipi_4_rx_dphy_ns_tieLowHigh_HV("I46"));
    I46->high(tie_high_hv);
    I46->low(tie_low_hv);
    I46->gd(gd);
    I46->vph(vph);
}

void mipi_4_rx_dphy_ns_DPHYLPTX_levelconvs::split_din() {
    const vl::logic<2> v = vl::rd(din);
    din_b[0].write(v.bit(0).sl());
    din_b[1].write(v.bit(1).sl());
}

void mipi_4_rx_dphy_ns_DPHYLPTX_levelconvs::split_eniocont() {
    const vl::logic<2> v = vl::rd(eniocont);
    eniocont_b[0].write(v.bit(0).sl());
    eniocont_b[1].write(v.bit(1).sl());
}

void mipi_4_rx_dphy_ns_DPHYLPTX_levelconvs::merge_dinhv() {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(dinhv_b[0]));
    v.set_bit(1, vl::rd(dinhv_b[1]));
    vl::wr(dinhv, v);
}

void mipi_4_rx_dphy_ns_DPHYLPTX_levelconvs::merge_dinhvz() {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(dinhvz_b[0]));
    v.set_bit(1, vl::rd(dinhvz_b[1]));
    vl::wr(dinhvz, v);
}

void mipi_4_rx_dphy_ns_DPHYLPTX_levelconvs::merge_enioconthv() {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(enioconthv_b[0]));
    v.set_bit(1, vl::rd(enioconthv_b[1]));
    vl::wr(enioconthv, v);
}

void mipi_4_rx_dphy_ns_DPHYLPTX_levelconvs::merge_enioconthvz() {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(enioconthvz_b[0]));
    v.set_bit(1, vl::rd(enioconthvz_b[1]));
    vl::wr(enioconthvz, v);
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYLPTX_levelconvs::rd_din() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(din_b[0]));
    v.set_bit(1, vl::rd(din_b[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYLPTX_levelconvs::rd_dinhv() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(dinhv_b[0]));
    v.set_bit(1, vl::rd(dinhv_b[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYLPTX_levelconvs::rd_dinhvz() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(dinhvz_b[0]));
    v.set_bit(1, vl::rd(dinhvz_b[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYLPTX_levelconvs::rd_eniocont() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(eniocont_b[0]));
    v.set_bit(1, vl::rd(eniocont_b[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYLPTX_levelconvs::rd_enioconthv() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(enioconthv_b[0]));
    v.set_bit(1, vl::rd(enioconthv_b[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYLPTX_levelconvs::rd_enioconthvz() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(enioconthvz_b[0]));
    v.set_bit(1, vl::rd(enioconthvz_b[1]));
    return v;
}
