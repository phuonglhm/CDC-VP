//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_mpll_prediv_prog
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_mpll_prediv_prog.h"

mipi_pll_dphy_mpll_prediv_prog::mipi_pll_dphy_mpll_prediv_prog(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_ref_cntrl);
    sensitive << ref_cntrl;
    na6.reset(new mipi_pll_dphy_sc_no2x6_lvt("na6"));
    na6->a(refcntrl_n[1]);
    na6->b(refcntrl_n[0]);
    na6->z(en_4);
    na6->gd(gd);
    na6->vp(vp);
    na7.reset(new mipi_pll_dphy_sc_no2x6_lvt("na7"));
    na7->a(refcntrl_n[1]);
    na7->b(refcntrl[0]);
    na7->z(en_3);
    na7->gd(gd);
    na7->vp(vp);
    na8.reset(new mipi_pll_dphy_sc_no2x6_lvt("na8"));
    na8->a(refcntrl[1]);
    na8->b(refcntrl[0]);
    na8->z(en_1);
    na8->gd(gd);
    na8->vp(vp);
    na9.reset(new mipi_pll_dphy_sc_no2x6_lvt("na9"));
    na9->a(refcntrl[1]);
    na9->b(refcntrl_n[0]);
    na9->z(en_2);
    na9->gd(gd);
    na9->vp(vp);
    SC_METHOD(glue_I45_in);
    sensitive << ck_d4_p << clk_div3 << ck_d2_p << ck_p;
    SC_METHOD(glue_I45_s);
    sensitive << en_4 << en_3 << en_2 << en_1;
    I45.reset(new mipi_pll_dphy_mpll_mux2("I45"));
    I45->in(c_I45_in);
    I45->pwr_off(rst);
    I45->s(c_I45_s);
    I45->out(ck_ref_out);
    I45->gd(gd);
    I45->vp(vp);
    I2.reset(new mipi_pll_dphy_pll_tg_dffr("I2"));
    I2->clkn(ck_m);
    I2->clkp(ck_p);
    I2->rst(rst_3);
    I2->rst_n(rstz_3);
    I2->q(b);
    I2->qb(net482);
    I2->d(a);
    I2->db(d2b);
    I2->vddlv(vp);
    I2->vssa(gd);
    I83.reset(new mipi_pll_dphy_pll_tg_dffr("I83"));
    I83->clkn(ck_d2_m);
    I83->clkp(ck_d2_p);
    I83->rst(rst_24);
    I83->rst_n(rstz_24);
    I83->q(ck_d4_p);
    I83->qb(ck_d4_m);
    I83->d(ck_d4_m);
    I83->db(ck_d4_p);
    I83->vddlv(vp);
    I83->vssa(gd);
    ff0.reset(new mipi_pll_dphy_pll_tg_dffr("ff0"));
    ff0->clkn(ck_m);
    ff0->clkp(ck_p);
    ff0->rst(rst_3);
    ff0->rst_n(rstz_3);
    ff0->q(a);
    ff0->qb(d2b);
    ff0->d(d);
    ff0->db(d1b);
    ff0->vddlv(vp);
    ff0->vssa(gd);
    pll_tg_dffr1.reset(new mipi_pll_dphy_pll_tg_dffr("pll_tg_dffr1"));
    pll_tg_dffr1->clkn(ck_m);
    pll_tg_dffr1->clkp(ck_p);
    pll_tg_dffr1->rst(rst_24);
    pll_tg_dffr1->rst_n(rstz_24);
    pll_tg_dffr1->q(ck_d2_p);
    pll_tg_dffr1->qb(ck_d2_m);
    pll_tg_dffr1->d(ck_d2_m);
    pll_tg_dffr1->db(ck_d2_p);
    pll_tg_dffr1->vddlv(vp);
    pll_tg_dffr1->vssa(gd);
    I10.reset(new mipi_pll_dphy_sc_or2x6_lvt("I10"));
    I10->a(ck_p);
    I10->b(b1);
    I10->z(net526);
    I10->gd(gd);
    I10->vp(vp);
    I85.reset(new mipi_pll_dphy_sc_or2x6_lvt("I85"));
    I85->a(clk_div3);
    I85->b(a);
    I85->z(net525);
    I85->gd(gd);
    I85->vp(vp);
    I58.reset(new mipi_pll_dphy_sc_tie_hilow_lvt("I58"));
    I58->gd(gd);
    I58->tie_high(vp_esd);
    I58->tie_low(gd_esd);
    I58->vp(vp);
    I78.reset(new mipi_pll_dphy_sc_na2x12_lvt("I78"));
    I78->a(refcntrl[0]);
    I78->b(rstz);
    I78->z(rst_24);
    I78->gd(gd);
    I78->vp(vp);
    I79.reset(new mipi_pll_dphy_sc_na2x12_lvt("I79"));
    I79->a(en_3);
    I79->b(rstz);
    I79->z(rst_3);
    I79->gd(gd);
    I79->vp(vp);
    I5.reset(new mipi_pll_dphy_sc_invx16_lvt("I5"));
    I5->a(a);
    I5->z(net510);
    I5->gd(gd);
    I5->vp(vp);
    I6.reset(new mipi_pll_dphy_sc_invx16_lvt("I6"));
    I6->a(b);
    I6->z(net511);
    I6->gd(gd);
    I6->vp(vp);
    I12.reset(new mipi_pll_dphy_sc_invx16_lvt("I12"));
    I12->a(b);
    I12->z(b1);
    I12->gd(gd);
    I12->vp(vp);
    I70.reset(new mipi_pll_dphy_sc_invx16_lvt("I70"));
    I70->a(ck_ref_in);
    I70->z(ck_m_pre);
    I70->gd(gd);
    I70->vp(vp);
    I71.reset(new mipi_pll_dphy_sc_invx16_lvt("I71"));
    I71->a(d);
    I71->z(d1b);
    I71->gd(gd);
    I71->vp(vp);
    I77.reset(new mipi_pll_dphy_sc_invx16_lvt("I77"));
    I77->a(ck_m_pre);
    I77->z(ck_p);
    I77->gd(gd);
    I77->vp(vp);
    I80.reset(new mipi_pll_dphy_sc_invx16_lvt("I80"));
    I80->a(rst_3);
    I80->z(rstz_3);
    I80->gd(gd);
    I80->vp(vp);
    I81.reset(new mipi_pll_dphy_sc_invx16_lvt("I81"));
    I81->a(rst_24);
    I81->z(rstz_24);
    I81->gd(gd);
    I81->vp(vp);
    I4.reset(new mipi_pll_dphy_sc_and2x6_lvt("I4"));
    I4->a(net510);
    I4->b(net511);
    I4->z(d);
    I4->gd(gd);
    I4->vp(vp);
    I9.reset(new mipi_pll_dphy_sc_and2x6_lvt("I9"));
    I9->a(net525);
    I9->b(net526);
    I9->z(clk_div3);
    I9->gd(gd);
    I9->vp(vp);
    I82.reset(new mipi_pll_dphy_sc_invx8_lvt("I82"));
    I82->a(rstz);
    I82->z(rst);
    I82->gd(gd);
    I82->vp(vp);
    inv6_1.reset(new mipi_pll_dphy_sc_invx8_lvt("inv6_1"));
    inv6_1->a(ref_cntrl_b[1]);
    inv6_1->z(refcntrl_n[1]);
    inv6_1->gd(gd);
    inv6_1->vp(vp);
    inv6_0.reset(new mipi_pll_dphy_sc_invx8_lvt("inv6_0"));
    inv6_0->a(ref_cntrl_b[0]);
    inv6_0->z(refcntrl_n[0]);
    inv6_0->gd(gd);
    inv6_0->vp(vp);
    inv7_1.reset(new mipi_pll_dphy_sc_invx8_lvt("inv7_1"));
    inv7_1->a(refcntrl_n[1]);
    inv7_1->z(refcntrl[1]);
    inv7_1->gd(gd);
    inv7_1->vp(vp);
    inv7_0.reset(new mipi_pll_dphy_sc_invx8_lvt("inv7_0"));
    inv7_0->a(refcntrl_n[0]);
    inv7_0->z(refcntrl[0]);
    inv7_0->gd(gd);
    inv7_0->vp(vp);
    I84.reset(new mipi_pll_dphy_sc_tgate_x16_lvt("I84"));
    I84->a(ck_m_pre);
    I84->c_m(gd_esd);
    I84->c_p(vp_esd);
    I84->z(ck_m);
    I84->gd(gd);
    I84->vp(vp);
}

