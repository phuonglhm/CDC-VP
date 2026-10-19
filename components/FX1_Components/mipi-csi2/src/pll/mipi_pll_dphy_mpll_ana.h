//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_mpll_ana
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_MPLL_ANA_H
#define VL_MIPI_PLL_DPHY_MPLL_ANA_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_pll_dphy_mpll_cdm_prot_clk.h"
#include "mipi_pll_dphy_mpll_l2h.h"
#include "mipi_pll_dphy_mpll_logic_l2h.h"
#include "mipi_pll_dphy_mpll_phdet.h"
#include "mipi_pll_dphy_mpll_prediv_prog.h"
#include "mipi_pll_dphy_pllcoremodel.h"

class mipi_pll_dphy_mpll_ana : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> atb_sense_sel{"atb_sense_sel"};
    sc_core::sc_in<sc_dt::sc_logic> clkref{"clkref"};
    sc_core::sc_in<sc_dt::sc_lv<2>> clksel{"clksel"};
    sc_core::sc_in<sc_dt::sc_logic> clksqron{"clksqron"};
    sc_core::sc_in<sc_dt::sc_lv<7>> cp_bias_cntrl{"cp_bias_cntrl"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_lv<2>> gmp_cntrl{"gmp_cntrl"};
    sc_core::sc_in<sc_dt::sc_lv<4>> int_cntrl{"int_cntrl"};
    sc_core::sc_in<sc_dt::sc_lv<18>> meas_iv{"meas_iv"};
    sc_core::sc_in<sc_dt::sc_logic> mpll_gear_shift{"mpll_gear_shift"};
    sc_core::sc_in<sc_dt::sc_logic> mpll_ph_sel_clk{"mpll_ph_sel_clk"};
    sc_core::sc_in<sc_dt::sc_lv<9>> mpll_ph_sel_wd{"mpll_ph_sel_wd"};
    sc_core::sc_in<sc_dt::sc_logic> mpll_pmix_clk_sel{"mpll_pmix_clk_sel"};
    sc_core::sc_in<sc_dt::sc_lv<17>> mpll_prg{"mpll_prg"};
    sc_core::sc_in<sc_dt::sc_logic> mpll_pwron{"mpll_pwron"};
    sc_core::sc_in<sc_dt::sc_logic> mpll_pwron_pmix{"mpll_pwron_pmix"};
    sc_core::sc_in<sc_dt::sc_logic> mpll_relock_gear{"mpll_relock_gear"};
    sc_core::sc_in<sc_dt::sc_logic> mpll_relock_vint{"mpll_relock_vint"};
    sc_core::sc_in<sc_dt::sc_logic> mpll_reset{"mpll_reset"};
    sc_core::sc_in<sc_dt::sc_logic> mpll_rst_pmix{"mpll_rst_pmix"};
    sc_core::sc_in<sc_dt::sc_lv<5>> opmode{"opmode"};
    sc_core::sc_in<sc_dt::sc_logic> oscclk{"oscclk"};
    sc_core::sc_in<sc_dt::sc_lv<5>> prop_cntrl{"prop_cntrl"};
    sc_core::sc_in<sc_dt::sc_lv<2>> ref_cntrl{"ref_cntrl"};
    sc_core::sc_in<sc_dt::sc_logic> vbg{"vbg"};
    sc_core::sc_in<sc_dt::sc_lv<4>> vco_cntrl{"vco_cntrl"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vphreg{"vphreg"};
    sc_core::sc_in<sc_dt::sc_logic> vpl{"vpl"};
    sc_core::sc_in<sc_dt::sc_logic> vss{"vss"};
    sc_core::sc_out<sc_dt::sc_logic> byp_ncntrldiv{"byp_ncntrldiv"};
    sc_core::sc_out<sc_dt::sc_lv<8>> ck_l2h{"ck_l2h"};
    sc_core::sc_out<sc_dt::sc_logic> clk_i_m{"clk_i_m"};
    sc_core::sc_out<sc_dt::sc_logic> clk_i_p{"clk_i_p"};
    sc_core::sc_out<sc_dt::sc_logic> clk_q_m{"clk_q_m"};
    sc_core::sc_out<sc_dt::sc_logic> clk_q_p{"clk_q_p"};
    sc_core::sc_out<sc_dt::sc_logic> clkrefdiv{"clkrefdiv"};
    sc_core::sc_out<sc_dt::sc_logic> clksqr_on{"clksqr_on"};
    sc_core::sc_out<sc_dt::sc_logic> clksqr_on_h{"clksqr_on_h"};
    sc_core::sc_out<sc_dt::sc_logic> dn_lockdet{"dn_lockdet"};
    sc_core::sc_out<sc_dt::sc_lv<2>> meas_iv_h_out{"meas_iv_h_out"};
    sc_core::sc_out<sc_dt::sc_logic> mpll_ph_sel_clk_out{"mpll_ph_sel_clk_out"};
    sc_core::sc_out<sc_dt::sc_lv<9>> mpll_ph_sel_wd_out{"mpll_ph_sel_wd_out"};
    sc_core::sc_out<sc_dt::sc_lv<4>> ph_pmix{"ph_pmix"};
    sc_core::sc_out<sc_dt::sc_logic> pmix_clk_sel{"pmix_clk_sel"};
    sc_core::sc_out<sc_dt::sc_logic> pwron{"pwron"};
    sc_core::sc_out<sc_dt::sc_logic> pwron_n{"pwron_n"};
    sc_core::sc_out<sc_dt::sc_logic> pwron_pmix_h{"pwron_pmix_h"};
    sc_core::sc_out<sc_dt::sc_logic> pwron_pmix_n{"pwron_pmix_n"};
    sc_core::sc_out<sc_dt::sc_logic> reset{"reset"};
    sc_core::sc_out<sc_dt::sc_logic> rst_pmix_h{"rst_pmix_h"};
    sc_core::sc_out<sc_dt::sc_logic> up_lockdet{"up_lockdet"};
    sc_core::sc_out<sc_dt::sc_logic> vp_l2h{"vp_l2h"};
    sc_core::sc_out<sc_dt::sc_logic> vpl_det{"vpl_det"};
    sc_core::sc_out<sc_dt::sc_logic> vref_pmix{"vref_pmix"};
    sc_core::sc_out<sc_dt::sc_logic> vreg_mpll{"vreg_mpll"};
    sc_core::sc_in<sc_dt::sc_logic> atb_f_m{"atb_f_m"};
    sc_core::sc_in<sc_dt::sc_logic> atb_s_m{"atb_s_m"};
    sc_core::sc_in<sc_dt::sc_logic> atb_s_m_int{"atb_s_m_int"};
    sc_core::sc_in<sc_dt::sc_logic> atb_s_p{"atb_s_p"};
    sc_core::sc_in<sc_dt::sc_logic> atb_s_p_int{"atb_s_p_int"};
    sc_core::sc_in<sc_dt::sc_logic> ivco{"ivco"};
    sc_core::sc_in<sc_dt::sc_logic> ivco_osc{"ivco_osc"};
    sc_core::sc_in<sc_dt::sc_lv<10>> m_obs_core_model{"m_obs_core_model"};

    SC_HAS_PROCESS(mipi_pll_dphy_mpll_ana);
    explicit mipi_pll_dphy_mpll_ana(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> mpll_prg_b{"mpll_prg_b", 17, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> meas_iv_h_out_b{"meas_iv_h_out_b", 2, vl::make_z1};
    sc_core::sc_signal_resolved net1457{"net1457", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<4> int_cntrl_prot{"int_cntrl_prot", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_resolved net1458{"net1458", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<6> mpll_prg_hn{"mpll_prg_hn", vl::logic<6>::all_z().lv()};
    sc_core::sc_signal_resolved xor_clk{"xor_clk", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vcqp{"vcqp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vcq{"vcq", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved oscclk_prot{"oscclk_prot", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ckdcc_q_m{"ckdcc_q_m", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ckdcc_q_p{"ckdcc_q_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<2> ref_cntrl_prot{"ref_cntrl_prot", vl::logic<2>::all_z().lv()};
    sc_core::sc_vector<sc_core::sc_signal_resolved> meas_iv_h{"meas_iv_h", 17, vl::make_z1};
    sc_core::sc_signal_resolved dcc_2nd_on_h{"dcc_2nd_on_h", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vcntrl{"vcntrl", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<7> cp_bias_cntrl_prot{"cp_bias_cntrl_prot", vl::logic<7>::all_z().lv()};
    sc_core::sc_signal_resolved tie_low_h{"tie_low_h", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vref{"vref", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vmb{"vmb", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<6> mpll_prg_h{"mpll_prg_h", vl::logic<6>::all_z().lv()};
    sc_core::sc_signal_resolved inc_freq{"inc_freq", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ckref_prot{"ckref_prot", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_high_h{"tie_high_h", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vint{"vint", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved bypass_vreg_ckout{"bypass_vreg_ckout", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved icasc{"icasc", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pwron_h{"pwron_h", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vp_dnw{"vp_dnw", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved capon_n{"capon_n", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> vco_cntr_prot{"vco_cntr_prot", 4, vl::make_z1};
    sc_core::sc_signal_resolved atb_select_h{"atb_select_h", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<2> gmp_cntrl_prot{"gmp_cntrl_prot", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_resolved vp_cp{"vp_cp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rst_h{"rst_h", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vref_vpcp{"vref_vpcp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved icp{"icp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved bypass_vreg_cp_h{"bypass_vreg_cp_h", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vpsf{"vpsf", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<5> prop_cntrl_prot{"prop_cntrl_prot", vl::logic<5>::all_z().lv()};
    sc_core::sc_signal_rv<2> gmp_cntrl_h{"gmp_cntrl_h", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_resolved gs_h{"gs_h", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved bypass_vreg_ckout_h{"bypass_vreg_ckout_h", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vdoubled{"vdoubled", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved icp1{"icp1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vrfbf{"vrfbf", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vref_cko{"vref_cko", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vcim{"vcim", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved bypass_vreg_cp{"bypass_vreg_cp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved dcc_2nd_on{"dcc_2nd_on", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved dec_freq{"dec_freq", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vcip{"vcip", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vci{"vci", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ckdcc_i_m{"ckdcc_i_m", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved capon_hn{"capon_hn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vcqm{"vcqm", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ckdcc_i_p{"ckdcc_i_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> err_flg{"err_flg", (vl::logic<32>(0x0ULL).ext<1>()).sl()};
    sc_core::sc_signal<sc_dt::sc_logic> clk_stable{"clk_stable", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> pddq_h_dly{"pddq_h_dly", vl::logic<1>().sl()};
    sc_core::sc_signal_resolved mpwr_on_gated{"mpwr_on_gated", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved mpll_gear_shift_gated{"mpll_gear_shift_gated", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved mpll_rst_gated{"mpll_rst_gated", sc_dt::SC_LOGIC_Z};
    double t1 = 0.0;
    double t2 = 0.0;
    double t3 = 0.0;
    sc_core::sc_signal_resolved supply_ok_vco{"supply_ok_vco", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved supply_ok_vco_sync{"supply_ok_vco_sync", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lock_vco{"lock_vco", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved refclk_from_pllcore_model{"refclk_from_pllcore_model", sc_dt::SC_LOGIC_Z};
    vl::vreg<8> supply_ok_vco_ck_l2h{"supply_ok_vco_ck_l2h", vl::logic<8>().lv()};
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
    std::unique_ptr<mipi_pll_dphy_mpll_logic_l2h> regs;
    sc_core::sc_signal_rv<17> c_regs_meas_iv{"c_regs_meas_iv", vl::logic<17>::all_z().lv()};
    sc_core::sc_signal_rv<17> c_regs_mpll_prg{"c_regs_mpll_prg", vl::logic<17>::all_z().lv()};
    sc_core::sc_signal_rv<17> c_regs_meas_iv_h{"c_regs_meas_iv_h", vl::logic<17>::all_z().lv()};
    sc_core::sc_signal_rv<4> c_regs_vco_cntrl_prot{"c_regs_vco_cntrl_prot", vl::logic<4>::all_z().lv()};
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
    std::unique_ptr<mipi_pll_dphy_mpll_prediv_prog> prediv;
    std::unique_ptr<mipi_pll_dphy_pllcoremodel> I01PLLCOREMODEL;
    sc_core::sc_signal_resolved c_I01PLLCOREMODEL_on{"c_I01PLLCOREMODEL_on", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<64> c_I01PLLCOREMODEL_feedbackdiv{"c_I01PLLCOREMODEL_feedbackdiv", vl::logic<64>::all_z().lv()};
    sc_core::sc_signal_rv<64> c_I01PLLCOREMODEL_inputdiv{"c_I01PLLCOREMODEL_inputdiv", vl::logic<64>::all_z().lv()};
    sc_core::sc_signal_rv<64> c_I01PLLCOREMODEL_outputdiv{"c_I01PLLCOREMODEL_outputdiv", vl::logic<64>::all_z().lv()};
    sc_core::sc_signal_resolved tie1_{"tie1_", sc_dt::SC_LOGIC_1};
    vl::tie tie1__drv{"tie1__drv", sc_dt::SC_LOGIC_1};
    sc_core::sc_signal_rv<5> c_I01PLLCOREMODEL_phsel{"c_I01PLLCOREMODEL_phsel", vl::logic<5>::all_z().lv()};
    sc_core::sc_signal_resolved tie0_{"tie0_", sc_dt::SC_LOGIC_0};
    vl::tie tie0__drv{"tie0__drv", sc_dt::SC_LOGIC_0};
    sc_core::sc_signal_rv<64> nc_I01PLLCOREMODEL_ssc_depth{"nc_I01PLLCOREMODEL_ssc_depth", vl::logic<64>::all_z().lv()};
    sc_core::sc_signal_rv<64> nc_I01PLLCOREMODEL_ssc_freq{"nc_I01PLLCOREMODEL_ssc_freq", vl::logic<64>::all_z().lv()};
    sc_core::sc_signal_resolved nc_I01PLLCOREMODEL_fbclk{"nc_I01PLLCOREMODEL_fbclk", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved nc_I01PLLCOREMODEL_clkout_div{"nc_I01PLLCOREMODEL_clkout_div", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_mpll_l2h> l2h;
    sc_core::sc_signal_resolved nc_l2h_vci{"nc_l2h_vci", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved nc_l2h_vcq{"nc_l2h_vcq", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved nc_l2h_ckdcc_i_m{"nc_l2h_ckdcc_i_m", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved nc_l2h_ckdcc_i_p{"nc_l2h_ckdcc_i_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved nc_l2h_ckdcc_q_m{"nc_l2h_ckdcc_q_m", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved nc_l2h_ckdcc_q_p{"nc_l2h_ckdcc_q_p", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_mpll_phdet> pd;
    sc_core::sc_signal_resolved nc_pd_dec_freq{"nc_pd_dec_freq", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved nc_pd_inc_freq{"nc_pd_inc_freq", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_mpll_cdm_prot_clk> I113;
    sc_core::sc_signal<sc_dt::sc_logic> ev_sig{"ev_sig"};
    sc_core::sc_signal<sc_dt::sc_logic> ev_sig_2{"ev_sig_2"};
    sc_core::sc_signal<sc_dt::sc_logic> ev_sig_3{"ev_sig_3"};
    sc_core::sc_signal<sc_dt::sc_logic> ev_sig_4{"ev_sig_4"};
    sc_core::sc_signal<sc_dt::sc_logic> ev_sig_5{"ev_sig_5"};
    sc_core::sc_signal<sc_dt::sc_logic> ev_sig_6{"ev_sig_6"};
    sc_core::sc_signal<sc_dt::sc_logic> ev_sig_7{"ev_sig_7"};
    sc_core::sc_signal<sc_dt::sc_logic> ev_sig_8{"ev_sig_8"};

    void split_mpll_prg();
    void merge_meas_iv_h_out();
    vl::logic<17> rd_meas_iv_h() const;
    vl::logic<2> rd_meas_iv_h_out() const;
    vl::logic<17> rd_mpll_prg() const;
    vl::logic<4> rd_vco_cntr_prot() const;
    void assign_mpwr_on_gated();
    void assign_mpll_gear_shift_gated();
    void assign_mpll_rst_gated();
    void assign_supply_ok_vco();
    void assign_supply_ok_vco_sync();
    void assign_vreg_mpll();
    void assign_vp_l2h();
    void glue_regs_meas_iv();
    void glue_regs_mpll_prg();
    void split_regs_meas_iv_h();
    void split_regs_vco_cntrl_prot();
    void glue_I01PLLCOREMODEL_on();
    void glue_I01PLLCOREMODEL_feedbackdiv();
    void glue_I01PLLCOREMODEL_inputdiv();
    void glue_I01PLLCOREMODEL_outputdiv();
    void glue_I01PLLCOREMODEL_phsel();
    void proc_initial_3045();
    void proc_initial_3051();
    void proc_always_3133();
    void ev_glue();
    void proc_always_3136();
    void ev_glue_2();
    void proc_always_3139();
    void ev_glue_3();
    void proc_always_3142();
    void ev_glue_4();
    void proc_always_3145();
    void ev_glue_5();
    void proc_always_3148();
    void ev_glue_6();
    void proc_always_3151();
    void ev_glue_7();
    void proc_always_3154();
    void ev_glue_8();
};

#endif
