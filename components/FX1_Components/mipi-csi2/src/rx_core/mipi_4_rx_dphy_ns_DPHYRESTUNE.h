//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYRESTUNE
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYRESTUNE_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYRESTUNE_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_4_rx_dphy_ns_DPHYRESTUNE : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> cfgclk{"cfgclk"};
    sc_core::sc_in<sc_dt::sc_logic> en_res0_en_cap1_n{"en_res0_en_cap1_n"};
    sc_core::sc_in<sc_dt::sc_logic> pickfreq{"pickfreq"};
    sc_core::sc_in<sc_dt::sc_logic> pon_restune{"pon_restune"};
    sc_core::sc_in<sc_dt::sc_logic> pon_restune_hv{"pon_restune_hv"};
    sc_core::sc_out<sc_dt::sc_logic> clk2dig{"clk2dig"};
    sc_core::sc_out<sc_dt::sc_logic> clkosc2bg{"clkosc2bg"};
    sc_core::sc_out<sc_dt::sc_logic> outm{"outm"};
    sc_core::sc_out<sc_dt::sc_logic> outp{"outp"};
    sc_core::sc_in<sc_dt::sc_logic> cap_10pf{"cap_10pf"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYRESTUNE);
    explicit mipi_4_rx_dphy_ns_DPHYRESTUNE(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000.0;  // time precision (fs)
    sc_core::sc_signal<sc_dt::sc_lv<2>> corner{"corner", vl::logic<2>().lv()};
    sc_core::sc_signal_resolved net47{"net47", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> FREQ{"FREQ", 0.0};
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> clk2dig_r{"clk2dig_r", vl::logic<1>().sl()};   // value of output clk2dig
    vl::action_queue aq_{"aq_"};

    void drive_clk2dig();
    void assign_supply_ok();
    void proc_initial_160572();
    void proc_initial_160573();
    void proc_always_160575();
    void proc_always_160584();
};

#endif
