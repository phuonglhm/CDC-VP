//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_des_div4
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DES_DIV4_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DES_DIV4_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt.h"
#include "mipi_4_rx_dphy_ns_sc_dlax23_as_lvt.h"
#include "mipi_4_rx_dphy_ns_sc_srlax12_lvt.h"
#include "mipi_4_rx_dphy_ns_sc_tie_hilow_lvt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_des_div4 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk_en{"clk_en"};
    sc_core::sc_in<sc_dt::sc_logic> div_en{"div_en"};
    sc_core::sc_in<sc_dt::sc_logic> pll_clk{"pll_clk"};
    sc_core::sc_out<sc_dt::sc_lv<4>> div5{"div5"};
    sc_core::sc_out<sc_dt::sc_logic> local_clk{"local_clk"};
    sc_core::sc_out<sc_dt::sc_logic> word_clk{"word_clk"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_des_div4);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_des_div4(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> div5_b{"div5_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> sel{"sel", 4, vl::make_z1};
    sc_core::sc_signal_resolved tie_high{"tie_high", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> net1083{"net1083", 4, vl::make_z1};
    sc_core::sc_signal_resolved net943{"net943", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net944{"net944", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1076{"net1076", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pwr_on{"pwr_on", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net952{"net952", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1012{"net1012", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pwr_off{"pwr_off", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1015{"net1015", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1017{"net1017", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1018{"net1018", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net963{"net963", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1021{"net1021", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low{"tie_low", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net972{"net972", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net975{"net975", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1033{"net1033", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1034{"net1034", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net980{"net980", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1041{"net1041", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_m{"clk_m", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1042{"net1042", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net986{"net986", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1044{"net1044", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_p{"clk_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net930{"net930", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tie_hilow_lvt> TIE;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> I177;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> I291;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I176;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> INV1_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> INV1_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> INV1_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> INV1_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> I268;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> I272;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> I281;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x12_lvt> I299;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na3x16_lvt> I260;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx64_lvt> I292;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx64_lvt> I293;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> I239;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> I287;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> I288;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> I290;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x12_lvt> I186;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x12_lvt> I262;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> I265;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> I269;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> I273;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> I274;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt> I264;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt> I266;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt> I267;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt> I270;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt> I271;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt> I279;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt> I280;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_srlax12_lvt> I286;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_as_lvt> I258;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_as_lvt> I259;

    void merge_div5();
    vl::logic<4> rd_div5() const;
    vl::logic<4> rd_net1083() const;
    vl::logic<4> rd_sel() const;
};

#endif
