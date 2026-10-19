//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYCB_preamp_bias
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYCB_PREAMP_BIAS_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYCB_PREAMP_BIAS_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_4_rx_dphy_ns_DPHYCB_preamp_bias : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> rx_pre_pon_h{"rx_pre_pon_h"};
    sc_core::sc_in<sc_dt::sc_logic> rx_pre_pon_hz{"rx_pre_pon_hz"};
    sc_core::sc_out<sc_dt::sc_lv<5>> ibias_d2c_bias{"ibias_d2c_bias"};
    sc_core::sc_out<sc_dt::sc_lv<5>> ihv{"ihv"};
    sc_core::sc_out<sc_dt::sc_lv<5>> ilv{"ilv"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYCB_preamp_bias);
    explicit mipi_4_rx_dphy_ns_DPHYCB_preamp_bias(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enable{"enable", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal<sc_dt::sc_lv<5>, sc_core::SC_UNCHECKED_WRITERS> ibias_d2c_bias_r{"ibias_d2c_bias_r", vl::logic<5>().lv()};   // value of output ibias_d2c_bias
    sc_core::sc_signal<sc_dt::sc_lv<5>, sc_core::SC_UNCHECKED_WRITERS> ihv_r{"ihv_r", vl::logic<5>().lv()};   // value of output ihv
    sc_core::sc_signal<sc_dt::sc_lv<5>, sc_core::SC_UNCHECKED_WRITERS> ilv_r{"ilv_r", vl::logic<5>().lv()};   // value of output ilv

    void drive_ibias_d2c_bias();
    void drive_ihv();
    void drive_ilv();
    void assign_supply_ok();
    void assign_enable();
    void proc_initial_151641();
    void proc_always_151647();
};

#endif
