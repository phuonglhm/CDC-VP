//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PHASE_CHANGE_LOGIC
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_PHASE_CHANGE_LOGIC_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_PHASE_CHANGE_LOGIC_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_sc_dffx10_as_lvt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PHASE_CHANGE_LOGIC : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> ddl_phase_change{"ddl_phase_change"};
    sc_core::sc_in<sc_dt::sc_logic> hs_clkn{"hs_clkn"};
    sc_core::sc_in<sc_dt::sc_logic> hs_clkp{"hs_clkp"};
    sc_core::sc_in<sc_dt::sc_logic> reset{"reset"};
    sc_core::sc_out<sc_dt::sc_logic> flag_sync{"flag_sync"};
    sc_core::sc_out<sc_dt::sc_logic> gate_in_pulse{"gate_in_pulse"};
    sc_core::sc_out<sc_dt::sc_logic> gate_in_pulsez{"gate_in_pulsez"};
    sc_core::sc_out<sc_dt::sc_logic> gate_out_pulse{"gate_out_pulse"};
    sc_core::sc_out<sc_dt::sc_logic> gate_out_pulsez{"gate_out_pulsez"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PHASE_CHANGE_LOGIC);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PHASE_CHANGE_LOGIC(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved net197{"net197", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net198{"net198", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net199{"net199", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ff5z{"ff5z", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net212{"net212", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ff0{"ff0", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net194{"net194", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ff1{"ff1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ff2{"ff2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ff3{"ff3", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> I24;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> I25;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> I26;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> I27;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> I28;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dffx10_as_lvt> FF0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dffx10_as_lvt> FF1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dffx10_as_lvt> FF2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dffx10_as_lvt> FF3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dffx10_as_lvt> FF4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dffx10_as_lvt> FF5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I18;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I30;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> I14;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> I29;
};

#endif
