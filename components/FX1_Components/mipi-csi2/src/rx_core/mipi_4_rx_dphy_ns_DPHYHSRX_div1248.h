//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_div1248
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DIV1248_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DIV1248_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_div124_core.h"
#include "mipi_4_rx_dphy_ns_sc_tie_hilow_lvt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_div1248 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk{"clk"};
    sc_core::sc_in<sc_dt::sc_lv<2>> clk_div{"clk_div"};
    sc_core::sc_in<sc_dt::sc_logic> clk_en{"clk_en"};
    sc_core::sc_in<sc_dt::sc_logic> pon{"pon"};
    sc_core::sc_in<sc_dt::sc_logic> tie_h_cdm_vdd{"tie_h_cdm_vdd"};
    sc_core::sc_in<sc_dt::sc_logic> tie_l_cdm{"tie_l_cdm"};
    sc_core::sc_in<sc_dt::sc_logic> tie_l_cdm_vdd{"tie_l_cdm_vdd"};
    sc_core::sc_out<sc_dt::sc_logic> clk_out_div{"clk_out_div"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vdd{"vdd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vss{"vss"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_div1248);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_div1248(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> clk_div_b{"clk_div_b", 2, vl::make_z1};
    sc_core::sc_signal_resolved tie_high{"tie_high", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rst{"rst", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_n_soc{"clk_n_soc", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved div_confz{"div_confz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1035{"net1035", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low{"tie_low", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved div_conf{"div_conf", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved div_beta{"div_beta", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_div124_core> I18;
    sc_core::sc_signal_rv<2> c_I18_clk_div{"c_I18_clk_div", vl::logic<2>::all_z().lv()};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_div124_core> div;
    sc_core::sc_signal_rv<2> c_div_clk_div{"c_div_clk_div", vl::logic<2>::all_z().lv()};
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> I20;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> I119;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I120;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tie_hilow_lvt> I117;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> I22;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx6_lvt> I151;

    void split_clk_div();
    vl::logic<2> rd_clk_div() const;
    void glue_I18_clk_div();
    void glue_div_clk_div();
};

#endif
