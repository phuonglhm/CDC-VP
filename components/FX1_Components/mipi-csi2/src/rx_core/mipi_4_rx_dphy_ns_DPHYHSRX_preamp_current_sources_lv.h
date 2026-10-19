//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_preamp_current_sources_lv
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_PREAMP_CURRENT_SOURCES_LV_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_PREAMP_CURRENT_SOURCES_LV_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_preamp_current_sources_lv : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> ilv_cc{"ilv_cc"};
    sc_core::sc_in<sc_dt::sc_logic> ilv_cgm{"ilv_cgm"};
    sc_core::sc_in<sc_dt::sc_lv<4>> key{"key"};
    sc_core::sc_in<sc_dt::sc_lv<4>> keyz{"keyz"};
    sc_core::sc_in<sc_dt::sc_logic> on{"on"};
    sc_core::sc_in<sc_dt::sc_logic> onz{"onz"};
    sc_core::sc_out<sc_dt::sc_logic> ibias1{"ibias1"};
    sc_core::sc_out<sc_dt::sc_logic> ibias2{"ibias2"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_preamp_current_sources_lv);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_preamp_current_sources_lv(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> key_b{"key_b", 4, vl::make_z1};
    sc_core::sc_signal_resolved ilv_cc_int{"ilv_cc_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ilv_cgm_int{"ilv_cgm_int", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source> I0_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source> I1_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source> I2_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source> I3_2_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source> I3_2_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source> I28_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source> I28_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source> I29;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source> I30;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source> I32;

    void split_key();
    vl::logic<4> rd_key() const;
    void gate_ilv_cgm_int_buf0();
    void gate_ilv_cgm_int_buf1();
    void gate_ilv_cc_int_buf0();
    void gate_ilv_cc_int_buf1();
};

#endif
