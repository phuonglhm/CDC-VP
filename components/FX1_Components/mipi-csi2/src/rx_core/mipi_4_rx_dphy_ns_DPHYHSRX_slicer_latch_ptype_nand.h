//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_slicer_latch_ptype_nand
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_SLICER_LATCH_PTYPE_NAND_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_SLICER_LATCH_PTYPE_NAND_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_slicer_latch_ptype_nand : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk{"clk"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vi_m{"vi_m"};
    sc_core::sc_in<sc_dt::sc_logic> vi_p{"vi_p"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_out<sc_dt::sc_logic> q{"q"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_slicer_latch_ptype_nand);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_slicer_latch_ptype_nand(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> vp4{"vp4", vl::logic<1>().sl()};
    vl::logic<1> vm4;
    sc_core::sc_signal<sc_dt::sc_logic> vm5_storage{"vm5_storage", vl::logic<1>().sl()};
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_int{"clk_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net424{"net424", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved kp{"kp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net426{"net426", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vi_m_inv{"vi_m_inv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved sn{"sn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vi_p_inv{"vi_p_inv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved sp{"sp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_n{"clk_n", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> invc;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx48_lvt> invq;

    void assign_supply_ok();
    void assign_clk_int();
    void gate_vi_m_inv_buf();
    void gate_vi_p_inv_buf();
    void proc_initial_158279();
    void proc_initial_158280();
    void proc_always_158286();
    void proc_always_158317();
};

#endif
