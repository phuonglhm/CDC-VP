//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_preamp_data_current_sources_hv
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_PREAMP_DATA_CURRENT_SOURCES_HV_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_PREAMP_DATA_CURRENT_SOURCES_HV_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_preamp_data_current_sources_hv : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> ihv{"ihv"};
    sc_core::sc_in<sc_dt::sc_logic> keyhv{"keyhv"};
    sc_core::sc_in<sc_dt::sc_lv<4>> lbkeyhv{"lbkeyhv"};
    sc_core::sc_in<sc_dt::sc_logic> onhv{"onhv"};
    sc_core::sc_in<sc_dt::sc_logic> onhvz{"onhvz"};
    sc_core::sc_out<sc_dt::sc_logic> ibias_cal{"ibias_cal"};
    sc_core::sc_out<sc_dt::sc_logic> ibias_eq_l{"ibias_eq_l"};
    sc_core::sc_out<sc_dt::sc_logic> ibias_eq_r{"ibias_eq_r"};
    sc_core::sc_out<sc_dt::sc_logic> ibias_lb{"ibias_lb"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_preamp_data_current_sources_hv);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_preamp_data_current_sources_hv(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> lbkeyhv_b{"lbkeyhv_b", 4, vl::make_z1};
    sc_core::sc_signal_resolved ihv_int{"ihv_int", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_57;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_56;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_55;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_54;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_53;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_52;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_51;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_50;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_49;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_48;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_47;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_46;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_45;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_44;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_43;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_42;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_41;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_40;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_39;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_38;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_37;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_36;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_35;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_34;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_33;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_32;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_31;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_30;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_29;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_28;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_27;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_26;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_25;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_24;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_23;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_22;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_21;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_20;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_19;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_18;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_17;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_16;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_15;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_14;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_13;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_12;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_11;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_10;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_9;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_8;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_7;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_6;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy> I21_0;
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
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> IEQL_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> IEQL_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> IEQL_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> IEQL_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> IEQL_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> IEQR_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> IEQR_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> IEQR_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> IEQR_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv> IEQR_0;

    void split_lbkeyhv();
    vl::logic<4> rd_lbkeyhv() const;
    void gate_ihv_int_buf0();
    void gate_ihv_int_buf1();
};

#endif
