//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_dphyhsrxoffscal
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_dphyhsrxoffscal.h"

mipi_4_rx_dphy_ns_dphyhsrxoffscal::mipi_4_rx_dphy_ns_dphyhsrxoffscal(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_hsrxdouta);
    sensitive << hsrxdouta;
    SC_METHOD(assign_cfg_clk_scanable);
    sensitive << cfg_clk;
    SC_METHOD(assign_sresetz_cfg_clk);
    sensitive << resetz;
    SC_METHOD(assign_setoffs);
    sensitive << setoffs_ovr_en << setoffs_ovr << setoffs_sign_adjust;
    SC_METHOD(assign_setoffs_sign_adjust);
    sensitive << setoffs_int;
    SC_METHOD(assign_hsrxdouta_sync2);
    sensitive << hsrxdouta_sync2_w[0] << hsrxdouta_sync2_w[1];
    SC_METHOD(assign_hsrxdouta_sync2_upd);
    sensitive << assign_hsrxdouta_sync2_ev;
    dont_initialize();
    SC_METHOD(assign_aux_sum);
    sensitive << offstgglup0 << offstgglup1 << offstggldw0 << offstggldw1;
    SC_METHOD(glue_cfg_clk_div4_scanmux_d0);
    sensitive << cfg_clk_counter;
    // assign cfg_clk_div4_scanable = scanmode ? scanclk : cfg_clk_counter[1] ;
    cfg_clk_div4_scanmux.reset(new mipi_4_rx_dphy_ns_gen_mux("cfg_clk_div4_scanmux"));
    cfg_clk_div4_scanmux->z(cfg_clk_div4_scanable);
    cfg_clk_div4_scanmux->sel(scanmode);
    cfg_clk_div4_scanmux->d0(c_cfg_clk_div4_scanmux_d0);
    cfg_clk_div4_scanmux->d1(scanclk);
    hand_hsrxdouta0_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("hand_hsrxdouta0_sync"));
    hand_hsrxdouta0_sync->clk(cfg_clk_scanable);
    hand_hsrxdouta0_sync->rst(sresetz_cfg_clk);
    hand_hsrxdouta0_sync->data_in(hsrxdouta_b[0]);
    hand_hsrxdouta0_sync->scanmode(scanmode);
    hand_hsrxdouta0_sync->data_out(hsrxdouta_sync2_w[0]);
    hand_hsrxdouta0_sync->data_out_dly(nc_hand_hsrxdouta0_sync_data_out_dly);
    hand_hsrxdouta1_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("hand_hsrxdouta1_sync"));
    hand_hsrxdouta1_sync->clk(cfg_clk_scanable);
    hand_hsrxdouta1_sync->rst(sresetz_cfg_clk);
    hand_hsrxdouta1_sync->data_in(hsrxdouta_b[1]);
    hand_hsrxdouta1_sync->scanmode(scanmode);
    hand_hsrxdouta1_sync->data_out(hsrxdouta_sync2_w[1]);
    hand_hsrxdouta1_sync->data_out_dly(nc_hand_hsrxdouta1_sync_data_out_dly);
    SC_METHOD(proc_always_134);
    sensitive << cfg_clk_scanable.posedge_event() << sresetz_cfg_clk.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_159);
    sensitive << cfg_clk_div4_scanable.posedge_event() << sresetz_cfg_clk.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_196);
    sensitive << cfg_clk_div4_scanable.posedge_event() << sresetz_cfg_clk.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_206);
    sensitive << cfg_clk_div4_scanable.posedge_event() << sresetz_cfg_clk.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_216);
    sensitive << cfg_clk_div4_scanable.posedge_event() << sresetz_cfg_clk.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_227);
    sensitive << cfg_clk_div4_scanable.posedge_event() << sresetz_cfg_clk.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_258);
    sensitive << cfg_clk_div4_scanable.posedge_event() << sresetz_cfg_clk.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_275);
    sensitive << cfg_clk_div4_scanable.posedge_event() << sresetz_cfg_clk.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_307);
    sensitive << cfg_clk_div4_scanable.posedge_event() << sresetz_cfg_clk.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_353);
    sensitive << cfg_clk_div4_scanable.posedge_event() << sresetz_cfg_clk.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_360);
    sensitive << cfg_clk_div4_scanable.posedge_event() << sresetz_cfg_clk.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_367);
    sensitive << cfg_clk_div4_scanable.posedge_event() << sresetz_cfg_clk.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_373);
    sensitive << cfg_clk_div4_scanable.posedge_event() << sresetz_cfg_clk.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_379);
    sensitive << cfg_clk_div4_scanable.posedge_event() << sresetz_cfg_clk.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_388);
    sensitive << cfg_clk_div4_scanable.posedge_event() << sresetz_cfg_clk.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_397);
    sensitive << cfg_clk_div4_scanable.posedge_event() << sresetz_cfg_clk.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_406);
    sensitive << cfg_clk_div4_scanable.posedge_event() << sresetz_cfg_clk.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_421);
    sensitive << cfg_clk_div4_scanable.posedge_event() << sresetz_cfg_clk.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_443);
    sensitive << cfg_clk_div4_scanable.posedge_event() << sresetz_cfg_clk.negedge_event();
    dont_initialize();
}

