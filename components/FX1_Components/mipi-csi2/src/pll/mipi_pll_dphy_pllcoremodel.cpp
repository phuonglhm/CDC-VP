//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_pllcoremodel
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_pllcoremodel.h"

mipi_pll_dphy_pllcoremodel::mipi_pll_dphy_pllcoremodel(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(drive_clkout);
    sensitive << clkout_r;
    SC_METHOD(drive_fbclk);
    sensitive << fbclk_r;
    SC_METHOD(drive_lock);
    sensitive << lock_r;
    SC_METHOD(drive_refclk);
    sensitive << refclk_r;
    SC_METHOD(assign_phase_error_pulse);
    sensitive << refclk_r << fbclk_r;
    SC_METHOD(assign_jitter_on);
    sensitive << this_ref_per << rl_indiv << half_period << rl_fbkdiv;
    SC_METHOD(assign_vcoclk);
    sensitive << enable_ssc << sscclk << vcoclk_nossc;
    SC_METHOD(assign_clkout_div);
    sensitive << vcoclkdiv_phase;
    SC_METHOD(assign_clok_net);
    sensitive << refclk_r << fbclk_r;
    SC_METHOD(proc_initial_10699);
    SC_METHOD(proc_always_10742);
    sensitive << feedbackdiv;
    SC_METHOD(proc_always_10749);
    sensitive << inputdiv;
    SC_METHOD(proc_always_10756);
    sensitive << outputdiv;
    SC_METHOD(proc_always_10763);
    sensitive << ssc_depth;
    SC_METHOD(proc_always_10768);
    sensitive << ssc_freq;
    SC_METHOD(proc_always_10784);
    sensitive << clkin.pos();
    dont_initialize();
    SC_THREAD(proc_always_10795);
    set_stack_size(0x20000);
    SC_THREAD(proc_always_10805);
    set_stack_size(0x20000);
    SC_METHOD(proc_always_10808);
    sensitive << refclk_r.posedge_event() << pfd_reset.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_10814);
    sensitive << fbclk_r.posedge_event() << pfd_reset.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_10821);
    sensitive << refclk_r.posedge_event() << fbclk_r.posedge_event() << on.neg();
    dont_initialize();
    SC_THREAD(proc_always_10849);
    set_stack_size(0x20000);
    SC_METHOD(proc_always_10870);
    sensitive << up.posedge_event() << dw.posedge_event() << on.neg();
    dont_initialize();
    SC_METHOD(proc_initial_10903);
    SC_THREAD(proc_initial_10911);
    set_stack_size(0x20000);
    SC_THREAD(proc_initial_10931);
    set_stack_size(0x20000);
    SC_THREAD(proc_initial_10952);
    set_stack_size(0x20000);
    SC_METHOD(proc_always_10969);
    sensitive << vcoclk;
    SC_METHOD(proc_initial_10977);
    SC_THREAD(proc_initial_10979);
    set_stack_size(0x20000);
    SC_METHOD(proc_always_10991);
    sensitive << vcoclkdiv;
    SC_METHOD(proc_always_10996);
    sensitive << clok_net.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_10998);
    sensitive << clkin.pos() << on.neg();
    dont_initialize();
    SC_METHOD(proc_always_11012);
    sensitive << vcoclk_nossc.posedge_event() << on.neg();
    dont_initialize();
    SC_METHOD(proc_initial_11023);
    SC_THREAD(proc_initial_11024);
    set_stack_size(0x20000);
    SC_METHOD(proc_always_11039);
    sensitive << triwaveclk.posedge_event() << on.neg();
    dont_initialize();
    SC_METHOD(proc_always_11051);
    sensitive << triwaveclk.posedge_event() << on.neg();
    dont_initialize();
    SC_METHOD(proc_always_11062);
    sensitive << enable_ssc << center_downz << rl_sscdepth << triwaveform;
    SC_METHOD(proc_initial_11079);
    SC_THREAD(proc_initial_11080);
    set_stack_size(0x20000);
}

void mipi_pll_dphy_pllcoremodel::drive_clkout() { vl::wr(clkout, vl::rd(clkout_r)); }

void mipi_pll_dphy_pllcoremodel::drive_fbclk() { vl::wr(fbclk, vl::rd(fbclk_r)); }

void mipi_pll_dphy_pllcoremodel::drive_lock() { vl::wr(lock, vl::rd(lock_r)); }

void mipi_pll_dphy_pllcoremodel::drive_refclk() { vl::wr(refclk, vl::rd(refclk_r)); }

double mipi_pll_dphy_pllcoremodel::abs(double value) {
    double abs_r = 0.0;
    abs_r = vl::mux(vl::lbool(value < vl::to_real(vl::logic<32>(0x0ULL), true)), (-value), value);
    return abs_r;
}

void mipi_pll_dphy_pllcoremodel::assign_phase_error_pulse() {
    vl::wr(phase_error_pulse, vl::rd(refclk_r) ^ vl::rd(fbclk_r));
}

