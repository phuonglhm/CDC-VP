//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_outstage_divandmux_outstage
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_DPHYPLL_OUTSTAGE_DIVANDMUX_OUTSTAGE_H
#define VL_MIPI_PLL_DPHY_DPHYPLL_OUTSTAGE_DIVANDMUX_OUTSTAGE_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "pll_cells.h"

class mipi_pll_dphy_DPHYPLL_outstage_divandmux_outstage : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> dgnd{"dgnd"};
    sc_core::sc_in<sc_dt::sc_logic> dvdd{"dvdd"};
    sc_core::sc_in<sc_dt::sc_logic> input{"input"};
    sc_core::sc_out<sc_dt::sc_logic> output{"output"};
    sc_core::sc_inout<sc_dt::sc_logic> b2b{"b2b"};

    SC_HAS_PROCESS(mipi_pll_dphy_DPHYPLL_outstage_divandmux_outstage);
    explicit mipi_pll_dphy_DPHYPLL_outstage_divandmux_outstage(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved b{"b", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_schc_invx128_lvt> I92;
    std::unique_ptr<mipi_pll_dphy_schc_invx32_lvt> I93;
    std::unique_ptr<mipi_pll_dphy_schc_invx64_lvt> I91;
};

#endif
