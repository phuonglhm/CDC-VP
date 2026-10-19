//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_des_align
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DES_ALIGN_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DES_ALIGN_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_sc_dlax39_as_lvt.h"
#include "mipi_4_rx_dphy_ns_sc_syncx10_ar_lvt.h"
#include "mipi_4_rx_dphy_ns_sc_syncx10_as_lvt.h"
#include "mipi_4_rx_dphy_ns_sc_tie_hilow_lvt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_des_align : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk{"clk"};
    sc_core::sc_in<sc_dt::sc_logic> clk_en{"clk_en"};
    sc_core::sc_in<sc_dt::sc_logic> d0{"d0"};
    sc_core::sc_in<sc_dt::sc_logic> d1{"d1"};
    sc_core::sc_in<sc_dt::sc_logic> data_en{"data_en"};
    sc_core::sc_in<sc_dt::sc_logic> gated_polarity{"gated_polarity"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> tie_high_cdm_vdd{"tie_high_cdm_vdd"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low_cdm_vdd{"tie_low_cdm_vdd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_out<sc_dt::sc_logic> align0{"align0"};
    sc_core::sc_out<sc_dt::sc_logic> align1{"align1"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_des_align);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_des_align(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved data_sync_enz{"data_sync_enz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pull_upz{"pull_upz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved d1_2_n{"d1_2_n", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved d0_3_n{"d0_3_n", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pull_down{"pull_down", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved d1_3_n{"d1_3_n", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkz{"clkz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved data_sync_en{"data_sync_en", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved d0_del{"d0_del", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved d1_del{"d1_del", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved d0_n{"d0_n", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved d1_n{"d1_n", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low{"tie_low", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1039{"net1039", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved sync_a{"sync_a", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_enz{"clk_enz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved d1_2{"d1_2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved sync_az{"sync_az", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_nand_lvt> I83;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_nand_lvt> I104;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x12_lvt> I124;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> I103;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tie_hilow_lvt> I1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax39_as_lvt> la_d0_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax39_as_lvt> la_d1_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax39_as_lvt> la_d1_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx48_lvt> invd0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx48_lvt> invf0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_syncx10_ar_lvt> sync2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_syncx10_as_lvt> sync;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> I107;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> I112;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> I125;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> inva;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_des_gated_buf> I110;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_des_gated_buf> I111;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x12_lvt> I126;
};

#endif