void mipi_pll_dphy_pllcoremodel::assign_jitter_on() {
    vl::wr(jitter_on, vl::mux(vl::lbool(abs((((this_ref_per.read() * rl_indiv.read()) * 1.0) - ((half_period.read() * rl_fbkdiv.read()) * 2.0))) < 0.005), vl::logic<1>(0x1ULL), vl::logic<1>(0x0ULL)));
}

void mipi_pll_dphy_pllcoremodel::assign_vcoclk() {
    vl::wr(vcoclk, vl::mux(vl::rd(enable_ssc), vl::rd(sscclk), vl::rd(vcoclk_nossc)));
}

void mipi_pll_dphy_pllcoremodel::assign_clkout_div() {
    vl::wr(clkout_div, vl::rd(vcoclkdiv_phase));
}

void mipi_pll_dphy_pllcoremodel::assign_clok_net() {
    vl::wr(clok_net, vl::rd(refclk_r) & vl::rd(fbclk_r));
}

void mipi_pll_dphy_pllcoremodel::proc_initial_10699() {
    double half_period_sh = half_period.read();
    double phase_error_sh = phase_error.read();
    double refclk_lastedge_sh = refclk_lastedge.read();
    double rl_fbkdiv_sh = rl_fbkdiv.read();
    double rl_indiv_sh = rl_indiv.read();
    double rl_outdiv_sh = rl_outdiv.read();
    double rl_sscdepth_sh = rl_sscdepth.read();
    double rl_sscfreq_sh = rl_sscfreq.read();
    double slope_sh = slope.read();
    double ssc_amount_sh = ssc_amount.read();
    double start_error_sh = start_error.read();
    double this_ref_per_sh = this_ref_per.read();
    double ycross_sh = ycross.read();
    this_ref_per_sh = 200.0;
    this_ref_per.write(this_ref_per_sh);
    refclk_lastedge_sh = 200.0;
    refclk_lastedge.write(refclk_lastedge_sh);
    half_period_sh = 1.8;
    half_period.write(half_period_sh);
    start_error_sh = 0.0;
    start_error.write(start_error_sh);
    phase_error_sh = 0.0;
    phase_error.write(phase_error_sh);
    if (vl::lor(vl::ceq(vl::rd(feedbackdiv), vl::logic<64>::all_z()), vl::ceq(vl::rd(feedbackdiv), vl::logic<64>())).test()) {
        rl_fbkdiv_sh = vl::to_real(vl::logic<32>(0x32ULL), true);
        rl_fbkdiv.write(rl_fbkdiv_sh);
    } else if (vl::lt(vl::rd(feedbackdiv), vl::logic<64>(0x1ULL)).test()) {
        rl_fbkdiv_sh = vl::to_real(vl::logic<32>(0x32ULL), true);
        rl_fbkdiv.write(rl_fbkdiv_sh);
    } else {
        rl_fbkdiv_sh = vl::bitstoreal(vl::rd(feedbackdiv));
        rl_fbkdiv.write(rl_fbkdiv_sh);
    }
    if (vl::lor(vl::ceq(vl::rd(inputdiv), vl::logic<64>::all_z()), vl::ceq(vl::rd(inputdiv), vl::logic<64>())).test()) {
        rl_indiv_sh = vl::to_real(vl::logic<32>(0x2ULL), true);
        rl_indiv.write(rl_indiv_sh);
    } else if (vl::lt(vl::rd(inputdiv), vl::logic<64>(0x1ULL)).test()) {
        rl_indiv_sh = vl::to_real(vl::logic<32>(0x2ULL), true);
        rl_indiv.write(rl_indiv_sh);
    } else {
        rl_indiv_sh = vl::bitstoreal(vl::rd(inputdiv));
        rl_indiv.write(rl_indiv_sh);
    }
    if (vl::lor(vl::ceq(vl::rd(outputdiv), vl::logic<64>::all_z()), vl::ceq(vl::rd(outputdiv), vl::logic<64>())).test()) {
        rl_outdiv_sh = vl::to_real(vl::logic<32>(0x1ULL), true);
        rl_outdiv.write(rl_outdiv_sh);
    } else if (vl::lt(vl::rd(outputdiv), vl::logic<64>(0x1ULL)).test()) {
        rl_outdiv_sh = vl::to_real(vl::logic<32>(0x1ULL), true);
        rl_outdiv.write(rl_outdiv_sh);
    } else {
        rl_outdiv_sh = vl::bitstoreal(vl::rd(outputdiv));
        rl_outdiv.write(rl_outdiv_sh);
    }
    if (vl::lor(vl::ceq(vl::rd(ssc_depth), vl::logic<64>::all_z()), vl::ceq(vl::rd(ssc_depth), vl::logic<64>())).test()) {
        rl_sscdepth_sh = 0.01;
        rl_sscdepth.write(rl_sscdepth_sh);
    } else {
        rl_sscdepth_sh = vl::bitstoreal(vl::rd(ssc_depth));
        rl_sscdepth.write(rl_sscdepth_sh);
    }
    if (vl::lor(vl::ceq(vl::rd(ssc_freq), vl::logic<64>::all_z()), vl::ceq(vl::rd(ssc_freq), vl::logic<64>())).test()) {
        rl_sscfreq_sh = 100.0;
        rl_sscfreq.write(rl_sscfreq_sh);
    } else {
        rl_sscfreq_sh = vl::bitstoreal(vl::rd(ssc_freq));
        rl_sscfreq.write(rl_sscfreq_sh);
    }
    slope_sh = 0.0;
    slope.write(slope_sh);
    ycross_sh = 0.0;
    ycross.write(ycross_sh);
    ssc_amount_sh = 0.0;
    ssc_amount.write(ssc_amount_sh);
}

