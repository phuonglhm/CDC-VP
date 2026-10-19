//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV.h"

mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV::mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vp << gd;
    SC_METHOD(assign_enable);
    sensitive << on << onz << bn << bp;
    SC_METHOD(gate_ddl_delay_output);
    sensitive << o_int << supply_ok << enable;
    SC_METHOD(gate_ddl_delay_output_upd);
    sensitive << gate_ddl_delay_output_ev;
    dont_initialize();
    SC_METHOD(proc_always_155031);
    sensitive << supply_ok << pu << pd << enable << i;
    SC_METHOD(proc_initial_155045);
}

// wire  net197;
// wire  net198;
// wire  net195;
// wire  net196;
// nfinlvt  N1( .B(gd), .D(o), .G(i), .S(net197) );
// nfinlvt  N26( .B(gd), .D(net197), .G(on), .S(net198) );
// nfinlvt  N27( .B(gd), .D(net198), .G(bn), .S(gd) );
// nfinlvt  N33( .B(gd), .D(gd), .G(bn), .S(gd) );
// nfinlvt  P25( .B(gd), .D(o), .G(pd), .S(gd) );
// pfinlvt  P3( .B(vp), .D(o), .G(i), .S(net196) );
// pfinlvt  P26( .B(vp), .D(net196), .G(onz), .S(net195) );
// pfinlvt  P28( .B(vp), .D(net195), .G(bp), .S(vp) );
// pfinlvt  P31( .B(vp), .D(o), .G(pu), .S(vp) );
// pfinlvt  P32( .B(vp), .D(vp), .G(bp), .S(vp) );
void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV::assign_enable() {
    vl::wr(enable, vl::land(vl::land(vl::land(vl::ceq(vl::rd(on), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(onz), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(bn), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(bp), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV::gate_ddl_delay_output() {
    const vl::logic<1> nv = vl::bufif1_(vl::rd(o_int), vl::land(vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(enable), vl::logic<1>(0x1ULL))));
    const sc_core::sc_time d = vl::dly(ddl_delay.read(), TU_, TP_);
    if (nv == gate_ddl_delay_output_pv && !gate_ddl_delay_output_ev_pend) return;
    gate_ddl_delay_output_pv = nv;
    gate_ddl_delay_output_ev.cancel();
    gate_ddl_delay_output_ev_pend = true;
    gate_ddl_delay_output_ev.notify(d);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV::gate_ddl_delay_output_upd() {
    gate_ddl_delay_output_ev_pend = false;
    vl::wr(o, gate_ddl_delay_output_pv);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV::proc_always_155031() {
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)).test()) {
        if (vl::ceq(vl::rd(pu), vl::logic<1>(0x0ULL)).test()) {
            vl::wr(o_int, vl::logic<1>(0x1ULL));
        } else if (vl::ceq(vl::rd(pd), vl::logic<1>(0x1ULL)).test()) {
            vl::wr(o_int, vl::logic<1>(0x0ULL));
        } else if (vl::ceq(vl::rd(enable), vl::logic<1>(0x1ULL)).test()) {
            vl::wr(o_int, ~vl::rd(i));
        } else {
            vl::wr(o_int, vl::logic<1>());
        }
    } else {
        vl::wr(o_int, vl::logic<1>::all_z());
    }
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV::proc_initial_155045() {
    double ddl_delay_sh = ddl_delay.read();
    ddl_delay_sh = ((1.0 * 1.0) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
    if (!ddl_delay_forced_) ddl_delay.write(ddl_delay_sh);
}
