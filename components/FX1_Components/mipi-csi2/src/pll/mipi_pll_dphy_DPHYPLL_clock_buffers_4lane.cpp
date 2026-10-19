//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_clock_buffers_4lane
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_DPHYPLL_clock_buffers_4lane.h"

mipi_pll_dphy_DPHYPLL_clock_buffers_4lane::mipi_pll_dphy_DPHYPLL_clock_buffers_4lane(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    I139.reset(new mipi_pll_dphy_DPHYPLL_clock_buffers_twolanes("I139"));
    I139->agnd(agnd);
    I139->avddlv(avddlv);
    I139->tie_high(tie_high);
    I139->on_pllclk(enable);
    I139->clk_inp(clk_in0);
    I139->clk_inm(clk_in180);
    I139->clk_outp(clk_out0);
    I139->clk_outm(clk_out180);
    I144.reset(new mipi_pll_dphy_DPHYPLL_clock_buffers_twolanes("I144"));
    I144->agnd(agnd);
    I144->avddlv(avddlv);
    I144->tie_high(tie_high);
    I144->on_pllclk(enable);
    I144->clk_inp(clk_in90);
    I144->clk_inm(clk_in270);
    I144->clk_outp(clk_out90);
    I144->clk_outm(clk_out270);
    I74.reset(new mipi_pll_dphy_sc_invx64_lvt("I74"));
    I74->a(bs);
    I74->z(clkstop_out);
    I74->gd(agnd);
    I74->vp(avddlv);
    I73.reset(new mipi_pll_dphy_sc_invx32_lvt("I73"));
    I73->a(as);
    I73->z(bs);
    I73->gd(agnd);
    I73->vp(avddlv);
    I8.reset(new mipi_pll_dphy_sc_invx6_lvt("I8"));
    I8->a(clkstop_in);
    I8->z(net56);
    I8->gd(agnd);
    I8->vp(avddlv);
    I13.reset(new mipi_pll_dphy_sc_na2x6_lvt("I13"));
    I13->a(net56);
    I13->b(enable);
    I13->z(as);
    I13->gd(agnd);
    I13->vp(avddlv);
}