void mipi_pll_dphy_pllcoremodel::proc_always_10742() {
    double rl_fbkdiv_sh_2 = rl_fbkdiv.read();
    if (vl::lor(vl::ceq(vl::rd(feedbackdiv), vl::logic<64>::all_z()), vl::ceq(vl::rd(feedbackdiv), vl::logic<64>())).test()) {
        rl_fbkdiv_sh_2 = vl::to_real(vl::logic<32>(0x32ULL), true);
        rl_fbkdiv.write(rl_fbkdiv_sh_2);
    } else if (vl::lt(vl::rd(feedbackdiv), vl::logic<64>(0x1ULL)).test()) {
        rl_fbkdiv_sh_2 = vl::to_real(vl::logic<32>(0x32ULL), true);
        rl_fbkdiv.write(rl_fbkdiv_sh_2);
    } else {
        rl_fbkdiv_sh_2 = vl::bitstoreal(vl::rd(feedbackdiv));
        rl_fbkdiv.write(rl_fbkdiv_sh_2);
    }
}

void mipi_pll_dphy_pllcoremodel::proc_always_10749() {
    double rl_indiv_sh_2 = rl_indiv.read();
    if (vl::lor(vl::ceq(vl::rd(inputdiv), vl::logic<64>::all_z()), vl::ceq(vl::rd(inputdiv), vl::logic<64>())).test()) {
        rl_indiv_sh_2 = vl::to_real(vl::logic<32>(0x2ULL), true);
        rl_indiv.write(rl_indiv_sh_2);
    } else if (vl::lt(vl::rd(inputdiv), vl::logic<64>(0x1ULL)).test()) {
        rl_indiv_sh_2 = vl::to_real(vl::logic<32>(0x2ULL), true);
        rl_indiv.write(rl_indiv_sh_2);
    } else {
        rl_indiv_sh_2 = vl::bitstoreal(vl::rd(inputdiv));
        rl_indiv.write(rl_indiv_sh_2);
    }
}

void mipi_pll_dphy_pllcoremodel::proc_always_10756() {
    double rl_outdiv_sh_2 = rl_outdiv.read();
    if (vl::lor(vl::ceq(vl::rd(outputdiv), vl::logic<64>::all_z()), vl::ceq(vl::rd(outputdiv), vl::logic<64>())).test()) {
        rl_outdiv_sh_2 = vl::to_real(vl::logic<32>(0x1ULL), true);
        rl_outdiv.write(rl_outdiv_sh_2);
    } else if (vl::lt(vl::rd(outputdiv), vl::logic<64>(0x1ULL)).test()) {
        rl_outdiv_sh_2 = vl::to_real(vl::logic<32>(0x1ULL), true);
        rl_outdiv.write(rl_outdiv_sh_2);
    } else {
        rl_outdiv_sh_2 = vl::bitstoreal(vl::rd(outputdiv));
        rl_outdiv.write(rl_outdiv_sh_2);
    }
}

void mipi_pll_dphy_pllcoremodel::proc_always_10763() {
    double rl_sscdepth_sh_2 = rl_sscdepth.read();
    if (vl::lor(vl::ceq(vl::rd(ssc_depth), vl::logic<64>::all_z()), vl::ceq(vl::rd(ssc_depth), vl::logic<64>())).test()) {
        rl_sscdepth_sh_2 = 0.01;
        rl_sscdepth.write(rl_sscdepth_sh_2);
    } else {
        rl_sscdepth_sh_2 = vl::bitstoreal(vl::rd(ssc_depth));
        rl_sscdepth.write(rl_sscdepth_sh_2);
    }
}

void mipi_pll_dphy_pllcoremodel::proc_always_10768() {
    double rl_sscfreq_sh_2 = rl_sscfreq.read();
    if (vl::lor(vl::ceq(vl::rd(ssc_freq), vl::logic<64>::all_z()), vl::ceq(vl::rd(ssc_freq), vl::logic<64>())).test()) {
        rl_sscfreq_sh_2 = 100.0;
        rl_sscfreq.write(rl_sscfreq_sh_2);
    } else {
        rl_sscfreq_sh_2 = vl::bitstoreal(vl::rd(ssc_freq));
        rl_sscfreq.write(rl_sscfreq_sh_2);
    }
}

