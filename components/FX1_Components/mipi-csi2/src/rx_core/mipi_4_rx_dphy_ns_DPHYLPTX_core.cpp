//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYLPTX_core
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYLPTX_core.h"

mipi_4_rx_dphy_ns_DPHYLPTX_core::mipi_4_rx_dphy_ns_DPHYLPTX_core(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_setr);
    sensitive << setr;
    SC_METHOD(split_setrz);
    sensitive << setrz;
    SC_METHOD(assign_supply_ok);
    sensitive << vph << gd << enable;
    SC_METHOD(gate_PD);
    sensitive << enpulldwn;
    SC_METHOD(gate_PD_upd);
    sensitive << gate_PD_ev;
    dont_initialize();
    SC_METHOD(gate_LPDRV);
    sensitive << enpullupz << din << supply_ok << onb << onbz << enable << enablez << biasn << biasp;
    SC_METHOD(gate_LPDRV_upd);
    sensitive << gate_LPDRV_ev;
    dont_initialize();
    SC_METHOD(gate_I01POWER_AWARE);
    sensitive << vph << gd;
    SC_METHOD(gate_I01POWER_AWARE_upd);
    sensitive << gate_I01POWER_AWARE_ev;
    dont_initialize();
    I150.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_switch("I150"));
    I150->a(gpmos);
    I150->y(cp0);
    I150->gd(gd);
    I150->vph(vph);
    I150->pon(din);
    I150->ponz(dinz);
    I151.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_switch("I151"));
    I151->a(xxx1);
    I151->y(cp0);
    I151->gd(gd);
    I151->vph(vph);
    I151->pon(dinz);
    I151->ponz(din);
    I245.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_switch("I245"));
    I245->a(yyy1);
    I245->y(cn0);
    I245->gd(gd);
    I245->vph(vph);
    I245->pon(din);
    I245->ponz(dinz);
    I246.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_switch("I246"));
    I246->a(gnmos);
    I246->y(cn0);
    I246->gd(gd);
    I246->vph(vph);
    I246->pon(dinz);
    I246->ponz(din);
    I342.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_switch("I342"));
    I342->a(gd);
    I342->y(gd);
    I342->gd(gd);
    I342->vph(vph);
    I342->pon(sr_enhz);
    I342->ponz(sr_enh);
    I343.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_switch("I343"));
    I343->a(cn0);
    I343->y(net1916);
    I343->gd(gd);
    I343->vph(vph);
    I343->pon(sr_enhz);
    I343->ponz(sr_enh);
    I345.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_switch("I345"));
    I345->a(gd);
    I345->y(net1915);
    I345->gd(gd);
    I345->vph(vph);
    I345->pon(sr_enh);
    I345->ponz(sr_enhz);
    I346.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_switch("I346"));
    I346->a(cp0);
    I346->y(net1915);
    I346->gd(gd);
    I346->vph(vph);
    I346->pon(sr_enhz);
    I346->ponz(sr_enh);
    LOG.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_log("LOG"));
    LOG->din(din);
    LOG->dinz(dinz);
    LOG->enulp(enulp);
    LOG->onb(onb);
    LOG->enn(enn);
    LOG->enp(enp);
    LOG->enpullupz(enpullupz);
    LOG->gd(gd);
    LOG->vph(vph);
    sctg_I356.reset(new mipi_4_rx_dphy_ns_sctg_inv9x("sctg_I356"));
    sctg_I356->a(sr_bypasshv);
    sctg_I356->z(sr_enhz);
    sctg_I356->gd(gd);
    sctg_I356->vph(vph);
    sctg_I357.reset(new mipi_4_rx_dphy_ns_sctg_inv9x("sctg_I357"));
    sctg_I357->a(sr_enhz);
    sctg_I357->z(sr_enh);
    sctg_I357->gd(gd);
    sctg_I357->vph(vph);
    I259_29.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_29"));
    I259_29->A(enablez);
    I259_29->B(gpmos);
    I259_29->C(gnmos);
    I259_29->D(enable);
    I259_29->Z(dout_int[1]);
    I259_29->gd(gd);
    I259_29->vph(vph);
    I259_28.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_28"));
    I259_28->A(enablez);
    I259_28->B(gpmos);
    I259_28->C(gnmos);
    I259_28->D(enable);
    I259_28->Z(dout_int[0]);
    I259_28->gd(gd);
    I259_28->vph(vph);
    I259_27.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_27"));
    I259_27->A(enablez);
    I259_27->B(gpmos);
    I259_27->C(gnmos);
    I259_27->D(enable);
    I259_27->Z(dout_int[1]);
    I259_27->gd(gd);
    I259_27->vph(vph);
    I259_26.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_26"));
    I259_26->A(enablez);
    I259_26->B(gpmos);
    I259_26->C(gnmos);
    I259_26->D(enable);
    I259_26->Z(dout_int[0]);
    I259_26->gd(gd);
    I259_26->vph(vph);
    I259_25.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_25"));
    I259_25->A(enablez);
    I259_25->B(gpmos);
    I259_25->C(gnmos);
    I259_25->D(enable);
    I259_25->Z(dout_int[1]);
    I259_25->gd(gd);
    I259_25->vph(vph);
    I259_24.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_24"));
    I259_24->A(enablez);
    I259_24->B(gpmos);
    I259_24->C(gnmos);
    I259_24->D(enable);
    I259_24->Z(dout_int[0]);
    I259_24->gd(gd);
    I259_24->vph(vph);
    I259_23.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_23"));
    I259_23->A(enablez);
    I259_23->B(gpmos);
    I259_23->C(gnmos);
    I259_23->D(enable);
    I259_23->Z(dout_int[1]);
    I259_23->gd(gd);
    I259_23->vph(vph);
    I259_22.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_22"));
    I259_22->A(enablez);
    I259_22->B(gpmos);
    I259_22->C(gnmos);
    I259_22->D(enable);
    I259_22->Z(dout_int[0]);
    I259_22->gd(gd);
    I259_22->vph(vph);
    I259_21.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_21"));
    I259_21->A(enablez);
    I259_21->B(gpmos);
    I259_21->C(gnmos);
    I259_21->D(enable);
    I259_21->Z(dout_int[1]);
    I259_21->gd(gd);
    I259_21->vph(vph);
    I259_20.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_20"));
    I259_20->A(enablez);
    I259_20->B(gpmos);
    I259_20->C(gnmos);
    I259_20->D(enable);
    I259_20->Z(dout_int[0]);
    I259_20->gd(gd);
    I259_20->vph(vph);
    I259_19.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_19"));
    I259_19->A(enablez);
    I259_19->B(gpmos);
    I259_19->C(gnmos);
    I259_19->D(enable);
    I259_19->Z(dout_int[1]);
    I259_19->gd(gd);
    I259_19->vph(vph);
    I259_18.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_18"));
    I259_18->A(enablez);
    I259_18->B(gpmos);
    I259_18->C(gnmos);
    I259_18->D(enable);
    I259_18->Z(dout_int[0]);
    I259_18->gd(gd);
    I259_18->vph(vph);
    I259_17.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_17"));
    I259_17->A(enablez);
    I259_17->B(gpmos);
    I259_17->C(gnmos);
    I259_17->D(enable);
    I259_17->Z(dout_int[1]);
    I259_17->gd(gd);
    I259_17->vph(vph);
    I259_16.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_16"));
    I259_16->A(enablez);
    I259_16->B(gpmos);
    I259_16->C(gnmos);
    I259_16->D(enable);
    I259_16->Z(dout_int[0]);
    I259_16->gd(gd);
    I259_16->vph(vph);
    I259_15.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_15"));
    I259_15->A(enablez);
    I259_15->B(gpmos);
    I259_15->C(gnmos);
    I259_15->D(enable);
    I259_15->Z(dout_int[1]);
    I259_15->gd(gd);
    I259_15->vph(vph);
    I259_14.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_14"));
    I259_14->A(enablez);
    I259_14->B(gpmos);
    I259_14->C(gnmos);
    I259_14->D(enable);
    I259_14->Z(dout_int[0]);
    I259_14->gd(gd);
    I259_14->vph(vph);
    I259_13.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_13"));
    I259_13->A(setrz_b[2]);
    I259_13->B(gpmos);
    I259_13->C(gnmos);
    I259_13->D(setr_b[2]);
    I259_13->Z(dout_intr[2]);
    I259_13->gd(gd);
    I259_13->vph(vph);
    I259_12.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_12"));
    I259_12->A(setrz_b[2]);
    I259_12->B(gpmos);
    I259_12->C(gnmos);
    I259_12->D(setr_b[2]);
    I259_12->Z(dout_intr[2]);
    I259_12->gd(gd);
    I259_12->vph(vph);
    I259_11.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_11"));
    I259_11->A(setrz_b[2]);
    I259_11->B(gpmos);
    I259_11->C(gnmos);
    I259_11->D(setr_b[2]);
    I259_11->Z(dout_intr[2]);
    I259_11->gd(gd);
    I259_11->vph(vph);
    I259_10.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_10"));
    I259_10->A(setrz_b[2]);
    I259_10->B(gpmos);
    I259_10->C(gnmos);
    I259_10->D(setr_b[2]);
    I259_10->Z(dout_intr[2]);
    I259_10->gd(gd);
    I259_10->vph(vph);
    I259_9.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_9"));
    I259_9->A(setrz_b[2]);
    I259_9->B(gpmos);
    I259_9->C(gnmos);
    I259_9->D(setr_b[2]);
    I259_9->Z(dout_intr[2]);
    I259_9->gd(gd);
    I259_9->vph(vph);
    I259_8.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_8"));
    I259_8->A(setrz_b[2]);
    I259_8->B(gpmos);
    I259_8->C(gnmos);
    I259_8->D(setr_b[2]);
    I259_8->Z(dout_intr[2]);
    I259_8->gd(gd);
    I259_8->vph(vph);
    I259_7.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_7"));
    I259_7->A(setrz_b[2]);
    I259_7->B(gpmos);
    I259_7->C(gnmos);
    I259_7->D(setr_b[2]);
    I259_7->Z(dout_intr[2]);
    I259_7->gd(gd);
    I259_7->vph(vph);
    I259_6.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_6"));
    I259_6->A(setrz_b[2]);
    I259_6->B(gpmos);
    I259_6->C(gnmos);
    I259_6->D(setr_b[2]);
    I259_6->Z(dout_intr[2]);
    I259_6->gd(gd);
    I259_6->vph(vph);
    I259_5.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_5"));
    I259_5->A(setrz_b[1]);
    I259_5->B(gpmos);
    I259_5->C(gnmos);
    I259_5->D(setr_b[1]);
    I259_5->Z(dout_intr[1]);
    I259_5->gd(gd);
    I259_5->vph(vph);
    I259_4.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_4"));
    I259_4->A(setrz_b[1]);
    I259_4->B(gpmos);
    I259_4->C(gnmos);
    I259_4->D(setr_b[1]);
    I259_4->Z(dout_intr[1]);
    I259_4->gd(gd);
    I259_4->vph(vph);
    I259_3.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_3"));
    I259_3->A(setrz_b[1]);
    I259_3->B(gpmos);
    I259_3->C(gnmos);
    I259_3->D(setr_b[1]);
    I259_3->Z(dout_intr[1]);
    I259_3->gd(gd);
    I259_3->vph(vph);
    I259_2.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_2"));
    I259_2->A(setrz_b[1]);
    I259_2->B(gpmos);
    I259_2->C(gnmos);
    I259_2->D(setr_b[1]);
    I259_2->Z(dout_intr[1]);
    I259_2->gd(gd);
    I259_2->vph(vph);
    I259_1.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_1"));
    I259_1->A(setrz_b[0]);
    I259_1->B(gpmos);
    I259_1->C(gnmos);
    I259_1->D(setr_b[0]);
    I259_1->Z(dout_intr[0]);
    I259_1->gd(gd);
    I259_1->vph(vph);
    I259_0.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_keys("I259_0"));
    I259_0->A(setrz_b[0]);
    I259_0->B(gpmos);
    I259_0->C(gnmos);
    I259_0->D(setr_b[0]);
    I259_0->Z(dout_intr[0]);
    I259_0->gd(gd);
    I259_0->vph(vph);
    SC_METHOD(proc_always_160211);
    sensitive << supply_ok << enablez << enpullupz << biasp << biasn_fb << dout;
}

