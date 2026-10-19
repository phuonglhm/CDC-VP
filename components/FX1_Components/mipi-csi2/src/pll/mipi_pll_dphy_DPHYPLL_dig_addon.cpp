//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_dig_addon
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_DPHYPLL_dig_addon.h"

mipi_pll_dphy_DPHYPLL_dig_addon::mipi_pll_dphy_DPHYPLL_dig_addon(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(merge_clksel_int);
    sensitive << clksel_int_b[0] << clksel_int_b[1];
    SC_METHOD(merge_vco_cntrl_int);
    sensitive << vco_cntrl_int_b[0] << vco_cntrl_int_b[1] << vco_cntrl_int_b[2] << vco_cntrl_int_b[3];
    I1.reset(new mipi_pll_dphy_DPHYPLL_fd("I1"));
    I1->fbk_mux(clkfb);
    I1->hs_mux(vcoclk);
    I1->m0(m_intx0x);
    I1->tie_low_cdm(tie_low_cdm);
    I1->hs_div2(net700);
    I1->dgnd(vss);
    I1->dvdd(vdd);
    I1->gd(gd);
    I1->vpl(vreg_mpll);
    I1->vph(vph);
    SC_METHOD(glue_I118_clksel_int_avdd);
    sensitive << clksel_int_avdd[0] << clksel_int_avdd[1];
    I118.reset(new mipi_pll_dphy_DPHYPLL_dig_addon_clkselen("I118"));
    I118->clksel_en_avdd(clksel_en_avdd);
    I118->clksel_int_avdd(c_I118_clksel_int_avdd);
    I118->onpll_avdd(onpll_avdd);
    I118->clksel_int_output(clksel_int_output);
    I118->onpllz_avdd(onpllz_avdd);
    I118->gd(gd);
    I118->vpl(vreg_mpll);
    I75.reset(new mipi_pll_dphy_DPHYPLL_outstage_divandmux_outstage("I75"));
    I75->dgnd(vss);
    I75->dvdd(vdd);
    I75->input(clkout_gp_pre);
    I75->output(clkout_gp);
    I75->b2b(net704);
    TIE_CDM.reset(new mipi_pll_dphy_LEVELCONV_l2l_cdm_tie_LowHigh("TIE_CDM"));
    TIE_CDM->gd(gd);
    TIE_CDM->vp(vreg_mpll);
    TIE_CDM->tie_high(tie_high_cdm);
    TIE_CDM->tie_low(tie_low_cdm);
    SC_METHOD(glue_I50_p_int);
    sensitive << vco_cntrl_avdd[0] << vco_cntrl_avdd[1];
    I50.reset(new mipi_pll_dphy_DPHYPLL_outstage_divandmux("I50"));
    I50->clkdcc0(clkdcc0);
    I50->clkdcc180(clkdcc180);
    I50->clkdcc270(clkdcc270);
    I50->clkdcc90(clkdcc90);
    I50->clkext(clkext_prot);
    I50->clksel(clksel_int_output);
    I50->clkvco_lf(vcoclk_after_dcc);
    I50->onlvz(onpllz_avdd);
    I50->p_int(c_I50_p_int);
    I50->clkout0(clkout0);
    I50->clkout180(clkout180);
    I50->clkout270(clkout270);
    I50->clkout90(clkout90);
    I50->clkout_stop(clkout_stop);
    I50->agnd(gd);
    I50->avddlv(vreg_mpll);
    I112.reset(new mipi_pll_dphy_DPHYPLL_dig_addon_gppath("I112"));
    I112->clkvco_gp(clkvco_gp);
    I112->gp_clk_en(gp_clk_en_prot);
    I112->tie_low_cdm(tie_low_cdm);
    I112->clkout_gp(clkout_gp_pre);
    I112->gd(gd);
    I112->vpl(vreg_mpll);
    SC_METHOD(split_I0_vco_cntrl_int);
    sensitive << c_I0_vco_cntrl_int;
    dont_initialize();
    SC_METHOD(split_I0_clksel_int);
    sensitive << c_I0_clksel_int;
    dont_initialize();
    I0.reset(new mipi_pll_dphy_dphyplldig("I0"));
    I0->n(n);
    I0->m(m);
    I0->th1(th1);
    I0->th2(th2);
    I0->th3(th3);
    I0->tstplldig(tstplldig);
    I0->clksel(clksel);
    I0->th_delay(th_delay);
    I0->phy_prop_cntrl(phy_prop_cntrl);
    I0->phy_int_cntrl(phy_int_cntrl);
    I0->phy_vco_cntrl(phy_vco_cntrl);
    I0->phy_cpbias_cntrl(phy_cpbias_cntrl);
    I0->phy_gmp_cntrl(phy_gmp_cntrl);
    I0->phy_n(phy_n);
    I0->phy_m(phy_m);
    I0->int_cntrl_int(int_cntrl_int);
    I0->phy_in_clksel(phy_in_clksel);
    I0->prop_cntrl_int(prop_cntrl_int);
    I0->gmp_cntrl_int(gmp_cntrl_int);
    I0->vco_cntrl_int(c_I0_vco_cntrl_int);
    I0->clksel_int(c_I0_clksel_int);
    I0->cpbias_cntrl_int(cpbias_cntrl_int);
    I0->m_obs(m_obs);
    I0->n_obs(n_obs);
    I0->int_cntrl_obs(int_cntrl_obs);
    I0->prop_cntrl_obs(prop_cntrl_obs);
    I0->vco_cntrl_obs(vco_cntrl_obs);
    I0->cpbias_cntrl_obs(cpbias_cntrl_obs);
    I0->gmp_cntrl_obs(gmp_cntrl_obs);
    I0->int_cntrl(int_cntrl);
    I0->vco_cntrl(vco_cntrl);
    I0->prop_cntrl(prop_cntrl);
    I0->cpbias_cntrl(cpbias_cntrl);
    I0->gmp_cntrl(gmp_cntrl);
    I0->onpll(onpll);
    I0->vcoclk(net700);
    I0->clkin(clkin);
    I0->lock_det_on(lock_det_on);
    I0->lockdet_mode(lockdet_mode);
    I0->force_lock_phy(force_lock_phy);
    I0->lock_sel(lock_sel);
    I0->shadow_clear(shadow_clear);
    I0->updatepll(updatepll);
    I0->pll_shadow_control(pll_shadow_control);
    I0->force_lock(force_lock);
    I0->testlock(testlock);
    I0->lock(lock);
    I0->scanluctrl(scanluctrl);
    I0->scanrstz(scanrstz);
    I0->scanclk(scanclk);
    I0->scanen(scanen);
    I0->scanin(scanin);
    I0->scanmode(scanmode);
    I0->VSS(vss);
    I0->VDD(vdd);
    I0->scanout(scanout);
    I0->m_intx0x(m_intx0x);
    I0->lock_phy(lock_phy);
    I0->pll_shadow_control_obs(pll_shadow_control_obs);
    I0->clkfb(clkfb);
    I0->refclk(clkref);
    I117_5.reset(new mipi_pll_dphy_mpll_nandinv_1("I117_5"));
    I117_5->input(onpll);
    I117_5->tie_high_cdm(tie_high_cdm);
    I117_5->tie_low_cdm(tie_low_cdm);
    I117_5->output(onpll_avdd);
    I117_5->gd(gd);
    I117_5->vp(vreg_mpll);
    I117_4.reset(new mipi_pll_dphy_mpll_nandinv_1("I117_4"));
    I117_4->input(vco_cntrl_int_vdd5);
    I117_4->tie_high_cdm(tie_high_cdm);
    I117_4->tie_low_cdm(tie_low_cdm);
    I117_4->output(vco_cntrl_avdd[1]);
    I117_4->gd(gd);
    I117_4->vp(vreg_mpll);
    I117_3.reset(new mipi_pll_dphy_mpll_nandinv_1("I117_3"));
    I117_3->input(vco_cntrl_int_vdd4);
    I117_3->tie_high_cdm(tie_high_cdm);
    I117_3->tie_low_cdm(tie_low_cdm);
    I117_3->output(vco_cntrl_avdd[0]);
    I117_3->gd(gd);
    I117_3->vp(vreg_mpll);
    I117_2.reset(new mipi_pll_dphy_mpll_nandinv_1("I117_2"));
    I117_2->input(clksel_int_b[1]);
    I117_2->tie_high_cdm(tie_high_cdm);
    I117_2->tie_low_cdm(tie_low_cdm);
    I117_2->output(clksel_int_avdd[1]);
    I117_2->gd(gd);
    I117_2->vp(vreg_mpll);
    I117_1.reset(new mipi_pll_dphy_mpll_nandinv_1("I117_1"));
    I117_1->input(clksel_int_b[0]);
    I117_1->tie_high_cdm(tie_high_cdm);
    I117_1->tie_low_cdm(tie_low_cdm);
    I117_1->output(clksel_int_avdd[0]);
    I117_1->gd(gd);
    I117_1->vp(vreg_mpll);
    I117_0.reset(new mipi_pll_dphy_mpll_nandinv_1("I117_0"));
    I117_0->input(clksel_en);
    I117_0->tie_high_cdm(tie_high_cdm);
    I117_0->tie_low_cdm(tie_low_cdm);
    I117_0->output(clksel_en_avdd);
    I117_0->gd(gd);
    I117_0->vp(vreg_mpll);
    I127_3.reset(new mipi_pll_dphy_mpll_nandinv_1("I127_3"));
    I127_3->input(gp_clk_en);
    I127_3->tie_high_cdm(tie_high_cdm);
    I127_3->tie_low_cdm(tie_low_cdm);
    I127_3->output(gp_clk_en_prot);
    I127_3->gd(gd);
    I127_3->vp(vreg_mpll);
    I127_2.reset(new mipi_pll_dphy_mpll_nandinv_1("I127_2"));
    I127_2->input(clkext);
    I127_2->tie_high_cdm(tie_high_cdm);
    I127_2->tie_low_cdm(tie_low_cdm);
    I127_2->output(clkext_prot);
    I127_2->gd(gd);
    I127_2->vp(vreg_mpll);
    I127_1.reset(new mipi_pll_dphy_mpll_nandinv_1("I127_1"));
    I127_1->input(clkouten_left);
    I127_1->tie_high_cdm(tie_high_cdm);
    I127_1->tie_low_cdm(tie_low_cdm);
    I127_1->output(clkouten_left_prot);
    I127_1->gd(gd);
    I127_1->vp(vreg_mpll);
    I127_0.reset(new mipi_pll_dphy_mpll_nandinv_1("I127_0"));
    I127_0->input(clkouten_right);
    I127_0->tie_high_cdm(tie_high_cdm);
    I127_0->tie_low_cdm(tie_low_cdm);
    I127_0->output(clkouten_right_prot);
    I127_0->gd(gd);
    I127_0->vp(vreg_mpll);
    I147.reset(new mipi_pll_dphy_DPHYPLL_clock_buffers_4lane("I147"));
    I147->clk_in0(clkout0);
    I147->clk_in180(clkout180);
    I147->clk_in270(clkout270);
    I147->clk_in90(clkout90);
    I147->clkstop_in(clkout_stop);
    I147->enable(clkouten_left_prot);
    I147->tie_high(tie_high_cdm);
    I147->clk_out0(clkout0_left);
    I147->clk_out180(clkout180_left);
    I147->clk_out270(clkout270_left);
    I147->clk_out90(clkout90_left);
    I147->clkstop_out(clkout_stop_left);
    I147->agnd(gd);
    I147->avddlv(vreg_mpll);
    I148.reset(new mipi_pll_dphy_DPHYPLL_clock_buffers_4lane("I148"));
    I148->clk_in0(clkout0);
    I148->clk_in180(clkout180);
    I148->clk_in270(clkout270);
    I148->clk_in90(clkout90);
    I148->clkstop_in(clkout_stop);
    I148->enable(clkouten_right_prot);
    I148->tie_high(tie_high_cdm);
    I148->clk_out0(clkout0_right);
    I148->clk_out180(clkout180_right);
    I148->clk_out270(clkout270_right);
    I148->clk_out90(clkout90_right);
    I148->clkstop_out(clkout_stop_right);
    I148->agnd(gd);
    I148->avddlv(vreg_mpll);
}

