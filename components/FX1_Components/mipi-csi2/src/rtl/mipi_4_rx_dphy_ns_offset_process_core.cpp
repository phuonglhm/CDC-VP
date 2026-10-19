//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_offset_process_core
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_offset_process_core.h"

mipi_4_rx_dphy_ns_offset_process_core::mipi_4_rx_dphy_ns_offset_process_core(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    tie0__drv.o(tie0_);
    SC_METHOD(assign_ones_detector_complete_sum);
    sensitive << detected_ones_slicer_deserializer_0 << detected_ones_slicer_deserializer_1;
    SC_METHOD(assign_global_done);
    sensitive << done_ones_find[0] << done_ones_find[1];
    SC_METHOD(assign_data_ready);
    sensitive << data_ready_5;
    SC_METHOD(assign_store);
    sensitive << store_5;
    counter_slicer_0.reset(new mipi_4_rx_dphy_ns_ones_counter("counter_slicer_0"));
    counter_slicer_0->wordclk(wordclk);
    counter_slicer_0->enable_counter(enable_counter_sync);
    counter_slicer_0->resetz(resetz_wordclk);
    counter_slicer_0->slicer_deserializer_n_to_1(slicer_deserializer_0);
    counter_slicer_0->detected_ones(detected_ones_slicer_deserializer_0);
    counter_slicer_0->done(done_ones_find[0]);
    counter_slicer_1.reset(new mipi_4_rx_dphy_ns_ones_counter("counter_slicer_1"));
    counter_slicer_1->wordclk(wordclk);
    counter_slicer_1->enable_counter(enable_counter_sync);
    counter_slicer_1->resetz(resetz_wordclk);
    counter_slicer_1->slicer_deserializer_n_to_1(slicer_deserializer_1);
    counter_slicer_1->detected_ones(detected_ones_slicer_deserializer_1);
    counter_slicer_1->done(done_ones_find[1]);
    gen_sync_enable_counter_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_sync_enable_counter_sync"));
    gen_sync_enable_counter_sync->clk(wordclk);
    gen_sync_enable_counter_sync->rst(resetz_wordclk);
    gen_sync_enable_counter_sync->data_in(enable_counter);
    gen_sync_enable_counter_sync->scanmode(tie0_);
    gen_sync_enable_counter_sync->data_out(enable_counter_sync);
    gen_sync_enable_counter_sync->data_out_dly(nc_gen_sync_enable_counter_sync_data_out_dly);
    // gen sync in order to handle with the acknowledge
    gen_sync_data_ready_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_sync_data_ready_sync"));
    gen_sync_data_ready_sync->clk(wordclk);
    gen_sync_data_ready_sync->rst(resetz_wordclk);
    gen_sync_data_ready_sync->data_in(data_ready_at_clock_in);
    gen_sync_data_ready_sync->scanmode(tie0_);
    gen_sync_data_ready_sync->data_out(data_ready_ack);
    gen_sync_data_ready_sync->data_out_dly(nc_gen_sync_data_ready_sync_data_out_dly);
    sar_guard_band_I01.reset(new mipi_4_rx_dphy_ns_sar_guard_band("sar_guard_band_I01"));
    sar_guard_band_I01->clock(hsclock);
    sar_guard_band_I01->resetz(resetz_hsclock);
    sar_guard_band_I01->total_ones(total_ones);
    sar_guard_band_I01->data_ready(data_ready);
    sar_guard_band_I01->cal_done(cal_done);
    sar_guard_band_I01->no_convergence(no_convergence);
    sar_guard_band_I01->control_current_word(control_current_word);
    sar_guard_band_I01->cfgclkfreq(cfgclkfreq);
    sar_guard_band_I01->cmp_polarity(cmp_polarity);
    sar_guard_band_I01->start_calibration(start_calibration);
    sar_guard_band_I01->offset_comp_threshold(offset_comp_threshold);
    sar_guard_band_I01->tolerance(tolerance);
    sar_guard_band_I01->enable_counter(enable_counter);
    sar_guard_band_I01->number_of_ready_count(number_of_ready_count);
    sar_guard_band_I01->repeatcal(repeatcal);
    sar_guard_band_I01->afe_offset_cal_en(afe_offset_cal_en);
    sar_guard_band_I01->data_ready_at_clock_in(data_ready_at_clock_in);
    SC_METHOD(proc_always_144);
    sensitive << wordclk.pos() << resetz_wordclk.neg();
    dont_initialize();
    SC_METHOD(proc_always_166);
    sensitive << wordclk.pos() << resetz_wordclk.neg();
    dont_initialize();
    SC_METHOD(proc_always_203);
    sensitive << wordclk.pos() << resetz_wordclk.neg();
    dont_initialize();
    SC_METHOD(proc_always_249);
    sensitive << wordclk.pos() << resetz_wordclk.neg();
    dont_initialize();
    SC_METHOD(proc_always_268);
    sensitive << wordclk.pos() << resetz_wordclk.neg();
    dont_initialize();
}

vl::logic<2> mipi_4_rx_dphy_ns_offset_process_core::rd_done_ones_find() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(done_ones_find[0]));
    v.set_bit(1, vl::rd(done_ones_find[1]));
    return v;
}

void mipi_4_rx_dphy_ns_offset_process_core::assign_ones_detector_complete_sum() {
    vl::wr(ones_detector_complete_sum, vl::cat(vl::logic<4>(0x0ULL), vl::rd(detected_ones_slicer_deserializer_0)) + vl::cat(vl::logic<4>(0x0ULL), vl::rd(detected_ones_slicer_deserializer_1)));
}