void mipi_pll_dphy_pllcoremodel::proc_always_10784() {
    double refclk_lastedge_sh_2 = refclk_lastedge.read();
    double this_ref_per_sh_2 = this_ref_per.read();
    vl::logic<1> watchdog_sh = vl::rd(watchdog);
    watchdog_sh = vl::logic<1>(0x1ULL);
    vl::wr(watchdog, watchdog_sh);
    if ((vl::lbool((vl::realtime(TU_) - refclk_lastedge_sh_2) > 100000.0)).test()) {
        this_ref_per_sh_2 = 100000.0;
        this_ref_per.write(this_ref_per_sh_2);
    } else {
        this_ref_per_sh_2 = (((vl::realtime(TU_) - refclk_lastedge_sh_2) + (this_ref_per_sh_2 * 1.0)) / 2.0);
        this_ref_per.write(this_ref_per_sh_2);
    }
    refclk_lastedge_sh_2 = vl::realtime(TU_);
    refclk_lastedge.write(refclk_lastedge_sh_2);
}

void mipi_pll_dphy_pllcoremodel::proc_always_10795() {
    vl::logic<1> lock_sh = vl::rd(lock_r);
    double this_ref_per_sh_3 = this_ref_per.read();
    vl::logic<1> watchdog_sh_2 = vl::rd(watchdog);
    for (;;) {
        wait(vl::dly(100000.0, TU_, TP_));
        lock_sh = vl::rd(lock_r);
        this_ref_per_sh_3 = this_ref_per.read();
        watchdog_sh_2 = vl::rd(watchdog);
        if (watchdog_sh_2.truth_val() == 0) {
            this_ref_per_sh_3 = 200.0;
            this_ref_per.write(this_ref_per_sh_3);
            lock_sh = vl::logic<1>(0x0ULL);
            vl::wr(lock_r, lock_sh);
        } else {
            watchdog_sh_2 = vl::logic<1>(0x0ULL);
            vl::wr(watchdog, watchdog_sh_2);
        }
    }
}

void mipi_pll_dphy_pllcoremodel::proc_always_10805() {
    vl::logic<1> pfd_reset_sh = vl::rd(pfd_reset);
    for (;;) {
        wait(up.value_changed_event() | dw.value_changed_event() | on.value_changed_event());
        pfd_reset_sh = vl::rd(pfd_reset);
        wait(vl::dly(0.2, TU_, TP_));
        pfd_reset_sh = vl::rd(pfd_reset);
        pfd_reset_sh = vl::land(~vl::land(vl::rd(up), vl::rd(dw)), vl::rd(on));
        vl::wr(pfd_reset, pfd_reset_sh);
    }
}

void mipi_pll_dphy_pllcoremodel::proc_always_10808() {
    if (vl::rd(pfd_reset).truth_val() == 0) {
        vl::wr(up, vl::logic<1>(0x0ULL));
    } else {
        vl::wr(up, vl::logic<1>(0x1ULL));
    }
}

void mipi_pll_dphy_pllcoremodel::proc_always_10814() {
    if (vl::rd(pfd_reset).truth_val() == 0) {
        vl::wr(dw, vl::logic<1>(0x0ULL));
    } else {
        vl::wr(dw, vl::logic<1>(0x1ULL));
    }
}

void mipi_pll_dphy_pllcoremodel::proc_always_10821() {
    if (vl::rd(on).truth_val() == 0) {
        vl::wr(phase_error_sign, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<2> sel = vl::cat(vl::rd(refclk_r), vl::rd(fbclk_r));
            if (vl::case_eq(sel, vl::logic<2>(0x2ULL))) {
                vl::wr(phase_error_sign, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel, vl::logic<2>(0x1ULL))) {
                vl::wr(phase_error_sign, vl::logic<1>(0x0ULL));
            } else {
                vl::wr(phase_error_sign, vl::rd(phase_error_sign));
            }
        }
    }
}

// start_error = $realtime ;
// end
// start_error = $realtime ;
// if(refclk && fbclk)
// phase_error = 0 ;
// else begin
// if(refclk && fbclk)
// phase_error = $realtime - start_error ;
// end
// end
void mipi_pll_dphy_pllcoremodel::proc_always_10849() {
    double phase_error_sh_2 = phase_error.read();
    double start_error_sh_2 = start_error.read();
    for (;;) {
        wait(phase_error_pulse.posedge_event());
        phase_error_sh_2 = phase_error.read();
        start_error_sh_2 = start_error.read();
        wait(vl::dly(0.0001, TU_, TP_));
        phase_error_sh_2 = phase_error.read();
        start_error_sh_2 = start_error.read();
        if (vl::rd(phase_error_pulse).test()) {
            start_error_sh_2 = vl::realtime(TU_);
            start_error.write(start_error_sh_2);
            if (vl::land(vl::rd(refclk_r), vl::rd(fbclk_r)).test()) {
                phase_error_sh_2 = 0.0;
                phase_error.write(phase_error_sh_2);
            } else {
                wait(phase_error_pulse.negedge_event());
                phase_error_sh_2 = phase_error.read();
                start_error_sh_2 = start_error.read();
                if (vl::land(vl::rd(refclk_r), vl::rd(fbclk_r)).test()) {
                    phase_error_sh_2 = (vl::realtime(TU_) - start_error_sh_2);
                    phase_error.write(phase_error_sh_2);
                }
            }
        } else {
            phase_error_sh_2 = 0.0;
            phase_error.write(phase_error_sh_2);
        }
    }
}

