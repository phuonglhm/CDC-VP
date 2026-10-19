//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_SC_DLAX23_AR_LVT_H
#define VL_MIPI_4_RX_DPHY_NS_SC_DLAX23_AR_LVT_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> ck_m{"ck_m"};
    sc_core::sc_in<sc_dt::sc_logic> ck_p{"ck_p"};
    sc_core::sc_in<sc_dt::sc_logic> d{"d"};
    sc_core::sc_in<sc_dt::sc_logic> pwr_on{"pwr_on"};
    sc_core::sc_out<sc_dt::sc_logic> qb{"qb"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt);
    explicit mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ck_int{"ck_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> nb{"nb", vl::logic<1>().sl()};

    void assign_ck_int();
    void assign_supply_ok();
    void assign_qb();
    void proc_initial_161649();
    void proc_always_161653();
};

#endif
