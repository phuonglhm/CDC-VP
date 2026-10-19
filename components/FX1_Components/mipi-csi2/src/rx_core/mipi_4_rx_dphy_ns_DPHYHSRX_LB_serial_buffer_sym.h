//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_LB_serial_buffer_sym
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_LB_SERIAL_BUFFER_SYM_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_LB_SERIAL_BUFFER_SYM_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYHSTX_serial_buffer_sym.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_LB_serial_buffer_sym : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk_m{"clk_m"};
    sc_core::sc_in<sc_dt::sc_logic> clk_p{"clk_p"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> sel_sym{"sel_sym"};
    sc_core::sc_in<sc_dt::sc_logic> sym_a{"sym_a"};
    sc_core::sc_in<sc_dt::sc_logic> sym_b{"sym_b"};
    sc_core::sc_in<sc_dt::sc_logic> tra_a{"tra_a"};
    sc_core::sc_in<sc_dt::sc_logic> tra_b{"tra_b"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_out<sc_dt::sc_logic> data_ser_m{"data_ser_m"};
    sc_core::sc_out<sc_dt::sc_logic> data_ser_p{"data_ser_p"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_LB_serial_buffer_sym);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_LB_serial_buffer_sym(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved net405{"net405", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net423{"net423", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net420{"net420", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net422{"net422", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> I61;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I46;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSTX_serial_buffer_sym> I59;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> I60;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> sc_lvt_out_inv_p;
};

#endif