void mipi_pll_dphy_pllcoremodel::proc_always_10870() {
    double half_period_sh_2 = half_period.read();
    if (vl::rd(on).truth_val() == 0) {
        half_period_sh_2 = 1.8;
        half_period.write(half_period_sh_2);
    } else if (vl::lbool(this_ref_per.read() < 0.0).test()) {
        half_period_sh_2 = half_period_sh_2;
        half_period.write(half_period_sh_2);
    } else {
        {
            const vl::logic<2> sel_2 = vl::cat(vl::rd(up), vl::rd(dw));
            if (vl::case_eq(sel_2, vl::logic<2>(0x2ULL))) {
                if (vl::lbool(phase_error.read() < 0.005).test()) {
                    // if (phase_error > 500) half_period = half_period - VCO_ADJUST - 0.5;  //saturation
                    // else if (phase_error > half_period) half_period = half_period - VCO_ADJUST - (half_period*0.001) ;
                    // else half_period = half_period - VCO_ADJUST - (phase_error*0.001) ;  //smallest adjust
                    half_period_sh_2 = ((((1.0 - (1.0 / 5.0)) * half_period_sh_2) + (this_ref_per.read() / ((5.0 * vl::to_real(vl::logic<32>(0x2ULL), true)) * rl_fbkdiv.read()))) - ((phase_error.read() * 1.0) / (((5.0 * vl::to_real(vl::logic<32>(0x2ULL), true)) * rl_fbkdiv.read()) * vl::to_real(vl::logic<32>(0x9ULL), true))));
                    half_period.write(half_period_sh_2);
                } else {
                    half_period_sh_2 = ((((1.0 - (1.0 / 5.0)) * half_period_sh_2) + (this_ref_per.read() / ((5.0 * vl::to_real(vl::logic<32>(0x2ULL), true)) * rl_fbkdiv.read()))) - ((phase_error.read() * 1.0) / (((5.0 * vl::to_real(vl::logic<32>(0x2ULL), true)) * rl_fbkdiv.read()) * vl::to_real(vl::logic<32>(0x6ULL), true))));
                    half_period.write(half_period_sh_2);
                }
            } else if (vl::case_eq(sel_2, vl::logic<2>(0x1ULL))) {
                if (vl::lbool(phase_error.read() < 0.005).test()) {
                    // if (phase_error > 500) half_period = half_period + VCO_ADJUST + 0.5;  //saturation
                    // else if (phase_error > half_period) half_period = half_period + VCO_ADJUST + (half_period*0.001) ;
                    // else half_period = half_period + VCO_ADJUST + (phase_error*0.001) ;  //smallest adjust
                    half_period_sh_2 = ((((1.0 - (1.0 / 5.0)) * half_period_sh_2) + (this_ref_per.read() / ((5.0 * vl::to_real(vl::logic<32>(0x2ULL), true)) * rl_fbkdiv.read()))) + ((phase_error.read() * 1.0) / (((5.0 * vl::to_real(vl::logic<32>(0x2ULL), true)) * rl_fbkdiv.read()) * vl::to_real(vl::logic<32>(0x9ULL), true))));
                    half_period.write(half_period_sh_2);
                } else {
                    half_period_sh_2 = ((((1.0 - (1.0 / 5.0)) * half_period_sh_2) + (this_ref_per.read() / ((5.0 * vl::to_real(vl::logic<32>(0x2ULL), true)) * rl_fbkdiv.read()))) + ((phase_error.read() * 1.0) / (((5.0 * vl::to_real(vl::logic<32>(0x2ULL), true)) * rl_fbkdiv.read()) * vl::to_real(vl::logic<32>(0x6ULL), true))));
                    half_period.write(half_period_sh_2);
                }
            } else {
                half_period_sh_2 = half_period_sh_2;
                half_period.write(half_period_sh_2);
            }
        }
    }
    if (vl::lbool(half_period_sh_2 < 0.142).test()) {
        half_period_sh_2 = 0.142;
        half_period.write(half_period_sh_2);
    } else if ((vl::lbool(half_period_sh_2 > 1.8)).test()) {
        half_period_sh_2 = 1.8;
        half_period.write(half_period_sh_2);
    }
}

