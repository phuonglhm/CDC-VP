//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_mpll_startup
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_mpll_startup.h"

mipi_4_rx_dphy_ns_mpll_startup::mipi_4_rx_dphy_ns_mpll_startup(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    tie0__drv.o(tie0_);
    SC_METHOD(assign_clk_scanable);
    sensitive << cfgclk;
    SC_METHOD(assign_rstz_scanable);
    sensitive << rstz;
    // synchronization ( assumes onPLL is completly asynchronous of cfgclk)
    gen_onpll_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_onpll_sync"));
    gen_onpll_sync->clk(clk_scanable);
    gen_onpll_sync->rst(rstz_scanable);
    gen_onpll_sync->data_in(onpll);
    gen_onpll_sync->scanmode(tie0_);
    gen_onpll_sync->data_out(onpll_sync);
    gen_onpll_sync->data_out_dly(nc_gen_onpll_sync_data_out_dly);
    // Data coming from  testability (other clock domain : ensure good bus synchonization)
    sync_force_state_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("sync_force_state_sync"));
    sync_force_state_sync->clk(clk_scanable);
    sync_force_state_sync->rst(rstz_scanable);
    sync_force_state_sync->data_in(force_state);
    sync_force_state_sync->scanmode(tie0_);
    sync_force_state_sync->data_out(force_state_sync);
    sync_force_state_sync->data_out_dly(nc_sync_force_state_sync_data_out_dly);
    main_fsm_sync_bus.reset(new mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge0_bus_width4("main_fsm_sync_bus"));
    main_fsm_sync_bus->clk(clk_scanable);
    main_fsm_sync_bus->clk_inv(clk_scanable);
    main_fsm_sync_bus->rst(rstz_scanable);
    main_fsm_sync_bus->data_in(main_fsm_state);
    main_fsm_sync_bus->data_out(main_fsm_state_int);
    SC_METHOD(proc_always_135);
    sensitive << clk_scanable.posedge_event() << rstz_scanable.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_197);
    sensitive << clk_scanable.posedge_event() << rstz_scanable.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_225);
    sensitive << clk_scanable.posedge_event() << rstz_scanable.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_238);
    sensitive << clk_scanable.posedge_event() << rstz_scanable.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_249);
    sensitive << clk_scanable.posedge_event() << rstz_scanable.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_257);
    sensitive << clk_scanable.posedge_event() << rstz_scanable.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_267);
    sensitive << clk_scanable.posedge_event() << rstz_scanable.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_280);
    sensitive << clk_scanable.posedge_event() << rstz_scanable.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_289);
    sensitive << clk_scanable.posedge_event() << rstz_scanable.negedge_event();
    dont_initialize();
}

// Muxing for scan control
// gen_mux hand_clk_mux(.z(clk_scanable),  .sel(scanmode), .d0(cfgclk), .d1(scanclk));
// gen_mux hand_rst_mux(.z(rstz_scanable), .sel(scanmode), .d0(rstz),   .d1(scanrstz));
void mipi_4_rx_dphy_ns_mpll_startup::assign_clk_scanable() {
    vl::wr(clk_scanable, vl::rd(cfgclk));
}

void mipi_4_rx_dphy_ns_mpll_startup::assign_rstz_scanable() {
    vl::wr(rstz_scanable, vl::rd(rstz));
}

