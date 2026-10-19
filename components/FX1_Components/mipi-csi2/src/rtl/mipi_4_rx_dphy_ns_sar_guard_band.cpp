//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_sar_guard_band
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_sar_guard_band.h"

mipi_4_rx_dphy_ns_sar_guard_band::mipi_4_rx_dphy_ns_sar_guard_band(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    tie0__drv.o(tie0_);
    SC_METHOD(assign_cmpstateout);
    sensitive << total_ones_sync << offset_comp_threshold;
    SC_METHOD(assign_hit_threshold);
    sensitive << total_ones_sync << offset_comp_threshold << tolerance;
    SC_METHOD(assign_control_current_word);
    sensitive << bcal;
    SC_METHOD(assign_no_convergence);
    sensitive << calsarerror;
    SC_METHOD(assign_cal_done);
    sensitive << caldone;
    SC_METHOD(assign_comparator_polarity);
    sensitive << cmp_polarity << cmpstateout;
    SC_METHOD(assign_cmpstateout_syn);
    sensitive << comparator_polarity;
    SC_METHOD(assign_onehotbit);
    sensitive << bit_cnt;
    // dynamic_pulse_catcher i01_data_ready_strobe (
    // .pulse_in(data_ready),
    // .clk(clock),
    // .rst(resetz),
    // .pulse_out(data_ready_at_clock_in)
    // );
    data_ready_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("data_ready_sync"));
    data_ready_sync->clk(clock);
    data_ready_sync->rst(resetz);
    data_ready_sync->data_in(data_ready);
    data_ready_sync->scanmode(tie0_);
    data_ready_sync->data_out(data_ready_at_clock_in);
    data_ready_sync->data_out_dly(nc_data_ready_sync_data_out_dly);
    SC_METHOD(proc_always_154);
    sensitive << clock.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_203);
    sensitive << clock.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_241);
    sensitive << clock.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_252);
    sensitive << clock.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_269);
    sensitive << clock.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_316);
    sensitive << clock.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_345);
    sensitive << clock.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_358);
    sensitive << clock.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_368);
    sensitive << clock.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_391);
    sensitive << clock.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_404);
    sensitive << clock.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_421);
    sensitive << clock.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_449);
    sensitive << clock.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_492);
    sensitive << clock.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_510);
    sensitive << clock.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_540);
    sensitive << clock.pos() << resetz.neg();
    dont_initialize();
}

// cmpstateout = 1 means that compensation voltage is hiher than threshoold ( need to be decreased )
// cmpstateout = 0 means that compnestraion voltage is lower than threshoold ( need to be increased )
void mipi_4_rx_dphy_ns_sar_guard_band::assign_cmpstateout() {
    vl::wr(cmpstateout, vl::mux(vl::gt(vl::rd(total_ones_sync), vl::rd(offset_comp_threshold)), vl::logic<1>(0x1ULL), vl::logic<1>(0x0ULL)));
}

// decrease if excessive compensation 1'b1
void mipi_4_rx_dphy_ns_sar_guard_band::assign_hit_threshold() {
    vl::wr(hit_threshold, vl::land(vl::gt(vl::rd(total_ones_sync), vl::rd(offset_comp_threshold) - (vl::rd(tolerance) + vl::logic<8>(0x1ULL))), vl::lt(vl::rd(total_ones_sync), vl::rd(offset_comp_threshold) + (vl::rd(tolerance) + vl::logic<8>(0x1ULL)))));
}

// ensure connection with the bcal
void mipi_4_rx_dphy_ns_sar_guard_band::assign_control_current_word() {
    vl::wr(control_current_word, vl::rd(bcal));
}

void mipi_4_rx_dphy_ns_sar_guard_band::assign_no_convergence() {
    vl::wr(no_convergence, vl::rd(calsarerror));
}

void mipi_4_rx_dphy_ns_sar_guard_band::assign_cal_done() {
    vl::wr(cal_done, vl::rd(caldone));
}

void mipi_4_rx_dphy_ns_sar_guard_band::assign_comparator_polarity() {
    vl::wr(comparator_polarity, vl::mux(vl::rd(cmp_polarity), ~vl::rd(cmpstateout), vl::rd(cmpstateout)));
}

