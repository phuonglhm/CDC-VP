//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux.h"

mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux::mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(gate_clk_out_p_buf00);
    sensitive << clk_out_p_int << vp << gd << on << onz;
    SC_METHOD(gate_clk_out_p_buf01);
    sensitive << vp << gd << on << onz;
    SC_METHOD(gate_clk_out_n_buf00);
    sensitive << clk_out_n_int << vp << gd << on << onz;
    SC_METHOD(gate_clk_out_n_buf01);
    sensitive << vp << gd << on << onz;
    I6.reset(new mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_buf("I6"));
    I6->i(clk_long_p);
    I6->on(buf_on_long);
    I6->onz(buf_on_longz);
    I6->o(clk_out_p_int);
    I6->gd(gd);
    I6->vp(vp);
    I7.reset(new mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_buf("I7"));
    I7->i(clk_long_n);
    I7->on(buf_on_long);
    I7->onz(buf_on_longz);
    I7->o(clk_out_n_int);
    I7->gd(gd);
    I7->vp(vp);
    I8.reset(new mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_buf("I8"));
    I8->i(clk_short_p);
    I8->on(buf_on_short);
    I8->onz(buf_on_shortz);
    I8->o(clk_out_p_int);
    I8->gd(gd);
    I8->vp(vp);
    I9.reset(new mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_buf("I9"));
    I9->i(clk_short_n);
    I9->on(buf_on_short);
    I9->onz(buf_on_shortz);
    I9->o(clk_out_n_int);
    I9->gd(gd);
    I9->vp(vp);
    I10.reset(new mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_inv("I10"));
    I10->i(en_long);
    I10->o(en_long_z);
    I10->gd(gd);
    I10->vp(vp);
    I13.reset(new mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_inv("I13"));
    I13->i(on);
    I13->o(onz);
    I13->gd(gd);
    I13->vp(vp);
    I29.reset(new mipi_4_rx_dphy_ns_sc_na2x24_lvt("I29"));
    I29->a(on);
    I29->b(en_long);
    I29->z(buf_on_longz);
    I29->gd(gd);
    I29->vp(vp);
    I31.reset(new mipi_4_rx_dphy_ns_sc_na2x24_lvt("I31"));
    I31->a(on);
    I31->b(en_long_z);
    I31->z(buf_on_shortz);
    I31->gd(gd);
    I31->vp(vp);
    I30.reset(new mipi_4_rx_dphy_ns_sc_invx64_lvt("I30"));
    I30->a(buf_on_longz);
    I30->z(buf_on_long);
    I30->gd(gd);
    I30->vp(vp);
    I32.reset(new mipi_4_rx_dphy_ns_sc_invx32_lvt("I32"));
    I32->a(buf_on_shortz);
    I32->z(buf_on_short);
    I32->gd(gd);
    I32->vp(vp);
}

void mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux::gate_clk_out_p_buf00() {
    const vl::logic<1> v = vl::bufif1_(vl::rd(clk_out_p_int), vl::land(vl::land(vl::land(vl::eq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(on), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(onz), vl::logic<1>(0x0ULL))));
    vl::wr(clk_out_p, v);
}

void mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux::gate_clk_out_p_buf01() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x1ULL), vl::land(vl::land(vl::land(vl::eq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(on), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(onz), vl::logic<1>(0x1ULL))));
    vl::wr(clk_out_p, v);
}

void mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux::gate_clk_out_n_buf00() {
    const vl::logic<1> v = vl::bufif1_(vl::rd(clk_out_n_int), vl::land(vl::land(vl::land(vl::eq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(on), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(onz), vl::logic<1>(0x0ULL))));
    vl::wr(clk_out_n, v);
}

void mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux::gate_clk_out_n_buf01() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x0ULL), vl::land(vl::land(vl::land(vl::eq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(on), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(onz), vl::logic<1>(0x1ULL))));
    vl::wr(clk_out_n, v);
}
