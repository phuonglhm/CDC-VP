//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_clk
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_clk.h"

mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_clk::mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_clk(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(gate_m_cal_buf01);
    sensitive << net1886 << vp << gd << cal_en;
    SC_METHOD(gate_p_cal_buf01);
    sensitive << net1894 << vp << gd << cal_en;
    SC_METHOD(gate_m_cal_buf02);
    sensitive << vp << gd << cal_en;
    SC_METHOD(gate_p_cal_buf02);
    sensitive << vp << gd << cal_en;
    // // Instances of devices/nfinlvt/symbol
    // nfinlvt  N391( .B(gd), .D(gd), .G(gd), .S(gd) );
    // nfinlvt  N434( .B(gd), .D(net1899), .G(cal_en), .S(gd) );
    // nfinlvt  N436( .B(gd), .D(m_cal), .G(net1886), .S(net1899) );
    // nfinlvt  N439( .B(gd), .D(net1904), .G(cal_en), .S(gd) );
    // nfinlvt  N440( .B(gd), .D(p_cal), .G(net1894), .S(net1904) );
    I433.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_core("I433"));
    I433->clk_in_m(clk_in_m);
    I433->clk_in_p(clk_in_p);
    I433->clk_m_cmos(clk_m_cmos);
    I433->clk_p_cmos(clk_p_cmos);
    I433->gd(gd);
    I433->ibias(ibias);
    I433->pon(pon);
    I433->ponz(ponz);
    I433->tie_high(tie_high);
    I433->tie_low(tie_low);
    I433->tie_low_cdm(tie_low_cdm);
    I433->to_buff_calm(net1894);
    I433->to_buff_calp(net1886);
    I433->vp(vp);
}

// pfinlvt  P391( .B(vp), .D(vp), .G(vp), .S(vp) );
// pfinlvt  P434( .B(vp), .D(m_cal), .G(cal_en), .S(vp) );
// pfinlvt  P435( .B(vp), .D(m_cal), .G(net1886), .S(vp) );
// pfinlvt  P439( .B(vp), .D(p_cal), .G(cal_en), .S(vp) );
// pfinlvt  P440( .B(vp), .D(p_cal), .G(net1894), .S(vp) );
void mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_clk::gate_m_cal_buf01() {
    const vl::logic<1> v = vl::bufif1_(~vl::rd(net1886), vl::land(vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(cal_en), vl::logic<1>(0x1ULL))));
    vl::wr(clk_m_cal, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_clk::gate_p_cal_buf01() {
    const vl::logic<1> v = vl::bufif1_(~vl::rd(net1894), vl::land(vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(cal_en), vl::logic<1>(0x1ULL))));
    vl::wr(clk_p_cal, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_clk::gate_m_cal_buf02() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x1ULL), vl::land(vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(cal_en), vl::logic<1>(0x0ULL))));
    vl::wr(clk_m_cal, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_clk::gate_p_cal_buf02() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x1ULL), vl::land(vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(cal_en), vl::logic<1>(0x0ULL))));
    vl::wr(clk_p_cal, v);
}