void mipi_4_rx_dphy_ns_dphyhsrxoffscal::split_hsrxdouta() {
    const vl::logic<2> v = vl::rd(hsrxdouta);
    hsrxdouta_b[0].write(v.bit(0).sl());
    hsrxdouta_b[1].write(v.bit(1).sl());
}

vl::logic<2> mipi_4_rx_dphy_ns_dphyhsrxoffscal::rd_hsrxdouta() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(hsrxdouta_b[0]));
    v.set_bit(1, vl::rd(hsrxdouta_b[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_dphyhsrxoffscal::rd_hsrxdouta_sync2_w() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(hsrxdouta_sync2_w[0]));
    v.set_bit(1, vl::rd(hsrxdouta_sync2_w[1]));
    return v;
}

// assign arstz_scanable        = scanmode ? scanrstz : resetz  ;
// assign cfg_clk_scanable      = scanmode ? scanclk  : cfg_clk ;
void mipi_4_rx_dphy_ns_dphyhsrxoffscal::assign_cfg_clk_scanable() {
    vl::wr(cfg_clk_scanable, vl::rd(cfg_clk));
}

// gen_sync_rst hand_syncrstz_cfg_clk_sync (
// .clk            ( cfg_clk_scanable          ),
// .rst            ( arstz_scanable            ),
// .srst           ( syncrstz_cfg_clk          )
// );
// assign sresetz_cfg_clk       =  scanmode ?  scanrstz : syncrstz_cfg_clk ;
void mipi_4_rx_dphy_ns_dphyhsrxoffscal::assign_sresetz_cfg_clk() {
    vl::wr(sresetz_cfg_clk, vl::rd(resetz));
}

void mipi_4_rx_dphy_ns_dphyhsrxoffscal::assign_setoffs() {
    vl::wr(setoffs, vl::mux(vl::rd(setoffs_ovr_en), vl::rd(setoffs_ovr), vl::rd(setoffs_sign_adjust)));
}

void mipi_4_rx_dphy_ns_dphyhsrxoffscal::assign_setoffs_sign_adjust() {
    vl::wr(setoffs_sign_adjust, vl::mux(vl::rd(setoffs_int).bit(5), vl::rd(setoffs_int), vl::cat(vl::rd(setoffs_int).bit(5), ~vl::rd(setoffs_int).slice<5>(0))));
}

void mipi_4_rx_dphy_ns_dphyhsrxoffscal::assign_hsrxdouta_sync2() {
    const vl::logic<2> nv = rd_hsrxdouta_sync2_w();
    const sc_core::sc_time d = vl::dly(0.1, TU_, TP_);
    if (nv == assign_hsrxdouta_sync2_pv && !assign_hsrxdouta_sync2_ev_pend) return;
    assign_hsrxdouta_sync2_pv = nv;
    assign_hsrxdouta_sync2_ev.cancel();
    assign_hsrxdouta_sync2_ev_pend = true;
    assign_hsrxdouta_sync2_ev.notify(d);
}

void mipi_4_rx_dphy_ns_dphyhsrxoffscal::assign_hsrxdouta_sync2_upd() {
    assign_hsrxdouta_sync2_ev_pend = false;
    vl::wr(hsrxdouta_sync2, assign_hsrxdouta_sync2_pv);
}

// The + 8'd2 in the average calculation is to round up the final value.
// For example if average is 25.5 or 25.75 the word to be sent to AFE should be 26 and not 25.
void mipi_4_rx_dphy_ns_dphyhsrxoffscal::assign_aux_sum() {
    vl::wr(aux_sum, (((vl::cat(vl::logic<2>(0x0ULL), vl::rd(offstgglup0)) + vl::cat(vl::logic<2>(0x0ULL), vl::rd(offstgglup1))) + vl::cat(vl::logic<2>(0x0ULL), vl::rd(offstggldw0))) + vl::cat(vl::logic<2>(0x0ULL), vl::rd(offstggldw1))) + vl::logic<8>(0x2ULL));
}

void mipi_4_rx_dphy_ns_dphyhsrxoffscal::glue_cfg_clk_div4_scanmux_d0() { vl::wr(c_cfg_clk_div4_scanmux_d0, vl::rd(cfg_clk_counter).bit(1)); }

// CFG_CLK clock generation - 1/8 of CFG_CLK Clock
void mipi_4_rx_dphy_ns_dphyhsrxoffscal::proc_always_134() {
    if (vl::rd(sresetz_cfg_clk).truth_val() == 0) {
        vl::wr(cfg_clk_counter, vl::logic<2>(0x0ULL));
    } else {
        vl::wr(cfg_clk_counter, vl::rd(cfg_clk_counter) + vl::logic<2>(0x1ULL));
    }
}

void mipi_4_rx_dphy_ns_dphyhsrxoffscal::proc_always_159() {
    if (vl::rd(sresetz_cfg_clk).truth_val() == 0) {
        vl::wr(state, vl::logic<4>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel = vl::rd(state);
            if (vl::case_eq(sel, vl::logic<4>(0x0ULL))) {
                if (vl::land(vl::rd(encal), vl::lnot(vl::rd(setoffs_ovr_en))).test()) {
                    vl::wr(state, vl::logic<4>(0x1ULL));
                } else {
                    vl::wr(state, vl::logic<4>(0x0ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0x1ULL))) {
                if (vl::rd(setoffs_ovr_en).test()) {
                    vl::wr(state, vl::logic<4>(0x0ULL));
                } else if (vl::eq(vl::rd(masksr), vl::logic<3>(0x5ULL)).test()) {
                    vl::wr(state, vl::logic<4>(0x8ULL));
                } else {
                    vl::wr(state, vl::logic<4>(0x1ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0x8ULL))) {
                if (vl::eq(vl::rd(masksr), vl::logic<3>(0x5ULL)).test()) {
                    vl::wr(state, vl::logic<4>(0x2ULL));
                } else {
                    vl::wr(state, vl::logic<4>(0x8ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0x2ULL))) {
                if (vl::rd(setoffs_ovr_en).test()) {
                    vl::wr(state, vl::logic<4>(0x0ULL));
                } else if (vl::eq(vl::rd(setoffs_int), vl::logic<6>(0x0ULL)).test()) {
                    vl::wr(state, vl::logic<4>(0x5ULL));
                } else {
                    vl::wr(state, vl::logic<4>(0x2ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0x5ULL))) {
                if (vl::eq(vl::rd(masksr), vl::logic<3>(0x3ULL)).test()) {
                    vl::wr(state, vl::logic<4>(0x6ULL));
                } else {
                    vl::wr(state, vl::logic<4>(0x5ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0x6ULL))) {
                vl::wr(state, vl::logic<4>(0x3ULL));
            } else if (vl::case_eq(sel, vl::logic<4>(0x3ULL))) {
                if (vl::rd(setoffs_ovr_en).test()) {
                    vl::wr(state, vl::logic<4>(0x0ULL));
                } else if (vl::eq(vl::rd(setoffs_int), vl::logic<6>(0x3fULL)).test()) {
                    vl::wr(state, vl::logic<4>(0x7ULL));
                } else {
                    vl::wr(state, vl::logic<4>(0x3ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0x7ULL))) {
                if (vl::eq(vl::rd(masksr), vl::logic<3>(0x3ULL)).test()) {
                    vl::wr(state, vl::logic<4>(0x4ULL));
                } else {
                    vl::wr(state, vl::logic<4>(0x7ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0x4ULL))) {
                if (vl::rd(setoffs_ovr_en).test()) {
                    vl::wr(state, vl::logic<4>(0x0ULL));
                } else if (vl::rd(encal).truth_val() == 0) {
                    vl::wr(state, vl::logic<4>(0x0ULL));
                } else {
                    vl::wr(state, vl::logic<4>(0x4ULL));
                }
            } else {
                vl::wr(state, vl::logic<4>(0x0ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphyhsrxoffscal::proc_always_196() {
    if (vl::rd(sresetz_cfg_clk).truth_val() == 0) {
        vl::wr(calrdy, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<1> sel_2 = vl::rd(calrdy);
            if (vl::case_eq(sel_2, vl::logic<1>(0x0ULL))) {
                if (vl::eq(vl::rd(state), vl::logic<4>(0x4ULL)).test()) {
                    vl::wr(calrdy, vl::logic<1>(0x1ULL));
                } else {
                    vl::wr(calrdy, vl::logic<1>(0x0ULL));
                }
            } else if (vl::case_eq(sel_2, vl::logic<1>(0x1ULL))) {
                if (vl::lor(vl::land(vl::eq(vl::rd(state), vl::logic<4>(0x0ULL)), vl::rd(encal)), vl::rd(setoffs_ovr_en)).test()) {
                    vl::wr(calrdy, vl::logic<1>(0x0ULL));
                } else {
                    vl::wr(calrdy, vl::logic<1>(0x1ULL));
                }
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphyhsrxoffscal::proc_always_206() {
    if (vl::rd(sresetz_cfg_clk).truth_val() == 0) {
        vl::wr(calrdy_d, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<1> sel_3 = vl::rd(calrdy_d);
            if (vl::case_eq(sel_3, vl::logic<1>(0x0ULL))) {
                if (vl::rd(calrdy).test()) {
                    vl::wr(calrdy_d, vl::logic<1>(0x1ULL));
                } else {
                    vl::wr(calrdy_d, vl::logic<1>(0x0ULL));
                }
            } else if (vl::case_eq(sel_3, vl::logic<1>(0x1ULL))) {
                if (vl::lor(vl::land(vl::eq(vl::rd(state), vl::logic<4>(0x0ULL)), vl::rd(encal)), vl::rd(setoffs_ovr_en)).test()) {
                    vl::wr(calrdy_d, vl::logic<1>(0x0ULL));
                } else {
                    vl::wr(calrdy_d, vl::logic<1>(0x1ULL));
                }
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphyhsrxoffscal::proc_always_216() {
    if (vl::rd(sresetz_cfg_clk).truth_val() == 0) {
        vl::wr(caldone, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<1> sel_4 = vl::rd(caldone);
            if (vl::case_eq(sel_4, vl::logic<1>(0x0ULL))) {
                if (vl::rd(calrdy_d).test()) {
                    vl::wr(caldone, vl::logic<1>(0x1ULL));
                } else {
                    vl::wr(caldone, vl::logic<1>(0x0ULL));
                }
            } else if (vl::case_eq(sel_4, vl::logic<1>(0x1ULL))) {
                if (vl::lor(vl::land(vl::eq(vl::rd(state), vl::logic<4>(0x0ULL)), vl::rd(encal)), vl::rd(setoffs_ovr_en)).test()) {
                    vl::wr(caldone, vl::logic<1>(0x0ULL));
                } else {
                    vl::wr(caldone, vl::logic<1>(0x1ULL));
                }
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphyhsrxoffscal::proc_always_227() {
    if (vl::rd(sresetz_cfg_clk).truth_val() == 0) {
        vl::wr(startcal, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_5 = vl::rd(state);
            if (vl::case_eq(sel_5, vl::logic<4>(0x0ULL))) {
                vl::wr(startcal, vl::logic<1>(0x0ULL));
            } else if (vl::case_eq(sel_5, vl::logic<4>(0x1ULL))) {
                vl::wr(startcal, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_5, vl::logic<4>(0x8ULL))) {
                vl::wr(startcal, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_5, vl::logic<4>(0x2ULL))) {
                vl::wr(startcal, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_5, vl::logic<4>(0x5ULL))) {
                vl::wr(startcal, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_5, vl::logic<4>(0x6ULL))) {
                vl::wr(startcal, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_5, vl::logic<4>(0x3ULL))) {
                vl::wr(startcal, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_5, vl::logic<4>(0x7ULL))) {
                vl::wr(startcal, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_5, vl::logic<4>(0x4ULL))) {
                vl::wr(startcal, vl::logic<1>(0x0ULL));
            } else {
                vl::wr(startcal, vl::logic<1>(0x0ULL));
            }
        }
    }
}

// if(!sresetz_cfg_clk)
// rampupdwnz <= 1'b0 ;
// else case(rampupdwnz)
// 1'b0: if(setoffs_int == 6'b000000) rampupdwnz <= 1'b1;
// else rampupdwnz <= 1'b0;
// 1'b1: if(setoffs_int == 6'b111111) rampupdwnz <= 1'b0;
// else rampupdwnz <= 1'b1;
// endcase
void mipi_4_rx_dphy_ns_dphyhsrxoffscal::proc_always_258() {
    if (vl::rd(sresetz_cfg_clk).truth_val() == 0) {
        vl::wr(rampupdwnz, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_6 = vl::rd(state);
            if (vl::case_eq(sel_6, vl::logic<4>(0x0ULL))) {
                vl::wr(rampupdwnz, vl::logic<1>(0x0ULL));
            } else if (vl::case_eq(sel_6, vl::logic<4>(0x1ULL))) {
                vl::wr(rampupdwnz, vl::logic<1>(0x0ULL));
            } else if (vl::case_eq(sel_6, vl::logic<4>(0x8ULL))) {
                vl::wr(rampupdwnz, vl::logic<1>(0x0ULL));
            } else if (vl::case_eq(sel_6, vl::logic<4>(0x2ULL))) {
                vl::wr(rampupdwnz, vl::logic<1>(0x0ULL));
            } else if (vl::case_eq(sel_6, vl::logic<4>(0x5ULL))) {
                vl::wr(rampupdwnz, vl::logic<1>(0x0ULL));
            } else if (vl::case_eq(sel_6, vl::logic<4>(0x6ULL))) {
                vl::wr(rampupdwnz, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_6, vl::logic<4>(0x3ULL))) {
                vl::wr(rampupdwnz, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_6, vl::logic<4>(0x7ULL))) {
                vl::wr(rampupdwnz, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_6, vl::logic<4>(0x4ULL))) {
                vl::wr(rampupdwnz, vl::logic<1>(0x0ULL));
            } else {
                vl::wr(rampupdwnz, vl::logic<1>(0x0ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphyhsrxoffscal::proc_always_275() {
    if (vl::rd(sresetz_cfg_clk).truth_val() == 0) {
        vl::wr(masksr, vl::logic<3>(0x6ULL));
    } else {
        {
            const vl::logic<4> sel_7 = vl::rd(state);
            if (vl::case_eq(sel_7, vl::logic<4>(0x0ULL))) {
                vl::wr(masksr, vl::logic<3>(0x6ULL));
            } else if (vl::case_eq(sel_7, vl::logic<4>(0x1ULL))) {
                vl::wr(masksr, vl::cat(vl::rd(masksr).bit(0), vl::rd(masksr).slice<2>(1)));
            } else if (vl::case_eq(sel_7, vl::logic<4>(0x8ULL))) {
                vl::wr(masksr, vl::cat(vl::rd(masksr).bit(0), vl::rd(masksr).slice<2>(1)));
            } else if (vl::case_eq(sel_7, vl::logic<4>(0x2ULL))) {
                vl::wr(masksr, vl::cat(vl::rd(masksr).bit(0), vl::rd(masksr).slice<2>(1)));
            } else if (vl::case_eq(sel_7, vl::logic<4>(0x5ULL))) {
                vl::wr(masksr, vl::cat(vl::rd(masksr).bit(0), vl::rd(masksr).slice<2>(1)));
            } else if (vl::case_eq(sel_7, vl::logic<4>(0x3ULL))) {
                vl::wr(masksr, vl::cat(vl::rd(masksr).bit(0), vl::rd(masksr).slice<2>(1)));
            } else if (vl::case_eq(sel_7, vl::logic<4>(0x7ULL))) {
                vl::wr(masksr, vl::cat(vl::rd(masksr).bit(0), vl::rd(masksr).slice<2>(1)));
            } else if (vl::case_eq(sel_7, vl::logic<4>(0x4ULL))) {
                vl::wr(masksr, vl::logic<3>(0x6ULL));
            } else {
                vl::wr(masksr, vl::logic<3>(0x6ULL));
            }
        }
    }
}

// if(!sresetz_cfg_clk)
// setoffs_int <= 6'b111111 ;
// else if((startcal && state != STARTCAL && state != WAIT_STATE) &&  masksr == 3'b110)
// case(rampupdwnz)
// 1'b0: setoffs_int <= setoffs_int - 6'b000001 ;
// 1'b1: setoffs_int <= setoffs_int + 6'b000001 ;
// endcase
// else if(startcal && state != STARTCAL) setoffs_int <= setoffs_int ;
// else if(calrdy) setoffs_int<= setoffs_av[5:0] ;
// else setoffs_int <= 6'b111111 ;
void mipi_4_rx_dphy_ns_dphyhsrxoffscal::proc_always_307() {
    if (vl::rd(sresetz_cfg_clk).truth_val() == 0) {
        vl::wr(setoffs_int, vl::logic<6>(0x3fULL));
    } else {
        {
            const vl::logic<4> sel_8 = vl::rd(state);
            if (vl::case_eq(sel_8, vl::logic<4>(0x0ULL))) {
                if (vl::rd(calrdy).test()) {
                    vl::wr(setoffs_int, vl::rd(setoffs_av));
                } else {
                    vl::wr(setoffs_int, vl::logic<6>(0x3fULL));
                }
            } else if (vl::case_eq(sel_8, vl::logic<4>(0x1ULL))) {
                vl::wr(setoffs_int, vl::logic<6>(0x3fULL));
            } else if (vl::case_eq(sel_8, vl::logic<4>(0x8ULL))) {
                vl::wr(setoffs_int, vl::logic<6>(0x3fULL));
            } else if (vl::case_eq(sel_8, vl::logic<4>(0x2ULL))) {
                if (vl::eq(vl::rd(masksr), vl::logic<3>(0x6ULL)).test()) {
                    vl::wr(setoffs_int, vl::rd(setoffs_int) - vl::logic<6>(0x1ULL));
                } else {
                    vl::wr(setoffs_int, vl::rd(setoffs_int));
                }
            } else if (vl::case_eq(sel_8, vl::logic<4>(0x3ULL))) {
                if (vl::eq(vl::rd(masksr), vl::logic<3>(0x6ULL)).test()) {
                    vl::wr(setoffs_int, vl::rd(setoffs_int) + vl::logic<6>(0x1ULL));
                } else {
                    vl::wr(setoffs_int, vl::rd(setoffs_int));
                }
            } else if (vl::case_eq(sel_8, vl::logic<4>(0x5ULL))) {
                vl::wr(setoffs_int, vl::rd(setoffs_int));
            } else if (vl::case_eq(sel_8, vl::logic<4>(0x6ULL))) {
                vl::wr(setoffs_int, vl::rd(setoffs_int));
            } else if (vl::case_eq(sel_8, vl::logic<4>(0x7ULL))) {
                vl::wr(setoffs_int, vl::rd(setoffs_int));
            } else if (vl::case_eq(sel_8, vl::logic<4>(0x4ULL))) {
                if (vl::rd(calrdy).test()) {
                    vl::wr(setoffs_int, vl::rd(setoffs_av));
                } else {
                    vl::wr(setoffs_int, vl::rd(setoffs_int));
                }
            } else {
                vl::wr(setoffs_int, vl::rd(setoffs_int));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_dphyhsrxoffscal::proc_always_353() {
    if (vl::rd(sresetz_cfg_clk).truth_val() == 0) {
        vl::wr(hsrxsample0, vl::logic<1>(0x0ULL));
    } else if (vl::lor(vl::eq(vl::rd(masksr), vl::logic<3>(0x5ULL)), vl::eq(vl::rd(state), vl::logic<4>(0x1ULL))).test()) {
        vl::wr(hsrxsample0, vl::rd(hsrxdouta_sync2).bit(0));
    } else {
        vl::wr(hsrxsample0, vl::rd(hsrxsample0));
    }
}

void mipi_4_rx_dphy_ns_dphyhsrxoffscal::proc_always_360() {
    if (vl::rd(sresetz_cfg_clk).truth_val() == 0) {
        vl::wr(hsrxsample1, vl::logic<1>(0x1ULL));
    } else if (vl::lor(vl::eq(vl::rd(masksr), vl::logic<3>(0x5ULL)), vl::eq(vl::rd(state), vl::logic<4>(0x1ULL))).test()) {
        vl::wr(hsrxsample1, vl::rd(hsrxdouta_sync2).bit(1));
    } else {
        vl::wr(hsrxsample1, vl::rd(hsrxsample1));
    }
}

void mipi_4_rx_dphy_ns_dphyhsrxoffscal::proc_always_367() {
    if (vl::rd(sresetz_cfg_clk).truth_val() == 0) {
        vl::wr(hsrxsample0_d, vl::logic<1>(0x0ULL));
    } else {
        vl::wr(hsrxsample0_d, vl::rd(hsrxsample0));
    }
}

void mipi_4_rx_dphy_ns_dphyhsrxoffscal::proc_always_373() {
    if (vl::rd(sresetz_cfg_clk).truth_val() == 0) {
        vl::wr(hsrxsample1_d, vl::logic<1>(0x1ULL));
    } else {
        vl::wr(hsrxsample1_d, vl::rd(hsrxsample1));
    }
}

void mipi_4_rx_dphy_ns_dphyhsrxoffscal::proc_always_379() {
    if (vl::rd(sresetz_cfg_clk).truth_val() == 0) {
        vl::wr(offstgglup0, vl::logic<6>(0x0ULL));
    } else if (vl::lor(vl::land(vl::eq(vl::rd(state), vl::logic<4>(0x0ULL)), vl::rd(encal)), vl::eq(vl::rd(state), vl::logic<4>(0x1ULL))).test()) {
        vl::wr(offstgglup0, vl::logic<6>(0x0ULL));
    } else if (vl::rd(rampupdwnz).test()) {
        if (vl::ne(vl::rd(hsrxsample0_d), vl::rd(hsrxsample0)).test()) {
            vl::wr(offstgglup0, vl::rd(setoffs_int));
        } else {
            vl::wr(offstgglup0, vl::rd(offstgglup0));
        }
    } else {
        vl::wr(offstgglup0, vl::rd(offstgglup0));
    }
}

void mipi_4_rx_dphy_ns_dphyhsrxoffscal::proc_always_388() {
    if (vl::rd(sresetz_cfg_clk).truth_val() == 0) {
        vl::wr(offstgglup1, vl::logic<6>(0x0ULL));
    } else if (vl::lor(vl::land(vl::eq(vl::rd(state), vl::logic<4>(0x0ULL)), vl::rd(encal)), vl::eq(vl::rd(state), vl::logic<4>(0x1ULL))).test()) {
        vl::wr(offstgglup1, vl::logic<6>(0x0ULL));
    } else if (vl::rd(rampupdwnz).test()) {
        if (vl::ne(vl::rd(hsrxsample1_d), vl::rd(hsrxsample1)).test()) {
            vl::wr(offstgglup1, vl::rd(setoffs_int));
        } else {
            vl::wr(offstgglup1, vl::rd(offstgglup1));
        }
    } else {
        vl::wr(offstgglup1, vl::rd(offstgglup1));
    }
}

void mipi_4_rx_dphy_ns_dphyhsrxoffscal::proc_always_397() {
    if (vl::rd(sresetz_cfg_clk).truth_val() == 0) {
        vl::wr(offstggldw0, vl::logic<6>(0x0ULL));
    } else if (vl::lor(vl::land(vl::eq(vl::rd(state), vl::logic<4>(0x0ULL)), vl::rd(encal)), vl::eq(vl::rd(state), vl::logic<4>(0x1ULL))).test()) {
        vl::wr(offstggldw0, vl::logic<6>(0x0ULL));
    } else if (vl::rd(rampupdwnz).truth_val() == 0) {
        if (vl::ne(vl::rd(hsrxsample0_d), vl::rd(hsrxsample0)).test()) {
            vl::wr(offstggldw0, vl::rd(setoffs_int));
        } else {
            vl::wr(offstggldw0, vl::rd(offstggldw0));
        }
    } else {
        vl::wr(offstggldw0, vl::rd(offstggldw0));
    }
}

void mipi_4_rx_dphy_ns_dphyhsrxoffscal::proc_always_406() {
    if (vl::rd(sresetz_cfg_clk).truth_val() == 0) {
        vl::wr(offstggldw1, vl::logic<6>(0x0ULL));
    } else if (vl::lor(vl::land(vl::eq(vl::rd(state), vl::logic<4>(0x0ULL)), vl::rd(encal)), vl::eq(vl::rd(state), vl::logic<4>(0x1ULL))).test()) {
        vl::wr(offstggldw1, vl::logic<6>(0x0ULL));
    } else if (vl::rd(rampupdwnz).truth_val() == 0) {
        if (vl::ne(vl::rd(hsrxsample1_d), vl::rd(hsrxsample1)).test()) {
            vl::wr(offstggldw1, vl::rd(setoffs_int));
        } else {
            vl::wr(offstggldw1, vl::rd(offstggldw1));
        }
    } else {
        vl::wr(offstggldw1, vl::rd(offstggldw1));
    }
}

void mipi_4_rx_dphy_ns_dphyhsrxoffscal::proc_always_421() {
    if (vl::rd(sresetz_cfg_clk).truth_val() == 0) {
        vl::wr(setoffs_av, vl::logic<6>(0x0ULL));
    } else if (vl::lor(vl::lor(vl::lor(vl::eq(vl::rd(offstggldw0), vl::logic<6>(0x0ULL)), vl::eq(vl::rd(offstggldw1), vl::logic<6>(0x0ULL))), vl::eq(vl::rd(offstgglup0), vl::logic<6>(0x0ULL))), vl::eq(vl::rd(offstgglup1), vl::logic<6>(0x0ULL))).test()) {
        vl::wr(setoffs_av, vl::logic<6>(0x1fULL));
    } else if (vl::lor(vl::lor(vl::lor(vl::eq(vl::rd(offstggldw0), vl::logic<6>(0x3fULL)), vl::eq(vl::rd(offstggldw1), vl::logic<6>(0x3fULL))), vl::eq(vl::rd(offstgglup0), vl::logic<6>(0x3fULL))), vl::eq(vl::rd(offstgglup1), vl::logic<6>(0x3fULL))).test()) {
        vl::wr(setoffs_av, vl::logic<6>(0x1fULL));
    } else if (vl::lor(vl::eq(vl::rd(state), vl::logic<4>(0x4ULL)), vl::rd(calrdy)).test()) {
        vl::wr(setoffs_av, vl::rd(aux_sum).slice<6>(2));
    } else {
        vl::wr(setoffs_av, vl::logic<6>(0x0ULL));
    }
}

// errcal codification
// errcal[3] = 1'b1 - error detected | errcal[3] = 1'b0 - not an error
// errcal[2:0] = errcode/warncode
// error codes:
// 4'b1001 - No toggle
// Normal operation codes:
// 4'b0000 - 1 toggle in different calibration word
// 4'b0001 - 1 toggle in the same calibration word
void mipi_4_rx_dphy_ns_dphyhsrxoffscal::proc_always_443() {
    if (vl::rd(sresetz_cfg_clk).truth_val() == 0) {
        vl::wr(errcal, vl::logic<4>(0x0ULL));
    } else if (vl::rd(calrdy).test()) {
        if (vl::lor(vl::lor(vl::lor(vl::eq(vl::rd(offstggldw0), vl::logic<6>(0x0ULL)), vl::eq(vl::rd(offstggldw1), vl::logic<6>(0x0ULL))), vl::eq(vl::rd(offstgglup0), vl::logic<6>(0x0ULL))), vl::eq(vl::rd(offstgglup1), vl::logic<6>(0x0ULL))).test()) {
            vl::wr(errcal, vl::logic<4>(0x9ULL));
        } else if (vl::lor(vl::lor(vl::lor(vl::eq(vl::rd(offstggldw0), vl::logic<6>(0x3fULL)), vl::eq(vl::rd(offstggldw1), vl::logic<6>(0x3fULL))), vl::eq(vl::rd(offstgglup0), vl::logic<6>(0x3fULL))), vl::eq(vl::rd(offstgglup1), vl::logic<6>(0x3fULL))).test()) {
            vl::wr(errcal, vl::logic<4>(0x9ULL));
        } else if (vl::lor(vl::eq(vl::rd(offstggldw0), vl::rd(offstggldw1)), vl::eq(vl::rd(offstgglup0), vl::rd(offstgglup1))).test()) {
            vl::wr(errcal, vl::logic<4>(0x1ULL));
        } else {
            vl::wr(errcal, vl::logic<4>(0x0ULL));
        }
    } else {
        vl::wr(errcal, vl::logic<4>(0x0ULL));
    }
}
