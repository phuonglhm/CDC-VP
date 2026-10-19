//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_LEVELCONV_l2h_bias_soc
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_LEVELCONV_L2H_BIAS_SOC_H
#define VL_MIPI_PLL_DPHY_LEVELCONV_L2H_BIAS_SOC_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_pll_dphy_LEVELCONV_l2h_bias_soc : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> A_in{"A_in"};
    sc_core::sc_in<sc_dt::sc_logic> onz_hv{"onz_hv"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low_hv{"tie_low_hv"};
    sc_core::sc_in<sc_dt::sc_logic> vblvhv{"vblvhv"};
    sc_core::sc_out<sc_dt::sc_logic> Z{"Z"};
    sc_core::sc_out<sc_dt::sc_logic> Z_neg{"Z_neg"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vdd{"vdd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    sc_core::sc_in<sc_dt::sc_logic> vss{"vss"};

    SC_HAS_PROCESS(mipi_pll_dphy_LEVELCONV_l2h_bias_soc);
    explicit mipi_pll_dphy_LEVELCONV_l2h_bias_soc(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal<sc_dt::sc_logic> a{"a", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> b{"b", vl::logic<1>().sl()};
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved inz{"inz", sc_dt::SC_LOGIC_Z};

    void assign_supply_ok();
    void assign_inz();
    void assign_Z();
    void assign_Z_neg();
    void proc_always_2672();
};

#endif
