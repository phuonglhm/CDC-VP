//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYRXCLK_CLL_DEMUX_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYRXCLK_CLL_DEMUX_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_sc_syncx10_ar_lvt.h"
#include "mipi_4_rx_dphy_ns_sc_syncx10_as_lvt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk_en{"clk_en"};
    sc_core::sc_in<sc_dt::sc_logic> clk_in_n{"clk_in_n"};
    sc_core::sc_in<sc_dt::sc_logic> clk_in_p{"clk_in_p"};
    sc_core::sc_in<sc_dt::sc_logic> ddr_clk_sel{"ddr_clk_sel"};
    sc_core::sc_in<sc_dt::sc_logic> en_long{"en_long"};
    sc_core::sc_in<sc_dt::sc_logic> pon{"pon"};
    sc_core::sc_in<sc_dt::sc_logic> pull_clk_long{"pull_clk_long"};
    sc_core::sc_out<sc_dt::sc_logic> clk_out_div{"clk_out_div"};
    sc_core::sc_inout<sc_dt::sc_logic> clk_out_long_n{"clk_out_long_n"};
    sc_core::sc_inout<sc_dt::sc_logic> clk_out_long_p{"clk_out_long_p"};
    sc_core::sc_out<sc_dt::sc_logic> clk_out_short_n{"clk_out_short_n"};
    sc_core::sc_out<sc_dt::sc_logic> clk_out_short_p{"clk_out_short_p"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux);
    explicit mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved nor_pull_long{"nor_pull_long", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved buf_on_longz{"buf_on_longz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved buf_on_shortz{"buf_on_shortz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved buf_en_sync{"buf_en_sync", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_mux{"clk_mux", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pull_long{"pull_long", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_mux_z{"clk_mux_z", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved buf_en_syncz{"buf_en_syncz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ponz{"ponz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> dummy_clk{"dummy_clk", 2, vl::make_z1};
    sc_core::sc_signal_resolved buf_on_long{"buf_on_long", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved buf_on_short{"buf_on_short", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved a2{"a2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved en_longz{"en_longz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ddr_clk_en{"ddr_clk_en", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pull_longz{"pull_longz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_enz{"clk_enz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_out_long_n_int{"clk_out_long_n_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_out_long_p_int{"clk_out_long_p_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_out_short_n_int{"clk_out_short_n_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_out_short_p_int{"clk_out_short_p_int", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux_buf_long> I0;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux_buf_long> I1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> I48;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> I52;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> I63;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux_buf_short> I10;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux_buf_short> I11;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_syncx10_as_lvt> I41;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> I28;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> I37;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> I69;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> dummy_inv_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> dummy_inv_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx64_lvt> I30;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x24_lvt> I29_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x24_lvt> I29_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x24_lvt> I29_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x24_lvt> I29_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x24_lvt> I31_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x24_lvt> I31_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I36;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I39;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I32;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_syncx10_ar_lvt> I43;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x12_lvt> I54;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x12_lvt> I64;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_mux2x8_lvt> I62;

    vl::logic<2> rd_dummy_clk() const;
    void assign_supply_ok();
    void gate_clk_out_long_p_buf00();
    void gate_clk_out_long_p_buf01();
    void gate_clk_out_long_n_buf00();
    void gate_clk_out_long_n_buf01();
    void proc_always_160773();
};

#endif
