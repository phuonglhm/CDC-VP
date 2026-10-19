//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_mpll_cdm_prot_clk
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_MPLL_CDM_PROT_CLK_H
#define VL_MIPI_PLL_DPHY_MPLL_CDM_PROT_CLK_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_pll_dphy_mpll_nandinv_1.h"
#include "pll_cells.h"

class mipi_pll_dphy_mpll_cdm_prot_clk : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> ckref{"ckref"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> oscclk{"oscclk"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_out<sc_dt::sc_logic> ckref_prot{"ckref_prot"};
    sc_core::sc_out<sc_dt::sc_logic> oscclk_prot{"oscclk_prot"};

    SC_HAS_PROCESS(mipi_pll_dphy_mpll_cdm_prot_clk);
    explicit mipi_pll_dphy_mpll_cdm_prot_clk(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved tie_high_cdm{"tie_high_cdm", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low_cdm{"tie_low_cdm", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_LEVELCONV_l2l_cdm_tie_LowHigh> TIE_CDM;
    std::unique_ptr<mipi_pll_dphy_mpll_nandinv_1> I194;
    std::unique_ptr<mipi_pll_dphy_mpll_nandinv_1> I195;
};

#endif
