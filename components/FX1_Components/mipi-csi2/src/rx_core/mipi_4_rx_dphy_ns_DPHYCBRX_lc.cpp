//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYCBRX_lc
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYCBRX_lc.h"

mipi_4_rx_dphy_ns_DPHYCBRX_lc::mipi_4_rx_dphy_ns_DPHYCBRX_lc(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_atb_sel);
    sensitive << atb_sel;
    SC_METHOD(split_cb_cal_repl);
    sensitive << cb_cal_repl;
    SC_METHOD(split_cb_sel_vbg_prog);
    sensitive << cb_sel_vbg_prog;
    SC_METHOD(split_cb_sel_vref_lprx);
    sensitive << cb_sel_vref_lprx;
    SC_METHOD(split_cb_sel_vrefcd_lprx);
    sensitive << cb_sel_vrefcd_lprx;
    SC_METHOD(split_cb_vref_mpll_reg_sel);
    sensitive << cb_vref_mpll_reg_sel;
    SC_METHOD(split_dac_value);
    sensitive << dac_value;
    SC_METHOD(split_gmode);
    sensitive << gmode;
    SC_METHOD(split_lb_setrx);
    sensitive << lb_setrx;
    SC_METHOD(split_setrx);
    sensitive << setrx;
    SC_METHOD(split_txlp_prog);
    sensitive << txlp_prog;
    SC_METHOD(merge_atb_sel_hv);
    sensitive << atb_sel_hv_b[0] << atb_sel_hv_b[1];
    SC_METHOD(merge_atb_sel_hvz);
    sensitive << atb_sel_hvz_b[0] << atb_sel_hvz_b[1];
    SC_METHOD(merge_cb_cal_repl_hv);
    sensitive << cb_cal_repl_hv_b[0] << cb_cal_repl_hv_b[1] << cb_cal_repl_hv_b[2] << cb_cal_repl_hv_b[3];
    SC_METHOD(merge_cb_cal_repl_hvz);
    sensitive << cb_cal_repl_hvz_b[0] << cb_cal_repl_hvz_b[1] << cb_cal_repl_hvz_b[2] << cb_cal_repl_hvz_b[3];
    SC_METHOD(merge_cb_sel_vbg_prog_hv);
    sensitive << cb_sel_vbg_prog_hv_b[0] << cb_sel_vbg_prog_hv_b[1] << cb_sel_vbg_prog_hv_b[2];
    SC_METHOD(merge_cb_sel_vbg_prog_hvz);
    sensitive << cb_sel_vbg_prog_hvz_b[0] << cb_sel_vbg_prog_hvz_b[1] << cb_sel_vbg_prog_hvz_b[2];
    SC_METHOD(merge_cb_sel_vref_lprx_hv);
    sensitive << cb_sel_vref_lprx_hv_b[0] << cb_sel_vref_lprx_hv_b[1];
    SC_METHOD(merge_cb_sel_vref_lprx_hvz);
    sensitive << cb_sel_vref_lprx_hvz_b[0] << cb_sel_vref_lprx_hvz_b[1];
    SC_METHOD(merge_cb_sel_vrefcd_lprx_hv);
    sensitive << cb_sel_vrefcd_lprx_hv_b[0] << cb_sel_vrefcd_lprx_hv_b[1];
    SC_METHOD(merge_cb_sel_vrefcd_lprx_hvz);
    sensitive << cb_sel_vrefcd_lprx_hvz_b[0] << cb_sel_vrefcd_lprx_hvz_b[1];
    SC_METHOD(merge_cb_vref_mpll_reg_sel_hv);
    sensitive << cb_vref_mpll_reg_sel_hv_b[0] << cb_vref_mpll_reg_sel_hv_b[1] << cb_vref_mpll_reg_sel_hv_b[2];
    SC_METHOD(merge_dac_value_hv);
    sensitive << dac_value_hv_b[0] << dac_value_hv_b[1] << dac_value_hv_b[2] << dac_value_hv_b[3] << dac_value_hv_b[4] << dac_value_hv_b[5] << dac_value_hv_b[6] << dac_value_hv_b[7] << dac_value_hv_b[8] << dac_value_hv_b[9];
    SC_METHOD(merge_dac_value_hvz);
    sensitive << dac_value_hvz_b[0] << dac_value_hvz_b[1] << dac_value_hvz_b[2] << dac_value_hvz_b[3] << dac_value_hvz_b[4] << dac_value_hvz_b[5] << dac_value_hvz_b[6] << dac_value_hvz_b[7] << dac_value_hvz_b[8] << dac_value_hvz_b[9];
    SC_METHOD(merge_gmode_hv);
    sensitive << gmode_hv_b[0] << gmode_hv_b[1];
    SC_METHOD(merge_gmode_hvz);
    sensitive << gmode_hvz_b[0] << gmode_hvz_b[1];
    SC_METHOD(merge_lb_setzrx_hv);
    sensitive << lb_setzrx_hv_b[0] << lb_setzrx_hv_b[1] << lb_setzrx_hv_b[2];
    SC_METHOD(merge_setrx_hv);
    sensitive << setrx_hv_b[0] << setrx_hv_b[1] << setrx_hv_b[2];
    SC_METHOD(merge_txlp_prog_hv);
    sensitive << txlp_prog_hv_b[0] << txlp_prog_hv_b[1] << txlp_prog_hv_b[2];
    SC_METHOD(merge_txlp_prog_hvz);
    sensitive << txlp_prog_hvz_b[0] << txlp_prog_hvz_b[1] << txlp_prog_hvz_b[2];
    SC_METHOD(gate_N192);
    sensitive << tie_low_cdm;
    sc_lvt_I184.reset(new mipi_4_rx_dphy_ns_sc_na2x12_lvt("sc_lvt_I184"));
    sc_lvt_I184->a(atb_clk_ref);
    sc_lvt_I184->b(atb_en);
    sc_lvt_I184->z(atb_clk_ref_z);
    sc_lvt_I184->gd(vss);
    sc_lvt_I184->vp(vdd);
    I253.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2l_cdm_tie_LowHigh("I253"));
    I253->gd(vss);
    I253->vp(vdd);
    I253->tie_high(tie_high_cdm_digital);
    I253->tie_low(tie_low_cdm_digital);
    tie_cdm.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2l_cdm_tie_LowHigh("tie_cdm"));
    tie_cdm->gd(gd);
    tie_cdm->vp(vp);
    tie_cdm->tie_high(tie_high_cdm);
    tie_cdm->tie_low(tie_low_cdm);
    sc_lvs_I155.reset(new mipi_4_rx_dphy_ns_sc_invx16_lvt("sc_lvs_I155"));
    sc_lvs_I155->a(cb_en_int_z);
    sc_lvs_I155->z(cb_en_int);
    sc_lvs_I155->gd(gd);
    sc_lvs_I155->vp(vp);
    sc_lvt_I236.reset(new mipi_4_rx_dphy_ns_sc_invx16_lvt("sc_lvt_I236"));
    sc_lvt_I236->a(cb_ibias_en_int_z);
    sc_lvt_I236->z(cb_ibias_en_int);
    sc_lvt_I236->gd(gd);
    sc_lvt_I236->vp(vp);
    sc_lvt_I254.reset(new mipi_4_rx_dphy_ns_sc_invx6_lvt("sc_lvt_I254"));
    sc_lvt_I254->a(cb_det_envph_lv);
    sc_lvt_I254->z(cb_det_envph_lv_vp);
    sc_lvt_I254->gd(gd);
    sc_lvt_I254->vp(vp);
    sc_lvt_I256.reset(new mipi_4_rx_dphy_ns_sc_invx6_lvt("sc_lvt_I256"));
    sc_lvt_I256->a(net370);
    sc_lvt_I256->z(cb_det_supply);
    sc_lvt_I256->gd(vss);
    sc_lvt_I256->vp(vdd);
    I113.reset(new mipi_4_rx_dphy_ns_LEVELCONV_bias("I113"));
    I113->onz_hv(cb_en_hvz);
    I113->tie_low_hv(tie_low_hv);
    I113->vblvhv(vblvhv_cb_en);
    I113->gd(gd);
    I113->vph(vph);
    I191.reset(new mipi_4_rx_dphy_ns_LEVELCONV_bias("I191"));
    I191->onz_hv(atb_en_hvz);
    I191->tie_low_hv(tie_low_hv);
    I191->vblvhv(vblvhv_atb_en);
    I191->gd(gd);
    I191->vph(vph);
    I207.reset(new mipi_4_rx_dphy_ns_tieLowHigh_HV("I207"));
    I207->high(tie_high_hv);
    I207->low(tie_low_hv);
    I207->gd(gd);
    I207->vph(vph);
    I78.reset(new mipi_4_rx_dphy_ns_LEVELCONV_vp_det_soc("I78"));
    I78->A_in(atb_en);
    I78->Z(atb_en_hv);
    I78->Z_neg(atb_en_hvz);
    I78->gd(gd);
    I78->vph(vph);
    I78->vss(vss);
    I188.reset(new mipi_4_rx_dphy_ns_LEVELCONV_vp_det_soc("I188"));
    I188->A_in(cb_en);
    I188->Z(cb_en_hv);
    I188->Z_neg(cb_en_hvz);
    I188->gd(gd);
    I188->vph(vph);
    I188->vss(vss);
    I214.reset(new mipi_4_rx_dphy_ns_sc_no2x6_lvt("I214"));
    I214->a(tie_low_cdm);
    I214->b(cb_det_envph_lv_vp);
    I214->z(cb_det_supply_z1);
    I214->gd(gd);
    I214->vp(vp);
    sc_lvt_I250.reset(new mipi_4_rx_dphy_ns_sc_no2x6_lvt("sc_lvt_I250"));
    sc_lvt_I250->a(tie_low_cdm);
    sc_lvt_I250->b(cb_burn_oscilator_p);
    sc_lvt_I250->z(cb_burn_oscilator_int_z);
    sc_lvt_I250->gd(gd);
    sc_lvt_I250->vp(vp);
    I65_3.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I65_3"));
    I65_3->A_in(cb_cal_repl_b[3]);
    I65_3->onz_hv(cb_en_hvz);
    I65_3->tie_low_hv(tie_low_hv);
    I65_3->vblvhv(vblvhv_cb_en);
    I65_3->Z(cb_cal_repl_hv_b[3]);
    I65_3->Z_neg(cb_cal_repl_hvz_b[3]);
    I65_3->gd(gd);
    I65_3->vdd(vdd);
    I65_3->vph(vph);
    I65_3->vss(vss);
    I65_2.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I65_2"));
    I65_2->A_in(cb_cal_repl_b[2]);
    I65_2->onz_hv(cb_en_hvz);
    I65_2->tie_low_hv(tie_low_hv);
    I65_2->vblvhv(vblvhv_cb_en);
    I65_2->Z(cb_cal_repl_hv_b[2]);
    I65_2->Z_neg(cb_cal_repl_hvz_b[2]);
    I65_2->gd(gd);
    I65_2->vdd(vdd);
    I65_2->vph(vph);
    I65_2->vss(vss);
    I65_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I65_1"));
    I65_1->A_in(cb_cal_repl_b[1]);
    I65_1->onz_hv(cb_en_hvz);
    I65_1->tie_low_hv(tie_low_hv);
    I65_1->vblvhv(vblvhv_cb_en);
    I65_1->Z(cb_cal_repl_hv_b[1]);
    I65_1->Z_neg(cb_cal_repl_hvz_b[1]);
    I65_1->gd(gd);
    I65_1->vdd(vdd);
    I65_1->vph(vph);
    I65_1->vss(vss);
    I65_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I65_0"));
    I65_0->A_in(cb_cal_repl_b[0]);
    I65_0->onz_hv(cb_en_hvz);
    I65_0->tie_low_hv(tie_low_hv);
    I65_0->vblvhv(vblvhv_cb_en);
    I65_0->Z(cb_cal_repl_hv_b[0]);
    I65_0->Z_neg(cb_cal_repl_hvz_b[0]);
    I65_0->gd(gd);
    I65_0->vdd(vdd);
    I65_0->vph(vph);
    I65_0->vss(vss);
    I79_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I79_1"));
    I79_1->A_in(gmode_b[1]);
    I79_1->onz_hv(cb_en_hvz);
    I79_1->tie_low_hv(tie_low_hv);
    I79_1->vblvhv(vblvhv_cb_en);
    I79_1->Z(gmode_hv_b[1]);
    I79_1->Z_neg(gmode_hvz_b[1]);
    I79_1->gd(gd);
    I79_1->vdd(vdd);
    I79_1->vph(vph);
    I79_1->vss(vss);
    I79_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I79_0"));
    I79_0->A_in(gmode_b[0]);
    I79_0->onz_hv(cb_en_hvz);
    I79_0->tie_low_hv(tie_low_hv);
    I79_0->vblvhv(vblvhv_cb_en);
    I79_0->Z(gmode_hv_b[0]);
    I79_0->Z_neg(gmode_hvz_b[0]);
    I79_0->gd(gd);
    I79_0->vdd(vdd);
    I79_0->vph(vph);
    I79_0->vss(vss);
    I82_3.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I82_3"));
    I82_3->A_in(en_rx);
    I82_3->onz_hv(cb_en_hvz);
    I82_3->tie_low_hv(tie_low_hv);
    I82_3->vblvhv(vblvhv_cb_en);
    I82_3->Z(en_rx_hv);
    I82_3->Z_neg(enz_rx_hv);
    I82_3->gd(gd);
    I82_3->vdd(vdd);
    I82_3->vph(vph);
    I82_3->vss(vss);
    I82_2.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I82_2"));
    I82_2->A_in(lb_setrx_b[2]);
    I82_2->onz_hv(cb_en_hvz);
    I82_2->tie_low_hv(tie_low_hv);
    I82_2->vblvhv(vblvhv_cb_en);
    I82_2->Z(lb_setrx_hv[2]);
    I82_2->Z_neg(lb_setzrx_hv_b[2]);
    I82_2->gd(gd);
    I82_2->vdd(vdd);
    I82_2->vph(vph);
    I82_2->vss(vss);
    I82_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I82_1"));
    I82_1->A_in(lb_setrx_b[1]);
    I82_1->onz_hv(cb_en_hvz);
    I82_1->tie_low_hv(tie_low_hv);
    I82_1->vblvhv(vblvhv_cb_en);
    I82_1->Z(lb_setrx_hv[1]);
    I82_1->Z_neg(lb_setzrx_hv_b[1]);
    I82_1->gd(gd);
    I82_1->vdd(vdd);
    I82_1->vph(vph);
    I82_1->vss(vss);
    I82_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I82_0"));
    I82_0->A_in(lb_setrx_b[0]);
    I82_0->onz_hv(cb_en_hvz);
    I82_0->tie_low_hv(tie_low_hv);
    I82_0->vblvhv(vblvhv_cb_en);
    I82_0->Z(lb_setrx_hv[0]);
    I82_0->Z_neg(lb_setzrx_hv_b[0]);
    I82_0->gd(gd);
    I82_0->vdd(vdd);
    I82_0->vph(vph);
    I82_0->vss(vss);
    I111.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I111"));
    I111->A_in(rx_pre_pon);
    I111->onz_hv(cb_en_hvz);
    I111->tie_low_hv(tie_low_hv);
    I111->vblvhv(vblvhv_cb_en);
    I111->Z(rx_pre_pon_hv);
    I111->Z_neg(rx_pre_pon_hvz);
    I111->gd(gd);
    I111->vdd(vdd);
    I111->vph(vph);
    I111->vss(vss);
    I126.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I126"));
    I126->A_in(cb_lpibias_en);
    I126->onz_hv(cb_en_hvz);
    I126->tie_low_hv(tie_low_hv);
    I126->vblvhv(vblvhv_cb_en);
    I126->Z(cb_lpibias_en_hv);
    I126->Z_neg(cb_lpibias_en_hvz);
    I126->gd(gd);
    I126->vdd(vdd);
    I126->vph(vph);
    I126->vss(vss);
    I140.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I140"));
    I140->A_in(cb_ical_x4);
    I140->onz_hv(cb_en_hvz);
    I140->tie_low_hv(tie_low_hv);
    I140->vblvhv(vblvhv_cb_en);
    I140->Z(cb_ical_x4_hv);
    I140->Z_neg(cb_ical_x4_hvz);
    I140->gd(gd);
    I140->vdd(vdd);
    I140->vph(vph);
    I140->vss(vss);
    I167.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I167"));
    I167->A_in(rxlp_bias_prog);
    I167->onz_hv(cb_en_hvz);
    I167->tie_low_hv(tie_low_hv);
    I167->vblvhv(vblvhv_cb_en);
    I167->Z(rxlp_bias_prog_hv);
    I167->Z_neg(rxlp_bias_prog_hvz);
    I167->gd(gd);
    I167->vdd(vdd);
    I167->vph(vph);
    I167->vss(vss);
    I170_2.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I170_2"));
    I170_2->A_in(txlp_prog_b[2]);
    I170_2->onz_hv(cb_en_hvz);
    I170_2->tie_low_hv(tie_low_hv);
    I170_2->vblvhv(vblvhv_cb_en);
    I170_2->Z(txlp_prog_hv_b[2]);
    I170_2->Z_neg(txlp_prog_hvz_b[2]);
    I170_2->gd(gd);
    I170_2->vdd(vdd);
    I170_2->vph(vph);
    I170_2->vss(vss);
    I170_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I170_1"));
    I170_1->A_in(txlp_prog_b[1]);
    I170_1->onz_hv(cb_en_hvz);
    I170_1->tie_low_hv(tie_low_hv);
    I170_1->vblvhv(vblvhv_cb_en);
    I170_1->Z(txlp_prog_hv_b[1]);
    I170_1->Z_neg(txlp_prog_hvz_b[1]);
    I170_1->gd(gd);
    I170_1->vdd(vdd);
    I170_1->vph(vph);
    I170_1->vss(vss);
    I170_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I170_0"));
    I170_0->A_in(txlp_prog_b[0]);
    I170_0->onz_hv(cb_en_hvz);
    I170_0->tie_low_hv(tie_low_hv);
    I170_0->vblvhv(vblvhv_cb_en);
    I170_0->Z(txlp_prog_hv_b[0]);
    I170_0->Z_neg(txlp_prog_hvz_b[0]);
    I170_0->gd(gd);
    I170_0->vdd(vdd);
    I170_0->vph(vph);
    I170_0->vss(vss);
    I172.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I172"));
    I172->A_in(atb_ext_con);
    I172->onz_hv(atb_en_hvz);
    I172->tie_low_hv(tie_low_hv);
    I172->vblvhv(vblvhv_atb_en);
    I172->Z(atb_ext_con_hv);
    I172->Z_neg(atb_ext_con_hvz);
    I172->gd(gd);
    I172->vdd(vdd);
    I172->vph(vph);
    I172->vss(vss);
    I175.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I175"));
    I175->A_in(cb_en_45_ohm);
    I175->onz_hv(cb_en_hvz);
    I175->tie_low_hv(tie_low_hv);
    I175->vblvhv(vblvhv_cb_en);
    I175->Z(cb_en_45_ohm_hv);
    I175->Z_neg(cb_en_45_ohm_hvz);
    I175->gd(gd);
    I175->vdd(vdd);
    I175->vph(vph);
    I175->vss(vss);
    I185.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I185"));
    I185->A_in(cb_cal_en);
    I185->onz_hv(cb_en_hvz);
    I185->tie_low_hv(tie_low_hv);
    I185->vblvhv(vblvhv_cb_en);
    I185->Z(cb_cal_en_hv);
    I185->Z_neg(cb_cal_en_hvz);
    I185->gd(gd);
    I185->vdd(vdd);
    I185->vph(vph);
    I185->vss(vss);
    I200.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I200"));
    I200->A_in(cb_sel_lptx);
    I200->onz_hv(cb_en_hvz);
    I200->tie_low_hv(tie_low_hv);
    I200->vblvhv(vblvhv_cb_en);
    I200->Z(cb_sel_lptx_hv);
    I200->Z_neg(cb_sel_lptx_hvz);
    I200->gd(gd);
    I200->vdd(vdd);
    I200->vph(vph);
    I200->vss(vss);
    I201.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I201"));
    I201->A_in(cb_ibias_en);
    I201->onz_hv(cb_en_hvz);
    I201->tie_low_hv(tie_low_hv);
    I201->vblvhv(vblvhv_cb_en);
    I201->Z(cb_ibias_en_hv);
    I201->Z_neg(cb_ibias_en_hvz);
    I201->gd(gd);
    I201->vdd(vdd);
    I201->vph(vph);
    I201->vss(vss);
    I209.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I209"));
    I209->A_in(cb_rext_iocont_en);
    I209->onz_hv(cb_en_hvz);
    I209->tie_low_hv(tie_low_hv);
    I209->vblvhv(vblvhv_cb_en);
    I209->Z(cb_rext_iocont_en_hv);
    I209->Z_neg(cb_rext_iocont_en_hvz);
    I209->gd(gd);
    I209->vdd(vdd);
    I209->vph(vph);
    I209->vss(vss);
    I213_2.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I213_2"));
    I213_2->A_in(cb_vref_mpll_reg_sel_b[2]);
    I213_2->onz_hv(cb_en_hvz);
    I213_2->tie_low_hv(tie_low_hv);
    I213_2->vblvhv(vblvhv_cb_en);
    I213_2->Z(cb_vref_mpll_reg_sel_hv_b[2]);
    I213_2->Z_neg(cb_vref_mpll_reg_sel_hvz[2]);
    I213_2->gd(gd);
    I213_2->vdd(vdd);
    I213_2->vph(vph);
    I213_2->vss(vss);
    I213_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I213_1"));
    I213_1->A_in(cb_vref_mpll_reg_sel_b[1]);
    I213_1->onz_hv(cb_en_hvz);
    I213_1->tie_low_hv(tie_low_hv);
    I213_1->vblvhv(vblvhv_cb_en);
    I213_1->Z(cb_vref_mpll_reg_sel_hv_b[1]);
    I213_1->Z_neg(cb_vref_mpll_reg_sel_hvz[1]);
    I213_1->gd(gd);
    I213_1->vdd(vdd);
    I213_1->vph(vph);
    I213_1->vss(vss);
    I213_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I213_0"));
    I213_0->A_in(cb_vref_mpll_reg_sel_b[0]);
    I213_0->onz_hv(cb_en_hvz);
    I213_0->tie_low_hv(tie_low_hv);
    I213_0->vblvhv(vblvhv_cb_en);
    I213_0->Z(cb_vref_mpll_reg_sel_hv_b[0]);
    I213_0->Z_neg(cb_vref_mpll_reg_sel_hvz[0]);
    I213_0->gd(gd);
    I213_0->vdd(vdd);
    I213_0->vph(vph);
    I213_0->vss(vss);
    I217_2.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I217_2"));
    I217_2->A_in(setrx_b[2]);
    I217_2->onz_hv(cb_en_hvz);
    I217_2->tie_low_hv(tie_low_hv);
    I217_2->vblvhv(vblvhv_cb_en);
    I217_2->Z(setrx_hv_b[2]);
    I217_2->Z_neg(setrx_hvz[2]);
    I217_2->gd(gd);
    I217_2->vdd(vdd);
    I217_2->vph(vph);
    I217_2->vss(vss);
    I217_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I217_1"));
    I217_1->A_in(setrx_b[1]);
    I217_1->onz_hv(cb_en_hvz);
    I217_1->tie_low_hv(tie_low_hv);
    I217_1->vblvhv(vblvhv_cb_en);
    I217_1->Z(setrx_hv_b[1]);
    I217_1->Z_neg(setrx_hvz[1]);
    I217_1->gd(gd);
    I217_1->vdd(vdd);
    I217_1->vph(vph);
    I217_1->vss(vss);
    I217_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I217_0"));
    I217_0->A_in(setrx_b[0]);
    I217_0->onz_hv(cb_en_hvz);
    I217_0->tie_low_hv(tie_low_hv);
    I217_0->vblvhv(vblvhv_cb_en);
    I217_0->Z(setrx_hv_b[0]);
    I217_0->Z_neg(setrx_hvz[0]);
    I217_0->gd(gd);
    I217_0->vdd(vdd);
    I217_0->vph(vph);
    I217_0->vss(vss);
    I220_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I220_1"));
    I220_1->A_in(cb_sel_vrefcd_lprx_b[1]);
    I220_1->onz_hv(cb_en_hvz);
    I220_1->tie_low_hv(tie_low_hv);
    I220_1->vblvhv(vblvhv_cb_en);
    I220_1->Z(cb_sel_vrefcd_lprx_hv_b[1]);
    I220_1->Z_neg(cb_sel_vrefcd_lprx_hvz_b[1]);
    I220_1->gd(gd);
    I220_1->vdd(vdd);
    I220_1->vph(vph);
    I220_1->vss(vss);
    I220_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I220_0"));
    I220_0->A_in(cb_sel_vrefcd_lprx_b[0]);
    I220_0->onz_hv(cb_en_hvz);
    I220_0->tie_low_hv(tie_low_hv);
    I220_0->vblvhv(vblvhv_cb_en);
    I220_0->Z(cb_sel_vrefcd_lprx_hv_b[0]);
    I220_0->Z_neg(cb_sel_vrefcd_lprx_hvz_b[0]);
    I220_0->gd(gd);
    I220_0->vdd(vdd);
    I220_0->vph(vph);
    I220_0->vss(vss);
    I221_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I221_1"));
    I221_1->A_in(cb_sel_vref_lprx_b[1]);
    I221_1->onz_hv(cb_en_hvz);
    I221_1->tie_low_hv(tie_low_hv);
    I221_1->vblvhv(vblvhv_cb_en);
    I221_1->Z(cb_sel_vref_lprx_hv_b[1]);
    I221_1->Z_neg(cb_sel_vref_lprx_hvz_b[1]);
    I221_1->gd(gd);
    I221_1->vdd(vdd);
    I221_1->vph(vph);
    I221_1->vss(vss);
    I221_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I221_0"));
    I221_0->A_in(cb_sel_vref_lprx_b[0]);
    I221_0->onz_hv(cb_en_hvz);
    I221_0->tie_low_hv(tie_low_hv);
    I221_0->vblvhv(vblvhv_cb_en);
    I221_0->Z(cb_sel_vref_lprx_hv_b[0]);
    I221_0->Z_neg(cb_sel_vref_lprx_hvz_b[0]);
    I221_0->gd(gd);
    I221_0->vdd(vdd);
    I221_0->vph(vph);
    I221_0->vss(vss);
    I223_2.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I223_2"));
    I223_2->A_in(cb_sel_vbg_prog_b[2]);
    I223_2->onz_hv(cb_en_hvz);
    I223_2->tie_low_hv(tie_low_hv);
    I223_2->vblvhv(vblvhv_cb_en);
    I223_2->Z(cb_sel_vbg_prog_hv_b[2]);
    I223_2->Z_neg(cb_sel_vbg_prog_hvz_b[2]);
    I223_2->gd(gd);
    I223_2->vdd(vdd);
    I223_2->vph(vph);
    I223_2->vss(vss);
    I223_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I223_1"));
    I223_1->A_in(cb_sel_vbg_prog_b[1]);
    I223_1->onz_hv(cb_en_hvz);
    I223_1->tie_low_hv(tie_low_hv);
    I223_1->vblvhv(vblvhv_cb_en);
    I223_1->Z(cb_sel_vbg_prog_hv_b[1]);
    I223_1->Z_neg(cb_sel_vbg_prog_hvz_b[1]);
    I223_1->gd(gd);
    I223_1->vdd(vdd);
    I223_1->vph(vph);
    I223_1->vss(vss);
    I223_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I223_0"));
    I223_0->A_in(cb_sel_vbg_prog_b[0]);
    I223_0->onz_hv(cb_en_hvz);
    I223_0->tie_low_hv(tie_low_hv);
    I223_0->vblvhv(vblvhv_cb_en);
    I223_0->Z(cb_sel_vbg_prog_hv_b[0]);
    I223_0->Z_neg(cb_sel_vbg_prog_hvz_b[0]);
    I223_0->gd(gd);
    I223_0->vdd(vdd);
    I223_0->vph(vph);
    I223_0->vss(vss);
    I226_9.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I226_9"));
    I226_9->A_in(dac_value_b[9]);
    I226_9->onz_hv(atb_en_hvz);
    I226_9->tie_low_hv(tie_low_hv);
    I226_9->vblvhv(vblvhv_atb_en);
    I226_9->Z(dac_value_hv_b[9]);
    I226_9->Z_neg(dac_value_hvz_b[9]);
    I226_9->gd(gd);
    I226_9->vdd(vdd);
    I226_9->vph(vph);
    I226_9->vss(vss);
    I226_8.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I226_8"));
    I226_8->A_in(dac_value_b[8]);
    I226_8->onz_hv(atb_en_hvz);
    I226_8->tie_low_hv(tie_low_hv);
    I226_8->vblvhv(vblvhv_atb_en);
    I226_8->Z(dac_value_hv_b[8]);
    I226_8->Z_neg(dac_value_hvz_b[8]);
    I226_8->gd(gd);
    I226_8->vdd(vdd);
    I226_8->vph(vph);
    I226_8->vss(vss);
    I226_7.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I226_7"));
    I226_7->A_in(dac_value_b[7]);
    I226_7->onz_hv(atb_en_hvz);
    I226_7->tie_low_hv(tie_low_hv);
    I226_7->vblvhv(vblvhv_atb_en);
    I226_7->Z(dac_value_hv_b[7]);
    I226_7->Z_neg(dac_value_hvz_b[7]);
    I226_7->gd(gd);
    I226_7->vdd(vdd);
    I226_7->vph(vph);
    I226_7->vss(vss);
    I226_6.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I226_6"));
    I226_6->A_in(dac_value_b[6]);
    I226_6->onz_hv(atb_en_hvz);
    I226_6->tie_low_hv(tie_low_hv);
    I226_6->vblvhv(vblvhv_atb_en);
    I226_6->Z(dac_value_hv_b[6]);
    I226_6->Z_neg(dac_value_hvz_b[6]);
    I226_6->gd(gd);
    I226_6->vdd(vdd);
    I226_6->vph(vph);
    I226_6->vss(vss);
    I226_5.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I226_5"));
    I226_5->A_in(dac_value_b[5]);
    I226_5->onz_hv(atb_en_hvz);
    I226_5->tie_low_hv(tie_low_hv);
    I226_5->vblvhv(vblvhv_atb_en);
    I226_5->Z(dac_value_hv_b[5]);
    I226_5->Z_neg(dac_value_hvz_b[5]);
    I226_5->gd(gd);
    I226_5->vdd(vdd);
    I226_5->vph(vph);
    I226_5->vss(vss);
    I226_4.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I226_4"));
    I226_4->A_in(dac_value_b[4]);
    I226_4->onz_hv(atb_en_hvz);
    I226_4->tie_low_hv(tie_low_hv);
    I226_4->vblvhv(vblvhv_atb_en);
    I226_4->Z(dac_value_hv_b[4]);
    I226_4->Z_neg(dac_value_hvz_b[4]);
    I226_4->gd(gd);
    I226_4->vdd(vdd);
    I226_4->vph(vph);
    I226_4->vss(vss);
    I226_3.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I226_3"));
    I226_3->A_in(dac_value_b[3]);
    I226_3->onz_hv(atb_en_hvz);
    I226_3->tie_low_hv(tie_low_hv);
    I226_3->vblvhv(vblvhv_atb_en);
    I226_3->Z(dac_value_hv_b[3]);
    I226_3->Z_neg(dac_value_hvz_b[3]);
    I226_3->gd(gd);
    I226_3->vdd(vdd);
    I226_3->vph(vph);
    I226_3->vss(vss);
    I226_2.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I226_2"));
    I226_2->A_in(dac_value_b[2]);
    I226_2->onz_hv(atb_en_hvz);
    I226_2->tie_low_hv(tie_low_hv);
    I226_2->vblvhv(vblvhv_atb_en);
    I226_2->Z(dac_value_hv_b[2]);
    I226_2->Z_neg(dac_value_hvz_b[2]);
    I226_2->gd(gd);
    I226_2->vdd(vdd);
    I226_2->vph(vph);
    I226_2->vss(vss);
    I226_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I226_1"));
    I226_1->A_in(dac_value_b[1]);
    I226_1->onz_hv(atb_en_hvz);
    I226_1->tie_low_hv(tie_low_hv);
    I226_1->vblvhv(vblvhv_atb_en);
    I226_1->Z(dac_value_hv_b[1]);
    I226_1->Z_neg(dac_value_hvz_b[1]);
    I226_1->gd(gd);
    I226_1->vdd(vdd);
    I226_1->vph(vph);
    I226_1->vss(vss);
    I226_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I226_0"));
    I226_0->A_in(dac_value_b[0]);
    I226_0->onz_hv(atb_en_hvz);
    I226_0->tie_low_hv(tie_low_hv);
    I226_0->vblvhv(vblvhv_atb_en);
    I226_0->Z(dac_value_hv_b[0]);
    I226_0->Z_neg(dac_value_hvz_b[0]);
    I226_0->gd(gd);
    I226_0->vdd(vdd);
    I226_0->vph(vph);
    I226_0->vss(vss);
    I229.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I229"));
    I229->A_in(atb_clk_ref_z);
    I229->onz_hv(atb_en_hvz);
    I229->tie_low_hv(tie_low_hv);
    I229->vblvhv(vblvhv_atb_en);
    I229->Z(atb_clk_ref_hvz);
    I229->Z_neg(atb_clk_ref_hv);
    I229->gd(gd);
    I229->vdd(vdd);
    I229->vph(vph);
    I229->vss(vss);
    I231_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I231_1"));
    I231_1->A_in(atb_sel_b[1]);
    I231_1->onz_hv(atb_en_hvz);
    I231_1->tie_low_hv(tie_low_hv);
    I231_1->vblvhv(vblvhv_atb_en);
    I231_1->Z(atb_sel_hv_b[1]);
    I231_1->Z_neg(atb_sel_hvz_b[1]);
    I231_1->gd(gd);
    I231_1->vdd(vdd);
    I231_1->vph(vph);
    I231_1->vss(vss);
    I231_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc("I231_0"));
    I231_0->A_in(atb_sel_b[0]);
    I231_0->onz_hv(atb_en_hvz);
    I231_0->tie_low_hv(tie_low_hv);
    I231_0->vblvhv(vblvhv_atb_en);
    I231_0->Z(atb_sel_hv_b[0]);
    I231_0->Z_neg(atb_sel_hvz_b[0]);
    I231_0->gd(gd);
    I231_0->vdd(vdd);
    I231_0->vph(vph);
    I231_0->vss(vss);
    I154.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_nand_lvt("I154"));
    I154->a(cb_en);
    I154->tie_high_cdm(tie_high_cdm);
    I154->tie_low_cdm(tie_low_cdm);
    I154->z(cb_en_int_z);
    I154->gd(gd);
    I154->vp(vp);
    I215.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_nand_lvt("I215"));
    I215->a(cb_det_supply_z1);
    I215->tie_high_cdm(tie_high_cdm_digital);
    I215->tie_low_cdm(tie_low_cdm_digital);
    I215->z(net370);
    I215->gd(vss);
    I215->vp(vdd);
    I235.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_nand_lvt("I235"));
    I235->a(cb_ibias_en);
    I235->tie_high_cdm(tie_high_cdm);
    I235->tie_low_cdm(tie_low_cdm);
    I235->z(cb_ibias_en_int_z);
    I235->gd(gd);
    I235->vp(vp);
    I251.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_nand_lvt("I251"));
    I251->a(cb_burn_oscilator_int_z);
    I251->tie_high_cdm(tie_high_cdm_digital);
    I251->tie_low_cdm(tie_low_cdm_digital);
    I251->z(cb_burn_oscilator_int);
    I251->gd(vss);
    I251->vp(vdd);
    I222.reset(new mipi_4_rx_dphy_ns_LEVELCONV_vph_det_soc("I222"));
    I222->A_in(tie_high_hv);
    I222->Z(cb_det_envph_lv);
    I222->Z_neg(cb_det_envph_lvz);
    I222->gd(gd);
    I222->vdd(vp);
    I222->vss(gd);
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::split_atb_sel() {
    const vl::logic<2> v = vl::rd(atb_sel);
    atb_sel_b[0].write(v.bit(0).sl());
    atb_sel_b[1].write(v.bit(1).sl());
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::split_cb_cal_repl() {
    const vl::logic<4> v = vl::rd(cb_cal_repl);
    cb_cal_repl_b[0].write(v.bit(0).sl());
    cb_cal_repl_b[1].write(v.bit(1).sl());
    cb_cal_repl_b[2].write(v.bit(2).sl());
    cb_cal_repl_b[3].write(v.bit(3).sl());
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::split_cb_sel_vbg_prog() {
    const vl::logic<3> v = vl::rd(cb_sel_vbg_prog);
    cb_sel_vbg_prog_b[0].write(v.bit(0).sl());
    cb_sel_vbg_prog_b[1].write(v.bit(1).sl());
    cb_sel_vbg_prog_b[2].write(v.bit(2).sl());
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::split_cb_sel_vref_lprx() {
    const vl::logic<2> v = vl::rd(cb_sel_vref_lprx);
    cb_sel_vref_lprx_b[0].write(v.bit(0).sl());
    cb_sel_vref_lprx_b[1].write(v.bit(1).sl());
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::split_cb_sel_vrefcd_lprx() {
    const vl::logic<2> v = vl::rd(cb_sel_vrefcd_lprx);
    cb_sel_vrefcd_lprx_b[0].write(v.bit(0).sl());
    cb_sel_vrefcd_lprx_b[1].write(v.bit(1).sl());
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::split_cb_vref_mpll_reg_sel() {
    const vl::logic<3> v = vl::rd(cb_vref_mpll_reg_sel);
    cb_vref_mpll_reg_sel_b[0].write(v.bit(0).sl());
    cb_vref_mpll_reg_sel_b[1].write(v.bit(1).sl());
    cb_vref_mpll_reg_sel_b[2].write(v.bit(2).sl());
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::split_dac_value() {
    const vl::logic<10> v = vl::rd(dac_value);
    dac_value_b[0].write(v.bit(0).sl());
    dac_value_b[1].write(v.bit(1).sl());
    dac_value_b[2].write(v.bit(2).sl());
    dac_value_b[3].write(v.bit(3).sl());
    dac_value_b[4].write(v.bit(4).sl());
    dac_value_b[5].write(v.bit(5).sl());
    dac_value_b[6].write(v.bit(6).sl());
    dac_value_b[7].write(v.bit(7).sl());
    dac_value_b[8].write(v.bit(8).sl());
    dac_value_b[9].write(v.bit(9).sl());
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::split_gmode() {
    const vl::logic<2> v = vl::rd(gmode);
    gmode_b[0].write(v.bit(0).sl());
    gmode_b[1].write(v.bit(1).sl());
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::split_lb_setrx() {
    const vl::logic<3> v = vl::rd(lb_setrx);
    lb_setrx_b[0].write(v.bit(0).sl());
    lb_setrx_b[1].write(v.bit(1).sl());
    lb_setrx_b[2].write(v.bit(2).sl());
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::split_setrx() {
    const vl::logic<3> v = vl::rd(setrx);
    setrx_b[0].write(v.bit(0).sl());
    setrx_b[1].write(v.bit(1).sl());
    setrx_b[2].write(v.bit(2).sl());
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::split_txlp_prog() {
    const vl::logic<3> v = vl::rd(txlp_prog);
    txlp_prog_b[0].write(v.bit(0).sl());
    txlp_prog_b[1].write(v.bit(1).sl());
    txlp_prog_b[2].write(v.bit(2).sl());
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::merge_atb_sel_hv() {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(atb_sel_hv_b[0]));
    v.set_bit(1, vl::rd(atb_sel_hv_b[1]));
    vl::wr(atb_sel_hv, v);
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::merge_atb_sel_hvz() {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(atb_sel_hvz_b[0]));
    v.set_bit(1, vl::rd(atb_sel_hvz_b[1]));
    vl::wr(atb_sel_hvz, v);
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::merge_cb_cal_repl_hv() {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(cb_cal_repl_hv_b[0]));
    v.set_bit(1, vl::rd(cb_cal_repl_hv_b[1]));
    v.set_bit(2, vl::rd(cb_cal_repl_hv_b[2]));
    v.set_bit(3, vl::rd(cb_cal_repl_hv_b[3]));
    vl::wr(cb_cal_repl_hv, v);
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::merge_cb_cal_repl_hvz() {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(cb_cal_repl_hvz_b[0]));
    v.set_bit(1, vl::rd(cb_cal_repl_hvz_b[1]));
    v.set_bit(2, vl::rd(cb_cal_repl_hvz_b[2]));
    v.set_bit(3, vl::rd(cb_cal_repl_hvz_b[3]));
    vl::wr(cb_cal_repl_hvz, v);
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::merge_cb_sel_vbg_prog_hv() {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(cb_sel_vbg_prog_hv_b[0]));
    v.set_bit(1, vl::rd(cb_sel_vbg_prog_hv_b[1]));
    v.set_bit(2, vl::rd(cb_sel_vbg_prog_hv_b[2]));
    vl::wr(cb_sel_vbg_prog_hv, v);
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::merge_cb_sel_vbg_prog_hvz() {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(cb_sel_vbg_prog_hvz_b[0]));
    v.set_bit(1, vl::rd(cb_sel_vbg_prog_hvz_b[1]));
    v.set_bit(2, vl::rd(cb_sel_vbg_prog_hvz_b[2]));
    vl::wr(cb_sel_vbg_prog_hvz, v);
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::merge_cb_sel_vref_lprx_hv() {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(cb_sel_vref_lprx_hv_b[0]));
    v.set_bit(1, vl::rd(cb_sel_vref_lprx_hv_b[1]));
    vl::wr(cb_sel_vref_lprx_hv, v);
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::merge_cb_sel_vref_lprx_hvz() {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(cb_sel_vref_lprx_hvz_b[0]));
    v.set_bit(1, vl::rd(cb_sel_vref_lprx_hvz_b[1]));
    vl::wr(cb_sel_vref_lprx_hvz, v);
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::merge_cb_sel_vrefcd_lprx_hv() {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(cb_sel_vrefcd_lprx_hv_b[0]));
    v.set_bit(1, vl::rd(cb_sel_vrefcd_lprx_hv_b[1]));
    vl::wr(cb_sel_vrefcd_lprx_hv, v);
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::merge_cb_sel_vrefcd_lprx_hvz() {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(cb_sel_vrefcd_lprx_hvz_b[0]));
    v.set_bit(1, vl::rd(cb_sel_vrefcd_lprx_hvz_b[1]));
    vl::wr(cb_sel_vrefcd_lprx_hvz, v);
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::merge_cb_vref_mpll_reg_sel_hv() {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(cb_vref_mpll_reg_sel_hv_b[0]));
    v.set_bit(1, vl::rd(cb_vref_mpll_reg_sel_hv_b[1]));
    v.set_bit(2, vl::rd(cb_vref_mpll_reg_sel_hv_b[2]));
    vl::wr(cb_vref_mpll_reg_sel_hv, v);
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::merge_dac_value_hv() {
    vl::logic<10> v(0);
    v.set_bit(0, vl::rd(dac_value_hv_b[0]));
    v.set_bit(1, vl::rd(dac_value_hv_b[1]));
    v.set_bit(2, vl::rd(dac_value_hv_b[2]));
    v.set_bit(3, vl::rd(dac_value_hv_b[3]));
    v.set_bit(4, vl::rd(dac_value_hv_b[4]));
    v.set_bit(5, vl::rd(dac_value_hv_b[5]));
    v.set_bit(6, vl::rd(dac_value_hv_b[6]));
    v.set_bit(7, vl::rd(dac_value_hv_b[7]));
    v.set_bit(8, vl::rd(dac_value_hv_b[8]));
    v.set_bit(9, vl::rd(dac_value_hv_b[9]));
    vl::wr(dac_value_hv, v);
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::merge_dac_value_hvz() {
    vl::logic<10> v(0);
    v.set_bit(0, vl::rd(dac_value_hvz_b[0]));
    v.set_bit(1, vl::rd(dac_value_hvz_b[1]));
    v.set_bit(2, vl::rd(dac_value_hvz_b[2]));
    v.set_bit(3, vl::rd(dac_value_hvz_b[3]));
    v.set_bit(4, vl::rd(dac_value_hvz_b[4]));
    v.set_bit(5, vl::rd(dac_value_hvz_b[5]));
    v.set_bit(6, vl::rd(dac_value_hvz_b[6]));
    v.set_bit(7, vl::rd(dac_value_hvz_b[7]));
    v.set_bit(8, vl::rd(dac_value_hvz_b[8]));
    v.set_bit(9, vl::rd(dac_value_hvz_b[9]));
    vl::wr(dac_value_hvz, v);
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::merge_gmode_hv() {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(gmode_hv_b[0]));
    v.set_bit(1, vl::rd(gmode_hv_b[1]));
    vl::wr(gmode_hv, v);
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::merge_gmode_hvz() {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(gmode_hvz_b[0]));
    v.set_bit(1, vl::rd(gmode_hvz_b[1]));
    vl::wr(gmode_hvz, v);
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::merge_lb_setzrx_hv() {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(lb_setzrx_hv_b[0]));
    v.set_bit(1, vl::rd(lb_setzrx_hv_b[1]));
    v.set_bit(2, vl::rd(lb_setzrx_hv_b[2]));
    vl::wr(lb_setzrx_hv, v);
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::merge_setrx_hv() {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(setrx_hv_b[0]));
    v.set_bit(1, vl::rd(setrx_hv_b[1]));
    v.set_bit(2, vl::rd(setrx_hv_b[2]));
    vl::wr(setrx_hv, v);
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::merge_txlp_prog_hv() {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(txlp_prog_hv_b[0]));
    v.set_bit(1, vl::rd(txlp_prog_hv_b[1]));
    v.set_bit(2, vl::rd(txlp_prog_hv_b[2]));
    vl::wr(txlp_prog_hv, v);
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::merge_txlp_prog_hvz() {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(txlp_prog_hvz_b[0]));
    v.set_bit(1, vl::rd(txlp_prog_hvz_b[1]));
    v.set_bit(2, vl::rd(txlp_prog_hvz_b[2]));
    vl::wr(txlp_prog_hvz, v);
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_atb_sel() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(atb_sel_b[0]));
    v.set_bit(1, vl::rd(atb_sel_b[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_atb_sel_hv() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(atb_sel_hv_b[0]));
    v.set_bit(1, vl::rd(atb_sel_hv_b[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_atb_sel_hvz() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(atb_sel_hvz_b[0]));
    v.set_bit(1, vl::rd(atb_sel_hvz_b[1]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_cb_cal_repl() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(cb_cal_repl_b[0]));
    v.set_bit(1, vl::rd(cb_cal_repl_b[1]));
    v.set_bit(2, vl::rd(cb_cal_repl_b[2]));
    v.set_bit(3, vl::rd(cb_cal_repl_b[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_cb_cal_repl_hv() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(cb_cal_repl_hv_b[0]));
    v.set_bit(1, vl::rd(cb_cal_repl_hv_b[1]));
    v.set_bit(2, vl::rd(cb_cal_repl_hv_b[2]));
    v.set_bit(3, vl::rd(cb_cal_repl_hv_b[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_cb_cal_repl_hvz() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(cb_cal_repl_hvz_b[0]));
    v.set_bit(1, vl::rd(cb_cal_repl_hvz_b[1]));
    v.set_bit(2, vl::rd(cb_cal_repl_hvz_b[2]));
    v.set_bit(3, vl::rd(cb_cal_repl_hvz_b[3]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_cb_sel_vbg_prog() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(cb_sel_vbg_prog_b[0]));
    v.set_bit(1, vl::rd(cb_sel_vbg_prog_b[1]));
    v.set_bit(2, vl::rd(cb_sel_vbg_prog_b[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_cb_sel_vbg_prog_hv() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(cb_sel_vbg_prog_hv_b[0]));
    v.set_bit(1, vl::rd(cb_sel_vbg_prog_hv_b[1]));
    v.set_bit(2, vl::rd(cb_sel_vbg_prog_hv_b[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_cb_sel_vbg_prog_hvz() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(cb_sel_vbg_prog_hvz_b[0]));
    v.set_bit(1, vl::rd(cb_sel_vbg_prog_hvz_b[1]));
    v.set_bit(2, vl::rd(cb_sel_vbg_prog_hvz_b[2]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_cb_sel_vref_lprx() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(cb_sel_vref_lprx_b[0]));
    v.set_bit(1, vl::rd(cb_sel_vref_lprx_b[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_cb_sel_vref_lprx_hv() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(cb_sel_vref_lprx_hv_b[0]));
    v.set_bit(1, vl::rd(cb_sel_vref_lprx_hv_b[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_cb_sel_vref_lprx_hvz() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(cb_sel_vref_lprx_hvz_b[0]));
    v.set_bit(1, vl::rd(cb_sel_vref_lprx_hvz_b[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_cb_sel_vrefcd_lprx() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(cb_sel_vrefcd_lprx_b[0]));
    v.set_bit(1, vl::rd(cb_sel_vrefcd_lprx_b[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_cb_sel_vrefcd_lprx_hv() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(cb_sel_vrefcd_lprx_hv_b[0]));
    v.set_bit(1, vl::rd(cb_sel_vrefcd_lprx_hv_b[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_cb_sel_vrefcd_lprx_hvz() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(cb_sel_vrefcd_lprx_hvz_b[0]));
    v.set_bit(1, vl::rd(cb_sel_vrefcd_lprx_hvz_b[1]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_cb_vref_mpll_reg_sel() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(cb_vref_mpll_reg_sel_b[0]));
    v.set_bit(1, vl::rd(cb_vref_mpll_reg_sel_b[1]));
    v.set_bit(2, vl::rd(cb_vref_mpll_reg_sel_b[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_cb_vref_mpll_reg_sel_hv() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(cb_vref_mpll_reg_sel_hv_b[0]));
    v.set_bit(1, vl::rd(cb_vref_mpll_reg_sel_hv_b[1]));
    v.set_bit(2, vl::rd(cb_vref_mpll_reg_sel_hv_b[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_cb_vref_mpll_reg_sel_hvz() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(cb_vref_mpll_reg_sel_hvz[0]));
    v.set_bit(1, vl::rd(cb_vref_mpll_reg_sel_hvz[1]));
    v.set_bit(2, vl::rd(cb_vref_mpll_reg_sel_hvz[2]));
    return v;
}

vl::logic<10> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_dac_value() const {
    vl::logic<10> v(0);
    v.set_bit(0, vl::rd(dac_value_b[0]));
    v.set_bit(1, vl::rd(dac_value_b[1]));
    v.set_bit(2, vl::rd(dac_value_b[2]));
    v.set_bit(3, vl::rd(dac_value_b[3]));
    v.set_bit(4, vl::rd(dac_value_b[4]));
    v.set_bit(5, vl::rd(dac_value_b[5]));
    v.set_bit(6, vl::rd(dac_value_b[6]));
    v.set_bit(7, vl::rd(dac_value_b[7]));
    v.set_bit(8, vl::rd(dac_value_b[8]));
    v.set_bit(9, vl::rd(dac_value_b[9]));
    return v;
}

vl::logic<10> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_dac_value_hv() const {
    vl::logic<10> v(0);
    v.set_bit(0, vl::rd(dac_value_hv_b[0]));
    v.set_bit(1, vl::rd(dac_value_hv_b[1]));
    v.set_bit(2, vl::rd(dac_value_hv_b[2]));
    v.set_bit(3, vl::rd(dac_value_hv_b[3]));
    v.set_bit(4, vl::rd(dac_value_hv_b[4]));
    v.set_bit(5, vl::rd(dac_value_hv_b[5]));
    v.set_bit(6, vl::rd(dac_value_hv_b[6]));
    v.set_bit(7, vl::rd(dac_value_hv_b[7]));
    v.set_bit(8, vl::rd(dac_value_hv_b[8]));
    v.set_bit(9, vl::rd(dac_value_hv_b[9]));
    return v;
}

vl::logic<10> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_dac_value_hvz() const {
    vl::logic<10> v(0);
    v.set_bit(0, vl::rd(dac_value_hvz_b[0]));
    v.set_bit(1, vl::rd(dac_value_hvz_b[1]));
    v.set_bit(2, vl::rd(dac_value_hvz_b[2]));
    v.set_bit(3, vl::rd(dac_value_hvz_b[3]));
    v.set_bit(4, vl::rd(dac_value_hvz_b[4]));
    v.set_bit(5, vl::rd(dac_value_hvz_b[5]));
    v.set_bit(6, vl::rd(dac_value_hvz_b[6]));
    v.set_bit(7, vl::rd(dac_value_hvz_b[7]));
    v.set_bit(8, vl::rd(dac_value_hvz_b[8]));
    v.set_bit(9, vl::rd(dac_value_hvz_b[9]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_gmode() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(gmode_b[0]));
    v.set_bit(1, vl::rd(gmode_b[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_gmode_hv() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(gmode_hv_b[0]));
    v.set_bit(1, vl::rd(gmode_hv_b[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_gmode_hvz() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(gmode_hvz_b[0]));
    v.set_bit(1, vl::rd(gmode_hvz_b[1]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_lb_setrx() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(lb_setrx_b[0]));
    v.set_bit(1, vl::rd(lb_setrx_b[1]));
    v.set_bit(2, vl::rd(lb_setrx_b[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_lb_setrx_hv() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(lb_setrx_hv[0]));
    v.set_bit(1, vl::rd(lb_setrx_hv[1]));
    v.set_bit(2, vl::rd(lb_setrx_hv[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_lb_setzrx_hv() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(lb_setzrx_hv_b[0]));
    v.set_bit(1, vl::rd(lb_setzrx_hv_b[1]));
    v.set_bit(2, vl::rd(lb_setzrx_hv_b[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_setrx() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(setrx_b[0]));
    v.set_bit(1, vl::rd(setrx_b[1]));
    v.set_bit(2, vl::rd(setrx_b[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_setrx_hv() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(setrx_hv_b[0]));
    v.set_bit(1, vl::rd(setrx_hv_b[1]));
    v.set_bit(2, vl::rd(setrx_hv_b[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_setrx_hvz() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(setrx_hvz[0]));
    v.set_bit(1, vl::rd(setrx_hvz[1]));
    v.set_bit(2, vl::rd(setrx_hvz[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_txlp_prog() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(txlp_prog_b[0]));
    v.set_bit(1, vl::rd(txlp_prog_b[1]));
    v.set_bit(2, vl::rd(txlp_prog_b[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_txlp_prog_hv() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(txlp_prog_hv_b[0]));
    v.set_bit(1, vl::rd(txlp_prog_hv_b[1]));
    v.set_bit(2, vl::rd(txlp_prog_hv_b[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYCBRX_lc::rd_txlp_prog_hvz() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(txlp_prog_hvz_b[0]));
    v.set_bit(1, vl::rd(txlp_prog_hvz_b[1]));
    v.set_bit(2, vl::rd(txlp_prog_hvz_b[2]));
    return v;
}

void mipi_4_rx_dphy_ns_DPHYCBRX_lc::gate_N192() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x0ULL), vl::ceq(vl::rd(tie_low_cdm), vl::logic<1>(0x1ULL)));
    vl::wr(cb_burn_oscilator_p, v);
}
