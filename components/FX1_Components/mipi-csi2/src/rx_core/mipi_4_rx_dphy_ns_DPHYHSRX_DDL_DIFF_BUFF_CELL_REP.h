//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL_REP
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_DIFF_BUFF_CELL_REP_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_DIFF_BUFF_CELL_REP_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL_REP : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> bn{"bn"};
    sc_core::sc_in<sc_dt::sc_logic> bp{"bp"};
    sc_core::sc_in<sc_dt::sc_logic> in_n{"in_n"};
    sc_core::sc_in<sc_dt::sc_logic> in_p{"in_p"};
    sc_core::sc_in<sc_dt::sc_logic> on{"on"};
    sc_core::sc_in<sc_dt::sc_logic> onz{"onz"};
    sc_core::sc_in<sc_dt::sc_logic> tie_high{"tie_high"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low{"tie_low"};
    sc_core::sc_out<sc_dt::sc_logic> out_n{"out_n"};
    sc_core::sc_out<sc_dt::sc_logic> out_p{"out_p"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL_REP);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL_REP(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved middlen{"middlen", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved middlep{"middlep", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV> FEED_BACK_P;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV> I387;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV> I402;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV> I403;
};

#endif
