//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_dphytermcal
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_dphytermcal.h"

mipi_4_rx_dphy_ns_dphytermcal::mipi_4_rx_dphy_ns_dphytermcal(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    tie0__drv.o(tie0_);
    SC_METHOD(assign_rtunpon);
    sensitive << cal_en;
    SC_METHOD(assign_aux_sum);
    sensitive << aux_a << aux_b;
    SC_METHOD(assign_even_odd);
    sensitive << cal_round_max << aux_sum;
    SC_METHOD(assign_round_max_calc);
    sensitive << even_odd;
    SC_METHOD(assign_cal_error_int);
    // CMPOUT Synchronization
    compout_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("compout_sync"));
    compout_sync->clk(cfg_clk);
    compout_sync->rst(resetz);
    compout_sync->data_in(cmpout);
    compout_sync->scanmode(tie0_);
    compout_sync->data_out(cmpout_sync);
    compout_sync->data_out_dly(nc_compout_sync_data_out_dly);
    SC_METHOD(proc_always_98);
    sensitive << clock1m.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_115);
    sensitive << clock1m.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_126);
    sensitive << clock1m.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_142);
    sensitive << clock1m.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_184);
    sensitive << clock1m.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_219);
    sensitive << clock1m.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_236);
    sensitive << clock1m.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_248);
    sensitive << clock1m.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_260);
    sensitive << clock1m.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_272);
    sensitive << clock1m.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_296);
    sensitive << clock1m.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_308);
    sensitive << clock1m.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_320);
    sensitive << state << setrd << aux_a;
    SC_METHOD(proc_always_334);
    sensitive << state << setrd << aux_b;
    SC_METHOD(proc_always_349);
    sensitive << clock1m.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_369);
    sensitive << clock1m.pos() << resetz.neg();
    dont_initialize();
}

// Turn on/off rtunpon signal
void mipi_4_rx_dphy_ns_dphytermcal::assign_rtunpon() {
    vl::wr(rtunpon, vl::rd(cal_en));
}

// ROUND_MAX calculator
void mipi_4_rx_dphy_ns_dphytermcal::assign_aux_sum() {
    vl::wr(aux_sum, vl::cat(vl::logic<1>(0x0ULL), vl::rd(aux_a)) + vl::cat(vl::logic<1>(0x0ULL), vl::rd(aux_b)));
}

// operation done with WIDTH bits
void mipi_4_rx_dphy_ns_dphytermcal::assign_even_odd() {
    vl::wr(even_odd, vl::mux(vl::rd(cal_round_max) & vl::rd(aux_sum).bit(0), vl::rd(aux_sum) + vl::logic<5>(0x1ULL), vl::rd(aux_sum)));
}

void mipi_4_rx_dphy_ns_dphytermcal::assign_round_max_calc() {
    vl::wr(round_max_calc, vl::rd(even_odd).slice<4>(1));
}

void mipi_4_rx_dphy_ns_dphytermcal::assign_cal_error_int() {
    vl::wr(cal_error_int, vl::logic<1>(0x0ULL));
}

// counter needed to have comparator output synchronized
void mipi_4_rx_dphy_ns_dphytermcal::proc_always_98() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(counter, vl::logic<3>(0x0ULL));
    } else if (vl::land(vl::lor(vl::eq(vl::rd(state), vl::logic<3>(0x2ULL)), vl::eq(vl::rd(state), vl::logic<3>(0x3ULL))), vl::ne(vl::rd(counter), vl::rd(timertermcal))).test()) {
        vl::wr(counter, vl::rd(counter) + vl::logic<3>(0x1ULL));
    } else if (vl::lor(vl::eq(vl::rd(state), vl::logic<3>(0x1ULL)), vl::eq(vl::rd(state), vl::logic<3>(0x0ULL))).test()) {
        vl::wr(counter, vl::logic<3>(0x1ULL));
    } else {
        vl::wr(counter, vl::logic<3>(0x0ULL));
    }
}

// cal_en sync
void mipi_4_rx_dphy_ns_dphytermcal::proc_always_115() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(edge_detect, vl::logic<2>(0x0ULL));
    } else {
        vl::wr(edge_detect, vl::cat(vl::rd(edge_detect).bit(0), vl::rd(cal_en)));
    }
}

