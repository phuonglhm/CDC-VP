//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot.h"

mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot::mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_ddl_phase_data);
    sensitive << ddl_phase_data;
    SC_METHOD(split_gmode);
    sensitive << gmode;
    SC_METHOD(split_offset_cal);
    sensitive << offset_cal;
    SC_METHOD(merge_ddl_phase_datalv);
    sensitive << ddl_phase_datalv_b[0] << ddl_phase_datalv_b[1] << ddl_phase_datalv_b[2] << ddl_phase_datalv_b[3];
    SC_METHOD(merge_gmodelv);
    sensitive << gmodelv_b[0] << gmodelv_b[1];
    SC_METHOD(merge_offset_callv);
    sensitive << offset_callv_b[0] << offset_callv_b[1] << offset_callv_b[2] << offset_callv_b[3] << offset_callv_b[4] << offset_callv_b[5];
    TIE_CDM.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2l_cdm_tie_LowHigh("TIE_CDM"));
    TIE_CDM->gd(gd);
    TIE_CDM->vp(vp);
    TIE_CDM->tie_high(tie_high_cdm);
    TIE_CDM->tie_low(tie_low_cdm);
    CDM1A_5.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("CDM1A_5"));
    CDM1A_5->a(offset_cal_b[5]);
    CDM1A_5->tie_high_cdm(tie_high_cdm);
    CDM1A_5->tie_low_cdm(tie_low_cdm);
    CDM1A_5->z(net199[5]);
    CDM1A_5->gd(gd);
    CDM1A_5->vp(vp);
    CDM1A_4.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("CDM1A_4"));
    CDM1A_4->a(offset_cal_b[4]);
    CDM1A_4->tie_high_cdm(tie_high_cdm);
    CDM1A_4->tie_low_cdm(tie_low_cdm);
    CDM1A_4->z(net199[4]);
    CDM1A_4->gd(gd);
    CDM1A_4->vp(vp);
    CDM1A_3.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("CDM1A_3"));
    CDM1A_3->a(offset_cal_b[3]);
    CDM1A_3->tie_high_cdm(tie_high_cdm);
    CDM1A_3->tie_low_cdm(tie_low_cdm);
    CDM1A_3->z(net199[3]);
    CDM1A_3->gd(gd);
    CDM1A_3->vp(vp);
    CDM1A_2.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("CDM1A_2"));
    CDM1A_2->a(offset_cal_b[2]);
    CDM1A_2->tie_high_cdm(tie_high_cdm);
    CDM1A_2->tie_low_cdm(tie_low_cdm);
    CDM1A_2->z(net199[2]);
    CDM1A_2->gd(gd);
    CDM1A_2->vp(vp);
    CDM1A_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("CDM1A_1"));
    CDM1A_1->a(offset_cal_b[1]);
    CDM1A_1->tie_high_cdm(tie_high_cdm);
    CDM1A_1->tie_low_cdm(tie_low_cdm);
    CDM1A_1->z(net199[1]);
    CDM1A_1->gd(gd);
    CDM1A_1->vp(vp);
    CDM1A_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("CDM1A_0"));
    CDM1A_0->a(offset_cal_b[0]);
    CDM1A_0->tie_high_cdm(tie_high_cdm);
    CDM1A_0->tie_low_cdm(tie_low_cdm);
    CDM1A_0->z(net199[0]);
    CDM1A_0->gd(gd);
    CDM1A_0->vp(vp);
    I92_4.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I92_4"));
    I92_4->a(enlogic);
    I92_4->tie_high_cdm(tie_high_cdm);
    I92_4->tie_low_cdm(tie_low_cdm);
    I92_4->z(net200[4]);
    I92_4->gd(gd);
    I92_4->vp(vp);
    I92_3.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I92_3"));
    I92_3->a(pon);
    I92_3->tie_high_cdm(tie_high_cdm);
    I92_3->tie_low_cdm(tie_low_cdm);
    I92_3->z(net200[3]);
    I92_3->gd(gd);
    I92_3->vp(vp);
    I92_2.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I92_2"));
    I92_2->a(en);
    I92_2->tie_high_cdm(tie_high_cdm);
    I92_2->tie_low_cdm(tie_low_cdm);
    I92_2->z(net200[2]);
    I92_2->gd(gd);
    I92_2->vp(vp);
    I92_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I92_1"));
    I92_1->a(feed_int_clk);
    I92_1->tie_high_cdm(tie_high_cdm);
    I92_1->tie_low_cdm(tie_low_cdm);
    I92_1->z(net200[1]);
    I92_1->gd(gd);
    I92_1->vp(vp);
    I92_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I92_0"));
    I92_0->a(clk_from_long_channel);
    I92_0->tie_high_cdm(tie_high_cdm);
    I92_0->tie_low_cdm(tie_low_cdm);
    I92_0->z(net200[0]);
    I92_0->gd(gd);
    I92_0->vp(vp);
    I93_4.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I93_4"));
    I93_4->a(data_offset_cal_en);
    I93_4->tie_high_cdm(tie_high_cdm);
    I93_4->tie_low_cdm(tie_low_cdm);
    I93_4->z(net205[4]);
    I93_4->gd(gd);
    I93_4->vp(vp);
    I93_3.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I93_3"));
    I93_3->a(ddl_phase_data_b[3]);
    I93_3->tie_high_cdm(tie_high_cdm);
    I93_3->tie_low_cdm(tie_low_cdm);
    I93_3->z(net205[3]);
    I93_3->gd(gd);
    I93_3->vp(vp);
    I93_2.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I93_2"));
    I93_2->a(ddl_phase_data_b[2]);
    I93_2->tie_high_cdm(tie_high_cdm);
    I93_2->tie_low_cdm(tie_low_cdm);
    I93_2->z(net205[2]);
    I93_2->gd(gd);
    I93_2->vp(vp);
    I93_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I93_1"));
    I93_1->a(ddl_phase_data_b[1]);
    I93_1->tie_high_cdm(tie_high_cdm);
    I93_1->tie_low_cdm(tie_low_cdm);
    I93_1->z(net205[1]);
    I93_1->gd(gd);
    I93_1->vp(vp);
    I93_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I93_0"));
    I93_0->a(ddl_phase_data_b[0]);
    I93_0->tie_high_cdm(tie_high_cdm);
    I93_0->tie_low_cdm(tie_low_cdm);
    I93_0->z(net205[0]);
    I93_0->gd(gd);
    I93_0->vp(vp);
    I94_3.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I94_3"));
    I94_3->a(gmode_b[1]);
    I94_3->tie_high_cdm(tie_high_cdm);
    I94_3->tie_low_cdm(tie_low_cdm);
    I94_3->z(net197[3]);
    I94_3->gd(gd);
    I94_3->vp(vp);
    I94_2.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I94_2"));
    I94_2->a(gmode_b[0]);
    I94_2->tie_high_cdm(tie_high_cdm);
    I94_2->tie_low_cdm(tie_low_cdm);
    I94_2->z(net197[2]);
    I94_2->gd(gd);
    I94_2->vp(vp);
    I94_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I94_1"));
    I94_1->a(compatibility_mode);
    I94_1->tie_high_cdm(tie_high_cdm);
    I94_1->tie_low_cdm(tie_low_cdm);
    I94_1->z(net197[1]);
    I94_1->gd(gd);
    I94_1->vp(vp);
    I94_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt("I94_0"));
    I94_0->a(phase_change_p);
    I94_0->tie_high_cdm(tie_high_cdm);
    I94_0->tie_low_cdm(tie_low_cdm);
    I94_0->z(net197[0]);
    I94_0->gd(gd);
    I94_0->vp(vp);
    sc_lvt_I90.reset(new mipi_4_rx_dphy_ns_sc_na2x6_lvt("sc_lvt_I90"));
    sc_lvt_I90->a(ponlv);
    sc_lvt_I90->b(compatibility_modelv);
    sc_lvt_I90->z(compatibility_onlvz);
    sc_lvt_I90->gd(gd);
    sc_lvt_I90->vp(vp);
    sc_lvt_I18_4.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I18_4"));
    sc_lvt_I18_4->a(net205[4]);
    sc_lvt_I18_4->z(data_offset_cal_enlv);
    sc_lvt_I18_4->gd(gd);
    sc_lvt_I18_4->vp(vp);
    sc_lvt_I18_3.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I18_3"));
    sc_lvt_I18_3->a(net205[3]);
    sc_lvt_I18_3->z(ddl_phase_datalv_b[3]);
    sc_lvt_I18_3->gd(gd);
    sc_lvt_I18_3->vp(vp);
    sc_lvt_I18_2.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I18_2"));
    sc_lvt_I18_2->a(net205[2]);
    sc_lvt_I18_2->z(ddl_phase_datalv_b[2]);
    sc_lvt_I18_2->gd(gd);
    sc_lvt_I18_2->vp(vp);
    sc_lvt_I18_1.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I18_1"));
    sc_lvt_I18_1->a(net205[1]);
    sc_lvt_I18_1->z(ddl_phase_datalv_b[1]);
    sc_lvt_I18_1->gd(gd);
    sc_lvt_I18_1->vp(vp);
    sc_lvt_I18_0.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I18_0"));
    sc_lvt_I18_0->a(net205[0]);
    sc_lvt_I18_0->z(ddl_phase_datalv_b[0]);
    sc_lvt_I18_0->gd(gd);
    sc_lvt_I18_0->vp(vp);
    sc_lvt_I71_2.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I71_2"));
    sc_lvt_I71_2->a(enlogiclv);
    sc_lvt_I71_2->z(enlogiclvz);
    sc_lvt_I71_2->gd(gd);
    sc_lvt_I71_2->vp(vp);
    sc_lvt_I71_1.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I71_1"));
    sc_lvt_I71_1->a(ponlv);
    sc_lvt_I71_1->z(ponlvz);
    sc_lvt_I71_1->gd(gd);
    sc_lvt_I71_1->vp(vp);
    sc_lvt_I71_0.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I71_0"));
    sc_lvt_I71_0->a(enlv);
    sc_lvt_I71_0->z(enlvz);
    sc_lvt_I71_0->gd(gd);
    sc_lvt_I71_0->vp(vp);
    sc_lvt_I76_3.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I76_3"));
    sc_lvt_I76_3->a(net197[3]);
    sc_lvt_I76_3->z(gmodelv_b[1]);
    sc_lvt_I76_3->gd(gd);
    sc_lvt_I76_3->vp(vp);
    sc_lvt_I76_2.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I76_2"));
    sc_lvt_I76_2->a(net197[2]);
    sc_lvt_I76_2->z(gmodelv_b[0]);
    sc_lvt_I76_2->gd(gd);
    sc_lvt_I76_2->vp(vp);
    sc_lvt_I76_1.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I76_1"));
    sc_lvt_I76_1->a(net197[1]);
    sc_lvt_I76_1->z(compatibility_modelv);
    sc_lvt_I76_1->gd(gd);
    sc_lvt_I76_1->vp(vp);
    sc_lvt_I76_0.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I76_0"));
    sc_lvt_I76_0->a(net197[0]);
    sc_lvt_I76_0->z(phase_change_plv);
    sc_lvt_I76_0->gd(gd);
    sc_lvt_I76_0->vp(vp);
    sc_lvt_I95.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I95"));
    sc_lvt_I95->a(compatibility_onlvz);
    sc_lvt_I95->z(compatibility_onlv);
    sc_lvt_I95->gd(gd);
    sc_lvt_I95->vp(vp);
    sc_lvt_I272_4.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I272_4"));
    sc_lvt_I272_4->a(net200[4]);
    sc_lvt_I272_4->z(enlogiclv);
    sc_lvt_I272_4->gd(gd);
    sc_lvt_I272_4->vp(vp);
    sc_lvt_I272_3.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I272_3"));
    sc_lvt_I272_3->a(net200[3]);
    sc_lvt_I272_3->z(ponlv);
    sc_lvt_I272_3->gd(gd);
    sc_lvt_I272_3->vp(vp);
    sc_lvt_I272_2.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I272_2"));
    sc_lvt_I272_2->a(net200[2]);
    sc_lvt_I272_2->z(enlv);
    sc_lvt_I272_2->gd(gd);
    sc_lvt_I272_2->vp(vp);
    sc_lvt_I272_1.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I272_1"));
    sc_lvt_I272_1->a(net200[1]);
    sc_lvt_I272_1->z(feed_int_clklv);
    sc_lvt_I272_1->gd(gd);
    sc_lvt_I272_1->vp(vp);
    sc_lvt_I272_0.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I272_0"));
    sc_lvt_I272_0->a(net200[0]);
    sc_lvt_I272_0->z(clk_from_long_channellv);
    sc_lvt_I272_0->gd(gd);
    sc_lvt_I272_0->vp(vp);
    sc_lvt_I273_5.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I273_5"));
    sc_lvt_I273_5->a(net199[5]);
    sc_lvt_I273_5->z(offset_callv_b[5]);
    sc_lvt_I273_5->gd(gd);
    sc_lvt_I273_5->vp(vp);
    sc_lvt_I273_4.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I273_4"));
    sc_lvt_I273_4->a(net199[4]);
    sc_lvt_I273_4->z(offset_callv_b[4]);
    sc_lvt_I273_4->gd(gd);
    sc_lvt_I273_4->vp(vp);
    sc_lvt_I273_3.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I273_3"));
    sc_lvt_I273_3->a(net199[3]);
    sc_lvt_I273_3->z(offset_callv_b[3]);
    sc_lvt_I273_3->gd(gd);
    sc_lvt_I273_3->vp(vp);
    sc_lvt_I273_2.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I273_2"));
    sc_lvt_I273_2->a(net199[2]);
    sc_lvt_I273_2->z(offset_callv_b[2]);
    sc_lvt_I273_2->gd(gd);
    sc_lvt_I273_2->vp(vp);
    sc_lvt_I273_1.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I273_1"));
    sc_lvt_I273_1->a(net199[1]);
    sc_lvt_I273_1->z(offset_callv_b[1]);
    sc_lvt_I273_1->gd(gd);
    sc_lvt_I273_1->vp(vp);
    sc_lvt_I273_0.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I273_0"));
    sc_lvt_I273_0->a(net199[0]);
    sc_lvt_I273_0->z(offset_callv_b[0]);
    sc_lvt_I273_0->gd(gd);
    sc_lvt_I273_0->vp(vp);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot::split_ddl_phase_data() {
    const vl::logic<4> v = vl::rd(ddl_phase_data);
    ddl_phase_data_b[0].write(v.bit(0).sl());
    ddl_phase_data_b[1].write(v.bit(1).sl());
    ddl_phase_data_b[2].write(v.bit(2).sl());
    ddl_phase_data_b[3].write(v.bit(3).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot::split_gmode() {
    const vl::logic<2> v = vl::rd(gmode);
    gmode_b[0].write(v.bit(0).sl());
    gmode_b[1].write(v.bit(1).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot::split_offset_cal() {
    const vl::logic<6> v = vl::rd(offset_cal);
    offset_cal_b[0].write(v.bit(0).sl());
    offset_cal_b[1].write(v.bit(1).sl());
    offset_cal_b[2].write(v.bit(2).sl());
    offset_cal_b[3].write(v.bit(3).sl());
    offset_cal_b[4].write(v.bit(4).sl());
    offset_cal_b[5].write(v.bit(5).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot::merge_ddl_phase_datalv() {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(ddl_phase_datalv_b[0]));
    v.set_bit(1, vl::rd(ddl_phase_datalv_b[1]));
    v.set_bit(2, vl::rd(ddl_phase_datalv_b[2]));
    v.set_bit(3, vl::rd(ddl_phase_datalv_b[3]));
    vl::wr(ddl_phase_datalv, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot::merge_gmodelv() {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(gmodelv_b[0]));
    v.set_bit(1, vl::rd(gmodelv_b[1]));
    vl::wr(gmodelv, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot::merge_offset_callv() {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd(offset_callv_b[0]));
    v.set_bit(1, vl::rd(offset_callv_b[1]));
    v.set_bit(2, vl::rd(offset_callv_b[2]));
    v.set_bit(3, vl::rd(offset_callv_b[3]));
    v.set_bit(4, vl::rd(offset_callv_b[4]));
    v.set_bit(5, vl::rd(offset_callv_b[5]));
    vl::wr(offset_callv, v);
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot::rd_ddl_phase_data() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(ddl_phase_data_b[0]));
    v.set_bit(1, vl::rd(ddl_phase_data_b[1]));
    v.set_bit(2, vl::rd(ddl_phase_data_b[2]));
    v.set_bit(3, vl::rd(ddl_phase_data_b[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot::rd_ddl_phase_datalv() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(ddl_phase_datalv_b[0]));
    v.set_bit(1, vl::rd(ddl_phase_datalv_b[1]));
    v.set_bit(2, vl::rd(ddl_phase_datalv_b[2]));
    v.set_bit(3, vl::rd(ddl_phase_datalv_b[3]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot::rd_gmode() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(gmode_b[0]));
    v.set_bit(1, vl::rd(gmode_b[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot::rd_gmodelv() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(gmodelv_b[0]));
    v.set_bit(1, vl::rd(gmodelv_b[1]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot::rd_net197() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(net197[0]));
    v.set_bit(1, vl::rd(net197[1]));
    v.set_bit(2, vl::rd(net197[2]));
    v.set_bit(3, vl::rd(net197[3]));
    return v;
}

vl::logic<6> mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot::rd_net199() const {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd(net199[0]));
    v.set_bit(1, vl::rd(net199[1]));
    v.set_bit(2, vl::rd(net199[2]));
    v.set_bit(3, vl::rd(net199[3]));
    v.set_bit(4, vl::rd(net199[4]));
    v.set_bit(5, vl::rd(net199[5]));
    return v;
}

vl::logic<5> mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot::rd_net200() const {
    vl::logic<5> v(0);
    v.set_bit(0, vl::rd(net200[0]));
    v.set_bit(1, vl::rd(net200[1]));
    v.set_bit(2, vl::rd(net200[2]));
    v.set_bit(3, vl::rd(net200[3]));
    v.set_bit(4, vl::rd(net200[4]));
    return v;
}

vl::logic<5> mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot::rd_net205() const {
    vl::logic<5> v(0);
    v.set_bit(0, vl::rd(net205[0]));
    v.set_bit(1, vl::rd(net205[1]));
    v.set_bit(2, vl::rd(net205[2]));
    v.set_bit(3, vl::rd(net205[3]));
    v.set_bit(4, vl::rd(net205[4]));
    return v;
}

vl::logic<6> mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot::rd_offset_cal() const {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd(offset_cal_b[0]));
    v.set_bit(1, vl::rd(offset_cal_b[1]));
    v.set_bit(2, vl::rd(offset_cal_b[2]));
    v.set_bit(3, vl::rd(offset_cal_b[3]));
    v.set_bit(4, vl::rd(offset_cal_b[4]));
    v.set_bit(5, vl::rd(offset_cal_b[5]));
    return v;
}

vl::logic<6> mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot::rd_offset_callv() const {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd(offset_callv_b[0]));
    v.set_bit(1, vl::rd(offset_callv_b[1]));
    v.set_bit(2, vl::rd(offset_callv_b[2]));
    v.set_bit(3, vl::rd(offset_callv_b[3]));
    v.set_bit(4, vl::rd(offset_callv_b[4]));
    v.set_bit(5, vl::rd(offset_callv_b[5]));
    return v;
}
