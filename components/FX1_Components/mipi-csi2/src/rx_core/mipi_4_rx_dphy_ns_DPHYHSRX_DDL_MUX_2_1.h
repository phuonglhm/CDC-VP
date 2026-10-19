//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_MUX_2_1_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_MUX_2_1_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk_in{"clk_in"};
    sc_core::sc_in<sc_dt::sc_logic> ddl_clk{"ddl_clk"};
    sc_core::sc_in<sc_dt::sc_logic> tune_en{"tune_en"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved tune_enz{"tune_enz", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I8;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1_inv_en> I4;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1_inv_en> I46;
};

#endif