// SYNC cmpstateout
// Removed gen_sync to avoid latency in the comparator signal
// mipi_4_rx_dphy_ns_gen_sync cmpstateout_sync (
// .clk(clock),
// .rst(resetz),
// .data_in(comparator_polarity),
// .scanmode(1'b0),
// .data_out(cmpstateout_syn),
// .data_out_dly()
// );
void mipi_4_rx_dphy_ns_sar_guard_band::assign_cmpstateout_syn() {
    vl::wr(cmpstateout_syn, vl::rd(comparator_polarity));
}

// One-Hot bit generation ( indicates  the position to analyze)
void mipi_4_rx_dphy_ns_sar_guard_band::assign_onehotbit() {
    vl::wr(onehotbit, vl::mux(vl::eq(vl::rd(bit_cnt), vl::logic<3>(0x5ULL)), vl::logic<6>(0x20ULL), vl::mux(vl::eq(vl::rd(bit_cnt), vl::logic<3>(0x4ULL)), vl::logic<6>(0x10ULL), vl::mux(vl::eq(vl::rd(bit_cnt), vl::logic<3>(0x3ULL)), vl::logic<6>(0x8ULL), vl::mux(vl::eq(vl::rd(bit_cnt), vl::logic<3>(0x2ULL)), vl::logic<6>(0x4ULL), vl::mux(vl::eq(vl::rd(bit_cnt), vl::logic<3>(0x1ULL)), vl::logic<6>(0x2ULL), vl::mux(vl::eq(vl::rd(bit_cnt), vl::logic<3>(0x0ULL)), vl::logic<6>(0x1ULL), vl::logic<6>(0x0ULL))))))));
}

void mipi_4_rx_dphy_ns_sar_guard_band::proc_always_154() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(guard_time_finished, vl::logic<1>(0x0ULL));
        // enables to load the first value after reset
        vl::wr(guard_counter, vl::logic<10>(0x0ULL));
    } else {
        // only start couting after the first sample
        {
            const vl::logic<4> sel = vl::rd(state);
            if (vl::case_eq(sel, vl::logic<4>(0x9ULL))) {
                if (vl::eq(vl::rd(guard_counter), vl::cat(vl::logic<2>(0x0ULL), vl::rd(cfgclkfreq))).test()) {
                    // did we get our goal
                    vl::wr(guard_time_finished, vl::logic<1>(0x1ULL));
                } else {
                    vl::wr(guard_counter, vl::rd(guard_counter) + vl::logic<10>(0x1ULL));
                    vl::wr(guard_time_finished, vl::logic<1>(0x0ULL));
                }
            } else {
                vl::wr(guard_counter, vl::logic<10>(0x0ULL));
                // reset counter and flags
                vl::wr(guard_time_finished, vl::logic<1>(0x0ULL));
            }
        }
    }
}

// please sample at guard clock the respective data_ready
void mipi_4_rx_dphy_ns_sar_guard_band::proc_always_203() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(total_ones_sync, vl::logic<8>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_2 = vl::rd(state);
            if (vl::case_eq(sel_2, vl::logic<4>(0x8ULL))) {
                vl::wr(total_ones_sync, vl::rd(total_ones_sync));
            } else {
                if (vl::rd(data_ready_at_clock_in).test()) {
                    vl::wr(total_ones_sync, vl::rd(total_ones));
                } else {
                    vl::wr(total_ones_sync, vl::rd(total_ones_sync));
                }
            }
        }
    }
}

void mipi_4_rx_dphy_ns_sar_guard_band::proc_always_241() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(edge_detect, vl::logic<2>(0x0ULL));
    } else {
        vl::wr(edge_detect, vl::cat(vl::rd(edge_detect).bit(0), vl::rd(start_calibration)));
    }
}

void mipi_4_rx_dphy_ns_sar_guard_band::proc_always_252() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(start_calibration_syn, vl::logic<1>(0x0ULL));
    } else {
        if (vl::eq(vl::rd(edge_detect), vl::logic<2>(0x1ULL)).test()) {
            vl::wr(start_calibration_syn, vl::logic<1>(0x1ULL));
        } else {
            vl::wr(start_calibration_syn, vl::logic<1>(0x0ULL));
        }
    }
}

