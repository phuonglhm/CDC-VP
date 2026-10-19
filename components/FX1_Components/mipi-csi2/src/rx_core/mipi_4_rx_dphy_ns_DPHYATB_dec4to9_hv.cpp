//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYATB_dec4to9_hv
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYATB_dec4to9_hv.h"

mipi_4_rx_dphy_ns_DPHYATB_dec4to9_hv::mipi_4_rx_dphy_ns_DPHYATB_dec4to9_hv(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_A_in);
    sensitive << A_in;
    SC_METHOD(split_A_inz);
    sensitive << A_inz;
    SC_METHOD(merge_Z_out);
    sensitive << Z_out_b[0] << Z_out_b[1] << Z_out_b[2] << Z_out_b[3] << Z_out_b[4] << Z_out_b[5] << Z_out_b[6] << Z_out_b[7] << Z_out_b[8];
    nor0.reset(new mipi_4_rx_dphy_ns_DPHY3AFE_nor4hv("nor0"));
    nor0->A(A_in_b[0]);
    nor0->B(A_in_b[1]);
    nor0->C(A_in_b[2]);
    nor0->D(A_in_b[3]);
    nor0->Z(Z_out_b[0]);
    nor0->agnd(gd);
    nor0->avdd(vph);
    nor1.reset(new mipi_4_rx_dphy_ns_DPHY3AFE_nor4hv("nor1"));
    nor1->A(A_inz_b[0]);
    nor1->B(A_in_b[1]);
    nor1->C(A_in_b[2]);
    nor1->D(A_in_b[3]);
    nor1->Z(Z_out_b[1]);
    nor1->agnd(gd);
    nor1->avdd(vph);
    nor2.reset(new mipi_4_rx_dphy_ns_DPHY3AFE_nor4hv("nor2"));
    nor2->A(A_in_b[0]);
    nor2->B(A_inz_b[1]);
    nor2->C(A_in_b[2]);
    nor2->D(A_in_b[3]);
    nor2->Z(Z_out_b[2]);
    nor2->agnd(gd);
    nor2->avdd(vph);
    nor3.reset(new mipi_4_rx_dphy_ns_DPHY3AFE_nor4hv("nor3"));
    nor3->A(A_inz_b[0]);
    nor3->B(A_inz_b[1]);
    nor3->C(A_in_b[2]);
    nor3->D(A_in_b[3]);
    nor3->Z(Z_out_b[3]);
    nor3->agnd(gd);
    nor3->avdd(vph);
    nor4.reset(new mipi_4_rx_dphy_ns_DPHY3AFE_nor4hv("nor4"));
    nor4->A(A_in_b[0]);
    nor4->B(A_in_b[1]);
    nor4->C(A_inz_b[2]);
    nor4->D(A_in_b[3]);
    nor4->Z(Z_out_b[4]);
    nor4->agnd(gd);
    nor4->avdd(vph);
    nor5.reset(new mipi_4_rx_dphy_ns_DPHY3AFE_nor4hv("nor5"));
    nor5->A(A_inz_b[0]);
    nor5->B(A_in_b[1]);
    nor5->C(A_inz_b[2]);
    nor5->D(A_in_b[3]);
    nor5->Z(Z_out_b[5]);
    nor5->agnd(gd);
    nor5->avdd(vph);
    nor6.reset(new mipi_4_rx_dphy_ns_DPHY3AFE_nor4hv("nor6"));
    nor6->A(A_in_b[0]);
    nor6->B(A_inz_b[1]);
    nor6->C(A_inz_b[2]);
    nor6->D(A_in_b[3]);
    nor6->Z(Z_out_b[6]);
    nor6->agnd(gd);
    nor6->avdd(vph);
    nor7.reset(new mipi_4_rx_dphy_ns_DPHY3AFE_nor4hv("nor7"));
    nor7->A(A_inz_b[0]);
    nor7->B(A_inz_b[1]);
    nor7->C(A_inz_b[2]);
    nor7->D(A_in_b[3]);
    nor7->Z(Z_out_b[7]);
    nor7->agnd(gd);
    nor7->avdd(vph);
    nor8.reset(new mipi_4_rx_dphy_ns_DPHY3AFE_nor4hv("nor8"));
    nor8->A(A_in_b[0]);
    nor8->B(A_in_b[1]);
    nor8->C(A_in_b[2]);
    nor8->D(A_inz_b[3]);
    nor8->Z(Z_out_b[8]);
    nor8->agnd(gd);
    nor8->avdd(vph);
}

void mipi_4_rx_dphy_ns_DPHYATB_dec4to9_hv::split_A_in() {
    const vl::logic<4> v = vl::rd(A_in);
    A_in_b[0].write(v.bit(0).sl());
    A_in_b[1].write(v.bit(1).sl());
    A_in_b[2].write(v.bit(2).sl());
    A_in_b[3].write(v.bit(3).sl());
}

void mipi_4_rx_dphy_ns_DPHYATB_dec4to9_hv::split_A_inz() {
    const vl::logic<4> v = vl::rd(A_inz);
    A_inz_b[0].write(v.bit(0).sl());
    A_inz_b[1].write(v.bit(1).sl());
    A_inz_b[2].write(v.bit(2).sl());
    A_inz_b[3].write(v.bit(3).sl());
}

void mipi_4_rx_dphy_ns_DPHYATB_dec4to9_hv::merge_Z_out() {
    vl::logic<9> v(0);
    v.set_bit(0, vl::rd(Z_out_b[0]));
    v.set_bit(1, vl::rd(Z_out_b[1]));
    v.set_bit(2, vl::rd(Z_out_b[2]));
    v.set_bit(3, vl::rd(Z_out_b[3]));
    v.set_bit(4, vl::rd(Z_out_b[4]));
    v.set_bit(5, vl::rd(Z_out_b[5]));
    v.set_bit(6, vl::rd(Z_out_b[6]));
    v.set_bit(7, vl::rd(Z_out_b[7]));
    v.set_bit(8, vl::rd(Z_out_b[8]));
    vl::wr(Z_out, v);
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYATB_dec4to9_hv::rd_A_in() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(A_in_b[0]));
    v.set_bit(1, vl::rd(A_in_b[1]));
    v.set_bit(2, vl::rd(A_in_b[2]));
    v.set_bit(3, vl::rd(A_in_b[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYATB_dec4to9_hv::rd_A_inz() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(A_inz_b[0]));
    v.set_bit(1, vl::rd(A_inz_b[1]));
    v.set_bit(2, vl::rd(A_inz_b[2]));
    v.set_bit(3, vl::rd(A_inz_b[3]));
    return v;
}

vl::logic<9> mipi_4_rx_dphy_ns_DPHYATB_dec4to9_hv::rd_Z_out() const {
    vl::logic<9> v(0);
    v.set_bit(0, vl::rd(Z_out_b[0]));
    v.set_bit(1, vl::rd(Z_out_b[1]));
    v.set_bit(2, vl::rd(Z_out_b[2]));
    v.set_bit(3, vl::rd(Z_out_b[3]));
    v.set_bit(4, vl::rd(Z_out_b[4]));
    v.set_bit(5, vl::rd(Z_out_b[5]));
    v.set_bit(6, vl::rd(Z_out_b[6]));
    v.set_bit(7, vl::rd(Z_out_b[7]));
    v.set_bit(8, vl::rd(Z_out_b[8]));
    return v;
}
