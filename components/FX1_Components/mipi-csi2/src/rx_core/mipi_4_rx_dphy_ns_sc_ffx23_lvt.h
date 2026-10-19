//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_sc_ffx23_lvt
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_SC_FFX23_LVT_H
#define VL_MIPI_4_RX_DPHY_NS_SC_FFX23_LVT_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_4_rx_dphy_ns_sc_ffx23_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> ck{"ck"};
    sc_core::sc_in<sc_dt::sc_logic> d{"d"};
    sc_core::sc_out<sc_dt::sc_logic> q{"q"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_ffx23_lvt);
    explicit mipi_4_rx_dphy_ns_sc_ffx23_lvt(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> n1{"n1", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> n3{"n3", vl::logic<1>().sl()};
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};

    void assign_supply_ok();
    void assign_q();
    void proc_initial_161879();
    void proc_initial_161880();
    void proc_always_161882();
    void proc_always_161894();
};

#endif
