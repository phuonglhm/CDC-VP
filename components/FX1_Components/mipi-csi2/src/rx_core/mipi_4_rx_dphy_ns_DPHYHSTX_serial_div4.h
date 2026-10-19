//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSTX_serial_div4
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSTX_SERIAL_DIV4_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSTX_SERIAL_DIV4_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt.h"
#include "mipi_4_rx_dphy_ns_sc_dlax23_as_lvt.h"
#include "mipi_4_rx_dphy_ns_sc_srlax12_lvt.h"
#include "mipi_4_rx_dphy_ns_sc_tie_hilow_lvt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSTX_serial_div4 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk_en{"clk_en"};
    sc_core::sc_in<sc_dt::sc_logic> div_en{"div_en"};
    sc_core::sc_in<sc_dt::sc_logic> pll_clk{"pll_clk"};
    sc_core::sc_out<sc_dt::sc_logic> local_clk{"local_clk"};
    sc_core::sc_out<sc_dt::sc_lv<4>> sel_m{"sel_m"};
    sc_core::sc_out<sc_dt::sc_lv<4>> sel_p{"sel_p"};
    sc_core::sc_out<sc_dt::sc_logic> word_clk{"word_clk"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSTX_serial_div4);
    explicit mipi_4_rx_dphy_ns_DPHYHSTX_serial_div4(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> sel_m_b{"sel_m_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> sel_p_b{"sel_p_b", 4, vl::make_z1};
    sc_core::sc_signal_resolved tie_high{"tie_high", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> sel{"sel", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> pg{"pg", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> pgz2{"pgz2", 4, vl::make_z1};
    sc_core::sc_signal_resolved pwr_on{"pwr_on", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved n1{"n1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved n2{"n2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved n3{"n3", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved n4{"n4", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved n5{"n5", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pwr_off{"pwr_off", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved n6{"n6", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved n7{"n7", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved sr1{"sr1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved n8{"n8", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved sr2{"sr2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved n9{"n9", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved sr3{"sr3", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved sr4{"sr4", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low{"tie_low", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> pgz1{"pgz1", 4, vl::make_z1};
    sc_core::sc_signal_resolved set{"set", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved reset_n{"reset_n", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_m{"clk_m", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_p{"clk_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved n10{"n10", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved n11{"n11", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved n12{"n12", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved n13{"n13", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved n14{"n14", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved setz{"setz", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tie_hilow_lvt> TIE;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tgate_x12_lvt> TG_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tgate_x12_lvt> TG_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tgate_x12_lvt> TG_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tgate_x12_lvt> TG_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> INVCLK;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> INVO5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> INV6;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> INV8;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> INV10;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x12_lvt> NORO1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na3x16_lvt> NAND2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx64_lvt> INV07;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx64_lvt> INVO6;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV1_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV1_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV1_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV1_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INVEN;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INVO1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INVO2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INVO3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x12_lvt> ANDEN;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> INVCLK2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> INV2_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> INV2_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> INV2_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> INV2_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> INV3_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> INV3_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> INV3_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> INV3_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> INV4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> INV5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> INV7;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> INV9;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> INVO4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt> LA3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt> LA4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt> LA5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt> LA6;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt> LA7;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt> LA8;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt> LA9;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_srlax12_lvt> SR;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_as_lvt> LA1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_as_lvt> LA2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x24_lvt> NAND1;

    void merge_sel_m();
    void merge_sel_p();
    vl::logic<4> rd_pg() const;
    vl::logic<4> rd_pgz1() const;
    vl::logic<4> rd_pgz2() const;
    vl::logic<4> rd_sel() const;
    vl::logic<4> rd_sel_m() const;
    vl::logic<4> rd_sel_p() const;
};

#endif
