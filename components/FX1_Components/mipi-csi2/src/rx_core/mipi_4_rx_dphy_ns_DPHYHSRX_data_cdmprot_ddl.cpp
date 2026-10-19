//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot_ddl
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot_ddl.h"

mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot_ddl::mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot_ddl(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_ddl_phase_clk);
    sensitive << ddl_phase_clk;
    SC_METHOD(split_ddl_tune);
    sensitive << ddl_tune;
    SC_METHOD(merge_ddl_phase_clklv);
    sensitive << ddl_phase_clklv_b[0] << ddl_phase_clklv_b[1] << ddl_phase_clklv_b[2] << ddl_phase_clklv_b[3] << ddl_phase_clklv_b[4];
    SC_METHOD(merge_ddl_tunelv);
    sensitive << ddl_tunelv_b[0] << ddl_tunelv_b[1] << ddl_tunelv_b[2] << ddl_tunelv_b[3] << ddl_tunelv_b[4];
    TIE_CDM.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2l_cdm_tie_LowHigh("TIE_CDM"));
    TIE_CDM->gd(gd);
    TIE_CDM->vp(vp);
    TIE_CDM->tie_high(tie_high_cdm);
    TIE_CDM->tie_low(tie_low_cdm);
    I18_10.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("I18_10"));
    I18_10->a(net159[10]);
    I18_10->z(ddl_tunelv_b[4]);
    I18_10->gd(gd);
    I18_10->vp(vp);
    I18_9.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("I18_9"));
    I18_9->a(net159[9]);
    I18_9->z(ddl_tunelv_b[3]);
    I18_9->gd(gd);
    I18_9->vp(vp);
    I18_8.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("I18_8"));
    I18_8->a(net159[8]);
    I18_8->z(ddl_tunelv_b[2]);
    I18_8->gd(gd);
    I18_8->vp(vp);
    I18_7.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("I18_7"));
    I18_7->a(net159[7]);
    I18_7->z(ddl_tunelv_b[1]);
    I18_7->gd(gd);
    I18_7->vp(vp);
    I18_6.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("I18_6"));
    I18_6->a(net159[6]);
    I18_6->z(ddl_tunelv_b[0]);
    I18_6->gd(gd);
    I18_6->vp(vp);
    I18_5.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("I18_5"));
    I18_5->a(net159[5]);
    I18_5->z(ddl_phase_clklv_b[4]);
    I18_5->gd(gd);
    I18_5->vp(vp);
    I18_4.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("I18_4"));
    I18_4->a(net159[4]);
    I18_4->z(ddl_phase_clklv_b[3]);
    I18_4->gd(gd);
    I18_4->vp(vp);
    I18_3.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("I18_3"));
    I18_3->a(net159[3]);
    I18_3->z(ddl_phase_clklv_b[2]);
    I18_3->gd(gd);
    I18_3->vp(vp);
    I18_2.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("I18_2"));
    I18_2->a(net159[2]);
    I18_2->z(ddl_phase_clklv_b[1]);
    I18_2->gd(gd);
    I18_2->vp(vp);
    I18_1.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("I18_1"));
    I18_1->a(net159[1]);
    I18_1->z(ddl_phase_clklv_b[0]);
    I18_1->gd(gd);
    I18_1->vp(vp);
    I18_0.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("I18_0"));
    I18_0->a(net159[0]);
    I18_0->z(ddl_tune_enlv);
    I18_0->gd(gd);
    I18_0->vp(vp);
    I92_10.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I92_10"));
    I92_10->a(ddl_tune_b[4]);
    I92_10->tie_high_cdm(tie_high_cdm);
    I92_10->tie_low_cdm(tie_low_cdm);
    I92_10->z(net159[10]);
    I92_10->gd(gd);
    I92_10->vp(vp);
    I92_9.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I92_9"));
    I92_9->a(ddl_tune_b[3]);
    I92_9->tie_high_cdm(tie_high_cdm);
    I92_9->tie_low_cdm(tie_low_cdm);
    I92_9->z(net159[9]);
    I92_9->gd(gd);
    I92_9->vp(vp);
    I92_8.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I92_8"));
    I92_8->a(ddl_tune_b[2]);
    I92_8->tie_high_cdm(tie_high_cdm);
    I92_8->tie_low_cdm(tie_low_cdm);
    I92_8->z(net159[8]);
    I92_8->gd(gd);
    I92_8->vp(vp);
    I92_7.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I92_7"));
    I92_7->a(ddl_tune_b[1]);
    I92_7->tie_high_cdm(tie_high_cdm);
    I92_7->tie_low_cdm(tie_low_cdm);
    I92_7->z(net159[7]);
    I92_7->gd(gd);
    I92_7->vp(vp);
    I92_6.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I92_6"));
    I92_6->a(ddl_tune_b[0]);
    I92_6->tie_high_cdm(tie_high_cdm);
    I92_6->tie_low_cdm(tie_low_cdm);
    I92_6->z(net159[6]);
    I92_6->gd(gd);
    I92_6->vp(vp);
    I92_5.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I92_5"));
    I92_5->a(ddl_phase_clk_b[4]);
    I92_5->tie_high_cdm(tie_high_cdm);
    I92_5->tie_low_cdm(tie_low_cdm);
    I92_5->z(net159[5]);
    I92_5->gd(gd);
    I92_5->vp(vp);
    I92_4.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I92_4"));
    I92_4->a(ddl_phase_clk_b[3]);
    I92_4->tie_high_cdm(tie_high_cdm);
    I92_4->tie_low_cdm(tie_low_cdm);
    I92_4->z(net159[4]);
    I92_4->gd(gd);
    I92_4->vp(vp);
    I92_3.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I92_3"));
    I92_3->a(ddl_phase_clk_b[2]);
    I92_3->tie_high_cdm(tie_high_cdm);
    I92_3->tie_low_cdm(tie_low_cdm);
    I92_3->z(net159[3]);
    I92_3->gd(gd);
    I92_3->vp(vp);
    I92_2.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I92_2"));
    I92_2->a(ddl_phase_clk_b[1]);
    I92_2->tie_high_cdm(tie_high_cdm);
    I92_2->tie_low_cdm(tie_low_cdm);
    I92_2->z(net159[2]);
    I92_2->gd(gd);
    I92_2->vp(vp);
    I92_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I92_1"));
    I92_1->a(ddl_phase_clk_b[0]);
    I92_1->tie_high_cdm(tie_high_cdm);
    I92_1->tie_low_cdm(tie_low_cdm);
    I92_1->z(net159[1]);
    I92_1->gd(gd);
    I92_1->vp(vp);
    I92_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I92_0"));
    I92_0->a(ddl_tune_en);
    I92_0->tie_high_cdm(tie_high_cdm);
    I92_0->tie_low_cdm(tie_low_cdm);
    I92_0->z(net159[0]);
    I92_0->gd(gd);
    I92_0->vp(vp);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot_ddl::split_ddl_phase_clk() {
    const vl::logic<5> v = vl::rd(ddl_phase_clk);
    ddl_phase_clk_b[0].write(v.bit(0).sl());
    ddl_phase_clk_b[1].write(v.bit(1).sl());
    ddl_phase_clk_b[2].write(v.bit(2).sl());
    ddl_phase_clk_b[3].write(v.bit(3).sl());
    ddl_phase_clk_b[4].write(v.bit(4).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot_ddl::split_ddl_tune() {
    const vl::logic<5> v = vl::rd(ddl_tune);
    ddl_tune_b[0].write(v.bit(0).sl());
    ddl_tune_b[1].write(v.bit(1).sl());
    ddl_tune_b[2].write(v.bit(2).sl());
    ddl_tune_b[3].write(v.bit(3).sl());
    ddl_tune_b[4].write(v.bit(4).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot_ddl::merge_ddl_phase_clklv() {
    vl::logic<5> v(0);
    v.set_bit(0, vl::rd(ddl_phase_clklv_b[0]));
    v.set_bit(1, vl::rd(ddl_phase_clklv_b[1]));
    v.set_bit(2, vl::rd(ddl_phase_clklv_b[2]));
    v.set_bit(3, vl::rd(ddl_phase_clklv_b[3]));
    v.set_bit(4, vl::rd(ddl_phase_clklv_b[4]));
    vl::wr(ddl_phase_clklv, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot_ddl::merge_ddl_tunelv() {
    vl::logic<5> v(0);
    v.set_bit(0, vl::rd(ddl_tunelv_b[0]));
    v.set_bit(1, vl::rd(ddl_tunelv_b[1]));
    v.set_bit(2, vl::rd(ddl_tunelv_b[2]));
    v.set_bit(3, vl::rd(ddl_tunelv_b[3]));
    v.set_bit(4, vl::rd(ddl_tunelv_b[4]));
    vl::wr(ddl_tunelv, v);
}

vl::logic<5> mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot_ddl::rd_ddl_phase_clk() const {
    vl::logic<5> v(0);
    v.set_bit(0, vl::rd(ddl_phase_clk_b[0]));
    v.set_bit(1, vl::rd(ddl_phase_clk_b[1]));
    v.set_bit(2, vl::rd(ddl_phase_clk_b[2]));
    v.set_bit(3, vl::rd(ddl_phase_clk_b[3]));
    v.set_bit(4, vl::rd(ddl_phase_clk_b[4]));
    return v;
}

vl::logic<5> mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot_ddl::rd_ddl_phase_clklv() const {
    vl::logic<5> v(0);
    v.set_bit(0, vl::rd(ddl_phase_clklv_b[0]));
    v.set_bit(1, vl::rd(ddl_phase_clklv_b[1]));
    v.set_bit(2, vl::rd(ddl_phase_clklv_b[2]));
    v.set_bit(3, vl::rd(ddl_phase_clklv_b[3]));
    v.set_bit(4, vl::rd(ddl_phase_clklv_b[4]));
    return v;
}

vl::logic<5> mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot_ddl::rd_ddl_tune() const {
    vl::logic<5> v(0);
    v.set_bit(0, vl::rd(ddl_tune_b[0]));
    v.set_bit(1, vl::rd(ddl_tune_b[1]));
    v.set_bit(2, vl::rd(ddl_tune_b[2]));
    v.set_bit(3, vl::rd(ddl_tune_b[3]));
    v.set_bit(4, vl::rd(ddl_tune_b[4]));
    return v;
}

vl::logic<5> mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot_ddl::rd_ddl_tunelv() const {
    vl::logic<5> v(0);
    v.set_bit(0, vl::rd(ddl_tunelv_b[0]));
    v.set_bit(1, vl::rd(ddl_tunelv_b[1]));
    v.set_bit(2, vl::rd(ddl_tunelv_b[2]));
    v.set_bit(3, vl::rd(ddl_tunelv_b[3]));
    v.set_bit(4, vl::rd(ddl_tunelv_b[4]));
    return v;
}

vl::logic<11> mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot_ddl::rd_net159() const {
    vl::logic<11> v(0);
    v.set_bit(0, vl::rd(net159[0]));
    v.set_bit(1, vl::rd(net159[1]));
    v.set_bit(2, vl::rd(net159[2]));
    v.set_bit(3, vl::rd(net159[3]));
    v.set_bit(4, vl::rd(net159[4]));
    v.set_bit(5, vl::rd(net159[5]));
    v.set_bit(6, vl::rd(net159[6]));
    v.set_bit(7, vl::rd(net159[7]));
    v.set_bit(8, vl::rd(net159[8]));
    v.set_bit(9, vl::rd(net159[9]));
    v.set_bit(10, vl::rd(net159[10]));
    return v;
}
