//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_DPHYPLL_H
#define VL_MIPI_PLL_DPHY_DPHYPLL_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_pll_dphy_DPHYPLL_no_clamp.h"

class mipi_pll_dphy_DPHYPLL : public sc_core::sc_module {
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
    sc_core::sc_in<sc_dt::sc_logic> atb_sense{"atb_sense"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_inout<sc_dt::sc_logic> vdd{"vdd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    sc_core::sc_in<sc_dt::sc_logic> vpl{"vpl"};
    sc_core::sc_inout<sc_dt::sc_logic> vss{"vss"};
    sc_core::sc_in<sc_dt::sc_logic> vbg{"vbg"};
    sc_core::sc_out<sc_dt::sc_logic> vreg_mpll{"vreg_mpll"};

    SC_HAS_PROCESS(mipi_pll_dphy_DPHYPLL);
    explicit mipi_pll_dphy_DPHYPLL(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved net2001{"net2001", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved power_aware_ok{"power_aware_ok", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkout0_left_int{"clkout0_left_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkout0_right_int{"clkout0_right_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkout180_left_int{"clkout180_left_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkout180_right_int{"clkout180_right_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkout270_left_int{"clkout270_left_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkout270_right_int{"clkout270_right_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkout90_left_int{"clkout90_left_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkout90_right_int{"clkout90_right_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkout_gp_int{"clkout_gp_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkout_stop_left_int{"clkout_stop_left_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkout_stop_right_int{"clkout_stop_right_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vpl_det_int{"vpl_det_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vreg_mpll_int{"vreg_mpll_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tiehigh{"tiehigh", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_DPHYPLL_no_clamp> I242;

    void assign_clkout0_left();
    void assign_clkout0_right();
    void assign_clkout180_left();
    void assign_clkout180_right();
    void assign_clkout270_left();
    void assign_clkout270_right();
    void assign_clkout90_left();
    void assign_clkout90_right();
    void assign_clkout_gp();
    void assign_clkout_stop_left();
    void assign_clkout_stop_right();
    void assign_vpl_det();
    void assign_vreg_mpll();
    void assign_tiehigh();
};

#endif
