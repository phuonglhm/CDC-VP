//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_dig_addon_clkselen
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_DPHYPLL_dig_addon_clkselen.h"

mipi_pll_dphy_DPHYPLL_dig_addon_clkselen::mipi_pll_dphy_DPHYPLL_dig_addon_clkselen(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_clksel_int_avdd);
    sensitive << clksel_int_avdd;
    SC_METHOD(merge_clksel_int_output);
    sensitive << clksel_int_output_b[0] << clksel_int_output_b[1];
    I99_1.reset(new mipi_pll_dphy_sc_na2x6_lvt("I99_1"));
    I99_1->a(clksel_int_avdd_b[1]);
    I99_1->b(clksel_en_avdd);
    I99_1->z(net2[1]);
    I99_1->gd(gd);
    I99_1->vp(vpl);
    I99_0.reset(new mipi_pll_dphy_sc_na2x6_lvt("I99_0"));
    I99_0->a(clksel_int_avdd_b[0]);
    I99_0->b(clksel_en_avdd);
    I99_0->z(net2[0]);
    I99_0->gd(gd);
    I99_0->vp(vpl);
    I100_1.reset(new mipi_pll_dphy_sc_invx12_lvt("I100_1"));
    I100_1->a(net2[1]);
    I100_1->z(clksel_int_output_b[1]);
    I100_1->gd(gd);
    I100_1->vp(vpl);
    I100_0.reset(new mipi_pll_dphy_sc_invx12_lvt("I100_0"));
    I100_0->a(net2[0]);
    I100_0->z(clksel_int_output_b[0]);
    I100_0->gd(gd);
    I100_0->vp(vpl);
    I116.reset(new mipi_pll_dphy_sc_invx24_lvt("I116"));
    I116->a(onpll_avdd);
    I116->z(onpllz_avdd);
    I116->gd(gd);
    I116->vp(vpl);
}

void mipi_pll_dphy_DPHYPLL_dig_addon_clkselen::split_clksel_int_avdd() {
    const vl::logic<2> v = vl::rd(clksel_int_avdd);
    clksel_int_avdd_b[0].write(v.bit(0).sl());
    clksel_int_avdd_b[1].write(v.bit(1).sl());
}

void mipi_pll_dphy_DPHYPLL_dig_addon_clkselen::merge_clksel_int_output() {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(clksel_int_output_b[0]));
    v.set_bit(1, vl::rd(clksel_int_output_b[1]));
    vl::wr(clksel_int_output, v);
}

vl::logic<2> mipi_pll_dphy_DPHYPLL_dig_addon_clkselen::rd_clksel_int_avdd() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(clksel_int_avdd_b[0]));
    v.set_bit(1, vl::rd(clksel_int_avdd_b[1]));
    return v;
}

vl::logic<2> mipi_pll_dphy_DPHYPLL_dig_addon_clkselen::rd_clksel_int_output() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(clksel_int_output_b[0]));
    v.set_bit(1, vl::rd(clksel_int_output_b[1]));
    return v;
}

vl::logic<2> mipi_pll_dphy_DPHYPLL_dig_addon_clkselen::rd_net2() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(net2[0]));
    v.set_bit(1, vl::rd(net2[1]));
    return v;
}
