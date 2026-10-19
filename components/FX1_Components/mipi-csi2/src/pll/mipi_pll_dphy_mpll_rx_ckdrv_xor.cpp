//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_mpll_rx_ckdrv_xor
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_mpll_rx_ckdrv_xor.h"

mipi_pll_dphy_mpll_rx_ckdrv_xor::mipi_pll_dphy_mpll_rx_ckdrv_xor(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    xor_r.reset(new mipi_pll_dphy_mpll_rx_ckdrv_dccres("xor_r"));
    xor_r->gd(gd);
    xor_r->inr(xr);
    xor_r->outr(xor_out);
    I47.reset(new mipi_pll_dphy_wire_cap("I47"));
    I47->w(xorp);
    I48.reset(new mipi_pll_dphy_wire_cap("I48"));
    I48->w(xorm);
    I49.reset(new mipi_pll_dphy_wire_cap("I49"));
    I49->w(xr);
    nam_2.reset(new mipi_pll_dphy_mpll_rx_ckdrv_na2x31("nam_2"));
    nam_2->a(clk_i_m);
    nam_2->b(clk_q_m);
    nam_2->gd(gd);
    nam_2->vp(vp);
    nam_2->z(xorm);
    nam_1.reset(new mipi_pll_dphy_mpll_rx_ckdrv_na2x31("nam_1"));
    nam_1->a(clk_i_m);
    nam_1->b(clk_q_m);
    nam_1->gd(gd);
    nam_1->vp(vp);
    nam_1->z(xorm);
    nam_0.reset(new mipi_pll_dphy_mpll_rx_ckdrv_na2x31("nam_0"));
    nam_0->a(clk_i_m);
    nam_0->b(clk_q_m);
    nam_0->gd(gd);
    nam_0->vp(vp);
    nam_0->z(xorm);
    nap_2.reset(new mipi_pll_dphy_mpll_rx_ckdrv_na2x31("nap_2"));
    nap_2->a(clk_i_p);
    nap_2->b(clk_q_p);
    nap_2->gd(gd);
    nap_2->vp(vp);
    nap_2->z(xorp);
    nap_1.reset(new mipi_pll_dphy_mpll_rx_ckdrv_na2x31("nap_1"));
    nap_1->a(clk_i_p);
    nap_1->b(clk_q_p);
    nap_1->gd(gd);
    nap_1->vp(vp);
    nap_1->z(xorp);
    nap_0.reset(new mipi_pll_dphy_mpll_rx_ckdrv_na2x31("nap_0"));
    nap_0->a(clk_i_p);
    nap_0->b(clk_q_p);
    nap_0->gd(gd);
    nap_0->vp(vp);
    nap_0->z(xorp);
    napm.reset(new mipi_pll_dphy_mpll_rx_ckdrv_na2x31_thresh("napm"));
    napm->a(xorp);
    napm->b(xorm);
    napm->gd(gd);
    napm->vp(vp);
    napm->z(xr);
}
