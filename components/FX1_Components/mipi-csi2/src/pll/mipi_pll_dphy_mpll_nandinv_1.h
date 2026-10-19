//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_mpll_nandinv_1
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_MPLL_NANDINV_1_H
#define VL_MIPI_PLL_DPHY_MPLL_NANDINV_1_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "pll_cells.h"

class mipi_pll_dphy_mpll_nandinv_1 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> input{"input"};
    sc_core::sc_in<sc_dt::sc_logic> tie_high_cdm{"tie_high_cdm"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low_cdm{"tie_low_cdm"};
    sc_core::sc_out<sc_dt::sc_logic> output{"output"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_pll_dphy_mpll_nandinv_1);
    explicit mipi_pll_dphy_mpll_nandinv_1(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved net2{"net2", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_sc_invx12_lvt> I183;
    std::unique_ptr<mipi_pll_dphy_LEVELCONV_cdm_na3x6_lvt> I184;
};

#endif
