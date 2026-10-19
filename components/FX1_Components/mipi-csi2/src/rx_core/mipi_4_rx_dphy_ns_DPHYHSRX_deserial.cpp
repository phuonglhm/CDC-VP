//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_deserial
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_deserial.h"

mipi_4_rx_dphy_ns_DPHYHSRX_deserial::mipi_4_rx_dphy_ns_DPHYHSRX_deserial(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    I113.reset(new mipi_4_rx_dphy_ns_sc_invx16_lvt("I113"));
    I113->a(des2del[1]);
    I113->z(des[7]);
    I113->gd(vss);
    I113->vp(vdd);
    I117.reset(new mipi_4_rx_dphy_ns_sc_invx16_lvt("I117"));
    I117->a(des2del[0]);
    I117->z(des[6]);
    I117->gd(vss);
    I117->vp(vdd);
    I131.reset(new mipi_4_rx_dphy_ns_sc_na2x6_lvt("I131"));
    I131->a(clk_dummy);
    I131->b(en_logic);
    I131->z(net1156);
    I131->gd(gd);
    I131->vp(vp);
    NAND_CDM_2.reset(new mipi_4_rx_dphy_ns_sc_na2x6_lvt("NAND_CDM_2"));
    NAND_CDM_2->a(d0);
    NAND_CDM_2->b(en_logic);
    NAND_CDM_2->z(hs_data[1]);
    NAND_CDM_2->gd(gd);
    NAND_CDM_2->vp(vp);
    NAND_CDM_1.reset(new mipi_4_rx_dphy_ns_sc_na2x6_lvt("NAND_CDM_1"));
    NAND_CDM_1->a(d1);
    NAND_CDM_1->b(en_logic);
    NAND_CDM_1->z(hs_data[0]);
    NAND_CDM_1->gd(gd);
    NAND_CDM_1->vp(vp);
    NAND_CDM_0.reset(new mipi_4_rx_dphy_ns_sc_na2x6_lvt("NAND_CDM_0"));
    NAND_CDM_0->a(clk);
    NAND_CDM_0->b(en_logic);
    NAND_CDM_0->z(hs_clk);
    NAND_CDM_0->gd(gd);
    NAND_CDM_0->vp(vp);
    FF_7.reset(new mipi_4_rx_dphy_ns_sc_ffx23_lvt("FF_7"));
    FF_7->ck(local_clk);
    FF_7->d(des[7]);
    FF_7->q(dataz[7]);
    FF_7->gd(vss);
    FF_7->vp(vdd);
    FF_6.reset(new mipi_4_rx_dphy_ns_sc_ffx23_lvt("FF_6"));
    FF_6->ck(local_clk);
    FF_6->d(des[6]);
    FF_6->q(dataz[6]);
    FF_6->gd(vss);
    FF_6->vp(vdd);
    FF_5.reset(new mipi_4_rx_dphy_ns_sc_ffx23_lvt("FF_5"));
    FF_5->ck(local_clk);
    FF_5->d(des[5]);
    FF_5->q(dataz[5]);
    FF_5->gd(vss);
    FF_5->vp(vdd);
    FF_4.reset(new mipi_4_rx_dphy_ns_sc_ffx23_lvt("FF_4"));
    FF_4->ck(local_clk);
    FF_4->d(des[4]);
    FF_4->q(dataz[4]);
    FF_4->gd(vss);
    FF_4->vp(vdd);
    FF_3.reset(new mipi_4_rx_dphy_ns_sc_ffx23_lvt("FF_3"));
    FF_3->ck(local_clk);
    FF_3->d(des[3]);
    FF_3->q(dataz[3]);
    FF_3->gd(vss);
    FF_3->vp(vdd);
    FF_2.reset(new mipi_4_rx_dphy_ns_sc_ffx23_lvt("FF_2"));
    FF_2->ck(local_clk);
    FF_2->d(des[2]);
    FF_2->q(dataz[2]);
    FF_2->gd(vss);
    FF_2->vp(vdd);
    FF_1.reset(new mipi_4_rx_dphy_ns_sc_ffx23_lvt("FF_1"));
    FF_1->ck(local_clk);
    FF_1->d(des[1]);
    FF_1->q(dataz[1]);
    FF_1->gd(vss);
    FF_1->vp(vdd);
    FF_0.reset(new mipi_4_rx_dphy_ns_sc_ffx23_lvt("FF_0"));
    FF_0->ck(local_clk);
    FF_0->d(des[0]);
    FF_0->q(dataz[0]);
    FF_0->gd(vss);
    FF_0->vp(vdd);
    INV_7.reset(new mipi_4_rx_dphy_ns_sc_invx24_lvt("INV_7"));
    INV_7->a(dataz[7]);
    INV_7->z(data_int[0]);
    INV_7->gd(vss);
    INV_7->vp(vdd);
    INV_6.reset(new mipi_4_rx_dphy_ns_sc_invx24_lvt("INV_6"));
    INV_6->a(dataz[6]);
    INV_6->z(data_int[1]);
    INV_6->gd(vss);
    INV_6->vp(vdd);
    INV_5.reset(new mipi_4_rx_dphy_ns_sc_invx24_lvt("INV_5"));
    INV_5->a(dataz[5]);
    INV_5->z(data_int[2]);
    INV_5->gd(vss);
    INV_5->vp(vdd);
    INV_4.reset(new mipi_4_rx_dphy_ns_sc_invx24_lvt("INV_4"));
    INV_4->a(dataz[4]);
    INV_4->z(data_int[3]);
    INV_4->gd(vss);
    INV_4->vp(vdd);
    INV_3.reset(new mipi_4_rx_dphy_ns_sc_invx24_lvt("INV_3"));
    INV_3->a(dataz[3]);
    INV_3->z(data_int[4]);
    INV_3->gd(vss);
    INV_3->vp(vdd);
    INV_2.reset(new mipi_4_rx_dphy_ns_sc_invx24_lvt("INV_2"));
    INV_2->a(dataz[2]);
    INV_2->z(data_int[5]);
    INV_2->gd(vss);
    INV_2->vp(vdd);
    INV_1.reset(new mipi_4_rx_dphy_ns_sc_invx24_lvt("INV_1"));
    INV_1->a(dataz[1]);
    INV_1->z(data_int[6]);
    INV_1->gd(vss);
    INV_1->vp(vdd);
    INV_0.reset(new mipi_4_rx_dphy_ns_sc_invx24_lvt("INV_0"));
    INV_0->a(dataz[0]);
    INV_0->z(data_int[7]);
    INV_0->gd(vss);
    INV_0->vp(vdd);
    tie.reset(new mipi_4_rx_dphy_ns_sc_tie_hilow_lvt("tie"));
    tie->gd(vss);
    tie->tie_high(tie_high);
    tie->tie_low(tie_low);
    tie->vp(vdd);
    CDM_OUT_2.reset(new mipi_4_rx_dphy_ns_sc_no2x12_lvt("CDM_OUT_2"));
    CDM_OUT_2->a(tie_low_cdm);
    CDM_OUT_2->b(hs_data[1]);
    CDM_OUT_2->z(d0_dig);
    CDM_OUT_2->gd(gd);
    CDM_OUT_2->vp(vp);
    CDM_OUT_1.reset(new mipi_4_rx_dphy_ns_sc_no2x12_lvt("CDM_OUT_1"));
    CDM_OUT_1->a(tie_low_cdm);
    CDM_OUT_1->b(hs_data[0]);
    CDM_OUT_1->z(d1_dig);
    CDM_OUT_1->gd(gd);
    CDM_OUT_1->vp(vp);
    CDM_OUT_0.reset(new mipi_4_rx_dphy_ns_sc_no2x12_lvt("CDM_OUT_0"));
    CDM_OUT_0->a(tie_low_cdm);
    CDM_OUT_0->b(hs_clk);
    CDM_OUT_0->z(clk_dig);
    CDM_OUT_0->gd(gd);
    CDM_OUT_0->vp(vp);
    ALI.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_des_align("ALI"));
    ALI->clk(clk_dig_nn);
    ALI->clk_en(clk_en);
    ALI->d0(d0_dig);
    ALI->d1(d1_dig);
    ALI->data_en(data_en);
    ALI->gated_polarity(gated_polarity);
    ALI->gd(vss);
    ALI->tie_high_cdm_vdd(tie_high_cdm_vdd);
    ALI->tie_low_cdm_vdd(tie_low_cdm_vdd);
    ALI->vp(vdd);
    ALI->align0(allign0);
    ALI->align1(align1);
    SC_METHOD(split_des0_rx_data);
    sensitive << c_des0_rx_data;
    dont_initialize();
    des0.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_des_des4("des0"));
    des0->data(allign0);
    des0->div5(div5);
    des0->gd(vss);
    des0->tie_high(tie_high);
    des0->vp(vdd);
    des0->rx_data(c_des0_rx_data);
    SC_METHOD(split_des1_rx_data);
    sensitive << c_des1_rx_data;
    dont_initialize();
    des1.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_des_des4("des1"));
    des1->data(align1);
    des1->div5(div5);
    des1->gd(vss);
    des1->tie_high(tie_high);
    des1->vp(vdd);
    des1->rx_data(c_des1_rx_data);
    DIV4.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_des_div4("DIV4"));
    DIV4->clk_en(clk_en);
    DIV4->div_en(div_en);
    DIV4->pll_clk(clk_dig_nn);
    DIV4->div5(div5);
    DIV4->local_clk(local_clk);
    DIV4->word_clk(word_clk);
    DIV4->gd(vss);
    DIV4->vp(vdd);
    I83_1.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_nand_lvt("I83_1"));
    I83_1->a(clk_dig);
    I83_1->tie_high_cdm(tie_high_cdm_vdd);
    I83_1->tie_low_cdm(tie_low_cdm_vdd);
    I83_1->z(clk_dig_n);
    I83_1->gd(vss);
    I83_1->vp(vdd);
    I83_0.reset(new mipi_4_rx_dphy_ns_LEVELCONV_cdm_nand_lvt("I83_0"));
    I83_0->a(clk_dig);
    I83_0->tie_high_cdm(tie_high_cdm_vdd);
    I83_0->tie_low_cdm(tie_low_cdm_vdd);
    I83_0->z(clk_dig_n);
    I83_0->gd(vss);
    I83_0->vp(vdd);
    TIE_CDM.reset(new mipi_4_rx_dphy_ns_LEVELCONV_l2l_cdm_tie_LowHigh("TIE_CDM"));
    TIE_CDM->gd(vss);
    TIE_CDM->vp(vdd);
    TIE_CDM->tie_high(tie_high_cdm_vdd);
    TIE_CDM->tie_low(tie_low_cdm_vdd);
    I54.reset(new mipi_4_rx_dphy_ns_sc_invx96_lvt("I54"));
    I54->a(clk_dig_n);
    I54->z(clk_dig_nn);
    I54->gd(vss);
    I54->vp(vdd);
    SC_METHOD(proc_initial_156155);
    SC_METHOD(proc_always_156157);
    sensitive << data_swap << data_int[0] << data_int[1] << data_int[2] << data_int[3] << data_int[4] << data_int[5] << data_int[6] << data_int[7];
}