void mipi_4_rx_dphy_ns_DPHYLPTX_core::split_setr() {
    const vl::logic<3> v = vl::rd(setr);
    setr_b[0].write(v.bit(0).sl());
    setr_b[1].write(v.bit(1).sl());
    setr_b[2].write(v.bit(2).sl());
}

void mipi_4_rx_dphy_ns_DPHYLPTX_core::split_setrz() {
    const vl::logic<3> v = vl::rd(setrz);
    setrz_b[0].write(v.bit(0).sl());
    setrz_b[1].write(v.bit(1).sl());
    setrz_b[2].write(v.bit(2).sl());
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYLPTX_core::rd_dout_int() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(dout_int[0]));
    v.set_bit(1, vl::rd(dout_int[1]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYLPTX_core::rd_dout_intr() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(dout_intr[0]));
    v.set_bit(1, vl::rd(dout_intr[1]));
    v.set_bit(2, vl::rd(dout_intr[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYLPTX_core::rd_setr() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(setr_b[0]));
    v.set_bit(1, vl::rd(setr_b[1]));
    v.set_bit(2, vl::rd(setr_b[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYLPTX_core::rd_setrz() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(setrz_b[0]));
    v.set_bit(1, vl::rd(setrz_b[1]));
    v.set_bit(2, vl::rd(setrz_b[2]));
    return v;
}

void mipi_4_rx_dphy_ns_DPHYLPTX_core::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::land(vl::ceq(vl::rd(vph), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(enable), vl::logic<1>(0x1ULL))));
}

void mipi_4_rx_dphy_ns_DPHYLPTX_core::gate_PD() {
    const vl::logic<1> nv = vl::bufif1_(vl::logic<1>(0x0ULL), vl::ceq(vl::rd(enpulldwn), vl::logic<1>(0x1ULL)));
    const sc_core::sc_time d = vl::pick_delay(nv.get_bit_val(0), vl::dly(0.5, TU_, TP_), vl::dly(0.5, TU_, TP_), std::min(vl::dly(0.5, TU_, TP_), vl::dly(0.5, TU_, TP_)));
    if (nv == gate_PD_pv && !gate_PD_ev_pend) return;
    gate_PD_pv = nv;
    gate_PD_ev.cancel();
    gate_PD_ev_pend = true;
    gate_PD_ev.notify(d);
}

void mipi_4_rx_dphy_ns_DPHYLPTX_core::gate_PD_upd() {
    gate_PD_ev_pend = false;
    vl::wr_s(dout, gate_PD_pv, vl::ST_PULL);
}

void mipi_4_rx_dphy_ns_DPHYLPTX_core::gate_LPDRV() {
    const vl::logic<1> nv = vl::bufif1_(vl::mux(vl::ceq(vl::rd(enpullupz), vl::logic<1>(0x1ULL)), vl::rd(din), vl::logic<1>(0x1ULL)), vl::land(vl::land(vl::land(vl::land(vl::land(vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)), vl::lor(vl::land(vl::ceq(vl::rd(onb), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(onbz), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(enpullupz), vl::logic<1>(0x0ULL)))), vl::ceq(vl::rd(enable), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(enablez), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(biasn), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(biasp), vl::logic<1>(0x0ULL))));
    const sc_core::sc_time d = vl::pick_delay(nv.get_bit_val(0), vl::dly(20.0, TU_, TP_), vl::dly(25.0, TU_, TP_), std::min(vl::dly(20.0, TU_, TP_), vl::dly(25.0, TU_, TP_)));
    if (nv == gate_LPDRV_pv && !gate_LPDRV_ev_pend) return;
    gate_LPDRV_pv = nv;
    gate_LPDRV_ev.cancel();
    gate_LPDRV_ev_pend = true;
    gate_LPDRV_ev.notify(d);
}

void mipi_4_rx_dphy_ns_DPHYLPTX_core::gate_LPDRV_upd() {
    gate_LPDRV_ev_pend = false;
    vl::wr_s(dout, gate_LPDRV_pv, vl::ST_PULL);
}

void mipi_4_rx_dphy_ns_DPHYLPTX_core::gate_I01POWER_AWARE() {
    const vl::logic<1> nv = vl::bufif1_(vl::logic<1>(), vl::lor(vl::cne(vl::rd(vph), vl::logic<1>(0x1ULL)), vl::cne(vl::rd(gd), vl::logic<1>(0x0ULL))));
    const sc_core::sc_time d = vl::pick_delay(nv.get_bit_val(0), vl::dly(20.0, TU_, TP_), vl::dly(25.0, TU_, TP_), std::min(vl::dly(20.0, TU_, TP_), vl::dly(25.0, TU_, TP_)));
    if (nv == gate_I01POWER_AWARE_pv && !gate_I01POWER_AWARE_ev_pend) return;
    gate_I01POWER_AWARE_pv = nv;
    gate_I01POWER_AWARE_ev.cancel();
    gate_I01POWER_AWARE_ev_pend = true;
    gate_I01POWER_AWARE_ev.notify(d);
}

void mipi_4_rx_dphy_ns_DPHYLPTX_core::gate_I01POWER_AWARE_upd() {
    gate_I01POWER_AWARE_ev_pend = false;
    vl::wr_s(dout, gate_I01POWER_AWARE_pv, vl::ST_PULL);
}

// if(supply_ok)
// if(onbz===1'b0 && biasp===1'b0 && cascp===1'b0)
// yyy <= 1'b1;
// else
// yyy <= 1'bx;
// else
// yyy <= 1'bz;
// end
// if(supply_ok)
// if(onb===1'b1 && biasn===1'b1 && cascn===1'b1)
// xxx <= 1'b0;
// else
// xxx <= 1'bx;
// else
// xxx <= 1'bz;
// end
// if(supply_ok)
// if(biasn===1'b1 && enn===1'b1)
// gpmos <= 1'b1;
// else
// gpmos <= 1'bx;
// else
// gpmos <= 1'bz;
// end
// if(supply_ok)
// if(enp)
// gnmos <= 1'b0;
// else begin
// if(biasp===1'b1)
// gnmos <= 1'bx;
// else
// gnmos <= 1'b0;
// end
// else
// gnmos <= 1'bz;
// end
void mipi_4_rx_dphy_ns_DPHYLPTX_core::proc_always_160211() {
    if (vl::rd(supply_ok).test()) {
        if (vl::ceq(vl::rd(enablez), vl::logic<1>(0x1ULL)).test()) {
            vl::wr(loopout, vl::logic<1>(0x0ULL));
        } else if (vl::land(vl::ceq(vl::rd(enpullupz), vl::logic<1>(0x0ULL)), vl::ceq(vl::rd(biasp), vl::logic<1>(0x0ULL))).test()) {
            vl::wr(loopout, vl::logic<1>(0x1ULL));
        } else if (vl::ceq(vl::rd(biasn_fb), vl::logic<1>(0x0ULL)).test()) {
            vl::wr(loopout, vl::rd(dout));
        } else {
            vl::wr(loopout, vl::logic<1>());
        }
    } else {
        vl::wr(loopout, vl::logic<1>::all_z());
    }
}
