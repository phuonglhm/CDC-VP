//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_dphynorexttune
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_dphynorexttune.h"

mipi_4_rx_dphy_ns_dphynorexttune::mipi_4_rx_dphy_ns_dphynorexttune(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    tie0__drv.o(tie0_);
    sample_count_osc_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("sample_count_osc_sync"));
    sample_count_osc_sync->clk(osc_in);
    sample_count_osc_sync->rst(resetz_osc_in);
    sample_count_osc_sync->data_in(sample_count);
    sample_count_osc_sync->scanmode(tie0_);
    sample_count_osc_sync->data_out(sample_count_osc);
    sample_count_osc_sync->data_out_dly(nc_sample_count_osc_sync_data_out_dly);
    sample_count_cfg_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("sample_count_cfg_sync"));
    sample_count_cfg_sync->clk(cfg_clk);
    sample_count_cfg_sync->rst(rstz);
    sample_count_cfg_sync->data_in(sample_count_osc);
    sample_count_cfg_sync->scanmode(tie0_);
    sample_count_cfg_sync->data_out(sample_count_cfg);
    sample_count_cfg_sync->data_out_dly(nc_sample_count_cfg_sync_data_out_dly);
    osc_stable_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("osc_stable_sync"));
    osc_stable_sync->clk(osc_in);
    osc_stable_sync->rst(resetz_osc_in);
    osc_stable_sync->data_in(osc_stable);
    osc_stable_sync->scanmode(tie0_);
    osc_stable_sync->data_out(osc_stable_osc);
    osc_stable_sync->data_out_dly(nc_osc_stable_sync_data_out_dly);
    reset_done_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("reset_done_sync"));
    reset_done_sync->clk(osc_in);
    reset_done_sync->rst(resetz_osc_in);
    reset_done_sync->data_in(rst_done);
    reset_done_sync->scanmode(tie0_);
    reset_done_sync->data_out(rst_done_osc);
    reset_done_sync->data_out_dly(nc_reset_done_sync_data_out_dly);
    counter_cfg_bus_sync.reset(new mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge0_bus_width10("counter_cfg_bus_sync"));
    counter_cfg_bus_sync->clk(cfg_clk);
    counter_cfg_bus_sync->clk_inv(cfg_clk);
    counter_cfg_bus_sync->rst(rstz);
    counter_cfg_bus_sync->data_in(shadow_count_osc);
    counter_cfg_bus_sync->data_out(counter_cfg);
    SC_METHOD(proc_always_128);
    sensitive << cfg_clk.pos() << rstz.neg();
    dont_initialize();
    SC_METHOD(proc_always_138);
    sensitive << cfg_clk.pos() << rstz.neg();
    dont_initialize();
    SC_METHOD(proc_always_188);
    sensitive << cfg_clk.pos() << rstz.neg();
    dont_initialize();
    SC_METHOD(proc_always_269);
    sensitive << osc_in.pos() << resetz_osc_in.neg();
    dont_initialize();
    SC_METHOD(proc_always_304);
    sensitive << osc_in.pos() << resetz_osc_in.neg();
    dont_initialize();
    SC_METHOD(proc_always_324);
    sensitive << osc_in.pos() << resetz_osc_in.neg();
    dont_initialize();
    SC_METHOD(proc_always_355);
    sensitive << cfg_clk.pos() << rstz.neg();
    dont_initialize();
    SC_METHOD(proc_always_389);
    sensitive << cfg_clk.pos() << rstz.neg();
    dont_initialize();
    SC_METHOD(proc_always_408);
    sensitive << cfg_clk.pos() << rstz.neg();
    dont_initialize();
    SC_METHOD(proc_always_429);
    sensitive << cfg_clk.pos() << rstz.neg();
    dont_initialize();
    SC_METHOD(proc_always_449);
    sensitive << cfg_clk.pos() << rstz.neg();
    dont_initialize();
    SC_METHOD(proc_always_470);
    sensitive << cfg_clk.pos() << rstz.neg();
    dont_initialize();
    SC_METHOD(proc_always_491);
    sensitive << cfg_clk.pos() << rstz.neg();
    dont_initialize();
    SC_METHOD(proc_always_514);
    sensitive << cfg_clk.pos() << rstz.neg();
    dont_initialize();
    SC_METHOD(proc_always_531);
    sensitive << cfg_clk.pos() << rstz.neg();
    dont_initialize();
    SC_METHOD(proc_always_551);
    sensitive << cfg_clk.pos() << rstz.neg();
    dont_initialize();
    SC_METHOD(proc_always_568);
    sensitive << cfg_clk.pos() << rstz.neg();
    dont_initialize();
    SC_METHOD(proc_always_588);
    sensitive << cfg_clk.pos() << rstz.neg();
    dont_initialize();
    SC_METHOD(proc_always_612);
    sensitive << cfg_clk.pos() << rstz.neg();
    dont_initialize();
}

