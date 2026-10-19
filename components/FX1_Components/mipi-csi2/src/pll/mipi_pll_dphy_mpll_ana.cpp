//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_mpll_ana
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_mpll_ana.h"

mipi_pll_dphy_mpll_ana::mipi_pll_dphy_mpll_ana(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    tie0__drv.o(tie0_);
    tie1__drv.o(tie1_);
    SC_METHOD(split_mpll_prg);
    sensitive << mpll_prg;
    SC_METHOD(merge_meas_iv_h_out);
    sensitive << meas_iv_h_out_b[0] << meas_iv_h_out_b[1];
    SC_METHOD(assign_mpwr_on_gated);
    sensitive << mpll_pwron << pddq_h_dly;
    SC_METHOD(assign_mpll_gear_shift_gated);
    sensitive << mpll_gear_shift << pddq_h_dly;
    SC_METHOD(assign_mpll_rst_gated);
    sensitive << mpll_reset << pddq_h_dly;
    SC_METHOD(assign_supply_ok_vco);
    sensitive << vphreg << gd << pwron_n << pwron_h << vpl << bypass_vreg_ckout_h << bypass_vreg_ckout;
    SC_METHOD(assign_supply_ok_vco_sync);
    sensitive << supply_ok_vco << supply_ok_vco_ck_l2h;
    SC_METHOD(assign_vreg_mpll);
    sensitive << supply_ok_vco_sync;
    SC_METHOD(assign_vp_l2h);
    sensitive << supply_ok_vco_sync;
    SC_METHOD(glue_regs_meas_iv);
    sensitive << meas_iv;
    SC_METHOD(glue_regs_mpll_prg);
    sensitive << mpll_prg_b[0] << mpll_prg_b[1] << mpll_prg_b[2] << mpll_prg_b[3] << mpll_prg_b[4] << mpll_prg_b[5] << mpll_prg_b[6] << mpll_prg_b[7] << mpll_prg_b[8] << mpll_prg_b[9] << mpll_prg_b[10] << mpll_prg_b[11] << mpll_prg_b[12] << mpll_prg_b[13] << mpll_prg_b[14] << mpll_prg_b[15] << mpll_prg_b[16];
    SC_METHOD(split_regs_meas_iv_h);
    sensitive << c_regs_meas_iv_h;
    dont_initialize();
    SC_METHOD(split_regs_vco_cntrl_prot);
    sensitive << c_regs_vco_cntrl_prot;
    dont_initialize();
    // // Instances of widgets/alias/symbol
    // \alias   gg( .MINUS(icp), .PLUS(vref) );
    // mpll_vreg_ckout  vreg_cko( .bypass(bypass_vreg_ckout),
    // .bypass_h(bypass_vreg_ckout_h), .gd(gd),
    // .meas_vp_cko_h(meas_iv_h[3]), .meas_vref_h(meas_iv_h[16]),
    // .pwron_h(pwron_h), .pwron_n(pwron_n), .rst_h(rst_h),
    // .vphreg(vphreg), .vpl(vpl), .vref(vref_cko), .vreg_mpll(vreg_mpll),
    // .atb_s_m(atb_s_m_int), .atb_s_p(atb_s_p_int) );
    // mpll_vcntrl_cap  capx_vcntrl( .capon_hn(capon_hn), .capon_n(capon_n),
    // .gd(gd), .vcntrl(vcntrl), .vp_cp(vp_cp), .vpsf(vpsf) );
    // mpll_vreg  vrf( .gd(gd), .gmpc_h(gmp_cntrl_h), .gs_h(gs_h),
    // .meas_iv_h(meas_iv_h[10:6]), .meas_iv_h_14(meas_iv_h[14]),
    // .meas_iv_h_15(meas_iv_h[15]), .meas_iv_h_2(meas_iv_h[2]),
    // .pwron_h(pwron_h), .pwron_n(pwron_n), .reset_h(rst_h), .vinm(vref),
    // .vinp(vcntrl), .vmaster_bias(vmb), .vp(vpl), .vphreg(vphreg),
    // .vrefbf(vrfbf), .icasc(icasc), .icp(icp), .icp1(icp1), .ivco(ivco),
    // .vcntrl(vcntrl), .vmb(vmb), .vp_l2h(vp_l2h), .vpsf(vpsf),
    // .atb_force_m(atb_f_m), .atb_sense_m(atb_s_m_int),
    // .atb_sense_p(atb_s_p_int) );
    // mpll_pdcplf  pdcplf( .cp_prog(cp_bias_cntrl_prot[4:0]), .gd(gd),
    // .gmp_cntrl(gmp_cntrl_prot), .icasc(icasc), .icp(icp), .icp1(icp1),
    // .int_cntrl(int_cntrl_prot), .ivco_ctrl(ivco), .oscclk(oscclk_prot),
    // .prop_cntrl(prop_cntrl_prot), .pwron(pwron),
    // .ref_mpll_clk(clkrefdiv), .vcntrl_buf(vcntrl), .vp(vpl),
    // .vp_cp(vp_cp), .dn_lockdet(dn_lockdet), .up_lockdet(up_lockdet),
    // .dec_freq(dec_freq), .inc_freq(inc_freq), .iout_int_n(vint),
    // .iout_int_p(vint), .iout_prop_n(vcntrl), .iout_prop_p(vcntrl),
    // .vcntrl(vcntrl), .vint(vint) );
    // mipi_pll_dphy_tieLowHigh_HV  I110( .high(tie_high_h), .low(tie_low_h), .gd(gd),
    // .vph(vphreg) );
    // mpll_vreg1p2  vreg_cp( .bypass(bypass_vreg_cp),
    // .bypass_h(bypass_vreg_cp_h), .gd(gd), .meas_vpcp_h(meas_iv_h[4]),
    // .meas_vref_h(meas_iv_h[5]), .pwron_h(pwron_h), .pwron_n(pwron_n),
    // .rst_h(rst_h), .vref(vref_vpcp), .vp_cp(vp_cp),
    // .atb_s_m(atb_s_m_int), .atb_s_p(atb_s_p_int), .vp(vpl),
    // .vphreg(vphreg) );
    regs.reset(new mipi_pll_dphy_mpll_logic_l2h("regs"));
    regs->atb_sense_sel(atb_sense_sel);
    regs->capon(mpll_prg_b[4]);
    regs->clksel(clksel);
    regs->clksqren(clksqron);
    regs->cp_bias_cntrl(cp_bias_cntrl);
    regs->gd(gd);
    regs->gmp_cntrl(gmp_cntrl);
    regs->int_cntrl(int_cntrl);
    regs->meas_iv(c_regs_meas_iv);
    regs->mpll_gear_shift(mpll_gear_shift);
    regs->mpll_ph_sel_clk(mpll_ph_sel_clk);
    regs->mpll_ph_sel_wd(mpll_ph_sel_wd);
    regs->mpll_pmix_clk_sel(mpll_pmix_clk_sel);
    regs->mpll_prg(c_regs_mpll_prg);
    regs->mpll_pwron(mpll_pwron);
    regs->mpll_pwron_pmix(mpll_pwron_pmix);
    regs->mpll_relock_gear(mpll_relock_gear);
    regs->mpll_relock_vint(mpll_relock_vint);
    regs->mpll_rst(mpll_reset);
    regs->mpll_rst_pmix(mpll_rst_pmix);
    regs->opmode(opmode);
    regs->prop_cntrl(prop_cntrl);
    regs->ref_cntrl(ref_cntrl);
    regs->vco_cntrl(vco_cntrl);
    regs->vp(vp);
    regs->vphreg(vphreg);
    regs->vpl(vpl);
    regs->vss(vss);
    regs->atb_select_h(atb_select_h);
    regs->byp_ncntrl_div(byp_ncntrldiv);
    regs->bypass_ckout(bypass_vreg_ckout);
    regs->bypass_ckout_h(bypass_vreg_ckout_h);
    regs->bypass_cp(bypass_vreg_cp);
    regs->bypass_cp_h(bypass_vreg_cp_h);
    regs->capon_hn(capon_hn);
    regs->capon_n(capon_n);
    regs->clksqron(clksqr_on);
    regs->clksqron_h(clksqr_on_h);
    regs->cp_bias_cntrl_prot(cp_bias_cntrl_prot);
    regs->gmp_cntrl_h(gmp_cntrl_h);
    regs->gmp_cntrl_prot(gmp_cntrl_prot);
    regs->gs_h(gs_h);
    regs->int_cntrl_prot(int_cntrl_prot);
    regs->meas_iv_h(c_regs_meas_iv_h);
    regs->mpll_dcc_2nd_on(dcc_2nd_on);
    regs->mpll_dcc_2nd_on_h(dcc_2nd_on_h);
    regs->mpll_ph_sel_clk_out(mpll_ph_sel_clk_out);
    regs->mpll_ph_sel_wd_out(mpll_ph_sel_wd_out);
    regs->mpll_prg_h(mpll_prg_h);
    regs->mpll_prg_hn(mpll_prg_hn);
    regs->pmix_clk_sel(pmix_clk_sel);
    regs->prop_cntrl_prot(prop_cntrl_prot);
    regs->pwron(pwron);
    regs->pwron_h(pwron_h);
    regs->pwron_n(pwron_n);
    regs->pwron_pmix_h(pwron_pmix_h);
    regs->pwron_pmix_n(pwron_pmix_n);
    regs->ref_cntrl_prot(ref_cntrl_prot);
    regs->relock_gear_h(net1458);
    regs->relock_vint_h(net1457);
    regs->reset(reset);
    regs->reset_h(rst_h);
    regs->rst_pmix_h(rst_pmix_h);
    regs->vco_cntrl_prot(c_regs_vco_cntrl_prot);
    regs->vpl_det(vpl_det);
    // mpll_mux_vref  I103( .s_h(mpll_prg_h[13:12]), .s_hn(mpll_prg_hn[13:12])
    // , .vref({atb_s_m_int,vbg,vdoubled,vpl}), .vrefout(vref_cko),
    // .gd(gd), .tie_high(tie_high_h) );
    // mpll_mux_vref  I104( .s_h(mpll_prg_h[11:10]), .s_hn(mpll_prg_hn[11:10])
    // , .vref({atb_s_m_int,vbg,vdoubled,vpl}), .vrefout(vref_vpcp),
    // .gd(gd), .tie_high(tie_high_h) );
    // mpll_mux_vref  I106( .s_h(mpll_prg_h[15:14]), .s_hn(mpll_prg_hn[15:14])
    // , .vref({atb_s_m_int,vbg,vdoubled,vpl}), .vrefout(vref_pmix),
    // .gd(gd), .tie_high(tie_high_h) );
    // mpll_atb_enable  atb( .atb_select_h(atb_select_h), .gd(gd), .vp(vp),
    // .vphreg(vphreg), .atb_s_m_ext(atb_s_m), .atb_s_m_int(atb_s_m_int),
    // .atb_s_p_ext(atb_s_p), .atb_s_p_int(atb_s_p_int) );
    // mpll_vrfgen1  rfbf( .gd(gd), .reset_h(rst_h), .vp_l2h(vp_l2h),
    // .vp_dnw(vp_dnw) );
    prediv.reset(new mipi_pll_dphy_mpll_prediv_prog("prediv"));
    prediv->ck_ref_in(ckref_prot);
    prediv->gd(gd);
    prediv->ref_cntrl(ref_cntrl_prot);
    prediv->rstz(pwron);
    prediv->vp(vpl);
    prediv->ck_ref_out(clkrefdiv);
    SC_METHOD(glue_I01PLLCOREMODEL_on);
    sensitive << clk_stable << supply_ok_vco;
    SC_METHOD(glue_I01PLLCOREMODEL_feedbackdiv);
    sensitive << m_obs_core_model;
    SC_METHOD(glue_I01PLLCOREMODEL_inputdiv);
    SC_METHOD(glue_I01PLLCOREMODEL_outputdiv);
    SC_METHOD(glue_I01PLLCOREMODEL_phsel);
    I01PLLCOREMODEL.reset(new mipi_pll_dphy_pllcoremodel("I01PLLCOREMODEL"));
    I01PLLCOREMODEL->on(c_I01PLLCOREMODEL_on);
    I01PLLCOREMODEL->clkin(clkrefdiv);
    I01PLLCOREMODEL->feedbackdiv(c_I01PLLCOREMODEL_feedbackdiv);
    I01PLLCOREMODEL->inputdiv(c_I01PLLCOREMODEL_inputdiv);
    I01PLLCOREMODEL->outputdiv(c_I01PLLCOREMODEL_outputdiv);
    I01PLLCOREMODEL->bypass(tie1_);
    I01PLLCOREMODEL->deskewen(tie1_);
    I01PLLCOREMODEL->deskewin(oscclk_prot);
    I01PLLCOREMODEL->phsel(c_I01PLLCOREMODEL_phsel);
    I01PLLCOREMODEL->enable_ssc(tie0_);
    I01PLLCOREMODEL->center_downz(tie0_);
    I01PLLCOREMODEL->ssc_depth(nc_I01PLLCOREMODEL_ssc_depth);
    I01PLLCOREMODEL->ssc_freq(nc_I01PLLCOREMODEL_ssc_freq);
    I01PLLCOREMODEL->lock(lock_vco);
    I01PLLCOREMODEL->refclk(refclk_from_pllcore_model);
    I01PLLCOREMODEL->fbclk(nc_I01PLLCOREMODEL_fbclk);
    I01PLLCOREMODEL->clkout(ck_l2h);
    I01PLLCOREMODEL->clkout_div(nc_I01PLLCOREMODEL_clkout_div);
    l2h.reset(new mipi_pll_dphy_mpll_l2h("l2h"));
    l2h->ck_l2h(ck_l2h);
    l2h->gd(gd);
    l2h->highspeed_n(vco_cntr_prot[3]);
    l2h->pwron(pwron);
    l2h->quad_on(dcc_2nd_on);
    l2h->vci(nc_l2h_vci);
    l2h->vcim(tie0_);
    l2h->vcip(tie0_);
    l2h->vcq(nc_l2h_vcq);
    l2h->vcqm(tie0_);
    l2h->vcqp(tie0_);
    l2h->vreg_mpll(supply_ok_vco_sync);
    l2h->ckdcc_i_m(nc_l2h_ckdcc_i_m);
    l2h->ckdcc_i_p(nc_l2h_ckdcc_i_p);
    l2h->ckdcc_q_m(nc_l2h_ckdcc_q_m);
    l2h->ckdcc_q_p(nc_l2h_ckdcc_q_p);
    l2h->clk_i_m(clk_i_m);
    l2h->clk_i_p(clk_i_p);
    l2h->clk_q_m(clk_q_m);
    l2h->clk_q_p(clk_q_p);
    l2h->ph_pmix(ph_pmix);
    l2h->xor_clk(xor_clk);
    pd.reset(new mipi_pll_dphy_mpll_phdet("pd"));
    pd->gd(gd);
    pd->oscclk(oscclk);
    pd->refclk(refclk_from_pllcore_model);
    pd->vp(vp);
    pd->dec_freq(nc_pd_dec_freq);
    pd->dec_freqn(dn_lockdet);
    pd->inc_freq(nc_pd_inc_freq);
    pd->inc_freqn(up_lockdet);
    I113.reset(new mipi_pll_dphy_mpll_cdm_prot_clk("I113"));
    I113->ckref(clkref);
    I113->gd(gd);
    I113->oscclk(oscclk);
    I113->vp(vpl);
    I113->ckref_prot(ckref_prot);
    I113->oscclk_prot(oscclk_prot);
    SC_THREAD(proc_initial_3045);
    set_stack_size(0x20000);
    SC_THREAD(proc_initial_3051);
    set_stack_size(0x20000);
    SC_METHOD(ev_glue);
    sensitive << ck_l2h;
    SC_METHOD(proc_always_3133);
    sensitive << ev_sig.negedge_event();
    dont_initialize();
    SC_METHOD(ev_glue_2);
    sensitive << ck_l2h;
    SC_METHOD(proc_always_3136);
    sensitive << ev_sig_2.negedge_event();
    dont_initialize();
    SC_METHOD(ev_glue_3);
    sensitive << ck_l2h;
    SC_METHOD(proc_always_3139);
    sensitive << ev_sig_3.negedge_event();
    dont_initialize();
    SC_METHOD(ev_glue_4);
    sensitive << ck_l2h;
    SC_METHOD(proc_always_3142);
    sensitive << ev_sig_4.negedge_event();
    dont_initialize();
    SC_METHOD(ev_glue_5);
    sensitive << ck_l2h;
    SC_METHOD(proc_always_3145);
    sensitive << ev_sig_5.negedge_event();
    dont_initialize();
    SC_METHOD(ev_glue_6);
    sensitive << ck_l2h;
    SC_METHOD(proc_always_3148);
    sensitive << ev_sig_6.negedge_event();
    dont_initialize();
    SC_METHOD(ev_glue_7);
    sensitive << ck_l2h;
    SC_METHOD(proc_always_3151);
    sensitive << ev_sig_7.negedge_event();
    dont_initialize();
    SC_METHOD(ev_glue_8);
    sensitive << ck_l2h;
    SC_METHOD(proc_always_3154);
    sensitive << ev_sig_8.negedge_event();
    dont_initialize();
}

