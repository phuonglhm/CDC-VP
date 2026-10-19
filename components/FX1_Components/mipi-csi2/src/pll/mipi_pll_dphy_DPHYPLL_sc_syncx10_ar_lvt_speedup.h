//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_sc_syncx10_ar_lvt_speedup
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_DPHYPLL_SC_SYNCX10_AR_LVT_SPEEDUP_H
#define VL_MIPI_PLL_DPHY_DPHYPLL_SC_SYNCX10_AR_LVT_SPEEDUP_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_pll_dphy_DPHYPLL_sc_syncx10_ar_lvt_speedup : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> ck{"ck"};
    sc_core::sc_in<sc_dt::sc_logic> d{"d"};
    sc_core::sc_in<sc_dt::sc_logic> reset_b{"reset_b"};
    sc_core::sc_out<sc_dt::sc_logic> qb{"qb"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_pll_dphy_DPHYPLL_sc_syncx10_ar_lvt_speedup);
    explicit mipi_pll_dphy_DPHYPLL_sc_syncx10_ar_lvt_speedup(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved n2{"n2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net361{"net361", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved kp{"kp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved set{"set", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net348{"net348", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net349{"net349", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ckb{"ckb", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved d_n{"d_n", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net371{"net371", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net372{"net372", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net355{"net355", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved kp1{"kp1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved dd_n{"dd_n", sc_dt::SC_LOGIC_Z};
    // va_suck  sync_m( .clk(ckb), .in_h(d_n), .in_s(d_n) );
    // va_suck  sync_s( .clk(ckb), .in_h(n2), .in_s(n2) );
    // mipi_pll_dphy_wire_cap  I121( .w(n1) );
    // mipi_pll_dphy_wire_cap  I123( .w(n3) );
    // mipi_pll_dphy_wire_cap  I224( .w(d_n) );
    // mipi_pll_dphy_wire_cap  I225( .w(n2) );
    // mipi_pll_dphy_wire_cap  I226( .w(ckb) );
    // mipi_pll_dphy_wire_cap  I252( .w(dd_n) );
    // mipi_pll_dphy_wire_cap  I254( .w(kp) );
    // mipi_pll_dphy_wire_cap  I255( .w(set) );
    // mipi_pll_dphy_wire_cap  I256( .w(kp1) );
    // nfinlvt  N0( .B(gd), .D(ckb), .G(ck), .S(gd) );
    // nfinlvt  N1( .B(gd), .D(net371), .G(ckb), .S(gd) );
    // nfinlvt  N3( .B(gd), .D(d_n), .G(set), .S(gd) );
    // nfinlvt  N18b( .B(gd), .D(set), .G(reset_b), .S(gd) );
    // nfinlvt  N29( .B(gd), .D(n2), .G(n1), .S(gd) );
    // nfinlvt  N31( .B(gd), .D(qb), .G(n3), .S(gd) );
    // nfinlvt  N91( .B(gd), .D(n1), .G(kp), .S(net348) );
    // nfinlvt  N91b( .B(gd), .D(kp1), .G(set), .S(gd) );
    // nfinlvt  N95( .B(gd), .D(kp), .G(n1), .S(net355) );
    // nfinlvt  N98( .B(gd), .D(n3), .G(kp1), .S(net371) );
    // nfinlvt  N99( .B(gd), .D(net348), .G(ck), .S(gd) );
    // nfinlvt  N100( .B(gd), .D(net355), .G(reset_b), .S(gd) );
    // nfinlvt  N101( .B(gd), .D(kp1), .G(n3), .S(gd) );
    // nfinlvt  N104( .B(gd), .D(d_n), .G(d), .S(gd) );
    // pfinlvt  P0( .B(vp), .D(ckb), .G(ck), .S(vp) );
    // pfinlvt  P1( .B(vp), .D(n3), .G(kp1), .S(net372) );
    // pfinlvt  P2( .B(vp), .D(net361), .G(set), .S(vp) );
    // pfinlvt  P10b( .B(vp), .D(kp), .G(reset_b), .S(vp) );
    // pfinlvt  P15b( .B(vp), .D(dd_n), .G(set), .S(vp) );
    // pfinlvt  P17b( .B(vp), .D(set), .G(reset_b), .S(vp) );
    // pfinlvt  P30( .B(vp), .D(n2), .G(n1), .S(vp) );
    // pfinlvt  P33( .B(vp), .D(qb), .G(n3), .S(vp) );
    // pfinlvt  P92( .B(vp), .D(kp), .G(n1), .S(vp) );
    // pfinlvt  P93( .B(vp), .D(n1), .G(kp), .S(net349) );
    // pfinlvt  P93b( .B(vp), .D(net349), .G(ckb), .S(vp) );
    // pfinlvt  P102( .B(vp), .D(kp1), .G(n3), .S(net361) );
    // pfinlvt  P103( .B(vp), .D(net372), .G(ck), .S(vp) );
    // pfinlvt  P105( .B(vp), .D(d_n), .G(d), .S(dd_n) );
    // nfinulvt  N6( .B(gd), .D(d_n), .G(ckb), .S(n1) );
    sc_core::sc_signal<sc_dt::sc_logic> n1{"n1", vl::logic<1>().sl()};
    // va_suck  sync_m( .clk(ckb), .in_h(d_n), .in_s(d_n) );
    // va_suck  sync_s( .clk(ckb), .in_h(n2), .in_s(n2) );
    // mipi_pll_dphy_wire_cap  I121( .w(n1) );
    // mipi_pll_dphy_wire_cap  I123( .w(n3) );
    // mipi_pll_dphy_wire_cap  I224( .w(d_n) );
    // mipi_pll_dphy_wire_cap  I225( .w(n2) );
    // mipi_pll_dphy_wire_cap  I226( .w(ckb) );
    // mipi_pll_dphy_wire_cap  I252( .w(dd_n) );
    // mipi_pll_dphy_wire_cap  I254( .w(kp) );
    // mipi_pll_dphy_wire_cap  I255( .w(set) );
    // mipi_pll_dphy_wire_cap  I256( .w(kp1) );
    // nfinlvt  N0( .B(gd), .D(ckb), .G(ck), .S(gd) );
    // nfinlvt  N1( .B(gd), .D(net371), .G(ckb), .S(gd) );
    // nfinlvt  N3( .B(gd), .D(d_n), .G(set), .S(gd) );
    // nfinlvt  N18b( .B(gd), .D(set), .G(reset_b), .S(gd) );
    // nfinlvt  N29( .B(gd), .D(n2), .G(n1), .S(gd) );
    // nfinlvt  N31( .B(gd), .D(qb), .G(n3), .S(gd) );
    // nfinlvt  N91( .B(gd), .D(n1), .G(kp), .S(net348) );
    // nfinlvt  N91b( .B(gd), .D(kp1), .G(set), .S(gd) );
    // nfinlvt  N95( .B(gd), .D(kp), .G(n1), .S(net355) );
    // nfinlvt  N98( .B(gd), .D(n3), .G(kp1), .S(net371) );
    // nfinlvt  N99( .B(gd), .D(net348), .G(ck), .S(gd) );
    // nfinlvt  N100( .B(gd), .D(net355), .G(reset_b), .S(gd) );
    // nfinlvt  N101( .B(gd), .D(kp1), .G(n3), .S(gd) );
    // nfinlvt  N104( .B(gd), .D(d_n), .G(d), .S(gd) );
    // pfinlvt  P0( .B(vp), .D(ckb), .G(ck), .S(vp) );
    // pfinlvt  P1( .B(vp), .D(n3), .G(kp1), .S(net372) );
    // pfinlvt  P2( .B(vp), .D(net361), .G(set), .S(vp) );
    // pfinlvt  P10b( .B(vp), .D(kp), .G(reset_b), .S(vp) );
    // pfinlvt  P15b( .B(vp), .D(dd_n), .G(set), .S(vp) );
    // pfinlvt  P17b( .B(vp), .D(set), .G(reset_b), .S(vp) );
    // pfinlvt  P30( .B(vp), .D(n2), .G(n1), .S(vp) );
    // pfinlvt  P33( .B(vp), .D(qb), .G(n3), .S(vp) );
    // pfinlvt  P92( .B(vp), .D(kp), .G(n1), .S(vp) );
    // pfinlvt  P93( .B(vp), .D(n1), .G(kp), .S(net349) );
    // pfinlvt  P93b( .B(vp), .D(net349), .G(ckb), .S(vp) );
    // pfinlvt  P102( .B(vp), .D(kp1), .G(n3), .S(net361) );
    // pfinlvt  P103( .B(vp), .D(net372), .G(ck), .S(vp) );
    // pfinlvt  P105( .B(vp), .D(d_n), .G(d), .S(dd_n) );
    // nfinulvt  N6( .B(gd), .D(d_n), .G(ckb), .S(n1) );
    sc_core::sc_signal<sc_dt::sc_logic> n3{"n3", vl::logic<1>().sl()};
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};

    void assign_supply_ok();
    void assign_qb();
    void proc_always_2375();
    void proc_always_2387();
};

#endif
