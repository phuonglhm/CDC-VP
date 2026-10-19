//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_dphyifpowerctrlrx
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_dphyifpowerctrlrx.h"

mipi_4_rx_dphy_ns_dphyifpowerctrlrx::mipi_4_rx_dphy_ns_dphyifpowerctrlrx(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    tie0__drv.o(tie0_);
    gen_force_state_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_force_state_sync"));
    gen_force_state_sync->clk(cfg_clk);
    gen_force_state_sync->rst(resetz);
    gen_force_state_sync->data_in(force_state);
    gen_force_state_sync->scanmode(tie0_);
    gen_force_state_sync->data_out(force_state_sync);
    gen_force_state_sync->data_out_dly(nc_gen_force_state_sync_data_out_dly);
    target_state_sync_bus.reset(new mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge0_bus_width4("target_state_sync_bus"));
    target_state_sync_bus->clk(cfg_clk);
    target_state_sync_bus->clk_inv(cfg_clk);
    target_state_sync_bus->rst(resetz);
    target_state_sync_bus->data_in(target_state);
    target_state_sync_bus->data_out(target_state_sync);
    SC_METHOD(proc_always_271);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_373);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_389);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_404);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_418);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_432);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_449);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_464);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_480);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_490);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_502);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_517);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_534);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_543);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_555);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_567);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_581);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_592);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_605);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_621);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_636);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_647);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_668);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_686);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_705);
    sensitive << cfg_clk.pos() << resetz.neg();
    dont_initialize();
}

// Cold start FSM (controls all lanes)
void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_271() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(state_main, vl::logic<4>(0x0ULL));
    } else if (vl::rd(force_state_sync).test()) {
        vl::wr(state_main, vl::rd(target_state_sync));
    } else {
        {
            const vl::logic<4> sel = vl::rd(state_main);
            if (vl::case_eq(sel, vl::logic<4>(0x0ULL))) {
                vl::wr(state_main, vl::logic<4>(0x1ULL));
            } else if (vl::case_eq(sel, vl::logic<4>(0x1ULL))) {
                if (vl::rd(bandgapok).test()) {
                    vl::wr(state_main, vl::logic<4>(0x2ULL));
                } else {
                    vl::wr(state_main, vl::logic<4>(0x1ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0x2ULL))) {
                if (vl::rd(noext_burnin_res_cal).test()) {
                    vl::wr(state_main, vl::logic<4>(0x3ULL));
                } else {
                    vl::wr(state_main, vl::logic<4>(0x4ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0x3ULL))) {
                if (vl::rd(burnin_cal_en_ovr_en).test()) {
                    vl::wr(state_main, vl::logic<4>(0x5ULL));
                } else {
                    if (vl::rd(burnin_cal_done).test()) {
                        vl::wr(state_main, vl::logic<4>(0x5ULL));
                    } else {
                        vl::wr(state_main, vl::logic<4>(0x3ULL));
                    }
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0x4ULL))) {
                if (vl::rd(cb_cal_en_ovr_en).test()) {
                    if ((~vl::rd(mastermacro)).test()) {
                        vl::wr(state_main, vl::logic<4>(0x6ULL));
                    } else {
                        vl::wr(state_main, vl::logic<4>(0x5ULL));
                    }
                } else {
                    if (vl::rd(rescal_done).test()) {
                        if ((~vl::rd(mastermacro)).test()) {
                            vl::wr(state_main, vl::logic<4>(0x6ULL));
                        } else {
                            vl::wr(state_main, vl::logic<4>(0x5ULL));
                        }
                    } else {
                        vl::wr(state_main, vl::rd(state_main));
                    }
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0x5ULL))) {
                if (vl::rd(bypass_offset_machine).test()) {
                    vl::wr(state_main, vl::logic<4>(0x9ULL));
                } else {
                    if (vl::rd(offsetcal_clklane_done).test()) {
                        vl::wr(state_main, vl::logic<4>(0x6ULL));
                    } else {
                        vl::wr(state_main, vl::logic<4>(0x5ULL));
                    }
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0x6ULL))) {
                if (vl::rd(bypass_offset_machine).test()) {
                    vl::wr(state_main, vl::logic<4>(0x9ULL));
                } else {
                    if (vl::rd(offsetcal_lanes_done).test()) {
                        vl::wr(state_main, vl::logic<4>(0x9ULL));
                    } else {
                        vl::wr(state_main, vl::logic<4>(0x6ULL));
                    }
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0x9ULL))) {
                if (vl::rd(bypass_ddltunning_machine).test()) {
                    if (vl::rd(perform_skew).test()) {
                        // use backward compatibility mode ( 1'b1 dphy 1.1)
                        vl::wr(state_main, vl::logic<4>(0xaULL));
                    } else {
                        vl::wr(state_main, vl::logic<4>(0x7ULL));
                    }
                } else {
                    if (vl::rd(ddltunning_done).test()) {
                        if (vl::rd(perform_skew).test()) {
                            // use backward compatibility mode ( 1'b1 dphy 1.1)
                            vl::wr(state_main, vl::logic<4>(0xaULL));
                        } else {
                            vl::wr(state_main, vl::logic<4>(0x7ULL));
                        }
                    } else {
                        vl::wr(state_main, vl::logic<4>(0x9ULL));
                    }
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0xaULL))) {
                if (vl::rd(bypass_skew_machine).test()) {
                    vl::wr(state_main, vl::logic<4>(0x7ULL));
                } else {
                    if (vl::rd(skew_back_comp_done).test()) {
                        vl::wr(state_main, vl::logic<4>(0x7ULL));
                    } else {
                        vl::wr(state_main, vl::logic<4>(0xaULL));
                    }
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0x7ULL))) {
                if (vl::rd(ulpsactivenot).truth_val() == 0) {
                    vl::wr(state_main, vl::logic<4>(0x8ULL));
                } else {
                    vl::wr(state_main, vl::logic<4>(0x7ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0x8ULL))) {
                if (vl::rd(ulpsactivenot).test()) {
                    vl::wr(state_main, vl::logic<4>(0x7ULL));
                } else {
                    vl::wr(state_main, vl::logic<4>(0x8ULL));
                }
            } else {
                vl::wr(state_main, vl::logic<4>(0x0ULL));
            }
        }
    }
}

