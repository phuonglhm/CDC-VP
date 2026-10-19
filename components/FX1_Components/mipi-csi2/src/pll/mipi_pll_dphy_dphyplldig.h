//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_dphyplldig
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_DPHYPLLDIG_H
#define VL_MIPI_PLL_DPHY_DPHYPLLDIG_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "pll_cells.h"

class mipi_pll_dphy_dphyplldig : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_lv<4>> n{"n"};
    sc_core::sc_in<sc_dt::sc_lv<10>> m{"m"};
    sc_core::sc_in<sc_dt::sc_lv<10>> th1{"th1"};
    sc_core::sc_in<sc_dt::sc_lv<8>> th2{"th2"};
    sc_core::sc_in<sc_dt::sc_lv<8>> th3{"th3"};
    sc_core::sc_in<sc_dt::sc_lv<3>> tstplldig{"tstplldig"};
    sc_core::sc_in<sc_dt::sc_lv<2>> clksel{"clksel"};
    sc_core::sc_in<sc_dt::sc_lv<6>> th_delay{"th_delay"};
    sc_core::sc_in<sc_dt::sc_lv<6>> phy_prop_cntrl{"phy_prop_cntrl"};
    sc_core::sc_in<sc_dt::sc_lv<6>> phy_int_cntrl{"phy_int_cntrl"};
    sc_core::sc_in<sc_dt::sc_lv<6>> phy_vco_cntrl{"phy_vco_cntrl"};
    sc_core::sc_in<sc_dt::sc_lv<7>> phy_cpbias_cntrl{"phy_cpbias_cntrl"};
    sc_core::sc_in<sc_dt::sc_lv<2>> phy_gmp_cntrl{"phy_gmp_cntrl"};
    sc_core::sc_in<sc_dt::sc_lv<4>> phy_n{"phy_n"};
    sc_core::sc_in<sc_dt::sc_lv<10>> phy_m{"phy_m"};
    sc_core::sc_out<sc_dt::sc_lv<6>> int_cntrl_int{"int_cntrl_int"};
    sc_core::sc_out<sc_dt::sc_lv<2>> phy_in_clksel{"phy_in_clksel"};
    sc_core::sc_out<sc_dt::sc_lv<6>> prop_cntrl_int{"prop_cntrl_int"};
    sc_core::sc_out<sc_dt::sc_lv<2>> gmp_cntrl_int{"gmp_cntrl_int"};
    sc_core::sc_out<sc_dt::sc_lv<6>> vco_cntrl_int{"vco_cntrl_int"};
    sc_core::sc_out<sc_dt::sc_lv<2>> clksel_int{"clksel_int"};
    sc_core::sc_out<sc_dt::sc_lv<7>> cpbias_cntrl_int{"cpbias_cntrl_int"};
    sc_core::sc_out<sc_dt::sc_lv<10>> m_obs{"m_obs"};
    sc_core::sc_out<sc_dt::sc_lv<4>> n_obs{"n_obs"};
    sc_core::sc_out<sc_dt::sc_lv<6>> int_cntrl_obs{"int_cntrl_obs"};
    sc_core::sc_out<sc_dt::sc_lv<6>> prop_cntrl_obs{"prop_cntrl_obs"};
    sc_core::sc_out<sc_dt::sc_lv<6>> vco_cntrl_obs{"vco_cntrl_obs"};
    sc_core::sc_out<sc_dt::sc_lv<7>> cpbias_cntrl_obs{"cpbias_cntrl_obs"};
    sc_core::sc_out<sc_dt::sc_lv<2>> gmp_cntrl_obs{"gmp_cntrl_obs"};
    sc_core::sc_in<sc_dt::sc_lv<6>> int_cntrl{"int_cntrl"};
    sc_core::sc_in<sc_dt::sc_lv<6>> vco_cntrl{"vco_cntrl"};
    sc_core::sc_in<sc_dt::sc_lv<6>> prop_cntrl{"prop_cntrl"};
    sc_core::sc_in<sc_dt::sc_lv<7>> cpbias_cntrl{"cpbias_cntrl"};
    sc_core::sc_in<sc_dt::sc_lv<2>> gmp_cntrl{"gmp_cntrl"};
    sc_core::sc_in<sc_dt::sc_logic> onpll{"onpll"};
    sc_core::sc_in<sc_dt::sc_logic> vcoclk{"vcoclk"};
    sc_core::sc_in<sc_dt::sc_logic> clkin{"clkin"};
    sc_core::sc_in<sc_dt::sc_logic> lock_det_on{"lock_det_on"};
    sc_core::sc_in<sc_dt::sc_logic> lockdet_mode{"lockdet_mode"};
    sc_core::sc_in<sc_dt::sc_logic> force_lock_phy{"force_lock_phy"};
    sc_core::sc_in<sc_dt::sc_logic> lock_sel{"lock_sel"};
    sc_core::sc_in<sc_dt::sc_logic> shadow_clear{"shadow_clear"};
    sc_core::sc_in<sc_dt::sc_logic> updatepll{"updatepll"};
    sc_core::sc_in<sc_dt::sc_logic> pll_shadow_control{"pll_shadow_control"};
    sc_core::sc_in<sc_dt::sc_logic> force_lock{"force_lock"};
    sc_core::sc_out<sc_dt::sc_logic> testlock{"testlock"};
    sc_core::sc_out<sc_dt::sc_logic> lock{"lock"};
    sc_core::sc_in<sc_dt::sc_logic> scanluctrl{"scanluctrl"};
    sc_core::sc_in<sc_dt::sc_logic> scanrstz{"scanrstz"};
    sc_core::sc_in<sc_dt::sc_logic> scanclk{"scanclk"};
    sc_core::sc_in<sc_dt::sc_logic> scanen{"scanen"};
    sc_core::sc_in<sc_dt::sc_logic> scanin{"scanin"};
    sc_core::sc_in<sc_dt::sc_logic> scanmode{"scanmode"};
    sc_core::sc_inout<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_inout<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_out<sc_dt::sc_logic> scanout{"scanout"};
    sc_core::sc_out<sc_dt::sc_logic> m_intx0x{"m_intx0x"};
    sc_core::sc_out<sc_dt::sc_logic> lock_phy{"lock_phy"};
    sc_core::sc_out<sc_dt::sc_logic> pll_shadow_control_obs{"pll_shadow_control_obs"};
    sc_core::sc_out<sc_dt::sc_logic> clkfb{"clkfb"};
    sc_core::sc_out<sc_dt::sc_logic> refclk{"refclk"};

    SC_HAS_PROCESS(mipi_pll_dphy_dphyplldig);
    explicit mipi_pll_dphy_dphyplldig(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    std::vector<std::unique_ptr<sc_core::sc_signal_resolved>> n_;   // bit-level nets
    std::vector<std::unique_ptr<sc_core::sc_module>> owned_;       // netlist instances

    void split_n();
    void split_m();
    void split_th1();
    void split_th2();
    void split_th3();
    void split_tstplldig();
    void split_clksel();
    void split_th_delay();
    void split_phy_prop_cntrl();
    void split_phy_int_cntrl();
    void split_phy_vco_cntrl();
    void split_phy_cpbias_cntrl();
    void split_phy_gmp_cntrl();
    void split_phy_n();
    void split_phy_m();
    void merge_int_cntrl_int();
    void merge_phy_in_clksel();
    void merge_prop_cntrl_int();
    void merge_gmp_cntrl_int();
    void merge_vco_cntrl_int();
    void merge_clksel_int();
    void merge_cpbias_cntrl_int();
    void merge_m_obs();
    void merge_n_obs();
    void merge_int_cntrl_obs();
    void merge_prop_cntrl_obs();
    void merge_vco_cntrl_obs();
    void merge_cpbias_cntrl_obs();
    void merge_gmp_cntrl_obs();
    void split_int_cntrl();
    void split_vco_cntrl();
    void split_prop_cntrl();
    void split_cpbias_cntrl();
    void split_gmp_cntrl();
    vl::logic<2> rd_clksel() const;
    vl::logic<2> rd_clksel_int() const;
    vl::logic<7> rd_cpbias_cntrl() const;
    vl::logic<7> rd_cpbias_cntrl_int() const;
    vl::logic<7> rd_cpbias_cntrl_obs() const;
    vl::logic<2> rd_gmp_cntrl() const;
    vl::logic<2> rd_gmp_cntrl_int() const;
    vl::logic<2> rd_gmp_cntrl_obs() const;
    vl::logic<6> rd_int_cntrl() const;
    vl::logic<6> rd_int_cntrl_int() const;
    vl::logic<6> rd_int_cntrl_obs() const;
    vl::logic<10> rd_m() const;
    vl::logic<10> rd_m_obs() const;
    vl::logic<4> rd_n() const;
    vl::logic<4> rd_n_obs() const;
    vl::logic<7> rd_phy_cpbias_cntrl() const;
    vl::logic<2> rd_phy_gmp_cntrl() const;
    vl::logic<2> rd_phy_in_clksel() const;
    vl::logic<6> rd_phy_int_cntrl() const;
    vl::logic<10> rd_phy_m() const;
    vl::logic<4> rd_phy_n() const;
    vl::logic<6> rd_phy_prop_cntrl() const;
    vl::logic<6> rd_phy_vco_cntrl() const;
    vl::logic<6> rd_prop_cntrl() const;
    vl::logic<6> rd_prop_cntrl_int() const;
    vl::logic<6> rd_prop_cntrl_obs() const;
    vl::logic<10> rd_th1() const;
    vl::logic<8> rd_th2() const;
    vl::logic<8> rd_th3() const;
    vl::logic<6> rd_th_delay() const;
    vl::logic<3> rd_tstplldig() const;
    vl::logic<6> rd_vco_cntrl() const;
    vl::logic<6> rd_vco_cntrl_int() const;
    vl::logic<6> rd_vco_cntrl_obs() const;
    void assign_VSS();
    void assign_VDD();
    void build_0();
    void build_1();
    void build_2();
};

#endif
