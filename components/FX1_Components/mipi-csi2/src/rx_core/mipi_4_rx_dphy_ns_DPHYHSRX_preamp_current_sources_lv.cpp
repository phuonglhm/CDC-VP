//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_preamp_current_sources_lv
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_preamp_current_sources_lv.h"

mipi_4_rx_dphy_ns_DPHYHSRX_preamp_current_sources_lv::mipi_4_rx_dphy_ns_DPHYHSRX_preamp_current_sources_lv(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_key);
    sensitive << key;
    SC_METHOD(gate_ilv_cgm_int_buf0);
    sensitive << vp << on << onz;
    SC_METHOD(gate_ilv_cgm_int_buf1);
    sensitive << vp << on << onz;
    SC_METHOD(gate_ilv_cc_int_buf0);
    sensitive << vp << on << onz;
    SC_METHOD(gate_ilv_cc_int_buf1);
    sensitive << vp << on << onz;
    I0_2.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source("I0_2"));
    I0_2->bias(ilv_cc_int);
    I0_2->onz(key_b[0]);
    I0_2->o(ibias2);
    I0_2->vp(vp);
    I1_2.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source("I1_2"));
    I1_2->bias(ilv_cc_int);
    I1_2->onz(key_b[1]);
    I1_2->o(ibias2);
    I1_2->vp(vp);
    I2_2.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source("I2_2"));
    I2_2->bias(ilv_cc_int);
    I2_2->onz(key_b[2]);
    I2_2->o(ibias2);
    I2_2->vp(vp);
    I3_2_1.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source("I3_2_1"));
    I3_2_1->bias(ilv_cc_int);
    I3_2_1->onz(key_b[3]);
    I3_2_1->o(ibias2);
    I3_2_1->vp(vp);
    I3_2_0.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source("I3_2_0"));
    I3_2_0->bias(ilv_cc_int);
    I3_2_0->onz(key_b[3]);
    I3_2_0->o(ibias2);
    I3_2_0->vp(vp);
    I28_1.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source("I28_1"));
    I28_1->bias(ilv_cgm_int);
    I28_1->onz(key_b[3]);
    I28_1->o(ibias1);
    I28_1->vp(vp);
    I28_0.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source("I28_0"));
    I28_0->bias(ilv_cgm_int);
    I28_0->onz(key_b[3]);
    I28_0->o(ibias1);
    I28_0->vp(vp);
    I29.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source("I29"));
    I29->bias(ilv_cgm_int);
    I29->onz(key_b[1]);
    I29->o(ibias1);
    I29->vp(vp);
    I30.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source("I30"));
    I30->bias(ilv_cgm_int);
    I30->onz(key_b[2]);
    I30->o(ibias1);
    I30->vp(vp);
    I32.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source("I32"));
    I32->bias(ilv_cgm_int);
    I32->onz(key_b[0]);
    I32->o(ibias1);
    I32->vp(vp);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_current_sources_lv::split_key() {
    const vl::logic<4> v = vl::rd(key);
    key_b[0].write(v.bit(0).sl());
    key_b[1].write(v.bit(1).sl());
    key_b[2].write(v.bit(2).sl());
    key_b[3].write(v.bit(3).sl());
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_preamp_current_sources_lv::rd_key() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(key_b[0]));
    v.set_bit(1, vl::rd(key_b[1]));
    v.set_bit(2, vl::rd(key_b[2]));
    v.set_bit(3, vl::rd(key_b[3]));
    return v;
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_current_sources_lv::gate_ilv_cgm_int_buf0() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x1ULL), vl::land(vl::land(vl::eq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(on), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(onz), vl::logic<1>(0x0ULL))));
    vl::wr(ilv_cgm_int, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_current_sources_lv::gate_ilv_cgm_int_buf1() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x0ULL), vl::land(vl::land(vl::eq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(on), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(onz), vl::logic<1>(0x1ULL))));
    vl::wr(ilv_cgm_int, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_current_sources_lv::gate_ilv_cc_int_buf0() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x1ULL), vl::land(vl::land(vl::eq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(on), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(onz), vl::logic<1>(0x0ULL))));
    vl::wr(ilv_cc_int, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_current_sources_lv::gate_ilv_cc_int_buf1() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x0ULL), vl::land(vl::land(vl::eq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(on), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(onz), vl::logic<1>(0x1ULL))));
    vl::wr(ilv_cc_int, v);
}