// creating the wordclock/16  signal
void mipi_4_rx_dphy_ns_offset_process_core::assign_global_done() {
    vl::wr(global_done, vl::rand_(rd_done_ones_find()));
}

// always
// handling now with the acumulator
void mipi_4_rx_dphy_ns_offset_process_core::assign_data_ready() {
    vl::wr(data_ready, vl::rd(data_ready_5));
}

void mipi_4_rx_dphy_ns_offset_process_core::assign_store() {
    vl::wr(store, vl::rd(store_5));
}

// now register the outputs done and   detected_ones
// 8 bits number of counters
void mipi_4_rx_dphy_ns_offset_process_core::proc_always_144() {
    if (vl::rd(resetz_wordclk).truth_val() == 0) {
        // reset the sampling mechanism
        vl::wr(ones_detector_all_added, vl::logic<7>(0x0ULL));
    } else {
        if (vl::rd(enable_counter_sync).truth_val() == 0) {
            vl::wr(ones_detector_all_added, vl::logic<7>(0x0ULL));
        } else {
            vl::wr(ones_detector_all_added, vl::rd(ones_detector_complete_sum));
        }
    }
}

// indicates that all the slicers are correcly measured
void mipi_4_rx_dphy_ns_offset_process_core::proc_always_166() {
    if (vl::rd(resetz_wordclk).truth_val() == 0) {
        vl::wr(counter_ratio_1, vl::logic<6>(0x0ULL));
        vl::wr(store_5, vl::logic<1>(0x0ULL));
    } else {
        // include now the handshake mechanism for the data transfer between the two domains
        if (vl::rd(data_ready_ack).truth_val() == 0) {
            if (vl::rd(global_done).test()) {
                if (vl::eq(vl::rd(counter_ratio_1).ext<32>(), vl::logic<32>(0xfULL)).test()) {
                    vl::wr(counter_ratio_1, vl::rd(counter_ratio_1));
                    vl::wr(store_5, vl::logic<1>(0x1ULL));
                } else {
                    if (vl::eq(vl::rd(counter_ratio_1), vl::logic<6>(0x0ULL)).test()) {
                        if (vl::rd(store_5).test()) {
                            vl::wr(store_5, vl::logic<1>(0x0ULL));
                        }
                    }
                    vl::wr(counter_ratio_1, vl::rd(counter_ratio_1) + vl::logic<6>(0x1ULL));
                }
            } else {
                vl::wr(counter_ratio_1, vl::logic<6>(0x0ULL));
                vl::wr(store_5, vl::logic<1>(0x0ULL));
            }
        } else {
            // Only when the data is received by the other domain and ack in the current domain the store and counter are reseted
            vl::wr(counter_ratio_1, vl::logic<6>(0x0ULL));
            vl::wr(store_5, vl::logic<1>(0x0ULL));
        }
    }
}

void mipi_4_rx_dphy_ns_offset_process_core::proc_always_203() {
    if (vl::rd(resetz_wordclk).truth_val() == 0) {
        vl::wr(flag_5, vl::logic<1>(0x0ULL));
        vl::wr(data_ready_5, vl::logic<1>(0x0ULL));
    } else {
        if (vl::rd(data_ready_ack).truth_val() == 0) {
            if (vl::rd(global_done).test()) {
                if (vl::rd(store_5).test()) {
                    vl::wr(flag_5, vl::logic<1>(0x1ULL));
                    vl::wr(data_ready_5, vl::logic<1>(0x1ULL));
                } else {
                    if (vl::rd(flag_5).test()) {
                        vl::wr(flag_5, vl::logic<1>(0x0ULL));
                        vl::wr(data_ready_5, vl::logic<1>(0x0ULL));
                    } else {
                        vl::wr(flag_5, vl::rd(flag_5));
                        vl::wr(data_ready_5, vl::rd(data_ready_5));
                    }
                }
            } else {
                vl::wr(flag_5, vl::logic<1>(0x0ULL));
                vl::wr(data_ready_5, vl::logic<1>(0x0ULL));
            }
        } else {
            // reset the flag and data_ready5 only when the ack flag is synched into current domain
            vl::wr(flag_5, vl::logic<1>(0x0ULL));
            vl::wr(data_ready_5, vl::logic<1>(0x0ULL));
        }
    }
}

// accumulator
void mipi_4_rx_dphy_ns_offset_process_core::proc_always_249() {
    if (vl::rd(resetz_wordclk).truth_val() == 0) {
        // clear the accumulator before new update
        vl::wr(accumulator, vl::logic<8>(0x0ULL));
    } else {
        if (vl::rd(enable_counter_sync).truth_val() == 0) {
            vl::wr(accumulator, vl::logic<8>(0x0ULL));
        } else {
            if (vl::rd(store).test()) {
                vl::wr(accumulator, vl::rd(accumulator));
            } else {
                vl::wr(accumulator, vl::rd(accumulator) + vl::cat(vl::logic<1>(0x0ULL), vl::rd(ones_detector_all_added)));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_offset_process_core::proc_always_268() {
    // depends on the store flag that has info about the 5 or 50 ratio
    if (vl::rd(resetz_wordclk).truth_val() == 0) {
        // clear the accumulator before new update
        vl::wr(total_ones, vl::logic<8>(0x0ULL));
    } else {
        if (vl::rd(enable_counter_sync).truth_val() == 0) {
            vl::wr(total_ones, vl::logic<8>(0x0ULL));
        } else {
            if (vl::rd(store).test()) {
                vl::wr(total_ones, vl::rd(accumulator));
            }
        }
    }
}
