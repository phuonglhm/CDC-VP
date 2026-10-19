//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_dynamic_pulse_catcher
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DYNAMIC_PULSE_CATCHER_H
#define VL_MIPI_4_RX_DPHY_NS_DYNAMIC_PULSE_CATCHER_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DWbb_bcm35.h"

// Parameters: clock_edge=1, reset_edge=1, timing_viol_en=2
class mipi_4_rx_dphy_ns_dynamic_pulse_catcher : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> pulse_in{"pulse_in"};
    sc_core::sc_in<sc_dt::sc_logic> clk{"clk"};
    sc_core::sc_in<sc_dt::sc_logic> rst{"rst"};
    sc_core::sc_out<sc_dt::sc_logic> pulse_out{"pulse_out"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_dynamic_pulse_catcher);
    explicit mipi_4_rx_dphy_ns_dynamic_pulse_catcher(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000.0;  // time precision (fs)
    // wire pulse_sync;
    sc_core::sc_signal_resolved int_clk{"int_clk", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved int_rst{"int_rst", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_DWbb_bcm35> U1;

    void assign_int_clk();
    void assign_int_rst();
};

#endif
