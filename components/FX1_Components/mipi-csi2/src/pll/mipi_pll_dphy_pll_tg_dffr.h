//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_pll_tg_dffr
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_PLL_TG_DFFR_H
#define VL_MIPI_PLL_DPHY_PLL_TG_DFFR_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_pll_dphy_pll_tg_dffr : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clkn{"clkn"};
    sc_core::sc_in<sc_dt::sc_logic> clkp{"clkp"};
    sc_core::sc_in<sc_dt::sc_logic> rst{"rst"};
    sc_core::sc_in<sc_dt::sc_logic> rst_n{"rst_n"};
    sc_core::sc_out<sc_dt::sc_logic> q{"q"};
    sc_core::sc_out<sc_dt::sc_logic> qb{"qb"};
    sc_core::sc_in<sc_dt::sc_logic> d{"d"};
    sc_core::sc_in<sc_dt::sc_logic> db{"db"};
    sc_core::sc_in<sc_dt::sc_logic> vddlv{"vddlv"};
    sc_core::sc_in<sc_dt::sc_logic> vssa{"vssa"};

    SC_HAS_PROCESS(mipi_pll_dphy_pll_tg_dffr);
    explicit mipi_pll_dphy_pll_tg_dffr(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    // sc_invx4_lvt  I9( .a(an), .z(a), .gd(vssa), .vp(vddlv) );
    // mipi_pll_dphy_sc_invx4_lvt  I8( .a(a), .z(an), .gd(vssa), .vp(vddlv) );
    // mipi_pll_dphy_sc_invx4_lvt  I7( .a(b), .z(bn), .gd(vssa), .vp(vddlv) );
    // mipi_pll_dphy_sc_invx4_lvt  I6( .a(bn), .z(b), .gd(vssa), .vp(vddlv) );
    // mipi_pll_dphy_sc_invx8_lvt  I3( .a(b), .z(q), .gd(vssa), .vp(vddlv) );
    // mipi_pll_dphy_sc_invx8_lvt  I2( .a(bn), .z(qb), .gd(vssa), .vp(vddlv) );
    // mipi_pll_dphy_sc_invx8_lvt  I4( .a(a), .z(_n626), .gd(vssa), .vp(vddlv) );
    // mipi_pll_dphy_sc_invx8_lvt  I1( .a(an), .z(_n622), .gd(vssa), .vp(vddlv) );
    // nfinlvt  g_nmos_lv5( .B(vssa), .D(a), .G(rst), .S(_n936) );
    // nfinlvt  g_nmos_lv4( .B(vssa), .D(_n936), .G(clkp), .S(vssa) );
    // nfinlvt  P14( .B(vssa), .D(bn), .G(rst), .S(vssa) );
    // sc_tgate_x8_lvt  tg_dffr_lv_1_2( .a(_n626), .c_m(clkn), .c_p(clkp), .z(b),
    // .gd(vssa), .vp(vddlv) );
    // sc_tgate_x8_lvt  I12( .a(d), .c_m(clkp), .c_p(clkn), .z(a), .gd(vssa),
    // .vp(vddlv) );
    // sc_tgate_x8_lvt  I16( .a(_n622), .c_m(clkn), .c_p(clkp), .z(bn), .gd(vssa),
    // .vp(vddlv) );
    // sc_tgate_x8_lvt  I15( .a(db), .c_m(clkp), .c_p(clkn), .z(an), .gd(vssa),
    // .vp(vddlv) );
    // pfinlvt  N18( .B(vddlv), .D(_n1015), .G(clkn), .S(vddlv) );
    // pfinlvt  P13( .B(vddlv), .D(b), .G(rst_n), .S(vddlv) );
    // pfinlvt  N17( .B(vddlv), .D(an), .G(rst_n), .S(_n1015) );
    sc_core::sc_signal<sc_dt::sc_logic> a{"a", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> an{"an", vl::logic<1>().sl()};
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkp_I12{"clkp_I12", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkn_I12{"clkn_I12", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkp_I15{"clkp_I15", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkn_I15{"clkn_I15", sc_dt::SC_LOGIC_Z};

    void assign_supply_ok();
    void assign_clkp_I12();
    void assign_clkn_I12();
    void assign_clkp_I15();
    void assign_clkn_I15();
    void proc_always_4763();
    void proc_always_4775();
    void proc_always_4787();
    void proc_always_4801();
};

#endif
