//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_slicer_a_ptype
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_SLICER_A_PTYPE_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_SLICER_A_PTYPE_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_slicer_comp1a_ptype.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_slicer_latch_ptype_nand.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_slicer_a_ptype : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> en{"en"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> phi1{"phi1"};
    sc_core::sc_in<sc_dt::sc_logic> vi_m{"vi_m"};
    sc_core::sc_in<sc_dt::sc_logic> vi_p{"vi_p"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_out<sc_dt::sc_logic> q{"q"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_slicer_a_ptype);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_slicer_a_ptype(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved v1_p{"v1_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved phi1_gated{"phi1_gated", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved v1_m{"v1_m", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_slicer_comp1a_ptype> stg1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_slicer_latch_ptype_nand> stg3;
    // wire_cap  I23( .w(phi1_gated) );
    // wire_cap  I27( .w(v1_m) );
    // wire_cap  I28( .w(v1_p) );
    // wire_cap  I29( .w(vi_p) );
    // wire_cap  I30( .w(vi_m) );
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x24_lvt> I38;
};

#endif
