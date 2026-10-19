//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_no_clamp
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_DPHYPLL_NO_CLAMP_H
#define VL_MIPI_PLL_DPHY_DPHYPLL_NO_CLAMP_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_pll_dphy_DPHYPLL_dig_addon.h"
#include "mipi_pll_dphy_mpll_ana.h"
#include "mipi_pll_dphy_sc_tie_hilow_lvt.h"

class mipi_pll_dphy_DPHYPLL_no_clamp : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> atb_sense_sel{"atb_sense_sel"};
    sc_core::sc_in<sc_dt::sc_logic> clkext{"clkext"};
    sc_core::sc_in<sc_dt::sc_logic> clkin{"clkin"};
    sc_core::sc_in<sc_dt::sc_logic> clkouten_left{"clkouten_left"};
    sc_core::sc_in<sc_dt::sc_logic> clkouten_right{"clkouten_right"};
    sc_core::sc_in<sc_dt::sc_lv<2>> clksel{"clksel"};
    sc_core::sc_in<sc_dt::sc_logic> clksel_en{"clksel_en"};
    sc_core::sc_in<sc_dt::sc_lv<7>> cpbias_cntrl{"cpbias_cntrl"};
    sc_core::sc_in<sc_dt::sc_logic> force_lock{"force_lock"};
    sc_core::sc_in<sc_dt::sc_logic> force_lock_phy{"force_lock_phy"};
    sc_core::sc_in<sc_dt::sc_lv<2>> gmp_cntrl{"gmp_cntrl"};
    sc_core::sc_in<sc_dt::sc_logic> gp_clk_en{"gp_clk_en"};
    sc_core::sc_in<sc_dt::sc_lv<6>> int_cntrl{"int_cntrl"};
    sc_core::sc_in<sc_dt::sc_logic> lock_det_on{"lock_det_on"};
    sc_core::sc_in<sc_dt::sc_logic> lock_sel{"lock_sel"};
    sc_core::sc_in<sc_dt::sc_logic> lockdet_mode{"lockdet_mode"};
    sc_core::sc_in<sc_dt::sc_lv<10>> m{"m"};
    sc_core::sc_in<sc_dt::sc_lv<18>> meas_iv{"meas_iv"};
    sc_core::sc_in<sc_dt::sc_logic> mpll_gear_shift{"mpll_gear_shift"};
    sc_core::sc_in<sc_dt::sc_lv<5>> mpll_opmode{"mpll_opmode"};
    sc_core::sc_in<sc_dt::sc_lv<17>> mpll_prg{"mpll_prg"};
    sc_core::sc_in<sc_dt::sc_logic> mpll_pwron{"mpll_pwron"};
    sc_core::sc_in<sc_dt::sc_logic> mpll_reset{"mpll_reset"};
    sc_core::sc_in<sc_dt::sc_lv<4>> n{"n"};
    sc_core::sc_in<sc_dt::sc_lv<7>> phy_cpbias_cntrl{"phy_cpbias_cntrl"};
    sc_core::sc_in<sc_dt::sc_lv<2>> phy_gmp_cntrl{"phy_gmp_cntrl"};
    sc_core::sc_in<sc_dt::sc_lv<6>> phy_int_cntrl{"phy_int_cntrl"};
    sc_core::sc_in<sc_dt::sc_lv<10>> phy_m{"phy_m"};
    sc_core::sc_in<sc_dt::sc_lv<4>> phy_n{"phy_n"};
    sc_core::sc_in<sc_dt::sc_lv<6>> phy_prop_cntrl{"phy_prop_cntrl"};
    sc_core::sc_in<sc_dt::sc_lv<6>> phy_vco_cntrl{"phy_vco_cntrl"};
    sc_core::sc_in<sc_dt::sc_logic> pll_shadow_control{"pll_shadow_control"};
    sc_core::sc_in<sc_dt::sc_lv<6>> prop_cntrl{"prop_cntrl"};
    sc_core::sc_in<sc_dt::sc_logic> scanclk{"scanclk"};
    sc_core::sc_in<sc_dt::sc_logic> scanen{"scanen"};
    sc_core::sc_in<sc_dt::sc_logic> scanin{"scanin"};
    sc_core::sc_in<sc_dt::sc_logic> scanluctrl{"scanluctrl"};
    sc_core::sc_in<sc_dt::sc_logic> scanmode{"scanmode"};
    sc_core::sc_in<sc_dt::sc_logic> scanrstz{"scanrstz"};
    sc_core::sc_in<sc_dt::sc_logic> shadow_clear{"shadow_clear"};
    sc_core::sc_in<sc_dt::sc_lv<10>> th1{"th1"};
    sc_core::sc_in<sc_dt::sc_lv<8>> th2{"th2"};
    sc_core::sc_in<sc_dt::sc_lv<8>> th3{"th3"};
    sc_core::sc_in<sc_dt::sc_lv<6>> th_delay{"th_delay"};
    sc_core::sc_in<sc_dt::sc_lv<3>> tstplldig{"tstplldig"};
    sc_core::sc_in<sc_dt::sc_logic> updatepll{"updatepll"};
    sc_core::sc_in<sc_dt::sc_lv<6>> vco_cntrl{"vco_cntrl"};
    sc_core::sc_out<sc_dt::sc_logic> clkout0_left{"clkout0_left"};
    sc_core::sc_out<sc_dt::sc_logic> clkout0_right{"clkout0_right"};
    sc_core::sc_out<sc_dt::sc_logic> clkout180_left{"clkout180_left"};
    sc_core::sc_out<sc_dt::sc_logic> clkout180_right{"clkout180_right"};
    sc_core::sc_out<sc_dt::sc_logic> clkout270_left{"clkout270_left"};
    sc_core::sc_out<sc_dt::sc_logic> clkout270_right{"clkout270_right"};
    sc_core::sc_out<sc_dt::sc_logic> clkout90_left{"clkout90_left"};
    sc_core::sc_out<sc_dt::sc_logic> clkout90_right{"clkout90_right"};
    sc_core::sc_out<sc_dt::sc_logic> clkout_gp{"clkout_gp"};
    sc_core::sc_out<sc_dt::sc_logic> clkout_stop_left{"clkout_stop_left"};
    sc_core::sc_out<sc_dt::sc_logic> clkout_stop_right{"clkout_stop_right"};
    sc_core::sc_out<sc_dt::sc_lv<7>> cpbias_cntrl_obs{"cpbias_cntrl_obs"};
    sc_core::sc_out<sc_dt::sc_lv<2>> gmp_cntrl_obs{"gmp_cntrl_obs"};
    sc_core::sc_out<sc_dt::sc_lv<6>> int_cntrl_obs{"int_cntrl_obs"};
    sc_core::sc_out<sc_dt::sc_logic> lock{"lock"};
    sc_core::sc_out<sc_dt::sc_logic> lock_phy{"lock_phy"};
    sc_core::sc_out<sc_dt::sc_lv<10>> m_obs{"m_obs"};
    sc_core::sc_out<sc_dt::sc_lv<4>> n_obs{"n_obs"};
    sc_core::sc_out<sc_dt::sc_lv<2>> phy_in_clksel{"phy_in_clksel"};
    sc_core::sc_out<sc_dt::sc_logic> pll_shadow_control_obs{"pll_shadow_control_obs"};
    sc_core::sc_out<sc_dt::sc_lv<6>> prop_cntrl_obs{"prop_cntrl_obs"};
    sc_core::sc_out<sc_dt::sc_logic> scanout{"scanout"};
    sc_core::sc_out<sc_dt::sc_logic> testlock{"testlock"};
    sc_core::sc_out<sc_dt::sc_lv<6>> vco_cntrl_obs{"vco_cntrl_obs"};
    sc_core::sc_out<sc_dt::sc_logic> vpl_det{"vpl_det"};
    sc_core::sc_out<sc_dt::sc_logic> vreg_mpll{"vreg_mpll"};
    sc_core::sc_in<sc_dt::sc_logic> atb_sense{"atb_sense"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> ivco{"ivco"};
    sc_core::sc_in<sc_dt::sc_logic> ivco_osc{"ivco_osc"};
    sc_core::sc_in<sc_dt::sc_logic> vbg{"vbg"};
    sc_core::sc_inout<sc_dt::sc_logic> vdd{"vdd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    sc_core::sc_in<sc_dt::sc_logic> vpl{"vpl"};
    sc_core::sc_inout<sc_dt::sc_logic> vss{"vss"};

    SC_HAS_PROCESS(mipi_pll_dphy_DPHYPLL_no_clamp);
    explicit mipi_pll_dphy_DPHYPLL_no_clamp(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved up_lockdet{"up_lockdet", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1857{"net1857", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<4> vco_cntrl_int{"vco_cntrl_int", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_rv<6> prop_cntrl_int{"prop_cntrl_int", vl::logic<6>::all_z().lv()};
    sc_core::sc_signal_resolved clk_i_m{"clk_i_m", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> ph_pmix{"ph_pmix", 4, vl::make_z1};
    sc_core::sc_signal_resolved oscclk{"oscclk", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_i_p{"clk_i_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkref_in{"clkref_in", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<2> gmp_cntrl_int{"gmp_cntrl_int", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_resolved clk_q_m{"clk_q_m", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vref_pmix{"vref_pmix", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved atb_f_m{"atb_f_m", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_q_p{"clk_q_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low{"tie_low", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pwron_n{"pwron_n", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<8> ck_l2h{"ck_l2h", vl::logic<8>::all_z().lv()};
    sc_core::sc_signal_resolved byp_ncntrldiv{"byp_ncntrldiv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1902{"net1902", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1903{"net1903", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved dn_lockdet{"dn_lockdet", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pwron_pmix_h{"pwron_pmix_h", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pmix_clk_sel{"pmix_clk_sel", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clksqr_on_h{"clksqr_on_h", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<9> mpll_ph_sel_wd_out{"mpll_ph_sel_wd_out", vl::logic<9>::all_z().lv()};
    sc_core::sc_signal_resolved clksqr_on{"clksqr_on", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rst_pmix_h{"rst_pmix_h", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pwron_pmix_n{"pwron_pmix_n", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<6> int_cntrl_int{"int_cntrl_int", vl::logic<6>::all_z().lv()};
    sc_core::sc_signal_resolved clkrefdiv{"clkrefdiv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vp_l2h{"vp_l2h", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved reset{"reset", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<2> clksel_int{"clksel_int", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_resolved pwron{"pwron", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<2> meas_iv_h{"meas_iv_h", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_resolved mpll_ph_sel_clk_out{"mpll_ph_sel_clk_out", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<7> cpbias_cntrl_int{"cpbias_cntrl_int", vl::logic<7>::all_z().lv()};
    sc_core::sc_signal_rv<10> m_obs_core_model{"m_obs_core_model", vl::logic<10>::all_z().lv()};
    std::unique_ptr<mipi_pll_dphy_DPHYPLL_dig_addon> dig_dphy;
    std::unique_ptr<mipi_pll_dphy_mpll_ana> ana;
    sc_core::sc_signal_rv<4> c_ana_int_cntrl{"c_ana_int_cntrl", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_rv<9> c_ana_mpll_ph_sel_wd{"c_ana_mpll_ph_sel_wd", vl::logic<9>::all_z().lv()};
    sc_core::sc_signal_rv<5> c_ana_prop_cntrl{"c_ana_prop_cntrl", vl::logic<5>::all_z().lv()};
    sc_core::sc_signal_rv<2> c_ana_ref_cntrl{"c_ana_ref_cntrl", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_rv<4> c_ana_ph_pmix{"c_ana_ph_pmix", vl::logic<4>::all_z().lv()};
    std::unique_ptr<mipi_pll_dphy_sc_tie_hilow_lvt> I211;

    vl::logic<4> rd_ph_pmix() const;
    void assign_m_obs_core_model();
    void glue_ana_int_cntrl();
    void glue_ana_mpll_ph_sel_wd();
    void glue_ana_prop_cntrl();
    void glue_ana_ref_cntrl();
    void split_ana_ph_pmix();
};

#endif
