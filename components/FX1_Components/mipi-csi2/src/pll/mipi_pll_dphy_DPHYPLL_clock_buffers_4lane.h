//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_clock_buffers_4lane
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_DPHYPLL_CLOCK_BUFFERS_4LANE_H
#define VL_MIPI_PLL_DPHY_DPHYPLL_CLOCK_BUFFERS_4LANE_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_pll_dphy_DPHYPLL_clock_buffers_twolanes.h"
#include "pll_cells.h"

class mipi_pll_dphy_DPHYPLL_clock_buffers_4lane : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk_in0{"clk_in0"};
    sc_core::sc_in<sc_dt::sc_logic> clk_in180{"clk_in180"};
    sc_core::sc_in<sc_dt::sc_logic> clk_in270{"clk_in270"};
    sc_core::sc_in<sc_dt::sc_logic> clk_in90{"clk_in90"};
    sc_core::sc_in<sc_dt::sc_logic> clkstop_in{"clkstop_in"};
    sc_core::sc_in<sc_dt::sc_logic> enable{"enable"};
    sc_core::sc_in<sc_dt::sc_logic> tie_high{"tie_high"};
    sc_core::sc_out<sc_dt::sc_logic> clk_out0{"clk_out0"};
    sc_core::sc_out<sc_dt::sc_logic> clk_out180{"clk_out180"};
    sc_core::sc_out<sc_dt::sc_logic> clk_out270{"clk_out270"};
    sc_core::sc_out<sc_dt::sc_logic> clk_out90{"clk_out90"};
    sc_core::sc_out<sc_dt::sc_logic> clkstop_out{"clkstop_out"};
    sc_core::sc_in<sc_dt::sc_logic> agnd{"agnd"};
    sc_core::sc_in<sc_dt::sc_logic> avddlv{"avddlv"};

    SC_HAS_PROCESS(mipi_pll_dphy_DPHYPLL_clock_buffers_4lane);
    explicit mipi_pll_dphy_DPHYPLL_clock_buffers_4lane(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved net56{"net56", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved bs{"bs", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved as{"as", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_DPHYPLL_clock_buffers_twolanes> I139;
    std::unique_ptr<mipi_pll_dphy_DPHYPLL_clock_buffers_twolanes> I144;
    std::unique_ptr<mipi_pll_dphy_sc_invx64_lvt> I74;
    std::unique_ptr<mipi_pll_dphy_sc_invx32_lvt> I73;
    std::unique_ptr<mipi_pll_dphy_sc_invx6_lvt> I8;
    std::unique_ptr<mipi_pll_dphy_sc_na2x6_lvt> I13;
};

#endif