// FSM
void mipi_4_rx_dphy_ns_sar_guard_band::proc_always_269() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(state, vl::logic<4>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_3 = vl::rd(state);
            if (vl::case_eq(sel_3, vl::logic<4>(0x0ULL))) {
                if (vl::rd(start_calibration_syn).test()) {
                    vl::wr(state, vl::logic<4>(0x9ULL));
                } else {
                    vl::wr(state, vl::logic<4>(0x0ULL));
                }
            } else if (vl::case_eq(sel_3, vl::logic<4>(0x1ULL))) {
                if (vl::rd(calsardone).test()) {
                    vl::wr(state, vl::logic<4>(0x9ULL));
                } else {
                    vl::wr(state, vl::logic<4>(0x1ULL));
                }
            } else if (vl::case_eq(sel_3, vl::logic<4>(0x2ULL))) {
                if (vl::rd(calsardone).test()) {
                    vl::wr(state, vl::logic<4>(0x9ULL));
                } else {
                    vl::wr(state, vl::logic<4>(0x2ULL));
                }
            } else if (vl::case_eq(sel_3, vl::logic<4>(0x3ULL))) {
                if (vl::rd(calsardone).test()) {
                    vl::wr(state, vl::logic<4>(0x9ULL));
                } else {
                    vl::wr(state, vl::logic<4>(0x3ULL));
                }
            } else if (vl::case_eq(sel_3, vl::logic<4>(0x4ULL))) {
                if (vl::rd(calsardone).test()) {
                    vl::wr(state, vl::logic<4>(0x9ULL));
                } else {
                    vl::wr(state, vl::logic<4>(0x4ULL));
                }
            } else if (vl::case_eq(sel_3, vl::logic<4>(0x5ULL))) {
                if (vl::rd(calsardone).test()) {
                    vl::wr(state, vl::logic<4>(0x9ULL));
                } else {
                    vl::wr(state, vl::logic<4>(0x5ULL));
                }
            } else if (vl::case_eq(sel_3, vl::logic<4>(0x6ULL))) {
                if (vl::land(vl::rd(calsardone), vl::rd(calsarerror)).test()) {
                    vl::wr(state, vl::logic<4>(0x7ULL));
                } else if (vl::rd(calsardone).test()) {
                    vl::wr(state, vl::logic<4>(0x8ULL));
                } else {
                    vl::wr(state, vl::logic<4>(0x6ULL));
                }
            } else if (vl::case_eq(sel_3, vl::logic<4>(0x9ULL))) {
                if (vl::land(vl::rd(guard_time_finished), vl::lnot(vl::rd(data_ready_at_clock_in))).test()) {
                    vl::wr(state, vl::logic<4>(0xaULL));
                } else {
                    vl::wr(state, vl::logic<4>(0x9ULL));
                }
            } else if (vl::case_eq(sel_3, vl::logic<4>(0xaULL))) {
                if (vl::rd(sample_done).test()) {
                    vl::wr(state, vl::rd(save_state));
                } else {
                    vl::wr(state, vl::logic<4>(0xaULL));
                }
            } else if (vl::case_eq(sel_3, vl::logic<4>(0x7ULL))) {
                vl::wr(state, vl::logic<4>(0x8ULL));
            } else if (vl::case_eq(sel_3, vl::logic<4>(0x8ULL))) {
                vl::wr(state, vl::logic<4>(0x0ULL));
            } else {
                vl::wr(state, vl::logic<4>(0x0ULL));
            }
        }
    }
}

