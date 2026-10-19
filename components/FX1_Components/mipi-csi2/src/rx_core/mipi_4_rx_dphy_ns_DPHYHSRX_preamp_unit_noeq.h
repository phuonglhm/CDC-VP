//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit_noeq
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_PREAMP_UNIT_NOEQ_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_PREAMP_UNIT_NOEQ_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit_noeq : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> ibias{"ibias"};
    sc_core::sc_in<sc_dt::sc_logic> vim{"vim"};
    sc_core::sc_in<sc_dt::sc_logic> vip{"vip"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_out<sc_dt::sc_logic> voutm{"voutm"};
    sc_core::sc_out<sc_dt::sc_logic> voutp{"voutp"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit_noeq);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit_noeq(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};

    void assign_supply_ok();
    void proc_always_158071();
    void proc_always_158081();
};

#endif
