//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_sc_or2x6_lvt
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_SC_OR2X6_LVT_H
#define VL_MIPI_PLL_DPHY_SC_OR2X6_LVT_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "pll_cells.h"

class mipi_pll_dphy_sc_or2x6_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> b{"b"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_pll_dphy_sc_or2x6_lvt);
    explicit mipi_pll_dphy_sc_or2x6_lvt(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved no{"no", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> inv;
    std::unique_ptr<mipi_pll_dphy_wire_cap> I0;
    std::unique_ptr<mipi_pll_dphy_sc_no2x6_lvt> nor1;
};

#endif
