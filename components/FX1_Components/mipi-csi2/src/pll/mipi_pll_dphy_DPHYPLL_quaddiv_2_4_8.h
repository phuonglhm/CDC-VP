//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_quaddiv_2_4_8
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_DPHYPLL_QUADDIV_2_4_8_H
#define VL_MIPI_PLL_DPHY_DPHYPLL_QUADDIV_2_4_8_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_pll_dphy_sc_syncx10_ar_lvt.h"
#include "mipi_pll_dphy_sc_tie_hilow_lvt.h"
#include "pll_cells.h"

class mipi_pll_dphy_DPHYPLL_quaddiv_2_4_8 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> ck{"ck"};
    sc_core::sc_in<sc_dt::sc_logic> dgnd{"dgnd"};
    sc_core::sc_in<sc_dt::sc_logic> dvdd{"dvdd"};
    sc_core::sc_in<sc_dt::sc_lv<2>> p{"p"};
    sc_core::sc_in<sc_dt::sc_logic> pon_n{"pon_n"};
    sc_core::sc_out<sc_dt::sc_logic> q0{"q0"};
    sc_core::sc_out<sc_dt::sc_logic> q1{"q1"};
    sc_core::sc_out<sc_dt::sc_logic> q2{"q2"};
    sc_core::sc_out<sc_dt::sc_logic> q3{"q3"};

    SC_HAS_PROCESS(mipi_pll_dphy_DPHYPLL_quaddiv_2_4_8);
    explicit mipi_pll_dphy_DPHYPLL_quaddiv_2_4_8(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> p_b{"p_b", 2, vl::make_z1};
    sc_core::sc_signal_resolved net638{"net638", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_high{"tie_high", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net639{"net639", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ck_nn{"ck_nn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved q1x{"q1x", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net704{"net704", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ck_x1_div2{"ck_x1_div2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved q1y{"q1y", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net705{"net705", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ck_x2_n{"ck_x2_n", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved q3y{"q3y", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pon{"pon", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net707{"net707", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> p_buf{"p_buf", 2, vl::make_z1};
    sc_core::sc_signal_resolved q1xx{"q1xx", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved q0y{"q0y", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low{"tie_low", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved q2xx{"q2xx", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved q2x{"q2x", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ck_n{"ck_n", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ck_x2_div2{"ck_x2_div2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved q2y{"q2y", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ck_x1{"ck_x1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net601{"net601", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ck_x2{"ck_x2", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_sc_na2x12_lvt> I36;
    std::unique_ptr<mipi_pll_dphy_sc_mux2x8_lvt> I99;
    std::unique_ptr<mipi_pll_dphy_sc_mux2x8_lvt> I103;
    std::unique_ptr<mipi_pll_dphy_sc_invx32_lvt> I34;
    std::unique_ptr<mipi_pll_dphy_sc_invx32_lvt> I35;
    std::unique_ptr<mipi_pll_dphy_sc_invx32_lvt> I37;
    std::unique_ptr<mipi_pll_dphy_sc_tgate_x16_lvt> I71;
    std::unique_ptr<mipi_pll_dphy_sc_tgate_x16_lvt> I82;
    std::unique_ptr<mipi_pll_dphy_sc_tgate_x16_lvt> I83;
    std::unique_ptr<mipi_pll_dphy_sc_syncx10_ar_lvt> I102;
    std::unique_ptr<mipi_pll_dphy_sc_syncx10_ar_lvt> I104;
    std::unique_ptr<mipi_pll_dphy_sc_syncx10_ar_lvt> d1;
    std::unique_ptr<mipi_pll_dphy_sc_syncx10_ar_lvt> d2;
    std::unique_ptr<mipi_pll_dphy_sc_tie_hilow_lvt> I107;
    std::unique_ptr<mipi_pll_dphy_sc_invx24_lvt> I73;
    std::unique_ptr<mipi_pll_dphy_sc_invx24_lvt> I74;
    std::unique_ptr<mipi_pll_dphy_sc_invx24_lvt> I80;
    std::unique_ptr<mipi_pll_dphy_sc_invx24_lvt> I81;
    std::unique_ptr<mipi_pll_dphy_sc_invx24_lvt> I88;
    std::unique_ptr<mipi_pll_dphy_sc_invx24_lvt> I89;
    std::unique_ptr<mipi_pll_dphy_sc_invx24_lvt> I93;
    std::unique_ptr<mipi_pll_dphy_sc_invx24_lvt> I94;
    // sc_invx6_lvt  I78( .a(net707), .z(net601), .gd(dgnd), .vp(dvdd) );
    // mipi_pll_dphy_sc_invx6_lvt  I79( .a(net601), .z(net707), .gd(dgnd), .vp(dvdd) );
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I72;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I75;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I76;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I77;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I108;
    // // Instances of sc_lvt/sc_invx4_lvt/symbol
    // mipi_pll_dphy_sc_invx4_lvt  I84( .a(q2y), .z(q0y), .gd(dgnd), .vp(dvdd) );
    // mipi_pll_dphy_sc_invx4_lvt  I85( .a(q0y), .z(q2y), .gd(dgnd), .vp(dvdd) );
    // mipi_pll_dphy_sc_invx4_lvt  I86( .a(q1y), .z(q3y), .gd(dgnd), .vp(dvdd) );
    // mipi_pll_dphy_sc_invx4_lvt  I87( .a(q3y), .z(q1y), .gd(dgnd), .vp(dvdd) );
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> I91;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> I95;
    std::unique_ptr<mipi_pll_dphy_sc_loadx16_lvt> I96;

    void split_p();
    vl::logic<2> rd_p() const;
    vl::logic<2> rd_p_buf() const;
};

#endif
