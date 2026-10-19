//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_des_des4
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_des_des4.h"

mipi_4_rx_dphy_ns_DPHYHSRX_des_des4::mipi_4_rx_dphy_ns_DPHYHSRX_des_des4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_div5);
    sensitive << div5;
    SC_METHOD(merge_rx_data);
    sensitive << rx_data_b[0] << rx_data_b[1] << rx_data_b[2] << rx_data_b[3];
    INV_3.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("INV_3"));
    INV_3->a(div5_b[3]);
    INV_3->z(div5_neg[3]);
    INV_3->gd(gd);
    INV_3->vp(vp);
    INV_2.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("INV_2"));
    INV_2->a(div5_b[2]);
    INV_2->z(div5_neg[2]);
    INV_2->gd(gd);
    INV_2->vp(vp);
    INV_1.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("INV_1"));
    INV_1->a(div5_b[1]);
    INV_1->z(div5_neg[1]);
    INV_1->gd(gd);
    INV_1->vp(vp);
    INV_0.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("INV_0"));
    INV_0->a(div5_b[0]);
    INV_0->z(div5_neg[0]);
    INV_0->gd(gd);
    INV_0->vp(vp);
    I13.reset(new mipi_4_rx_dphy_ns_sc_dlax39_ar_lvt("I13"));
    I13->ck_m(div5_neg[2]);
    I13->ck_p(div5_b[2]);
    I13->d(data);
    I13->pwr_on(tie_high);
    I13->qb(rx_data_b[2]);
    I13->gd(gd);
    I13->vp(vp);
    I18.reset(new mipi_4_rx_dphy_ns_sc_dlax39_ar_lvt("I18"));
    I18->ck_m(div5_neg[1]);
    I18->ck_p(div5_b[1]);
    I18->d(data);
    I18->pwr_on(tie_high);
    I18->qb(rx_data_b[1]);
    I18->gd(gd);
    I18->vp(vp);
    I21.reset(new mipi_4_rx_dphy_ns_sc_dlax39_ar_lvt("I21"));
    I21->ck_m(div5_neg[0]);
    I21->ck_p(div5_b[0]);
    I21->d(data);
    I21->pwr_on(tie_high);
    I21->qb(rx_data_b[0]);
    I21->gd(gd);
    I21->vp(vp);
    I22.reset(new mipi_4_rx_dphy_ns_sc_dlax39_ar_lvt("I22"));
    I22->ck_m(div5_b[3]);
    I22->ck_p(div5_neg[3]);
    I22->d(rx_data_del);
    I22->pwr_on(tie_high);
    I22->qb(rx_data_b[3]);
    I22->gd(gd);
    I22->vp(vp);
    la0.reset(new mipi_4_rx_dphy_ns_sc_dlax39_ar_lvt("la0"));
    la0->ck_m(div5_neg[3]);
    la0->ck_p(div5_b[3]);
    la0->d(data);
    la0->pwr_on(tie_high);
    la0->qb(rx_data_del);
    la0->gd(gd);
    la0->vp(vp);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_des_des4::split_div5() {
    const vl::logic<4> v = vl::rd(div5);
    div5_b[0].write(v.bit(0).sl());
    div5_b[1].write(v.bit(1).sl());
    div5_b[2].write(v.bit(2).sl());
    div5_b[3].write(v.bit(3).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_des_des4::merge_rx_data() {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(rx_data_b[0]));
    v.set_bit(1, vl::rd(rx_data_b[1]));
    v.set_bit(2, vl::rd(rx_data_b[2]));
    v.set_bit(3, vl::rd(rx_data_b[3]));
    vl::wr(rx_data, v);
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_des_des4::rd_div5() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(div5_b[0]));
    v.set_bit(1, vl::rd(div5_b[1]));
    v.set_bit(2, vl::rd(div5_b[2]));
    v.set_bit(3, vl::rd(div5_b[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_des_des4::rd_div5_neg() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(div5_neg[0]));
    v.set_bit(1, vl::rd(div5_neg[1]));
    v.set_bit(2, vl::rd(div5_neg[2]));
    v.set_bit(3, vl::rd(div5_neg[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_des_des4::rd_rx_data() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(rx_data_b[0]));
    v.set_bit(1, vl::rd(rx_data_b[1]));
    v.set_bit(2, vl::rd(rx_data_b[2]));
    v.set_bit(3, vl::rd(rx_data_b[3]));
    return v;
}
