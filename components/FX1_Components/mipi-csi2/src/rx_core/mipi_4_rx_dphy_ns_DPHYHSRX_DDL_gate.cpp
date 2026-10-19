//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL_gate
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_gate.h"

mipi_4_rx_dphy_ns_DPHYHSRX_DDL_gate::mipi_4_rx_dphy_ns_DPHYHSRX_DDL_gate(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vp << gd;
    SC_METHOD(gate_o_buff00);
    sensitive << supply_ok << pu;
    SC_METHOD(gate_o_buff01);
    sensitive << i << supply_ok << pu << on << onz;
    SC_METHOD(gate_o_buff01_upd);
    sensitive << gate_o_buff01_ev;
    dont_initialize();
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_gate::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_gate::gate_o_buff00() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x1ULL), vl::land(vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(pu), vl::logic<1>(0x0ULL))));
    vl::wr(o, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_gate::gate_o_buff01() {
    const vl::logic<1> nv = vl::bufif1_(~vl::rd(i), vl::land(vl::land(vl::land(vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(pu), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(on), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(onz), vl::logic<1>(0x0ULL))));
    const sc_core::sc_time d = vl::dly(data_delay, TU_, TP_);
    if (nv == gate_o_buff01_pv && !gate_o_buff01_ev_pend) return;
    gate_o_buff01_pv = nv;
    gate_o_buff01_ev.cancel();
    gate_o_buff01_ev_pend = true;
    gate_o_buff01_ev.notify(d);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_gate::gate_o_buff01_upd() {
    gate_o_buff01_ev_pend = false;
    vl::wr(o, gate_o_buff01_pv);
}