void mipi_4_rx_dphy_ns_dphytermcal::proc_always_126() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(cal_en_det, vl::logic<1>(0x0ULL));
    } else {
        if (vl::eq(vl::rd(edge_detect), vl::logic<2>(0x1ULL)).test()) {
            vl::wr(cal_en_det, vl::logic<1>(0x1ULL));
        } else {
            vl::wr(cal_en_det, vl::logic<1>(0x0ULL));
        }
    }
}

// Rescaldig FSM
void mipi_4_rx_dphy_ns_dphytermcal::proc_always_142() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(state, vl::logic<3>(0x0ULL));
    } else {
        {
            const vl::logic<3> sel = vl::rd(state);
            if (vl::case_eq(sel, vl::logic<3>(0x0ULL))) {
                if (vl::rd(cal_en_det).test()) {
                    vl::wr(state, vl::logic<3>(0x1ULL));
                } else {
                    vl::wr(state, vl::logic<3>(0x0ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<3>(0x1ULL))) {
                if (vl::logic<1>(0x0ULL).truth_val() == 0) {
                    vl::wr(state, vl::logic<3>(0x2ULL));
                } else if (vl::logic<1>(0x0ULL).test()) {
                    vl::wr(state, vl::logic<3>(0x3ULL));
                } else {
                    vl::wr(state, vl::rd(state));
                }
            } else if (vl::case_eq(sel, vl::logic<3>(0x2ULL))) {
                if (vl::rd(cal_error_int).test()) {
                    vl::wr(state, vl::logic<3>(0x5ULL));
                } else if (vl::land(vl::land(vl::lnot(vl::rd(not_same_cmpout)), vl::eq(vl::rd(setrd), vl::logic<4>(0xfULL))), vl::eq(vl::rd(counter), vl::rd(timertermcal))).test()) {
                    vl::wr(state, vl::logic<3>(0x4ULL));
                } else if (vl::land(vl::land(vl::eq(vl::rd(setrd), vl::logic<4>(0xfULL)), vl::eq(vl::rd(counter), vl::rd(timertermcal))), vl::lnot(vl::rd(cal_error_int))).test()) {
                    vl::wr(state, vl::logic<3>(0x3ULL));
                } else {
                    vl::wr(state, vl::logic<3>(0x2ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<3>(0x3ULL))) {
                if (vl::rd(cal_error_int).test()) {
                    vl::wr(state, vl::logic<3>(0x5ULL));
                } else if (vl::land(vl::eq(vl::rd(setrd), vl::logic<4>(0x0ULL)), vl::eq(vl::rd(counter), vl::rd(timertermcal))).test()) {
                    vl::wr(state, vl::logic<3>(0x6ULL));
                } else if ((vl::logic<1>(0x0ULL) & vl::rd(cmpout_tripd_toggles)).test()) {
                    vl::wr(state, vl::logic<3>(0x6ULL));
                } else {
                    vl::wr(state, vl::logic<3>(0x3ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<3>(0x4ULL))) {
                vl::wr(state, vl::logic<3>(0x7ULL));
            } else if (vl::case_eq(sel, vl::logic<3>(0x5ULL))) {
                if (vl::eq(vl::rd(setrd), vl::logic<4>(0x7ULL)).test()) {
                    vl::wr(state, vl::logic<3>(0x7ULL));
                } else {
                    vl::wr(state, vl::logic<3>(0x5ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<3>(0x6ULL))) {
                vl::wr(state, vl::logic<3>(0x7ULL));
            } else if (vl::case_eq(sel, vl::logic<3>(0x7ULL))) {
                if (vl::rd(cal_en_det).truth_val() == 0) {
                    vl::wr(state, vl::logic<3>(0x0ULL));
                } else {
                    vl::wr(state, vl::logic<3>(0x7ULL));
                }
            } else {
                vl::wr(state, vl::logic<3>(0x0ULL));
            }
        }
    }
}

// Load values to setrd
void mipi_4_rx_dphy_ns_dphytermcal::proc_always_184() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(setrd, vl::logic<4>(0x0ULL));
    } else {
        {
            const vl::logic<3> sel_2 = vl::rd(state);
            if (vl::case_eq(sel_2, vl::logic<3>(0x1ULL))) {
                if (vl::logic<1>(0x0ULL).truth_val() == 0) {
                    vl::wr(setrd, vl::rd(setrd));
                } else if (vl::logic<1>(0x0ULL).test()) {
                    vl::wr(setrd, vl::rd(setrd));
                } else {
                    vl::wr(setrd, vl::rd(setrd));
                }
            } else if (vl::case_eq(sel_2, vl::logic<3>(0x2ULL))) {
                if (vl::land(vl::land(vl::land(vl::eq(vl::rd(counter), vl::rd(timertermcal) - vl::logic<3>(0x2ULL)), vl::eq(vl::rd(setrd), vl::logic<4>(0xfULL))), vl::rd(not_same_cmpout)), vl::lnot(vl::rd(cal_error_int))).test()) {
                    vl::wr(setrd, vl::logic<4>(0xeULL));
                } else if (vl::land(vl::eq(vl::rd(counter), vl::rd(timertermcal) - vl::logic<3>(0x2ULL)), vl::ne(vl::rd(setrd), vl::logic<4>(0xfULL))).test()) {
                    vl::wr(setrd, vl::rd(setrd) + vl::logic<4>(0x1ULL));
                } else if (vl::land(vl::land(vl::land(vl::eq(vl::rd(setrd), vl::logic<4>(0xfULL)), vl::eq(vl::rd(counter), vl::rd(timertermcal) - vl::logic<3>(0x2ULL))), vl::lnot(vl::rd(not_same_cmpout))), vl::rd(aux_tripu)).test()) {
                    vl::wr(setrd, vl::logic<4>(0xfULL));
                } else if (vl::land(vl::land(vl::land(vl::eq(vl::rd(setrd), vl::logic<4>(0xfULL)), vl::eq(vl::rd(counter), vl::rd(timertermcal) - vl::logic<3>(0x2ULL))), vl::lnot(vl::rd(not_same_cmpout))), vl::lnot(vl::rd(aux_tripu))).test()) {
                    vl::wr(setrd, vl::logic<4>(0x0ULL));
                } else if (vl::rd(cal_error_int).test()) {
                    vl::wr(setrd, vl::logic<4>(0x7ULL));
                } else {
                    vl::wr(setrd, vl::rd(setrd));
                }
            } else if (vl::case_eq(sel_2, vl::logic<3>(0x3ULL))) {
                if (vl::land(vl::eq(vl::rd(counter), vl::rd(timertermcal) - vl::logic<3>(0x2ULL)), vl::ne(vl::rd(setrd), vl::logic<4>(0x0ULL))).test()) {
                    vl::wr(setrd, vl::rd(setrd) - vl::logic<4>(0x1ULL));
                } else {
                    vl::wr(setrd, vl::rd(setrd));
                }
            } else if (vl::case_eq(sel_2, vl::logic<3>(0x4ULL))) {
                if (vl::rd(cmpout_sync).test()) {
                    vl::wr(setrd, vl::logic<4>(0xfULL));
                } else {
                    vl::wr(setrd, vl::logic<4>(0x0ULL));
                }
            } else if (vl::case_eq(sel_2, vl::logic<3>(0x5ULL))) {
                vl::wr(setrd, vl::logic<4>(0x7ULL));
            } else if (vl::case_eq(sel_2, vl::logic<3>(0x6ULL))) {
                vl::wr(setrd, vl::mux(vl::logic<1>(0x0ULL), vl::rd(aux_b), vl::rd(round_max_calc)));
            } else {
                vl::wr(setrd, vl::rd(setrd));
            }
        }
    }
}

// Read values from CMPOUT with setrd from 00000 to 11111
void mipi_4_rx_dphy_ns_dphytermcal::proc_always_219() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(aux_tripu, vl::logic<1>(0x0ULL));
    } else if (vl::eq(vl::rd(state), vl::logic<3>(0x7ULL)).test()) {
        vl::wr(aux_tripu, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<3> sel_3 = vl::rd(state);
            if (vl::case_eq(sel_3, vl::logic<3>(0x1ULL))) {
                vl::wr(aux_tripu, vl::rd(cmpout_sync));
            } else if (vl::case_eq(sel_3, vl::logic<3>(0x2ULL))) {
                if (vl::land(vl::land(vl::eq(vl::rd(counter), vl::rd(timertermcal) - vl::logic<3>(0x1ULL)), vl::ne(vl::rd(cmpout_sync), vl::rd(aux_tripu))), vl::lnot(vl::rd(cmpout_tripu_toggles))).test()) {
                    vl::wr(aux_tripu, vl::rd(cmpout_sync));
                } else {
                    vl::wr(aux_tripu, vl::rd(aux_tripu));
                }
            } else {
                vl::wr(aux_tripu, vl::rd(aux_tripu));
            }
        }
    }
}

// Store setrd value when cmpout is different from the first reading
void mipi_4_rx_dphy_ns_dphytermcal::proc_always_236() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(aux_a, vl::logic<4>(0x0ULL));
    } else if (vl::eq(vl::rd(state), vl::logic<3>(0x7ULL)).test()) {
        vl::wr(aux_a, vl::logic<4>(0x0ULL));
    } else if (vl::land(vl::land(vl::land(vl::eq(vl::rd(state), vl::logic<3>(0x2ULL)), vl::eq(vl::rd(counter), vl::rd(timertermcal) - vl::logic<3>(0x1ULL))), vl::ne(vl::rd(cmpout_sync), vl::rd(aux_tripu))), vl::lnot(vl::rd(cmpout_tripu_toggles))).test()) {
        vl::wr(aux_a, vl::rd(setrd));
    } else {
        vl::wr(aux_a, vl::rd(aux_a));
    }
}

// Flag that cmpout toggles during the RAMPUP state
void mipi_4_rx_dphy_ns_dphytermcal::proc_always_248() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(cmpout_tripu_toggles, vl::logic<1>(0x0ULL));
    } else if (vl::eq(vl::rd(state), vl::logic<3>(0x7ULL)).test()) {
        vl::wr(cmpout_tripu_toggles, vl::logic<1>(0x0ULL));
    } else if (vl::land(vl::land(vl::land(vl::eq(vl::rd(state), vl::logic<3>(0x2ULL)), vl::eq(vl::rd(counter), vl::rd(timertermcal) - vl::logic<3>(0x1ULL))), vl::ne(vl::rd(cmpout_sync), vl::rd(aux_tripu))), vl::lnot(vl::rd(cmpout_tripu_toggles))).test()) {
        vl::wr(cmpout_tripu_toggles, vl::logic<1>(0x1ULL));
    } else {
        vl::wr(cmpout_tripu_toggles, vl::rd(cmpout_tripu_toggles));
    }
}

