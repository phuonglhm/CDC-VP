//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYLPTX_bias
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYLPTX_BIAS_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYLPTX_BIAS_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_4_rx_dphy_ns_DPHYLPTX_bias : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_lv<3>> cur_setz{"cur_setz"};
    sc_core::sc_in<sc_dt::sc_logic> pon{"pon"};
    sc_core::sc_in<sc_dt::sc_logic> ponz{"ponz"};
    sc_core::sc_in<sc_dt::sc_logic> tie_high{"tie_high"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low{"tie_low"};
    sc_core::sc_in<sc_dt::sc_logic> vref1200m{"vref1200m"};
    sc_core::sc_out<sc_dt::sc_logic> biasn{"biasn"};
    sc_core::sc_out<sc_dt::sc_logic> biasn_fb{"biasn_fb"};
    sc_core::sc_out<sc_dt::sc_logic> biasp{"biasp"};
    sc_core::sc_out<sc_dt::sc_logic> cascn{"cascn"};
    sc_core::sc_out<sc_dt::sc_logic> cascp{"cascp"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> ibias{"ibias"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYLPTX_bias);
    explicit mipi_4_rx_dphy_ns_DPHYLPTX_bias(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};

    void assign_supply_ok();
    void proc_always_159848();
};

#endif
