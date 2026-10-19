//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_mpll_phdet
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_MPLL_PHDET_H
#define VL_MIPI_PLL_DPHY_MPLL_PHDET_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "pll_cells.h"

class mipi_pll_dphy_mpll_phdet : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> oscclk{"oscclk"};
    sc_core::sc_in<sc_dt::sc_logic> refclk{"refclk"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_out<sc_dt::sc_logic> dec_freq{"dec_freq"};
    sc_core::sc_out<sc_dt::sc_logic> dec_freqn{"dec_freqn"};
    sc_core::sc_out<sc_dt::sc_logic> inc_freq{"inc_freq"};
    sc_core::sc_out<sc_dt::sc_logic> inc_freqn{"inc_freqn"};

    SC_HAS_PROCESS(mipi_pll_dphy_mpll_phdet);
    explicit mipi_pll_dphy_mpll_phdet(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved net261{"net261", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net280{"net280", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net281{"net281", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved r1{"r1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved dnb{"dnb", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net283{"net283", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net347{"net347", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved r4{"r4", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved upb{"upb", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved s_up{"s_up", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net332{"net332", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net286{"net286", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net287{"net287", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ref_up{"ref_up", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net288{"net288", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net336{"net336", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_dn{"clk_dn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved s_dn{"s_dn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved resetb{"resetb", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net158{"net158", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net294{"net294", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net323{"net323", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_sc_invx32_lvt> I32;
    std::unique_ptr<mipi_pll_dphy_sc_invx24_lvt> I12;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> I28;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> I29;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> I30;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> I31;
    std::unique_ptr<mipi_pll_dphy_sc_na2x12_lvt> I0;
    std::unique_ptr<mipi_pll_dphy_sc_na2x12_lvt> I1;
    std::unique_ptr<mipi_pll_dphy_sc_na2x12_lvt> I14;
    std::unique_ptr<mipi_pll_dphy_sc_na2x12_lvt> I21;
    std::unique_ptr<mipi_pll_dphy_sc_na2x12_lvt> I38;
    std::unique_ptr<mipi_pll_dphy_sc_na2x12_lvt> I40;
    std::unique_ptr<mipi_pll_dphy_sc_na2x12_lvt> I41;
    std::unique_ptr<mipi_pll_dphy_sc_na2x12_lvt> I42;
    std::unique_ptr<mipi_pll_dphy_sc_na4x6_lvt> I10;
    std::unique_ptr<mipi_pll_dphy_wire_cap> I4;
    std::unique_ptr<mipi_pll_dphy_wire_cap> I5;
    std::unique_ptr<mipi_pll_dphy_wire_cap> I18;
    std::unique_ptr<mipi_pll_dphy_wire_cap> I20;
    std::unique_ptr<mipi_pll_dphy_wire_cap> I24;
    std::unique_ptr<mipi_pll_dphy_wire_cap> I25;
    std::unique_ptr<mipi_pll_dphy_wire_cap> I26;
    std::unique_ptr<mipi_pll_dphy_wire_cap> I36;
    std::unique_ptr<mipi_pll_dphy_wire_cap> I43;
    std::unique_ptr<mipi_pll_dphy_wire_cap> I44;
    std::unique_ptr<mipi_pll_dphy_wire_cap> I45;
    std::unique_ptr<mipi_pll_dphy_wire_cap> I46;
    std::unique_ptr<mipi_pll_dphy_wire_cap> I47;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I15;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I16;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I19;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I22;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I23;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I27;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I35;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I37;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I48;
    std::unique_ptr<mipi_pll_dphy_sc_na3x16_lvt> I17;
    std::unique_ptr<mipi_pll_dphy_sc_na3x16_lvt> I39;
};

#endif
