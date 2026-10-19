//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_lb_dirver
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_lb_dirver.h"

mipi_4_rx_dphy_ns_DPHYHSRX_lb_dirver::mipi_4_rx_dphy_ns_DPHYHSRX_lb_dirver(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << ibias_lb << vp << gd << vph;
    SC_METHOD(gate_lb_outp_buf);
    sensitive << lb_marg_bufp << supply_ok << en_lb_hz << en_lb_h;
    SC_METHOD(gate_lb_outm_buf);
    sensitive << lb_marg_bufm << supply_ok << en_lb_hz << en_lb_h;
    SC_METHOD(gate_net114_buf);
    sensitive << net114 << lbcal << en_lb << supply_ok << pull_dwn;
    SC_METHOD(gate_net114_buf2);
    sensitive << lbcal << en_lb << supply_ok << pull_dwn;
    SC_METHOD(gate_lb_marg_m_buf0);
    sensitive << net114_int << lbcal << supply_ok;
    SC_METHOD(gate_lb_marg_m_buf1);
    sensitive << lbcal << supply_ok;
    SC_METHOD(gate_net120_buf);
    sensitive << net120 << lbcal << en_lb << supply_ok << pull_dwn;
    SC_METHOD(gate_net120_buf2);
    sensitive << lbcal << en_lb << supply_ok << pull_dwn;
    SC_METHOD(gate_lb_marg_p_buf0);
    sensitive << net120_int << lbcal << supply_ok;
    SC_METHOD(gate_lb_marg_p_buf1);
    sensitive << lbcal << supply_ok;
    sctg_invlb.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_invlb"));
    sctg_invlb->a(en_lb_h);
    sctg_invlb->z(en_lb_hz);
    sctg_invlb->gd(gd);
    sctg_invlb->vph(vph);
    I27.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux_buf("I27"));
    I27->i(lb_marg_m);
    I27->on(long_lb);
    I27->onz(long_lbz);
    I27->o(net114);
    I27->gd(gd);
    I27->vp(vp);
    I28.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux_buf("I28"));
    I28->i(lb_clkmarg_m);
    I28->on(short_lb);
    I28->onz(short_lbz);
    I28->o(net114);
    I28->gd(gd);
    I28->vp(vp);
    I29.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux_buf("I29"));
    I29->i(lb_clkmarg_p);
    I29->on(short_lb);
    I29->onz(short_lbz);
    I29->o(net120);
    I29->gd(gd);
    I29->vp(vp);
    I30.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux_buf("I30"));
    I30->i(lb_marg_p);
    I30->on(long_lb);
    I30->onz(long_lbz);
    I30->o(net120);
    I30->gd(gd);
    I30->vp(vp);
    I33.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("I33"));
    I33->a(en_lb);
    I33->z(en_lbz);
    I33->gd(gd);
    I33->vp(vp);
    I34.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("I34"));
    I34->a(en_clklb);
    I34->z(short_lbz);
    I34->gd(gd);
    I34->vp(vp);
    I41.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("I41"));
    I41->a(long_lb);
    I41->z(long_lbz);
    I41->gd(gd);
    I41->vp(vp);
    I42.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("I42"));
    I42->a(short_lbz);
    I42->z(short_lb);
    I42->gd(gd);
    I42->vp(vp);
    I35.reset(new mipi_4_rx_dphy_ns_sc_no2x6_lvt("I35"));
    I35->a(en_clklb);
    I35->b(en_lbz);
    I35->z(long_lb);
    I35->gd(gd);
    I35->vp(vp);
    I40.reset(new mipi_4_rx_dphy_ns_sc_no2x6_lvt("I40"));
    I40->a(long_lb);
    I40->b(short_lb);
    I40->z(pull_dwn);
    I40->gd(gd);
    I40->vp(vp);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_lb_dirver::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::land(vl::land(vl::ceq(vl::rd(ibias_lb), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(vph), vl::logic<1>(0x1ULL))));
}

void mipi_4_rx_dphy_ns_DPHYHSRX_lb_dirver::gate_lb_outp_buf() {
    const vl::logic<1> v = vl::bufif1_(~vl::rd(lb_marg_bufp), vl::land(vl::land(vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(en_lb_hz), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(en_lb_h), vl::logic<1>(0x1ULL))));
    vl::wr(lb_outp, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_lb_dirver::gate_lb_outm_buf() {
    const vl::logic<1> v = vl::bufif1_(~vl::rd(lb_marg_bufm), vl::land(vl::land(vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(en_lb_hz), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(en_lb_h), vl::logic<1>(0x1ULL))));
    vl::wr(lb_outm, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_lb_dirver::gate_net114_buf() {
    const vl::logic<1> v = vl::bufif1_(vl::rd(net114), vl::land(vl::land(vl::land(vl::ceq(vl::rd(lbcal), vl::logic<1>(0x0ULL)), vl::ceq(vl::rd(en_lb), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(pull_dwn), vl::logic<1>(0x0ULL))));
    vl::wr(net114_int, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_lb_dirver::gate_net114_buf2() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x0ULL), vl::land(vl::land(vl::land(vl::ceq(vl::rd(lbcal), vl::logic<1>(0x0ULL)), vl::ceq(vl::rd(en_lb), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(pull_dwn), vl::logic<1>(0x1ULL))));
    vl::wr(net114_int, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_lb_dirver::gate_lb_marg_m_buf0() {
    const vl::logic<1> v = vl::bufif1_(vl::rd(net114_int), vl::land(vl::ceq(vl::rd(lbcal), vl::logic<1>(0x0ULL)), vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL))));
    vl::wr(lb_marg_bufm, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_lb_dirver::gate_lb_marg_m_buf1() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x0ULL), vl::land(vl::ceq(vl::rd(lbcal), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL))));
    vl::wr(lb_marg_bufm, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_lb_dirver::gate_net120_buf() {
    const vl::logic<1> v = vl::bufif1_(vl::rd(net120), vl::land(vl::land(vl::land(vl::ceq(vl::rd(lbcal), vl::logic<1>(0x0ULL)), vl::ceq(vl::rd(en_lb), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(pull_dwn), vl::logic<1>(0x0ULL))));
    vl::wr(net120_int, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_lb_dirver::gate_net120_buf2() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x0ULL), vl::land(vl::land(vl::land(vl::ceq(vl::rd(lbcal), vl::logic<1>(0x0ULL)), vl::ceq(vl::rd(en_lb), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(pull_dwn), vl::logic<1>(0x1ULL))));
    vl::wr(net120_int, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_lb_dirver::gate_lb_marg_p_buf0() {
    const vl::logic<1> v = vl::bufif1_(vl::rd(net120_int), vl::land(vl::ceq(vl::rd(lbcal), vl::logic<1>(0x0ULL)), vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL))));
    vl::wr(lb_marg_bufp, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_lb_dirver::gate_lb_marg_p_buf1() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x0ULL), vl::land(vl::ceq(vl::rd(lbcal), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL))));
    vl::wr(lb_marg_bufp, v);
}