// FSM ( thgsi provides you where to go after you enter in the WAITCAL 1us and the data ready sampling)
void mipi_4_rx_dphy_ns_sar_guard_band::proc_always_316() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(save_state, vl::logic<4>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_4 = vl::rd(state);
            if (vl::case_eq(sel_4, vl::logic<4>(0x0ULL))) {
                if (vl::rd(start_calibration_syn).test()) {
                    vl::wr(save_state, vl::logic<4>(0x1ULL));
                } else {
                    vl::wr(save_state, vl::rd(save_state));
                }
            } else if (vl::case_eq(sel_4, vl::logic<4>(0x1ULL))) {
                if (vl::rd(calsardone).test()) {
                    vl::wr(save_state, vl::logic<4>(0x2ULL));
                } else {
                    vl::wr(save_state, vl::rd(save_state));
                }
            } else if (vl::case_eq(sel_4, vl::logic<4>(0x2ULL))) {
                if (vl::rd(calsardone).test()) {
                    vl::wr(save_state, vl::logic<4>(0x3ULL));
                } else {
                    vl::wr(save_state, vl::rd(save_state));
                }
            } else if (vl::case_eq(sel_4, vl::logic<4>(0x3ULL))) {
                if (vl::rd(calsardone).test()) {
                    vl::wr(save_state, vl::logic<4>(0x4ULL));
                } else {
                    vl::wr(save_state, vl::rd(save_state));
                }
            } else if (vl::case_eq(sel_4, vl::logic<4>(0x4ULL))) {
                if (vl::rd(calsardone).test()) {
                    vl::wr(save_state, vl::logic<4>(0x5ULL));
                } else {
                    vl::wr(save_state, vl::rd(save_state));
                }
            } else if (vl::case_eq(sel_4, vl::logic<4>(0x5ULL))) {
                if (vl::rd(calsardone).test()) {
                    vl::wr(save_state, vl::logic<4>(0x6ULL));
                } else {
                    vl::wr(save_state, vl::rd(save_state));
                }
            } else {
                vl::wr(save_state, vl::rd(save_state));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_sar_guard_band::proc_always_345() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(afe_offset_cal_en, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_5 = vl::rd(state);
            if (vl::case_eq(sel_5, vl::logic<4>(0x0ULL))) {
                vl::wr(afe_offset_cal_en, vl::logic<1>(0x0ULL));
            } else {
                vl::wr(afe_offset_cal_en, vl::logic<1>(0x1ULL));
            }
        }
    }
}

// sample data ready ( a couple of samples is needed)
void mipi_4_rx_dphy_ns_sar_guard_band::proc_always_358() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(ready_cnt, vl::logic<3>(0x0ULL));
    } else if (vl::eq(vl::rd(ready_cnt), vl::rd(number_of_ready_count)).test()) {
        vl::wr(ready_cnt, vl::logic<3>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_6 = vl::rd(state);
            if (vl::case_eq(sel_6, vl::logic<4>(0xaULL))) {
                if (vl::rd(data_ready_at_clock_in).test()) {
                    vl::wr(ready_cnt, vl::rd(ready_cnt) + vl::logic<3>(0x1ULL));
                } else {
                    vl::wr(ready_cnt, vl::rd(ready_cnt));
                }
            } else {
                vl::wr(ready_cnt, vl::logic<3>(0x0ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_sar_guard_band::proc_always_368() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(sample_done, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_7 = vl::rd(state);
            if (vl::case_eq(sel_7, vl::logic<4>(0xaULL))) {
                if (vl::eq(vl::rd(ready_cnt), vl::rd(number_of_ready_count)).test()) {
                    vl::wr(sample_done, vl::logic<1>(0x1ULL));
                }
            } else {
                vl::wr(sample_done, vl::logic<1>(0x0ULL));
            }
        }
    }
}

// // triggers the 1us counting
// if(!resetz)
// grant_guard_time_count <= 1'b0 ;
// else if(guard_time_finished) grant_guard_time_count <= 1'b0 ;  // did we count the 1us time ( if yes deactivate counter and then change state)
// else case(state)
// WAITCAL    : grant_guard_time_count <= 1'b1  ;  // trigger counting once we enter in the WAITCAL time
// default : grant_guard_time_count <= 1'b0  ;
// endcase
// Bit Counter enable ( enables counting moving in the SAR bits)
void mipi_4_rx_dphy_ns_sar_guard_band::proc_always_391() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(enable_counter, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_8 = vl::rd(state);
            if (vl::case_eq(sel_8, vl::logic<4>(0x9ULL))) {
                if (vl::rd(guard_time_finished).test()) {
                    vl::wr(enable_counter, vl::logic<1>(0x1ULL));
                } else {
                    vl::wr(enable_counter, vl::logic<1>(0x0ULL));
                }
            } else if (vl::case_eq(sel_8, vl::logic<4>(0x8ULL))) {
                vl::wr(enable_counter, vl::logic<1>(0x0ULL));
            } else {
                vl::wr(enable_counter, vl::logic<1>(0x1ULL));
            }
        }
    }
}

// Bit Counter enable ( enables counting moving in the SAR bits)
void mipi_4_rx_dphy_ns_sar_guard_band::proc_always_404() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(bit_cnt_ena, vl::logic<1>(0x0ULL));
    } else if (vl::rd(calsardone).test()) {
        vl::wr(bit_cnt_ena, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_9 = vl::rd(state);
            if (vl::case_eq(sel_9, vl::logic<4>(0x1ULL))) {
                vl::wr(bit_cnt_ena, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_9, vl::logic<4>(0x2ULL))) {
                vl::wr(bit_cnt_ena, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_9, vl::logic<4>(0x3ULL))) {
                vl::wr(bit_cnt_ena, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_9, vl::logic<4>(0x4ULL))) {
                vl::wr(bit_cnt_ena, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_9, vl::logic<4>(0x5ULL))) {
                vl::wr(bit_cnt_ena, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_9, vl::logic<4>(0x6ULL))) {
                vl::wr(bit_cnt_ena, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_9, vl::logic<4>(0x9ULL))) {
                vl::wr(bit_cnt_ena, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_9, vl::logic<4>(0xaULL))) {
                vl::wr(bit_cnt_ena, vl::logic<1>(0x1ULL));
            } else {
                vl::wr(bit_cnt_ena, vl::logic<1>(0x0ULL));
            }
        }
    }
}

