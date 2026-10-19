//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHY3AFECBRX
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHY3AFECBRX.h"

mipi_4_rx_dphy_ns_DPHY3AFECBRX::mipi_4_rx_dphy_ns_DPHY3AFECBRX(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(merge_cb_10u_ibias_ddl);
    sensitive << cb_10u_ibias_ddl_b[0] << cb_10u_ibias_ddl_b[1] << cb_10u_ibias_ddl_b[2] << cb_10u_ibias_ddl_b[3];
    SC_METHOD(merge_cb_5u_ilprx);
    sensitive << cb_5u_ilprx_b[0] << cb_5u_ilprx_b[1] << cb_5u_ilprx_b[2] << cb_5u_ilprx_b[3] << cb_5u_ilprx_b[4] << cb_5u_ilprx_b[5] << cb_5u_ilprx_b[6] << cb_5u_ilprx_b[7] << cb_5u_ilprx_b[8] << cb_5u_ilprx_b[9];
    SC_METHOD(assign_supply_ok);
    sensitive << vph << vp << vdd << vss << gd;
    SC_METHOD(assign_enable);
    sensitive << cb_on << cb_bg_en << cb_sel_chop_clk << cb_chop_clk_en;
    SC_METHOD(assign_cb_vref_lprx);
    sensitive << cb_tx_ok;
    SC_METHOD(assign_cb_vrefcd_lprx);
    sensitive << cb_tx_ok;
    SC_METHOD(assign_cb_vreflptx);
    sensitive << cb_tx_lp_ok;
    SC_METHOD(assign_cb_5u_ilprx);
    sensitive << cb_tx_lp_ok;
    SC_METHOD(assign_cb_10u_ilptx);
    sensitive << cb_tx_lp_ok;
    SC_METHOD(assign_cb_10u_ibias_ddl);
    sensitive << cb_tx_ok << cb_en_int;
    SC_METHOD(assign_cb_vref_mpll_reg_int);
    sensitive << cb_tx_lp_ok << cb_vref_mpll_reg_sel_hv;
    SC_METHOD(assign_cb_vref_mpll_reg);
    sensitive << supply_ok << cb_vref_mpll_reg_int;
    SC_METHOD(assign_vctrl_replica_int);
    sensitive << cb_cal_en_hv << cb_cal_repl_hv;
    SC_METHOD(assign_vctrl_replica_int_calibration);
    sensitive << vctrl_replica_int;
    SC_METHOD(assign_vref_bias);
    sensitive << supply_ok << enable;
    bias_preamp.reset(new mipi_4_rx_dphy_ns_DPHYCB_preamp_bias("bias_preamp"));
    bias_preamp->rx_pre_pon_h(rx_pre_pon_hv);
    bias_preamp->rx_pre_pon_hz(rx_pre_pon_hvz);
    bias_preamp->ibias_d2c_bias(cb_ibias_d2c_bias);
    bias_preamp->ihv(cb_ihv_hsrx);
    bias_preamp->ilv(cb_ilv_hsrx);
    bias_preamp->gd(gd);
    bias_preamp->vph(vph);
    IOcont.reset(new mipi_4_rx_dphy_ns_DPHY_iocont_buffer("IOcont"));
    IOcont->a(rext);
    IOcont->enhv(cb_rext_iocont_en_hv);
    IOcont->tielow_hv(tie_low_hv);
    IOcont->y(cb_rext_iocont);
    IOcont->gd(gd);
    IOcont->vdd(vdd);
    IOcont->vp(vp);
    IOcont->vph(vph);
    IOcont->vss(vss);
    SC_METHOD(split_bias_cb_10u_ibias_ddl);
    sensitive << c_bias_cb_10u_ibias_ddl;
    dont_initialize();
    SC_METHOD(split_bias_cb_5u_ilprx);
    sensitive << c_bias_cb_5u_ilprx;
    dont_initialize();
    // rppwl  R560( .b(gd), .in(vref_1000_int), .out(gd) );
    // rppwl  R561( .b(gd), .in(vctrl_replica), .out(gd) );
    // rppwl  R562( .b(gd), .in(cb_en_up_1st_hv), .out(gd) );
    // rppwl  R563( .b(vph), .in(cb_en_up_1st_hvz), .out(vph) );
    bias.reset(new mipi_4_rx_dphy_ns_DPHYCBRX_bias("bias"));
    bias->cb_cal_en_hv(cb_ibias_en_hv);
    bias->cb_cal_repl_hv(cb_cal_repl_hv);
    bias->en_45_ohm_hv(cb_en_45_ohm_hv);
    bias->ota_on_hv(cb_ibias_en_hv);
    bias->ota_on_hvz(cb_ibias_en_hvz);
    bias->ota_on_int_z(cb_ibias_en_int_z);
    bias->replv_i(repl_oloop);
    bias->vref(vref_bias);
    bias->cb_10u_ibias_ddl(c_bias_cb_10u_ibias_ddl);
    bias->cb_10u_ilptx(cb_10u_ilptx);
    bias->cb_5u_ilprx(c_bias_cb_5u_ilprx);
    bias->cb_preamp_cm(cb_ilv_ccm_hsrx);
    bias->replv_o(repl_oloop);
    bias->gd(gd);
    bias->vph(vph);
    dphyatb.reset(new mipi_4_rx_dphy_ns_DPHYATB("dphyatb"));
    dphyatb->atb_en(atb_on);
    dphyatb->atb_ext_con_hv(atb_ext_con_hv);
    dphyatb->atb_ext_con_hvz(atb_ext_con_hvz);
    dphyatb->atb_line(atb_line);
    dphyatb->cb_en_up_1st_hv(cb_en_up_1st_hv);
    dphyatb->cb_en_up_1st_hvz(cb_en_up_1st_hvz);
    dphyatb->clkhn(atb_clk_ref_hvz);
    dphyatb->clkhp(atb_clk_ref_hv);
    dphyatb->gd(gd);
    dphyatb->ponh(atb_en_hv);
    dphyatb->ponhz(atb_en_hvz);
    dphyatb->repl(cb_repl);
    dphyatb->rext(rext_esd);
    dphyatb->sarh(dac_value_hv);
    dphyatb->sarhz(dac_value_hvz);
    dphyatb->selh(atb_sel_hv);
    dphyatb->selhz(atb_sel_hvz);
    dphyatb->tie_low_hv(tie_low_hv);
    dphyatb->vctrl_replica(vctrl_replica_int_calibration);
    dphyatb->vph(vph);
    dphyatb->vref_1000(vref_1000_int);
    dphyatb->atb(atb);
    dphyatb->cb_comp_out(cb_comp_out);
    dphyatb->nbias(nbias);
    dphyatb->vdd(vdd);
    dphyatb->vss(vss);
    // DPHYCBRX_rext_drv  I593( .enz_rx_hv(enz_rx_hv),
    // .lb_setzrx_hv(lb_setzrx_hv), .rex(rext), .vph(vph) );
    // DPHYCBRX_cal  cal( .cb_cal_en_hv(cb_cal_en_hv),
    // .cb_cal_repl_hv(cb_cal_repl_hv), .cb_ical_x4_hv(cb_ical_x4_hv),
    // .en_45_ohm_hv(cb_en_45_ohm_hv), .rext(rext), .rext_2(vsense),
    // .vref_cal(vref_cal), .cb_repl(cb_repl), .rext_esd(rext_esd),
    // .gd(gd), .vph(vph) );
    // DPHYCB_vgen  vgen( .cb_bg_en(cb_bg_en),
    // .cb_burn_oscilator_int(cb_burn_oscilator_int),
    // .cb_cal_en_up_hv(tie_low_hv), .cb_cal_en_up_hvz(tie_high_hv),
    // .cb_chop_clk(cb_chop_clk), .cb_chop_clk_en(cb_chop_clk_en),
    // .cb_lpibias_en_hv(cb_lpibias_en_hv), .cb_on(cb_on),
    // .cb_sel_chop_clk(cb_sel_chop_clk), .cb_sel_lptx_hv(cb_sel_lptx_hv),
    // .cb_sel_lptx_hvz(cb_sel_lptx_hvz),
    // .cb_sel_vbg_prog_hv(cb_sel_vbg_prog_hv),
    // .cb_sel_vbg_prog_hvz(cb_sel_vbg_prog_hvz),
    // .cb_sel_vref_lprx_hv(cb_sel_vref_lprx_hv),
    // .cb_sel_vrefcd_lprx_hv(cb_sel_vrefcd_lprx_hv),
    // .cb_vbe_on(atb_vbe_sel),
    // .cb_vref_mpll_reg_sel_hv(cb_vref_mpll_reg_sel_hv),
    // .atb_vbe_out(atb_line), .cb_lptx(cb_vreflptx),
    // .cb_vref_lprx(cb_vref_lprx), .cb_vref_mpll_reg(cb_vref_mpll_reg),
    // .cb_vrefcd_lprx(cb_vrefcd_lprx), .vbg_prog(vbg_prog),
    // .vref_1000(vref_1000), .vref_15(vref_bias), .vref_31(vref_31),
    // .vref_cal(vref_cal), .gd(gd), .vdd(vdd), .vph(vph), .vss(vss) );
    levelshifters.reset(new mipi_4_rx_dphy_ns_DPHYCBRX_lc("levelshifters"));
    levelshifters->atb_clk_ref(atb_comp_clk);
    levelshifters->atb_en(atb_on);
    levelshifters->atb_ext_con(atb_ext_con);
    levelshifters->atb_sel(cb_atb_sel);
    levelshifters->cb_burn_oscilator_p(cb_burn_oscilator_p);
    levelshifters->cb_cal_en(cb_cal_en);
    levelshifters->cb_cal_repl(cb_cal_repl);
    levelshifters->cb_en(cb_on);
    levelshifters->cb_en_45_ohm(cb_en_45_ohm);
    levelshifters->cb_ibias_en(cb_ibias_en);
    levelshifters->cb_ical_x4(cb_ical_x4);
    levelshifters->cb_lpibias_en(cb_lpibias_en);
    levelshifters->cb_rext_iocont_en(cb_rext_iocont_en);
    levelshifters->cb_sel_lptx(cb_sel_vreflptx_prog);
    levelshifters->cb_sel_vbg_prog(cb_sel_v400_prog);
    levelshifters->cb_sel_vref_lprx(cb_sel_vref_lprx);
    levelshifters->cb_sel_vrefcd_lprx(cb_sel_vrefcd_lprx);
    levelshifters->cb_vref_mpll_reg_sel(cb_vref_mpll_reg_sel);
    levelshifters->dac_value(dac_prog);
    levelshifters->en_rx(cb_lb_en_rext);
    levelshifters->gmode(gmode);
    levelshifters->lb_setrx(cb_lb_setrx);
    levelshifters->rx_pre_pon(cb_pre_iref_on);
    levelshifters->rxlp_bias_prog(rxlp_bias_prog);
    levelshifters->setrx(setrx);
    levelshifters->txlp_prog(txlp_prog);
    levelshifters->atb_clk_ref_hv(atb_clk_ref_hv);
    levelshifters->atb_clk_ref_hvz(atb_clk_ref_hvz);
    levelshifters->atb_en_hv(atb_en_hv);
    levelshifters->atb_en_hvz(atb_en_hvz);
    levelshifters->atb_ext_con_hv(atb_ext_con_hv);
    levelshifters->atb_ext_con_hvz(atb_ext_con_hvz);
    levelshifters->atb_sel_hv(atb_sel_hv);
    levelshifters->atb_sel_hvz(atb_sel_hvz);
    levelshifters->cb_burn_oscilator_int(cb_burn_oscilator_int);
    levelshifters->cb_cal_en_hv(cb_cal_en_hv);
    levelshifters->cb_cal_en_hvz(cb_cal_en_hvz);
    levelshifters->cb_cal_repl_hv(cb_cal_repl_hv);
    levelshifters->cb_cal_repl_hvz(cb_cal_repl_hvz);
    levelshifters->cb_det_supply(cb_det_supply);
    levelshifters->cb_en_45_ohm_hv(cb_en_45_ohm_hv);
    levelshifters->cb_en_hv(cb_on_hv);
    levelshifters->cb_en_hvz(cb_on_hvz);
    levelshifters->cb_en_int(cb_en_int);
    levelshifters->cb_en_int_z(cb_en_int_z);
    levelshifters->cb_ibias_en_hv(cb_ibias_en_hv);
    levelshifters->cb_ibias_en_hvz(cb_ibias_en_hvz);
    levelshifters->cb_ibias_en_int(cb_ibias_en_int);
    levelshifters->cb_ibias_en_int_z(cb_ibias_en_int_z);
    levelshifters->cb_ical_x4_hv(cb_ical_x4_hv);
    levelshifters->cb_ical_x4_hvz(cb_ical_x4_hvz);
    levelshifters->cb_lpibias_en_hv(cb_lpibias_en_hv);
    levelshifters->cb_lpibias_en_hvz(cb_lpibias_en_hvz);
    levelshifters->cb_rext_iocont_en_hv(cb_rext_iocont_en_hv);
    levelshifters->cb_rext_iocont_en_hvz(cb_rext_iocont_en_hvz);
    levelshifters->cb_sel_lptx_hv(cb_sel_lptx_hv);
    levelshifters->cb_sel_lptx_hvz(cb_sel_lptx_hvz);
    levelshifters->cb_sel_vbg_prog_hv(cb_sel_vbg_prog_hv);
    levelshifters->cb_sel_vbg_prog_hvz(cb_sel_vbg_prog_hvz);
    levelshifters->cb_sel_vref_lprx_hv(cb_sel_vref_lprx_hv);
    levelshifters->cb_sel_vref_lprx_hvz(cb_sel_vref_lprx_hvz);
    levelshifters->cb_sel_vrefcd_lprx_hv(cb_sel_vrefcd_lprx_hv);
    levelshifters->cb_sel_vrefcd_lprx_hvz(cb_sel_vrefcd_lprx_hvz);
    levelshifters->cb_vref_mpll_reg_sel_hv(cb_vref_mpll_reg_sel_hv);
    levelshifters->dac_value_hv(dac_value_hv);
    levelshifters->dac_value_hvz(dac_value_hvz);
    levelshifters->enz_rx_hv(enz_rx_hv);
    levelshifters->gmode_hv(gmode_hv);
    levelshifters->gmode_hvz(gmode_hvz);
    levelshifters->lb_setzrx_hv(lb_setzrx_hv);
    levelshifters->rx_pre_pon_hv(rx_pre_pon_hv);
    levelshifters->rx_pre_pon_hvz(rx_pre_pon_hvz);
    levelshifters->rxlp_bias_prog_hv(rxlp_bias_prog_hv);
    levelshifters->rxlp_bias_prog_hvz(rxlp_bias_prog_hvz);
    levelshifters->setrx_hv(setrx_hv);
    levelshifters->tie_high_hv(tie_high_hv);
    levelshifters->tie_low_hv(tie_low_hv);
    levelshifters->txlp_prog_hv(txlp_prog_hv);
    levelshifters->txlp_prog_hvz(txlp_prog_hvz);
    levelshifters->gd(gd);
    levelshifters->vdd(vdd);
    levelshifters->vp(vp);
    levelshifters->vph(vph);
    levelshifters->vss(vss);
    SC_METHOD(proc_initial_146331);
    SC_THREAD(proc_always_146336);
    set_stack_size(0x20000);
    SC_THREAD(proc_always_146350);
    set_stack_size(0x20000);
}

