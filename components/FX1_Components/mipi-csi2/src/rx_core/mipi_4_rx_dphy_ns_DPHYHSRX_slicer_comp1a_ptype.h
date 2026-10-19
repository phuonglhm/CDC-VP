//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_slicer_comp1a_ptype
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_SLICER_COMP1A_PTYPE_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_SLICER_COMP1A_PTYPE_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_slicer_comp1a_ptype : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk{"clk"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vi_m{"vi_m"};
    sc_core::sc_in<sc_dt::sc_logic> vi_p{"vi_p"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_out<sc_dt::sc_logic> vom{"vom"};
    sc_core::sc_out<sc_dt::sc_logic> vop{"vop"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_slicer_comp1a_ptype);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_slicer_comp1a_ptype(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> violation{"violation", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> vi_m_prev{"vi_m_prev", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> vi_p_prev{"vi_p_prev", vl::logic<1>().sl()};
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};

    void assign_supply_ok();
    void proc_initial_158170();
    void proc_always_158176();
    void proc_always_158191();
    void proc_always_158201();
};

#endif