void mipi_pll_dphy_pllcoremodel::proc_initial_10903() {
    vl::logic<8> clkout_sh = vl::rd(clkout_r);
    vl::logic<1> fbclk_sh = vl::rd(fbclk_r);
    vl::logic<1> refclk_sh = vl::rd(refclk_r);
    vl::logic<1> vcoclk_nossc_sh = vl::rd(vcoclk_nossc);
    clkout_sh = vl::logic<8>(0x0ULL);
    vl::wr(clkout_r, clkout_sh);
    fbclk_sh = vl::logic<1>(0x0ULL);
    vl::wr(fbclk_r, fbclk_sh);
    refclk_sh = vl::logic<1>(0x0ULL);
    vl::wr(refclk_r, refclk_sh);
    vcoclk_nossc_sh = vl::logic<1>(0x0ULL);
    vl::wr(vcoclk_nossc, vcoclk_nossc_sh);
}

void mipi_pll_dphy_pllcoremodel::proc_initial_10911() {
    for (;;) {
        wait(vl::dly(0.0, TU_, TP_));
        if ((vl::lbool(half_period.read() > 0.0)).test()) {
            if (vl::ceq(vl::rd(deskewen), vl::logic<1>(0x1ULL)).test()) {
                wait(deskewin.value_changed_event());
            } else {
                for (int64_t rep = (int64_t)std::llround(rl_fbkdiv.read()); rep > 0; --rep) {
                    wait(vcoclk_nossc.value_changed_event());
                }
            }
            if (vl::ceq(vl::rd(deskewen), vl::logic<1>(0x1ULL)).test()) {
                vl::wr(fbclk_r, vl::rd(deskewin));
            } else {
                vl::wr(fbclk_r, ~vl::rd(fbclk_r));
            }
        } else {
            wait(vl::dly(100.0, TU_, TP_));
            vl::wr(fbclk_r, vl::logic<1>(0x0ULL));
        }
    }
}

void mipi_pll_dphy_pllcoremodel::proc_initial_10931() {
    for (;;) {
        wait(vl::dly(0.0, TU_, TP_));
        if ((vl::lbool(half_period.read() > 0.0)).test()) {
            if (vl::ceq(vl::rd(deskewen), vl::logic<1>(0x1ULL)).test()) {
                wait(clkin.value_changed_event());
                vl::wr(refclk_r, vl::rd(clkin));
            } else {
                for (int64_t rep_2 = (int64_t)std::llround(rl_indiv.read()); rep_2 > 0; --rep_2) {
                    wait(clkin.value_changed_event());
                }
                vl::wr(refclk_r, ~vl::rd(refclk_r));
            }
        } else {
            wait(vl::dly(100.0, TU_, TP_));
            vl::wr(refclk_r, vl::logic<1>(0x0ULL));
        }
    }
}

void mipi_pll_dphy_pllcoremodel::proc_initial_10952() {
    for (;;) {
        wait(vl::dly(0.0, TU_, TP_));
        if ((vl::lbool(half_period.read() > 0.0)).test()) {
            wait(vl::dly(half_period.read(), TU_, TP_));
            vl::wr(vcoclk_nossc, ~vl::rd(vcoclk_nossc));
        } else {
            wait(vl::dly(100.0, TU_, TP_));
            vl::wr(vcoclk_nossc, vl::logic<1>(0x0ULL));
        }
    }
}

void mipi_pll_dphy_pllcoremodel::proc_always_10969() {
    phcount = vl::logic<32>(0x0ULL);
    while (vl::slt(phcount, vl::logic<32>(0x8ULL)).test()) {
        if (vl::rd(enable_ssc).test()) {
            { const vl::logic<1> v_ = vl::land(vl::rd(vcoclk), vl::lor(vl::rd(lock_r), vl::rd(bypass))); const int64_t p0_ = vl::ipos(vl::idx(phcount, true), 0, true); aq_.after(vl::dly(((vl::to_real(vl::logic<32>(0x2ULL) * phcount, true) * ssc_hperiod.read()) / vl::to_real(vl::logic<32>(0x8ULL), true)), TU_, TP_), [this, v_, p0_]() { clkout_r.write_bit(p0_, v_); }); }
        } else {
            { const vl::logic<1> v_ = vl::land(vl::rd(vcoclk), vl::lor(vl::rd(lock_r), vl::rd(bypass))); const int64_t p0_ = vl::ipos(vl::idx(phcount, true), 0, true); aq_.after(vl::dly(((vl::to_real(vl::logic<32>(0x2ULL) * phcount, true) * half_period.read()) / vl::to_real(vl::logic<32>(0x8ULL), true)), TU_, TP_), [this, v_, p0_]() { clkout_r.write_bit(p0_, v_); }); }
        }
        phcount = phcount + vl::logic<32>(0x1ULL);
    }
}

