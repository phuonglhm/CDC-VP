//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_gen_sync_rst
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_GEN_SYNC_RST_H
#define VL_MIPI_4_RX_DPHY_NS_GEN_SYNC_RST_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

// Parameters: reset_value=0, clock_edge=1, reset_edge=0, stage_number=3, timing_viol_en=2
class mipi_4_rx_dphy_ns_gen_sync_rst : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk{"clk"};
    sc_core::sc_in<sc_dt::sc_logic> rst{"rst"};
    sc_core::sc_out<sc_dt::sc_logic> srst{"srst"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_gen_sync_rst);
    explicit mipi_4_rx_dphy_ns_gen_sync_rst(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000.0;  // time precision (fs)
    sc_core::sc_signal<sc_dt::sc_logic> reset_async_ff{"reset_async_ff", vl::logic<1>().sl()};
    sc_core::sc_signal_resolved int_clk{"int_clk", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved int_rst{"int_rst", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal<sc_dt::sc_lv<3>> syncrst{"syncrst", vl::logic<3>().lv()};

    void assign_int_clk();
    void assign_int_rst();
    void assign_srst();
    void proc_always_909();
    void proc_always_928();
};

#endif
