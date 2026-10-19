//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_outstage_outmux_glitchless
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_DPHYPLL_OUTSTAGE_OUTMUX_GLITCHLESS_H
#define VL_MIPI_PLL_DPHY_DPHYPLL_OUTSTAGE_OUTMUX_GLITCHLESS_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_pll_dphy_sc_syncx10_as_lvt.h"
#include "pll_cells.h"

class mipi_pll_dphy_DPHYPLL_outstage_outmux_glitchless : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_lv<4>> ckin1{"ckin1"};
    sc_core::sc_in<sc_dt::sc_lv<4>> ckin2{"ckin2"};
    sc_core::sc_in<sc_dt::sc_logic> clk1_on{"clk1_on"};
    sc_core::sc_in<sc_dt::sc_logic> clk2_on{"clk2_on"};
    sc_core::sc_in<sc_dt::sc_logic> res1{"res1"};
    sc_core::sc_out<sc_dt::sc_lv<4>> clk_out{"clk_out"};
    sc_core::sc_out<sc_dt::sc_logic> clk_stop{"clk_stop"};
    sc_core::sc_out<sc_dt::sc_logic> mux_off{"mux_off"};
    sc_core::sc_in<sc_dt::sc_logic> dgnd{"dgnd"};
    sc_core::sc_in<sc_dt::sc_logic> dvdd{"dvdd"};

    SC_HAS_PROCESS(mipi_pll_dphy_DPHYPLL_outstage_outmux_glitchless);
    explicit mipi_pll_dphy_DPHYPLL_outstage_outmux_glitchless(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> ckin1_b{"ckin1_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> ckin2_b{"ckin2_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> clk_out_b{"clk_out_b", 4, vl::make_z1};
    sc_core::sc_signal_resolved aok{"aok", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved b4n{"b4n", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net230{"net230", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net231{"net231", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net232{"net232", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net233{"net233", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ckin2n{"ckin2n", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net234{"net234", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net235{"net235", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net236{"net236", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net237{"net237", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved b5n{"b5n", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net245{"net245", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net246{"net246", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved bok{"bok", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved a1{"a1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved a2{"a2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved a3{"a3", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved a4{"a4", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved a5{"a5", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved b1{"b1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved b2{"b2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved b3{"b3", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved b4{"b4", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ckin1n{"ckin1n", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved b5{"b5", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved a3n{"a3n", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved a4n{"a4n", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved b3n{"b3n", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved a5n{"a5n", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I55;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I56;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I58;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I71;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I72;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I73_1;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I73_0;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I74;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I75;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I76_1;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I76_0;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I77;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I78;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I79_1;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I79_0;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I80;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I81;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I82_1;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I82_0;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> I83;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> I84;
    std::unique_ptr<mipi_pll_dphy_sc_invx6_lvt> I50;
    std::unique_ptr<mipi_pll_dphy_sc_invx6_lvt> I51;
    std::unique_ptr<mipi_pll_dphy_sc_invx6_lvt> I52;
    std::unique_ptr<mipi_pll_dphy_sc_invx6_lvt> I63;
    std::unique_ptr<mipi_pll_dphy_sc_invx6_lvt> I68;
    std::unique_ptr<mipi_pll_dphy_sc_invx6_lvt> I70;
    std::unique_ptr<mipi_pll_dphy_sc_syncx10_as_lvt> I47;
    std::unique_ptr<mipi_pll_dphy_sc_syncx10_as_lvt> I48;
    std::unique_ptr<mipi_pll_dphy_sc_syncx10_as_lvt> I49;
    std::unique_ptr<mipi_pll_dphy_sc_syncx10_as_lvt> I62;
    std::unique_ptr<mipi_pll_dphy_sc_syncx10_as_lvt> I65;
    std::unique_ptr<mipi_pll_dphy_sc_syncx10_as_lvt> I66;
    std::unique_ptr<mipi_pll_dphy_sc_syncx10_as_lvt> I67;
    std::unique_ptr<mipi_pll_dphy_sc_syncx10_as_lvt> I69;
    std::unique_ptr<mipi_pll_dphy_sc_syncx10_as_lvt> d1;
    std::unique_ptr<mipi_pll_dphy_sc_syncx10_as_lvt> d2;
    std::unique_ptr<mipi_pll_dphy_sc_na2x12_lvt> I53;
    std::unique_ptr<mipi_pll_dphy_sc_na2x12_lvt> I64;
    std::unique_ptr<mipi_pll_dphy_sc_na2x12_lvt> na1;

    void split_ckin1();
    void split_ckin2();
    void merge_clk_out();
    vl::logic<4> rd_ckin1() const;
    vl::logic<4> rd_ckin2() const;
    vl::logic<4> rd_clk_out() const;
};

#endif
