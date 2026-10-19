//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_lane_scan_buf
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_LANE_SCAN_BUF_H
#define VL_MIPI_PLL_DPHY_LANE_SCAN_BUF_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "pll_cells.h"

class mipi_pll_dphy_lane_scan_buf : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};

    SC_HAS_PROCESS(mipi_pll_dphy_lane_scan_buf);
    explicit mipi_pll_dphy_lane_scan_buf(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved net37{"net37", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I5;
    std::unique_ptr<mipi_pll_dphy_sc_invx48_lvt> I6;
};

#endif
