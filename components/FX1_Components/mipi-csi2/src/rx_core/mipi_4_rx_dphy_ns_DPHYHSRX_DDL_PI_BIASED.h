//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_PI_BIASED_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_PI_BIASED_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> bn{"bn"};
    sc_core::sc_in<sc_dt::sc_logic> bp{"bp"};
    sc_core::sc_in<sc_dt::sc_logic> inA{"inA"};
    sc_core::sc_in<sc_dt::sc_logic> inB{"inB"};
    sc_core::sc_in<sc_dt::sc_logic> pd{"pd"};
    sc_core::sc_in<sc_dt::sc_logic> pu{"pu"};
    sc_core::sc_in<sc_dt::sc_logic> tie_high{"tie_high"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low{"tie_low"};
    sc_core::sc_in<sc_dt::sc_lv<2>> word{"word"};
    sc_core::sc_out<sc_dt::sc_logic> o{"o"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000.0;  // time precision (fs)
    // wire  [1:0] wordz;
    // wire  outA;
    // wire  net181;
    // wire  outB;
    // wire  net182;
    // wire  net183;
    // wire  ph_out_n;
    // wire  net184;
    // wire  net185;
    // wire  net186;
    // wire  net187;
    // wire  net188;
    // wire  net190;
    // wire  net189;
    // wire  net191;
    // wire  net192;
    // wire  net193;
    // wire  net194;
    // wire  net195;
    // wire  net196;
    // rpp  R38( .b(vp), .highC(outA), .lowC(ph_out_n) );
    // rpp  R39( .b(vp), .highC(ph_out_n), .lowC(outB) );
    // mipi_4_rx_dphy_ns_sc_invx4_lvt  I0_1( .a(word[1]), .z(wordz[1]), .gd(gd), .vp(vp) );
    // mipi_4_rx_dphy_ns_sc_invx4_lvt  I0_0( .a(word[0]), .z(wordz[0]), .gd(gd), .vp(vp) );
    // mipi_4_rx_dphy_ns_sc_invx48_lvt  e( .a(ph_out_n), .z(o), .gd(gd), .vp(vp) );
    // pfinlvt  M1( .B(vp), .D(outA), .G(inA), .S(net182) );
    // pfinlvt  N43( .B(vp), .D(vp), .G(ph_out_n), .S(vp) );
    // pfinlvt  P19( .B(vp), .D(net182), .G(word[1]), .S(net181) );
    // pfinlvt  P20( .B(vp), .D(net189), .G(bp), .S(vp) );
    // pfinlvt  P21( .B(vp), .D(net182), .G(word[0]), .S(net189) );
    // pfinlvt  P22( .B(vp), .D(net182), .G(tie_low), .S(net193) );
    // pfinlvt  P23( .B(vp), .D(net193), .G(bp), .S(vp) );
    // pfinlvt  P28( .B(vp), .D(net181), .G(bp), .S(vp) );
    // pfinlvt  P31( .B(vp), .D(net185), .G(bp), .S(vp) );
    // pfinlvt  P32( .B(vp), .D(net186), .G(wordz[0]), .S(net191) );
    // pfinlvt  P33( .B(vp), .D(net191), .G(bp), .S(vp) );
    // pfinlvt  P34( .B(vp), .D(net186), .G(wordz[1]), .S(net185) );
    // pfinlvt  P35( .B(vp), .D(net186), .G(tie_low), .S(net195) );
    // pfinlvt  P36( .B(vp), .D(net195), .G(bp), .S(vp) );
    // pfinlvt  P44( .B(vp), .D(outB), .G(inB), .S(net186) );
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> pi_delay{"pi_delay", 0.0};
    sc_core::sc_vector<sc_core::sc_signal_resolved> pi_chain{"pi_chain", 3, vl::make_z1};
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal<sc_dt::sc_logic> ph_out{"ph_out", vl::logic<1>().sl()};
    bool pi_delay_forced_ = false;
    sc_core::sc_event gate_pi_taps_0_ev;
    vl::logic<1> gate_pi_taps_0_pv = vl::logic<1>::all_z();
    bool gate_pi_taps_0_ev_pend = false;
    sc_core::sc_event gate_pi_taps_1_ev;
    vl::logic<1> gate_pi_taps_1_pv = vl::logic<1>::all_z();
    bool gate_pi_taps_1_ev_pend = false;
    sc_core::sc_event gate_pi_taps_2_ev;
    vl::logic<1> gate_pi_taps_2_pv = vl::logic<1>::all_z();
    bool gate_pi_taps_2_ev_pend = false;

    vl::logic<3> rd_pi_chain() const;
    void force_pi_delay(double v) { pi_delay_forced_ = true; pi_delay.write(v); }
    void release_pi_delay() { pi_delay_forced_ = false; }
    void assign_supply_ok();
    void gate_pi_taps_0();
    void gate_pi_taps_0_upd();
    void gate_pi_taps_1();
    void gate_pi_taps_1_upd();
    void gate_pi_taps_2();
    void gate_pi_taps_2_upd();
    void proc_initial_155484();
    void proc_always_155488();
    void proc_always_155496();
};

#endif
