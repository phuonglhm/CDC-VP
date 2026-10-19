//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_dig_addon_gppath
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_DPHYPLL_DIG_ADDON_GPPATH_H
#define VL_MIPI_PLL_DPHY_DPHYPLL_DIG_ADDON_GPPATH_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_pll_dphy_sc_syncx10_as_lvt.h"
#include "pll_cells.h"

class mipi_pll_dphy_DPHYPLL_dig_addon_gppath : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clkvco_gp{"clkvco_gp"};
    sc_core::sc_in<sc_dt::sc_logic> gp_clk_en{"gp_clk_en"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low_cdm{"tie_low_cdm"};
    sc_core::sc_out<sc_dt::sc_logic> clkout_gp{"clkout_gp"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vpl{"vpl"};

    SC_HAS_PROCESS(mipi_pll_dphy_DPHYPLL_dig_addon_gppath);
    explicit mipi_pll_dphy_DPHYPLL_dig_addon_gppath(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved gp_clk_enz{"gp_clk_enz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net11{"net11", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkvco_gp_div{"clkvco_gp_div", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_sc_invx12_lvt> I97;
    std::unique_ptr<mipi_pll_dphy_sc_syncx10_as_lvt> I48;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> CDM_OUT;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> INV_CDM;
};

#endif
