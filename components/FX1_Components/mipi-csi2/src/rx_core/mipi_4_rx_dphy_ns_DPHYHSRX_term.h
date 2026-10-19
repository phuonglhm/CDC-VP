//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_term
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_TERM_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_TERM_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_term : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> dn_atb_en_hv{"dn_atb_en_hv"};
    sc_core::sc_in<sc_dt::sc_logic> dp_atb_en_hv{"dp_atb_en_hv"};
    sc_core::sc_in<sc_dt::sc_logic> en_term_h{"en_term_h"};
    sc_core::sc_in<sc_dt::sc_logic> lbres_cm_en_hv{"lbres_cm_en_hv"};
    sc_core::sc_in<sc_dt::sc_logic> lbres_dn_enz_hv{"lbres_dn_enz_hv"};
    sc_core::sc_in<sc_dt::sc_logic> lbres_dp_enz_hv{"lbres_dp_enz_hv"};
    sc_core::sc_in<sc_dt::sc_logic> rxhs_rx_en200mode_hvz{"rxhs_rx_en200mode_hvz"};
    sc_core::sc_in<sc_dt::sc_lv<3>> setrx_h{"setrx_h"};
    sc_core::sc_in<sc_dt::sc_logic> swbulk{"swbulk"};
    sc_core::sc_in<sc_dt::sc_logic> atb_line{"atb_line"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> rxdn{"rxdn"};
    sc_core::sc_in<sc_dt::sc_logic> rxdp{"rxdp"};
    sc_core::sc_in<sc_dt::sc_logic> vmid{"vmid"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_term);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_term(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
};

#endif
