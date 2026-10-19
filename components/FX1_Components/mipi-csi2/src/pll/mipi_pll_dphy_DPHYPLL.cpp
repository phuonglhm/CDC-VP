//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_DPHYPLL.h"

mipi_pll_dphy_DPHYPLL::mipi_pll_dphy_DPHYPLL(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_clkout0_left);
    sensitive << clkout0_left_int;
    SC_METHOD(assign_clkout0_right);
    sensitive << clkout0_right_int;
    SC_METHOD(assign_clkout180_left);
    sensitive << clkout180_left_int;
    SC_METHOD(assign_clkout180_right);
    sensitive << clkout180_right_int;
    SC_METHOD(assign_clkout270_left);
    sensitive << clkout270_left_int;
    SC_METHOD(assign_clkout270_right);
    sensitive << clkout270_right_int;
    SC_METHOD(assign_clkout90_left);
    sensitive << clkout90_left_int;
    SC_METHOD(assign_clkout90_right);
    sensitive << clkout90_right_int;
    SC_METHOD(assign_clkout_gp);
    sensitive << clkout_gp_int;
    SC_METHOD(assign_clkout_stop_left);
    sensitive << clkout_stop_left_int;
    SC_METHOD(assign_clkout_stop_right);
    sensitive << clkout_stop_right_int;
    SC_METHOD(assign_vpl_det);
    sensitive << vpl_det_int;
    SC_METHOD(assign_vreg_mpll);
    sensitive << vreg_mpll_int;
    SC_METHOD(assign_tiehigh);
    I242.reset(new mipi_pll_dphy_DPHYPLL_no_clamp("I242"));
    I242->atb_sense_sel(atb_sense_sel);
    I242->clkext(clkext);
    I242->clkin(clkin);
    I242->clkouten_left(clkouten_left);
    I242->clkouten_right(clkouten_right);
    I242->clksel(clksel);
    I242->clksel_en(clksel_en);
    I242->cpbias_cntrl(cpbias_cntrl);
    I242->force_lock(force_lock);
    I242->force_lock_phy(force_lock_phy);
    I242->gmp_cntrl(gmp_cntrl);
    I242->gp_clk_en(gp_clk_en);
    I242->int_cntrl(int_cntrl);
    I242->lock_det_on(lock_det_on);
    I242->lock_sel(lock_sel);
    I242->lockdet_mode(lockdet_mode);
    I242->m(m);
    I242->meas_iv(meas_iv);
    I242->mpll_gear_shift(mpll_gear_shift);
    I242->mpll_opmode(mpll_opmode);
    I242->mpll_prg(mpll_prg);
    I242->mpll_pwron(mpll_pwron);
    I242->mpll_reset(mpll_reset);
    I242->n(n);
    I242->phy_cpbias_cntrl(phy_cpbias_cntrl);
    I242->phy_gmp_cntrl(phy_gmp_cntrl);
    I242->phy_int_cntrl(phy_int_cntrl);
    I242->phy_m(phy_m);
    I242->phy_n(phy_n);
    I242->phy_prop_cntrl(phy_prop_cntrl);
    I242->phy_vco_cntrl(phy_vco_cntrl);
    I242->pll_shadow_control(pll_shadow_control);
    I242->prop_cntrl(prop_cntrl);
    I242->scanclk(scanclk);
    I242->scanen(scanen);
    I242->scanin(scanin);
    I242->scanluctrl(scanluctrl);
    I242->scanmode(scanmode);
    I242->scanrstz(scanrstz);
    I242->shadow_clear(shadow_clear);
    I242->th1(th1);
    I242->th2(th2);
    I242->th3(th3);
    I242->th_delay(th_delay);
    I242->tstplldig(tstplldig);
    I242->updatepll(updatepll);
    I242->vco_cntrl(vco_cntrl);
    I242->clkout0_left(clkout0_left_int);
    I242->clkout0_right(clkout0_right_int);
    I242->clkout180_left(clkout180_left_int);
    I242->clkout180_right(clkout180_right_int);
    I242->clkout270_left(clkout270_left_int);
    I242->clkout270_right(clkout270_right_int);
    I242->clkout90_left(clkout90_left_int);
    I242->clkout90_right(clkout90_right_int);
    I242->clkout_gp(clkout_gp_int);
    I242->clkout_stop_left(clkout_stop_left_int);
    I242->clkout_stop_right(clkout_stop_right_int);
    I242->cpbias_cntrl_obs(cpbias_cntrl_obs);
    I242->gmp_cntrl_obs(gmp_cntrl_obs);
    I242->int_cntrl_obs(int_cntrl_obs);
    I242->lock(lock);
    I242->lock_phy(lock_phy);
    I242->m_obs(m_obs);
    I242->n_obs(n_obs);
    I242->phy_in_clksel(phy_in_clksel);
    I242->pll_shadow_control_obs(pll_shadow_control_obs);
    I242->prop_cntrl_obs(prop_cntrl_obs);
    I242->scanout(scanout);
    I242->testlock(testlock);
    I242->vco_cntrl_obs(vco_cntrl_obs);
    I242->vpl_det(vpl_det_int);
    I242->vreg_mpll(vreg_mpll_int);
    I242->atb_sense(atb_sense);
    I242->gd(gd);
    I242->ivco(net2001);
    I242->ivco_osc(net2001);
    I242->vbg(vbg);
    I242->vdd(vdd);
    I242->vph(vph);
    I242->vpl(vpl);
    I242->vss(vss);
}

