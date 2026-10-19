//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_div124_core
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DIV124_CORE_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DIV124_CORE_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt.h"
#include "mipi_4_rx_dphy_ns_sc_dlax23_as_lvt.h"
#include "mipi_4_rx_dphy_ns_sc_syncx10_ar_lvt.h"
#include "mipi_4_rx_dphy_ns_sc_syncx10_as_lvt.h"
#include "mipi_4_rx_dphy_ns_sc_tie_hilow_lvt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_div124_core : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk{"clk"};
    sc_core::sc_in<sc_dt::sc_lv<2>> clk_div{"clk_div"};
    sc_core::sc_in<sc_dt::sc_logic> clk_en{"clk_en"};
    sc_core::sc_in<sc_dt::sc_logic> rst{"rst"};
    sc_core::sc_in<sc_dt::sc_logic> tie_high_cdm_vdd{"tie_high_cdm_vdd"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low_cdm_vdd{"tie_low_cdm_vdd"};
    sc_core::sc_out<sc_dt::sc_logic> clk_out_div{"clk_out_div"};
    sc_core::sc_in<sc_dt::sc_logic> vdd{"vdd"};
    sc_core::sc_in<sc_dt::sc_logic> vss{"vss"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_div124_core);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_div124_core(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> clk_div_b{"clk_div_b", 2, vl::make_z1};
    sc_core::sc_signal_resolved tie_high{"tie_high", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_n_soc{"clk_n_soc", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rst_int{"rst_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> clk_div_in{"clk_div_in", 2, vl::make_z1};
    sc_core::sc_signal_resolved clk_p_soc{"clk_p_soc", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rst_socz{"rst_socz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rst_intz{"rst_intz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved s1z{"s1z", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved d1z{"d1z", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved on1z{"on1z", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved a2{"a2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved d1_in{"d1_in", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low{"tie_low", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_enz{"clk_enz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved s2z{"s2z", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_en_syncz{"clk_en_syncz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved b1{"b1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved b2{"b2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rst_soc{"rst_soc", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_en_sync{"clk_en_sync", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1120{"net1120", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1121{"net1121", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1122{"net1122", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved on1{"on1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_n{"clk_n", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> clk_divz{"clk_divz", 2, vl::make_z1};
    sc_core::sc_signal_resolved clk_p{"clk_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved d2_in{"d2_in", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved d1{"d1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved d2{"d2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved s1{"s1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved s2{"s2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_out_div_int{"clk_out_div_int", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tie_hilow_lvt> I114;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na4x6_lvt> I129;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na4x6_lvt> I130;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na3x6_lvt> I126;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na3x6_lvt> I131;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> logic0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> or_clk_en;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> I52;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> I108;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> I112;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> I115;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> neg_clk_en;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_syncx10_ar_lvt> dd1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt> I116;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt> I120;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_syncx10_as_lvt> I110;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_syncx10_as_lvt> I111;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_syncx10_as_lvt> dd2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx6_lvt> I124;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx6_lvt> I125;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx6_lvt> I127;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx6_lvt> Inv;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx6_lvt> inv_set_div_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx6_lvt> inv_set_div_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx6_lvt> logic1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_div_buf> buf_bypass;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_div_buf> buf_div;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I83;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_as_lvt> I117;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_as_lvt> I121;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> I128;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> en_set_div_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> en_set_div_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> I132;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> clk_neg;

    void split_clk_div();
    vl::logic<2> rd_clk_div() const;
    vl::logic<2> rd_clk_div_in() const;
    vl::logic<2> rd_clk_divz() const;
    void proc_always_156397();
};

#endif