// Bit Counter ( updates the position that is being analyzed)
void mipi_4_rx_dphy_ns_sar_guard_band::proc_always_421() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(bit_cnt, vl::logic<3>(0x5ULL));
    } else {
        {
            const vl::logic<4> sel_10 = vl::rd(state);
            if (vl::case_eq(sel_10, vl::logic<4>(0x1ULL))) {
                if (vl::land(vl::ne(vl::rd(bit_cnt), vl::logic<3>(0x0ULL)), vl::rd(calsardone)).test()) {
                    vl::wr(bit_cnt, vl::rd(bit_cnt) - vl::logic<3>(0x1ULL));
                }
            } else if (vl::case_eq(sel_10, vl::logic<4>(0x2ULL))) {
                if (vl::land(vl::ne(vl::rd(bit_cnt), vl::logic<3>(0x0ULL)), vl::rd(calsardone)).test()) {
                    vl::wr(bit_cnt, vl::rd(bit_cnt) - vl::logic<3>(0x1ULL));
                }
            } else if (vl::case_eq(sel_10, vl::logic<4>(0x3ULL))) {
                if (vl::land(vl::ne(vl::rd(bit_cnt), vl::logic<3>(0x0ULL)), vl::rd(calsardone)).test()) {
                    vl::wr(bit_cnt, vl::rd(bit_cnt) - vl::logic<3>(0x1ULL));
                }
            } else if (vl::case_eq(sel_10, vl::logic<4>(0x4ULL))) {
                if (vl::land(vl::ne(vl::rd(bit_cnt), vl::logic<3>(0x0ULL)), vl::rd(calsardone)).test()) {
                    vl::wr(bit_cnt, vl::rd(bit_cnt) - vl::logic<3>(0x1ULL));
                }
            } else if (vl::case_eq(sel_10, vl::logic<4>(0x5ULL))) {
                if (vl::land(vl::ne(vl::rd(bit_cnt), vl::logic<3>(0x0ULL)), vl::rd(calsardone)).test()) {
                    vl::wr(bit_cnt, vl::rd(bit_cnt) - vl::logic<3>(0x1ULL));
                }
            } else if (vl::case_eq(sel_10, vl::logic<4>(0x6ULL))) {
                if (vl::land(vl::ne(vl::rd(bit_cnt), vl::logic<3>(0x0ULL)), vl::rd(calsardone)).test()) {
                    vl::wr(bit_cnt, vl::rd(bit_cnt));
                }
            } else if (vl::case_eq(sel_10, vl::logic<4>(0x9ULL))) {
                vl::wr(bit_cnt, vl::rd(bit_cnt));
            } else if (vl::case_eq(sel_10, vl::logic<4>(0xaULL))) {
                vl::wr(bit_cnt, vl::rd(bit_cnt));
            } else {
                vl::wr(bit_cnt, vl::logic<3>(0x5ULL));
            }
        }
    }
}

