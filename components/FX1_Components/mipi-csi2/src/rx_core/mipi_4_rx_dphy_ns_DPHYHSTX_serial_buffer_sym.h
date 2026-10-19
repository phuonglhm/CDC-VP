//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSTX_serial_buffer_sym
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSTX_SERIAL_BUFFER_SYM_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSTX_SERIAL_BUFFER_SYM_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYHSTX_serial_mux2x_diff_buf.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSTX_serial_buffer_sym : public sc_core::sc_module {
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

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSTX_serial_buffer_sym);
    explicit mipi_4_rx_dphy_ns_DPHYHSTX_serial_buffer_sym(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved data_an{"data_an", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved data_ser_m_mux{"data_ser_m_mux", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved data_a{"data_a", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved data_b{"data_b", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved data_ser_p_mux{"data_ser_p_mux", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved data_bn{"data_bn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved sel_tra{"sel_tra", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSTX_serial_mux2x_diff_buf> mux_m;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSTX_serial_mux2x_diff_buf> mux_p;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSTX_drv_mux> I52;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSTX_drv_mux> I53;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I46;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I47;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx6_lvt> sc_lvt_I58;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> sc_lvt_out_inv_m;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> sc_lvt_out_inv_p;
};

#endif
