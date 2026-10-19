//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_mpll_l2h
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_mpll_l2h.h"

mipi_pll_dphy_mpll_l2h::mipi_pll_dphy_mpll_l2h(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_ck_l2h);
    sensitive << ck_l2h;
    SC_METHOD(merge_ph_pmix);
    sensitive << ph_pmix_b[0] << ph_pmix_b[1] << ph_pmix_b[2] << ph_pmix_b[3];
    del_iq1.reset(new mipi_pll_dphy_mpll_rx_dcc_del_x0("del_iq1"));
    del_iq1->clk_i_m(clkim1);
    del_iq1->clk_i_p(clkip1);
    del_iq1->gd(gd);
    del_iq1->hs(hs);
    del_iq1->pwr_on(pwr_on_rx);
    del_iq1->pwr_onz(pwr_onz_rx);
    del_iq1->vdcc(vci);
    del_iq1->vp(vreg_mpll);
    del_iq1->vp_tie(tie_high);
    del_iq1->ckdcc_out_m(ckdcc_i_m);
    del_iq1->ckdcc_out_p(ckdcc_i_p);
    del_iq1->clk_o_m(clk_i_m);
    del_iq1->clk_o_p(clk_i_p);
    del_iq1->ph_pmix_out_m(ph_pmix_b[2]);
    del_iq1->ph_pmix_out_p(ph_pmix_b[0]);
    del_iq2.reset(new mipi_pll_dphy_mpll_rx_dcc_del_x0("del_iq2"));
    del_iq2->clk_i_m(clkqm1);
    del_iq2->clk_i_p(clkqp1);
    del_iq2->gd(gd);
    del_iq2->hs(hs);
    del_iq2->pwr_on(pwr_on_rx);
    del_iq2->pwr_onz(pwr_onz_rx);
    del_iq2->vdcc(vcq);
    del_iq2->vp(vreg_mpll);
    del_iq2->vp_tie(tie_high);
    del_iq2->ckdcc_out_m(ckdcc_q_m);
    del_iq2->ckdcc_out_p(ckdcc_q_p);
    del_iq2->clk_o_m(clk_q_m);
    del_iq2->clk_o_p(clk_q_p);
    del_iq2->ph_pmix_out_m(ph_pmix_b[3]);
    del_iq2->ph_pmix_out_p(ph_pmix_b[1]);
    xor_.reset(new mipi_pll_dphy_mpll_rx_ckdrv_xor("xor_"));
    xor_->clk_i_m(clk_i_m);
    xor_->clk_i_p(clk_i_p);
    xor_->clk_q_m(clk_q_m);
    xor_->clk_q_p(clk_q_p);
    xor_->gd(gd);
    xor_->vp(vreg_mpll);
    xor_->xor_out(xor_clk);
    Idcc0.reset(new mipi_pll_dphy_mpll_l2h_dcc("Idcc0"));
    Idcc0->ck_l2h(ck_l2h_b[0]);
    Idcc0->gd(gd);
    Idcc0->pwr_on(pwr_on);
    Idcc0->pwr_onz(pwr_onz);
    Idcc0->vpgate(vcip);
    Idcc0->vreg_mpll(vreg_mpll);
    Idcc0->clk_out(clkip1);
    Idcc1.reset(new mipi_pll_dphy_mpll_l2h_dcc("Idcc1"));
    Idcc1->ck_l2h(ck_l2h_b[4]);
    Idcc1->gd(gd);
    Idcc1->pwr_on(pwr_on);
    Idcc1->pwr_onz(pwr_onz);
    Idcc1->vpgate(vcim);
    Idcc1->vreg_mpll(vreg_mpll);
    Idcc1->clk_out(clkim1);
    Idcc2.reset(new mipi_pll_dphy_mpll_l2h_dcc("Idcc2"));
    Idcc2->ck_l2h(ck_l2h_b[2]);
    Idcc2->gd(gd);
    Idcc2->pwr_on(pwr_on_rx);
    Idcc2->pwr_onz(pwr_onz_rx);
    Idcc2->vpgate(vcqp);
    Idcc2->vreg_mpll(vreg_mpll);
    Idcc2->clk_out(clkqp1);
    Idcc3.reset(new mipi_pll_dphy_mpll_l2h_dcc("Idcc3"));
    Idcc3->ck_l2h(ck_l2h_b[6]);
    Idcc3->gd(gd);
    Idcc3->pwr_on(pwr_on_rx);
    Idcc3->pwr_onz(pwr_onz_rx);
    Idcc3->vpgate(vcqm);
    Idcc3->vreg_mpll(vreg_mpll);
    Idcc3->clk_out(clkqm1);
    // nfinlvt  N34_3( .B(gd), .D(gd), .G(ck_l2h[1]), .S(gd) );
    // nfinlvt  N34_2( .B(gd), .D(gd), .G(ck_l2h[3]), .S(gd) );
    // nfinlvt  N34_1( .B(gd), .D(gd), .G(ck_l2h[5]), .S(gd) );
    // nfinlvt  N34_0( .B(gd), .D(gd), .G(ck_l2h[7]), .S(gd) );
    // nfinlvt  dumN1( .B(gd), .D(gd), .G(gd), .S(gd) );
    I39.reset(new mipi_pll_dphy_sc_tie_hilow_lvt("I39"));
    I39->gd(gd);
    I39->tie_high(tie_high);
    I39->tie_low(tie_low);
    I39->vp(vreg_mpll);
    I43.reset(new mipi_pll_dphy_sc_invx8_lvt("I43"));
    I43->a(pwr_onz);
    I43->z(pwr_on);
    I43->gd(gd);
    I43->vp(vreg_mpll);
    I48.reset(new mipi_pll_dphy_sc_invx8_lvt("I48"));
    I48->a(pwr_onz_rx);
    I48->z(pwr_on_rx);
    I48->gd(gd);
    I48->vp(vreg_mpll);
    I57.reset(new mipi_pll_dphy_sc_invx8_lvt("I57"));
    I57->a(pwron);
    I57->z(pwr_onz);
    I57->gd(gd);
    I57->vp(vreg_mpll);
    I61.reset(new mipi_pll_dphy_sc_invx8_lvt("I61"));
    I61->a(highspeed_n);
    I61->z(hs);
    I61->gd(gd);
    I61->vp(vreg_mpll);
    I47.reset(new mipi_pll_dphy_sc_na2x6_lvt("I47"));
    I47->a(pwron);
    I47->b(quad_on);
    I47->z(pwr_onz_rx);
    I47->gd(gd);
    I47->vp(vreg_mpll);
}