void mipi_pll_dphy_DPHYPLL_dig_addon::merge_clksel_int() {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(clksel_int_b[0]));
    v.set_bit(1, vl::rd(clksel_int_b[1]));
    vl::wr(clksel_int, v);
}

void mipi_pll_dphy_DPHYPLL_dig_addon::merge_vco_cntrl_int() {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(vco_cntrl_int_b[0]));
    v.set_bit(1, vl::rd(vco_cntrl_int_b[1]));
    v.set_bit(2, vl::rd(vco_cntrl_int_b[2]));
    v.set_bit(3, vl::rd(vco_cntrl_int_b[3]));
    vl::wr(vco_cntrl_int, v);
}

vl::logic<2> mipi_pll_dphy_DPHYPLL_dig_addon::rd_clksel_int() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(clksel_int_b[0]));
    v.set_bit(1, vl::rd(clksel_int_b[1]));
    return v;
}

vl::logic<2> mipi_pll_dphy_DPHYPLL_dig_addon::rd_clksel_int_avdd() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(clksel_int_avdd[0]));
    v.set_bit(1, vl::rd(clksel_int_avdd[1]));
    return v;
}

vl::logic<2> mipi_pll_dphy_DPHYPLL_dig_addon::rd_vco_cntrl_avdd() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(vco_cntrl_avdd[0]));
    v.set_bit(1, vl::rd(vco_cntrl_avdd[1]));
    return v;
}

