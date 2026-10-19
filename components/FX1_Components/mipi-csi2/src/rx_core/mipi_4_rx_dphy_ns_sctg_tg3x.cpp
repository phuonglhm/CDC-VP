//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_sctg_tg3x
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_sctg_tg3x.h"

mipi_4_rx_dphy_ns_sctg_tg3x::mipi_4_rx_dphy_ns_sctg_tg3x(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(gate_buf_out);
    sensitive << i << vph << gd << en_n << en;
}

void mipi_4_rx_dphy_ns_sctg_tg3x::gate_buf_out() {
    const vl::logic<1> v = vl::nmos_(vl::rd(i), vl::land(vl::land(vl::land(vl::ceq(vl::rd(vph), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(en_n), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(en), vl::logic<1>(0x1ULL))));
    vl::wr(o, v);
}