// Termination Calibration enable
void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_373() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(cb_cal_en, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_2 = vl::rd(state_main);
            if (vl::case_eq(sel_2, vl::logic<4>(0x4ULL))) {
                vl::wr(cb_cal_en, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_2, vl::logic<4>(0x2ULL))) {
                vl::wr(cb_cal_en, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_2, vl::logic<4>(0x3ULL))) {
                vl::wr(cb_cal_en, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_2, vl::logic<4>(0x5ULL))) {
                vl::wr(cb_cal_en, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_2, vl::logic<4>(0x6ULL))) {
                vl::wr(cb_cal_en, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_2, vl::logic<4>(0x9ULL))) {
                vl::wr(cb_cal_en, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_2, vl::logic<4>(0xaULL))) {
                vl::wr(cb_cal_en, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_2, vl::logic<4>(0x7ULL))) {
                vl::wr(cb_cal_en, vl::logic<1>(0x1ULL));
            } else {
                vl::wr(cb_cal_en, vl::logic<1>(0x0ULL));
            }
        }
    }
}

// Termination Calibration enable
void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_389() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(burnin_cal_en, vl::logic<1>(0x0ULL));
    } else {
        if (vl::rd(burnin_cal_en_ovr_en).test()) {
            vl::wr(burnin_cal_en, vl::logic<1>(0x0ULL));
        } else {
            {
                const vl::logic<4> sel_3 = vl::rd(state_main);
                if (vl::case_eq(sel_3, vl::logic<4>(0x3ULL))) {
                    vl::wr(burnin_cal_en, vl::logic<1>(0x1ULL));
                } else {
                    vl::wr(burnin_cal_en, vl::logic<1>(0x0ULL));
                }
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_404() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(rescal_en, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_4 = vl::rd(state_main);
            if (vl::case_eq(sel_4, vl::logic<4>(0x4ULL))) {
                if (vl::rd(cb_cal_en_ovr_en).test()) {
                    vl::wr(rescal_en, vl::logic<1>(0x0ULL));
                } else {
                    vl::wr(rescal_en, vl::logic<1>(0x1ULL));
                }
            } else {
                vl::wr(rescal_en, vl::logic<1>(0x0ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_418() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(hsrxencal, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_5 = vl::rd(state_main);
            if (vl::case_eq(sel_5, vl::logic<4>(0x5ULL))) {
                if (vl::rd(bypass_offset_machine).test()) {
                    vl::wr(hsrxencal, vl::logic<1>(0x0ULL));
                } else {
                    if (vl::ne(vl::rd(timer), vl::cat(vl::logic<2>(0x0ULL), vl::rd(rxpontime))).test()) {
                        vl::wr(hsrxencal, vl::logic<1>(0x1ULL));
                    }
                }
            } else {
                vl::wr(hsrxencal, vl::logic<1>(0x0ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_432() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(rxclk_feed_int_clk, vl::logic<1>(0x0ULL));
    } else if (vl::rd(bist_control).test()) {
        // control for loobback : BIST
        vl::wr(rxclk_feed_int_clk, vl::logic<1>(0x1ULL));
    } else {
        {
            const vl::logic<4> sel_6 = vl::rd(state_main);
            if (vl::case_eq(sel_6, vl::logic<4>(0x6ULL))) {
                vl::wr(rxclk_feed_int_clk, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_6, vl::logic<4>(0xaULL))) {
                vl::wr(rxclk_feed_int_clk, vl::logic<1>(0x1ULL));
            } else {
                vl::wr(rxclk_feed_int_clk, vl::logic<1>(0x0ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_449() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(rxlane_feed_int_clk, vl::logic<1>(0x0ULL));
    } else if (vl::rd(bist_control).test()) {
        vl::wr(rxlane_feed_int_clk, vl::logic<1>(0x1ULL));
    } else {
        {
            const vl::logic<4> sel_7 = vl::rd(state_main);
            if (vl::case_eq(sel_7, vl::logic<4>(0xaULL))) {
                vl::wr(rxlane_feed_int_clk, vl::logic<1>(0x1ULL));
            } else {
                vl::wr(rxlane_feed_int_clk, vl::logic<1>(0x0ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_464() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(hsrxencal_lane_offset, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_8 = vl::rd(state_main);
            if (vl::case_eq(sel_8, vl::logic<4>(0x6ULL))) {
                if (vl::rd(bypass_offset_machine).test()) {
                    vl::wr(hsrxencal_lane_offset, vl::logic<1>(0x0ULL));
                } else {
                    vl::wr(hsrxencal_lane_offset, vl::logic<1>(0x1ULL));
                }
            } else {
                vl::wr(hsrxencal_lane_offset, vl::logic<1>(0x0ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_480() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(des_en_for_datalane_offset_cal, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_9 = vl::rd(state_main);
            if (vl::case_eq(sel_9, vl::logic<4>(0x6ULL))) {
                vl::wr(des_en_for_datalane_offset_cal, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_9, vl::logic<4>(0xaULL))) {
                vl::wr(des_en_for_datalane_offset_cal, vl::logic<1>(0x1ULL));
            } else {
                vl::wr(des_en_for_datalane_offset_cal, vl::logic<1>(0x0ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_490() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(bandgapok, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_10 = vl::rd(state_main);
            if (vl::case_eq(sel_10, vl::logic<4>(0x1ULL))) {
                if (vl::eq(vl::rd(timer), vl::rd(timeinitbg)).test()) {
                    vl::wr(bandgapok, vl::logic<1>(0x1ULL));
                } else {
                    vl::wr(bandgapok, vl::rd(bandgapok));
                }
            } else if (vl::case_eq(sel_10, vl::logic<4>(0x8ULL))) {
                if (vl::eq(vl::rd(timer), vl::rd(timeinitbg)).test()) {
                    vl::wr(bandgapok, vl::logic<1>(0x1ULL));
                } else {
                    vl::wr(bandgapok, vl::rd(bandgapok));
                }
            } else {
                vl::wr(bandgapok, vl::logic<1>(0x0ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_502() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(bintpon, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_11 = vl::rd(state_main);
            if (vl::case_eq(sel_11, vl::logic<4>(0x0ULL))) {
                vl::wr(bintpon, vl::logic<1>(0x0ULL));
            } else if (vl::case_eq(sel_11, vl::logic<4>(0x8ULL))) {
                if (vl::rd(ulpsactivenot).test()) {
                    vl::wr(bintpon, vl::logic<1>(0x1ULL));
                } else {
                    vl::wr(bintpon, vl::logic<1>(0x0ULL));
                }
            } else {
                vl::wr(bintpon, vl::logic<1>(0x1ULL));
            }
        }
    }
}

// Lane control from IF to bring lane up to stopstate
void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_517() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(enable_lanes, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_12 = vl::rd(state_main);
            if (vl::case_eq(sel_12, vl::logic<4>(0x7ULL))) {
                if (vl::rd(enable_lanes).truth_val() == 0) {
                    if (vl::rd(lptxdin).test()) {
                        // Cold Start
                        vl::wr(enable_lanes, vl::logic<1>(0x1ULL));
                    } else if (vl::land(vl::lnot(vl::rd(lptxenpulldwn)), vl::lor(vl::lnot(vl::rd(ulpsactivenot)), vl::rd(txulpsexit))).test()) {
                        // coming from ULP
                        vl::wr(enable_lanes, vl::logic<1>(0x1ULL));
                    } else {
                        vl::wr(enable_lanes, vl::logic<1>(0x0ULL));
                    }
                } else {
                    vl::wr(enable_lanes, vl::rd(enable_lanes));
                }
            } else {
                vl::wr(enable_lanes, vl::logic<1>(0x0ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_534() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(lptxpon, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_13 = vl::rd(state_main);
            if (vl::case_eq(sel_13, vl::logic<4>(0x7ULL))) {
                vl::wr(lptxpon, vl::logic<1>(0x1ULL));
            } else {
                vl::wr(lptxpon, vl::logic<1>(0x0ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_543() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(lptxdin, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_14 = vl::rd(state_main);
            if (vl::case_eq(sel_14, vl::logic<4>(0x7ULL))) {
                if (vl::land(vl::lnot(vl::rd(lptxenpulldwn)), vl::lor(vl::rd(ulpsactivenot), ~vl::rd(txulpsexit))).test()) {
                    // lptxdin is gated with ulpsactivenot
                    vl::wr(lptxdin, vl::logic<1>(0x1ULL));
                } else {
                    vl::wr(lptxdin, vl::logic<1>(0x0ULL));
                }
            } else {
                vl::wr(lptxdin, vl::logic<1>(0x0ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_555() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(lptxena, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_15 = vl::rd(state_main);
            if (vl::case_eq(sel_15, vl::logic<4>(0x7ULL))) {
                if (vl::ge(vl::rd(timer), vl::rd(timeout)).test()) {
                    vl::wr(lptxena, vl::logic<1>(0x1ULL));
                } else {
                    vl::wr(lptxena, vl::logic<1>(0x0ULL));
                }
            } else if (vl::case_eq(sel_15, vl::logic<4>(0x8ULL))) {
                vl::wr(lptxena, vl::logic<1>(0x0ULL));
            } else {
                vl::wr(lptxena, vl::logic<1>(0x0ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_567() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(lptxenpulldwn, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_16 = vl::rd(state_main);
            if (vl::case_eq(sel_16, vl::logic<4>(0x0ULL))) {
                vl::wr(lptxenpulldwn, vl::logic<1>(0x0ULL));
            } else if (vl::case_eq(sel_16, vl::logic<4>(0x7ULL))) {
                if (vl::rd(lptxena).test()) {
                    vl::wr(lptxenpulldwn, vl::logic<1>(0x0ULL));
                } else {
                    vl::wr(lptxenpulldwn, vl::logic<1>(0x1ULL));
                }
            } else if (vl::case_eq(sel_16, vl::logic<4>(0x8ULL))) {
                vl::wr(lptxenpulldwn, vl::logic<1>(0x1ULL));
            } else {
                vl::wr(lptxenpulldwn, vl::logic<1>(0x1ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_581() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(hsrxpon_clklane, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_17 = vl::rd(state_main);
            if (vl::case_eq(sel_17, vl::logic<4>(0x5ULL))) {
                vl::wr(hsrxpon_clklane, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_17, vl::logic<4>(0x6ULL))) {
                vl::wr(hsrxpon_clklane, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_17, vl::logic<4>(0xaULL))) {
                vl::wr(hsrxpon_clklane, vl::logic<1>(0x1ULL));
            } else {
                vl::wr(hsrxpon_clklane, vl::logic<1>(0x0ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_592() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(hsrxpon_datalane, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_18 = vl::rd(state_main);
            if (vl::case_eq(sel_18, vl::logic<4>(0x6ULL))) {
                vl::wr(hsrxpon_datalane, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_18, vl::logic<4>(0x9ULL))) {
                vl::wr(hsrxpon_datalane, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_18, vl::logic<4>(0xaULL))) {
                vl::wr(hsrxpon_datalane, vl::logic<1>(0x1ULL));
            } else {
                vl::wr(hsrxpon_datalane, vl::logic<1>(0x0ULL));
            }
        }
    }
}

// DDL tunning enable
void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_605() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(ddltunning_en, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_19 = vl::rd(state_main);
            if (vl::case_eq(sel_19, vl::logic<4>(0x9ULL))) {
                if (vl::rd(bypass_ddltunning_machine).test()) {
                    vl::wr(ddltunning_en, vl::logic<1>(0x0ULL));
                } else {
                    vl::wr(ddltunning_en, vl::logic<1>(0x1ULL));
                }
            } else {
                vl::wr(ddltunning_en, vl::logic<1>(0x0ULL));
            }
        }
    }
}

// skew comp mode generation
void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_621() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(skew_back_comp_en, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_20 = vl::rd(state_main);
            if (vl::case_eq(sel_20, vl::logic<4>(0xaULL))) {
                if (vl::rd(bypass_skew_machine).test()) {
                    vl::wr(skew_back_comp_en, vl::logic<1>(0x0ULL));
                } else {
                    vl::wr(skew_back_comp_en, vl::logic<1>(0x1ULL));
                }
            } else {
                vl::wr(skew_back_comp_en, vl::logic<1>(0x0ULL));
            }
        }
    }
}

// PLL boot
void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_636() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(pll_boot_fsm, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_21 = vl::rd(state_main);
            if (vl::case_eq(sel_21, vl::logic<4>(0x1ULL))) {
                vl::wr(pll_boot_fsm, vl::logic<1>(0x0ULL));
            } else if (vl::case_eq(sel_21, vl::logic<4>(0x8ULL))) {
                vl::wr(pll_boot_fsm, vl::logic<1>(0x0ULL));
            } else {
                vl::wr(pll_boot_fsm, vl::logic<1>(0x1ULL));
            }
        }
    }
}

// mux for internal feed control
void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_647() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(rx_rxhs_prog_mux_pwron, vl::logic<1>(0x0ULL));
    } else {
        if (vl::rd(bist_control).test()) {
            vl::wr(rx_rxhs_prog_mux_pwron, vl::logic<1>(0x1ULL));
        } else {
            {
                const vl::logic<4> sel_22 = vl::rd(state_main);
                if (vl::case_eq(sel_22, vl::logic<4>(0x5ULL))) {
                    vl::wr(rx_rxhs_prog_mux_pwron, vl::logic<1>(0x1ULL));
                } else if (vl::case_eq(sel_22, vl::logic<4>(0x6ULL))) {
                    vl::wr(rx_rxhs_prog_mux_pwron, vl::logic<1>(0x1ULL));
                } else if (vl::case_eq(sel_22, vl::logic<4>(0xaULL))) {
                    vl::wr(rx_rxhs_prog_mux_pwron, vl::logic<1>(0x1ULL));
                } else {
                    vl::wr(rx_rxhs_prog_mux_pwron, vl::logic<1>(0x0ULL));
                }
            }
        }
    }
}

// when clock lane is off we need to perform datalane offset and skew backwards calibration, so some signals need to be controlled
// we must indicate this information.
void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_668() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(do_offs_and_skew_enableclk_not, vl::logic<1>(0x0ULL));
    } else {
        if (vl::rd(enableclk).test()) {
            vl::wr(do_offs_and_skew_enableclk_not, vl::logic<1>(0x0ULL));
        } else {
            {
                const vl::logic<4> sel_23 = vl::rd(state_main);
                if (vl::case_eq(sel_23, vl::logic<4>(0x6ULL))) {
                    vl::wr(do_offs_and_skew_enableclk_not, vl::logic<1>(0x1ULL));
                } else if (vl::case_eq(sel_23, vl::logic<4>(0xaULL))) {
                    vl::wr(do_offs_and_skew_enableclk_not, vl::logic<1>(0x1ULL));
                } else {
                    vl::wr(do_offs_and_skew_enableclk_not, vl::logic<1>(0x0ULL));
                }
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_686() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(timer, vl::logic<10>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_24 = vl::rd(state_main);
            if (vl::case_eq(sel_24, vl::logic<4>(0x0ULL))) {
                vl::wr(timer, vl::logic<10>(0x0ULL));
            } else if (vl::case_eq(sel_24, vl::logic<4>(0x1ULL))) {
                if (vl::ne(vl::rd(timer), vl::rd(timeout)).test()) {
                    vl::wr(timer, vl::rd(timer) + vl::logic<10>(0x1ULL));
                }
            } else if (vl::case_eq(sel_24, vl::logic<4>(0x5ULL))) {
                if (vl::ne(vl::rd(timer), vl::cat(vl::logic<2>(0x0ULL), vl::rd(rxpontime))).test()) {
                    vl::wr(timer, vl::rd(timer) + vl::logic<10>(0x1ULL));
                } else if (vl::rd(offsetcal_clklane_done).test()) {
                    vl::wr(timer, vl::logic<10>(0x0ULL));
                } else {
                    vl::wr(timer, vl::cat(vl::logic<2>(0x0ULL), vl::rd(rxpontime)));
                }
            } else if (vl::case_eq(sel_24, vl::logic<4>(0x7ULL))) {
                if (vl::ne(vl::rd(timer), vl::rd(timeout)).test()) {
                    vl::wr(timer, vl::rd(timer) + vl::logic<10>(0x1ULL));
                } else {
                    vl::wr(timer, vl::rd(timeout));
                }
            } else if (vl::case_eq(sel_24, vl::logic<4>(0x8ULL))) {
                vl::wr(timer, vl::logic<10>(0x0ULL));
            } else {
                vl::wr(timer, vl::logic<10>(0x0ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphyifpowerctrlrx::proc_always_705() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(timeout, vl::logic<10>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_25 = vl::rd(state_main);
            if (vl::case_eq(sel_25, vl::logic<4>(0x1ULL))) {
                vl::wr(timeout, vl::rd(timeinitbg) + vl::rd(txdindlytime));
            } else if (vl::case_eq(sel_25, vl::logic<4>(0x4ULL))) {
                vl::wr(timeout, vl::rd(txstarttime) + vl::rd(txdindlytime));
            } else if (vl::case_eq(sel_25, vl::logic<4>(0x5ULL))) {
                vl::wr(timeout, vl::cat(vl::logic<2>(0x0ULL), vl::rd(rxpontime)) + vl::rd(txdindlytime));
            } else if (vl::case_eq(sel_25, vl::logic<4>(0x9ULL))) {
                vl::wr(timeout, vl::rd(txstarttime) + vl::rd(txdindlytime));
            } else if (vl::case_eq(sel_25, vl::logic<4>(0x7ULL))) {
                vl::wr(timeout, vl::rd(timeout));
            } else if (vl::case_eq(sel_25, vl::logic<4>(0x8ULL))) {
                vl::wr(timeout, vl::rd(twakeupulpm) + vl::rd(txdindlytime));
            } else {
                vl::wr(timeout, vl::rd(txstarttime) + vl::rd(txdindlytime));
            }
        }
    }
}
