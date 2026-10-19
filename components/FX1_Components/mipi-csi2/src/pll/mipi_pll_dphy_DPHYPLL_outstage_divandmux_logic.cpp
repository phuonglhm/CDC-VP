//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_outstage_divandmux_logic
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_DPHYPLL_outstage_divandmux_logic.h"

mipi_pll_dphy_DPHYPLL_outstage_divandmux_logic::mipi_pll_dphy_DPHYPLL_outstage_divandmux_logic(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_clksel);
    sensitive << clksel;
    SC_METHOD(split_p_int);
    sensitive << p_int;
    SC_METHOD(merge_clksel_int);
    sensitive << clksel_int_b[0] << clksel_int_b[1];
    I40.reset(new mipi_pll_dphy_sc_na2x12_lvt("I40"));
    I40->a(net6);
    I40->b(negnow);
    I40->z(vco_direct);
    I40->gd(agnd);
    I40->vp(avddlv);
    I52.reset(new mipi_pll_dphy_sc_na2x12_lvt("I52"));
    I52->a(p_int_b[0]);
    I52->b(p_int_b[1]);
    I52->z(net13);
    I52->gd(agnd);
    I52->vp(avddlv);
    I53.reset(new mipi_pll_dphy_sc_na2x12_lvt("I53"));
    I53->a(p_int_b[0]);
    I53->b(net14);
    I53->z(net12);
    I53->gd(agnd);
    I53->vp(avddlv);
    I54.reset(new mipi_pll_dphy_sc_na2x12_lvt("I54"));
    I54->a(net11);
    I54->b(p_int_b[1]);
    I54->z(net8);
    I54->gd(agnd);
    I54->vp(avddlv);
    I55.reset(new mipi_pll_dphy_sc_na2x12_lvt("I55"));
    I55->a(net12);
    I55->b(net13);
    I55->z(net10);
    I55->gd(agnd);
    I55->vp(avddlv);
    I57.reset(new mipi_pll_dphy_sc_na2x12_lvt("I57"));
    I57->a(net8);
    I57->b(net9);
    I57->z(negnow);
    I57->gd(agnd);
    I57->vp(avddlv);
    I98.reset(new mipi_pll_dphy_sc_na2x12_lvt("I98"));
    I98->a(clksel_b[1]);
    I98->b(clksel_not);
    I98->z(net5);
    I98->gd(agnd);
    I98->vp(avddlv);
    I33.reset(new mipi_pll_dphy_sc_no2x12_lvt("I33"));
    I33->a(net3);
    I33->b(net4);
    I33->z(onlvz_buf);
    I33->gd(agnd);
    I33->vp(avddlv);
    I39.reset(new mipi_pll_dphy_sc_no2x12_lvt("I39"));
    I39->a(clksel_b[1]);
    I39->b(net7);
    I39->z(net6);
    I39->gd(agnd);
    I39->vp(avddlv);
    I38.reset(new mipi_pll_dphy_sc_invx16_lvt("I38"));
    I38->a(clksel_b[0]);
    I38->z(net7);
    I38->gd(agnd);
    I38->vp(avddlv);
    I50.reset(new mipi_pll_dphy_sc_invx16_lvt("I50"));
    I50->a(p_int_b[0]);
    I50->z(net11);
    I50->gd(agnd);
    I50->vp(avddlv);
    I51.reset(new mipi_pll_dphy_sc_invx16_lvt("I51"));
    I51->a(p_int_b[1]);
    I51->z(net14);
    I51->gd(agnd);
    I51->vp(avddlv);
    I56.reset(new mipi_pll_dphy_sc_invx16_lvt("I56"));
    I56->a(net10);
    I56->z(net9);
    I56->gd(agnd);
    I56->vp(avddlv);
    I58.reset(new mipi_pll_dphy_sc_invx16_lvt("I58"));
    I58->a(clksel_b[1]);
    I58->z(net2);
    I58->gd(agnd);
    I58->vp(avddlv);
    I59.reset(new mipi_pll_dphy_sc_invx16_lvt("I59"));
    I59->a(clksel_b[0]);
    I59->z(net1);
    I59->gd(agnd);
    I59->vp(avddlv);
    I94.reset(new mipi_pll_dphy_sc_invx16_lvt("I94"));
    I94->a(clksel_b[0]);
    I94->z(clksel_not);
    I94->gd(agnd);
    I94->vp(avddlv);
    I100.reset(new mipi_pll_dphy_sc_invx16_lvt("I100"));
    I100->a(onlvz);
    I100->z(net4);
    I100->gd(agnd);
    I100->vp(avddlv);
    I101.reset(new mipi_pll_dphy_sc_invx16_lvt("I101"));
    I101->a(net5);
    I101->z(net3);
    I101->gd(agnd);
    I101->vp(avddlv);
    I43.reset(new mipi_pll_dphy_sc_mux2x8_lvt("I43"));
    I43->a0(net2);
    I43->a1(clksel_b[1]);
    I43->s(vco_direct);
    I43->z(clksel_int_b[1]);
    I43->gd(agnd);
    I43->vp(avddlv);
    I44.reset(new mipi_pll_dphy_sc_mux2x8_lvt("I44"));
    I44->a0(net1);
    I44->a1(clksel_b[0]);
    I44->s(vco_direct);
    I44->z(clksel_int_b[0]);
    I44->gd(agnd);
    I44->vp(avddlv);
}

void mipi_pll_dphy_DPHYPLL_outstage_divandmux_logic::split_clksel() {
    const vl::logic<2> v = vl::rd(clksel);
    clksel_b[0].write(v.bit(0).sl());
    clksel_b[1].write(v.bit(1).sl());
}

void mipi_pll_dphy_DPHYPLL_outstage_divandmux_logic::split_p_int() {
    const vl::logic<2> v = vl::rd(p_int);
    p_int_b[0].write(v.bit(0).sl());
    p_int_b[1].write(v.bit(1).sl());
}

void mipi_pll_dphy_DPHYPLL_outstage_divandmux_logic::merge_clksel_int() {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(clksel_int_b[0]));
    v.set_bit(1, vl::rd(clksel_int_b[1]));
    vl::wr(clksel_int, v);
}

vl::logic<2> mipi_pll_dphy_DPHYPLL_outstage_divandmux_logic::rd_clksel() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(clksel_b[0]));
    v.set_bit(1, vl::rd(clksel_b[1]));
    return v;
}

vl::logic<2> mipi_pll_dphy_DPHYPLL_outstage_divandmux_logic::rd_clksel_int() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(clksel_int_b[0]));
    v.set_bit(1, vl::rd(clksel_int_b[1]));
    return v;
}

vl::logic<2> mipi_pll_dphy_DPHYPLL_outstage_divandmux_logic::rd_p_int() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(p_int_b[0]));
    v.set_bit(1, vl::rd(p_int_b[1]));
    return v;
}
