//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DWbb_bcm35
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DWBB_BCM35_H
#define VL_MIPI_4_RX_DPHY_NS_DWBB_BCM35_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DWbb_bcm21.h"

// Parameters: REG_EVENT=0, F_SYNC_TYPE=2, VERIF_EN=0
class mipi_4_rx_dphy_ns_DWbb_bcm35 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk_s{"clk_s"};
    // spurious pulse input to be detected
    sc_core::sc_in<sc_dt::sc_logic> clk_d{"clk_d"};
    // destination domain clock
    sc_core::sc_in<sc_dt::sc_logic> rst_d_n{"rst_d_n"};
    // destination domain reset (active low, async)
    sc_core::sc_out<sc_dt::sc_logic> event_d{"event_d"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DWbb_bcm35);
    explicit mipi_4_rx_dphy_ns_DWbb_bcm35(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000.0;  // time precision (fs)
    // pulse detect event output (destination domain)
    sc_core::sc_signal_resolved test{"test", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal<sc_dt::sc_logic> sample_sync_spcl{"sample_sync_spcl", vl::logic<1>().sl()};
    // capture register
    sc_core::sc_signal_resolved sync_captr{"sync_captr", sc_dt::SC_LOGIC_Z};
    // synchronized capture value
    sc_core::sc_signal<sc_dt::sc_logic> hist{"hist", vl::logic<1>().sl()};
    // history of synchronized capture value
    sc_core::sc_signal_resolved detect{"detect", sc_dt::SC_LOGIC_Z};
    // pulse event detect signal (unregistered)
    sc_core::sc_signal<sc_dt::sc_logic> detect_q{"detect_q", vl::logic<1>().sl()};
    // pulse event detect register
    sc_core::sc_signal_resolved pulse_captr_cc{"pulse_captr_cc", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pulse_captr_s{"pulse_captr_s", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved next_captr_val_cc{"next_captr_val_cc", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_DWbb_bcm21> U_SYNC;

    void assign_next_captr_val_cc();
    void assign_pulse_captr_s();
    void assign_pulse_captr_cc();
    void assign_detect();
    void assign_event_d();
    void proc_always_2588();
    void proc_always_2642();
};

#endif
