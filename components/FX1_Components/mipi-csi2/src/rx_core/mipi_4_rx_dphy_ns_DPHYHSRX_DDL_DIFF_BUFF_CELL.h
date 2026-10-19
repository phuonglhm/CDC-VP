//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_DIFF_BUFF_CELL_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_DIFF_BUFF_CELL_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BUFF_CELL.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> back{"back"};
    sc_core::sc_in<sc_dt::sc_logic> bn{"bn"};
    sc_core::sc_in<sc_dt::sc_logic> bp{"bp"};
    sc_core::sc_in<sc_dt::sc_logic> forward{"forward"};
    sc_core::sc_in<sc_dt::sc_logic> in_back_n{"in_back_n"};
    sc_core::sc_in<sc_dt::sc_logic> in_back_p{"in_back_p"};
    sc_core::sc_in<sc_dt::sc_logic> in_forward_n{"in_forward_n"};
    sc_core::sc_in<sc_dt::sc_logic> in_forward_p{"in_forward_p"};
    sc_core::sc_in<sc_dt::sc_logic> pdn{"pdn"};
    sc_core::sc_in<sc_dt::sc_logic> pdp{"pdp"};
    sc_core::sc_in<sc_dt::sc_logic> pun{"pun"};
    sc_core::sc_in<sc_dt::sc_logic> pup{"pup"};
    sc_core::sc_in<sc_dt::sc_logic> tie_high{"tie_high"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low{"tie_low"};
    sc_core::sc_out<sc_dt::sc_logic> out_back_n{"out_back_n"};
    sc_core::sc_out<sc_dt::sc_logic> out_back_p{"out_back_p"};
    sc_core::sc_out<sc_dt::sc_logic> out_forward_n{"out_forward_n"};
    sc_core::sc_out<sc_dt::sc_logic> out_forward_p{"out_forward_p"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BUFF_CELL> BUFF_CELLN;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BUFF_CELL> BUFF_CELLP;
};

#endif
