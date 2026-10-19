//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL_gate
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_GATE_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_GATE_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_DDL_gate : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> i{"i"};
    sc_core::sc_in<sc_dt::sc_logic> on{"on"};
    sc_core::sc_in<sc_dt::sc_logic> onz{"onz"};
    sc_core::sc_in<sc_dt::sc_logic> pd{"pd"};
    sc_core::sc_in<sc_dt::sc_logic> pu{"pu"};
    sc_core::sc_out<sc_dt::sc_logic> o{"o"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_DDL_gate);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_DDL_gate(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000.0;  // time precision (fs)
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    double data_delay = 0.0;
    sc_core::sc_event gate_o_buff01_ev;
    vl::logic<1> gate_o_buff01_pv = vl::logic<1>::all_z();
    bool gate_o_buff01_ev_pend = false;

    void assign_supply_ok();
    void gate_o_buff00();
    void gate_o_buff01();
    void gate_o_buff01_upd();
};

#endif
