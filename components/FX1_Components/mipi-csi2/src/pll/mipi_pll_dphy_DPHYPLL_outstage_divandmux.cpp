//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_outstage_divandmux
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_DPHYPLL_outstage_divandmux.h"

mipi_pll_dphy_DPHYPLL_outstage_divandmux::mipi_pll_dphy_DPHYPLL_outstage_divandmux(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_clksel);
    sensitive << clksel;
    premux.reset(new mipi_pll_dphy_sc_mux2x8_lvt("premux"));
    premux->a0(clkvco_lf);
    premux->a1(clkext);
    premux->s(clksel1_del);
    premux->z(net261);
    premux->gd(agnd);
    premux->vp(avddlv);
    // nfinlvt  N123( .B(agnd), .D(agnd), .G(agnd), .S(agnd) );
    // pfinlvt  P124( .B(avddlv), .D(avddlv), .G(avddlv), .S(avddlv) );
    quaddiv.reset(new mipi_pll_dphy_DPHYPLL_quaddiv_2_4_8("quaddiv"));
    quaddiv->ck(net261);
    quaddiv->dgnd(agnd);
    quaddiv->dvdd(avddlv);
    quaddiv->p(p_int);
    quaddiv->pon_n(onlvz_buf);
    quaddiv->q0(clkext0);
    quaddiv->q1(clkext1);
    quaddiv->q2(clkext2);
    quaddiv->q3(clkext3);
    I120.reset(new mipi_pll_dphy_sc_dlax23_ar_lvt("I120"));
    I120->ck_m(mux_off);
    I120->ck_p(mux_off_z);
    I120->d(net355);
    I120->pwr_on(onlv_buf);
    I120->qb(clksel1_del);
    I120->gd(agnd);
    I120->vp(avddlv);
    SC_METHOD(glue_logic_clksel);
    sensitive << clksel_b[0] << clksel_b[1];
    logic.reset(new mipi_pll_dphy_DPHYPLL_outstage_divandmux_logic("logic"));
    logic->clksel(c_logic_clksel);
    logic->onlvz(onlvz);
    logic->p_int(p_int);
    logic->clksel_int(clksel_int);
    logic->onlvz_buf(onlvz_buf);
    logic->agnd(agnd);
    logic->avddlv(avddlv);
    muxglitch.reset(new mipi_pll_dphy_DPHYPLL_outstage_4out("muxglitch"));
    muxglitch->clkdcc0(clkdcc0);
    muxglitch->clkdcc1(clkdcc90);
    muxglitch->clkdcc2(clkdcc180);
    muxglitch->clkdcc3(clkdcc270);
    muxglitch->clkext0(clkext0);
    muxglitch->clkext1(clkext1);
    muxglitch->clkext2(clkext2);
    muxglitch->clkext3(clkext3);
    muxglitch->clksel(clksel_int);
    muxglitch->onlvz(onlvz_buf);
    muxglitch->clk_stop(clkout_stop);
    muxglitch->clkout0(clkout0);
    muxglitch->clkout1(clkout90);
    muxglitch->clkout2(clkout180);
    muxglitch->clkout3(clkout270);
    muxglitch->mux_off(mux_off);
    muxglitch->dgnd(agnd);
    muxglitch->dvdd(avddlv);
    I87.reset(new mipi_pll_dphy_sc_invx32_lvt("I87"));
    I87->a(onlvz_buf);
    I87->z(onlv_buf);
    I87->gd(agnd);
    I87->vp(avddlv);
    I119.reset(new mipi_pll_dphy_sc_invx32_lvt("I119"));
    I119->a(mux_off);
    I119->z(mux_off_z);
    I119->gd(agnd);
    I119->vp(avddlv);
    I122.reset(new mipi_pll_dphy_sc_invx32_lvt("I122"));
    I122->a(clksel_b[1]);
    I122->z(net355);
    I122->gd(agnd);
    I122->vp(avddlv);
}

void mipi_pll_dphy_DPHYPLL_outstage_divandmux::split_clksel() {
    const vl::logic<2> v = vl::rd(clksel);
    clksel_b[0].write(v.bit(0).sl());
    clksel_b[1].write(v.bit(1).sl());
}

vl::logic<2> mipi_pll_dphy_DPHYPLL_outstage_divandmux::rd_clksel() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(clksel_b[0]));
    v.set_bit(1, vl::rd(clksel_b[1]));
    return v;
}

void mipi_pll_dphy_DPHYPLL_outstage_divandmux::glue_logic_clksel() { vl::wr(c_logic_clksel, rd_clksel()); }