void mipi_4_rx_dphy_ns_DPHY3AFECBRX::merge_cb_10u_ibias_ddl() {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(cb_10u_ibias_ddl_b[0]));
    v.set_bit(1, vl::rd(cb_10u_ibias_ddl_b[1]));
    v.set_bit(2, vl::rd(cb_10u_ibias_ddl_b[2]));
    v.set_bit(3, vl::rd(cb_10u_ibias_ddl_b[3]));
    vl::wr(cb_10u_ibias_ddl, v);
}

void mipi_4_rx_dphy_ns_DPHY3AFECBRX::merge_cb_5u_ilprx() {
    vl::logic<10> v(0);
    v.set_bit(0, vl::rd(cb_5u_ilprx_b[0]));
    v.set_bit(1, vl::rd(cb_5u_ilprx_b[1]));
    v.set_bit(2, vl::rd(cb_5u_ilprx_b[2]));
    v.set_bit(3, vl::rd(cb_5u_ilprx_b[3]));
    v.set_bit(4, vl::rd(cb_5u_ilprx_b[4]));
    v.set_bit(5, vl::rd(cb_5u_ilprx_b[5]));
    v.set_bit(6, vl::rd(cb_5u_ilprx_b[6]));
    v.set_bit(7, vl::rd(cb_5u_ilprx_b[7]));
    v.set_bit(8, vl::rd(cb_5u_ilprx_b[8]));
    v.set_bit(9, vl::rd(cb_5u_ilprx_b[9]));
    vl::wr(cb_5u_ilprx, v);
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHY3AFECBRX::rd_cb_10u_ibias_ddl() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(cb_10u_ibias_ddl_b[0]));
    v.set_bit(1, vl::rd(cb_10u_ibias_ddl_b[1]));
    v.set_bit(2, vl::rd(cb_10u_ibias_ddl_b[2]));
    v.set_bit(3, vl::rd(cb_10u_ibias_ddl_b[3]));
    return v;
}