// Successive approximation state done flag - Pulse generation
void mipi_4_rx_dphy_ns_sar_guard_band::proc_always_449() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(calsardone, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_11 = vl::rd(state);
            if (vl::case_eq(sel_11, vl::logic<4>(0x0ULL))) {
                vl::wr(calsardone, vl::logic<1>(0x0ULL));
            } else if (vl::case_eq(sel_11, vl::logic<4>(0x1ULL))) {
                if (vl::rd(calsardone).test()) {
                    vl::wr(calsardone, vl::logic<1>(0x0ULL));
                } else if (vl::ne(vl::rd(bit_cnt), vl::logic<3>(0x0ULL)).test()) {
                    vl::wr(calsardone, vl::logic<1>(0x1ULL));
                }
            } else if (vl::case_eq(sel_11, vl::logic<4>(0x2ULL))) {
                if (vl::rd(calsardone).test()) {
                    vl::wr(calsardone, vl::logic<1>(0x0ULL));
                } else if (vl::ne(vl::rd(bit_cnt), vl::logic<3>(0x0ULL)).test()) {
                    vl::wr(calsardone, vl::logic<1>(0x1ULL));
                }
            } else if (vl::case_eq(sel_11, vl::logic<4>(0x3ULL))) {
                if (vl::rd(calsardone).test()) {
                    vl::wr(calsardone, vl::logic<1>(0x0ULL));
                } else if (vl::ne(vl::rd(bit_cnt), vl::logic<3>(0x0ULL)).test()) {
                    vl::wr(calsardone, vl::logic<1>(0x1ULL));
                }
            } else if (vl::case_eq(sel_11, vl::logic<4>(0x4ULL))) {
                if (vl::rd(calsardone).test()) {
                    vl::wr(calsardone, vl::logic<1>(0x0ULL));
                } else if (vl::ne(vl::rd(bit_cnt), vl::logic<3>(0x0ULL)).test()) {
                    vl::wr(calsardone, vl::logic<1>(0x1ULL));
                }
            } else if (vl::case_eq(sel_11, vl::logic<4>(0x5ULL))) {
                if (vl::rd(calsardone).test()) {
                    vl::wr(calsardone, vl::logic<1>(0x0ULL));
                } else if (vl::ne(vl::rd(bit_cnt), vl::logic<3>(0x0ULL)).test()) {
                    vl::wr(calsardone, vl::logic<1>(0x1ULL));
                }
            } else if (vl::case_eq(sel_11, vl::logic<4>(0x6ULL))) {
                if (vl::rd(calsardone).test()) {
                    vl::wr(calsardone, vl::logic<1>(0x0ULL));
                } else if (vl::eq(vl::rd(bit_cnt), vl::logic<3>(0x0ULL)).test()) {
                    vl::wr(calsardone, vl::logic<1>(0x1ULL));
                }
            } else if (vl::case_eq(sel_11, vl::logic<4>(0x7ULL))) {
                vl::wr(calsardone, vl::logic<1>(0x0ULL));
            } else if (vl::case_eq(sel_11, vl::logic<4>(0x8ULL))) {
                vl::wr(calsardone, vl::logic<1>(0x0ULL));
            } else {
                vl::wr(calsardone, vl::logic<1>(0x0ULL));
            }
        }
    }
}

// // Calibration error flag
// if(!resetz)
// calerror <= 1'b0 ;
// else begin
// if(repeatcal)
// calerror <= 1'b0 ;
// else begin
// case(state)
// CALERROR: calerror <= 1'b1      ;
// CALDONE : calerror <= calerror  ;
// CALIDLE : calerror <= calerror  ;
// default : calerror <= 1'b0      ;
// endcase
// end
// end
// Calibration done flag
void mipi_4_rx_dphy_ns_sar_guard_band::proc_always_492() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(caldone, vl::logic<1>(0x0ULL));
    } else {
        if (vl::rd(repeatcal).test()) {
            vl::wr(caldone, vl::logic<1>(0x0ULL));
        } else {
            {
                const vl::logic<4> sel_12 = vl::rd(state);
                if (vl::case_eq(sel_12, vl::logic<4>(0x8ULL))) {
                    vl::wr(caldone, vl::logic<1>(0x1ULL));
                } else if (vl::case_eq(sel_12, vl::logic<4>(0x0ULL))) {
                    vl::wr(caldone, vl::rd(caldone));
                } else {
                    vl::wr(caldone, vl::logic<1>(0x0ULL));
                }
            }
        }
    }
}