// Assuming start_tune is syn with cfg_clk
// Sampling start_tune
void mipi_4_rx_dphy_ns_dphynorexttune::proc_always_128() {
    if (vl::rd(rstz).truth_val() == 0) {
        vl::wr(start_tune_samp, vl::logic<2>(0x0ULL));
    } else {
        vl::wr(start_tune_samp, vl::cat(vl::rd(start_tune_samp).bit(0), vl::rd(start_tune)));
    }
}

// Added pulse generator. This FSM is posedge triggered with(start_tune)
void mipi_4_rx_dphy_ns_dphynorexttune::proc_always_138() {
    if (vl::rd(rstz).truth_val() == 0) {
        vl::wr(start_tune_edge, vl::logic<1>(0x0ULL));
    } else {
        if (vl::eq(vl::rd(start_tune_samp), vl::logic<2>(0x1ULL)).test()) {
            vl::wr(start_tune_edge, vl::logic<1>(0x1ULL));
        } else {
            vl::wr(start_tune_edge, vl::logic<1>(0x0ULL));
        }
    }
}

void mipi_4_rx_dphy_ns_dphynorexttune::proc_always_188() {
    if (vl::rd(rstz).truth_val() == 0) {
        vl::wr(fsm_state, vl::logic<3>(0x0ULL));
    } else {
        {
            const vl::logic<3> sel = vl::rd(fsm_state);
            if (vl::case_eq(sel, vl::logic<3>(0x0ULL))) {
                if (vl::rd(start_tune_edge).test()) {
                    vl::wr(fsm_state, vl::logic<3>(0x1ULL));
                } else {
                    vl::wr(fsm_state, vl::logic<3>(0x0ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<3>(0x1ULL))) {
                vl::wr(fsm_state, vl::logic<3>(0x2ULL));
            } else if (vl::case_eq(sel, vl::logic<3>(0x2ULL))) {
                if (vl::rd(osc_stable).test()) {
                    vl::wr(fsm_state, vl::logic<3>(0x3ULL));
                } else {
                    vl::wr(fsm_state, vl::logic<3>(0x2ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<3>(0x3ULL))) {
                if (vl::rd(sample_count_cfg).test()) {
                    vl::wr(fsm_state, vl::logic<3>(0x4ULL));
                } else {
                    vl::wr(fsm_state, vl::logic<3>(0x3ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<3>(0x4ULL))) {
                if (vl::rd(store_complete).test()) {
                    vl::wr(fsm_state, vl::logic<3>(0x5ULL));
                } else {
                    vl::wr(fsm_state, vl::logic<3>(0x4ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<3>(0x5ULL))) {
                if (vl::rd(rst_done).test()) {
                    vl::wr(fsm_state, vl::logic<3>(0x6ULL));
                } else {
                    vl::wr(fsm_state, vl::logic<3>(0x5ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<3>(0x6ULL))) {
                if (vl::rd(count_done).test()) {
                    vl::wr(fsm_state, vl::logic<3>(0x7ULL));
                } else {
                    vl::wr(fsm_state, vl::logic<3>(0x2ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<3>(0x7ULL))) {
                if (vl::rd(tune_done).test()) {
                    vl::wr(fsm_state, vl::logic<3>(0x0ULL));
                } else {
                    vl::wr(fsm_state, vl::logic<3>(0x7ULL));
                }
            } else {
                vl::wr(fsm_state, vl::logic<3>(0x0ULL));
            }
        }
    }
}

// gen_sync_bus #(.bus_width(3)) fsm_state_osc_bus_sync (
// .clk        ( osc_in        ),
// .clk_inv    ( osc_in        ),
// //.rst        ( rstz_osc_in   ),
// .rst        ( resetz_osc_in ),
// .data_in    ( fsm_state     ),
// .data_out   ( fsm_state_osc )
// );
// sync rst de-assertion to osc_in clock
// gen_sync_rst rstz_osc_in_rst_sync (
// .clk    ( osc_in        ),
// .rst    ( rstz          ),
// .srst   ( rstz_osc_in_noscan   )
// );
// gen_mux rstz_osc_in_mux(.z(rstz_osc_in),  .sel(scanmode), .d0(rstz_osc_in_noscan), .d1(scanrstz) );
// FSM  operating at  osc_in ( burn in clock )
void mipi_4_rx_dphy_ns_dphynorexttune::proc_always_269() {
    if (vl::rd(resetz_osc_in).truth_val() == 0) {
        vl::wr(fsm_state_osc, vl::logic<3>(0x0ULL));
    } else {
        {
            const vl::logic<3> sel_2 = vl::rd(fsm_state_osc);
            if (vl::case_eq(sel_2, vl::logic<3>(0x0ULL))) {
                if (vl::rd(osc_stable_osc).test()) {
                    vl::wr(fsm_state_osc, vl::logic<3>(0x3ULL));
                } else {
                    vl::wr(fsm_state_osc, vl::logic<3>(0x0ULL));
                }
            } else if (vl::case_eq(sel_2, vl::logic<3>(0x3ULL))) {
                if (vl::rd(sample_count_osc).test()) {
                    vl::wr(fsm_state_osc, vl::logic<3>(0x4ULL));
                } else {
                    vl::wr(fsm_state_osc, vl::logic<3>(0x3ULL));
                }
            } else if (vl::case_eq(sel_2, vl::logic<3>(0x4ULL))) {
                vl::wr(fsm_state_osc, vl::logic<3>(0x5ULL));
            } else if (vl::case_eq(sel_2, vl::logic<3>(0x5ULL))) {
                if (vl::rd(rst_done_osc).test()) {
                    vl::wr(fsm_state_osc, vl::logic<3>(0x0ULL));
                } else {
                    vl::wr(fsm_state_osc, vl::logic<3>(0x5ULL));
                }
            } else {
                vl::wr(fsm_state_osc, vl::logic<3>(0x0ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphynorexttune::proc_always_304() {
    // if(!rstz_osc_in) begin
    if (vl::rd(resetz_osc_in).truth_val() == 0) {
        vl::wr(shadow_count_osc, vl::logic<10>(0x0ULL));
    } else {
        {
            const vl::logic<3> sel_3 = vl::rd(fsm_state_osc);
            if (vl::case_eq(sel_3, vl::logic<3>(0x0ULL))) {
                vl::wr(shadow_count_osc, vl::logic<10>(0x0ULL));
            } else if (vl::case_eq(sel_3, vl::logic<3>(0x4ULL))) {
                vl::wr(shadow_count_osc, vl::rd(counter_osc));
            } else {
                vl::wr(shadow_count_osc, vl::rd(shadow_count_osc));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphynorexttune::proc_always_324() {
    // if(!rstz_osc_in) begin
    if (vl::rd(resetz_osc_in).truth_val() == 0) {
        vl::wr(counter_osc, vl::logic<10>(0x0ULL));
    } else {
        {
            const vl::logic<3> sel_4 = vl::rd(fsm_state_osc);
            if (vl::case_eq(sel_4, vl::logic<3>(0x0ULL))) {
                vl::wr(counter_osc, vl::logic<10>(0x0ULL));
            } else if (vl::case_eq(sel_4, vl::logic<3>(0x3ULL))) {
                vl::wr(counter_osc, vl::rd(counter_osc) + vl::logic<10>(0x1ULL));
            } else if (vl::case_eq(sel_4, vl::logic<3>(0x5ULL))) {
                vl::wr(counter_osc, vl::logic<10>(0x0ULL));
            } else {
                vl::wr(counter_osc, vl::rd(counter_osc));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphynorexttune::proc_always_355() {
    if (vl::rd(rstz).truth_val() == 0) {
        vl::wr(count_time, vl::logic<9>(0x0ULL));
    } else {
        {
            const vl::logic<3> sel_5 = vl::rd(fsm_state);
            if (vl::case_eq(sel_5, vl::logic<3>(0x0ULL))) {
                vl::wr(count_time, vl::logic<9>(0x0ULL));
            } else if (vl::case_eq(sel_5, vl::logic<3>(0x2ULL))) {
                if (vl::eq(vl::rd(count_time), vl::rd(stability_time)).test()) {
                    vl::wr(count_time, vl::logic<9>(0x0ULL));
                } else {
                    vl::wr(count_time, vl::rd(count_time) + vl::logic<9>(0x1ULL));
                }
            } else if (vl::case_eq(sel_5, vl::logic<3>(0x3ULL))) {
                if (vl::eq(vl::rd(count_time), vl::rd(timebase)).test()) {
                    vl::wr(count_time, vl::rd(count_time));
                } else {
                    vl::wr(count_time, vl::rd(count_time) + vl::logic<9>(0x1ULL));
                }
            } else if (vl::case_eq(sel_5, vl::logic<3>(0x5ULL))) {
                vl::wr(count_time, vl::logic<9>(0x0ULL));
            } else if (vl::case_eq(sel_5, vl::logic<3>(0x7ULL))) {
                if (vl::eq(vl::rd(count_time), vl::logic<9>(0x1fULL)).test()) {
                    vl::wr(count_time, vl::rd(count_time));
                } else {
                    vl::wr(count_time, vl::rd(count_time) + vl::logic<9>(0x1ULL));
                }
            } else {
                vl::wr(count_time, vl::rd(count_time));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphynorexttune::proc_always_389() {
    if (vl::rd(rstz).truth_val() == 0) {
        vl::wr(noext_burn_error, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<3> sel_6 = vl::rd(fsm_state);
            if (vl::case_eq(sel_6, vl::logic<3>(0x7ULL))) {
                if (vl::eq(vl::rd(count_time), vl::logic<9>(0x1fULL)).test()) {
                    vl::wr(noext_burn_error, vl::logic<1>(0x1ULL));
                } else {
                    vl::wr(noext_burn_error, vl::logic<1>(0x0ULL));
                }
            } else if (vl::case_eq(sel_6, vl::logic<3>(0x1ULL))) {
                vl::wr(noext_burn_error, vl::logic<1>(0x0ULL));
            } else {
                vl::wr(noext_burn_error, vl::rd(noext_burn_error));
            }
        }
    }
}

// 4us reached. Assertion of a flag
void mipi_4_rx_dphy_ns_dphynorexttune::proc_always_408() {
    if (vl::rd(rstz).truth_val() == 0) {
        vl::wr(sample_count, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<3> sel_7 = vl::rd(fsm_state);
            if (vl::case_eq(sel_7, vl::logic<3>(0x3ULL))) {
                if (vl::eq(vl::rd(count_time), vl::rd(timebase)).test()) {
                    vl::wr(sample_count, vl::logic<1>(0x1ULL));
                } else {
                    vl::wr(sample_count, vl::logic<1>(0x0ULL));
                }
            } else if (vl::case_eq(sel_7, vl::logic<3>(0x4ULL))) {
                vl::wr(sample_count, vl::logic<1>(0x1ULL));
            } else {
                vl::wr(sample_count, vl::logic<1>(0x0ULL));
            }
        }
    }
}

// Flag osc_stable if stability time is reached.
void mipi_4_rx_dphy_ns_dphynorexttune::proc_always_429() {
    if (vl::rd(rstz).truth_val() == 0) {
        vl::wr(osc_stable, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<3> sel_8 = vl::rd(fsm_state);
            if (vl::case_eq(sel_8, vl::logic<3>(0x2ULL))) {
                if (vl::eq(vl::rd(count_time), vl::rd(stability_time)).test()) {
                    vl::wr(osc_stable, vl::logic<1>(0x1ULL));
                } else {
                    vl::wr(osc_stable, vl::logic<1>(0x0ULL));
                }
            } else if (vl::case_eq(sel_8, vl::logic<3>(0x3ULL))) {
                vl::wr(osc_stable, vl::logic<1>(0x1ULL));
            } else {
                vl::wr(osc_stable, vl::logic<1>(0x0ULL));
            }
        }
    }
}

// store count_osc0 value
void mipi_4_rx_dphy_ns_dphynorexttune::proc_always_449() {
    if (vl::rd(rstz).truth_val() == 0) {
        vl::wr(count_osc0, vl::logic<10>(0x0ULL));
    } else {
        {
            const vl::logic<3> sel_9 = vl::rd(fsm_state);
            if (vl::case_eq(sel_9, vl::logic<3>(0x1ULL))) {
                vl::wr(count_osc0, vl::logic<10>(0x0ULL));
            } else if (vl::case_eq(sel_9, vl::logic<3>(0x4ULL))) {
                if (vl::eq(vl::rd(osc_en), vl::logic<1>(0x0ULL)).test()) {
                    vl::wr(count_osc0, vl::rd(counter_cfg));
                } else {
                    vl::wr(count_osc0, vl::rd(count_osc0));
                }
            } else {
                vl::wr(count_osc0, vl::rd(count_osc0));
            }
        }
    }
}

// store count_osc1 value
void mipi_4_rx_dphy_ns_dphynorexttune::proc_always_470() {
    if (vl::rd(rstz).truth_val() == 0) {
        vl::wr(count_osc1, vl::logic<10>(0x0ULL));
    } else {
        {
            const vl::logic<3> sel_10 = vl::rd(fsm_state);
            if (vl::case_eq(sel_10, vl::logic<3>(0x1ULL))) {
                vl::wr(count_osc1, vl::logic<10>(0x0ULL));
            } else if (vl::case_eq(sel_10, vl::logic<3>(0x4ULL))) {
                if (vl::eq(vl::rd(osc_en), vl::logic<1>(0x1ULL)).test()) {
                    vl::wr(count_osc1, vl::rd(counter_cfg));
                } else {
                    vl::wr(count_osc1, vl::rd(count_osc1));
                }
            } else {
                vl::wr(count_osc1, vl::rd(count_osc1));
            }
        }
    }
}

// Check count complete
void mipi_4_rx_dphy_ns_dphynorexttune::proc_always_491() {
    if (vl::rd(rstz).truth_val() == 0) {
        vl::wr(store_complete, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<3> sel_11 = vl::rd(fsm_state);
            if (vl::case_eq(sel_11, vl::logic<3>(0x4ULL))) {
                if (vl::eq(vl::rd(osc_en), vl::logic<1>(0x0ULL)).test()) {
                    if (vl::eq(vl::rd(count_osc0), vl::logic<10>(0x0ULL)).test()) {
                        vl::wr(store_complete, vl::logic<1>(0x0ULL));
                    } else {
                        vl::wr(store_complete, vl::logic<1>(0x1ULL));
                    }
                } else if (vl::eq(vl::rd(count_osc1), vl::logic<10>(0x0ULL)).test()) {
                    vl::wr(store_complete, vl::logic<1>(0x0ULL));
                } else {
                    vl::wr(store_complete, vl::logic<1>(0x1ULL));
                }
            } else {
                vl::wr(store_complete, vl::logic<1>(0x0ULL));
            }
        }
    }
}

// check rszt
void mipi_4_rx_dphy_ns_dphynorexttune::proc_always_514() {
    if (vl::rd(rstz).truth_val() == 0) {
        vl::wr(rst_done, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<3> sel_12 = vl::rd(fsm_state);
            if (vl::case_eq(sel_12, vl::logic<3>(0x5ULL))) {
                if (vl::eq(vl::rd(count_time), vl::logic<9>(0x0ULL)).test()) {
                    vl::wr(rst_done, vl::logic<1>(0x1ULL));
                } else {
                    vl::wr(rst_done, vl::logic<1>(0x0ULL));
                }
            } else {
                vl::wr(rst_done, vl::logic<1>(0x0ULL));
            }
        }
    }
}

// check count
void mipi_4_rx_dphy_ns_dphynorexttune::proc_always_531() {
    if (vl::rd(rstz).truth_val() == 0) {
        vl::wr(count_done, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<3> sel_13 = vl::rd(fsm_state);
            if (vl::case_eq(sel_13, vl::logic<3>(0x0ULL))) {
                vl::wr(count_done, vl::logic<1>(0x0ULL));
            } else if (vl::case_eq(sel_13, vl::logic<3>(0x5ULL))) {
                if (vl::eq(vl::rd(count_osc1), vl::logic<10>(0x0ULL)).test()) {
                    vl::wr(count_done, vl::logic<1>(0x0ULL));
                } else {
                    vl::wr(count_done, vl::logic<1>(0x1ULL));
                }
            } else {
                vl::wr(count_done, vl::rd(count_done));
            }
        }
    }
}

// set osc_en
void mipi_4_rx_dphy_ns_dphynorexttune::proc_always_551() {
    if (vl::rd(rstz).truth_val() == 0) {
        vl::wr(osc_en, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<3> sel_14 = vl::rd(fsm_state);
            if (vl::case_eq(sel_14, vl::logic<3>(0x6ULL))) {
                vl::wr(osc_en, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_14, vl::logic<3>(0x7ULL))) {
                vl::wr(osc_en, vl::logic<1>(0x0ULL));
            } else {
                vl::wr(osc_en, vl::rd(osc_en));
            }
        }
    }
}

// set osc_pon
void mipi_4_rx_dphy_ns_dphynorexttune::proc_always_568() {
    if (vl::rd(rstz).truth_val() == 0) {
        vl::wr(osc_pon, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<3> sel_15 = vl::rd(fsm_state);
            if (vl::case_eq(sel_15, vl::logic<3>(0x0ULL))) {
                if (vl::rd(start_tune_edge).test()) {
                    vl::wr(osc_pon, vl::logic<1>(0x1ULL));
                } else {
                    vl::wr(osc_pon, vl::logic<1>(0x0ULL));
                }
            } else if (vl::case_eq(sel_15, vl::logic<3>(0x7ULL))) {
                vl::wr(osc_pon, vl::logic<1>(0x0ULL));
            } else {
                vl::wr(osc_pon, vl::rd(osc_pon));
            }
        }
    }
}

// analyze counters values
void mipi_4_rx_dphy_ns_dphynorexttune::proc_always_588() {
    if (vl::rd(rstz).truth_val() == 0) {
        vl::wr(setr, vl::logic<4>(0x0ULL));
    } else {
        {
            const vl::logic<3> sel_16 = vl::rd(fsm_state);
            if (vl::case_eq(sel_16, vl::logic<3>(0x3ULL))) {
                vl::wr(setr, vl::logic<4>(0x0ULL));
            } else if (vl::case_eq(sel_16, vl::logic<3>(0x7ULL))) {
                if (vl::gt(vl::rd(count_osc0), vl::rd(count_osc1)).test()) {
                    vl::wr(setr, vl::rd(setr_max));
                } else if (vl::lt(vl::rd(count_osc0), vl::rd(count_osc1)).test()) {
                    vl::wr(setr, vl::rd(setr_min));
                } else if (vl::land(vl::eq(vl::rd(count_osc0), vl::rd(count_osc1)), vl::le(vl::rd(count_osc0), vl::rd(count_ref))).test()) {
                    vl::wr(setr, vl::rd(setr_min));
                } else {
                    vl::wr(setr, vl::rd(setr_typ));
                }
            } else {
                vl::wr(setr, vl::rd(setr));
            }
        }
    }
}

// Tune done
void mipi_4_rx_dphy_ns_dphynorexttune::proc_always_612() {
    if (vl::rd(rstz).truth_val() == 0) {
        vl::wr(tune_done, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<3> sel_17 = vl::rd(fsm_state);
            if (vl::case_eq(sel_17, vl::logic<3>(0x1ULL))) {
                vl::wr(tune_done, vl::logic<1>(0x0ULL));
            } else if (vl::case_eq(sel_17, vl::logic<3>(0x7ULL))) {
                if (vl::land(vl::eq(vl::rd(setr), vl::logic<4>(0x0ULL)), vl::eq(vl::rd(noext_burn_error), vl::logic<1>(0x0ULL))).test()) {
                    vl::wr(tune_done, vl::logic<1>(0x0ULL));
                } else {
                    vl::wr(tune_done, vl::logic<1>(0x1ULL));
                }
            } else {
                vl::wr(tune_done, vl::rd(tune_done));
            }
        }
    }
}