void mipi_pll_dphy_DPHYPLL::assign_clkout0_left() {
    vl::wr(clkout0_left, vl::rd(clkout0_left_int));
}

void mipi_pll_dphy_DPHYPLL::assign_clkout0_right() {
    vl::wr(clkout0_right, vl::rd(clkout0_right_int));
}

void mipi_pll_dphy_DPHYPLL::assign_clkout180_left() {
    vl::wr(clkout180_left, vl::rd(clkout180_left_int));
}

void mipi_pll_dphy_DPHYPLL::assign_clkout180_right() {
    vl::wr(clkout180_right, vl::rd(clkout180_right_int));
}

void mipi_pll_dphy_DPHYPLL::assign_clkout270_left() {
    vl::wr(clkout270_left, vl::rd(clkout270_left_int));
}

void mipi_pll_dphy_DPHYPLL::assign_clkout270_right() {
    vl::wr(clkout270_right, vl::rd(clkout270_right_int));
}

void mipi_pll_dphy_DPHYPLL::assign_clkout90_left() {
    vl::wr(clkout90_left, vl::rd(clkout90_left_int));
}

void mipi_pll_dphy_DPHYPLL::assign_clkout90_right() {
    vl::wr(clkout90_right, vl::rd(clkout90_right_int));
}

void mipi_pll_dphy_DPHYPLL::assign_clkout_gp() {
    vl::wr(clkout_gp, vl::rd(clkout_gp_int));
}

void mipi_pll_dphy_DPHYPLL::assign_clkout_stop_left() {
    vl::wr(clkout_stop_left, vl::rd(clkout_stop_left_int));
}

void mipi_pll_dphy_DPHYPLL::assign_clkout_stop_right() {
    vl::wr(clkout_stop_right, vl::rd(clkout_stop_right_int));
}

void mipi_pll_dphy_DPHYPLL::assign_vpl_det() {
    vl::wr(vpl_det, vl::rd(vpl_det_int));
}

void mipi_pll_dphy_DPHYPLL::assign_vreg_mpll() {
    vl::wr(vreg_mpll, vl::rd(vreg_mpll_int));
}

void mipi_pll_dphy_DPHYPLL::assign_tiehigh() {
    vl::wr(tiehigh, vl::logic<1>(0x1ULL));
}
