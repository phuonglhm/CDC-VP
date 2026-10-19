//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic.h"

mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_atb_sw_hv);
    sensitive << atb_sw_hv;
    SC_METHOD(split_gear);
    sensitive << gear;
    SC_METHOD(split_gearhv);
    sensitive << gearhv;
    SC_METHOD(split_lb_current_h);
    sensitive << lb_current_h;
    SC_METHOD(split_rx_eq_bcal_h);
    sensitive << rx_eq_bcal_h;
    SC_METHOD(split_rx_hs_bcal);
    sensitive << rx_hs_bcal;
    SC_METHOD(merge_atb_sw_hvz);
    sensitive << atb_sw_hvz_b[0] << atb_sw_hvz_b[1];
    SC_METHOD(merge_key);
    sensitive << key_b[0] << key_b[1] << key_b[2] << key_b[3];
    SC_METHOD(merge_keyhv);
    sensitive << keyhv_b[0] << keyhv_b[1] << keyhv_b[2] << keyhv_b[3];
    SC_METHOD(merge_keyhvz);
    sensitive << keyhvz_b[0] << keyhvz_b[1] << keyhvz_b[2] << keyhvz_b[3];
    SC_METHOD(merge_keyz);
    sensitive << keyz_b[0] << keyz_b[1] << keyz_b[2] << keyz_b[3];
    SC_METHOD(merge_lbkeyhv);
    sensitive << lbkeyhv_b[0] << lbkeyhv_b[1] << lbkeyhv_b[2] << lbkeyhv_b[3];
    SC_METHOD(merge_rx_eq_bcal_hz);
    sensitive << rx_eq_bcal_hz_b[0] << rx_eq_bcal_hz_b[1] << rx_eq_bcal_hz_b[2];
    SC_METHOD(merge_rx_hs_bcalz);
    sensitive << rx_hs_bcalz_b[0] << rx_hs_bcalz_b[1] << rx_hs_bcalz_b[2] << rx_hs_bcalz_b[3] << rx_hs_bcalz_b[4] << rx_hs_bcalz_b[5];
    I69.reset(new mipi_4_rx_dphy_ns_sc_na2x6_lvt("I69"));
    I69->a(encal_z);
    I69->b(en_lb);
    I69->z(lbcalsw);
    I69->gd(gd);
    I69->vp(vp);
    sc_lvt_I30.reset(new mipi_4_rx_dphy_ns_sc_na2x6_lvt("sc_lvt_I30"));
    sc_lvt_I30->a(gear_b[1]);
    sc_lvt_I30->b(gear_b[0]);
    sc_lvt_I30->z(keyx[2]);
    sc_lvt_I30->gd(gd);
    sc_lvt_I30->vp(vp);
    sc_lvt_I33_2.reset(new mipi_4_rx_dphy_ns_sc_na2x6_lvt("sc_lvt_I33_2"));
    sc_lvt_I33_2->a(keyxz[2]);
    sc_lvt_I33_2->b(keyz_b[0]);
    sc_lvt_I33_2->z(key_b[3]);
    sc_lvt_I33_2->gd(gd);
    sc_lvt_I33_2->vp(vp);
    sc_lvt_I33_1.reset(new mipi_4_rx_dphy_ns_sc_na2x6_lvt("sc_lvt_I33_1"));
    sc_lvt_I33_1->a(keyxz[1]);
    sc_lvt_I33_1->b(keyz_b[0]);
    sc_lvt_I33_1->z(key_b[2]);
    sc_lvt_I33_1->gd(gd);
    sc_lvt_I33_1->vp(vp);
    sc_lvt_I33_0.reset(new mipi_4_rx_dphy_ns_sc_na2x6_lvt("sc_lvt_I33_0"));
    sc_lvt_I33_0->a(keyxz[0]);
    sc_lvt_I33_0->b(keyz_b[0]);
    sc_lvt_I33_0->z(key_b[1]);
    sc_lvt_I33_0->gd(gd);
    sc_lvt_I33_0->vp(vp);
    sc_lvt_NAND0.reset(new mipi_4_rx_dphy_ns_sc_na2x6_lvt("sc_lvt_NAND0"));
    sc_lvt_NAND0->a(on);
    sc_lvt_NAND0->b(rx_pre_burst);
    sc_lvt_NAND0->z(key_b[0]);
    sc_lvt_NAND0->gd(gd);
    sc_lvt_NAND0->vp(vp);
    sctg_I28.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_I28"));
    sctg_I28->a(net64);
    sctg_I28->z(net67);
    sctg_I28->gd(gd);
    sctg_I28->vph(vph);
    sctg_I42.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_I42"));
    sctg_I42->a(gearhv_b[1]);
    sctg_I42->z(keyxhv[1]);
    sctg_I42->gd(gd);
    sctg_I42->vph(vph);
    sctg_I43_2.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_I43_2"));
    sctg_I43_2->a(keyhv_b[3]);
    sctg_I43_2->z(keyhvz_b[3]);
    sctg_I43_2->gd(gd);
    sctg_I43_2->vph(vph);
    sctg_I43_1.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_I43_1"));
    sctg_I43_1->a(keyhv_b[2]);
    sctg_I43_1->z(keyhvz_b[2]);
    sctg_I43_1->gd(gd);
    sctg_I43_1->vph(vph);
    sctg_I43_0.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_I43_0"));
    sctg_I43_0->a(keyhv_b[1]);
    sctg_I43_0->z(keyhvz_b[1]);
    sctg_I43_0->gd(gd);
    sctg_I43_0->vph(vph);
    sctg_I56_2.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_I56_2"));
    sctg_I56_2->a(keyxhv[2]);
    sctg_I56_2->z(keyxhvz[2]);
    sctg_I56_2->gd(gd);
    sctg_I56_2->vph(vph);
    sctg_I56_1.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_I56_1"));
    sctg_I56_1->a(keyxhv[1]);
    sctg_I56_1->z(keyxhvz[1]);
    sctg_I56_1->gd(gd);
    sctg_I56_1->vph(vph);
    sctg_I56_0.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_I56_0"));
    sctg_I56_0->a(keyxhv[0]);
    sctg_I56_0->z(keyxhvz[0]);
    sctg_I56_0->gd(gd);
    sctg_I56_0->vph(vph);
    sctg_I58.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_I58"));
    sctg_I58->a(net179);
    sctg_I58->z(calswn_h);
    sctg_I58->gd(gd);
    sctg_I58->vph(vph);
    sctg_INVH0.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_INVH0"));
    sctg_INVH0->a(onhv);
    sctg_INVH0->z(onhvz);
    sctg_INVH0->gd(gd);
    sctg_INVH0->vph(vph);
    sctg_INVH1.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_INVH1"));
    sctg_INVH1->a(keyhv_b[0]);
    sctg_INVH1->z(keyhvz_b[0]);
    sctg_INVH1->gd(gd);
    sctg_INVH1->vph(vph);
    sctg_INVH5.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_INVH5"));
    sctg_INVH5->a(en_lb_h);
    sctg_INVH5->z(en_lb_hz);
    sctg_INVH5->gd(gd);
    sctg_INVH5->vph(vph);
    sctg_INVH6_2.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_INVH6_2"));
    sctg_INVH6_2->a(rx_eq_bcal_h_b[2]);
    sctg_INVH6_2->z(rx_eq_bcal_hz_b[2]);
    sctg_INVH6_2->gd(gd);
    sctg_INVH6_2->vph(vph);
    sctg_INVH6_1.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_INVH6_1"));
    sctg_INVH6_1->a(rx_eq_bcal_h_b[1]);
    sctg_INVH6_1->z(rx_eq_bcal_hz_b[1]);
    sctg_INVH6_1->gd(gd);
    sctg_INVH6_1->vph(vph);
    sctg_INVH6_0.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_INVH6_0"));
    sctg_INVH6_0->a(rx_eq_bcal_h_b[0]);
    sctg_INVH6_0->z(rx_eq_bcal_hz_b[0]);
    sctg_INVH6_0->gd(gd);
    sctg_INVH6_0->vph(vph);
    sctg_INVH7_1.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_INVH7_1"));
    sctg_INVH7_1->a(atb_sw_hv_b[1]);
    sctg_INVH7_1->z(atb_sw_hvz_b[1]);
    sctg_INVH7_1->gd(gd);
    sctg_INVH7_1->vph(vph);
    sctg_INVH7_0.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_INVH7_0"));
    sctg_INVH7_0->a(atb_sw_hv_b[0]);
    sctg_INVH7_0->z(atb_sw_hvz_b[0]);
    sctg_INVH7_0->gd(gd);
    sctg_INVH7_0->vph(vph);
    I70.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("I70"));
    I70->a(rx_hs_encal);
    I70->z(encal_z);
    I70->gd(gd);
    I70->vp(vp);
    sc_lvt_I31.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("sc_lvt_I31"));
    sc_lvt_I31->a(gear_b[1]);
    sc_lvt_I31->z(keyx[1]);
    sc_lvt_I31->gd(gd);
    sc_lvt_I31->vp(vp);
    sc_lvt_I34_2.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("sc_lvt_I34_2"));
    sc_lvt_I34_2->a(key_b[3]);
    sc_lvt_I34_2->z(keyz_b[3]);
    sc_lvt_I34_2->gd(gd);
    sc_lvt_I34_2->vp(vp);
    sc_lvt_I34_1.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("sc_lvt_I34_1"));
    sc_lvt_I34_1->a(key_b[2]);
    sc_lvt_I34_1->z(keyz_b[2]);
    sc_lvt_I34_1->gd(gd);
    sc_lvt_I34_1->vp(vp);
    sc_lvt_I34_0.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("sc_lvt_I34_0"));
    sc_lvt_I34_0->a(key_b[1]);
    sc_lvt_I34_0->z(keyz_b[1]);
    sc_lvt_I34_0->gd(gd);
    sc_lvt_I34_0->vp(vp);
    sc_lvt_I57_2.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("sc_lvt_I57_2"));
    sc_lvt_I57_2->a(keyx[2]);
    sc_lvt_I57_2->z(keyxz[2]);
    sc_lvt_I57_2->gd(gd);
    sc_lvt_I57_2->vp(vp);
    sc_lvt_I57_1.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("sc_lvt_I57_1"));
    sc_lvt_I57_1->a(keyx[1]);
    sc_lvt_I57_1->z(keyxz[1]);
    sc_lvt_I57_1->gd(gd);
    sc_lvt_I57_1->vp(vp);
    sc_lvt_I57_0.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("sc_lvt_I57_0"));
    sc_lvt_I57_0->a(keyx[0]);
    sc_lvt_I57_0->z(keyxz[0]);
    sc_lvt_I57_0->gd(gd);
    sc_lvt_I57_0->vp(vp);
    sc_lvt_INV0.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("sc_lvt_INV0"));
    sc_lvt_INV0->a(on);
    sc_lvt_INV0->z(onz);
    sc_lvt_INV0->gd(gd);
    sc_lvt_INV0->vp(vp);
    sc_lvt_INV1.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("sc_lvt_INV1"));
    sc_lvt_INV1->a(key_b[0]);
    sc_lvt_INV1->z(keyz_b[0]);
    sc_lvt_INV1->gd(gd);
    sc_lvt_INV1->vp(vp);
    sc_lvt_INV4_5.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("sc_lvt_INV4_5"));
    sc_lvt_INV4_5->a(rx_hs_bcal_b[5]);
    sc_lvt_INV4_5->z(rx_hs_bcalz_b[5]);
    sc_lvt_INV4_5->gd(gd);
    sc_lvt_INV4_5->vp(vp);
    sc_lvt_INV4_4.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("sc_lvt_INV4_4"));
    sc_lvt_INV4_4->a(rx_hs_bcal_b[4]);
    sc_lvt_INV4_4->z(rx_hs_bcalz_b[4]);
    sc_lvt_INV4_4->gd(gd);
    sc_lvt_INV4_4->vp(vp);
    sc_lvt_INV4_3.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("sc_lvt_INV4_3"));
    sc_lvt_INV4_3->a(rx_hs_bcal_b[3]);
    sc_lvt_INV4_3->z(rx_hs_bcalz_b[3]);
    sc_lvt_INV4_3->gd(gd);
    sc_lvt_INV4_3->vp(vp);
    sc_lvt_INV4_2.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("sc_lvt_INV4_2"));
    sc_lvt_INV4_2->a(rx_hs_bcal_b[2]);
    sc_lvt_INV4_2->z(rx_hs_bcalz_b[2]);
    sc_lvt_INV4_2->gd(gd);
    sc_lvt_INV4_2->vp(vp);
    sc_lvt_INV4_1.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("sc_lvt_INV4_1"));
    sc_lvt_INV4_1->a(rx_hs_bcal_b[1]);
    sc_lvt_INV4_1->z(rx_hs_bcalz_b[1]);
    sc_lvt_INV4_1->gd(gd);
    sc_lvt_INV4_1->vp(vp);
    sc_lvt_INV4_0.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("sc_lvt_INV4_0"));
    sc_lvt_INV4_0->a(rx_hs_bcal_b[0]);
    sc_lvt_INV4_0->z(rx_hs_bcalz_b[0]);
    sc_lvt_INV4_0->gd(gd);
    sc_lvt_INV4_0->vp(vp);
    sc_lvt_INV5.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("sc_lvt_INV5"));
    sc_lvt_INV5->a(en_lb);
    sc_lvt_INV5->z(en_lbz);
    sc_lvt_INV5->gd(gd);
    sc_lvt_INV5->vp(vp);
    sc_lvt_I32.reset(new mipi_4_rx_dphy_ns_sc_no2x6_lvt("sc_lvt_I32"));
    sc_lvt_I32->a(gear_b[1]);
    sc_lvt_I32->b(gear_b[0]);
    sc_lvt_I32->z(keyx[0]);
    sc_lvt_I32->gd(gd);
    sc_lvt_I32->vp(vp);
    sctg_I29.reset(new mipi_4_rx_dphy_ns_sctg_nor1x("sctg_I29"));
    sctg_I29->a0(net67);
    sctg_I29->a1(rx_eq_bcal_h_b[2]);
    sctg_I29->z(rx_eq_bcal_bypass_h);
    sctg_I29->gd(gd);
    sctg_I29->vph(vph);
    sctg_NORH1.reset(new mipi_4_rx_dphy_ns_sctg_nor1x("sctg_NORH1"));
    sctg_NORH1->a0(rx_eq_bcal_h_b[0]);
    sctg_NORH1->a1(rx_eq_bcal_h_b[1]);
    sctg_NORH1->z(net64);
    sctg_NORH1->gd(gd);
    sctg_NORH1->vph(vph);
    sctg_I40.reset(new mipi_4_rx_dphy_ns_sctg_nand4x("sctg_I40"));
    sctg_I40->a0(gearhv_b[1]);
    sctg_I40->a1(gearhv_b[0]);
    sctg_I40->z(keyxhv[2]);
    sctg_I40->gd(gd);
    sctg_I40->vph(vph);
    sctg_I44_2.reset(new mipi_4_rx_dphy_ns_sctg_nand4x("sctg_I44_2"));
    sctg_I44_2->a0(keyxhvz[2]);
    sctg_I44_2->a1(keyhvz_b[0]);
    sctg_I44_2->z(keyhv_b[3]);
    sctg_I44_2->gd(gd);
    sctg_I44_2->vph(vph);
    sctg_I44_1.reset(new mipi_4_rx_dphy_ns_sctg_nand4x("sctg_I44_1"));
    sctg_I44_1->a0(keyxhvz[1]);
    sctg_I44_1->a1(keyhvz_b[0]);
    sctg_I44_1->z(keyhv_b[2]);
    sctg_I44_1->gd(gd);
    sctg_I44_1->vph(vph);
    sctg_I44_0.reset(new mipi_4_rx_dphy_ns_sctg_nand4x("sctg_I44_0"));
    sctg_I44_0->a0(keyxhvz[0]);
    sctg_I44_0->a1(keyhvz_b[0]);
    sctg_I44_0->z(keyhv_b[1]);
    sctg_I44_0->gd(gd);
    sctg_I44_0->vph(vph);
    sctg_I59.reset(new mipi_4_rx_dphy_ns_sctg_nand4x("sctg_I59"));
    sctg_I59->a0(net155);
    sctg_I59->a1(rx_pre_bursthv);
    sctg_I59->z(net179);
    sctg_I59->gd(gd);
    sctg_I59->vph(vph);
    sctg_I65_3.reset(new mipi_4_rx_dphy_ns_sctg_nand4x("sctg_I65_3"));
    sctg_I65_3->a0(en_lb_h);
    sctg_I65_3->a1(lb_current_h_b[3]);
    sctg_I65_3->z(lbkeyhv_b[3]);
    sctg_I65_3->gd(gd);
    sctg_I65_3->vph(vph);
    sctg_I65_2.reset(new mipi_4_rx_dphy_ns_sctg_nand4x("sctg_I65_2"));
    sctg_I65_2->a0(en_lb_h);
    sctg_I65_2->a1(lb_current_h_b[2]);
    sctg_I65_2->z(lbkeyhv_b[2]);
    sctg_I65_2->gd(gd);
    sctg_I65_2->vph(vph);
    sctg_I65_1.reset(new mipi_4_rx_dphy_ns_sctg_nand4x("sctg_I65_1"));
    sctg_I65_1->a0(en_lb_h);
    sctg_I65_1->a1(lb_current_h_b[1]);
    sctg_I65_1->z(lbkeyhv_b[1]);
    sctg_I65_1->gd(gd);
    sctg_I65_1->vph(vph);
    sctg_I65_0.reset(new mipi_4_rx_dphy_ns_sctg_nand4x("sctg_I65_0"));
    sctg_I65_0->a0(en_lb_h);
    sctg_I65_0->a1(lb_current_h_b[0]);
    sctg_I65_0->z(lbkeyhv_b[0]);
    sctg_I65_0->gd(gd);
    sctg_I65_0->vph(vph);
    sctg_NANDH0.reset(new mipi_4_rx_dphy_ns_sctg_nand4x("sctg_NANDH0"));
    sctg_NANDH0->a0(onhv);
    sctg_NANDH0->a1(rx_pre_bursthv);
    sctg_NANDH0->z(keyhv_b[0]);
    sctg_NANDH0->gd(gd);
    sctg_NANDH0->vph(vph);
    sctg_I41.reset(new mipi_4_rx_dphy_ns_sctg_nor4x("sctg_I41"));
    sctg_I41->a0(gearhv_b[1]);
    sctg_I41->a1(gearhv_b[0]);
    sctg_I41->z(keyxhv[0]);
    sctg_I41->gd(gd);
    sctg_I41->vph(vph);
    sctg_NORH0.reset(new mipi_4_rx_dphy_ns_sctg_nor4x("sctg_NORH0"));
    sctg_NORH0->a0(rx_hs_encal_h);
    sctg_NORH0->a1(en_lb_h);
    sctg_NORH0->z(net155);
    sctg_NORH0->gd(gd);
    sctg_NORH0->vph(vph);
    scyg_I64.reset(new mipi_4_rx_dphy_ns_sctg_nor4x("scyg_I64"));
    scyg_I64->a0(calswn_h);
    scyg_I64->a1(en_lb_h);
    scyg_I64->z(calsw_h);
    scyg_I64->gd(gd);
    scyg_I64->vph(vph);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::split_atb_sw_hv() {
    const vl::logic<2> v = vl::rd(atb_sw_hv);
    atb_sw_hv_b[0].write(v.bit(0).sl());
    atb_sw_hv_b[1].write(v.bit(1).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::split_gear() {
    const vl::logic<2> v = vl::rd(gear);
    gear_b[0].write(v.bit(0).sl());
    gear_b[1].write(v.bit(1).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::split_gearhv() {
    const vl::logic<2> v = vl::rd(gearhv);
    gearhv_b[0].write(v.bit(0).sl());
    gearhv_b[1].write(v.bit(1).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::split_lb_current_h() {
    const vl::logic<4> v = vl::rd(lb_current_h);
    lb_current_h_b[0].write(v.bit(0).sl());
    lb_current_h_b[1].write(v.bit(1).sl());
    lb_current_h_b[2].write(v.bit(2).sl());
    lb_current_h_b[3].write(v.bit(3).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::split_rx_eq_bcal_h() {
    const vl::logic<3> v = vl::rd(rx_eq_bcal_h);
    rx_eq_bcal_h_b[0].write(v.bit(0).sl());
    rx_eq_bcal_h_b[1].write(v.bit(1).sl());
    rx_eq_bcal_h_b[2].write(v.bit(2).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::split_rx_hs_bcal() {
    const vl::logic<6> v = vl::rd(rx_hs_bcal);
    rx_hs_bcal_b[0].write(v.bit(0).sl());
    rx_hs_bcal_b[1].write(v.bit(1).sl());
    rx_hs_bcal_b[2].write(v.bit(2).sl());
    rx_hs_bcal_b[3].write(v.bit(3).sl());
    rx_hs_bcal_b[4].write(v.bit(4).sl());
    rx_hs_bcal_b[5].write(v.bit(5).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::merge_atb_sw_hvz() {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(atb_sw_hvz_b[0]));
    v.set_bit(1, vl::rd(atb_sw_hvz_b[1]));
    vl::wr(atb_sw_hvz, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::merge_key() {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(key_b[0]));
    v.set_bit(1, vl::rd(key_b[1]));
    v.set_bit(2, vl::rd(key_b[2]));
    v.set_bit(3, vl::rd(key_b[3]));
    vl::wr(key, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::merge_keyhv() {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(keyhv_b[0]));
    v.set_bit(1, vl::rd(keyhv_b[1]));
    v.set_bit(2, vl::rd(keyhv_b[2]));
    v.set_bit(3, vl::rd(keyhv_b[3]));
    vl::wr(keyhv, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::merge_keyhvz() {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(keyhvz_b[0]));
    v.set_bit(1, vl::rd(keyhvz_b[1]));
    v.set_bit(2, vl::rd(keyhvz_b[2]));
    v.set_bit(3, vl::rd(keyhvz_b[3]));
    vl::wr(keyhvz, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::merge_keyz() {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(keyz_b[0]));
    v.set_bit(1, vl::rd(keyz_b[1]));
    v.set_bit(2, vl::rd(keyz_b[2]));
    v.set_bit(3, vl::rd(keyz_b[3]));
    vl::wr(keyz, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::merge_lbkeyhv() {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(lbkeyhv_b[0]));
    v.set_bit(1, vl::rd(lbkeyhv_b[1]));
    v.set_bit(2, vl::rd(lbkeyhv_b[2]));
    v.set_bit(3, vl::rd(lbkeyhv_b[3]));
    vl::wr(lbkeyhv, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::merge_rx_eq_bcal_hz() {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(rx_eq_bcal_hz_b[0]));
    v.set_bit(1, vl::rd(rx_eq_bcal_hz_b[1]));
    v.set_bit(2, vl::rd(rx_eq_bcal_hz_b[2]));
    vl::wr(rx_eq_bcal_hz, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::merge_rx_hs_bcalz() {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd(rx_hs_bcalz_b[0]));
    v.set_bit(1, vl::rd(rx_hs_bcalz_b[1]));
    v.set_bit(2, vl::rd(rx_hs_bcalz_b[2]));
    v.set_bit(3, vl::rd(rx_hs_bcalz_b[3]));
    v.set_bit(4, vl::rd(rx_hs_bcalz_b[4]));
    v.set_bit(5, vl::rd(rx_hs_bcalz_b[5]));
    vl::wr(rx_hs_bcalz, v);
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::rd_atb_sw_hv() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(atb_sw_hv_b[0]));
    v.set_bit(1, vl::rd(atb_sw_hv_b[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::rd_atb_sw_hvz() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(atb_sw_hvz_b[0]));
    v.set_bit(1, vl::rd(atb_sw_hvz_b[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::rd_gear() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(gear_b[0]));
    v.set_bit(1, vl::rd(gear_b[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::rd_gearhv() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(gearhv_b[0]));
    v.set_bit(1, vl::rd(gearhv_b[1]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::rd_key() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(key_b[0]));
    v.set_bit(1, vl::rd(key_b[1]));
    v.set_bit(2, vl::rd(key_b[2]));
    v.set_bit(3, vl::rd(key_b[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::rd_keyhv() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(keyhv_b[0]));
    v.set_bit(1, vl::rd(keyhv_b[1]));
    v.set_bit(2, vl::rd(keyhv_b[2]));
    v.set_bit(3, vl::rd(keyhv_b[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::rd_keyhvz() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(keyhvz_b[0]));
    v.set_bit(1, vl::rd(keyhvz_b[1]));
    v.set_bit(2, vl::rd(keyhvz_b[2]));
    v.set_bit(3, vl::rd(keyhvz_b[3]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::rd_keyx() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(keyx[0]));
    v.set_bit(1, vl::rd(keyx[1]));
    v.set_bit(2, vl::rd(keyx[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::rd_keyxhv() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(keyxhv[0]));
    v.set_bit(1, vl::rd(keyxhv[1]));
    v.set_bit(2, vl::rd(keyxhv[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::rd_keyxhvz() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(keyxhvz[0]));
    v.set_bit(1, vl::rd(keyxhvz[1]));
    v.set_bit(2, vl::rd(keyxhvz[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::rd_keyxz() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(keyxz[0]));
    v.set_bit(1, vl::rd(keyxz[1]));
    v.set_bit(2, vl::rd(keyxz[2]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::rd_keyz() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(keyz_b[0]));
    v.set_bit(1, vl::rd(keyz_b[1]));
    v.set_bit(2, vl::rd(keyz_b[2]));
    v.set_bit(3, vl::rd(keyz_b[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::rd_lb_current_h() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(lb_current_h_b[0]));
    v.set_bit(1, vl::rd(lb_current_h_b[1]));
    v.set_bit(2, vl::rd(lb_current_h_b[2]));
    v.set_bit(3, vl::rd(lb_current_h_b[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::rd_lbkeyhv() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(lbkeyhv_b[0]));
    v.set_bit(1, vl::rd(lbkeyhv_b[1]));
    v.set_bit(2, vl::rd(lbkeyhv_b[2]));
    v.set_bit(3, vl::rd(lbkeyhv_b[3]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::rd_rx_eq_bcal_h() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(rx_eq_bcal_h_b[0]));
    v.set_bit(1, vl::rd(rx_eq_bcal_h_b[1]));
    v.set_bit(2, vl::rd(rx_eq_bcal_h_b[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::rd_rx_eq_bcal_hz() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(rx_eq_bcal_hz_b[0]));
    v.set_bit(1, vl::rd(rx_eq_bcal_hz_b[1]));
    v.set_bit(2, vl::rd(rx_eq_bcal_hz_b[2]));
    return v;
}

vl::logic<6> mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::rd_rx_hs_bcal() const {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd(rx_hs_bcal_b[0]));
    v.set_bit(1, vl::rd(rx_hs_bcal_b[1]));
    v.set_bit(2, vl::rd(rx_hs_bcal_b[2]));
    v.set_bit(3, vl::rd(rx_hs_bcal_b[3]));
    v.set_bit(4, vl::rd(rx_hs_bcal_b[4]));
    v.set_bit(5, vl::rd(rx_hs_bcal_b[5]));
    return v;
}

vl::logic<6> mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic::rd_rx_hs_bcalz() const {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd(rx_hs_bcalz_b[0]));
    v.set_bit(1, vl::rd(rx_hs_bcalz_b[1]));
    v.set_bit(2, vl::rd(rx_hs_bcalz_b[2]));
    v.set_bit(3, vl::rd(rx_hs_bcalz_b[3]));
    v.set_bit(4, vl::rd(rx_hs_bcalz_b[4]));
    v.set_bit(5, vl::rd(rx_hs_bcalz_b[5]));
    return v;
}