void mipi_pll_dphy_mpll_ana::split_mpll_prg() {
    const vl::logic<17> v = vl::rd(mpll_prg);
    mpll_prg_b[0].write(v.bit(0).sl());
    mpll_prg_b[1].write(v.bit(1).sl());
    mpll_prg_b[2].write(v.bit(2).sl());
    mpll_prg_b[3].write(v.bit(3).sl());
    mpll_prg_b[4].write(v.bit(4).sl());
    mpll_prg_b[5].write(v.bit(5).sl());
    mpll_prg_b[6].write(v.bit(6).sl());
    mpll_prg_b[7].write(v.bit(7).sl());
    mpll_prg_b[8].write(v.bit(8).sl());
    mpll_prg_b[9].write(v.bit(9).sl());
    mpll_prg_b[10].write(v.bit(10).sl());
    mpll_prg_b[11].write(v.bit(11).sl());
    mpll_prg_b[12].write(v.bit(12).sl());
    mpll_prg_b[13].write(v.bit(13).sl());
    mpll_prg_b[14].write(v.bit(14).sl());
    mpll_prg_b[15].write(v.bit(15).sl());
    mpll_prg_b[16].write(v.bit(16).sl());
}

void mipi_pll_dphy_mpll_ana::merge_meas_iv_h_out() {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(meas_iv_h_out_b[0]));
    v.set_bit(1, vl::rd(meas_iv_h_out_b[1]));
    vl::wr(meas_iv_h_out, v);
}

