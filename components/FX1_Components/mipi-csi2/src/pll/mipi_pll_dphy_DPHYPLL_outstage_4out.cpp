//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_outstage_4out
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_DPHYPLL_outstage_4out.h"

mipi_pll_dphy_DPHYPLL_outstage_4out::mipi_pll_dphy_DPHYPLL_outstage_4out(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_clksel);
    sensitive << clksel;
    I71.reset(new mipi_pll_dphy_sc_invx16_lvt_highz("I71"));
    I71->a(net389);
    I71->on(en_b2b);
    I71->onz(en_b2b_z);
    I71->z(net388);
    I71->gd(dgnd);
    I71->vp(dvdd);
    I84.reset(new mipi_pll_dphy_sc_invx16_lvt_highz("I84"));
    I84->a(net388);
    I84->on(en_b2b);
    I84->onz(en_b2b_z);
    I84->z(net389);
    I84->gd(dgnd);
    I84->vp(dvdd);
    I110.reset(new mipi_pll_dphy_sc_invx16_lvt_highz("I110"));
    I110->a(net391);
    I110->on(en_b2b);
    I110->onz(en_b2b_z);
    I110->z(net390);
    I110->gd(dgnd);
    I110->vp(dvdd);
    I111.reset(new mipi_pll_dphy_sc_invx16_lvt_highz("I111"));
    I111->a(net390);
    I111->on(en_b2b);
    I111->onz(en_b2b_z);
    I111->z(net391);
    I111->gd(dgnd);
    I111->vp(dvdd);
    SC_METHOD(glue_mux_out_ckin1);
    sensitive << clkdcc3 << clkdcc2 << clkdcc1 << clkdcc0;
    SC_METHOD(glue_mux_out_ckin2);
    sensitive << clkext3 << clkext2 << clkext1 << clkext0;
    SC_METHOD(split_mux_out_clk_out);
    sensitive << c_mux_out_clk_out;
    dont_initialize();
    mux_out.reset(new mipi_pll_dphy_DPHYPLL_outstage_outmux_glitchless("mux_out"));
    mux_out->ckin1(c_mux_out_ckin1);
    mux_out->ckin2(c_mux_out_ckin2);
    mux_out->clk1_on(cksel0rst);
    mux_out->clk2_on(cksel1rst);
    mux_out->res1(onlvz);
    mux_out->clk_out(c_mux_out_clk_out);
    mux_out->clk_stop(clk_stop);
    mux_out->mux_off(mux_off);
    mux_out->dgnd(dgnd);
    mux_out->dvdd(dvdd);
    I113.reset(new mipi_pll_dphy_sc_invx24_lvt("I113"));
    I113->a(en_b2b);
    I113->z(en_b2b_z);
    I113->gd(dgnd);
    I113->vp(dvdd);
    inv_1.reset(new mipi_pll_dphy_sc_invx24_lvt("inv_1"));
    inv_1->a(clksel_b[1]);
    inv_1->z(net262);
    inv_1->gd(dgnd);
    inv_1->vp(dvdd);
    inv_2.reset(new mipi_pll_dphy_sc_invx24_lvt("inv_2"));
    inv_2->a(clksel_b[0]);
    inv_2->z(net259);
    inv_2->gd(dgnd);
    inv_2->vp(dvdd);
    I102.reset(new mipi_pll_dphy_DPHYPLL_outstage_divandmux_outstage("I102"));
    I102->dgnd(dgnd);
    I102->dvdd(dvdd);
    I102->input(clkout_mux0);
    I102->output(clkout0);
    I102->b2b(nc_I102_b2b);
    I103.reset(new mipi_pll_dphy_DPHYPLL_outstage_divandmux_outstage("I103"));
    I103->dgnd(dgnd);
    I103->dvdd(dvdd);
    I103->input(clkout_mux1);
    I103->output(clkout1);
    I103->b2b(nc_I103_b2b);
    I104.reset(new mipi_pll_dphy_DPHYPLL_outstage_divandmux_outstage("I104"));
    I104->dgnd(dgnd);
    I104->dvdd(dvdd);
    I104->input(clkout_mux2);
    I104->output(clkout2);
    I104->b2b(nc_I104_b2b);
    I105.reset(new mipi_pll_dphy_DPHYPLL_outstage_divandmux_outstage("I105"));
    I105->dgnd(dgnd);
    I105->dvdd(dvdd);
    I105->input(clkout_mux3);
    I105->output(clkout3);
    I105->b2b(nc_I105_b2b);
    I112.reset(new mipi_pll_dphy_sc_no2x12_lvt("I112"));
    I112->a(clk_stop);
    I112->b(onlvz);
    I112->z(en_b2b);
    I112->gd(dgnd);
    I112->vp(dvdd);
    nor_1.reset(new mipi_pll_dphy_sc_no2x12_lvt("nor_1"));
    nor_1->a(net262);
    nor_1->b(onlvz);
    nor_1->z(cksel1rst);
    nor_1->gd(dgnd);
    nor_1->vp(dvdd);
    nor_2.reset(new mipi_pll_dphy_sc_no2x12_lvt("nor_2"));
    nor_2->a(net259);
    nor_2->b(onlvz);
    nor_2->z(cksel0rst);
    nor_2->gd(dgnd);
    nor_2->vp(dvdd);
}

void mipi_pll_dphy_DPHYPLL_outstage_4out::split_clksel() {
    const vl::logic<2> v = vl::rd(clksel);
    clksel_b[0].write(v.bit(0).sl());
    clksel_b[1].write(v.bit(1).sl());
}

vl::logic<2> mipi_pll_dphy_DPHYPLL_outstage_4out::rd_clksel() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(clksel_b[0]));
    v.set_bit(1, vl::rd(clksel_b[1]));
    return v;
}

void mipi_pll_dphy_DPHYPLL_outstage_4out::glue_mux_out_ckin1() { vl::wr(c_mux_out_ckin1, vl::cat(vl::rd(clkdcc3), vl::rd(clkdcc2), vl::rd(clkdcc1), vl::rd(clkdcc0))); }

void mipi_pll_dphy_DPHYPLL_outstage_4out::glue_mux_out_ckin2() { vl::wr(c_mux_out_ckin2, vl::cat(vl::rd(clkext3), vl::rd(clkext2), vl::rd(clkext1), vl::rd(clkext0))); }

void mipi_pll_dphy_DPHYPLL_outstage_4out::split_mux_out_clk_out() {
    const vl::logic<4> v = vl::rd(c_mux_out_clk_out);
    vl::wr(clkout_mux0, v.bit(0));
    vl::wr(clkout_mux1, v.bit(1));
    vl::wr(clkout_mux2, v.bit(2));
    vl::wr(clkout_mux3, v.bit(3));
}
