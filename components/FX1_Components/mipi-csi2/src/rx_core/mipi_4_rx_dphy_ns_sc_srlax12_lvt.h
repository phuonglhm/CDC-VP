//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_sc_srlax12_lvt
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_SC_SRLAX12_LVT_H
#define VL_MIPI_4_RX_DPHY_NS_SC_SRLAX12_LVT_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_4_rx_dphy_ns_sc_srlax12_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> reset_n{"reset_n"};
    sc_core::sc_in<sc_dt::sc_logic> set{"set"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_srlax12_lvt);
    explicit mipi_4_rx_dphy_ns_sc_srlax12_lvt(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal<sc_dt::sc_logic> nb{"nb", vl::logic<1>().sl()};

    void assign_supply_ok();
    void assign_z();
    void proc_always_162402();
};

#endif
