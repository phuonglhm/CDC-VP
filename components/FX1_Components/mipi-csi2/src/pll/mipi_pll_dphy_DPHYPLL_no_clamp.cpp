//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_no_clamp
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_DPHYPLL_no_clamp.h"

mipi_pll_dphy_DPHYPLL_no_clamp::mipi_pll_dphy_DPHYPLL_no_clamp(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_m_obs_core_model);
    sensitive << m_obs;
    dig_dphy.reset(new mipi_pll_dphy_DPHYPLL_dig_addon("dig_dphy"));
    dig_dphy->clkdcc0(clk_i_p);
    dig_dphy->clkdcc180(clk_i_m);
    dig_dphy->clkdcc270(clk_q_m);
    dig_dphy->clkdcc90(clk_q_p);
    dig_dphy->clkext(clkext);
    dig_dphy->clkin(clkin);
    dig_dphy->clkouten_left(clkouten_left);
    dig_dphy->clkouten_right(clkouten_right);
    dig_dphy->clksel(clksel);
    dig_dphy->clksel_en(clksel_en);
    dig_dphy->clkvco_gp(ph_pmix[1]);
    dig_dphy->cpbias_cntrl(cpbias_cntrl);
    dig_dphy->force_lock(force_lock);
    dig_dphy->force_lock_phy(force_lock_phy);
    dig_dphy->gmp_cntrl(gmp_cntrl);
    dig_dphy->gp_clk_en(gp_clk_en);
    dig_dphy->int_cntrl(int_cntrl);
    dig_dphy->lock_det_on(lock_det_on);
    dig_dphy->lock_sel(lock_sel);
    dig_dphy->lockdet_mode(lockdet_mode);
    dig_dphy->m(m);
    dig_dphy->n(n);
    dig_dphy->onpll(mpll_pwron);
    dig_dphy->phy_cpbias_cntrl(phy_cpbias_cntrl);
    dig_dphy->phy_gmp_cntrl(phy_gmp_cntrl);
    dig_dphy->phy_int_cntrl(phy_int_cntrl);
    dig_dphy->phy_m(phy_m);
    dig_dphy->phy_n(phy_n);
    dig_dphy->phy_prop_cntrl(phy_prop_cntrl);
    dig_dphy->phy_vco_cntrl(phy_vco_cntrl);
    dig_dphy->pll_shadow_control(pll_shadow_control);
    dig_dphy->prop_cntrl(prop_cntrl);
    dig_dphy->scanclk(scanclk);
    dig_dphy->scanen(scanen);
    dig_dphy->scanin(scanin);
    dig_dphy->scanluctrl(scanluctrl);
    dig_dphy->scanmode(scanmode);
    dig_dphy->scanrstz(scanrstz);
    dig_dphy->shadow_clear(shadow_clear);
    dig_dphy->th1(th1);
    dig_dphy->th2(th2);
    dig_dphy->th3(th3);
    dig_dphy->th_delay(th_delay);
    dig_dphy->tstplldig(tstplldig);
    dig_dphy->updatepll(updatepll);
    dig_dphy->vco_cntrl(vco_cntrl);
    dig_dphy->vcoclk(ph_pmix[0]);
    dig_dphy->vcoclk_after_dcc(ph_pmix[0]);
    dig_dphy->clkfb(oscclk);
    dig_dphy->clkout0_left(clkout0_left);
    dig_dphy->clkout0_right(clkout0_right);
    dig_dphy->clkout180_left(clkout180_left);
    dig_dphy->clkout180_right(clkout180_right);
    dig_dphy->clkout270_left(clkout270_left);
    dig_dphy->clkout270_right(clkout270_right);
    dig_dphy->clkout90_left(clkout90_left);
    dig_dphy->clkout90_right(clkout90_right);
    dig_dphy->clkout_gp(clkout_gp);
    dig_dphy->clkout_stop_left(clkout_stop_left);
    dig_dphy->clkout_stop_right(clkout_stop_right);
    dig_dphy->clkref(clkref_in);
    dig_dphy->clksel_int(clksel_int);
    dig_dphy->cpbias_cntrl_int(cpbias_cntrl_int);
    dig_dphy->cpbias_cntrl_obs(cpbias_cntrl_obs);
    dig_dphy->gmp_cntrl_int(gmp_cntrl_int);
    dig_dphy->gmp_cntrl_obs(gmp_cntrl_obs);
    dig_dphy->int_cntrl_int(int_cntrl_int);
    dig_dphy->int_cntrl_obs(int_cntrl_obs);
    dig_dphy->lock(lock);
    dig_dphy->lock_phy(lock_phy);
    dig_dphy->m_obs(m_obs);
    dig_dphy->n_obs(n_obs);
    dig_dphy->phy_in_clksel(phy_in_clksel);
    dig_dphy->pll_shadow_control_obs(pll_shadow_control_obs);
    dig_dphy->prop_cntrl_int(prop_cntrl_int);
    dig_dphy->prop_cntrl_obs(prop_cntrl_obs);
    dig_dphy->scanout(scanout);
    dig_dphy->testlock(testlock);
    dig_dphy->vco_cntrl_int(vco_cntrl_int);
    dig_dphy->vco_cntrl_obs(vco_cntrl_obs);
    dig_dphy->gd(gd);
    dig_dphy->vdd(vdd);
    dig_dphy->vph(vph);
    dig_dphy->vreg_mpll(vreg_mpll);
    dig_dphy->vss(vss);
    SC_METHOD(glue_ana_int_cntrl);
    sensitive << int_cntrl_int;
    SC_METHOD(glue_ana_mpll_ph_sel_wd);
    sensitive << tie_low;
    SC_METHOD(glue_ana_prop_cntrl);
    sensitive << prop_cntrl_int;
    SC_METHOD(glue_ana_ref_cntrl);
    sensitive << tie_low;
    SC_METHOD(split_ana_ph_pmix);
    sensitive << c_ana_ph_pmix;
    dont_initialize();
    ana.reset(new mipi_pll_dphy_mpll_ana("ana"));
    ana->atb_sense_sel(atb_sense_sel);
    ana->clkref(clkref_in);
    ana->clksel(clksel_int);
    ana->clksqron(tie_low);
    ana->cp_bias_cntrl(cpbias_cntrl_int);
    ana->gd(gd);
    ana->gmp_cntrl(gmp_cntrl_int);
    ana->int_cntrl(c_ana_int_cntrl);
    ana->meas_iv(meas_iv);
    ana->mpll_gear_shift(mpll_gear_shift);
    ana->mpll_ph_sel_clk(tie_low);
    ana->mpll_ph_sel_wd(c_ana_mpll_ph_sel_wd);
    ana->mpll_pmix_clk_sel(tie_low);
    ana->mpll_prg(mpll_prg);
    ana->mpll_pwron(mpll_pwron);
    ana->mpll_pwron_pmix(tie_low);
    ana->mpll_relock_gear(tie_low);
    ana->mpll_relock_vint(tie_low);
    ana->mpll_reset(mpll_reset);
    ana->mpll_rst_pmix(tie_low);
    ana->opmode(mpll_opmode);
    ana->oscclk(oscclk);
    ana->prop_cntrl(c_ana_prop_cntrl);
    ana->ref_cntrl(c_ana_ref_cntrl);
    ana->vbg(vbg);
    ana->vco_cntrl(vco_cntrl_int);
    ana->vp(vdd);
    ana->vphreg(vph);
    ana->vpl(vpl);
    ana->vss(vss);
    ana->byp_ncntrldiv(byp_ncntrldiv);
    ana->ck_l2h(ck_l2h);
    ana->clk_i_m(clk_i_m);
    ana->clk_i_p(clk_i_p);
    ana->clk_q_m(clk_q_m);
    ana->clk_q_p(clk_q_p);
    ana->clkrefdiv(clkrefdiv);
    ana->clksqr_on(clksqr_on);
    ana->clksqr_on_h(clksqr_on_h);
    ana->dn_lockdet(dn_lockdet);
    ana->meas_iv_h_out(meas_iv_h);
    ana->mpll_ph_sel_clk_out(mpll_ph_sel_clk_out);
    ana->mpll_ph_sel_wd_out(mpll_ph_sel_wd_out);
    ana->ph_pmix(c_ana_ph_pmix);
    ana->pmix_clk_sel(pmix_clk_sel);
    ana->pwron(pwron);
    ana->pwron_n(pwron_n);
    ana->pwron_pmix_h(pwron_pmix_h);
    ana->pwron_pmix_n(pwron_pmix_n);
    ana->reset(reset);
    ana->rst_pmix_h(rst_pmix_h);
    ana->up_lockdet(up_lockdet);
    ana->vp_l2h(vp_l2h);
    ana->vpl_det(vpl_det);
    ana->vref_pmix(vref_pmix);
    ana->vreg_mpll(vreg_mpll);
    ana->atb_f_m(atb_f_m);
    ana->atb_s_m(atb_sense);
    ana->atb_s_m_int(net1902);
    ana->atb_s_p(atb_sense);
    ana->atb_s_p_int(net1903);
    ana->ivco(ivco);
    ana->ivco_osc(ivco_osc);
    ana->m_obs_core_model(m_obs_core_model);
    I211.reset(new mipi_pll_dphy_sc_tie_hilow_lvt("I211"));
    I211->gd(vss);
    I211->tie_high(net1857);
    I211->tie_low(tie_low);
    I211->vp(vdd);
}

