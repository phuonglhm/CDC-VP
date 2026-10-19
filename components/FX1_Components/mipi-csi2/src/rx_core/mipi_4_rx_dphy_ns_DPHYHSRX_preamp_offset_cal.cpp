//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal.h"

mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal::mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(drive_vcaln);
    sensitive << vcaln_r;
    SC_METHOD(drive_vcalp);
    sensitive << vcalp_r;
    SC_METHOD(assign_rx_hs_bcal);
    sensitive << rx_hs_bcalz;
    SC_METHOD(assign_supply_ok);
    sensitive << vp << gd;
    SC_METHOD(assign_enable);
    sensitive << rx_pre_ponz << vbplv_ccm;
    SC_METHOD(assign_rx_hs_bcal_ref);
    SC_METHOD(gate_atb_line_buf0);
    sensitive << vcalp_r << supply_ok << atb_sw_hv << atb_sw_hvz;
    SC_METHOD(gate_atb_line_buf1);
    sensitive << vcaln_r << supply_ok << atb_sw_hv << atb_sw_hvz;
    SC_METHOD(proc_initial_157579);
    SC_METHOD(proc_initial_157580);
    SC_THREAD(proc_initial_157590);
    set_stack_size(0x20000);
    SC_METHOD(proc_always_157611);
    sensitive << cfgclkfreq;
    SC_THREAD(proc_initial_157615);
    set_stack_size(0x20000);
    SC_METHOD(proc_always_157624);
    sensitive << rx_hs_bcal;
    SC_METHOD(proc_always_157640);
    sensitive << rx_hs_bcal;
    SC_METHOD(proc_always_157653);
    sensitive << rx_hs_bcal_ref;
    SC_METHOD(proc_initial_157667);
    SC_METHOD(proc_always_157673);
    sensitive << supply_ok << enable << rx_hs_bcal_aux << internal_clk << rx_hs_bcal_ref_aux;
    SC_METHOD(proc_always_157683);
    sensitive << supply_ok << enable << rx_hs_bcal_aux << internal_clk << rx_hs_bcal_ref_aux;
    SC_METHOD(proc_always_157706);
    sensitive << supply_ok << enable << rx_hs_bcal_aux << internal_clk << rx_hs_bcal_ref_aux;
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal::drive_vcaln() { vl::wr(vcaln, vl::rd(vcaln_r)); }

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal::drive_vcalp() { vl::wr(vcalp, vl::rd(vcalp_r)); }

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal::assign_rx_hs_bcal() {
    vl::wr(rx_hs_bcal, ~vl::rd(rx_hs_bcalz));
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal::assign_enable() {
    vl::wr(enable, vl::land(vl::ceq(vl::rd(rx_pre_ponz), vl::logic<1>(0x0ULL)), vl::ceq(vl::rd(vbplv_ccm), vl::logic<1>(0x1ULL))));
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal::assign_rx_hs_bcal_ref() {
    vl::wr(rx_hs_bcal_ref, vl::logic<6>(0xcULL));
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal::gate_atb_line_buf0() {
    const vl::logic<1> v = vl::bufif1_(vl::rd(vcalp_r), vl::land(vl::land(vl::land(vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(atb_sw_hv).bit(1), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(atb_sw_hv).bit(0), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(atb_sw_hvz).bit(1), vl::logic<1>(0x0ULL))));
    vl::wr(atb_line, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal::gate_atb_line_buf1() {
    const vl::logic<1> v = vl::bufif1_(vl::rd(vcaln_r), vl::land(vl::land(vl::land(vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(atb_sw_hv).bit(0), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(atb_sw_hv).bit(1), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(atb_sw_hvz).bit(0), vl::logic<1>(0x0ULL))));
    vl::wr(atb_line, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal::proc_initial_157579() {
    vl::logic<1> vcalp_sh = vl::rd(vcalp_r);
    vcalp_sh = vl::logic<1>(0x1ULL);
    vl::wr(vcalp_r, vcalp_sh);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal::proc_initial_157580() {
    vl::logic<1> vcaln_sh = vl::rd(vcaln_r);
    vcaln_sh = vl::logic<1>(0x0ULL);
    vl::wr(vcaln_r, vcaln_sh);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal::proc_initial_157590() {
    vl::logic<1> aux_sh = vl::rd(aux);
    double cfg_clk_period_2_sh = cfg_clk_period_2.read();
    double cfgclkfreq_sh = cfgclkfreq.read();
    vl::logic<6> rx_hs_bcal_aux_sh = vl::rd(rx_hs_bcal_aux);
    vl::logic<6> slicer_tol_sh = vl::rd(slicer_tol);
    double t0_sh = t0.read();
    double t1_sh = t1.read();
    vl::logic<1> timming_error_sh = vl::rd(timming_error);
    aux_sh = vl::random().ext<1>();
    vl::wr(aux, aux_sh);
    t0_sh = vl::realtime(TU_);
    t0.write(t0_sh);
    t1_sh = t0_sh;
    t1.write(t1_sh);
    timming_error_sh = vl::logic<1>(0x0ULL);
    vl::wr(timming_error, timming_error_sh);
    rx_hs_bcal_aux_sh = vl::logic<6>(0x20ULL);
    vl::wr(rx_hs_bcal_aux, rx_hs_bcal_aux_sh);
    slicer_tol_sh = vl::logic<6>(0x1ULL);
    vl::wr(slicer_tol, slicer_tol_sh);
    cfgclkfreq_sh = 27.0;
    cfgclkfreq.write(cfgclkfreq_sh);
    cfg_clk_period_2_sh = (1000.0 / (cfgclkfreq_sh * vl::to_real(vl::logic<32>(0x2ULL), true)));
    cfg_clk_period_2.write(cfg_clk_period_2_sh);
    wait(vl::dly(10.0, TU_, TP_));
    aux_sh = vl::rd(aux);
    cfg_clk_period_2_sh = cfg_clk_period_2.read();
    cfgclkfreq_sh = cfgclkfreq.read();
    rx_hs_bcal_aux_sh = vl::rd(rx_hs_bcal_aux);
    slicer_tol_sh = vl::rd(slicer_tol);
    t0_sh = t0.read();
    t1_sh = t1.read();
    timming_error_sh = vl::rd(timming_error);
    if (vl::ceq(vl::rd(rx_hs_bcal_ref).bit(5), vl::logic<1>(0x1ULL)).test()) {
        vl::wr(rx_hs_bcal_ref_aux, vl::rd(rx_hs_bcal_ref));
    } else {
        rx_hs_bcal_ref_aux.write_bit(5, vl::rd(rx_hs_bcal_ref).bit(5));
        rx_hs_bcal_ref_aux.write_part(0, ~vl::rd(rx_hs_bcal_ref).slice<5>(0));
    }
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal::proc_always_157611() {
    double cfg_clk_period_2_sh_2 = cfg_clk_period_2.read();
    cfg_clk_period_2_sh_2 = (1000.0 / (cfgclkfreq.read() * vl::to_real(vl::logic<32>(0x2ULL), true)));
    cfg_clk_period_2.write(cfg_clk_period_2_sh_2);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal::proc_initial_157615() {
    vl::logic<1> internal_clk_sh = vl::rd(internal_clk);
    internal_clk_sh = vl::logic<1>(0x0ULL);
    vl::wr(internal_clk, internal_clk_sh);
    wait(vl::dly(1.0, TU_, TP_));
    internal_clk_sh = vl::rd(internal_clk);
    for (;;) {
        wait(vl::dly(cfg_clk_period_2.read(), TU_, TP_));
        internal_clk_sh = vl::rd(internal_clk);
        vl::wr(internal_clk, ~internal_clk_sh);
    }
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal::proc_always_157624() {
    double t0_sh_2 = t0.read();
    double t1_sh_2 = t1.read();
    if (vl::land(vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(enable), vl::logic<1>(0x1ULL))).test()) {
        t1_sh_2 = vl::realtime(TU_);
        t1.write(t1_sh_2);
        if (vl::lbool((t1_sh_2 - t0_sh_2) < vl::to_real(vl::logic<32>(0x3e8ULL), true)).test()) {
            vl::wr(timming_error, vl::logic<1>(0x1ULL));
            t0_sh_2 = t1_sh_2;
            t0.write(t0_sh_2);
        } else {
            vl::wr(timming_error, vl::logic<1>(0x0ULL));
            t0_sh_2 = t1_sh_2;
            t0.write(t0_sh_2);
        }
    } else {
        vl::wr(timming_error, vl::logic<1>(0x0ULL));
    }
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal::proc_always_157640() {
    if (vl::land(vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(enable), vl::logic<1>(0x1ULL))).test()) {
        if (vl::ceq(vl::rd(rx_hs_bcal).bit(5), vl::logic<1>(0x1ULL)).test()) {
            vl::wr(rx_hs_bcal_aux, vl::rd(rx_hs_bcal));
        } else {
            rx_hs_bcal_aux.write_bit(5, vl::rd(rx_hs_bcal).bit(5));
            rx_hs_bcal_aux.write_part(0, ~vl::rd(rx_hs_bcal).slice<5>(0));
        }
    } else {
        vl::wr(rx_hs_bcal_aux, vl::logic<6>(0x20ULL));
    }
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal::proc_always_157653() {
    if (vl::ceq(vl::rd(rx_hs_bcal_ref).bit(5), vl::logic<1>(0x1ULL)).test()) {
        vl::wr(rx_hs_bcal_ref_aux, vl::rd(rx_hs_bcal_ref));
    } else {
        rx_hs_bcal_ref_aux.write_bit(5, vl::rd(rx_hs_bcal_ref).bit(5));
        rx_hs_bcal_ref_aux.write_part(0, ~vl::rd(rx_hs_bcal_ref).slice<5>(0));
    }
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal::proc_initial_157667() {
    vl::logic<6> zero_minus_tolerance_sh = vl::rd(zero_minus_tolerance);
    vl::logic<6> zero_plus_tolerance_sh = vl::rd(zero_plus_tolerance);
    zero_minus_tolerance_sh = vl::logic<6>(0x0ULL);
    vl::wr(zero_minus_tolerance, zero_minus_tolerance_sh);
    zero_plus_tolerance_sh = vl::logic<6>(0x0ULL) + vl::rd(slicer_tol);
    vl::wr(zero_plus_tolerance, zero_plus_tolerance_sh);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal::proc_always_157673() {
    vl::logic<32> lower_boundary_sh = vl::rd(lower_boundary);
    vl::logic<32> upper_boundary_sh = vl::rd(upper_boundary);
    lower_boundary_sh = vl::rd(rx_hs_bcal_ref_aux).ext<32>() - vl::rd(slicer_tol).ext<32>();
    vl::wr(lower_boundary, lower_boundary_sh);
    upper_boundary_sh = vl::rd(rx_hs_bcal_ref_aux).ext<32>() + vl::rd(slicer_tol).ext<32>();
    vl::wr(upper_boundary, upper_boundary_sh);
    if (vl::eq(vl::rd(rx_hs_bcal_ref_aux), vl::logic<6>(0x0ULL)).test()) {
        lower_boundary_sh = vl::rd(zero_minus_tolerance).ext<32>();
        vl::wr(lower_boundary, lower_boundary_sh);
        upper_boundary_sh = vl::rd(zero_plus_tolerance).ext<32>();
        vl::wr(upper_boundary, upper_boundary_sh);
    }
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal::proc_always_157683() {
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)).test()) {
        if (vl::ceq(vl::rd(enable), vl::logic<1>(0x1ULL)).test()) {
            if (vl::lt(vl::rd(rx_hs_bcal_aux).ext<32>(), vl::rd(lower_boundary)).test()) {
                vl::wr(vcalp_r, vl::logic<1>(0x1ULL));
            } else if (vl::gt(vl::rd(rx_hs_bcal_aux).ext<32>(), vl::rd(upper_boundary)).test()) {
                vl::wr(vcalp_r, vl::logic<1>(0x0ULL));
            } else {
                if (vl::rd(internal_clk).test()) {
                    vl::wr(vcalp_r, vl::rd(aux));
                    vl::wr(aux, ~vl::rd(aux));
                } else {
                    vl::wr(vcalp_r, vl::rd(vcalp_r));
                    vl::wr(aux, vl::rd(aux));
                }
            }
        } else {
            vl::wr(vcalp_r, vl::logic<1>());
        }
    } else {
        vl::wr(vcalp_r, vl::logic<1>::all_z());
    }
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal::proc_always_157706() {
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)).test()) {
        if (vl::ceq(vl::rd(enable), vl::logic<1>(0x1ULL)).test()) {
            if (vl::lt(vl::rd(rx_hs_bcal_aux).ext<32>(), vl::rd(lower_boundary)).test()) {
                vl::wr(vcaln_r, vl::logic<1>(0x0ULL));
            } else if (vl::gt(vl::rd(rx_hs_bcal_aux).ext<32>(), vl::rd(upper_boundary)).test()) {
                vl::wr(vcaln_r, vl::logic<1>(0x1ULL));
            } else if (vl::rd(internal_clk).test()) {
                vl::wr(vcaln_r, ~vl::rd(aux));
            } else {
                vl::wr(vcaln_r, vl::rd(vcaln_r));
            }
        } else {
            vl::wr(vcaln_r, vl::logic<1>());
        }
    } else {
        vl::wr(vcaln_r, vl::logic<1>::all_z());
    }
}
