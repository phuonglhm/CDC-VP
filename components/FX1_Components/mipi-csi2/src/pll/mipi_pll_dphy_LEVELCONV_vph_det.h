//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_LEVELCONV_vph_det
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_LEVELCONV_VPH_DET_H
#define VL_MIPI_PLL_DPHY_LEVELCONV_VPH_DET_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_pll_dphy_LEVELCONV_vph_det : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> A_in{"A_in"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low_hv{"tie_low_hv"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low_lv{"tie_low_lv"};
    sc_core::sc_out<sc_dt::sc_logic> Z{"Z"};
    sc_core::sc_out<sc_dt::sc_logic> Z_neg{"Z_neg"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_pll_dphy_LEVELCONV_vph_det);
    explicit mipi_pll_dphy_LEVELCONV_vph_det(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_rv<19> a{"a", vl::logic<19>::all_z().lv()};
    sc_core::sc_signal_resolved net227{"net227", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved inr{"inr", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved gate{"gate", sc_dt::SC_LOGIC_Z};

    void gate_buf1();
    void gate_buf2();
};

#endif
