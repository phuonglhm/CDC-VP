//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYLPRX_ULP
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYLPRX_ULP_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYLPRX_ULP_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYLPRX_ULP : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> ULPout{"ULPout"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vdd{"vdd"};
    sc_core::sc_in<sc_dt::sc_logic> ving{"ving"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    sc_core::sc_in<sc_dt::sc_logic> vss{"vss"};
    sc_core::sc_in<sc_dt::sc_logic> pon{"pon"};
    sc_core::sc_in<sc_dt::sc_logic> ponhv{"ponhv"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYLPRX_ULP);
    explicit mipi_4_rx_dphy_ns_DPHYLPRX_ULP(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved A{"A", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved B{"B", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved AZ{"AZ", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net186{"net186", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ULPout_int{"ULPout_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved out_neg{"out_neg", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net84{"net84", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net158{"net158", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved out_pre{"out_pre", sc_dt::SC_LOGIC_Z};
    sc_core::sc_event gate_I01ULPRCV_ev;
    vl::logic<1> gate_I01ULPRCV_pv = vl::logic<1>::all_z();
    bool gate_I01ULPRCV_ev_pend = false;
    sc_core::sc_event gate_I02ULPRCV_ev;
    vl::logic<1> gate_I02ULPRCV_pv = vl::logic<1>::all_z();
    bool gate_I02ULPRCV_ev_pend = false;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_nand2x> sctg_I31;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_nand2x> sctg_I32;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_I30;

    void assign_supply_ok();
    void assign_B();
    void gate_ULPRCV_I01();
    void gate_ULPRCV_I02();
    void gate_I01ULPRCV();
    void gate_I01ULPRCV_upd();
    void gate_I02ULPRCV();
    void gate_I02ULPRCV_upd();
};

#endif
