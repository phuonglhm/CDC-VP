//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_div1248
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_div1248.h"

mipi_4_rx_dphy_ns_DPHYHSRX_div1248::mipi_4_rx_dphy_ns_DPHYHSRX_div1248(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_clk_div);
    sensitive << clk_div;
    SC_METHOD(glue_I18_clk_div);
    sensitive << tie_low << div_conf;
    I18.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_div124_core("I18"));
    I18->clk(div_beta);
    I18->clk_div(c_I18_clk_div);
    I18->clk_en(clk_en);
    I18->rst(rst);
    I18->tie_high_cdm_vdd(tie_h_cdm_vdd);
    I18->tie_low_cdm_vdd(tie_l_cdm_vdd);
    I18->clk_out_div(clk_out_div);
    I18->vdd(vdd);
    I18->vss(vss);
    SC_METHOD(glue_div_clk_div);
    sensitive << clk_div_b[0] << clk_div_b[1];
    div.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_div124_core("div"));
    div->clk(clk_n_soc);
    div->clk_div(c_div_clk_div);
    div->clk_en(clk_en);
    div->rst(rst);
    div->tie_high_cdm_vdd(tie_h_cdm_vdd);
    div->tie_low_cdm_vdd(tie_l_cdm_vdd);
    div->clk_out_div(div_beta);
    div->vdd(vdd);
    div->vss(vss);
    I20.reset(new mipi_4_rx_dphy_ns_sc_invx16_lvt("I20"));
    I20->a(div_confz);
    I20->z(div_conf);
    I20->gd(vss);
    I20->vp(vdd);
    I119.reset(new mipi_4_rx_dphy_ns_sc_invx16_lvt("I119"));
    I119->a(pon);
    I119->z(rst);
    I119->gd(vss);
    I119->vp(vdd);
    I120.reset(new mipi_4_rx_dphy_ns_sc_no2x6_lvt("I120"));
    I120->a(net1035);
    I120->b(tie_l_cdm);
    I120->z(clk_n_soc);
    I120->gd(gd);
    I120->vp(vp);
    I117.reset(new mipi_4_rx_dphy_ns_sc_tie_hilow_lvt("I117"));
    I117->gd(vss);
    I117->tie_high(tie_high);
    I117->tie_low(tie_low);
    I117->vp(vdd);
    I22.reset(new mipi_4_rx_dphy_ns_sc_na2x6_lvt("I22"));
    I22->a(clk_div_b[0]);
    I22->b(clk_div_b[1]);
    I22->z(div_confz);
    I22->gd(vss);
    I22->vp(vdd);
    I151.reset(new mipi_4_rx_dphy_ns_sc_invx6_lvt("I151"));
    I151->a(clk);
    I151->z(net1035);
    I151->gd(gd);
    I151->vp(vp);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_div1248::split_clk_div() {
    const vl::logic<2> v = vl::rd(clk_div);
    clk_div_b[0].write(v.bit(0).sl());
    clk_div_b[1].write(v.bit(1).sl());
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYHSRX_div1248::rd_clk_div() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(clk_div_b[0]));
    v.set_bit(1, vl::rd(clk_div_b[1]));
    return v;
}

void mipi_4_rx_dphy_ns_DPHYHSRX_div1248::glue_I18_clk_div() { vl::wr(c_I18_clk_div, vl::cat(vl::rd(tie_low), vl::rd(div_conf).bit(0))); }

void mipi_4_rx_dphy_ns_DPHYHSRX_div1248::glue_div_clk_div() { vl::wr(c_div_clk_div, rd_clk_div()); }
