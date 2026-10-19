//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit.h"

mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit::mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_phase);
    sensitive << phase;
    SC_METHOD(merge_y);
    sensitive << y_b[0] << y_b[1] << y_b[2] << y_b[3];
    SC_METHOD(merge_yz);
    sensitive << yz_b[0] << yz_b[1] << yz_b[2] << yz_b[3];
    I8_1.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("I8_1"));
    I8_1->a(phasez[1]);
    I8_1->z(phase_in[1]);
    I8_1->gd(gd);
    I8_1->vp(vp);
    I8_0.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("I8_0"));
    I8_0->a(phasez[0]);
    I8_0->z(phase_in[0]);
    I8_0->gd(gd);
    I8_0->vp(vp);
    I24_1.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("I24_1"));
    I24_1->a(phase_b[1]);
    I24_1->z(phasez[1]);
    I24_1->gd(gd);
    I24_1->vp(vp);
    I24_0.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("I24_0"));
    I24_0->a(phase_b[0]);
    I24_0->z(phasez[0]);
    I24_0->gd(gd);
    I24_0->vp(vp);
    I27_3.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("I27_3"));
    I27_3->a(x[3]);
    I27_3->z(xz[3]);
    I27_3->gd(gd);
    I27_3->vp(vp);
    I27_2.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("I27_2"));
    I27_2->a(x[2]);
    I27_2->z(xz[2]);
    I27_2->gd(gd);
    I27_2->vp(vp);
    I27_1.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("I27_1"));
    I27_1->a(x[1]);
    I27_1->z(xz[1]);
    I27_1->gd(gd);
    I27_1->vp(vp);
    I27_0.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("I27_0"));
    I27_0->a(x[0]);
    I27_0->z(xz[0]);
    I27_0->gd(gd);
    I27_0->vp(vp);
    I28.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("I28"));
    I28->a(xz[0]);
    I28->z(z[0]);
    I28->gd(gd);
    I28->vp(vp);
    inv_pon.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("inv_pon"));
    inv_pon->a(pon);
    inv_pon->z(ponz);
    inv_pon->gd(gd);
    inv_pon->vp(vp);
    inv_tune_en.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("inv_tune_en"));
    inv_tune_en->a(tune_enz);
    inv_tune_en->z(tune_en_in);
    inv_tune_en->gd(gd);
    inv_tune_en->vp(vp);
    I10_3.reset(new mipi_4_rx_dphy_ns_sc_no2x6_lvt("I10_3"));
    I10_3->a(z[3]);
    I10_3->b(tune_en_in);
    I10_3->z(net143[3]);
    I10_3->gd(gd);
    I10_3->vp(vp);
    I10_2.reset(new mipi_4_rx_dphy_ns_sc_no2x6_lvt("I10_2"));
    I10_2->a(z[2]);
    I10_2->b(tune_en_in);
    I10_2->z(net143[2]);
    I10_2->gd(gd);
    I10_2->vp(vp);
    I10_1.reset(new mipi_4_rx_dphy_ns_sc_no2x6_lvt("I10_1"));
    I10_1->a(z[1]);
    I10_1->b(tune_en_in);
    I10_1->z(net143[1]);
    I10_1->gd(gd);
    I10_1->vp(vp);
    I10_0.reset(new mipi_4_rx_dphy_ns_sc_no2x6_lvt("I10_0"));
    I10_0->a(z[0]);
    I10_0->b(tune_en_in);
    I10_0->z(net143[0]);
    I10_0->gd(gd);
    I10_0->vp(vp);
    I25_3.reset(new mipi_4_rx_dphy_ns_sc_no2x6_lvt("I25_3"));
    I25_3->a(net143[3]);
    I25_3->b(ponz);
    I25_3->z(net149[3]);
    I25_3->gd(gd);
    I25_3->vp(vp);
    I25_2.reset(new mipi_4_rx_dphy_ns_sc_no2x6_lvt("I25_2"));
    I25_2->a(net143[2]);
    I25_2->b(ponz);
    I25_2->z(net149[2]);
    I25_2->gd(gd);
    I25_2->vp(vp);
    I25_1.reset(new mipi_4_rx_dphy_ns_sc_no2x6_lvt("I25_1"));
    I25_1->a(net143[1]);
    I25_1->b(ponz);
    I25_1->z(net149[1]);
    I25_1->gd(gd);
    I25_1->vp(vp);
    I25_0.reset(new mipi_4_rx_dphy_ns_sc_no2x6_lvt("I25_0"));
    I25_0->a(net143[0]);
    I25_0->b(ponz);
    I25_0->z(net149[0]);
    I25_0->gd(gd);
    I25_0->vp(vp);
    I26_2.reset(new mipi_4_rx_dphy_ns_sc_no2x6_lvt("I26_2"));
    I26_2->a(xz[3]);
    I26_2->b(xz[2]);
    I26_2->z(z[3]);
    I26_2->gd(gd);
    I26_2->vp(vp);
    I26_1.reset(new mipi_4_rx_dphy_ns_sc_no2x6_lvt("I26_1"));
    I26_1->a(xz[2]);
    I26_1->b(xz[1]);
    I26_1->z(z[2]);
    I26_1->gd(gd);
    I26_1->vp(vp);
    I26_0.reset(new mipi_4_rx_dphy_ns_sc_no2x6_lvt("I26_0"));
    I26_0->a(xz[1]);
    I26_0->b(xz[0]);
    I26_0->z(z[1]);
    I26_0->gd(gd);
    I26_0->vp(vp);
    I9_3.reset(new mipi_4_rx_dphy_ns_sc_invx32_lvt("I9_3"));
    I9_3->a(net152[3]);
    I9_3->z(y_b[3]);
    I9_3->gd(gd);
    I9_3->vp(vp);
    I9_2.reset(new mipi_4_rx_dphy_ns_sc_invx32_lvt("I9_2"));
    I9_2->a(net152[2]);
    I9_2->z(y_b[2]);
    I9_2->gd(gd);
    I9_2->vp(vp);
    I9_1.reset(new mipi_4_rx_dphy_ns_sc_invx32_lvt("I9_1"));
    I9_1->a(net152[1]);
    I9_1->z(y_b[1]);
    I9_1->gd(gd);
    I9_1->vp(vp);
    I9_0.reset(new mipi_4_rx_dphy_ns_sc_invx32_lvt("I9_0"));
    I9_0->a(net152[0]);
    I9_0->z(y_b[0]);
    I9_0->gd(gd);
    I9_0->vp(vp);
    I29_3.reset(new mipi_4_rx_dphy_ns_sc_invx32_lvt("I29_3"));
    I29_3->a(y_b[3]);
    I29_3->z(yz_b[3]);
    I29_3->gd(gd);
    I29_3->vp(vp);
    I29_2.reset(new mipi_4_rx_dphy_ns_sc_invx32_lvt("I29_2"));
    I29_2->a(y_b[2]);
    I29_2->z(yz_b[2]);
    I29_2->gd(gd);
    I29_2->vp(vp);
    I29_1.reset(new mipi_4_rx_dphy_ns_sc_invx32_lvt("I29_1"));
    I29_1->a(y_b[1]);
    I29_1->z(yz_b[1]);
    I29_1->gd(gd);
    I29_1->vp(vp);
    I29_0.reset(new mipi_4_rx_dphy_ns_sc_invx32_lvt("I29_0"));
    I29_0->a(y_b[0]);
    I29_0->z(yz_b[0]);
    I29_0->gd(gd);
    I29_0->vp(vp);
    I4.reset(new mipi_4_rx_dphy_ns_sc_na2x6_lvt("I4"));
    I4->a(phasez[0]);
    I4->b(phase_in[1]);
    I4->z(x[2]);
    I4->gd(gd);
    I4->vp(vp);
    I5.reset(new mipi_4_rx_dphy_ns_sc_na2x6_lvt("I5"));
    I5->a(phasez[0]);
    I5->b(phasez[1]);
    I5->z(x[0]);
    I5->gd(gd);
    I5->vp(vp);
    I6.reset(new mipi_4_rx_dphy_ns_sc_na2x6_lvt("I6"));
    I6->a(phase_in[0]);
    I6->b(phasez[1]);
    I6->z(x[1]);
    I6->gd(gd);
    I6->vp(vp);
    I7.reset(new mipi_4_rx_dphy_ns_sc_na2x6_lvt("I7"));
    I7->a(phase_in[0]);
    I7->b(phase_in[1]);
    I7->z(x[3]);
    I7->gd(gd);
    I7->vp(vp);
    nand_tune_en.reset(new mipi_4_rx_dphy_ns_sc_na2x6_lvt("nand_tune_en"));
    nand_tune_en->a(tune_en);
    nand_tune_en->b(pon);
    nand_tune_en->z(tune_enz);
    nand_tune_en->gd(gd);
    nand_tune_en->vp(vp);
    I30_3.reset(new mipi_4_rx_dphy_ns_sc_invx16_lvt("I30_3"));
    I30_3->a(net149[3]);
    I30_3->z(net152[3]);
    I30_3->gd(gd);
    I30_3->vp(vp);
    I30_2.reset(new mipi_4_rx_dphy_ns_sc_invx16_lvt("I30_2"));
    I30_2->a(net149[2]);
    I30_2->z(net152[2]);
    I30_2->gd(gd);
    I30_2->vp(vp);
    I30_1.reset(new mipi_4_rx_dphy_ns_sc_invx16_lvt("I30_1"));
    I30_1->a(net149[1]);
    I30_1->z(net152[1]);
    I30_1->gd(gd);
    I30_1->vp(vp);
    I30_0.reset(new mipi_4_rx_dphy_ns_sc_invx16_lvt("I30_0"));
    I30_0->a(net149[0]);
    I30_0->z(net152[0]);
    I30_0->gd(gd);
    I30_0->vp(vp);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit::split_phase() {
    const vl::logic<2> v = vl::rd(phase);
    phase_b[0].write(v.bit(0).sl());
    phase_b[1].write(v.bit(1).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit::merge_y() {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(y_b[0]));
    v.set_bit(1, vl::rd(y_b[1]));
    v.set_bit(2, vl::rd(y_b[2]));
    v.set_bit(3, vl::rd(y_b[3]));
    vl::wr(y, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit::merge_yz() {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(yz_b[0]));
    v.set_bit(1, vl::rd(yz_b[1]));
    v.set_bit(2, vl::rd(yz_b[2]));
    v.set_bit(3, vl::rd(yz_b[3]));
    vl::wr(yz, v);
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit::rd_net143() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(net143[0]));
    v.set_bit(1, vl::rd(net143[1]));
    v.set_bit(2, vl::rd(net143[2]));
    v.set_bit(3, vl::rd(net143[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit::rd_net149() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(net149[0]));
    v.set_bit(1, vl::rd(net149[1]));
    v.set_bit(2, vl::rd(net149[2]));
    v.set_bit(3, vl::rd(net149[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit::rd_net152() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(net152[0]));
    v.set_bit(1, vl::rd(net152[1]));
    v.set_bit(2, vl::rd(net152[2]));
    v.set_bit(3, vl::rd(net152[3]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit::rd_phase() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(phase_b[0]));
    v.set_bit(1, vl::rd(phase_b[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit::rd_phase_in() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(phase_in[0]));
    v.set_bit(1, vl::rd(phase_in[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit::rd_phasez() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(phasez[0]));
    v.set_bit(1, vl::rd(phasez[1]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit::rd_x() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(x[0]));
    v.set_bit(1, vl::rd(x[1]));
    v.set_bit(2, vl::rd(x[2]));
    v.set_bit(3, vl::rd(x[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit::rd_xz() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(xz[0]));
    v.set_bit(1, vl::rd(xz[1]));
    v.set_bit(2, vl::rd(xz[2]));
    v.set_bit(3, vl::rd(xz[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit::rd_y() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(y_b[0]));
    v.set_bit(1, vl::rd(y_b[1]));
    v.set_bit(2, vl::rd(y_b[2]));
    v.set_bit(3, vl::rd(y_b[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit::rd_yz() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(yz_b[0]));
    v.set_bit(1, vl::rd(yz_b[1]));
    v.set_bit(2, vl::rd(yz_b[2]));
    v.set_bit(3, vl::rd(yz_b[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit::rd_z() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(z[0]));
    v.set_bit(1, vl::rd(z[1]));
    v.set_bit(2, vl::rd(z[2]));
    v.set_bit(3, vl::rd(z[3]));
    return v;
}
