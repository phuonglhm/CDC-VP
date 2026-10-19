//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge0 (parameter specialisation)
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_STATIC_PULSE_CATCHER__RESET_EDGE0_H
#define VL_MIPI_4_RX_DPHY_NS_STATIC_PULSE_CATCHER__RESET_EDGE0_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DWbb_bcm41.h"

// Parameters: reset_value=0, set_value=1, clock_edge=1, reset_edge=0, timing_viol_en=2
class mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge0 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> pulse_in{"pulse_in"};
    sc_core::sc_in<sc_dt::sc_logic> clk{"clk"};
    sc_core::sc_in<sc_dt::sc_logic> rst{"rst"};
    sc_core::sc_in<sc_dt::sc_logic> clk_cap{"clk_cap"};
    sc_core::sc_in<sc_dt::sc_logic> rst_cap{"rst_cap"};
    sc_core::sc_out<sc_dt::sc_logic> pulse_out{"pulse_out"};
    sc_core::sc_out<sc_dt::sc_logic> pulse_in_ff{"pulse_in_ff"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge0);
    explicit mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge0(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000.0;  // time precision (fs)
    sc_core::sc_signal_resolved int_clk{"int_clk", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved int_rst{"int_rst", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_DWbb_bcm41> U1;

    void assign_int_clk();
    void assign_int_rst();
    void proc_always_2421();
};

#endif
