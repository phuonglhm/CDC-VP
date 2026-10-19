//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYLPTX_log
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYLPTX_log.h"

mipi_4_rx_dphy_ns_DPHYLPTX_log::mipi_4_rx_dphy_ns_DPHYLPTX_log(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_proghv);
    sensitive << proghv;
    SC_METHOD(split_setrhv);
    sensitive << setrhv;
    SC_METHOD(merge_proghvz);
    sensitive << proghvz_b[0] << proghvz_b[1] << proghvz_b[2];
    SC_METHOD(merge_setrz);
    sensitive << setrz_b[0] << setrz_b[1] << setrz_b[2];
    I261.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_log_inv0("I261"));
    I261->gd(gd);
    I261->a(onb);
    I261->z(onbz);
    I261->vph(vph);
    I265.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_log_inv0("I265"));
    I265->gd(gd);
    I265->a(enablez);
    I265->z(enable);
    I265->vph(vph);
    inv_2.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_log_inv0("inv_2"));
    inv_2->gd(gd);
    inv_2->a(proghv_b[2]);
    inv_2->z(proghvz_b[2]);
    inv_2->vph(vph);
    inv_1.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_log_inv0("inv_1"));
    inv_1->gd(gd);
    inv_1->a(proghv_b[1]);
    inv_1->z(proghvz_b[1]);
    inv_1->vph(vph);
    inv_0.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_log_inv0("inv_0"));
    inv_0->gd(gd);
    inv_0->a(proghv_b[0]);
    inv_0->z(proghvz_b[0]);
    inv_0->vph(vph);
    setra_2.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_log_inv0("setra_2"));
    setra_2->gd(gd);
    setra_2->a(setrhv_b[2]);
    setra_2->z(setrz_b[2]);
    setra_2->vph(vph);
    setra_1.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_log_inv0("setra_1"));
    setra_1->gd(gd);
    setra_1->a(setrhv_b[1]);
    setra_1->z(setrz_b[1]);
    setra_1->vph(vph);
    setra_0.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_log_inv0("setra_0"));
    setra_0->gd(gd);
    setra_0->a(setrhv_b[0]);
    setra_0->z(setrz_b[0]);
    setra_0->vph(vph);
    I249.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_log_nand2("I249"));
    I249->A(pon);
    I249->B(en);
    I249->Y(enablez);
    I249->gd(gd);
    I249->vph(vph);
    I250.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core_log_nor2("I250"));
    I250->A(enablez);
    I250->B(enulp);
    I250->Y(onb);
    I250->gd(gd);
    I250->vph(vph);
}

void mipi_4_rx_dphy_ns_DPHYLPTX_log::split_proghv() {
    const vl::logic<3> v = vl::rd(proghv);
    proghv_b[0].write(v.bit(0).sl());
    proghv_b[1].write(v.bit(1).sl());
    proghv_b[2].write(v.bit(2).sl());
}

void mipi_4_rx_dphy_ns_DPHYLPTX_log::split_setrhv() {
    const vl::logic<3> v = vl::rd(setrhv);
    setrhv_b[0].write(v.bit(0).sl());
    setrhv_b[1].write(v.bit(1).sl());
    setrhv_b[2].write(v.bit(2).sl());
}

void mipi_4_rx_dphy_ns_DPHYLPTX_log::merge_proghvz() {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(proghvz_b[0]));
    v.set_bit(1, vl::rd(proghvz_b[1]));
    v.set_bit(2, vl::rd(proghvz_b[2]));
    vl::wr(proghvz, v);
}

void mipi_4_rx_dphy_ns_DPHYLPTX_log::merge_setrz() {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(setrz_b[0]));
    v.set_bit(1, vl::rd(setrz_b[1]));
    v.set_bit(2, vl::rd(setrz_b[2]));
    vl::wr(setrz, v);
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYLPTX_log::rd_proghv() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(proghv_b[0]));
    v.set_bit(1, vl::rd(proghv_b[1]));
    v.set_bit(2, vl::rd(proghv_b[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYLPTX_log::rd_proghvz() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(proghvz_b[0]));
    v.set_bit(1, vl::rd(proghvz_b[1]));
    v.set_bit(2, vl::rd(proghvz_b[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYLPTX_log::rd_setrhv() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(setrhv_b[0]));
    v.set_bit(1, vl::rd(setrhv_b[1]));
    v.set_bit(2, vl::rd(setrhv_b[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYLPTX_log::rd_setrz() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(setrz_b[0]));
    v.set_bit(1, vl::rd(setrz_b[1]));
    v.set_bit(2, vl::rd(setrz_b[2]));
    return v;
}
