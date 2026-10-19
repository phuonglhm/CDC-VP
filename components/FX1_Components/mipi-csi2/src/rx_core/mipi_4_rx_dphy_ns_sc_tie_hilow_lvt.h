//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_sc_tie_hilow_lvt
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_SC_TIE_HILOW_LVT_H
#define VL_MIPI_4_RX_DPHY_NS_SC_TIE_HILOW_LVT_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_sc_tie_hilow_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_inout<sc_dt::sc_logic> tie_high{"tie_high"};
    sc_core::sc_inout<sc_dt::sc_logic> tie_low{"tie_low"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_tie_hilow_lvt);
    explicit mipi_4_rx_dphy_ns_sc_tie_hilow_lvt(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved soft_low{"soft_low", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved soft_high{"soft_high", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> inv_high;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> inv_low;

    void assign_soft_low();
    void assign_soft_high();
};

#endif
