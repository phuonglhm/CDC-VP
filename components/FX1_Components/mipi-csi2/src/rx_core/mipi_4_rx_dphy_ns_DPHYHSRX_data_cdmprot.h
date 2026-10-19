//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DATA_CDMPROT_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DATA_CDMPROT_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk_from_long_channel{"clk_from_long_channel"};
    sc_core::sc_in<sc_dt::sc_logic> compatibility_mode{"compatibility_mode"};
    sc_core::sc_in<sc_dt::sc_logic> data_offset_cal_en{"data_offset_cal_en"};
    sc_core::sc_in<sc_dt::sc_lv<4>> ddl_phase_data{"ddl_phase_data"};
    sc_core::sc_in<sc_dt::sc_logic> en{"en"};
    sc_core::sc_in<sc_dt::sc_logic> enlogic{"enlogic"};
    sc_core::sc_in<sc_dt::sc_logic> feed_int_clk{"feed_int_clk"};
    sc_core::sc_in<sc_dt::sc_lv<2>> gmode{"gmode"};
    sc_core::sc_in<sc_dt::sc_lv<6>> offset_cal{"offset_cal"};
    sc_core::sc_in<sc_dt::sc_logic> phase_change_p{"phase_change_p"};
    sc_core::sc_in<sc_dt::sc_logic> pon{"pon"};
    sc_core::sc_out<sc_dt::sc_logic> clk_from_long_channellv{"clk_from_long_channellv"};
    sc_core::sc_out<sc_dt::sc_logic> compatibility_onlv{"compatibility_onlv"};
    sc_core::sc_out<sc_dt::sc_logic> compatibility_onlvz{"compatibility_onlvz"};
    sc_core::sc_out<sc_dt::sc_logic> data_offset_cal_enlv{"data_offset_cal_enlv"};
    sc_core::sc_out<sc_dt::sc_lv<4>> ddl_phase_datalv{"ddl_phase_datalv"};
    sc_core::sc_out<sc_dt::sc_logic> enlogiclv{"enlogiclv"};
    sc_core::sc_out<sc_dt::sc_logic> enlogiclvz{"enlogiclvz"};
    sc_core::sc_out<sc_dt::sc_logic> enlv{"enlv"};
    sc_core::sc_out<sc_dt::sc_logic> enlvz{"enlvz"};
    sc_core::sc_out<sc_dt::sc_logic> feed_int_clklv{"feed_int_clklv"};
    sc_core::sc_out<sc_dt::sc_lv<2>> gmodelv{"gmodelv"};
    sc_core::sc_out<sc_dt::sc_lv<6>> offset_callv{"offset_callv"};
    sc_core::sc_out<sc_dt::sc_logic> phase_change_plv{"phase_change_plv"};
    sc_core::sc_out<sc_dt::sc_logic> ponlv{"ponlv"};
    sc_core::sc_out<sc_dt::sc_logic> ponlvz{"ponlvz"};
    sc_core::sc_out<sc_dt::sc_logic> tie_high_cdm{"tie_high_cdm"};
    sc_core::sc_out<sc_dt::sc_logic> tie_low_cdm{"tie_low_cdm"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> ddl_phase_data_b{"ddl_phase_data_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> gmode_b{"gmode_b", 2, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> offset_cal_b{"offset_cal_b", 6, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> ddl_phase_datalv_b{"ddl_phase_datalv_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> gmodelv_b{"gmodelv_b", 2, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> offset_callv_b{"offset_callv_b", 6, vl::make_z1};
    sc_core::sc_signal_resolved compatibility_modelv{"compatibility_modelv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> net205{"net205", 5, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> net200{"net200", 5, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> net199{"net199", 6, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> net197{"net197", 4, vl::make_z1};
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2l_cdm_tie_LowHigh> TIE_CDM;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> CDM1A_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> CDM1A_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> CDM1A_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> CDM1A_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> CDM1A_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> CDM1A_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I92_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I92_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I92_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I92_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I92_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I93_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I93_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I93_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I93_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I93_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I94_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I94_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I94_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I94_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> sc_lvt_I90;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I18_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I18_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I18_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I18_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I18_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I71_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I71_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I71_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I76_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I76_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I76_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I76_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I95;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I272_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I272_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I272_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I272_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I272_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I273_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I273_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I273_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I273_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I273_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> sc_lvt_I273_0;

    void split_ddl_phase_data();
    void split_gmode();
    void split_offset_cal();
    void merge_ddl_phase_datalv();
    void merge_gmodelv();
    void merge_offset_callv();
    vl::logic<4> rd_ddl_phase_data() const;
    vl::logic<4> rd_ddl_phase_datalv() const;
    vl::logic<2> rd_gmode() const;
    vl::logic<2> rd_gmodelv() const;
    vl::logic<4> rd_net197() const;
    vl::logic<6> rd_net199() const;
    vl::logic<5> rd_net200() const;
    vl::logic<5> rd_net205() const;
    vl::logic<6> rd_offset_cal() const;
    vl::logic<6> rd_offset_callv() const;
};

#endif