void mipi_pll_dphy_mpll_l2h::split_ck_l2h() {
    const vl::logic<8> v = vl::rd(ck_l2h);
    ck_l2h_b[0].write(v.bit(0).sl());
    ck_l2h_b[1].write(v.bit(1).sl());
    ck_l2h_b[2].write(v.bit(2).sl());
    ck_l2h_b[3].write(v.bit(3).sl());
    ck_l2h_b[4].write(v.bit(4).sl());
    ck_l2h_b[5].write(v.bit(5).sl());
    ck_l2h_b[6].write(v.bit(6).sl());
    ck_l2h_b[7].write(v.bit(7).sl());
}

void mipi_pll_dphy_mpll_l2h::merge_ph_pmix() {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(ph_pmix_b[0]));
    v.set_bit(1, vl::rd(ph_pmix_b[1]));
    v.set_bit(2, vl::rd(ph_pmix_b[2]));
    v.set_bit(3, vl::rd(ph_pmix_b[3]));
    vl::wr(ph_pmix, v);
}

vl::logic<8> mipi_pll_dphy_mpll_l2h::rd_ck_l2h() const {
    vl::logic<8> v(0);
    v.set_bit(0, vl::rd(ck_l2h_b[0]));
    v.set_bit(1, vl::rd(ck_l2h_b[1]));
    v.set_bit(2, vl::rd(ck_l2h_b[2]));
    v.set_bit(3, vl::rd(ck_l2h_b[3]));
    v.set_bit(4, vl::rd(ck_l2h_b[4]));
    v.set_bit(5, vl::rd(ck_l2h_b[5]));
    v.set_bit(6, vl::rd(ck_l2h_b[6]));
    v.set_bit(7, vl::rd(ck_l2h_b[7]));
    return v;
}

vl::logic<4> mipi_pll_dphy_mpll_l2h::rd_ph_pmix() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(ph_pmix_b[0]));
    v.set_bit(1, vl::rd(ph_pmix_b[1]));
    v.set_bit(2, vl::rd(ph_pmix_b[2]));
    v.set_bit(3, vl::rd(ph_pmix_b[3]));
    return v;
}
