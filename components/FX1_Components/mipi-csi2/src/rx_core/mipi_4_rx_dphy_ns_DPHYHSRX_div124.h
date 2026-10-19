//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_div124
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DIV124_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DIV124_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_div124_core.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_div124 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk{"clk"};
    sc_core::sc_in<sc_dt::sc_logic> clk2dig_int{"clk2dig_int"};
    sc_core::sc_in<sc_dt::sc_lv<2>> clk_div{"clk_div"};
    sc_core::sc_in<sc_dt::sc_logic> clk_en{"clk_en"};
    sc_core::sc_in<sc_dt::sc_logic> clk_m_cal_int{"clk_m_cal_int"};
    sc_core::sc_in<sc_dt::sc_logic> clk_p_cal_int{"clk_p_cal_int"};
    sc_core::sc_in<sc_dt::sc_logic> pon{"pon"};
    sc_core::sc_in<sc_dt::sc_logic> tie_h_cdm{"tie_h_cdm"};
    sc_core::sc_in<sc_dt::sc_logic> tie_l_cdm{"tie_l_cdm"};
    sc_core::sc_out<sc_dt::sc_logic> clk2dig{"clk2dig"};
    sc_core::sc_out<sc_dt::sc_logic> clk_m_cal{"clk_m_cal"};
    sc_core::sc_out<sc_dt::sc_logic> clk_out_div{"clk_out_div"};
    sc_core::sc_out<sc_dt::sc_logic> clk_p_cal{"clk_p_cal"};
    sc_core::sc_out<sc_dt::sc_logic> tie_h_cdm_vdd{"tie_h_cdm_vdd"};
    sc_core::sc_out<sc_dt::sc_logic> tie_l_cdm_vdd{"tie_l_cdm_vdd"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vdd{"vdd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vss{"vss"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_div124);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_div124(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved rst{"rst", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> net1187{"net1187", 2, vl::make_z1};
    sc_core::sc_signal_resolved clk_n_soc{"clk_n_soc", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk2dig_in{"clk2dig_in", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1172{"net1172", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2l_cdm_tie_LowHigh> TIE_CDM;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> sc_lvt_I153;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> sc_lvt_I156_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> sc_lvt_I156_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> sc_lvt_I_CDM_OR;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> sc_lvt_I119;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I83;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I155_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt> I155_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx6_lvt> sc_lvt_I151;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_div124_core> I150;

    vl::logic<2> rd_net1187() const;
};

#endif