// Flag when cmpout changes
void mipi_4_rx_dphy_ns_dphytermcal::proc_always_260() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(not_same_cmpout, vl::logic<1>(0x0ULL));
    } else if (vl::eq(vl::rd(state), vl::logic<3>(0x7ULL)).test()) {
        vl::wr(not_same_cmpout, vl::logic<1>(0x0ULL));
    } else if (vl::land(vl::land(vl::eq(vl::rd(state), vl::logic<3>(0x2ULL)), vl::eq(vl::rd(counter), vl::rd(timertermcal) - vl::logic<3>(0x1ULL))), vl::ne(vl::rd(cmpout_sync), vl::rd(aux_tripu))).test()) {
        vl::wr(not_same_cmpout, vl::logic<1>(0x1ULL));
    } else {
        vl::wr(not_same_cmpout, vl::rd(not_same_cmpout));
    }
}

// Read values from CMPOUT with setrd from 11110 to 00000
void mipi_4_rx_dphy_ns_dphytermcal::proc_always_272() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(aux_tripd, vl::logic<1>(0x0ULL));
    } else if (vl::eq(vl::rd(state), vl::logic<3>(0x7ULL)).test()) {
        vl::wr(aux_tripd, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<3> sel_4 = vl::rd(state);
            if (vl::case_eq(sel_4, vl::logic<3>(0x1ULL))) {
                if (vl::logic<1>(0x0ULL).test()) {
                    vl::wr(aux_tripd, vl::rd(cmpout_sync));
                } else {
                    vl::wr(aux_tripd, vl::rd(aux_tripd));
                }
            } else if (vl::case_eq(sel_4, vl::logic<3>(0x2ULL))) {
                if (vl::land(vl::eq(vl::rd(setrd), vl::logic<4>(0xfULL)), vl::eq(vl::rd(counter), vl::rd(timertermcal) - vl::logic<3>(0x1ULL))).test()) {
                    vl::wr(aux_tripd, vl::rd(cmpout_sync));
                } else {
                    vl::wr(aux_tripd, vl::rd(aux_tripd));
                }
            } else if (vl::case_eq(sel_4, vl::logic<3>(0x3ULL))) {
                if (vl::land(vl::land(vl::eq(vl::rd(counter), vl::rd(timertermcal) - vl::logic<3>(0x1ULL)), vl::ne(vl::rd(cmpout_sync), vl::rd(aux_tripd))), vl::lnot(vl::rd(cmpout_tripd_toggles))).test()) {
                    vl::wr(aux_tripd, vl::rd(cmpout_sync));
                } else {
                    vl::wr(aux_tripd, vl::rd(aux_tripd));
                }
            } else {
                vl::wr(aux_tripd, vl::rd(aux_tripd));
            }
        }
    }
}

