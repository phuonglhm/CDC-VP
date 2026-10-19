//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_gen_mux
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_gen_mux.h"

mipi_4_rx_dphy_ns_gen_mux::mipi_4_rx_dphy_ns_gen_mux(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_z);
    sensitive << sel << d1 << d0;
    SC_METHOD(assign_z_upd);
    sensitive << assign_z_ev;
    dont_initialize();
}

void mipi_4_rx_dphy_ns_gen_mux::assign_z() {
    const vl::logic<1> nv = vl::mux(vl::rd(sel), vl::rd(d1), vl::rd(d0));
    const sc_core::sc_time d = vl::dly(0.0, TU_, TP_);
    if (nv == assign_z_pv && !assign_z_ev_pend) return;
    assign_z_pv = nv;
    assign_z_ev.cancel();
    assign_z_ev_pend = true;
    assign_z_ev.notify(d);
}

void mipi_4_rx_dphy_ns_gen_mux::assign_z_upd() {
    assign_z_ev_pend = false;
    vl::wr(z, assign_z_pv);
}