// Offset calibration control bits for PWM receiver
void mipi_4_rx_dphy_ns_sar_guard_band::proc_always_510() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(bcal, vl::logic<6>(0x20ULL));
    } else if (vl::lor(vl::lor(vl::lor(vl::lor(vl::lor(vl::eq(vl::rd(state), vl::logic<4>(0x1ULL)), vl::eq(vl::rd(state), vl::logic<4>(0x2ULL))), vl::eq(vl::rd(state), vl::logic<4>(0x3ULL))), vl::eq(vl::rd(state), vl::logic<4>(0x4ULL))), vl::eq(vl::rd(state), vl::logic<4>(0x5ULL))), vl::eq(vl::rd(state), vl::logic<4>(0x6ULL))).test()) {
        if (vl::rd(bit_cnt_ena).truth_val() == 0) {
            vl::wr(bcal, vl::logic<6>(0x20ULL));
        } else if (vl::rd(caldone).truth_val() == 0) {
            if (vl::lor(vl::eq(vl::rd(bit_cnt), vl::logic<3>(0x7ULL)), vl::eq(vl::rd(bcal).bit(5), vl::logic<1>(0x1ULL))).test()) {
                vl::wr(bcal, (vl::rd(bcal) & ((~vl::rd(onehotbit)) | vl::rep<6>(vl::rd(cmpstateout_syn)))) | vl::cat(vl::logic<1>(0x0ULL), vl::rd(onehotbit).slice<5>(1)));
            } else {
                vl::wr(bcal, (vl::rd(bcal) & ((~vl::rd(onehotbit)) | vl::rep<6>(~vl::rd(cmpstateout_syn)))) | vl::cat(vl::logic<1>(0x0ULL), vl::rd(onehotbit).slice<5>(1)));
            }
        } else {
            vl::wr(bcal, vl::rd(bcal));
        }
    } else if (vl::eq(vl::rd(state), vl::logic<4>(0x7ULL)).test()) {
        vl::wr(bcal, vl::logic<6>(0x0ULL));
    } else if (vl::eq(vl::rd(state), vl::logic<4>(0x8ULL)).test()) {
        if (vl::rd(calsarerror).test()) {
            vl::wr(bcal, vl::logic<6>(0x0ULL));
        } else {
            vl::wr(bcal, vl::rd(bcal));
        }
    } else if (vl::eq(vl::rd(state), vl::logic<4>(0x0ULL)).test()) {
        if (vl::rd(start_calibration_syn).truth_val() == 0) {
            // keeps the current value until you get a new calibration request
            vl::wr(bcal, vl::rd(bcal));
        } else {
            vl::wr(bcal, vl::logic<6>(0x20ULL));
        }
    }
}

// ERRORS
void mipi_4_rx_dphy_ns_sar_guard_band::proc_always_540() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(calsarerror, vl::logic<1>(0x0ULL));
    } else if (vl::rd(repeatcal).test()) {
        vl::wr(calsarerror, vl::logic<1>(0x0ULL));
    } else if (vl::land(vl::rd(calsardone), vl::eq(vl::rd(state), vl::logic<4>(0x6ULL))).test()) {
        if (vl::lor(vl::land(vl::land(vl::eq(vl::rd(bcal), vl::logic<6>(0x3fULL)), vl::rd(cmpstateout_syn)), ~vl::rd(hit_threshold)), vl::land(vl::land(vl::eq(vl::rd(bcal), vl::logic<6>(0x1fULL)), ~vl::rd(cmpstateout_syn)), ~vl::rd(hit_threshold))).test()) {
            vl::wr(calsarerror, vl::logic<1>(0x1ULL));
        } else {
            vl::wr(calsarerror, vl::rd(calsarerror));
        }
    } else {
        vl::wr(calsarerror, vl::rd(calsarerror));
    }
}