vl::logic<17> mipi_pll_dphy_mpll_ana::rd_meas_iv_h() const {
    vl::logic<17> v(0);
    v.set_bit(0, vl::rd(meas_iv_h[0]));
    v.set_bit(1, vl::rd(meas_iv_h[1]));
    v.set_bit(2, vl::rd(meas_iv_h[2]));
    v.set_bit(3, vl::rd(meas_iv_h[3]));
    v.set_bit(4, vl::rd(meas_iv_h[4]));
    v.set_bit(5, vl::rd(meas_iv_h[5]));
    v.set_bit(6, vl::rd(meas_iv_h[6]));
    v.set_bit(7, vl::rd(meas_iv_h[7]));
    v.set_bit(8, vl::rd(meas_iv_h[8]));
    v.set_bit(9, vl::rd(meas_iv_h[9]));
    v.set_bit(10, vl::rd(meas_iv_h[10]));
    v.set_bit(11, vl::rd(meas_iv_h[11]));
    v.set_bit(12, vl::rd(meas_iv_h[12]));
    v.set_bit(13, vl::rd(meas_iv_h[13]));
    v.set_bit(14, vl::rd(meas_iv_h[14]));
    v.set_bit(15, vl::rd(meas_iv_h[15]));
    v.set_bit(16, vl::rd(meas_iv_h[16]));
    return v;
}

