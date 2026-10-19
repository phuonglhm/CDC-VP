//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_mpll_l2h
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_MPLL_L2H_H
#define VL_MIPI_PLL_DPHY_MPLL_L2H_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_pll_dphy_mpll_rx_ckdrv_xor.h"
#include "mipi_pll_dphy_mpll_rx_dcc_del_x0.h"
#include "mipi_pll_dphy_sc_tie_hilow_lvt.h"
#include "pll_cells.h"

class mipi_pll_dphy_mpll_l2h : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_lv<8>> ck_l2h{"ck_l2h"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> highspeed_n{"highspeed_n"};
    sc_core::sc_in<sc_dt::sc_logic> pwron{"pwron"};
    sc_core::sc_in<sc_dt::sc_logic> quad_on{"quad_on"};
    sc_core::sc_inout<sc_dt::sc_logic> vci{"vci"};
    sc_core::sc_in<sc_dt::sc_logic> vcim{"vcim"};
    sc_core::sc_in<sc_dt::sc_logic> vcip{"vcip"};
    sc_core::sc_inout<sc_dt::sc_logic> vcq{"vcq"};
    sc_core::sc_in<sc_dt::sc_logic> vcqm{"vcqm"};
    sc_core::sc_in<sc_dt::sc_logic> vcqp{"vcqp"};
    sc_core::sc_in<sc_dt::sc_logic> vreg_mpll{"vreg_mpll"};
    sc_core::sc_out<sc_dt::sc_logic> ckdcc_i_m{"ckdcc_i_m"};
    sc_core::sc_out<sc_dt::sc_logic> ckdcc_i_p{"ckdcc_i_p"};
    sc_core::sc_out<sc_dt::sc_logic> ckdcc_q_m{"ckdcc_q_m"};
    sc_core::sc_out<sc_dt::sc_logic> ckdcc_q_p{"ckdcc_q_p"};
    sc_core::sc_out<sc_dt::sc_logic> clk_i_m{"clk_i_m"};
    sc_core::sc_out<sc_dt::sc_logic> clk_i_p{"clk_i_p"};
    sc_core::sc_out<sc_dt::sc_logic> clk_q_m{"clk_q_m"};
    sc_core::sc_out<sc_dt::sc_logic> clk_q_p{"clk_q_p"};
    sc_core::sc_out<sc_dt::sc_lv<4>> ph_pmix{"ph_pmix"};
    sc_core::sc_out<sc_dt::sc_logic> xor_clk{"xor_clk"};

    SC_HAS_PROCESS(mipi_pll_dphy_mpll_l2h);
    explicit mipi_pll_dphy_mpll_l2h(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> ck_l2h_b{"ck_l2h_b", 8, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> ph_pmix_b{"ph_pmix_b", 4, vl::make_z1};
    sc_core::sc_signal_resolved pwr_onz_rx{"pwr_onz_rx", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_high{"tie_high", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkqm1{"clkqm1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkip1{"clkip1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkim1{"clkim1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low{"tie_low", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved hs{"hs", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkqp1{"clkqp1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pwr_on{"pwr_on", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pwr_on_rx{"pwr_on_rx", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pwr_onz{"pwr_onz", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_mpll_rx_dcc_del_x0> del_iq1;
    std::unique_ptr<mipi_pll_dphy_mpll_rx_dcc_del_x0> del_iq2;
    std::unique_ptr<mipi_pll_dphy_mpll_rx_ckdrv_xor> xor_;
    std::unique_ptr<mipi_pll_dphy_mpll_l2h_dcc> Idcc0;
    std::unique_ptr<mipi_pll_dphy_mpll_l2h_dcc> Idcc1;
    std::unique_ptr<mipi_pll_dphy_mpll_l2h_dcc> Idcc2;
    std::unique_ptr<mipi_pll_dphy_mpll_l2h_dcc> Idcc3;
    // nfinlvt  N34_3( .B(gd), .D(gd), .G(ck_l2h[1]), .S(gd) );
    // nfinlvt  N34_2( .B(gd), .D(gd), .G(ck_l2h[3]), .S(gd) );
    // nfinlvt  N34_1( .B(gd), .D(gd), .G(ck_l2h[5]), .S(gd) );
    // nfinlvt  N34_0( .B(gd), .D(gd), .G(ck_l2h[7]), .S(gd) );
    // nfinlvt  dumN1( .B(gd), .D(gd), .G(gd), .S(gd) );
    std::unique_ptr<mipi_pll_dphy_sc_tie_hilow_lvt> I39;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> I43;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> I48;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> I57;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> I61;
    std::unique_ptr<mipi_pll_dphy_sc_na2x6_lvt> I47;

    void split_ck_l2h();
    void merge_ph_pmix();
    vl::logic<8> rd_ck_l2h() const;
    vl::logic<4> rd_ph_pmix() const;
};

#endif
