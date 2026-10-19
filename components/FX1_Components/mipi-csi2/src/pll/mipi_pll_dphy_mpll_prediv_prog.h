//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_mpll_prediv_prog
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_MPLL_PREDIV_PROG_H
#define VL_MIPI_PLL_DPHY_MPLL_PREDIV_PROG_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_pll_dphy_mpll_mux2.h"
#include "mipi_pll_dphy_pll_tg_dffr.h"
#include "mipi_pll_dphy_sc_and2x6_lvt.h"
#include "mipi_pll_dphy_sc_or2x6_lvt.h"
#include "mipi_pll_dphy_sc_tie_hilow_lvt.h"
#include "pll_cells.h"

class mipi_pll_dphy_mpll_prediv_prog : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> ck_ref_in{"ck_ref_in"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_lv<2>> ref_cntrl{"ref_cntrl"};
    sc_core::sc_in<sc_dt::sc_logic> rstz{"rstz"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_out<sc_dt::sc_logic> ck_ref_out{"ck_ref_out"};

    SC_HAS_PROCESS(mipi_pll_dphy_mpll_prediv_prog);
    explicit mipi_pll_dphy_mpll_prediv_prog(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> ref_cntrl_b{"ref_cntrl_b", 2, vl::make_z1};
    sc_core::sc_signal_resolved rstz_24{"rstz_24", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> refcntrl_n{"refcntrl_n", 2, vl::make_z1};
    sc_core::sc_signal_resolved vp_esd{"vp_esd", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rst{"rst", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved a{"a", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ck_d4_m{"ck_d4_m", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved b{"b", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ck_d2_p{"ck_d2_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> refcntrl{"refcntrl", 2, vl::make_z1};
    sc_core::sc_signal_resolved net525{"net525", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved en_1{"en_1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved b1{"b1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net526{"net526", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved en_2{"en_2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved d{"d", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ck_d4_p{"ck_d4_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net510{"net510", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net482{"net482", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved en_3{"en_3", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net511{"net511", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved en_4{"en_4", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved d2b{"d2b", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ck_m_pre{"ck_m_pre", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ck_m{"ck_m", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rst_3{"rst_3", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rstz_3{"rstz_3", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ck_p{"ck_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rst_24{"rst_24", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved gd_esd{"gd_esd", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved d1b{"d1b", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_div3{"clk_div3", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ck_d2_m{"ck_d2_m", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_sc_no2x6_lvt> na6;
    std::unique_ptr<mipi_pll_dphy_sc_no2x6_lvt> na7;
    std::unique_ptr<mipi_pll_dphy_sc_no2x6_lvt> na8;
    std::unique_ptr<mipi_pll_dphy_sc_no2x6_lvt> na9;
    std::unique_ptr<mipi_pll_dphy_mpll_mux2> I45;
    sc_core::sc_signal_rv<4> c_I45_in{"c_I45_in", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_rv<4> c_I45_s{"c_I45_s", vl::logic<4>::all_z().lv()};
    std::unique_ptr<mipi_pll_dphy_pll_tg_dffr> I2;
    std::unique_ptr<mipi_pll_dphy_pll_tg_dffr> I83;
    std::unique_ptr<mipi_pll_dphy_pll_tg_dffr> ff0;
    std::unique_ptr<mipi_pll_dphy_pll_tg_dffr> pll_tg_dffr1;
    std::unique_ptr<mipi_pll_dphy_sc_or2x6_lvt> I10;
    std::unique_ptr<mipi_pll_dphy_sc_or2x6_lvt> I85;
    std::unique_ptr<mipi_pll_dphy_sc_tie_hilow_lvt> I58;
    std::unique_ptr<mipi_pll_dphy_sc_na2x12_lvt> I78;
    std::unique_ptr<mipi_pll_dphy_sc_na2x12_lvt> I79;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I5;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I6;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I12;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I70;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I71;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I77;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I80;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I81;
    std::unique_ptr<mipi_pll_dphy_sc_and2x6_lvt> I4;
    std::unique_ptr<mipi_pll_dphy_sc_and2x6_lvt> I9;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> I82;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> inv6_1;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> inv6_0;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> inv7_1;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> inv7_0;
    std::unique_ptr<mipi_pll_dphy_sc_tgate_x16_lvt> I84;

    void split_ref_cntrl();
    vl::logic<2> rd_ref_cntrl() const;
    vl::logic<2> rd_refcntrl() const;
    vl::logic<2> rd_refcntrl_n() const;
    void glue_I45_in();
    void glue_I45_s();
};

#endif
