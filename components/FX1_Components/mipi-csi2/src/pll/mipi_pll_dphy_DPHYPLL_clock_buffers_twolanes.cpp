//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_clock_buffers_twolanes
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_DPHYPLL_clock_buffers_twolanes.h"

mipi_pll_dphy_DPHYPLL_clock_buffers_twolanes::mipi_pll_dphy_DPHYPLL_clock_buffers_twolanes(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    TREEVPCLK.reset(new mipi_pll_dphy_sc_ck_tri_nax36_lvt_pwrdlow("TREEVPCLK"));
    TREEVPCLK->clk1_m(clk_inm);
    TREEVPCLK->clk1_p(clk_inp);
    TREEVPCLK->pwr_on(on_pllclk);
    TREEVPCLK->tie_high(tie_high);
    TREEVPCLK->clk3_m(am);
    TREEVPCLK->clk3_p(ap);
    TREEVPCLK->gd(agnd);
    TREEVPCLK->vp(avddlv);
    I7.reset(new mipi_pll_dphy_sc_invx192_lvt("I7"));
    I7->a(bm);
    I7->z(clk_outm);
    I7->gd(agnd);
    I7->vp(avddlv);
    I91.reset(new mipi_pll_dphy_sc_invx192_lvt("I91"));
    I91->a(bp);
    I91->z(clk_outp);
    I91->gd(agnd);
    I91->vp(avddlv);
    I8.reset(new mipi_pll_dphy_sc_invx64_lvt("I8"));
    I8->a(am);
    I8->z(bm);
    I8->gd(agnd);
    I8->vp(avddlv);
    I93.reset(new mipi_pll_dphy_sc_invx64_lvt("I93"));
    I93->a(ap);
    I93->z(bp);
    I93->gd(agnd);
    I93->vp(avddlv);
}
