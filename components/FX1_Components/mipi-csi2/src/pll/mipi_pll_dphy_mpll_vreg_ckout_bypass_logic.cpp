//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_mpll_vreg_ckout_bypass_logic
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_mpll_vreg_ckout_bypass_logic.h"

mipi_pll_dphy_mpll_vreg_ckout_bypass_logic::mipi_pll_dphy_mpll_vreg_ckout_bypass_logic(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_clk_sel);
    sensitive << clk_sel;
    I5.reset(new mipi_pll_dphy_sc_invx4_lvt("I5"));
    I5->a(mpll_prg);
    I5->z(net38);
    I5->gd(vss);
    I5->vp(vp);
    I2.reset(new mipi_pll_dphy_sc_no2x6_lvt("I2"));
    I2->a(clk_sel_b[0]);
    I2->b(mpll_pwron);
    I2->z(net36);
    I2->gd(vss);
    I2->vp(vp);
    I4.reset(new mipi_pll_dphy_sc_na3x6_lvt("I4"));
    I4->a(clk_sel_b[1]);
    I4->b(net36);
    I4->c(opmode);
    I4->z(net32);
    I4->gd(vss);
    I4->vp(vp);
    I3.reset(new mipi_pll_dphy_sc_na2x6_lvt("I3"));
    I3->a(net38);
    I3->b(net32);
    I3->z(bypass);
    I3->gd(vss);
    I3->vp(vp);
}

void mipi_pll_dphy_mpll_vreg_ckout_bypass_logic::split_clk_sel() {
    const vl::logic<2> v = vl::rd(clk_sel);
    clk_sel_b[0].write(v.bit(0).sl());
    clk_sel_b[1].write(v.bit(1).sl());
}

vl::logic<2> mipi_pll_dphy_mpll_vreg_ckout_bypass_logic::rd_clk_sel() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(clk_sel_b[0]));
    v.set_bit(1, vl::rd(clk_sel_b[1]));
    return v;
}