// Main FSM
void mipi_4_rx_dphy_ns_mpll_startup::proc_always_135() {
    if (vl::rd(rstz_scanable).truth_val() == 0) {
        vl::wr(main_state, vl::logic<4>(0x0ULL));
    } else {
        if (vl::rd(force_state_sync).test()) {
            // load the state you wanto to go if that is the case
            vl::wr(main_state, vl::rd(main_fsm_state_int));
        } else {
            {
                const vl::logic<4> sel = vl::rd(main_state);
                if (vl::case_eq(sel, vl::logic<4>(0x0ULL))) {
                    if (vl::rd(onpll_sync).truth_val() == 0) {
                        vl::wr(main_state, vl::logic<4>(0x0ULL));
                    } else {
                        vl::wr(main_state, vl::logic<4>(0x1ULL));
                    }
                } else if (vl::case_eq(sel, vl::logic<4>(0x1ULL))) {
                    if (vl::rd(onpll_sync).truth_val() == 0) {
                        vl::wr(main_state, vl::logic<4>(0x0ULL));
                    } else {
                        vl::wr(main_state, vl::logic<4>(0x2ULL));
                    }
                } else if (vl::case_eq(sel, vl::logic<4>(0x2ULL))) {
                    if (vl::rd(onpll_sync).truth_val() == 0) {
                        vl::wr(main_state, vl::logic<4>(0x0ULL));
                    } else if (vl::rd(change_state).test()) {
                        vl::wr(main_state, vl::logic<4>(0x3ULL));
                    } else {
                        vl::wr(main_state, vl::logic<4>(0x2ULL));
                    }
                } else if (vl::case_eq(sel, vl::logic<4>(0x3ULL))) {
                    if (vl::rd(onpll_sync).truth_val() == 0) {
                        vl::wr(main_state, vl::logic<4>(0x0ULL));
                    } else {
                        vl::wr(main_state, vl::logic<4>(0x4ULL));
                    }
                } else if (vl::case_eq(sel, vl::logic<4>(0x4ULL))) {
                    if (vl::rd(onpll_sync).truth_val() == 0) {
                        vl::wr(main_state, vl::logic<4>(0x0ULL));
                    } else if (vl::rd(change_state).test()) {
                        vl::wr(main_state, vl::logic<4>(0x5ULL));
                    } else {
                        vl::wr(main_state, vl::logic<4>(0x4ULL));
                    }
                } else if (vl::case_eq(sel, vl::logic<4>(0x5ULL))) {
                    if (vl::rd(onpll_sync).truth_val() == 0) {
                        vl::wr(main_state, vl::logic<4>(0x0ULL));
                    } else {
                        vl::wr(main_state, vl::logic<4>(0x6ULL));
                    }
                } else if (vl::case_eq(sel, vl::logic<4>(0x6ULL))) {
                    if (vl::rd(onpll_sync).truth_val() == 0) {
                        vl::wr(main_state, vl::logic<4>(0x0ULL));
                    } else if (vl::rd(change_state).test()) {
                        vl::wr(main_state, vl::logic<4>(0x7ULL));
                    } else {
                        vl::wr(main_state, vl::logic<4>(0x6ULL));
                    }
                } else if (vl::case_eq(sel, vl::logic<4>(0x7ULL))) {
                    if (vl::rd(onpll_sync).truth_val() == 0) {
                        vl::wr(main_state, vl::logic<4>(0x0ULL));
                    } else {
                        vl::wr(main_state, vl::logic<4>(0x8ULL));
                    }
                } else if (vl::case_eq(sel, vl::logic<4>(0x8ULL))) {
                    if (vl::rd(onpll_sync).truth_val() == 0) {
                        vl::wr(main_state, vl::logic<4>(0x0ULL));
                    } else {
                        vl::wr(main_state, vl::logic<4>(0x8ULL));
                    }
                } else {
                    vl::wr(main_state, vl::logic<4>(0x0ULL));
                }
            }
        }
    }
}

// Counter for MPLL startup
void mipi_4_rx_dphy_ns_mpll_startup::proc_always_197() {
    if (vl::rd(rstz_scanable).truth_val() == 0) {
        vl::wr(counter, vl::logic<9>(0x0ULL));
        vl::wr(change_state, vl::logic<1>(0x0ULL));
    } else {
        if (vl::rd(onpll_sync).test()) {
            if (vl::rd(counter_run).test()) {
                if (vl::eq(vl::rd(counter), vl::rd(target)).test()) {
                    vl::wr(change_state, vl::logic<1>(0x1ULL));
                    vl::wr(counter, vl::logic<9>(0x0ULL));
                } else {
                    vl::wr(counter, vl::rd(counter) + vl::logic<9>(0x1ULL));
                    vl::wr(change_state, vl::logic<1>(0x0ULL));
                }
            } else {
                vl::wr(counter, vl::logic<9>(0x0ULL));
                vl::wr(change_state, vl::logic<1>(0x0ULL));
            }
        } else {
            vl::wr(counter, vl::logic<9>(0x0ULL));
            vl::wr(change_state, vl::logic<1>(0x0ULL));
        }
    }
}

