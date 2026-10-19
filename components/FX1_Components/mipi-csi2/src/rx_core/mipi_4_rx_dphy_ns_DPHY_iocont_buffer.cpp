//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHY_iocont_buffer
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHY_iocont_buffer.h"

mipi_4_rx_dphy_ns_DPHY_iocont_buffer::mipi_4_rx_dphy_ns_DPHY_iocont_buffer(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vdd << vp << gd << vss;
    SC_METHOD(gate_buf_enz);
    sensitive << enhv << supply_ok;
    SC_METHOD(gate_out_y_buf);
    sensitive << a_internal << supply_ok << tielow_hv << enz << enhv;
    SC_METHOD(gate_pull_up_inv);
    sensitive << supply_ok << tielow_hv << enz << enhv;
    SC_METHOD(proc_initial_159432);
}

void mipi_4_rx_dphy_ns_DPHY_iocont_buffer::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::land(vl::land(vl::ceq(vl::rd(vdd), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(vss), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHY_iocont_buffer::gate_buf_enz() {
    const vl::logic<1> v = vl::bufif1_(~vl::rd(enhv), vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)));
    vl::wr(enz, v);
}

void mipi_4_rx_dphy_ns_DPHY_iocont_buffer::gate_out_y_buf() {
    const vl::logic<1> v = vl::bufif1_(vl::rd(a_internal), vl::land(vl::land(vl::land(vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(tielow_hv), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(enz), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(enhv), vl::logic<1>(0x1ULL))));
    vl::wr(y, v);
}

void mipi_4_rx_dphy_ns_DPHY_iocont_buffer::gate_pull_up_inv() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x0ULL), vl::land(vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)), vl::lor(vl::ceq(vl::rd(tielow_hv), vl::logic<1>(0x1ULL)), vl::land(vl::ceq(vl::rd(enz), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(enhv), vl::logic<1>(0x0ULL))))));
    vl::wr(y, v);
}

void mipi_4_rx_dphy_ns_DPHY_iocont_buffer::proc_initial_159432() {
    vl::logic<1> a_internal_sh = vl::rd(a_internal);
    a_internal_sh = vl::logic<1>(0x0ULL);
    vl::wr(a_internal, a_internal_sh);
}
