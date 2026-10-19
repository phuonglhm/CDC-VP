//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_mpll_vreg_bypass_logic
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_MPLL_VREG_BYPASS_LOGIC_H
#define VL_MIPI_PLL_DPHY_MPLL_VREG_BYPASS_LOGIC_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "pll_cells.h"

class mipi_pll_dphy_mpll_vreg_bypass_logic : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> mpll_prg{"mpll_prg"};
    sc_core::sc_in<sc_dt::sc_logic> mpll_pwron{"mpll_pwron"};
    sc_core::sc_in<sc_dt::sc_logic> opmode{"opmode"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vss{"vss"};
    sc_core::sc_out<sc_dt::sc_logic> bypass{"bypass"};

    SC_HAS_PROCESS(mipi_pll_dphy_mpll_vreg_bypass_logic);
    explicit mipi_pll_dphy_mpll_vreg_bypass_logic(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved net22{"net22", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net23{"net23", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net24{"net24", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net25{"net25", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net29{"net29", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net33{"net33", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_sc_na3x6_lvt> I0;
    std::unique_ptr<mipi_pll_dphy_sc_na3x6_lvt> I1;
    std::unique_ptr<mipi_pll_dphy_sc_na3x6_lvt> I2;
    std::unique_ptr<mipi_pll_dphy_sc_na3x6_lvt> I3;
    std::unique_ptr<mipi_pll_dphy_sc_invx4_lvt> I4;
    std::unique_ptr<mipi_pll_dphy_sc_invx4_lvt> I5;
    std::unique_ptr<mipi_pll_dphy_sc_invx4_lvt> I6;
};

#endif
