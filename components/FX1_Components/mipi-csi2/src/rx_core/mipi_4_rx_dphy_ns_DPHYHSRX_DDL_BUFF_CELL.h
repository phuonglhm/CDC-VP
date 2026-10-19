//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BUFF_CELL
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_BUFF_CELL_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_BUFF_CELL_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_inv_en.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BUFF_CELL : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> back{"back"};
    sc_core::sc_in<sc_dt::sc_logic> bn{"bn"};
    sc_core::sc_in<sc_dt::sc_logic> bp{"bp"};
    sc_core::sc_in<sc_dt::sc_logic> forward{"forward"};
    sc_core::sc_in<sc_dt::sc_logic> in_back{"in_back"};
    sc_core::sc_in<sc_dt::sc_logic> in_forward{"in_forward"};
    sc_core::sc_in<sc_dt::sc_logic> pd{"pd"};
    sc_core::sc_in<sc_dt::sc_logic> pu{"pu"};
    sc_core::sc_in<sc_dt::sc_logic> tie_high{"tie_high"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low{"tie_low"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_inout<sc_dt::sc_logic> out_back{"out_back"};
    sc_core::sc_inout<sc_dt::sc_logic> out_forward{"out_forward"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BUFF_CELL);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BUFF_CELL(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV> INV_BACK;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV> INV_FORWARD;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_inv_en> INV_TRISTATE;
};

#endif
