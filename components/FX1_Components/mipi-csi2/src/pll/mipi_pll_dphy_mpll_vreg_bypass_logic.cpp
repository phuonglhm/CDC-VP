//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_mpll_vreg_bypass_logic
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_mpll_vreg_bypass_logic.h"

mipi_pll_dphy_mpll_vreg_bypass_logic::mipi_pll_dphy_mpll_vreg_bypass_logic(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    I0.reset(new mipi_pll_dphy_sc_na3x6_lvt("I0"));
    I0->a(mpll_pwron);
    I0->b(opmode);
    I0->c(net33);
    I0->z(net24);
    I0->gd(vss);
    I0->vp(vp);
    I1.reset(new mipi_pll_dphy_sc_na3x6_lvt("I1"));
    I1->a(mpll_pwron);
    I1->b(net29);
    I1->c(mpll_prg);
    I1->z(net23);
    I1->gd(vss);
    I1->vp(vp);
    I2.reset(new mipi_pll_dphy_sc_na3x6_lvt("I2"));
    I2->a(net25);
    I2->b(opmode);
    I2->c(mpll_prg);
    I2->z(net22);
    I2->gd(vss);
    I2->vp(vp);
    I3.reset(new mipi_pll_dphy_sc_na3x6_lvt("I3"));
    I3->a(net22);
    I3->b(net23);
    I3->c(net24);
    I3->z(bypass);
    I3->gd(vss);
    I3->vp(vp);
    I4.reset(new mipi_pll_dphy_sc_invx4_lvt("I4"));
    I4->a(mpll_pwron);
    I4->z(net25);
    I4->gd(vss);
    I4->vp(vp);
    I5.reset(new mipi_pll_dphy_sc_invx4_lvt("I5"));
    I5->a(opmode);
    I5->z(net29);
    I5->gd(vss);
    I5->vp(vp);
    I6.reset(new mipi_pll_dphy_sc_invx4_lvt("I6"));
    I6->a(mpll_prg);
    I6->z(net33);
    I6->gd(vss);
    I6->vp(vp);
}