vl::logic<4> mipi_pll_dphy_DPHYPLL_no_clamp::rd_ph_pmix() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(ph_pmix[0]));
    v.set_bit(1, vl::rd(ph_pmix[1]));
    v.set_bit(2, vl::rd(ph_pmix[2]));
    v.set_bit(3, vl::rd(ph_pmix[3]));
    return v;
}

void mipi_pll_dphy_DPHYPLL_no_clamp::assign_m_obs_core_model() {
    vl::wr(m_obs_core_model, vl::rd(m_obs));
}

void mipi_pll_dphy_DPHYPLL_no_clamp::glue_ana_int_cntrl() { vl::wr(c_ana_int_cntrl, vl::rd(int_cntrl_int).slice<4>(0)); }

void mipi_pll_dphy_DPHYPLL_no_clamp::glue_ana_mpll_ph_sel_wd() { vl::wr(c_ana_mpll_ph_sel_wd, vl::cat(vl::rd(tie_low), vl::rd(tie_low), vl::rd(tie_low), vl::rd(tie_low), vl::rd(tie_low), vl::rd(tie_low), vl::rd(tie_low), vl::rd(tie_low), vl::rd(tie_low))); }

void mipi_pll_dphy_DPHYPLL_no_clamp::glue_ana_prop_cntrl() { vl::wr(c_ana_prop_cntrl, vl::rd(prop_cntrl_int).slice<5>(0)); }

void mipi_pll_dphy_DPHYPLL_no_clamp::glue_ana_ref_cntrl() { vl::wr(c_ana_ref_cntrl, vl::cat(vl::rd(tie_low), vl::rd(tie_low))); }

void mipi_pll_dphy_DPHYPLL_no_clamp::split_ana_ph_pmix() {
    const vl::logic<4> v = vl::rd(c_ana_ph_pmix);
    ph_pmix[0].write(v.bit(0).sl());
    ph_pmix[1].write(v.bit(1).sl());
    ph_pmix[2].write(v.bit(2).sl());
    ph_pmix[3].write(v.bit(3).sl());
}
