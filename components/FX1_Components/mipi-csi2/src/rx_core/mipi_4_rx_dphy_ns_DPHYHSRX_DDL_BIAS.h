//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BIAS
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_BIAS_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_BIAS_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BIAS : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> bias_ddl{"bias_ddl"};
    sc_core::sc_in<sc_dt::sc_logic> pon{"pon"};
    sc_core::sc_in<sc_dt::sc_logic> pon_hv{"pon_hv"};
    sc_core::sc_in<sc_dt::sc_logic> ponz{"ponz"};
    sc_core::sc_in<sc_dt::sc_logic> tie_high{"tie_high"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low{"tie_low"};
    sc_core::sc_in<sc_dt::sc_lv<5>> tune{"tune"};
    sc_core::sc_out<sc_dt::sc_logic> bn{"bn"};
    sc_core::sc_out<sc_dt::sc_logic> bp{"bp"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BIAS);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BIAS(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved net343{"net343", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net428{"net428", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net348{"net348", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net350{"net350", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net352{"net352", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net433{"net433", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net354{"net354", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net356{"net356", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enable{"enable", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> bp_r{"bp_r", vl::logic<1>().sl()};   // value of output bp

    void drive_bp();
    void assign_supply_ok();
    void assign_enable();
    void proc_always_154783();
    void proc_always_154793();
};

#endif
