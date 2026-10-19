//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit1
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_PREAMP_UNIT1_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_PREAMP_UNIT1_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit1 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_lv<4>> g{"g"};
    sc_core::sc_in<sc_dt::sc_logic> ibias{"ibias"};
    sc_core::sc_in<sc_dt::sc_logic> vim{"vim"};
    sc_core::sc_in<sc_dt::sc_logic> vip{"vip"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_out<sc_dt::sc_logic> voutm{"voutm"};
    sc_core::sc_out<sc_dt::sc_logic> voutp{"voutp"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit1);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit1(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved enable{"enable", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};

    void assign_supply_ok();
    void assign_enable();
    void proc_always_157833();
    void proc_always_157847();
};

#endif
