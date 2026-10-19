//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_div124
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_div124.h"

mipi_4_rx_dphy_ns_DPHYHSRX_div124::mipi_4_rx_dphy_ns_DPHYHSRX_div124(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    TIE_CDM.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2l_cdm_tie_LowHigh("TIE_CDM"));
    TIE_CDM->gd(vss);
    TIE_CDM->vp(vdd);
    TIE_CDM->tie_high(tie_h_cdm_vdd);
    TIE_CDM->tie_low(tie_l_cdm_vdd);
    sc_lvt_I153.reset(new mipi_4_rx_dphy_ns_sc_no2x6_lvt("sc_lvt_I153"));
    sc_lvt_I153->a(clk2dig_int);
    sc_lvt_I153->b(tie_l_cdm);
    sc_lvt_I153->z(clk2dig_in);
    sc_lvt_I153->gd(gd);
    sc_lvt_I153->vp(vp);
    sc_lvt_I156_0.reset(new mipi_4_rx_dphy_ns_sc_no2x6_lvt("sc_lvt_I156_0"));
    sc_lvt_I156_0->a(clk_p_cal_int);
    sc_lvt_I156_0->b(tie_l_cdm);
    sc_lvt_I156_0->z(net1187[1]);
    sc_lvt_I156_0->gd(gd);
    sc_lvt_I156_0->vp(vp);
    sc_lvt_I156_1.reset(new mipi_4_rx_dphy_ns_sc_no2x6_lvt("sc_lvt_I156_1"));
    sc_lvt_I156_1->a(clk_m_cal_int);
    sc_lvt_I156_1->b(tie_l_cdm);
    sc_lvt_I156_1->z(net1187[0]);
    sc_lvt_I156_1->gd(gd);
    sc_lvt_I156_1->vp(vp);
    sc_lvt_I_CDM_OR.reset(new mipi_4_rx_dphy_ns_sc_no2x6_lvt("sc_lvt_I_CDM_OR"));
    sc_lvt_I_CDM_OR->a(net1172);
    sc_lvt_I_CDM_OR->b(tie_l_cdm);
    sc_lvt_I_CDM_OR->z(clk_n_soc);
    sc_lvt_I_CDM_OR->gd(gd);
    sc_lvt_I_CDM_OR->vp(vp);
    sc_lvt_I119.reset(new mipi_4_rx_dphy_ns_sc_invx16_lvt("sc_lvt_I119"));
    sc_lvt_I119->a(pon);
    sc_lvt_I119->z(rst);
    sc_lvt_I119->gd(vss);
    sc_lvt_I119->vp(vdd);
    I83.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I83"));
    I83->a(clk2dig_in);
    I83->tie_high_cdm(tie_h_cdm_vdd);
    I83->tie_low_cdm(tie_l_cdm_vdd);
    I83->z(clk2dig);
    I83->gd(vss);
    I83->vp(vdd);
    I155_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I155_0"));
    I155_0->a(net1187[1]);
    I155_0->tie_high_cdm(tie_h_cdm_vdd);
    I155_0->tie_low_cdm(tie_l_cdm_vdd);
    I155_0->z(clk_p_cal);
    I155_0->gd(vss);
    I155_0->vp(vdd);
    I155_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I155_1"));
    I155_1->a(net1187[0]);
    I155_1->tie_high_cdm(tie_h_cdm_vdd);
    I155_1->tie_low_cdm(tie_l_cdm_vdd);
    I155_1->z(clk_m_cal);
    I155_1->gd(vss);
    I155_1->vp(vdd);
    sc_lvt_I151.reset(new mipi_4_rx_dphy_ns_sc_invx6_lvt("sc_lvt_I151"));
    sc_lvt_I151->a(clk);
    sc_lvt_I151->z(net1172);
    sc_lvt_I151->gd(gd);
    sc_lvt_I151->vp(vp);
    I150.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_div124_core("I150"));
    I150->clk(clk_n_soc);
    I150->clk_div(clk_div);
    I150->clk_en(clk_en);
    I150->rst(rst);
    I150->tie_high_cdm_vdd(tie_h_cdm_vdd);
    I150->tie_low_cdm_vdd(tie_l_cdm_vdd);
    I150->clk_out_div(clk_out_div);
    I150->vdd(vdd);
    I150->vss(vss);
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYHSRX_div124::rd_net1187() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(net1187[0]));
    v.set_bit(1, vl::rd(net1187[1]));
    return v;
}
