//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_LEVELCONV_h2l_soc
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_LEVELCONV_H2L_SOC_H
#define VL_MIPI_4_RX_DPHY_NS_LEVELCONV_H2L_SOC_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_4_rx_dphy_ns_LEVELCONV_h2l_soc : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> A_in{"A_in"};
    sc_core::sc_in<sc_dt::sc_logic> on_hv{"on_hv"};
    sc_core::sc_in<sc_dt::sc_logic> onz_lv_soc{"onz_lv_soc"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low_lv_soc{"tie_low_lv_soc"};
    sc_core::sc_out<sc_dt::sc_logic> Z{"Z"};
    sc_core::sc_out<sc_dt::sc_logic> Z_neg{"Z_neg"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vdd{"vdd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    sc_core::sc_in<sc_dt::sc_logic> vss{"vss"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_LEVELCONV_h2l_soc);
    explicit mipi_4_rx_dphy_ns_LEVELCONV_h2l_soc(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};

    void assign_supply_ok();
    void proc_always_161156();
};

#endif
