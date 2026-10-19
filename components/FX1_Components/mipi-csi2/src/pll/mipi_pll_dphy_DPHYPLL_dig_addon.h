//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_dig_addon
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_DPHYPLL_DIG_ADDON_H
#define VL_MIPI_PLL_DPHY_DPHYPLL_DIG_ADDON_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_pll_dphy_DPHYPLL_clock_buffers_4lane.h"
#include "mipi_pll_dphy_DPHYPLL_dig_addon_clkselen.h"
#include "mipi_pll_dphy_DPHYPLL_dig_addon_gppath.h"
#include "mipi_pll_dphy_DPHYPLL_fd.h"
#include "mipi_pll_dphy_DPHYPLL_outstage_divandmux.h"
#include "mipi_pll_dphy_DPHYPLL_outstage_divandmux_outstage.h"
#include "mipi_pll_dphy_dphyplldig.h"
#include "mipi_pll_dphy_mpll_nandinv_1.h"
#include "pll_cells.h"

class mipi_pll_dphy_DPHYPLL_dig_addon : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clkdcc0{"clkdcc0"};
    sc_core::sc_in<sc_dt::sc_logic> clkdcc180{"clkdcc180"};
    sc_core::sc_in<sc_dt::sc_logic> clkdcc270{"clkdcc270"};
    sc_core::sc_in<sc_dt::sc_logic> clkdcc90{"clkdcc90"};
    sc_core::sc_in<sc_dt::sc_logic> clkext{"clkext"};
    sc_core::sc_in<sc_dt::sc_logic> clkin{"clkin"};
    sc_core::sc_in<sc_dt::sc_logic> clkouten_left{"clkouten_left"};
    sc_core::sc_in<sc_dt::sc_logic> clkouten_right{"clkouten_right"};
    sc_core::sc_in<sc_dt::sc_lv<2>> clksel{"clksel"};
    sc_core::sc_in<sc_dt::sc_logic> clksel_en{"clksel_en"};
    sc_core::sc_in<sc_dt::sc_logic> clkvco_gp{"clkvco_gp"};
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
    sc_core::sc_in<sc_dt::sc_lv<4>> n{"n"};
    sc_core::sc_in<sc_dt::sc_logic> onpll{"onpll"};
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
    sc_core::sc_in<sc_dt::sc_logic> vcoclk{"vcoclk"};
    sc_core::sc_in<sc_dt::sc_logic> vcoclk_after_dcc{"vcoclk_after_dcc"};
    sc_core::sc_out<sc_dt::sc_logic> clkfb{"clkfb"};
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
    sc_core::sc_out<sc_dt::sc_logic> clkref{"clkref"};
    sc_core::sc_out<sc_dt::sc_lv<2>> clksel_int{"clksel_int"};
    sc_core::sc_out<sc_dt::sc_lv<7>> cpbias_cntrl_int{"cpbias_cntrl_int"};
    sc_core::sc_out<sc_dt::sc_lv<7>> cpbias_cntrl_obs{"cpbias_cntrl_obs"};
    sc_core::sc_out<sc_dt::sc_lv<2>> gmp_cntrl_int{"gmp_cntrl_int"};
    sc_core::sc_out<sc_dt::sc_lv<2>> gmp_cntrl_obs{"gmp_cntrl_obs"};
    sc_core::sc_out<sc_dt::sc_lv<6>> int_cntrl_int{"int_cntrl_int"};
    sc_core::sc_out<sc_dt::sc_lv<6>> int_cntrl_obs{"int_cntrl_obs"};
    sc_core::sc_out<sc_dt::sc_logic> lock{"lock"};
    sc_core::sc_out<sc_dt::sc_logic> lock_phy{"lock_phy"};
    sc_core::sc_out<sc_dt::sc_lv<10>> m_obs{"m_obs"};
    sc_core::sc_out<sc_dt::sc_lv<4>> n_obs{"n_obs"};
    sc_core::sc_out<sc_dt::sc_lv<2>> phy_in_clksel{"phy_in_clksel"};
    sc_core::sc_out<sc_dt::sc_logic> pll_shadow_control_obs{"pll_shadow_control_obs"};
    sc_core::sc_out<sc_dt::sc_lv<6>> prop_cntrl_int{"prop_cntrl_int"};
    sc_core::sc_out<sc_dt::sc_lv<6>> prop_cntrl_obs{"prop_cntrl_obs"};
    sc_core::sc_out<sc_dt::sc_logic> scanout{"scanout"};
    sc_core::sc_out<sc_dt::sc_logic> testlock{"testlock"};
    sc_core::sc_out<sc_dt::sc_lv<4>> vco_cntrl_int{"vco_cntrl_int"};
    sc_core::sc_out<sc_dt::sc_lv<6>> vco_cntrl_obs{"vco_cntrl_obs"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_inout<sc_dt::sc_logic> vdd{"vdd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    sc_core::sc_in<sc_dt::sc_logic> vreg_mpll{"vreg_mpll"};
    sc_core::sc_inout<sc_dt::sc_logic> vss{"vss"};

    SC_HAS_PROCESS(mipi_pll_dphy_DPHYPLL_dig_addon);
    explicit mipi_pll_dphy_DPHYPLL_dig_addon(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> clksel_int_b{"clksel_int_b", 2, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> vco_cntrl_int_b{"vco_cntrl_int_b", 4, vl::make_z1};
    sc_core::sc_signal_resolved net704{"net704", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkout180{"clkout180", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low_cdm{"tie_low_cdm", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkout270{"clkout270", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkouten_left_prot{"clkouten_left_prot", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<2> clksel_int_output{"clksel_int_output", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_resolved clkout_gp_pre{"clkout_gp_pre", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> clksel_int_avdd{"clksel_int_avdd", 2, vl::make_z1};
    sc_core::sc_signal_resolved onpllz_avdd{"onpllz_avdd", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clksel_en_avdd{"clksel_en_avdd", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved m_intx0x{"m_intx0x", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkout0{"clkout0", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_high_cdm{"tie_high_cdm", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vco_cntrl_int_vdd4{"vco_cntrl_int_vdd4", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vco_cntrl_int_vdd5{"vco_cntrl_int_vdd5", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> vco_cntrl_avdd{"vco_cntrl_avdd", 2, vl::make_z1};
    sc_core::sc_signal_resolved clkext_prot{"clkext_prot", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved onpll_avdd{"onpll_avdd", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved gp_clk_en_prot{"gp_clk_en_prot", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkout90{"clkout90", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net700{"net700", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkouten_right_prot{"clkouten_right_prot", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkout_stop{"clkout_stop", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_DPHYPLL_fd> I1;
    std::unique_ptr<mipi_pll_dphy_DPHYPLL_dig_addon_clkselen> I118;
    sc_core::sc_signal_rv<2> c_I118_clksel_int_avdd{"c_I118_clksel_int_avdd", vl::logic<2>::all_z().lv()};
    std::unique_ptr<mipi_pll_dphy_DPHYPLL_outstage_divandmux_outstage> I75;
    std::unique_ptr<mipi_pll_dphy_LEVELCONV_l2l_cdm_tie_LowHigh> TIE_CDM;
    std::unique_ptr<mipi_pll_dphy_DPHYPLL_outstage_divandmux> I50;
    sc_core::sc_signal_rv<2> c_I50_p_int{"c_I50_p_int", vl::logic<2>::all_z().lv()};
    std::unique_ptr<mipi_pll_dphy_DPHYPLL_dig_addon_gppath> I112;
    std::unique_ptr<mipi_pll_dphy_dphyplldig> I0;
    sc_core::sc_signal_rv<6> c_I0_vco_cntrl_int{"c_I0_vco_cntrl_int", vl::logic<6>::all_z().lv()};
    sc_core::sc_signal_rv<2> c_I0_clksel_int{"c_I0_clksel_int", vl::logic<2>::all_z().lv()};
    std::unique_ptr<mipi_pll_dphy_mpll_nandinv_1> I117_5;
    std::unique_ptr<mipi_pll_dphy_mpll_nandinv_1> I117_4;
    std::unique_ptr<mipi_pll_dphy_mpll_nandinv_1> I117_3;
    std::unique_ptr<mipi_pll_dphy_mpll_nandinv_1> I117_2;
    std::unique_ptr<mipi_pll_dphy_mpll_nandinv_1> I117_1;
    std::unique_ptr<mipi_pll_dphy_mpll_nandinv_1> I117_0;
    std::unique_ptr<mipi_pll_dphy_mpll_nandinv_1> I127_3;
    std::unique_ptr<mipi_pll_dphy_mpll_nandinv_1> I127_2;
    std::unique_ptr<mipi_pll_dphy_mpll_nandinv_1> I127_1;
    std::unique_ptr<mipi_pll_dphy_mpll_nandinv_1> I127_0;
    std::unique_ptr<mipi_pll_dphy_DPHYPLL_clock_buffers_4lane> I147;
    std::unique_ptr<mipi_pll_dphy_DPHYPLL_clock_buffers_4lane> I148;

    void merge_clksel_int();
    void merge_vco_cntrl_int();
    vl::logic<2> rd_clksel_int() const;
    vl::logic<2> rd_clksel_int_avdd() const;
    vl::logic<2> rd_vco_cntrl_avdd() const;
    vl::logic<4> rd_vco_cntrl_int() const;
    void glue_I118_clksel_int_avdd();
    void glue_I50_p_int();
    void split_I0_vco_cntrl_int();
    void split_I0_clksel_int();
};

#endif
