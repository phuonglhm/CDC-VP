//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL_inv_en
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_inv_en.h"

mipi_4_rx_dphy_ns_DPHYHSRX_DDL_inv_en::mipi_4_rx_dphy_ns_DPHYHSRX_DDL_inv_en(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(gate_DPHYHSRX_DDL_inv_en_buff);
    sensitive << i << vp << gd << on << onz;
    SC_METHOD(gate_DPHYHSRX_DDL_inv_en_buff_upd);
    sensitive << gate_DPHYHSRX_DDL_inv_en_buff_ev;
    dont_initialize();
    SC_METHOD(proc_initial_154981);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_inv_en::gate_DPHYHSRX_DDL_inv_en_buff() {
    const vl::logic<1> nv = vl::bufif1_(~vl::rd(i), vl::land(vl::land(vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(on), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(onz), vl::logic<1>(0x0ULL))));
    const sc_core::sc_time d = vl::dly(ddl_delay.read(), TU_, TP_);
    if (nv == gate_DPHYHSRX_DDL_inv_en_buff_pv && !gate_DPHYHSRX_DDL_inv_en_buff_ev_pend) return;
    gate_DPHYHSRX_DDL_inv_en_buff_pv = nv;
    gate_DPHYHSRX_DDL_inv_en_buff_ev.cancel();
    gate_DPHYHSRX_DDL_inv_en_buff_ev_pend = true;
    gate_DPHYHSRX_DDL_inv_en_buff_ev.notify(d);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_inv_en::gate_DPHYHSRX_DDL_inv_en_buff_upd() {
    gate_DPHYHSRX_DDL_inv_en_buff_ev_pend = false;
    vl::wr(o, gate_DPHYHSRX_DDL_inv_en_buff_pv);
}

// wire  net73;
// wire  net74;
// pfinlvt  P3( .B(vp), .D(o), .G(onz), .S(net73) );
// pfinlvt  P4( .B(vp), .D(net73), .G(i), .S(vp) );
// pfinlvt  P11( .B(vp), .D(net73), .G(on), .S(vp) );
// nfinlvt  N0( .B(gd), .D(o), .G(on), .S(net74) );
// nfinlvt  N1( .B(gd), .D(net74), .G(i), .S(gd) );
// nfinlvt  N10( .B(gd), .D(net74), .G(onz), .S(gd) );
void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_inv_en::proc_initial_154981() {
    double ddl_delay_sh = ddl_delay.read();
    ddl_delay_sh = ((1.0 * 1.0) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
    if (!ddl_delay_forced_) ddl_delay.write(ddl_delay_sh);
}
