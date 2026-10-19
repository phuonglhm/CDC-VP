//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED.h"

mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED::mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vp << gd << tie_high << tie_low << bp << bn;
    SC_METHOD(gate_pi_taps_0);
    sensitive << inA;
    SC_METHOD(gate_pi_taps_0_upd);
    sensitive << gate_pi_taps_0_ev;
    dont_initialize();
    SC_METHOD(gate_pi_taps_1);
    sensitive << pi_chain[0];
    SC_METHOD(gate_pi_taps_1_upd);
    sensitive << gate_pi_taps_1_ev;
    dont_initialize();
    SC_METHOD(gate_pi_taps_2);
    sensitive << pi_chain[1];
    SC_METHOD(gate_pi_taps_2_upd);
    sensitive << gate_pi_taps_2_ev;
    dont_initialize();
    SC_METHOD(proc_initial_155484);
    SC_METHOD(proc_always_155488);
    sensitive << word << inA << pi_chain[0] << pi_chain[1] << pi_chain[2];
    SC_METHOD(proc_always_155496);
    sensitive << supply_ok << pd << pu << ph_out;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED::rd_pi_chain() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(pi_chain[0]));
    v.set_bit(1, vl::rd(pi_chain[1]));
    v.set_bit(2, vl::rd(pi_chain[2]));
    return v;
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED::assign_supply_ok() {
    vl::wr(supply_ok, ((((vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)) & vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))) & vl::ceq(vl::rd(tie_high), vl::logic<1>(0x1ULL))) & vl::ceq(vl::rd(tie_low), vl::logic<1>(0x0ULL))) & vl::ceq(vl::rd(bp), vl::logic<1>(0x0ULL))) & vl::ceq(vl::rd(bn), vl::logic<1>(0x1ULL)));
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED::gate_pi_taps_0() {
    const vl::logic<1> nv = vl::buf_(vl::rd(inA));
    const sc_core::sc_time d = vl::dly(pi_delay.read(), TU_, TP_);
    if (nv == gate_pi_taps_0_pv && !gate_pi_taps_0_ev_pend) return;
    gate_pi_taps_0_pv = nv;
    gate_pi_taps_0_ev.cancel();
    gate_pi_taps_0_ev_pend = true;
    gate_pi_taps_0_ev.notify(d);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED::gate_pi_taps_0_upd() {
    gate_pi_taps_0_ev_pend = false;
    const vl::logic<1> v_ = gate_pi_taps_0_pv;
    pi_chain[0].write(v_.bit(0).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED::gate_pi_taps_1() {
    const vl::logic<1> nv = vl::buf_(vl::rd(pi_chain[0]));
    const sc_core::sc_time d = vl::dly(pi_delay.read(), TU_, TP_);
    if (nv == gate_pi_taps_1_pv && !gate_pi_taps_1_ev_pend) return;
    gate_pi_taps_1_pv = nv;
    gate_pi_taps_1_ev.cancel();
    gate_pi_taps_1_ev_pend = true;
    gate_pi_taps_1_ev.notify(d);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED::gate_pi_taps_1_upd() {
    gate_pi_taps_1_ev_pend = false;
    const vl::logic<1> v_ = gate_pi_taps_1_pv;
    pi_chain[1].write(v_.bit(0).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED::gate_pi_taps_2() {
    const vl::logic<1> nv = vl::buf_(vl::rd(pi_chain[1]));
    const sc_core::sc_time d = vl::dly(pi_delay.read(), TU_, TP_);
    if (nv == gate_pi_taps_2_pv && !gate_pi_taps_2_ev_pend) return;
    gate_pi_taps_2_pv = nv;
    gate_pi_taps_2_ev.cancel();
    gate_pi_taps_2_ev_pend = true;
    gate_pi_taps_2_ev.notify(d);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED::gate_pi_taps_2_upd() {
    gate_pi_taps_2_ev_pend = false;
    const vl::logic<1> v_ = gate_pi_taps_2_pv;
    pi_chain[2].write(v_.bit(0).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED::proc_initial_155484() {
    double pi_delay_sh = pi_delay.read();
    pi_delay_sh = ((1.0 * 1.0) / (vl::to_real(vl::logic<32>(0x8ULL), true) * 4.0));
    if (!pi_delay_forced_) pi_delay.write(pi_delay_sh);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED::proc_always_155488() {
    vl::logic<1> ph_out_sh = vl::rd(ph_out);
    {
        const vl::logic<2> sel = vl::rd(word);
        if (vl::case_eq(sel, vl::logic<2>(0x0ULL))) {
            ph_out_sh = vl::rd(inA);
            vl::wr(ph_out, ph_out_sh);
        } else if (vl::case_eq(sel, vl::logic<2>(0x1ULL))) {
            ph_out_sh = vl::rd(pi_chain[0]);
            vl::wr(ph_out, ph_out_sh);
        } else if (vl::case_eq(sel, vl::logic<2>(0x2ULL))) {
            ph_out_sh = vl::rd(pi_chain[1]);
            vl::wr(ph_out, ph_out_sh);
        } else if (vl::case_eq(sel, vl::logic<2>(0x3ULL))) {
            ph_out_sh = vl::rd(pi_chain[2]);
            vl::wr(ph_out, ph_out_sh);
        }
    }
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED::proc_always_155496() {
    if (vl::rd(supply_ok).truth_val() == 0) {
        vl::wr(o, vl::logic<1>::all_z());
    } else if (vl::ceq(vl::rd(pd), vl::logic<1>(0x1ULL)).test()) {
        vl::wr(o, vl::logic<1>(0x1ULL));
    } else if (vl::ceq(vl::rd(pu), vl::logic<1>(0x0ULL)).test()) {
        vl::wr(o, vl::logic<1>(0x0ULL));
    } else {
        vl::wr(o, vl::rd(ph_out));
    }
}
