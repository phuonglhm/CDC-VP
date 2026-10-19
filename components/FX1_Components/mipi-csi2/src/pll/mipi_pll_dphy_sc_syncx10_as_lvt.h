//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_sc_syncx10_as_lvt
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_SC_SYNCX10_AS_LVT_H
#define VL_MIPI_PLL_DPHY_SC_SYNCX10_AS_LVT_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_pll_dphy_sc_syncx10_as_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> ck{"ck"};
    sc_core::sc_in<sc_dt::sc_logic> d{"d"};
    sc_core::sc_in<sc_dt::sc_logic> set{"set"};
    sc_core::sc_out<sc_dt::sc_logic> qb{"qb"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_pll_dphy_sc_syncx10_as_lvt);
    explicit mipi_pll_dphy_sc_syncx10_as_lvt(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    // va_suck  syncx10_as_a( .clk(ckb), .in_h(d_n), .in_s(d_n) );
    // va_suck  syncx10_as_b( .clk(ckb), .in_h(n2), .in_s(n2) );
    // mipi_pll_dphy_wire_cap  I226( .w(ckb) );
    // mipi_pll_dphy_wire_cap  I123( .w(n3) );
    // mipi_pll_dphy_wire_cap  I255( .w(reset_b) );
    // mipi_pll_dphy_wire_cap  I121( .w(n1) );
    // mipi_pll_dphy_wire_cap  I256( .w(kp1) );
    // mipi_pll_dphy_wire_cap  I254( .w(kp) );
    // mipi_pll_dphy_wire_cap  I225( .w(n2) );
    // mipi_pll_dphy_wire_cap  I252( .w(ff) );
    // mipi_pll_dphy_wire_cap  I224( .w(d_n) );
    // nfinlvt  N0( .B(gd), .D(ckb), .G(ck), .S(gd) );
    // nfinlvt  N4( .B(gd), .D(net481), .G(reset_b), .S(gd) );
    // nfinlvt  N98( .B(gd), .D(n3), .G(kp1), .S(net371) );
    // nfinlvt  N18b( .B(gd), .D(reset_b), .G(set), .S(gd) );
    // nfinlvt  N99( .B(gd), .D(net348), .G(ck), .S(gd) );
    // nfinlvt  N29( .B(gd), .D(n2), .G(n1), .S(gd) );
    // nfinlvt  N102( .B(gd), .D(net371), .G(ckb), .S(gd) );
    // nfinlvt  N91( .B(gd), .D(n1), .G(kp), .S(net348) );
    // nfinlvt  N91b( .B(gd), .D(kp), .G(set), .S(gd) );
    // nfinlvt  N31( .B(gd), .D(qb), .G(n3), .S(gd) );
    // nfinlvt  N1( .B(gd), .D(ff), .G(reset_b), .S(gd) );
    // nfinlvt  N6( .B(gd), .D(d_n), .G(ckb), .S(n1) );
    // nfinlvt  N101( .B(gd), .D(kp1), .G(n3), .S(net481) );
    // nfinlvt  N32( .B(gd), .D(n2), .G(ck), .S(n3) );
    // nfinlvt  N95( .B(gd), .D(kp), .G(n1), .S(gd) );
    // nfinlvt  N104( .B(gd), .D(d_n), .G(d), .S(ff) );
    // pfinlvt  P0( .B(vp), .D(ckb), .G(ck), .S(vp) );
    // pfinlvt  P17b( .B(vp), .D(reset_b), .G(set), .S(vp) );
    // pfinlvt  P103( .B(vp), .D(net372), .G(ck), .S(vp) );
    // pfinlvt  P10b( .B(vp), .D(kp1), .G(reset_b), .S(vp) );
    // pfinlvt  P105( .B(vp), .D(d_n), .G(d), .S(vp) );
    // pfinlvt  P30( .B(vp), .D(n2), .G(n1), .S(vp) );
    // pfinlvt  P2( .B(vp), .D(d_n), .G(reset_b), .S(vp) );
    // pfinlvt  P1( .B(vp), .D(n3), .G(kp1), .S(net372) );
    // pfinlvt  P3( .B(vp), .D(net474), .G(set), .S(vp) );
    // pfinlvt  P33( .B(vp), .D(qb), .G(n3), .S(vp) );
    // pfinlvt  P28( .B(vp), .D(d_n), .G(ck), .S(n1) );
    // pfinlvt  P34( .B(vp), .D(n2), .G(ckb), .S(n3) );
    // pfinlvt  P92( .B(vp), .D(kp), .G(n1), .S(net474) );
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> n1{"n1", vl::logic<1>().sl()};
    // va_suck  syncx10_as_a( .clk(ckb), .in_h(d_n), .in_s(d_n) );
    // va_suck  syncx10_as_b( .clk(ckb), .in_h(n2), .in_s(n2) );
    // mipi_pll_dphy_wire_cap  I226( .w(ckb) );
    // mipi_pll_dphy_wire_cap  I123( .w(n3) );
    // mipi_pll_dphy_wire_cap  I255( .w(reset_b) );
    // mipi_pll_dphy_wire_cap  I121( .w(n1) );
    // mipi_pll_dphy_wire_cap  I256( .w(kp1) );
    // mipi_pll_dphy_wire_cap  I254( .w(kp) );
    // mipi_pll_dphy_wire_cap  I225( .w(n2) );
    // mipi_pll_dphy_wire_cap  I252( .w(ff) );
    // mipi_pll_dphy_wire_cap  I224( .w(d_n) );
    // nfinlvt  N0( .B(gd), .D(ckb), .G(ck), .S(gd) );
    // nfinlvt  N4( .B(gd), .D(net481), .G(reset_b), .S(gd) );
    // nfinlvt  N98( .B(gd), .D(n3), .G(kp1), .S(net371) );
    // nfinlvt  N18b( .B(gd), .D(reset_b), .G(set), .S(gd) );
    // nfinlvt  N99( .B(gd), .D(net348), .G(ck), .S(gd) );
    // nfinlvt  N29( .B(gd), .D(n2), .G(n1), .S(gd) );
    // nfinlvt  N102( .B(gd), .D(net371), .G(ckb), .S(gd) );
    // nfinlvt  N91( .B(gd), .D(n1), .G(kp), .S(net348) );
    // nfinlvt  N91b( .B(gd), .D(kp), .G(set), .S(gd) );
    // nfinlvt  N31( .B(gd), .D(qb), .G(n3), .S(gd) );
    // nfinlvt  N1( .B(gd), .D(ff), .G(reset_b), .S(gd) );
    // nfinlvt  N6( .B(gd), .D(d_n), .G(ckb), .S(n1) );
    // nfinlvt  N101( .B(gd), .D(kp1), .G(n3), .S(net481) );
    // nfinlvt  N32( .B(gd), .D(n2), .G(ck), .S(n3) );
    // nfinlvt  N95( .B(gd), .D(kp), .G(n1), .S(gd) );
    // nfinlvt  N104( .B(gd), .D(d_n), .G(d), .S(ff) );
    // pfinlvt  P0( .B(vp), .D(ckb), .G(ck), .S(vp) );
    // pfinlvt  P17b( .B(vp), .D(reset_b), .G(set), .S(vp) );
    // pfinlvt  P103( .B(vp), .D(net372), .G(ck), .S(vp) );
    // pfinlvt  P10b( .B(vp), .D(kp1), .G(reset_b), .S(vp) );
    // pfinlvt  P105( .B(vp), .D(d_n), .G(d), .S(vp) );
    // pfinlvt  P30( .B(vp), .D(n2), .G(n1), .S(vp) );
    // pfinlvt  P2( .B(vp), .D(d_n), .G(reset_b), .S(vp) );
    // pfinlvt  P1( .B(vp), .D(n3), .G(kp1), .S(net372) );
    // pfinlvt  P3( .B(vp), .D(net474), .G(set), .S(vp) );
    // pfinlvt  P33( .B(vp), .D(qb), .G(n3), .S(vp) );
    // pfinlvt  P28( .B(vp), .D(d_n), .G(ck), .S(n1) );
    // pfinlvt  P34( .B(vp), .D(n2), .G(ckb), .S(n3) );
    // pfinlvt  P92( .B(vp), .D(kp), .G(n1), .S(net474) );
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> n3{"n3", vl::logic<1>().sl()};

    void assign_supply_ok();
    void assign_qb();
    void proc_initial_1009();
    void proc_initial_1010();
    void proc_always_1012();
    void proc_always_1024();
};

#endif
