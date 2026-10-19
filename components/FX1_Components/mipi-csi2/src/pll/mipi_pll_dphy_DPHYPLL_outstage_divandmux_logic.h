//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_outstage_divandmux_logic
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_DPHYPLL_OUTSTAGE_DIVANDMUX_LOGIC_H
#define VL_MIPI_PLL_DPHY_DPHYPLL_OUTSTAGE_DIVANDMUX_LOGIC_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "pll_cells.h"

class mipi_pll_dphy_DPHYPLL_outstage_divandmux_logic : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_lv<2>> clksel{"clksel"};
    sc_core::sc_in<sc_dt::sc_logic> onlvz{"onlvz"};
    sc_core::sc_in<sc_dt::sc_lv<2>> p_int{"p_int"};
    sc_core::sc_out<sc_dt::sc_lv<2>> clksel_int{"clksel_int"};
    sc_core::sc_out<sc_dt::sc_logic> onlvz_buf{"onlvz_buf"};
    sc_core::sc_in<sc_dt::sc_logic> agnd{"agnd"};
    sc_core::sc_in<sc_dt::sc_logic> avddlv{"avddlv"};

    SC_HAS_PROCESS(mipi_pll_dphy_DPHYPLL_outstage_divandmux_logic);
    explicit mipi_pll_dphy_DPHYPLL_outstage_divandmux_logic(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> clksel_b{"clksel_b", 2, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> p_int_b{"p_int_b", 2, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> clksel_int_b{"clksel_int_b", 2, vl::make_z1};
    sc_core::sc_signal_resolved net9{"net9", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clksel_not{"clksel_not", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vco_direct{"vco_direct", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net10{"net10", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1{"net1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net2{"net2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net11{"net11", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net3{"net3", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net12{"net12", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net4{"net4", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net13{"net13", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net5{"net5", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net14{"net14", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net6{"net6", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net7{"net7", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved negnow{"negnow", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net8{"net8", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_sc_na2x12_lvt> I40;
    std::unique_ptr<mipi_pll_dphy_sc_na2x12_lvt> I52;
    std::unique_ptr<mipi_pll_dphy_sc_na2x12_lvt> I53;
    std::unique_ptr<mipi_pll_dphy_sc_na2x12_lvt> I54;
    std::unique_ptr<mipi_pll_dphy_sc_na2x12_lvt> I55;
    std::unique_ptr<mipi_pll_dphy_sc_na2x12_lvt> I57;
    std::unique_ptr<mipi_pll_dphy_sc_na2x12_lvt> I98;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I33;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I39;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I38;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I50;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I51;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I56;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I58;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I59;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I94;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I100;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I101;
    std::unique_ptr<mipi_pll_dphy_sc_mux2x8_lvt> I43;
    std::unique_ptr<mipi_pll_dphy_sc_mux2x8_lvt> I44;

    void split_clksel();
    void split_p_int();
    void merge_clksel_int();
    vl::logic<2> rd_clksel() const;
    vl::logic<2> rd_clksel_int() const;
    vl::logic<2> rd_p_int() const;
};

#endif