vl::logic<2> mipi_pll_dphy_mpll_ana::rd_meas_iv_h_out() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(meas_iv_h_out_b[0]));
    v.set_bit(1, vl::rd(meas_iv_h_out_b[1]));
    return v;
}

vl::logic<17> mipi_pll_dphy_mpll_ana::rd_mpll_prg() const {
    vl::logic<17> v(0);
    v.set_bit(0, vl::rd(mpll_prg_b[0]));
    v.set_bit(1, vl::rd(mpll_prg_b[1]));
    v.set_bit(2, vl::rd(mpll_prg_b[2]));
    v.set_bit(3, vl::rd(mpll_prg_b[3]));
    v.set_bit(4, vl::rd(mpll_prg_b[4]));
    v.set_bit(5, vl::rd(mpll_prg_b[5]));
    v.set_bit(6, vl::rd(mpll_prg_b[6]));
    v.set_bit(7, vl::rd(mpll_prg_b[7]));
    v.set_bit(8, vl::rd(mpll_prg_b[8]));
    v.set_bit(9, vl::rd(mpll_prg_b[9]));
    v.set_bit(10, vl::rd(mpll_prg_b[10]));
    v.set_bit(11, vl::rd(mpll_prg_b[11]));
    v.set_bit(12, vl::rd(mpll_prg_b[12]));
    v.set_bit(13, vl::rd(mpll_prg_b[13]));
    v.set_bit(14, vl::rd(mpll_prg_b[14]));
    v.set_bit(15, vl::rd(mpll_prg_b[15]));
    v.set_bit(16, vl::rd(mpll_prg_b[16]));
    return v;
}