void mipi_pll_dphy_mpll_prediv_prog::split_ref_cntrl() {
    const vl::logic<2> v = vl::rd(ref_cntrl);
    ref_cntrl_b[0].write(v.bit(0).sl());
    ref_cntrl_b[1].write(v.bit(1).sl());
}

vl::logic<2> mipi_pll_dphy_mpll_prediv_prog::rd_ref_cntrl() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(ref_cntrl_b[0]));
    v.set_bit(1, vl::rd(ref_cntrl_b[1]));
    return v;
}

vl::logic<2> mipi_pll_dphy_mpll_prediv_prog::rd_refcntrl() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(refcntrl[0]));
    v.set_bit(1, vl::rd(refcntrl[1]));
    return v;
}

vl::logic<2> mipi_pll_dphy_mpll_prediv_prog::rd_refcntrl_n() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(refcntrl_n[0]));
    v.set_bit(1, vl::rd(refcntrl_n[1]));
    return v;
}

void mipi_pll_dphy_mpll_prediv_prog::glue_I45_in() { vl::wr(c_I45_in, vl::cat(vl::rd(ck_d4_p), vl::rd(clk_div3), vl::rd(ck_d2_p), vl::rd(ck_p))); }

void mipi_pll_dphy_mpll_prediv_prog::glue_I45_s() { vl::wr(c_I45_s, vl::cat(vl::rd(en_4), vl::rd(en_3), vl::rd(en_2), vl::rd(en_1))); }
