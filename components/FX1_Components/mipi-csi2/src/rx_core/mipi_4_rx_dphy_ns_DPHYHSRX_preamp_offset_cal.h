//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_PREAMP_OFFSET_CAL_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_PREAMP_OFFSET_CAL_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_lv<2>> atb_sw_hv{"atb_sw_hv"};
    sc_core::sc_in<sc_dt::sc_lv<2>> atb_sw_hvz{"atb_sw_hvz"};
    sc_core::sc_in<sc_dt::sc_lv<6>> rx_hs_bcalz{"rx_hs_bcalz"};
    sc_core::sc_in<sc_dt::sc_logic> rx_pre_ponz{"rx_pre_ponz"};
    sc_core::sc_in<sc_dt::sc_logic> vbplv_ccm{"vbplv_ccm"};
    sc_core::sc_out<sc_dt::sc_logic> vcaln{"vcaln"};
    sc_core::sc_out<sc_dt::sc_logic> vcalp{"vcalp"};
    sc_core::sc_inout<sc_dt::sc_logic> atb_line{"atb_line"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> aux{"aux", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> timming_error{"timming_error", vl::logic<1>().sl()};
    vl::vreg<6> rx_hs_bcal_aux{"rx_hs_bcal_aux", vl::logic<6>().lv()};
    vl::vreg<6> rx_hs_bcal_ref_aux{"rx_hs_bcal_ref_aux", vl::logic<6>().lv()};
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<6> rx_hs_bcal_ref{"rx_hs_bcal_ref", vl::logic<6>::all_z().lv()};
    sc_core::sc_signal_resolved enable{"enable", sc_dt::SC_LOGIC_Z};
    // wire  net74;
    // wire  net75;
    // wire  net76;
    // wire  net77;
    // wire  net78;
    // wire  net84;
    // wire  net85;
    // wire  net70;
    // wire  net71;
    // rppwl  R27( .b(vp), .in(net71), .out(vcalp) );
    // rppwl  R28( .b(vp), .in(net70), .out(vcaln) );
    // rppwl  R30( .b(vp), .in(net84), .out(net85) );
    // rppwl  R31_0( .b(vp), .in(net85), .out(net78) );
    // rppwl  R31_1( .b(vp), .in(net85), .out(net78) );
    // rppwl  RCM( .b(vp), .in(net84), .out(gd) );
    // nfinhv  NATB0( .B(gd), .D(atb_line), .G(atb_sw_hv[1]), .S(net74) );
    // nfinhv  NATB1( .B(gd), .D(atb_line), .G(atb_sw_hv[0]), .S(net75) );
    // nfinhv  NATB2( .B(gd), .D(net75), .G(atb_sw_hvz[0]), .S(gd) );
    // nfinhv  NATB3( .B(gd), .D(net74), .G(atb_sw_hvz[1]), .S(gd) );
    // nfinhv  NATB4( .B(gd), .D(net74), .G(atb_sw_hv[1]), .S(net71) );
    // nfinhv  NATB5( .B(gd), .D(net75), .G(atb_sw_hv[0]), .S(net70) );
    // pfinlvt  PCCM( .B(vp), .D(net76), .G(vbplv_ccm), .S(vp) );
    // pfinlvt  PKEY_CCM( .B(vp), .D(net77), .G(rx_pre_ponz), .S(net76) );
    // DPHYHSRX_preamp_idac  DAC( .bcalz(rx_hs_bcalz), .l(vcaln), .r(vcalp),
    // .gd(gd), .i1(net77), .i2(net78), .vp(vp) );
    // // Instances of basic/ipin/symbol
    sc_core::sc_signal<sc_dt::sc_logic> internal_clk{"internal_clk", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_lv<6>> slicer_tol{"slicer_tol", vl::logic<6>().lv()};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> cfg_clk_period_2{"cfg_clk_period_2", 0.0};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> cfgclkfreq{"cfgclkfreq", 0.0};
    sc_core::sc_signal_rv<6> rx_hs_bcal{"rx_hs_bcal", vl::logic<6>::all_z().lv()};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> t0{"t0", 0.0};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> t1{"t1", 0.0};
    sc_core::sc_signal<sc_dt::sc_lv<32>> upper_boundary{"upper_boundary", vl::logic<32>().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<32>> lower_boundary{"lower_boundary", vl::logic<32>().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<6>> zero_plus_tolerance{"zero_plus_tolerance", vl::logic<6>().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<6>> zero_minus_tolerance{"zero_minus_tolerance", vl::logic<6>().lv()};
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> vcaln_r{"vcaln_r", vl::logic<1>().sl()};   // value of output vcaln
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> vcalp_r{"vcalp_r", vl::logic<1>().sl()};   // value of output vcalp

    void drive_vcaln();
    void drive_vcalp();
    void assign_rx_hs_bcal();
    void assign_supply_ok();
    void assign_enable();
    void assign_rx_hs_bcal_ref();
    void gate_atb_line_buf0();
    void gate_atb_line_buf1();
    void proc_initial_157579();
    void proc_initial_157580();
    void proc_initial_157590();
    void proc_always_157611();
    void proc_initial_157615();
    void proc_always_157624();
    void proc_always_157640();
    void proc_always_157653();
    void proc_initial_157667();
    void proc_always_157673();
    void proc_always_157683();
    void proc_always_157706();
};

#endif
