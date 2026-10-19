//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSTX_serial
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSTX_SERIAL_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSTX_SERIAL_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYHSTX_serial_div4.h"
#include "mipi_4_rx_dphy_ns_DPHYHSTX_serial_store.h"
#include "mipi_4_rx_dphy_ns_sc_dffx10_ar_lvt.h"
#include "mipi_4_rx_dphy_ns_sc_dffx10_as_lvt.h"
#include "mipi_4_rx_dphy_ns_sc_tie_hilow_lvt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSTX_serial : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk_en{"clk_en"};
    sc_core::sc_in<sc_dt::sc_logic> clk_kill{"clk_kill"};
    sc_core::sc_in<sc_dt::sc_logic> clkout_stop{"clkout_stop"};
    sc_core::sc_in<sc_dt::sc_logic> div_en{"div_en"};
    sc_core::sc_in<sc_dt::sc_logic> phase_m{"phase_m"};
    sc_core::sc_in<sc_dt::sc_logic> phase_m_dummy{"phase_m_dummy"};
    sc_core::sc_in<sc_dt::sc_logic> phase_p{"phase_p"};
    sc_core::sc_in<sc_dt::sc_logic> phase_p_dummy{"phase_p_dummy"};
    sc_core::sc_in<sc_dt::sc_logic> serial_clk_en{"serial_clk_en"};
    sc_core::sc_in<sc_dt::sc_logic> serial_en{"serial_en"};
    sc_core::sc_in<sc_dt::sc_logic> tra_en{"tra_en"};
    sc_core::sc_in<sc_dt::sc_lv<8>> tx_data{"tx_data"};
    sc_core::sc_out<sc_dt::sc_logic> clk_buf_m{"clk_buf_m"};
    sc_core::sc_out<sc_dt::sc_logic> clk_buf_p{"clk_buf_p"};
    sc_core::sc_out<sc_dt::sc_logic> sym_a{"sym_a"};
    sc_core::sc_out<sc_dt::sc_logic> sym_b{"sym_b"};
    sc_core::sc_out<sc_dt::sc_logic> tra_a{"tra_a"};
    sc_core::sc_out<sc_dt::sc_logic> tra_b{"tra_b"};
    sc_core::sc_out<sc_dt::sc_logic> word_clk{"word_clk"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vpclk{"vpclk"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSTX_serial);
    explicit mipi_4_rx_dphy_ns_DPHYHSTX_serial(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved up2a{"up2a", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_high{"tie_high", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved serial_clk_en_int{"serial_clk_en_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved up3a{"up3a", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved serial_clk_en_int_del{"serial_clk_en_int_del", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_kill_m{"clk_kill_m", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1243{"net1243", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_kill_p{"clk_kill_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<4> clk_p{"clk_p", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_resolved tie_low{"tie_low", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved local_clk{"local_clk", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<4> clk_m{"clk_m", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_resolved clk2_div{"clk2_div", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved down2a{"down2a", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved serial_clk_enz{"serial_clk_enz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved up1{"up1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved up2{"up2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved serial_clk_en_intz{"serial_clk_en_intz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved up3{"up3", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved up4{"up4", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved down3a{"down3a", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_tree_on{"clk_tree_on", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved down1{"down1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_store_m{"clk_store_m", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved down2{"down2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved down3{"down3", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved down4{"down4", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1444{"net1444", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_store_p{"clk_store_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved kill1{"kill1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved kill2{"kill2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved kill3{"kill3", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved kill4{"kill4", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved kill5{"kill5", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1449{"net1449", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk1_div{"clk1_div", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved div_enz{"div_enz", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x12_lvt> ior_serial_clk_en;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dffx10_ar_lvt> KILL2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dffx10_ar_lvt> KILL4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dffx10_ar_lvt> KILL5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dffx10_as_lvt> KILL1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dffx10_as_lvt> KILL3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tgate_x8_lvt> TG1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tgate_x8_lvt> TG2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tgate_x8_lvt> TG3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tgate_x8_lvt> TG4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tgate_x8_lvt> TG5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tgate_x8_lvt> TG6;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tie_hilow_lvt> TIE_L_H;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx48_lvt> INV6A;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx48_lvt> INV6B;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> INVS2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INVS1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV_DIV;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSTX_serial_div4> div;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> INV2AA;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> INV2BB;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> INV3A;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> INV3B;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> INV4A;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> INV4B;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> INV5A;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> INV5B;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> INVS0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx6_lvt> INV1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx6_lvt> INV2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x12_lvt> NAND1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSTX_serial_store> store;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> NAND1A;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> NAND1B;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> NAND2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> NAND2A;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> NAND2B;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> NAND3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> INV1A;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> INV1B;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> INV2A;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> INV2B;
};

#endif
