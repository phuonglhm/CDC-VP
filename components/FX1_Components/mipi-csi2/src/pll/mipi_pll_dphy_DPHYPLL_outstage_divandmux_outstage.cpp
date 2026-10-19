//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_outstage_divandmux_outstage
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_DPHYPLL_outstage_divandmux_outstage.h"

mipi_pll_dphy_DPHYPLL_outstage_divandmux_outstage::mipi_pll_dphy_DPHYPLL_outstage_divandmux_outstage(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    I92.reset(new mipi_pll_dphy_schc_invx128_lvt("I92"));
    I92->a(b);
    I92->z(output);
    I92->gd(dgnd);
    I92->vp(dvdd);
    I93.reset(new mipi_pll_dphy_schc_invx32_lvt("I93"));
    I93->a(input);
    I93->z(b2b);
    I93->gd(dgnd);
    I93->vp(dvdd);
    I91.reset(new mipi_pll_dphy_schc_invx64_lvt("I91"));
    I91->a(b2b);
    I91->z(b);
    I91->gd(dgnd);
    I91->vp(dvdd);
}