vl::logic<8> mipi_4_rx_dphy_ns_DPHYHSRX_deserial::rd_data_int() const {
    vl::logic<8> v(0);
    v.set_bit(0, vl::rd(data_int[0]));
    v.set_bit(1, vl::rd(data_int[1]));
    v.set_bit(2, vl::rd(data_int[2]));
    v.set_bit(3, vl::rd(data_int[3]));
    v.set_bit(4, vl::rd(data_int[4]));
    v.set_bit(5, vl::rd(data_int[5]));
    v.set_bit(6, vl::rd(data_int[6]));
    v.set_bit(7, vl::rd(data_int[7]));
    return v;
}

vl::logic<8> mipi_4_rx_dphy_ns_DPHYHSRX_deserial::rd_dataz() const {
    vl::logic<8> v(0);
    v.set_bit(0, vl::rd(dataz[0]));
    v.set_bit(1, vl::rd(dataz[1]));
    v.set_bit(2, vl::rd(dataz[2]));
    v.set_bit(3, vl::rd(dataz[3]));
    v.set_bit(4, vl::rd(dataz[4]));
    v.set_bit(5, vl::rd(dataz[5]));
    v.set_bit(6, vl::rd(dataz[6]));
    v.set_bit(7, vl::rd(dataz[7]));
    return v;
}

