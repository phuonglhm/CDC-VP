//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_sctg_tg3x
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_SCTG_TG3X_H
#define VL_MIPI_4_RX_DPHY_NS_SCTG_TG3X_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_4_rx_dphy_ns_sctg_tg3x : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> en{"en"};
    sc_core::sc_in<sc_dt::sc_logic> en_n{"en_n"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> i{"i"};
    sc_core::sc_inout<sc_dt::sc_logic> o{"o"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sctg_tg3x);
    explicit mipi_4_rx_dphy_ns_sctg_tg3x(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)

    void gate_buf_out();
};

#endif