// Store setrd value when cmpout is different from aux_tripd
void mipi_4_rx_dphy_ns_dphytermcal::proc_always_296() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(aux_b, vl::logic<4>(0x0ULL));
    } else if (vl::eq(vl::rd(state), vl::logic<3>(0x7ULL)).test()) {
        vl::wr(aux_b, vl::logic<4>(0x0ULL));
    } else if (vl::land(vl::land(vl::land(vl::land(vl::eq(vl::rd(state), vl::logic<3>(0x3ULL)), vl::le(vl::rd(setrd), vl::logic<4>(0xeULL))), vl::eq(vl::rd(counter), vl::rd(timertermcal) - vl::logic<3>(0x1ULL))), vl::ne(vl::rd(cmpout_sync), vl::rd(aux_tripd))), vl::lnot(vl::rd(cmpout_tripd_toggles))).test()) {
        vl::wr(aux_b, vl::rd(setrd));
    } else {
        vl::wr(aux_b, vl::rd(aux_b));
    }
}

// Flag that cmpout toggles during the RAMPDWN state
void mipi_4_rx_dphy_ns_dphytermcal::proc_always_308() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(cmpout_tripd_toggles, vl::logic<1>(0x0ULL));
    } else if (vl::eq(vl::rd(state), vl::logic<3>(0x7ULL)).test()) {
        vl::wr(cmpout_tripd_toggles, vl::logic<1>(0x0ULL));
    } else if (vl::land(vl::land(vl::land(vl::eq(vl::rd(state), vl::logic<3>(0x3ULL)), vl::eq(vl::rd(counter), vl::rd(timertermcal) - vl::logic<3>(0x1ULL))), vl::ne(vl::rd(cmpout_sync), vl::rd(aux_tripd))), vl::lnot(vl::rd(cmpout_tripd_toggles))).test()) {
        vl::wr(cmpout_tripd_toggles, vl::logic<1>(0x1ULL));
    } else {
        vl::wr(cmpout_tripd_toggles, vl::rd(cmpout_tripd_toggles));
    }
}