void mipi_pll_dphy_pllcoremodel::proc_initial_10977() {
    vl::logic<1> vcoclkdiv_sh = vl::rd(vcoclkdiv);
    vcoclkdiv_sh = vl::logic<1>(0x0ULL);
    vl::wr(vcoclkdiv, vcoclkdiv_sh);
}

void mipi_pll_dphy_pllcoremodel::proc_initial_10979() {
    vl::logic<1> vcoclkdiv_sh_2 = vl::rd(vcoclkdiv);
    for (;;) {
        wait(vl::dly(0.0, TU_, TP_));
        vcoclkdiv_sh_2 = vl::rd(vcoclkdiv);
        if (vl::rd(bypass).test()) {
            wait(clkin.value_changed_event());
            vcoclkdiv_sh_2 = vl::rd(vcoclkdiv);
            vcoclkdiv_sh_2 = vl::rd(clkin);
            vl::wr(vcoclkdiv, vcoclkdiv_sh_2);
        } else if (vl::lbool(rl_outdiv.read() == 1.0).test()) {
            wait(vcoclk.value_changed_event());
            vcoclkdiv_sh_2 = vl::rd(vcoclkdiv);
            vcoclkdiv_sh_2 = vl::rd(vcoclk);
            vl::wr(vcoclkdiv, vcoclkdiv_sh_2);
        } else {
            for (int64_t rep_3 = (int64_t)std::llround(rl_outdiv.read()); rep_3 > 0; --rep_3) {
                wait(vcoclk.value_changed_event());
                vcoclkdiv_sh_2 = vl::rd(vcoclkdiv);
            }
            vcoclkdiv_sh_2 = vl::lnot(vcoclkdiv_sh_2);
            vl::wr(vcoclkdiv, vcoclkdiv_sh_2);
        }
    }
}

void mipi_pll_dphy_pllcoremodel::proc_always_10991() {
    { const vl::logic<1> v_ = vl::land(vl::rd(vcoclkdiv), vl::lor(vl::rd(lock_r), vl::rd(bypass))); aq_.after(vl::dly(((vl::to_real(vl::logic<32>(0x2ULL) * vl::rd(phsel).ext<32>(), false) * half_period.read()) / vl::to_real(vl::logic<32>(0x20ULL), true)), TU_, TP_), [this, v_]() { vl::wr(vcoclkdiv_phase, v_); }); }
}

void mipi_pll_dphy_pllcoremodel::proc_always_10996() {
    phase_error_pos.write(phase_error.read());
}

void mipi_pll_dphy_pllcoremodel::proc_always_10998() {
    if (vl::rd(on).truth_val() == 0) {
        vl::wr(lock_counter, vl::logic<6>(0x0ULL));
    } else if (vl::land(vl::land(vl::lbool((((this_ref_per.read() * rl_indiv.read()) * 1.0) - ((half_period.read() * 2.0) * rl_fbkdiv.read())) < (0.2 * rl_fbkdiv.read())), vl::lbool((((this_ref_per.read() * rl_indiv.read()) * 1.0) - ((half_period.read() * 2.0) * rl_fbkdiv.read())) > ((-0.2) * rl_fbkdiv.read()))), vl::lbool(phase_error_pos.read() < ((this_ref_per.read() * rl_indiv.read()) * 0.05))).test()) {
        if (vl::ne(vl::rd(lock_counter), vl::logic<6>(0x3fULL)).test()) {
            vl::wr(lock_counter, vl::rd(lock_counter) + vl::logic<6>(0x1ULL));
        } else {
            vl::wr(lock_counter, vl::rd(lock_counter));
        }
    } else if ((vl::lor(vl::lbool((((this_ref_per.read() * rl_indiv.read()) * 1.0) - ((half_period.read() * 2.0) * rl_fbkdiv.read())) >= (0.2 * rl_fbkdiv.read())), vl::lbool((((this_ref_per.read() * rl_indiv.read()) * 1.0) - ((half_period.read() * 2.0) * rl_fbkdiv.read())) <= ((-0.2) * rl_fbkdiv.read())))).test()) {
        vl::wr(lock_counter, vl::logic<6>(0x0ULL));
    } else {
        vl::wr(lock_counter, vl::rd(lock_counter));
    }
}

void mipi_pll_dphy_pllcoremodel::proc_always_11012() {
    if (vl::rd(on).truth_val() == 0) {
        vl::wr(lock_r, vl::logic<1>(0x0ULL));
    } else if (vl::eq(vl::rd(lock_counter), vl::logic<6>(0x3fULL)).test()) {
        vl::wr(lock_r, vl::logic<1>(0x1ULL));
    } else {
        vl::wr(lock_r, vl::logic<1>(0x0ULL));
    }
}

void mipi_pll_dphy_pllcoremodel::proc_initial_11023() {
    vl::logic<1> triwaveclk_sh = vl::rd(triwaveclk);
    triwaveclk_sh = vl::logic<1>(0x0ULL);
    vl::wr(triwaveclk, triwaveclk_sh);
}

