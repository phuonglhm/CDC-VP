//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_CLK_MUX_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_CLK_MUX_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_sc_tie_hilow_lvt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk_osc_n{"clk_osc_n"};
    sc_core::sc_in<sc_dt::sc_logic> clk_osc_p{"clk_osc_p"};
    sc_core::sc_in<sc_dt::sc_logic> config_clk_n{"config_clk_n"};
    sc_core::sc_in<sc_dt::sc_logic> config_clk_p{"config_clk_p"};
    sc_core::sc_in<sc_dt::sc_logic> lb_sel{"lb_sel"};
    sc_core::sc_in<sc_dt::sc_logic> lbclk_n{"lbclk_n"};
    sc_core::sc_in<sc_dt::sc_logic> lbclk_p{"lbclk_p"};
    sc_core::sc_in<sc_dt::sc_logic> lbdata_n{"lbdata_n"};
    sc_core::sc_in<sc_dt::sc_logic> lbdata_p{"lbdata_p"};
    sc_core::sc_in<sc_dt::sc_logic> on{"on"};
    sc_core::sc_in<sc_dt::sc_logic> osc_sel{"osc_sel"};
    sc_core::sc_out<sc_dt::sc_logic> buf_on_lb{"buf_on_lb"};
    // inout clk_out_n;
    // inout clk_out_p;
    // inout data_out_n;
    // inout data_out_p;
    sc_core::sc_out<sc_dt::sc_logic> clk_out_n{"clk_out_n"};
    sc_core::sc_out<sc_dt::sc_logic> clk_out_p{"clk_out_p"};
    sc_core::sc_out<sc_dt::sc_logic> data_out_n{"data_out_n"};
    sc_core::sc_out<sc_dt::sc_logic> data_out_p{"data_out_p"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved buf_on_lbz{"buf_on_lbz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_high{"tie_high", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved data_glob_enz{"data_glob_enz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net111{"net111", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved buf_on_cfgz{"buf_on_cfgz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lb_selz{"lb_selz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved buf_on_osc{"buf_on_osc", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved onz{"onz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved buf_on_oscz{"buf_on_oscz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved buf_on_cfg{"buf_on_cfg", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved osc_selz{"osc_selz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low{"tie_low", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net105{"net105", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net106{"net106", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net107{"net107", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved data_glob_en{"data_glob_en", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> sc_lvt_I32;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux_buf_long> I35;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux_buf_long> I36;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux_buf_long> I39;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux_buf_long> I40;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_inv> I10;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_inv> I13;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_inv> I15;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux_buf> I1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux_buf> I2;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux_buf> I3;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux_buf> I9;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux_buf> I21;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux_buf> I22;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux_buf> I31;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux_buf> I32;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx64_lvt> I12;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx64_lvt> I37;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx64_lvt> sc_lvt_I30;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no3x6_lvt> I38;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na3x16_lvt> I14;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na3x16_lvt> sc_lvt_I29;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na3x16_lvt> sc_lvt_I31;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tie_hilow_lvt> I43;

    void gate_N1();
    void gate_N42();
};

#endif
