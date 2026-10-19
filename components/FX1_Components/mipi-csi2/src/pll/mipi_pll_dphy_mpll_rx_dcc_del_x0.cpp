//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_mpll_rx_dcc_del_x0
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_mpll_rx_dcc_del_x0.h"

mipi_pll_dphy_mpll_rx_dcc_del_x0::mipi_pll_dphy_mpll_rx_dcc_del_x0(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(gate_buf_gate);
    sensitive << vdcc_i << vp << gd << pwr_on << pwr_onz;
    I97.reset(new mipi_pll_dphy_sc_invx8_lvt("I97"));
    I97->a(clk_i_m);
    I97->z(net1110);
    I97->gd(gd);
    I97->vp(vp);
    I109.reset(new mipi_pll_dphy_sc_invx8_lvt("I109"));
    I109->a(net1119);
    I109->z(net1120);
    I109->gd(gd);
    I109->vp(vp);
    I112.reset(new mipi_pll_dphy_sc_invx8_lvt("I112"));
    I112->a(net1121);
    I112->z(net1122);
    I112->gd(gd);
    I112->vp(vp);
    invip.reset(new mipi_pll_dphy_sc_invx8_lvt("invip"));
    invip->a(clk_i_p);
    invip->z(net1109);
    invip->gd(gd);
    invip->vp(vp);
    mpath.reset(new mipi_pll_dphy_mpll_rx_dcc_del_x0_path("mpath"));
    mpath->clk_i(net1110);
    mpath->gd(gd);
    mpath->hs(hs);
    mpath->vdcc_iq(vdcc_i);
    mpath->vp(vp);
    mpath->vp_tie(vp_tie);
    mpath->clk_o(net1121);
    ppath.reset(new mipi_pll_dphy_mpll_rx_dcc_del_x0_path("ppath"));
    ppath->clk_i(net1109);
    ppath->gd(gd);
    ppath->hs(hs);
    ppath->vdcc_iq(vdcc_i);
    ppath->vp(vp);
    ppath->vp_tie(vp_tie);
    ppath->clk_o(net1119);
    I104.reset(new mipi_pll_dphy_sc_invx24_lvt("I104"));
    I104->a(clk_o_p);
    I104->z(ph_pmix_out_p);
    I104->gd(gd);
    I104->vp(vp);
    I105.reset(new mipi_pll_dphy_sc_invx24_lvt("I105"));
    I105->a(clk_o_m);
    I105->z(ph_pmix_out_m);
    I105->gd(gd);
    I105->vp(vp);
    I110.reset(new mipi_pll_dphy_sc_invx24_lvt("I110"));
    I110->a(net1120);
    I110->z(net1111);
    I110->gd(gd);
    I110->vp(vp);
    I111.reset(new mipi_pll_dphy_sc_invx24_lvt("I111"));
    I111->a(net1122);
    I111->z(net1112);
    I111->gd(gd);
    I111->vp(vp);
    I103.reset(new mipi_pll_dphy_mpll_l2h_res("I103"));
    I103->gd(gd);
    I103->ip(clk_o_p);
    I103->op(ckdcc_out_p);
    I106.reset(new mipi_pll_dphy_mpll_l2h_res("I106"));
    I106->gd(gd);
    I106->ip(clk_o_m);
    I106->op(ckdcc_out_m);
    I102.reset(new mipi_pll_dphy_sc_invx48_lvt("I102"));
    I102->a(net1111);
    I102->z(clk_o_p);
    I102->gd(gd);
    I102->vp(vp);
    I107.reset(new mipi_pll_dphy_sc_invx48_lvt("I107"));
    I107->a(net1112);
    I107->z(clk_o_m);
    I107->gd(gd);
    I107->vp(vp);
}

// pfinlvt  P43( .B(vp), .D(vdcc), .G(pwr_onz), .S(vdcc_i) );
// pfinlvt  P45( .B(vp), .D(vdcc_i), .G(pwr_on), .S(vp) );
// nfinlvt  N43( .B(gd), .D(vdcc_i), .G(pwr_on), .S(vdcc) );
// nfinlvt  dumN2( .B(gd), .D(gd), .G(gd), .S(gd) );
void mipi_pll_dphy_mpll_rx_dcc_del_x0::gate_buf_gate() {
    const vl::logic<1> v = vl::bufif1_(vl::rd(vdcc_i), vl::land(vl::land(vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(pwr_on), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(pwr_onz), vl::logic<1>(0x0ULL))));
    vl::wr(vdcc, v);
}
