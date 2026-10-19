//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_mpll_nandinv_1
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_mpll_nandinv_1.h"

mipi_pll_dphy_mpll_nandinv_1::mipi_pll_dphy_mpll_nandinv_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    I183.reset(new mipi_pll_dphy_sc_invx12_lvt("I183"));
    I183->a(net2);
    I183->z(output);
    I183->gd(gd);
    I183->vp(vp);
    I184.reset(new mipi_pll_dphy_LEVELCONV_cdm_na3x6_lvt("I184"));
    I184->a(input);
    I184->tie_high_cdm(tie_high_cdm);
    I184->tie_low_cdm(tie_low_cdm);
    I184->z(net2);
    I184->gd(gd);
    I184->vp(vp);
}
