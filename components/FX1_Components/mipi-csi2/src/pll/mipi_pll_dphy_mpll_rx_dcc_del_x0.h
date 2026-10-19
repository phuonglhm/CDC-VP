//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_mpll_rx_dcc_del_x0
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_MPLL_RX_DCC_DEL_X0_H
#define VL_MIPI_PLL_DPHY_MPLL_RX_DCC_DEL_X0_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "pll_cells.h"

class mipi_pll_dphy_mpll_rx_dcc_del_x0 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk_i_m{"clk_i_m"};
    sc_core::sc_in<sc_dt::sc_logic> clk_i_p{"clk_i_p"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> hs{"hs"};
    sc_core::sc_in<sc_dt::sc_logic> pwr_on{"pwr_on"};
    sc_core::sc_in<sc_dt::sc_logic> pwr_onz{"pwr_onz"};
    sc_core::sc_inout<sc_dt::sc_logic> vdcc{"vdcc"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vp_tie{"vp_tie"};
    sc_core::sc_out<sc_dt::sc_logic> ckdcc_out_m{"ckdcc_out_m"};
    sc_core::sc_out<sc_dt::sc_logic> ckdcc_out_p{"ckdcc_out_p"};
    sc_core::sc_out<sc_dt::sc_logic> clk_o_m{"clk_o_m"};
    sc_core::sc_out<sc_dt::sc_logic> clk_o_p{"clk_o_p"};
    sc_core::sc_out<sc_dt::sc_logic> ph_pmix_out_m{"ph_pmix_out_m"};
    sc_core::sc_out<sc_dt::sc_logic> ph_pmix_out_p{"ph_pmix_out_p"};

    SC_HAS_PROCESS(mipi_pll_dphy_mpll_rx_dcc_del_x0);
    explicit mipi_pll_dphy_mpll_rx_dcc_del_x0(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved net1109{"net1109", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1110{"net1110", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1111{"net1111", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1112{"net1112", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vdcc_i{"vdcc_i", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1120{"net1120", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1119{"net1119", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1121{"net1121", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1122{"net1122", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> I97;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> I109;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> I112;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> invip;
    std::unique_ptr<mipi_pll_dphy_mpll_rx_dcc_del_x0_path> mpath;
    std::unique_ptr<mipi_pll_dphy_mpll_rx_dcc_del_x0_path> ppath;
    std::unique_ptr<mipi_pll_dphy_sc_invx24_lvt> I104;
    std::unique_ptr<mipi_pll_dphy_sc_invx24_lvt> I105;
    std::unique_ptr<mipi_pll_dphy_sc_invx24_lvt> I110;
    std::unique_ptr<mipi_pll_dphy_sc_invx24_lvt> I111;
    std::unique_ptr<mipi_pll_dphy_mpll_l2h_res> I103;
    std::unique_ptr<mipi_pll_dphy_mpll_l2h_res> I106;
    std::unique_ptr<mipi_pll_dphy_sc_invx48_lvt> I102;
    std::unique_ptr<mipi_pll_dphy_sc_invx48_lvt> I107;

    void gate_buf_gate();
};

#endif
