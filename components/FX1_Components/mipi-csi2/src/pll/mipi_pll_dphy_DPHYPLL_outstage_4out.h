//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_outstage_4out
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_DPHYPLL_OUTSTAGE_4OUT_H
#define VL_MIPI_PLL_DPHY_DPHYPLL_OUTSTAGE_4OUT_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_pll_dphy_DPHYPLL_outstage_divandmux_outstage.h"
#include "mipi_pll_dphy_DPHYPLL_outstage_outmux_glitchless.h"
#include "pll_cells.h"

class mipi_pll_dphy_DPHYPLL_outstage_4out : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clkdcc0{"clkdcc0"};
    sc_core::sc_in<sc_dt::sc_logic> clkdcc1{"clkdcc1"};
    sc_core::sc_in<sc_dt::sc_logic> clkdcc2{"clkdcc2"};
    sc_core::sc_in<sc_dt::sc_logic> clkdcc3{"clkdcc3"};
    sc_core::sc_in<sc_dt::sc_logic> clkext0{"clkext0"};
    sc_core::sc_in<sc_dt::sc_logic> clkext1{"clkext1"};
    sc_core::sc_in<sc_dt::sc_logic> clkext2{"clkext2"};
    sc_core::sc_in<sc_dt::sc_logic> clkext3{"clkext3"};
    sc_core::sc_in<sc_dt::sc_lv<2>> clksel{"clksel"};
    sc_core::sc_in<sc_dt::sc_logic> onlvz{"onlvz"};
    sc_core::sc_out<sc_dt::sc_logic> clk_stop{"clk_stop"};
    sc_core::sc_out<sc_dt::sc_logic> clkout0{"clkout0"};
    sc_core::sc_out<sc_dt::sc_logic> clkout1{"clkout1"};
    sc_core::sc_out<sc_dt::sc_logic> clkout2{"clkout2"};
    sc_core::sc_out<sc_dt::sc_logic> clkout3{"clkout3"};
    sc_core::sc_out<sc_dt::sc_logic> mux_off{"mux_off"};
    sc_core::sc_in<sc_dt::sc_logic> dgnd{"dgnd"};
    sc_core::sc_in<sc_dt::sc_logic> dvdd{"dvdd"};

    SC_HAS_PROCESS(mipi_pll_dphy_DPHYPLL_outstage_4out);
    explicit mipi_pll_dphy_DPHYPLL_outstage_4out(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> clksel_b{"clksel_b", 2, vl::make_z1};
    sc_core::sc_signal_resolved net262{"net262", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cksel0rst{"cksel0rst", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved en_b2b{"en_b2b", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved en_b2b_z{"en_b2b_z", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net388{"net388", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net390{"net390", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net389{"net389", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkout_mux0{"clkout_mux0", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net391{"net391", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkout_mux1{"clkout_mux1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cksel1rst{"cksel1rst", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkout_mux2{"clkout_mux2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkout_mux3{"clkout_mux3", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net259{"net259", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt_highz> I71;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt_highz> I84;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt_highz> I110;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt_highz> I111;
    std::unique_ptr<mipi_pll_dphy_DPHYPLL_outstage_outmux_glitchless> mux_out;
    sc_core::sc_signal_rv<4> c_mux_out_ckin1{"c_mux_out_ckin1", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_rv<4> c_mux_out_ckin2{"c_mux_out_ckin2", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_rv<4> c_mux_out_clk_out{"c_mux_out_clk_out", vl::logic<4>::all_z().lv()};
    std::unique_ptr<mipi_pll_dphy_sc_invx24_lvt> I113;
    std::unique_ptr<mipi_pll_dphy_sc_invx24_lvt> inv_1;
    std::unique_ptr<mipi_pll_dphy_sc_invx24_lvt> inv_2;
    std::unique_ptr<mipi_pll_dphy_DPHYPLL_outstage_divandmux_outstage> I102;
    sc_core::sc_signal_resolved nc_I102_b2b{"nc_I102_b2b", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_DPHYPLL_outstage_divandmux_outstage> I103;
    sc_core::sc_signal_resolved nc_I103_b2b{"nc_I103_b2b", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_DPHYPLL_outstage_divandmux_outstage> I104;
    sc_core::sc_signal_resolved nc_I104_b2b{"nc_I104_b2b", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_DPHYPLL_outstage_divandmux_outstage> I105;
    sc_core::sc_signal_resolved nc_I105_b2b{"nc_I105_b2b", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I112;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> nor_1;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> nor_2;

    void split_clksel();
    vl::logic<2> rd_clksel() const;
    void glue_mux_out_ckin1();
    void glue_mux_out_ckin2();
    void split_mux_out_clk_out();
};

#endif
