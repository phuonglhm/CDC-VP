//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_clock_buffers_twolanes
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_DPHYPLL_CLOCK_BUFFERS_TWOLANES_H
#define VL_MIPI_PLL_DPHY_DPHYPLL_CLOCK_BUFFERS_TWOLANES_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "pll_cells.h"

class mipi_pll_dphy_DPHYPLL_clock_buffers_twolanes : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> agnd{"agnd"};
    sc_core::sc_in<sc_dt::sc_logic> avddlv{"avddlv"};
    sc_core::sc_in<sc_dt::sc_logic> tie_high{"tie_high"};
    sc_core::sc_in<sc_dt::sc_logic> on_pllclk{"on_pllclk"};
    sc_core::sc_in<sc_dt::sc_logic> clk_inp{"clk_inp"};
    sc_core::sc_in<sc_dt::sc_logic> clk_inm{"clk_inm"};
    sc_core::sc_out<sc_dt::sc_logic> clk_outp{"clk_outp"};
    sc_core::sc_out<sc_dt::sc_logic> clk_outm{"clk_outm"};

    SC_HAS_PROCESS(mipi_pll_dphy_DPHYPLL_clock_buffers_twolanes);
    explicit mipi_pll_dphy_DPHYPLL_clock_buffers_twolanes(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved bp{"bp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved am{"am", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ap{"ap", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved bm{"bm", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_sc_ck_tri_nax36_lvt_pwrdlow> TREEVPCLK;
    std::unique_ptr<mipi_pll_dphy_sc_invx192_lvt> I7;
    std::unique_ptr<mipi_pll_dphy_sc_invx192_lvt> I91;
    std::unique_ptr<mipi_pll_dphy_sc_invx64_lvt> I8;
    std::unique_ptr<mipi_pll_dphy_sc_invx64_lvt> I93;
};

#endif