vl::logic<10> mipi_4_rx_dphy_ns_DPHY3AFECBRX::rd_cb_5u_ilprx() const {
    vl::logic<10> v(0);
    v.set_bit(0, vl::rd(cb_5u_ilprx_b[0]));
    v.set_bit(1, vl::rd(cb_5u_ilprx_b[1]));
    v.set_bit(2, vl::rd(cb_5u_ilprx_b[2]));
    v.set_bit(3, vl::rd(cb_5u_ilprx_b[3]));
    v.set_bit(4, vl::rd(cb_5u_ilprx_b[4]));
    v.set_bit(5, vl::rd(cb_5u_ilprx_b[5]));
    v.set_bit(6, vl::rd(cb_5u_ilprx_b[6]));
    v.set_bit(7, vl::rd(cb_5u_ilprx_b[7]));
    v.set_bit(8, vl::rd(cb_5u_ilprx_b[8]));
    v.set_bit(9, vl::rd(cb_5u_ilprx_b[9]));
    return v;
}

void mipi_4_rx_dphy_ns_DPHY3AFECBRX::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::land(vl::land(vl::land(vl::ceq(vl::rd(vph), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(vdd), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(vss), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHY3AFECBRX::assign_enable() {
    vl::wr(enable, vl::land(vl::land(vl::land(vl::ceq(vl::rd(cb_on), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(cb_bg_en), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(cb_sel_chop_clk), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(cb_chop_clk_en), vl::logic<1>(0x1ULL))));
}

void mipi_4_rx_dphy_ns_DPHY3AFECBRX::assign_cb_vref_lprx() {
    vl::wr(cb_vref_lprx, vl::mux(vl::ceq(vl::rd(cb_tx_ok), vl::logic<1>(0x1ULL)), vl::logic<1>(0x1ULL), vl::logic<1>(0x0ULL)));
}

void mipi_4_rx_dphy_ns_DPHY3AFECBRX::assign_cb_vrefcd_lprx() {
    vl::wr(cb_vrefcd_lprx, vl::mux(vl::ceq(vl::rd(cb_tx_ok), vl::logic<1>(0x1ULL)), vl::logic<1>(0x1ULL), vl::logic<1>(0x0ULL)));
}

void mipi_4_rx_dphy_ns_DPHY3AFECBRX::assign_cb_vreflptx() {
    vl::wr(cb_vreflptx, vl::mux(vl::ceq(vl::rd(cb_tx_lp_ok), vl::logic<1>(0x1ULL)), vl::logic<1>(0x1ULL), vl::logic<1>(0x0ULL)));
}

void mipi_4_rx_dphy_ns_DPHY3AFECBRX::assign_cb_5u_ilprx() {
    const vl::logic<10> v_ = vl::mux(vl::ceq(vl::rd(cb_tx_lp_ok), vl::logic<1>(0x1ULL)), vl::logic<10>(0x3ffULL), vl::logic<10>(0x0ULL));
    cb_5u_ilprx_b[0].write(v_.bit(0).sl());
    cb_5u_ilprx_b[1].write(v_.bit(1).sl());
    cb_5u_ilprx_b[2].write(v_.bit(2).sl());
    cb_5u_ilprx_b[3].write(v_.bit(3).sl());
    cb_5u_ilprx_b[4].write(v_.bit(4).sl());
    cb_5u_ilprx_b[5].write(v_.bit(5).sl());
    cb_5u_ilprx_b[6].write(v_.bit(6).sl());
    cb_5u_ilprx_b[7].write(v_.bit(7).sl());
    cb_5u_ilprx_b[8].write(v_.bit(8).sl());
    cb_5u_ilprx_b[9].write(v_.bit(9).sl());
}

void mipi_4_rx_dphy_ns_DPHY3AFECBRX::assign_cb_10u_ilptx() {
    vl::wr(cb_10u_ilptx, vl::mux(vl::ceq(vl::rd(cb_tx_lp_ok), vl::logic<1>(0x1ULL)), vl::logic<1>(0x1ULL), vl::logic<1>(0x0ULL)));
}

void mipi_4_rx_dphy_ns_DPHY3AFECBRX::assign_cb_10u_ibias_ddl() {
    const vl::logic<4> v_ = vl::mux(vl::land(vl::ceq(vl::rd(cb_tx_ok), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(cb_en_int), vl::logic<1>(0x1ULL))), vl::logic<4>(0xfULL), vl::logic<4>(0x0ULL));
    cb_10u_ibias_ddl_b[0].write(v_.bit(0).sl());
    cb_10u_ibias_ddl_b[1].write(v_.bit(1).sl());
    cb_10u_ibias_ddl_b[2].write(v_.bit(2).sl());
    cb_10u_ibias_ddl_b[3].write(v_.bit(3).sl());
}

void mipi_4_rx_dphy_ns_DPHY3AFECBRX::assign_cb_vref_mpll_reg_int() {
    vl::wr(cb_vref_mpll_reg_int, vl::mux(vl::land(vl::land(vl::ceq(vl::rd(cb_tx_lp_ok), vl::logic<1>(0x1ULL)), vl::cne(vl::rd(cb_vref_mpll_reg_sel_hv), vl::logic<3>::all_z())), vl::cne(vl::rd(cb_vref_mpll_reg_sel_hv), vl::logic<3>())), vl::logic<1>(0x1ULL), vl::logic<1>(0x0ULL)));
}

void mipi_4_rx_dphy_ns_DPHY3AFECBRX::assign_cb_vref_mpll_reg() {
    vl::wr(cb_vref_mpll_reg, vl::mux(vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)), vl::rd(cb_vref_mpll_reg_int), vl::logic<1>()));
}

void mipi_4_rx_dphy_ns_DPHY3AFECBRX::assign_vctrl_replica_int() {
    vl::wr(vctrl_replica_int, vl::mux(vl::ceq(vl::rd(cb_cal_en_hv), vl::logic<1>(0x1ULL)), vl::rd(cb_cal_repl_hv), vl::logic<4>(0x0ULL)));
}

void mipi_4_rx_dphy_ns_DPHY3AFECBRX::assign_vctrl_replica_int_calibration() {
    vl::wr(vctrl_replica_int_calibration, vl::cat(vl::logic<6>(0x0ULL), vl::rd(vctrl_replica_int)));
}

void mipi_4_rx_dphy_ns_DPHY3AFECBRX::assign_vref_bias() {
    vl::wr(vref_bias, vl::land(vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(enable), vl::logic<1>(0x1ULL))));
}

void mipi_4_rx_dphy_ns_DPHY3AFECBRX::split_bias_cb_10u_ibias_ddl() {
    const vl::logic<4> v = vl::rd(c_bias_cb_10u_ibias_ddl);
    cb_10u_ibias_ddl_b[0].write(v.bit(0).sl());
    cb_10u_ibias_ddl_b[1].write(v.bit(1).sl());
    cb_10u_ibias_ddl_b[2].write(v.bit(2).sl());
    cb_10u_ibias_ddl_b[3].write(v.bit(3).sl());
}

void mipi_4_rx_dphy_ns_DPHY3AFECBRX::split_bias_cb_5u_ilprx() {
    const vl::logic<10> v = vl::rd(c_bias_cb_5u_ilprx);
    cb_5u_ilprx_b[0].write(v.bit(0).sl());
    cb_5u_ilprx_b[1].write(v.bit(1).sl());
    cb_5u_ilprx_b[2].write(v.bit(2).sl());
    cb_5u_ilprx_b[3].write(v.bit(3).sl());
    cb_5u_ilprx_b[4].write(v.bit(4).sl());
    cb_5u_ilprx_b[5].write(v.bit(5).sl());
    cb_5u_ilprx_b[6].write(v.bit(6).sl());
    cb_5u_ilprx_b[7].write(v.bit(7).sl());
    cb_5u_ilprx_b[8].write(v.bit(8).sl());
    cb_5u_ilprx_b[9].write(v.bit(9).sl());
}

void mipi_4_rx_dphy_ns_DPHY3AFECBRX::proc_initial_146331() {
    vl::logic<1> cb_tx_lp_ok_sh = vl::rd(cb_tx_lp_ok);
    vl::logic<1> cb_tx_ok_sh = vl::rd(cb_tx_ok);
    cb_tx_ok_sh = vl::logic<1>(0x0ULL);
    vl::wr(cb_tx_ok, cb_tx_ok_sh);
    cb_tx_lp_ok_sh = vl::logic<1>(0x0ULL);
    vl::wr(cb_tx_lp_ok, cb_tx_lp_ok_sh);
}

void mipi_4_rx_dphy_ns_DPHY3AFECBRX::proc_always_146336() {
    for (;;) {
        wait(supply_ok.value_changed_event() | enable.value_changed_event());
        if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)).test()) {
            if (vl::ceq(vl::rd(enable), vl::logic<1>(0x1ULL)).test()) {
                wait(vl::dly(4000.0, TU_, TP_));
                vl::wr(cb_tx_ok, vl::logic<1>(0x1ULL));
            } else {
                vl::wr(cb_tx_ok, vl::logic<1>(0x0ULL));
            }
        } else {
            vl::wr(cb_tx_ok, vl::logic<1>(0x0ULL));
        }
    }
}

void mipi_4_rx_dphy_ns_DPHY3AFECBRX::proc_always_146350() {
    for (;;) {
        wait(supply_ok.value_changed_event() | enable.value_changed_event() | cb_lpibias_en_hv.value_changed_event());
        if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)).test()) {
            if (vl::land(vl::ceq(vl::rd(enable), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(cb_lpibias_en_hv), vl::logic<1>(0x1ULL))).test()) {
                wait(vl::dly(10000.0, TU_, TP_));
                vl::wr(cb_tx_lp_ok, vl::logic<1>(0x1ULL));
            } else {
                vl::wr(cb_tx_lp_ok, vl::logic<1>(0x0ULL));
            }
        } else {
            vl::wr(cb_tx_lp_ok, vl::logic<1>(0x0ULL));
        }
    }
}
