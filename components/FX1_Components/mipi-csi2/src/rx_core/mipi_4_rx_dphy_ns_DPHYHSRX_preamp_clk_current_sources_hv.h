//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk_current_sources_hv
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_PREAMP_CLK_CURRENT_SOURCES_HV_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_PREAMP_CLK_CURRENT_SOURCES_HV_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk_current_sources_hv : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> ihv{"ihv"};
    sc_core::sc_in<sc_dt::sc_logic> keyhv{"keyhv"};
    sc_core::sc_in<sc_dt::sc_logic> keyhvz{"keyhvz"};
    sc_core::sc_in<sc_dt::sc_lv<4>> lbkeyhv{"lbkeyhv"};
    sc_core::sc_in<sc_dt::sc_logic> onhv{"onhv"};
    sc_core::sc_in<sc_dt::sc_logic> onhvz{"onhvz"};
    sc_core::sc_out<sc_dt::sc_logic> ibias{"ibias"};
    sc_core::sc_out<sc_dt::sc_logic> ibias_cal{"ibias_cal"};
    sc_core::sc_out<sc_dt::sc_logic> ibias_lb{"ibias_lb"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk_current_sources_hv);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk_current_sources_hv(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> lbkeyhv_b{"lbkeyhv_b", 4, vl::make_z1};
    sc_core::sc_signal_resolved ihv_int{"ihv_int", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> I0_CALL;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> I0_CALR;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> I22_7;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> I22_6;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> I22_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> I22_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> I22_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> I22_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> I22_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> I22_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> I23_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> I23_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> I24_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> I24_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> I24_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> I24_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> I25;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> IEQL_9;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> IEQL_8;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> IEQL_7;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> IEQL_6;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> IEQL_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> IEQL_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> IEQL_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> IEQL_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> IEQL_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> IEQL_0;

    void split_lbkeyhv();
    vl::logic<4> rd_lbkeyhv() const;
    void gate_ihv_int_buf0();
    void gate_ihv_int_buf1();
};

#endif