void mipi_4_rx_dphy_ns_mpll_startup::proc_always_225() {
    if (vl::rd(rstz_scanable).truth_val() == 0) {
        vl::wr(counter_run, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_2 = vl::rd(main_state);
            if (vl::case_eq(sel_2, vl::logic<4>(0x2ULL))) {
                vl::wr(counter_run, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_2, vl::logic<4>(0x3ULL))) {
                vl::wr(counter_run, vl::logic<1>(0x0ULL));
            } else if (vl::case_eq(sel_2, vl::logic<4>(0x4ULL))) {
                vl::wr(counter_run, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_2, vl::logic<4>(0x5ULL))) {
                vl::wr(counter_run, vl::logic<1>(0x0ULL));
            } else if (vl::case_eq(sel_2, vl::logic<4>(0x6ULL))) {
                vl::wr(counter_run, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_2, vl::logic<4>(0x7ULL))) {
                vl::wr(counter_run, vl::logic<1>(0x0ULL));
            } else {
                vl::wr(counter_run, vl::logic<1>(0x0ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_mpll_startup::proc_always_238() {
    if (vl::rd(rstz_scanable).truth_val() == 0) {
        vl::wr(target, vl::logic<9>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_3 = vl::rd(main_state);
            if (vl::case_eq(sel_3, vl::logic<4>(0x2ULL))) {
                vl::wr(target, vl::rd(mpll_rst_time));
            } else if (vl::case_eq(sel_3, vl::logic<4>(0x4ULL))) {
                vl::wr(target, vl::rd(mpll_gear_shift_time));
            } else if (vl::case_eq(sel_3, vl::logic<4>(0x6ULL))) {
                vl::wr(target, vl::rd(mpll_lock_det_time));
            } else {
                vl::wr(target, vl::rd(mpll_rst_time));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_mpll_startup::proc_always_249() {
    if (vl::rd(rstz_scanable).truth_val() == 0) {
        vl::wr(mpll_pwron, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_4 = vl::rd(main_state);
            if (vl::case_eq(sel_4, vl::logic<4>(0x0ULL))) {
                vl::wr(mpll_pwron, vl::logic<1>(0x0ULL));
            } else {
                vl::wr(mpll_pwron, vl::logic<1>(0x1ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_mpll_startup::proc_always_257() {
    if (vl::rd(rstz_scanable).truth_val() == 0) {
        vl::wr(mpll_reset, vl::logic<1>(0x1ULL));
    } else {
        {
            const vl::logic<4> sel_5 = vl::rd(main_state);
            if (vl::case_eq(sel_5, vl::logic<4>(0x0ULL))) {
                vl::wr(mpll_reset, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_5, vl::logic<4>(0x1ULL))) {
                vl::wr(mpll_reset, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_5, vl::logic<4>(0x2ULL))) {
                vl::wr(mpll_reset, vl::logic<1>(0x1ULL));
            } else {
                vl::wr(mpll_reset, vl::logic<1>(0x0ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_mpll_startup::proc_always_267() {
    if (vl::rd(rstz_scanable).truth_val() == 0) {
        vl::wr(mpll_gear_shift, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_6 = vl::rd(main_state);
            if (vl::case_eq(sel_6, vl::logic<4>(0x0ULL))) {
                vl::wr(mpll_gear_shift, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_6, vl::logic<4>(0x1ULL))) {
                vl::wr(mpll_gear_shift, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_6, vl::logic<4>(0x2ULL))) {
                vl::wr(mpll_gear_shift, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_6, vl::logic<4>(0x3ULL))) {
                vl::wr(mpll_gear_shift, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_6, vl::logic<4>(0x4ULL))) {
                vl::wr(mpll_gear_shift, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_6, vl::logic<4>(0x5ULL))) {
                vl::wr(mpll_gear_shift, vl::logic<1>(0x0ULL));
            } else {
                vl::wr(mpll_gear_shift, vl::logic<1>(0x0ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_mpll_startup::proc_always_280() {
    if (vl::rd(rstz_scanable).truth_val() == 0) {
        vl::wr(lock_det_on, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_7 = vl::rd(main_state);
            if (vl::case_eq(sel_7, vl::logic<4>(0x7ULL))) {
                vl::wr(lock_det_on, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_7, vl::logic<4>(0x8ULL))) {
                vl::wr(lock_det_on, vl::logic<1>(0x1ULL));
            } else {
                vl::wr(lock_det_on, vl::logic<1>(0x0ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_mpll_startup::proc_always_289() {
    if (vl::rd(rstz_scanable).truth_val() == 0) {
        vl::wr(fsm_done, vl::logic<1>(0x0ULL));
    } else if (vl::eq(vl::rd(main_state), vl::logic<4>(0x8ULL)).test()) {
        vl::wr(fsm_done, vl::logic<1>(0x1ULL));
    } else {
        vl::wr(fsm_done, vl::logic<1>(0x0ULL));
    }
}
