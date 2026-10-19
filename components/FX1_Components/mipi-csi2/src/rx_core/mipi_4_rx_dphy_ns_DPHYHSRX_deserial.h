//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_deserial
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DESERIAL_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DESERIAL_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_des_align.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_des_des4.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_des_div4.h"
#include "mipi_4_rx_dphy_ns_sc_ffx23_lvt.h"
#include "mipi_4_rx_dphy_ns_sc_tie_hilow_lvt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_deserial : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk{"clk"};
    sc_core::sc_in<sc_dt::sc_logic> clk_dummy{"clk_dummy"};
    sc_core::sc_in<sc_dt::sc_logic> clk_en{"clk_en"};
    sc_core::sc_in<sc_dt::sc_logic> d0{"d0"};
    sc_core::sc_in<sc_dt::sc_logic> d1{"d1"};
    sc_core::sc_in<sc_dt::sc_logic> data_en{"data_en"};
    sc_core::sc_in<sc_dt::sc_logic> div_en{"div_en"};
    sc_core::sc_in<sc_dt::sc_logic> en_logic{"en_logic"};
    sc_core::sc_in<sc_dt::sc_logic> gated_polarity{"gated_polarity"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low_cdm{"tie_low_cdm"};
    sc_core::sc_out<sc_dt::sc_lv<8>> data{"data"};
    sc_core::sc_out<sc_dt::sc_logic> tie_high_cdm_vdd{"tie_high_cdm_vdd"};
    sc_core::sc_out<sc_dt::sc_logic> tie_low_cdm_vdd{"tie_low_cdm_vdd"};
    sc_core::sc_out<sc_dt::sc_logic> word_clk{"word_clk"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vdd{"vdd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vss{"vss"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_deserial);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_deserial(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> des{"des", 8, vl::make_z1};
    sc_core::sc_signal_resolved tie_high{"tie_high", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> hs_data{"hs_data", 2, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> dataz{"dataz", 8, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> des2del{"des2del", 2, vl::make_z1};
    sc_core::sc_signal_resolved net1156{"net1156", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved hs_clk{"hs_clk", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_dig{"clk_dig", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<4> div5{"div5", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_resolved d1_dig{"d1_dig", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low{"tie_low", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved local_clk{"local_clk", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved align1{"align1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_dig_nn{"clk_dig_nn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_dig_n{"clk_dig_n", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved allign0{"allign0", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved d0_dig{"d0_dig", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> data_int{"data_int", 8, vl::make_z1};
    sc_core::sc_signal<sc_dt::sc_logic> data_swap{"data_swap", vl::logic<1>().sl()};
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> I113;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> I117;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> I131;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> NAND_CDM_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> NAND_CDM_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> NAND_CDM_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_ffx23_lvt> FF_7;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_ffx23_lvt> FF_6;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_ffx23_lvt> FF_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_ffx23_lvt> FF_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_ffx23_lvt> FF_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_ffx23_lvt> FF_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_ffx23_lvt> FF_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_ffx23_lvt> FF_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> INV_7;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> INV_6;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> INV_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> INV_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> INV_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> INV_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> INV_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> INV_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tie_hilow_lvt> tie;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x12_lvt> CDM_OUT_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x12_lvt> CDM_OUT_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x12_lvt> CDM_OUT_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_des_align> ALI;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_des_des4> des0;
    sc_core::sc_signal_rv<4> c_des0_rx_data{"c_des0_rx_data", vl::logic<4>::all_z().lv()};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_des_des4> des1;
    sc_core::sc_signal_rv<4> c_des1_rx_data{"c_des1_rx_data", vl::logic<4>::all_z().lv()};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_des_div4> DIV4;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_nand_lvt> I83_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_nand_lvt> I83_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2l_cdm_tie_LowHigh> TIE_CDM;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx96_lvt> I54;
    vl::action_queue aq_{"aq_"};

    vl::logic<8> rd_data_int() const;
    vl::logic<8> rd_dataz() const;
    vl::logic<8> rd_des() const;
    vl::logic<2> rd_des2del() const;
    vl::logic<2> rd_hs_data() const;
    void split_des0_rx_data();
    void split_des1_rx_data();
    void proc_initial_156155();
    void proc_always_156157();
};

#endif
