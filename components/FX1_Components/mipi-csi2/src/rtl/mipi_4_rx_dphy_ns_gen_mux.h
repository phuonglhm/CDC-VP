//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_gen_mux
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_GEN_MUX_H
#define VL_MIPI_4_RX_DPHY_NS_GEN_MUX_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

// Parameters: DELAY=0, WIDTH=1
class mipi_4_rx_dphy_ns_gen_mux : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> sel{"sel"};
    // -- INPUT AND OUTPUT PORTS  --------------------------------------------------
    sc_core::sc_in<sc_dt::sc_logic> d0{"d0"};
    sc_core::sc_in<sc_dt::sc_logic> d1{"d1"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_gen_mux);
    explicit mipi_4_rx_dphy_ns_gen_mux(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000.0;  // time precision (fs)
    sc_core::sc_event assign_z_ev;
    vl::logic<1> assign_z_pv = vl::logic<1>::all_z();
    bool assign_z_ev_pend = false;

    void assign_z();
    void assign_z_upd();
};

#endif