vl::logic<4> mipi_pll_dphy_mpll_ana::rd_vco_cntr_prot() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(vco_cntr_prot[0]));
    v.set_bit(1, vl::rd(vco_cntr_prot[1]));
    v.set_bit(2, vl::rd(vco_cntr_prot[2]));
    v.set_bit(3, vl::rd(vco_cntr_prot[3]));
    return v;
}

// mpll_vco  vco( .gd(gd), .ivco_osc(ivco_osc), .nwell(vp_dnw),
// .pwron(pwron), .quad_on(dcc_2nd_on), .reset(reset), .vci(vci),
// .vcim(vcim), .vcip(vcip), .vco_ctrl(vco_cntr_prot), .vcq(vcq),
// .vcqm(vcqm), .vcqp(vcqp), .vp(vpl), .vp_l2h(vp_l2h),
// .vreg_mpll(vreg_mpll), .ck_l2h(ck_l2h), .ckdcc_i_m(ckdcc_i_m),
// .ckdcc_i_p(ckdcc_i_p), .ckdcc_q_m(ckdcc_q_m), .ckdcc_q_p(ckdcc_q_p)
// , .clk_i_m(clk_i_m), .clk_i_p(clk_i_p), .clk_q_m(clk_q_m),
// .clk_q_p(clk_q_p), .ph_pmix(ph_pmix), .xor_clk(xor_clk) );
// pmix_vbamp  vbamp( .gd(gd), .inp(vbg), .pon_h(pwron_h),
// .prg(mpll_prg_h), .vph(vphreg), .vout(vdoubled) );
// mpll_dcc_top  dcc( .ckdcc_i_m(ckdcc_i_m), .ckdcc_i_p(ckdcc_i_p),
// .ckdcc_q_m(ckdcc_q_m), .ckdcc_q_p(ckdcc_q_p),
// .dcc2nd_onh(dcc_2nd_on_h), .gd(gd), .mvc_iq_h(meas_iv_h[11]),
// .mvcm_h(meas_iv_h[1]), .mvcp_h(meas_iv_h[0]), .pwr_on_h(pwron_h),
// .rst_h(rst_h), .vphreg(vphreg), .vreg_mpll(vreg_mpll),
// .xor_in(xor_clk), .vci(vci), .vcim(vcim), .vcip(vcip), .vcq(vcq),
// .vcqm(vcqm), .vcqp(vcqp), .atb_s_m(atb_s_m_int),
// .atb_s_p(atb_s_p_int) );
void mipi_pll_dphy_mpll_ana::assign_mpwr_on_gated() {
    vl::wr(mpwr_on_gated, vl::land(vl::rd(mpll_pwron), vl::lnot(vl::rd(pddq_h_dly))));
}