void mipi_4_rx_dphy_ns_dphytermcal::proc_always_320() {
    vl::logic<1> setrd_auxa_not_seq_sh = vl::rd(setrd_auxa_not_seq);
    setrd_auxa_not_seq_sh = vl::logic<1>(0x0ULL);
    vl::wr(setrd_auxa_not_seq, setrd_auxa_not_seq_sh);
    if (vl::eq(vl::rd(state), vl::logic<3>(0x2ULL)).test()) {
        if (vl::lt(vl::rd(setrd), vl::logic<4>(0xfULL)).test()) {
            if (vl::eq(vl::rd(aux_a) + vl::logic<4>(0x1ULL), vl::rd(setrd)).test()) {
                setrd_auxa_not_seq_sh = vl::logic<1>(0x1ULL);
                vl::wr(setrd_auxa_not_seq, setrd_auxa_not_seq_sh);
            } else {
                setrd_auxa_not_seq_sh = vl::logic<1>(0x0ULL);
                vl::wr(setrd_auxa_not_seq, setrd_auxa_not_seq_sh);
            }
        } else {
            setrd_auxa_not_seq_sh = vl::logic<1>(0x1ULL);
            vl::wr(setrd_auxa_not_seq, setrd_auxa_not_seq_sh);
        }
    }
}