vl::logic<4> mipi_pll_dphy_DPHYPLL_dig_addon::rd_vco_cntrl_int() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(vco_cntrl_int_b[0]));
    v.set_bit(1, vl::rd(vco_cntrl_int_b[1]));
    v.set_bit(2, vl::rd(vco_cntrl_int_b[2]));
    v.set_bit(3, vl::rd(vco_cntrl_int_b[3]));
    return v;
}

void mipi_pll_dphy_DPHYPLL_dig_addon::glue_I118_clksel_int_avdd() { vl::wr(c_I118_clksel_int_avdd, rd_clksel_int_avdd()); }

void mipi_pll_dphy_DPHYPLL_dig_addon::glue_I50_p_int() { vl::wr(c_I50_p_int, rd_vco_cntrl_avdd()); }

void mipi_pll_dphy_DPHYPLL_dig_addon::split_I0_vco_cntrl_int() {
    const vl::logic<6> v = vl::rd(c_I0_vco_cntrl_int);
    vco_cntrl_int_b[0].write(v.bit(0).sl());
    vco_cntrl_int_b[1].write(v.bit(1).sl());
    vco_cntrl_int_b[2].write(v.bit(2).sl());
    vco_cntrl_int_b[3].write(v.bit(3).sl());
    vl::wr(vco_cntrl_int_vdd4, v.bit(4));
    vl::wr(vco_cntrl_int_vdd5, v.bit(5));
}

void mipi_pll_dphy_DPHYPLL_dig_addon::split_I0_clksel_int() {
    const vl::logic<2> v = vl::rd(c_I0_clksel_int);
    clksel_int_b[0].write(v.bit(0).sl());
    clksel_int_b[1].write(v.bit(1).sl());
}