void mipi_pll_dphy_mpll_ana::assign_mpll_gear_shift_gated() {
    vl::wr(mpll_gear_shift_gated, vl::land(vl::rd(mpll_gear_shift), vl::lnot(vl::rd(pddq_h_dly))));
}

void mipi_pll_dphy_mpll_ana::assign_mpll_rst_gated() {
    vl::wr(mpll_rst_gated, vl::land(vl::rd(mpll_reset), vl::lnot(vl::rd(pddq_h_dly))));
}

void mipi_pll_dphy_mpll_ana::assign_supply_ok_vco() {
    vl::wr(supply_ok_vco, vl::lor(vl::land(vl::land(vl::land(vl::ceq(vl::rd(vphreg), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(pwron_n), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(pwron_h), vl::logic<1>(0x1ULL))), vl::land(vl::land(vl::land(vl::ceq(vl::rd(vpl), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(bypass_vreg_ckout_h), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(bypass_vreg_ckout), vl::logic<1>(0x1ULL)))));
}

void mipi_pll_dphy_mpll_ana::assign_supply_ok_vco_sync() {
    vl::wr(supply_ok_vco_sync, vl::mux(vl::rd(supply_ok_vco), vl::rand_(vl::rd(supply_ok_vco_ck_l2h)), vl::ror_(vl::rd(supply_ok_vco_ck_l2h))));
}

void mipi_pll_dphy_mpll_ana::assign_vreg_mpll() {
    vl::wr(vreg_mpll, vl::rd(supply_ok_vco_sync));
}

void mipi_pll_dphy_mpll_ana::assign_vp_l2h() {
    vl::wr(vp_l2h, vl::rd(supply_ok_vco_sync));
}

void mipi_pll_dphy_mpll_ana::glue_regs_meas_iv() { vl::wr(c_regs_meas_iv, vl::rd(meas_iv).slice<17>(0)); }

void mipi_pll_dphy_mpll_ana::glue_regs_mpll_prg() { vl::wr(c_regs_mpll_prg, rd_mpll_prg()); }

void mipi_pll_dphy_mpll_ana::split_regs_meas_iv_h() {
    const vl::logic<17> v = vl::rd(c_regs_meas_iv_h);
    meas_iv_h[0].write(v.bit(0).sl());
    meas_iv_h[1].write(v.bit(1).sl());
    meas_iv_h[2].write(v.bit(2).sl());
    meas_iv_h[3].write(v.bit(3).sl());
    meas_iv_h[4].write(v.bit(4).sl());
    meas_iv_h[5].write(v.bit(5).sl());
    meas_iv_h[6].write(v.bit(6).sl());
    meas_iv_h[7].write(v.bit(7).sl());
    meas_iv_h[8].write(v.bit(8).sl());
    meas_iv_h[9].write(v.bit(9).sl());
    meas_iv_h[10].write(v.bit(10).sl());
    meas_iv_h[11].write(v.bit(11).sl());
    meas_iv_h_out_b[0].write(v.bit(12).sl());
    meas_iv_h_out_b[1].write(v.bit(13).sl());
    meas_iv_h[14].write(v.bit(14).sl());
    meas_iv_h[15].write(v.bit(15).sl());
    meas_iv_h[16].write(v.bit(16).sl());
}

void mipi_pll_dphy_mpll_ana::split_regs_vco_cntrl_prot() {
    const vl::logic<4> v = vl::rd(c_regs_vco_cntrl_prot);
    vco_cntr_prot[0].write(v.bit(0).sl());
    vco_cntr_prot[1].write(v.bit(1).sl());
    vco_cntr_prot[2].write(v.bit(2).sl());
    vco_cntr_prot[3].write(v.bit(3).sl());
}

void mipi_pll_dphy_mpll_ana::glue_I01PLLCOREMODEL_on() { vl::wr(c_I01PLLCOREMODEL_on, vl::land(vl::rd(clk_stable), vl::rd(supply_ok_vco))); }

void mipi_pll_dphy_mpll_ana::glue_I01PLLCOREMODEL_feedbackdiv() { vl::wr(c_I01PLLCOREMODEL_feedbackdiv, vl::realtobits((vl::to_real(vl::cat(vl::rd(m_obs_core_model).slice<9>(1) + vl::logic<9>(0x1ULL), vl::rd(m_obs_core_model).bit(0)), false) * 1.0))); }

void mipi_pll_dphy_mpll_ana::glue_I01PLLCOREMODEL_inputdiv() { vl::wr(c_I01PLLCOREMODEL_inputdiv, vl::realtobits(1.0)); }

void mipi_pll_dphy_mpll_ana::glue_I01PLLCOREMODEL_outputdiv() { vl::wr(c_I01PLLCOREMODEL_outputdiv, vl::realtobits(1.0)); }

void mipi_pll_dphy_mpll_ana::glue_I01PLLCOREMODEL_phsel() { vl::wr(c_I01PLLCOREMODEL_phsel, vl::logic<5>(0x0ULL)); }

void mipi_pll_dphy_mpll_ana::proc_initial_3045() {
    vl::logic<1> pddq_h_dly_sh = vl::rd(pddq_h_dly);
    pddq_h_dly_sh = vl::logic<1>(0x1ULL);
    vl::wr(pddq_h_dly, pddq_h_dly_sh);
    wait(vl::dly(100.0, TU_, TP_));
    pddq_h_dly_sh = vl::rd(pddq_h_dly);
    pddq_h_dly_sh = vl::logic<1>(0x0ULL);
    vl::wr(pddq_h_dly, pddq_h_dly_sh);
}

void mipi_pll_dphy_mpll_ana::proc_initial_3051() {
    vl::logic<1> clk_stable_sh = vl::rd(clk_stable);
    vl::logic<1> err_flg_sh = vl::rd(err_flg);
    clk_stable_sh = vl::logic<1>(0x1ULL);
    vl::wr(clk_stable, clk_stable_sh);
    wait(vl::dly(0.1, TU_, TP_));
    clk_stable_sh = vl::rd(clk_stable);
    err_flg_sh = vl::rd(err_flg);
    err_flg_sh = vl::logic<1>(0x0ULL);
    vl::wr(err_flg, err_flg_sh);
    clk_stable_sh = vl::logic<1>(0x0ULL);
    vl::wr(clk_stable, clk_stable_sh);
    wait(mpwr_on_gated.value_changed_event());
    clk_stable_sh = vl::rd(clk_stable);
    err_flg_sh = vl::rd(err_flg);
    t1 = vl::realtime(TU_);
    for (;;) {
        if (vl::rd(mpwr_on_gated).test()) {
            if (vl::rd(mpll_rst_gated).test()) {
                wait(mpll_rst_gated.negedge_event());
                clk_stable_sh = vl::rd(clk_stable);
                err_flg_sh = vl::rd(err_flg);
                t2 = vl::realtime(TU_);
                wait(vl::dly(1.0, TU_, TP_));
                clk_stable_sh = vl::rd(clk_stable);
                err_flg_sh = vl::rd(err_flg);
                // if (t2 - t1 < 10000) begin
                // $display("%.3fns - Error! [ERROR: reset timing is less than 10us] ", $realtime);
                // err_flg = 1'b1;
                // end
                {
                    sc_core::sc_process_handle fork_blk_1, fork_blk_2;
                    sc_core::sc_spawn_options fork_opt;
                    fork_opt.set_stack_size(0x20000);
                    fork_blk_1 = sc_core::sc_spawn([&]() {
                        if (vl::rd(mpll_gear_shift_gated).test()) {
                            wait(mpll_gear_shift_gated.negedge_event());
                            clk_stable_sh = vl::rd(clk_stable);
                            err_flg_sh = vl::rd(err_flg);
                            t3 = vl::realtime(TU_);
                            wait(vl::dly(1.0, TU_, TP_));
                            clk_stable_sh = vl::rd(clk_stable);
                            err_flg_sh = vl::rd(err_flg);
                            // if (t3 - t2 < `DPHY_MPLL_RST_TIME)
                            // begin
                            // $display("%.3fns - Error! [ERROR: incorrect deassertion for mpll_gear_shift] ", $realtime);
                            // clk_stable = 1'b0;
                            // err_flg = 1'b1;
                            // end
                            // else
                            // clk_stable = 1'b1;
                            clk_stable_sh = vl::logic<1>(0x1ULL);
                            vl::wr(clk_stable, clk_stable_sh);
                            for (int64_t rep = 400; rep > 0; --rep) {
                                wait(clkrefdiv.posedge_event());
                                clk_stable_sh = vl::rd(clk_stable);
                                err_flg_sh = vl::rd(err_flg);
                            }
                        } else {
                            wait(vl::dly(2700.0, TU_, TP_));
                            clk_stable_sh = vl::rd(clk_stable);
                            err_flg_sh = vl::rd(err_flg);
                        }
                        fork_blk_2.kill();
                    }, sc_core::sc_gen_unique_name("fork_blk_1"), &fork_opt);
                    fork_blk_2 = sc_core::sc_spawn([&]() {
                        wait(mpll_reset.posedge_event());
                        clk_stable_sh = vl::rd(clk_stable);
                        err_flg_sh = vl::rd(err_flg);
                        fork_blk_1.kill();
                    }, sc_core::sc_gen_unique_name("fork_blk_2"), &fork_opt);
                    while (!(fork_blk_1.terminated() && fork_blk_2.terminated())) wait(fork_blk_1.terminated_event() | fork_blk_2.terminated_event());
                }
                clk_stable_sh = vl::rd(clk_stable);
                err_flg_sh = vl::rd(err_flg);
            } else {
                wait(mpll_rst_gated.posedge_event());
                clk_stable_sh = vl::rd(clk_stable);
                err_flg_sh = vl::rd(err_flg);
                clk_stable_sh = vl::logic<1>(0x0ULL);
                vl::wr(clk_stable, clk_stable_sh);
                t1 = vl::realtime(TU_);
                wait(vl::dly(1.0, TU_, TP_));
                clk_stable_sh = vl::rd(clk_stable);
                err_flg_sh = vl::rd(err_flg);
            }
        } else {
            clk_stable_sh = vl::logic<1>(0x0ULL);
            vl::wr(clk_stable, clk_stable_sh);
            err_flg_sh = vl::logic<1>(0x0ULL);
            vl::wr(err_flg, err_flg_sh);
            wait(mpwr_on_gated.posedge_event());
            clk_stable_sh = vl::rd(clk_stable);
            err_flg_sh = vl::rd(err_flg);
            t1 = vl::realtime(TU_);
            wait(vl::dly(0.001, TU_, TP_));
            clk_stable_sh = vl::rd(clk_stable);
            err_flg_sh = vl::rd(err_flg);
        }
    }
}

// begin
// if (supply_ok_vco===1'b0)
// supply_ok_vco_ck_l2h[0] <= 1'b0;
// else if (supply_ok_vco===1'b1)
// supply_ok_vco_ck_l2h[0] <= 1'b1;
// end
void mipi_pll_dphy_mpll_ana::proc_always_3133() {
    supply_ok_vco_ck_l2h.write_bit(0, vl::rd(supply_ok_vco));
}

void mipi_pll_dphy_mpll_ana::ev_glue() { vl::wr(ev_sig, vl::rd(ck_l2h).bit(0)); }

void mipi_pll_dphy_mpll_ana::proc_always_3136() {
    supply_ok_vco_ck_l2h.write_bit(1, vl::rd(supply_ok_vco));
}

void mipi_pll_dphy_mpll_ana::ev_glue_2() { vl::wr(ev_sig_2, vl::rd(ck_l2h).bit(1)); }

void mipi_pll_dphy_mpll_ana::proc_always_3139() {
    supply_ok_vco_ck_l2h.write_bit(2, vl::rd(supply_ok_vco));
}

void mipi_pll_dphy_mpll_ana::ev_glue_3() { vl::wr(ev_sig_3, vl::rd(ck_l2h).bit(2)); }

void mipi_pll_dphy_mpll_ana::proc_always_3142() {
    supply_ok_vco_ck_l2h.write_bit(3, vl::rd(supply_ok_vco));
}

void mipi_pll_dphy_mpll_ana::ev_glue_4() { vl::wr(ev_sig_4, vl::rd(ck_l2h).bit(3)); }

void mipi_pll_dphy_mpll_ana::proc_always_3145() {
    supply_ok_vco_ck_l2h.write_bit(4, vl::rd(supply_ok_vco));
}

void mipi_pll_dphy_mpll_ana::ev_glue_5() { vl::wr(ev_sig_5, vl::rd(ck_l2h).bit(4)); }

void mipi_pll_dphy_mpll_ana::proc_always_3148() {
    supply_ok_vco_ck_l2h.write_bit(5, vl::rd(supply_ok_vco));
}

void mipi_pll_dphy_mpll_ana::ev_glue_6() { vl::wr(ev_sig_6, vl::rd(ck_l2h).bit(5)); }

void mipi_pll_dphy_mpll_ana::proc_always_3151() {
    supply_ok_vco_ck_l2h.write_bit(6, vl::rd(supply_ok_vco));
}

void mipi_pll_dphy_mpll_ana::ev_glue_7() { vl::wr(ev_sig_7, vl::rd(ck_l2h).bit(6)); }

void mipi_pll_dphy_mpll_ana::proc_always_3154() {
    supply_ok_vco_ck_l2h.write_bit(7, vl::rd(supply_ok_vco));
}

void mipi_pll_dphy_mpll_ana::ev_glue_8() { vl::wr(ev_sig_8, vl::rd(ck_l2h).bit(7)); }
