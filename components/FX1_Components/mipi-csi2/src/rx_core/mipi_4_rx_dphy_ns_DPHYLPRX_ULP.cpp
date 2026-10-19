//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYLPRX_ULP
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYLPRX_ULP.h"

mipi_4_rx_dphy_ns_DPHYLPRX_ULP::mipi_4_rx_dphy_ns_DPHYLPRX_ULP(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vph << gd << vdd << vss;
    SC_METHOD(assign_B);
    sensitive << A;
    SC_METHOD(gate_ULPRCV_I01);
    sensitive << ving << supply_ok << ponhv;
    SC_METHOD(gate_ULPRCV_I02);
    sensitive << supply_ok << ponhv;
    SC_METHOD(gate_I01ULPRCV);
    sensitive << out_pre << supply_ok;
    SC_METHOD(gate_I01ULPRCV_upd);
    sensitive << gate_I01ULPRCV_ev;
    dont_initialize();
    SC_METHOD(gate_I02ULPRCV);
    sensitive << ULPout_int << supply_ok;
    SC_METHOD(gate_I02ULPRCV_upd);
    sensitive << gate_I02ULPRCV_ev;
    dont_initialize();
    sctg_I31.reset(new mipi_4_rx_dphy_ns_sctg_nand2x("sctg_I31"));
    sctg_I31->a0(out_pre);
    sctg_I31->a1(AZ);
    sctg_I31->z(net186);
    sctg_I31->gd(gd);
    sctg_I31->vph(vph);
    sctg_I32.reset(new mipi_4_rx_dphy_ns_sctg_nand2x("sctg_I32"));
    sctg_I32->a0(net186);
    sctg_I32->a1(B);
    sctg_I32->z(out_pre);
    sctg_I32->gd(gd);
    sctg_I32->vph(vph);
    sctg_I30.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_I30"));
    sctg_I30->a(A);
    sctg_I30->z(AZ);
    sctg_I30->gd(gd);
    sctg_I30->vph(vph);
}

void mipi_4_rx_dphy_ns_DPHYLPRX_ULP::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::land(vl::land(vl::ceq(vl::rd(vph), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(vdd), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(vss), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHYLPRX_ULP::assign_B() {
    vl::wr(B, vl::rd(A));
}

void mipi_4_rx_dphy_ns_DPHYLPRX_ULP::gate_ULPRCV_I01() {
    const vl::logic<1> v = vl::bufif1_(~vl::rd(ving), vl::land(vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(ponhv), vl::logic<1>(0x1ULL))));
    vl::wr(A, v);
}

void mipi_4_rx_dphy_ns_DPHYLPRX_ULP::gate_ULPRCV_I02() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x1ULL), vl::land(vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(ponhv), vl::logic<1>(0x0ULL))));
    vl::wr(A, v);
}

void mipi_4_rx_dphy_ns_DPHYLPRX_ULP::gate_I01ULPRCV() {
    const vl::logic<1> nv = vl::bufif1_(vl::rd(out_pre), vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)));
    const sc_core::sc_time d = vl::pick_delay(nv.get_bit_val(0), vl::dly(13.0, TU_, TP_), vl::dly(13.0, TU_, TP_), std::min(vl::dly(13.0, TU_, TP_), vl::dly(13.0, TU_, TP_)));
    if (nv == gate_I01ULPRCV_pv && !gate_I01ULPRCV_ev_pend) return;
    gate_I01ULPRCV_pv = nv;
    gate_I01ULPRCV_ev.cancel();
    gate_I01ULPRCV_ev_pend = true;
    gate_I01ULPRCV_ev.notify(d);
}

void mipi_4_rx_dphy_ns_DPHYLPRX_ULP::gate_I01ULPRCV_upd() {
    gate_I01ULPRCV_ev_pend = false;
    vl::wr(ULPout_int, gate_I01ULPRCV_pv);
}

void mipi_4_rx_dphy_ns_DPHYLPRX_ULP::gate_I02ULPRCV() {
    const vl::logic<1> nv = vl::bufif1_(vl::rd(ULPout_int), vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)));
    const sc_core::sc_time d = vl::pick_delay(nv.get_bit_val(0), vl::dly(13.0, TU_, TP_), vl::dly(13.0, TU_, TP_), std::min(vl::dly(13.0, TU_, TP_), vl::dly(13.0, TU_, TP_)));
    if (nv == gate_I02ULPRCV_pv && !gate_I02ULPRCV_ev_pend) return;
    gate_I02ULPRCV_pv = nv;
    gate_I02ULPRCV_ev.cancel();
    gate_I02ULPRCV_ev_pend = true;
    gate_I02ULPRCV_ev.notify(d);
}

void mipi_4_rx_dphy_ns_DPHYLPRX_ULP::gate_I02ULPRCV_upd() {
    gate_I02ULPRCV_ev_pend = false;
    vl::wr(ULPout, gate_I02ULPRCV_pv);
}
