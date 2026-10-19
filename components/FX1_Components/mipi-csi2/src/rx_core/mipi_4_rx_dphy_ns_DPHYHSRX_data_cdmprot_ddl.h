//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot_ddl
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DATA_CDMPROT_DDL_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DATA_CDMPROT_DDL_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot_ddl : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_lv<5>> ddl_phase_clk{"ddl_phase_clk"};
    sc_core::sc_in<sc_dt::sc_lv<5>> ddl_tune{"ddl_tune"};
    sc_core::sc_in<sc_dt::sc_logic> ddl_tune_en{"ddl_tune_en"};
    sc_core::sc_out<sc_dt::sc_lv<5>> ddl_phase_clklv{"ddl_phase_clklv"};
    sc_core::sc_out<sc_dt::sc_logic> ddl_tune_enlv{"ddl_tune_enlv"};
    sc_core::sc_out<sc_dt::sc_lv<5>> ddl_tunelv{"ddl_tunelv"};
    sc_core::sc_out<sc_dt::sc_logic> tie_high_cdm{"tie_high_cdm"};
    sc_core::sc_out<sc_dt::sc_logic> tie_low_cdm{"tie_low_cdm"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot_ddl);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot_ddl(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> ddl_phase_clk_b{"ddl_phase_clk_b", 5, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> ddl_tune_b{"ddl_tune_b", 5, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> ddl_phase_clklv_b{"ddl_phase_clklv_b", 5, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> ddl_tunelv_b{"ddl_tunelv_b", 5, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> net159{"net159", 11, vl::make_z1};
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2l_cdm_tie_LowHigh> TIE_CDM;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> I18_10;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> I18_9;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> I18_8;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> I18_7;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> I18_6;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> I18_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> I18_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> I18_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> I18_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> I18_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> I18_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I92_10;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I92_9;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I92_8;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I92_7;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I92_6;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I92_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I92_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I92_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I92_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I92_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I92_0;

    void split_ddl_phase_clk();
    void split_ddl_tune();
    void merge_ddl_phase_clklv();
    void merge_ddl_tunelv();
    vl::logic<5> rd_ddl_phase_clk() const;
    vl::logic<5> rd_ddl_phase_clklv() const;
    vl::logic<5> rd_ddl_tune() const;
    vl::logic<5> rd_ddl_tunelv() const;
    vl::logic<11> rd_net159() const;
};

#endif
