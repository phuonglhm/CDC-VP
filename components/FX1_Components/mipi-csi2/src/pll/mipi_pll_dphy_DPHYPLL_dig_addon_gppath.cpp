//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_dig_addon_gppath
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_DPHYPLL_dig_addon_gppath.h"

mipi_pll_dphy_DPHYPLL_dig_addon_gppath::mipi_pll_dphy_DPHYPLL_dig_addon_gppath(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    I97.reset(new mipi_pll_dphy_sc_invx12_lvt("I97"));
    I97->a(gp_clk_en);
    I97->z(gp_clk_enz);
    I97->gd(gd);
    I97->vp(vpl);
    I48.reset(new mipi_pll_dphy_sc_syncx10_as_lvt("I48"));
    I48->ck(clkvco_gp);
    I48->d(clkvco_gp_div);
    I48->set(gp_clk_enz);
    I48->qb(clkvco_gp_div);
    I48->gd(gd);
    I48->vp(vpl);
    CDM_OUT.reset(new mipi_pll_dphy_sc_no2x12_lvt("CDM_OUT"));
    CDM_OUT->a(tie_low_cdm);
    CDM_OUT->b(net11);
    CDM_OUT->z(clkout_gp);
    CDM_OUT->gd(gd);
    CDM_OUT->vp(vpl);
    INV_CDM.reset(new mipi_pll_dphy_sc_invx8_lvt("INV_CDM"));
    INV_CDM->a(clkvco_gp_div);
    INV_CDM->z(net11);
    INV_CDM->gd(gd);
    INV_CDM->vp(vpl);
}