void mipi_pll_dphy_pllcoremodel::proc_initial_11024() {
    for (;;) {
        wait(vl::dly(0.0, TU_, TP_));
        if (vl::land(vl::rd(enable_ssc), vl::lbool(rl_sscfreq.read() > 0.0)).test()) {
            wait(vl::dly((1000000000.0 / (((4.0 * rl_sscfreq.read()) * 1000.0) * vl::to_real(vl::logic<32>(0x400ULL), true))), TU_, TP_));
            vl::wr(triwaveclk, ~vl::rd(triwaveclk));
        } else {
            vl::wr(triwaveclk, vl::logic<1>(0x0ULL));
            wait(vl::dly(1000.0, TU_, TP_));
        }
    }
}

void mipi_pll_dphy_pllcoremodel::proc_always_11039() {
    if (vl::rd(on).truth_val() == 0) {
        vl::wr(triwaveform, vl::logic<10>(0x0ULL));
    } else if (vl::rd(enable_ssc).truth_val() == 0) {
        vl::wr(triwaveform, vl::logic<10>(0x0ULL));
    } else if (vl::eq(vl::rd(up_wave), vl::logic<2>(0x3ULL)).test()) {
        vl::wr(triwaveform, vl::rd(triwaveform) + vl::logic<10>(0x1ULL));
    } else if (vl::eq(vl::rd(up_wave), vl::logic<2>(0x0ULL)).test()) {
        vl::wr(triwaveform, vl::rd(triwaveform) - vl::logic<10>(0x1ULL));
    } else {
        vl::wr(triwaveform, vl::rd(triwaveform));
    }
}

void mipi_pll_dphy_pllcoremodel::proc_always_11051() {
    if (vl::rd(on).truth_val() == 0) {
        vl::wr(up_wave, vl::logic<2>(0x3ULL));
    } else if (vl::eq(vl::rd(triwaveform).slice<9>(1), vl::logic<9>(0x1ffULL)).test()) {
        vl::wr(up_wave, vl::cat(vl::rd(up_wave).bit(0), vl::logic<1>(0x0ULL)));
    } else if (vl::eq(vl::rd(triwaveform).slice<9>(1), vl::logic<9>(0x0ULL)).test()) {
        vl::wr(up_wave, vl::cat(vl::rd(up_wave).bit(0), vl::logic<1>(0x1ULL)));
    } else {
        vl::wr(up_wave, vl::rd(up_wave));
    }
}

void mipi_pll_dphy_pllcoremodel::proc_always_11062() {
    double slope_sh_2 = slope.read();
    double ssc_amount_sh_2 = ssc_amount.read();
    double ycross_sh_2 = ycross.read();
    if (vl::rd(enable_ssc).truth_val() == 0) {
        slope_sh_2 = 0.0;
        slope.write(slope_sh_2);
        ycross_sh_2 = 0.0;
        ycross.write(ycross_sh_2);
        ssc_amount_sh_2 = 0.0;
        ssc_amount.write(ssc_amount_sh_2);
    } else {
        slope_sh_2 = (rl_sscdepth.read() / vl::to_real(vl::logic<32>(0x400ULL), true));
        slope.write(slope_sh_2);
        if (vl::rd(center_downz).test()) {
            ycross_sh_2 = ((-rl_sscdepth.read()) / 2.0);
            ycross.write(ycross_sh_2);
        } else {
            ycross_sh_2 = 0.0;
            ycross.write(ycross_sh_2);
        }
        ssc_amount_sh_2 = ((slope_sh_2 * vl::to_real(vl::rd(triwaveform), false)) + ycross_sh_2);
        ssc_amount.write(ssc_amount_sh_2);
    }
}

void mipi_pll_dphy_pllcoremodel::proc_initial_11079() {
    vl::logic<1> sscclk_sh = vl::rd(sscclk);
    sscclk_sh = vl::logic<1>(0x0ULL);
    vl::wr(sscclk, sscclk_sh);
}

void mipi_pll_dphy_pllcoremodel::proc_initial_11080() {
    double ssc_hperiod_sh = ssc_hperiod.read();
    for (;;) {
        wait(vl::dly(0.0, TU_, TP_));
        ssc_hperiod_sh = ssc_hperiod.read();
        if (vl::land(vl::rd(enable_ssc), vl::lbool(half_period.read() > ssc_amount.read())).test()) {
            ssc_hperiod_sh = (half_period.read() * (1.0 + ssc_amount.read()));
            ssc_hperiod.write(ssc_hperiod_sh);
            wait(vl::dly((half_period.read() * (1.0 + ssc_amount.read())), TU_, TP_));
            ssc_hperiod_sh = ssc_hperiod.read();
            vl::wr(sscclk, ~vl::rd(sscclk));
        } else {
            wait(vl::dly(1000.0, TU_, TP_));
            ssc_hperiod_sh = ssc_hperiod.read();
            vl::wr(sscclk, vl::logic<1>(0x0ULL));
        }
    }
}
