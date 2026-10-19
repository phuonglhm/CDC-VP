//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_mpll_cdm_prot_clk
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_mpll_cdm_prot_clk.h"

mipi_pll_dphy_mpll_cdm_prot_clk::mipi_pll_dphy_mpll_cdm_prot_clk(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    TIE_CDM.reset(new mipi_pll_dphy_LEVELCONV_l2l_cdm_tie_LowHigh("TIE_CDM"));
    TIE_CDM->gd(gd);
    TIE_CDM->vp(vp);
    TIE_CDM->tie_high(tie_high_cdm);
    TIE_CDM->tie_low(tie_low_cdm);
    I194.reset(new mipi_pll_dphy_mpll_nandinv_1("I194"));
    I194->input(ckref);
    I194->tie_high_cdm(tie_high_cdm);
    I194->tie_low_cdm(tie_low_cdm);
    I194->output(ckref_prot);
    I194->gd(gd);
    I194->vp(vp);
    I195.reset(new mipi_pll_dphy_mpll_nandinv_1("I195"));
    I195->input(oscclk);
    I195->tie_high_cdm(tie_high_cdm);
    I195->tie_low_cdm(tie_low_cdm);
    I195->output(oscclk_prot);
    I195->gd(gd);
    I195->vp(vp);
}
