//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYCBRX_bias
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYCBRX_bias.h"

mipi_4_rx_dphy_ns_DPHYCBRX_bias::mipi_4_rx_dphy_ns_DPHYCBRX_bias(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vph << gd;
    SC_METHOD(assign_term_rep_ok);
    sensitive << cb_cal_en_hv << cb_cal_repl_hv;
    SC_METHOD(gate_cb_10u_ilptx_buf);
    sensitive << supply_ok << ota_on_hv << ota_on_hvz << term_rep_ok << vref;
    SC_METHOD(proc_always_152069);
    sensitive << supply_ok << ota_on_hv << ota_on_hvz << term_rep_ok << vref;
    SC_METHOD(proc_always_152078);
    sensitive << supply_ok << ota_on_hv << ota_on_hvz << term_rep_ok << vref;
    SC_METHOD(proc_always_152087);
    sensitive << supply_ok << ota_on_int_z << term_rep_ok << vref;
}

void mipi_4_rx_dphy_ns_DPHYCBRX_bias::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vph), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHYCBRX_bias::assign_term_rep_ok() {
    vl::wr(term_rep_ok, vl::land(vl::land(vl::ceq(vl::rd(cb_cal_en_hv), vl::logic<1>(0x1ULL)), vl::cne(vl::rd(cb_cal_repl_hv), vl::logic<4>::all_z())), vl::cne(vl::rd(cb_cal_repl_hv), vl::logic<4>())));
}

void mipi_4_rx_dphy_ns_DPHYCBRX_bias::gate_cb_10u_ilptx_buf() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x1ULL), vl::land(vl::land(vl::land(vl::land(vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(ota_on_hv), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(ota_on_hvz), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(term_rep_ok), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(vref), vl::logic<1>(0x1ULL))));
    vl::wr(cb_10u_ilptx, v);
}

void mipi_4_rx_dphy_ns_DPHYCBRX_bias::proc_always_152069() {
    vl::logic<10> cb_5u_ilprx_sh = vl::rd(cb_5u_ilprx);
    if (vl::land(vl::land(vl::land(vl::land(vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(ota_on_hv), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(ota_on_hvz), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(term_rep_ok), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(vref), vl::logic<1>(0x1ULL))).test()) {
        cb_5u_ilprx_sh = vl::logic<10>(0x3ffULL);
        vl::wr(cb_5u_ilprx, cb_5u_ilprx_sh);
    } else {
        cb_5u_ilprx_sh = vl::logic<10>::all_z();
        vl::wr(cb_5u_ilprx, cb_5u_ilprx_sh);
    }
}

void mipi_4_rx_dphy_ns_DPHYCBRX_bias::proc_always_152078() {
    vl::logic<4> cb_10u_ibias_ddl_sh = vl::rd(cb_10u_ibias_ddl);
    if (vl::land(vl::land(vl::land(vl::land(vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(ota_on_hv), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(ota_on_hvz), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(term_rep_ok), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(vref), vl::logic<1>(0x1ULL))).test()) {
        cb_10u_ibias_ddl_sh = vl::logic<4>(0xfULL);
        vl::wr(cb_10u_ibias_ddl, cb_10u_ibias_ddl_sh);
    } else {
        cb_10u_ibias_ddl_sh = vl::logic<4>::all_z();
        vl::wr(cb_10u_ibias_ddl, cb_10u_ibias_ddl_sh);
    }
}

void mipi_4_rx_dphy_ns_DPHYCBRX_bias::proc_always_152087() {
    vl::logic<5> cb_preamp_cm_sh = vl::rd(cb_preamp_cm);
    if (vl::land(vl::land(vl::land(vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(ota_on_int_z), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(term_rep_ok), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(vref), vl::logic<1>(0x1ULL))).test()) {
        cb_preamp_cm_sh = vl::logic<5>(0x1fULL);
        vl::wr(cb_preamp_cm, cb_preamp_cm_sh);
    } else {
        cb_preamp_cm_sh = vl::logic<5>::all_z();
        vl::wr(cb_preamp_cm, cb_preamp_cm_sh);
    }
}