void mipi_4_rx_dphy_ns_dphytermcal::proc_always_334() {
    vl::logic<1> setrd_auxb_not_seq_sh = vl::rd(setrd_auxb_not_seq);
    setrd_auxb_not_seq_sh = vl::logic<1>(0x0ULL);
    vl::wr(setrd_auxb_not_seq, setrd_auxb_not_seq_sh);
    if (vl::eq(vl::rd(state), vl::logic<3>(0x3ULL)).test()) {
        if (vl::gt(vl::rd(setrd), vl::logic<4>(0x0ULL)).test()) {
            if (vl::eq(vl::rd(aux_b), vl::rd(setrd) + vl::logic<4>(0x1ULL)).test()) {
                setrd_auxb_not_seq_sh = vl::logic<1>(0x1ULL);
                vl::wr(setrd_auxb_not_seq, setrd_auxb_not_seq_sh);
            } else {
                setrd_auxb_not_seq_sh = vl::logic<1>(0x0ULL);
                vl::wr(setrd_auxb_not_seq, setrd_auxb_not_seq_sh);
            }
        } else {
            setrd_auxb_not_seq_sh = vl::logic<1>(0x1ULL);
            vl::wr(setrd_auxb_not_seq, setrd_auxb_not_seq_sh);
        }
    }
}

// Flag an error if aux_tripu != from aux_tripd
void mipi_4_rx_dphy_ns_dphytermcal::proc_always_349() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(cal_error, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<3> sel_5 = vl::rd(state);
            if (vl::case_eq(sel_5, vl::logic<3>(0x2ULL))) {
                if (vl::land(vl::land(vl::land(vl::rd(cmpout_tripu_toggles), vl::ne(vl::rd(aux_tripu), vl::rd(cmpout_sync))), vl::eq(vl::rd(counter), vl::rd(timertermcal))), vl::lnot(vl::rd(setrd_auxa_not_seq))).test()) {
                    vl::wr(cal_error, vl::logic<1>(0x1ULL));
                } else {
                    vl::wr(cal_error, vl::rd(cal_error));
                }
            } else if (vl::case_eq(sel_5, vl::logic<3>(0x3ULL))) {
                if (vl::land(vl::land(vl::land(vl::rd(cmpout_tripd_toggles), vl::ne(vl::rd(aux_tripd), vl::rd(cmpout_sync))), vl::eq(vl::rd(counter), vl::rd(timertermcal))), vl::lnot(vl::rd(setrd_auxb_not_seq))).test()) {
                    vl::wr(cal_error, vl::logic<1>(0x1ULL));
                } else {
                    vl::wr(cal_error, vl::rd(cal_error));
                }
            } else {
                vl::wr(cal_error, vl::rd(cal_error));
            }
        }
    }
}

// makes FSM ignore error and not abort calibration procedure
// Calibration done flag
void mipi_4_rx_dphy_ns_dphytermcal::proc_always_369() {
    if (vl::rd(resetz).truth_val() == 0) {
        vl::wr(cal_done, vl::logic<1>(0x0ULL));
    } else if (vl::eq(vl::rd(state), vl::logic<3>(0x7ULL)).test()) {
        vl::wr(cal_done, vl::logic<1>(0x1ULL));
    } else {
        vl::wr(cal_done, vl::rd(cal_done));
    }
}
