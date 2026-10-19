//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYRXCLK_CLL_MUX_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYRXCLK_CLL_MUX_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk_long_n{"clk_long_n"};
    sc_core::sc_in<sc_dt::sc_logic> clk_long_p{"clk_long_p"};
    sc_core::sc_in<sc_dt::sc_logic> clk_short_n{"clk_short_n"};
    sc_core::sc_in<sc_dt::sc_logic> clk_short_p{"clk_short_p"};
    sc_core::sc_in<sc_dt::sc_logic> en_long{"en_long"};
    sc_core::sc_in<sc_dt::sc_logic> on{"on"};
    sc_core::sc_in<sc_dt::sc_logic> tie_high{"tie_high"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low{"tie_low"};
    sc_core::sc_out<sc_dt::sc_logic> clk_out_n{"clk_out_n"};
    sc_core::sc_out<sc_dt::sc_logic> clk_out_p{"clk_out_p"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux);
    explicit mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved clk_out_p_int{"clk_out_p_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_out_n_int{"clk_out_n_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved buf_on_longz{"buf_on_longz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved buf_on_shortz{"buf_on_shortz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved en_long_z{"en_long_z", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved onz{"onz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved buf_on_long{"buf_on_long", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved buf_on_short{"buf_on_short", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_buf> I6;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_buf> I7;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_buf> I8;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_buf> I9;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_inv> I10;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_inv> I13;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x24_lvt> I29;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x24_lvt> I31;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx64_lvt> I30;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I32;

    void gate_clk_out_p_buf00();
    void gate_clk_out_p_buf01();
    void gate_clk_out_n_buf00();
    void gate_clk_out_n_buf01();
};

#endif