vl::logic<8> mipi_4_rx_dphy_ns_DPHYHSRX_deserial::rd_des() const {
    vl::logic<8> v(0);
    v.set_bit(0, vl::rd(des[0]));
    v.set_bit(1, vl::rd(des[1]));
    v.set_bit(2, vl::rd(des[2]));
    v.set_bit(3, vl::rd(des[3]));
    v.set_bit(4, vl::rd(des[4]));
    v.set_bit(5, vl::rd(des[5]));
    v.set_bit(6, vl::rd(des[6]));
    v.set_bit(7, vl::rd(des[7]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYHSRX_deserial::rd_des2del() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(des2del[0]));
    v.set_bit(1, vl::rd(des2del[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYHSRX_deserial::rd_hs_data() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(hs_data[0]));
    v.set_bit(1, vl::rd(hs_data[1]));
    return v;
}

void mipi_4_rx_dphy_ns_DPHYHSRX_deserial::split_des0_rx_data() {
    const vl::logic<4> v = vl::rd(c_des0_rx_data);
    des[0].write(v.bit(0).sl());
    des[2].write(v.bit(1).sl());
    des[4].write(v.bit(2).sl());
    des2del[0].write(v.bit(3).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_deserial::split_des1_rx_data() {
    const vl::logic<4> v = vl::rd(c_des1_rx_data);
    des[1].write(v.bit(0).sl());
    des[3].write(v.bit(1).sl());
    des[5].write(v.bit(2).sl());
    des2del[1].write(v.bit(3).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_deserial::proc_initial_156155() {
    vl::logic<1> data_swap_sh = vl::rd(data_swap);
    data_swap_sh = vl::logic<1>(0x0ULL);
    vl::wr(data_swap, data_swap_sh);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_deserial::proc_always_156157() {
    { const vl::logic<8> v_ = vl::mux(vl::rd(data_swap), vl::cat(vl::rd(data_int[0]), vl::rd(data_int[1]), vl::rd(data_int[2]), vl::rd(data_int[3]), vl::rd(data_int[4]), vl::rd(data_int[5]), vl::rd(data_int[6]), vl::rd(data_int[7])), rd_data_int()); aq_.after(vl::dly(1.0, TU_, TP_), [this, v_]() { vl::wr(data, v_); }); }
}
