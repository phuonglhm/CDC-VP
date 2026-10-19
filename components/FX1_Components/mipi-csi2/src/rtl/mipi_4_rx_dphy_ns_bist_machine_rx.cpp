//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_bist_machine_rx
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_bist_machine_rx.h"

mipi_4_rx_dphy_ns_bist_machine_rx::mipi_4_rx_dphy_ns_bist_machine_rx(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    tie0__drv.o(tie0_);
    SC_METHOD(merge_ch_error_latch);
    sensitive << ch_error_latch_b[0] << ch_error_latch_b[1] << ch_error_latch_b[2] << ch_error_latch_b[3];
    SC_METHOD(assign_master_pat_strobe);
    sensitive << sync_strobes;
    SC_METHOD(assign_error_gating_out);
    sensitive << error_gating;
    SC_METHOD(assign_ch_error_scanable);
    sensitive << ch_error << error_gating << scanmode;
    SC_METHOD(assign_switch_on_lb_mux);
    sensitive << trigger_pg;
    SC_METHOD(assign_feed_int_in_bist);
    sensitive << trigger_pg;
    SC_METHOD(assign_trigger_hsdirect);
    sensitive << trigger_pg;
    SC_METHOD(assign_clk_en_clklane);
    sensitive << ser_shift_boot_seq;
    SC_METHOD(assign_div_en_clklane);
    sensitive << ser_shift_boot_seq;
    SC_METHOD(assign_clksel_en);
    sensitive << ser_shift_boot_seq;
    SC_METHOD(assign_serial_clk_en_clklane);
    sensitive << ser_shift_boot_seq;
    SC_METHOD(assign_serial_en_clklane);
    sensitive << ser_shift_boot_seq;
    SC_METHOD(assign_clk_en_datalane);
    sensitive << ser_shift_boot_seq;
    SC_METHOD(assign_div_en_datalane);
    sensitive << ser_shift_boot_seq;
    SC_METHOD(assign_serial_data_en_datalane);
    sensitive << ser_shift_boot_seq_datalane;
    SC_METHOD(assign_serial_en_datalane);
    sensitive << ser_shift_boot_seq_datalane;
    // wiring for N lanes
    // -- WIRING LOGIC -------------------------------------------------------------
    bist_enable_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge1("bist_enable_sync"));
    bist_enable_sync->clk(clk);
    bist_enable_sync->rst(rst);
    bist_enable_sync->data_in(bist_en);
    bist_enable_sync->scanmode(tie0_);
    bist_enable_sync->data_out(bist_en_sync[0]);
    bist_enable_sync->data_out_dly(bist_en_sync[1]);
    hand_clk_scan_mux.reset(new mipi_4_rx_dphy_ns_gen_mux("hand_clk_scan_mux"));
    hand_clk_scan_mux->z(clk_scanable);
    hand_clk_scan_mux->sel(scanmode);
    hand_clk_scan_mux->d0(tie0_);
    hand_clk_scan_mux->d1(scanclk);
    SC_METHOD(glue_hand_error_rst_mux_d1);
    sensitive << scanrst;
    hand_error_rst_mux.reset(new mipi_4_rx_dphy_ns_gen_mux("hand_error_rst_mux"));
    hand_error_rst_mux->z(error_rst_scanable);
    hand_error_rst_mux->sel(scanmode);
    hand_error_rst_mux->d0(error_rst);
    hand_error_rst_mux->d1(c_hand_error_rst_mux_d1);
    gen_force_state_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge1("gen_force_state_sync"));
    gen_force_state_sync->clk(clk);
    gen_force_state_sync->rst(rst);
    gen_force_state_sync->data_in(force_state);
    gen_force_state_sync->scanmode(tie0_);
    gen_force_state_sync->data_out(force_state_sync);
    gen_force_state_sync->data_out_dly(nc_gen_force_state_sync_data_out_dly);
    target_state_sync_bus.reset(new mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge1_bus_width4("target_state_sync_bus"));
    target_state_sync_bus->clk(clk);
    target_state_sync_bus->clk_inv(clk);
    target_state_sync_bus->rst(rst);
    target_state_sync_bus->data_in(target_state);
    target_state_sync_bus->data_out(target_state_sync);
    GEN_MUX_CREATE_0_hand_ch_pat_strobe_mux.reset(new mipi_4_rx_dphy_ns_gen_mux("GEN_MUX_CREATE_0_hand_ch_pat_strobe_mux"));
    GEN_MUX_CREATE_0_hand_ch_pat_strobe_mux->z(ch_pat_strobe_scanable);
    GEN_MUX_CREATE_0_hand_ch_pat_strobe_mux->sel(scanmode);
    GEN_MUX_CREATE_0_hand_ch_pat_strobe_mux->d0(ch_pat_strobe);
    GEN_MUX_CREATE_0_hand_ch_pat_strobe_mux->d1(scanclk);
    DYNAMIC_PULSE_CREATE_0_hand_pat_strobe_pcatch.reset(new mipi_4_rx_dphy_ns_dynamic_pulse_catcher("DYNAMIC_PULSE_CREATE_0_hand_pat_strobe_pcatch"));
    DYNAMIC_PULSE_CREATE_0_hand_pat_strobe_pcatch->pulse_in(ch_pat_strobe_scanable);
    DYNAMIC_PULSE_CREATE_0_hand_pat_strobe_pcatch->clk(clk);
    DYNAMIC_PULSE_CREATE_0_hand_pat_strobe_pcatch->rst(rst);
    DYNAMIC_PULSE_CREATE_0_hand_pat_strobe_pcatch->pulse_out(ch_pat_strobe_pulse);
    STATIC_PULSE_CREATE_0_hand_ch_error_pcatch.reset(new mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge1("STATIC_PULSE_CREATE_0_hand_ch_error_pcatch"));
    STATIC_PULSE_CREATE_0_hand_ch_error_pcatch->pulse_in(ch_error_scanable[0]);
    STATIC_PULSE_CREATE_0_hand_ch_error_pcatch->clk(clk);
    STATIC_PULSE_CREATE_0_hand_ch_error_pcatch->rst(rst);
    STATIC_PULSE_CREATE_0_hand_ch_error_pcatch->clk_cap(clk_scanable);
    STATIC_PULSE_CREATE_0_hand_ch_error_pcatch->rst_cap(error_rst_scanable);
    STATIC_PULSE_CREATE_0_hand_ch_error_pcatch->pulse_out(ch_error_pulse[0]);
    STATIC_PULSE_CREATE_0_hand_ch_error_pcatch->pulse_in_ff(ch_error_latch_b[0]);
    STATIC_PULSE_CREATE_1_hand_ch_error_pcatch.reset(new mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge1("STATIC_PULSE_CREATE_1_hand_ch_error_pcatch"));
    STATIC_PULSE_CREATE_1_hand_ch_error_pcatch->pulse_in(ch_error_scanable[1]);
    STATIC_PULSE_CREATE_1_hand_ch_error_pcatch->clk(clk);
    STATIC_PULSE_CREATE_1_hand_ch_error_pcatch->rst(rst);
    STATIC_PULSE_CREATE_1_hand_ch_error_pcatch->clk_cap(clk_scanable);
    STATIC_PULSE_CREATE_1_hand_ch_error_pcatch->rst_cap(error_rst_scanable);
    STATIC_PULSE_CREATE_1_hand_ch_error_pcatch->pulse_out(ch_error_pulse[1]);
    STATIC_PULSE_CREATE_1_hand_ch_error_pcatch->pulse_in_ff(ch_error_latch_b[1]);
    STATIC_PULSE_CREATE_2_hand_ch_error_pcatch.reset(new mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge1("STATIC_PULSE_CREATE_2_hand_ch_error_pcatch"));
    STATIC_PULSE_CREATE_2_hand_ch_error_pcatch->pulse_in(ch_error_scanable[2]);
    STATIC_PULSE_CREATE_2_hand_ch_error_pcatch->clk(clk);
    STATIC_PULSE_CREATE_2_hand_ch_error_pcatch->rst(rst);
    STATIC_PULSE_CREATE_2_hand_ch_error_pcatch->clk_cap(clk_scanable);
    STATIC_PULSE_CREATE_2_hand_ch_error_pcatch->rst_cap(error_rst_scanable);
    STATIC_PULSE_CREATE_2_hand_ch_error_pcatch->pulse_out(ch_error_pulse[2]);
    STATIC_PULSE_CREATE_2_hand_ch_error_pcatch->pulse_in_ff(ch_error_latch_b[2]);
    STATIC_PULSE_CREATE_3_hand_ch_error_pcatch.reset(new mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge1("STATIC_PULSE_CREATE_3_hand_ch_error_pcatch"));
    STATIC_PULSE_CREATE_3_hand_ch_error_pcatch->pulse_in(ch_error_scanable[3]);
    STATIC_PULSE_CREATE_3_hand_ch_error_pcatch->clk(clk);
    STATIC_PULSE_CREATE_3_hand_ch_error_pcatch->rst(rst);
    STATIC_PULSE_CREATE_3_hand_ch_error_pcatch->clk_cap(clk_scanable);
    STATIC_PULSE_CREATE_3_hand_ch_error_pcatch->rst_cap(error_rst_scanable);
    STATIC_PULSE_CREATE_3_hand_ch_error_pcatch->pulse_out(ch_error_pulse[3]);
    STATIC_PULSE_CREATE_3_hand_ch_error_pcatch->pulse_in_ff(ch_error_latch_b[3]);
    SC_METHOD(proc_always_279);
    sensitive << clk.pos() << rst.pos();
    dont_initialize();
    SC_METHOD(proc_always_287);
    sensitive << clk.pos() << rst.pos();
    dont_initialize();
    SC_METHOD(proc_always_377);
    sensitive << clk.pos() << rst.pos();
    dont_initialize();
    SC_METHOD(proc_always_450);
    sensitive << clk.pos() << rst.pos();
    dont_initialize();
    SC_METHOD(proc_always_471);
    sensitive << clk.pos() << rst.pos();
    dont_initialize();
    SC_METHOD(proc_always_525);
    sensitive << clk.pos() << rst.pos();
    dont_initialize();
    SC_METHOD(proc_always_535);
    sensitive << clk.pos() << rst.pos();
    dont_initialize();
    SC_METHOD(proc_always_551);
    sensitive << clk.pos() << rst.pos();
    dont_initialize();
    SC_METHOD(proc_always_574);
    sensitive << clk.pos() << rst.pos();
    dont_initialize();
    SC_METHOD(proc_always_581);
    sensitive << clk.pos() << rst.pos();
    dont_initialize();
    SC_METHOD(proc_always_593);
    sensitive << clk.pos() << rst.pos();
    dont_initialize();
    SC_METHOD(proc_always_607);
    sensitive << clk.pos() << rst.pos();
    dont_initialize();
    SC_METHOD(proc_always_630);
    sensitive << clk.pos() << rst.pos();
    dont_initialize();
}

void mipi_4_rx_dphy_ns_bist_machine_rx::merge_ch_error_latch() {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(ch_error_latch_b[0]));
    v.set_bit(1, vl::rd(ch_error_latch_b[1]));
    v.set_bit(2, vl::rd(ch_error_latch_b[2]));
    v.set_bit(3, vl::rd(ch_error_latch_b[3]));
    vl::wr(ch_error_latch, v);
}

vl::logic<2> mipi_4_rx_dphy_ns_bist_machine_rx::rd_bist_en_sync() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(bist_en_sync[0]));
    v.set_bit(1, vl::rd(bist_en_sync[1]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_bist_machine_rx::rd_ch_error_latch() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(ch_error_latch_b[0]));
    v.set_bit(1, vl::rd(ch_error_latch_b[1]));
    v.set_bit(2, vl::rd(ch_error_latch_b[2]));
    v.set_bit(3, vl::rd(ch_error_latch_b[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_bist_machine_rx::rd_ch_error_pulse() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(ch_error_pulse[0]));
    v.set_bit(1, vl::rd(ch_error_pulse[1]));
    v.set_bit(2, vl::rd(ch_error_pulse[2]));
    v.set_bit(3, vl::rd(ch_error_pulse[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_bist_machine_rx::rd_ch_error_scanable() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(ch_error_scanable[0]));
    v.set_bit(1, vl::rd(ch_error_scanable[1]));
    v.set_bit(2, vl::rd(ch_error_scanable[2]));
    v.set_bit(3, vl::rd(ch_error_scanable[3]));
    return v;
}

// assign master_pat_strobe = (sync_strobes == 3'b111);    //waits for all channels to strobe
void mipi_4_rx_dphy_ns_bist_machine_rx::assign_master_pat_strobe() {
    vl::wr(master_pat_strobe, vl::eq(vl::rd(sync_strobes), vl::logic<1>(0x1ULL)));
}

void mipi_4_rx_dphy_ns_bist_machine_rx::assign_error_gating_out() {
    vl::wr(error_gating_out, vl::rd(error_gating).bit(1));
}

// Check for errors in the PRBS matcher
// assign ch0_error_scanable = (ch0_error & error_gating[1]) & (~scanmode);
// assign ch1_error_scanable = (ch1_error & error_gating[1]) & (~scanmode);
// assign ch2_error_scanable = (ch2_error & error_gating[1]) & (~scanmode);
void mipi_4_rx_dphy_ns_bist_machine_rx::assign_ch_error_scanable() {
    const vl::logic<4> v_ = (vl::rd(ch_error) & vl::rep<4>(vl::rd(error_gating).bit(1))) & vl::rep<4>(~vl::rd(scanmode));
    ch_error_scanable[0].write(v_.bit(0).sl());
    ch_error_scanable[1].write(v_.bit(1).sl());
    ch_error_scanable[2].write(v_.bit(2).sl());
    ch_error_scanable[3].write(v_.bit(3).sl());
}

// lbmux switch on control
void mipi_4_rx_dphy_ns_bist_machine_rx::assign_switch_on_lb_mux() {
    vl::wr(switch_on_lb_mux, vl::rd(trigger_pg));
}

// trigger_pg is active when the PLL has recahed the lock state and keeps it active later
void mipi_4_rx_dphy_ns_bist_machine_rx::assign_feed_int_in_bist() {
    vl::wr(feed_int_in_bist, vl::rd(trigger_pg));
}

// trigger_pg is active when the PLL has recahed the lock state and keeps it active later
void mipi_4_rx_dphy_ns_bist_machine_rx::assign_trigger_hsdirect() {
    vl::wr(trigger_hsdirect, vl::rd(trigger_pg));
}

void mipi_4_rx_dphy_ns_bist_machine_rx::assign_clk_en_clklane() {
    vl::wr(clk_en_clklane, vl::rd(ser_shift_boot_seq).bit(0));
}

void mipi_4_rx_dphy_ns_bist_machine_rx::assign_div_en_clklane() {
    vl::wr(div_en_clklane, vl::rd(ser_shift_boot_seq).bit(1));
}

void mipi_4_rx_dphy_ns_bist_machine_rx::assign_clksel_en() {
    vl::wr(clksel_en, vl::rd(ser_shift_boot_seq).bit(2));
}

// 2clock cycles before enabling the serializer
void mipi_4_rx_dphy_ns_bist_machine_rx::assign_serial_clk_en_clklane() {
    vl::wr(serial_clk_en_clklane, vl::rd(ser_shift_boot_seq).bit(4));
}

void mipi_4_rx_dphy_ns_bist_machine_rx::assign_serial_en_clklane() {
    vl::wr(serial_en_clklane, vl::rd(ser_shift_boot_seq).bit(5));
}

void mipi_4_rx_dphy_ns_bist_machine_rx::assign_clk_en_datalane() {
    vl::wr(clk_en_datalane, vl::rd(ser_shift_boot_seq).bit(0));
}

void mipi_4_rx_dphy_ns_bist_machine_rx::assign_div_en_datalane() {
    vl::wr(div_en_datalane, vl::rd(ser_shift_boot_seq).bit(1));
}

void mipi_4_rx_dphy_ns_bist_machine_rx::assign_serial_data_en_datalane() {
    vl::wr(serial_data_en_datalane, vl::rd(ser_shift_boot_seq_datalane).bit(0));
}

void mipi_4_rx_dphy_ns_bist_machine_rx::assign_serial_en_datalane() {
    vl::wr(serial_en_datalane, vl::rd(ser_shift_boot_seq_datalane).bit(1));
}

void mipi_4_rx_dphy_ns_bist_machine_rx::glue_hand_error_rst_mux_d1() { vl::wr(c_hand_error_rst_mux_d1, ~vl::rd(scanrst)); }

void mipi_4_rx_dphy_ns_bist_machine_rx::proc_always_279() {
    if (vl::rd(rst).test()) {
        vl::wr(error_rst, vl::logic<1>(0x0ULL));
    } else {
        vl::wr(error_rst, ~(vl::lnot(vl::rd(bist_en_sync[0])) | ((vl::ne(vl::rd(bist_state), vl::logic<4>(0x5ULL)) & vl::ne(vl::rd(bist_state), vl::logic<4>(0x6ULL))) & vl::ne(vl::rd(bist_state), vl::logic<4>(0x7ULL)))));
    }
}

// BIST FSM
void mipi_4_rx_dphy_ns_bist_machine_rx::proc_always_287() {
    if (vl::rd(rst).test()) {
        vl::wr(bist_state, vl::logic<4>(0x0ULL));
    } else if (vl::rd(force_state_sync).test()) {
        // state override capbility
        vl::wr(bist_state, vl::rd(target_state_sync));
    } else {
        {
            const vl::logic<4> sel = vl::rd(bist_state);
            if (vl::case_eq(sel, vl::logic<4>(0x0ULL))) {
                if (vl::rd(bist_en_sync[0]).test()) {
                    vl::wr(bist_state, vl::logic<4>(0x1ULL));
                } else {
                    vl::wr(bist_state, vl::logic<4>(0x0ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0x1ULL))) {
                if (vl::rd(fsm_done).test()) {
                    vl::wr(bist_state, vl::logic<4>(0x2ULL));
                } else {
                    vl::wr(bist_state, vl::logic<4>(0x1ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0x2ULL))) {
                if (vl::rd(ser_boot_done).test()) {
                    vl::wr(bist_state, vl::logic<4>(0x3ULL));
                } else {
                    vl::wr(bist_state, vl::logic<4>(0x2ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0x3ULL))) {
                if (vl::eq(vl::rd(counter), vl::rd(del_datalane)).test()) {
                    // datalanes
                    vl::wr(bist_state, vl::logic<4>(0x9ULL));
                } else {
                    vl::wr(bist_state, vl::logic<4>(0x3ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0x9ULL))) {
                if (vl::rd(ser_boot_done).test()) {
                    vl::wr(bist_state, vl::logic<4>(0xcULL));
                } else {
                    vl::wr(bist_state, vl::logic<4>(0x9ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0xcULL))) {
                if (vl::eq(vl::rd(counter), vl::rd(pipe_clean_time)).test()) {
                    // pipe cleaning
                    vl::wr(bist_state, vl::logic<4>(0xdULL));
                } else {
                    vl::wr(bist_state, vl::logic<4>(0xcULL));
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0xdULL))) {
                vl::wr(bist_state, vl::logic<4>(0x4ULL));
            } else if (vl::case_eq(sel, vl::logic<4>(0x4ULL))) {
                if (vl::eq(vl::rd(counter), vl::rd(pipe_clean_time)).test()) {
                    // pipe cleaning
                    vl::wr(bist_state, vl::logic<4>(0x5ULL));
                } else {
                    vl::wr(bist_state, vl::logic<4>(0x4ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0x5ULL))) {
                if (vl::rd(master_pat_strobe).test()) {
                    vl::wr(bist_state, vl::logic<4>(0x6ULL));
                } else {
                    vl::wr(bist_state, vl::logic<4>(0x5ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0x6ULL))) {
                if (vl::rd(master_pat_strobe).test()) {
                    vl::wr(bist_state, vl::logic<4>(0x7ULL));
                } else {
                    vl::wr(bist_state, vl::logic<4>(0x6ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0x7ULL))) {
                if (vl::land(vl::lnot(vl::rd(bist_en_sync[0])), vl::rd(bist_en_sync[1])).test()) {
                    vl::wr(bist_state, vl::logic<4>(0x8ULL));
                } else {
                    vl::wr(bist_state, vl::logic<4>(0x7ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0x8ULL))) {
                if (vl::eq(vl::rd(counter), vl::rd(del_datalane)).test()) {
                    vl::wr(bist_state, vl::logic<4>(0xaULL));
                } else {
                    vl::wr(bist_state, vl::logic<4>(0x8ULL));
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0xaULL))) {
                if (vl::eq(vl::rd(ser_shift_boot_seq_datalane), vl::logic<2>(0x0ULL)).test()) {
                    // switch off serializer datalane
                    vl::wr(bist_state, vl::logic<4>(0xeULL));
                } else {
                    vl::wr(bist_state, vl::logic<4>(0xaULL));
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0xeULL))) {
                if (vl::eq(vl::rd(counter), vl::rd(del_datalane)).test()) {
                    vl::wr(bist_state, vl::logic<4>(0xbULL));
                } else {
                    vl::wr(bist_state, vl::logic<4>(0xeULL));
                }
            } else if (vl::case_eq(sel, vl::logic<4>(0xbULL))) {
                if (vl::eq(vl::rd(ser_shift_boot_seq), vl::logic<6>(0x0ULL)).test()) {
                    // switch off serializer datalane
                    vl::wr(bist_state, vl::logic<4>(0x0ULL));
                } else {
                    vl::wr(bist_state, vl::logic<4>(0xbULL));
                }
            } else {
                vl::wr(bist_state, vl::logic<4>(0x0ULL));
            }
        }
    }
}

// support counters
void mipi_4_rx_dphy_ns_bist_machine_rx::proc_always_377() {
    if (vl::rd(rst).test()) {
        vl::wr(counter, vl::logic<8>(0x0ULL));
    } else if (((((vl::eq(vl::rd(bist_state), vl::logic<4>(0x3ULL)) | vl::eq(vl::rd(bist_state), vl::logic<4>(0x4ULL))) | vl::eq(vl::rd(bist_state), vl::logic<4>(0x8ULL))) | vl::eq(vl::rd(bist_state), vl::logic<4>(0xcULL))) | vl::eq(vl::rd(bist_state), vl::logic<4>(0xeULL))).test()) {
        if (vl::ne(vl::rd(counter), vl::logic<8>(0xffULL)).test()) {
            vl::wr(counter, vl::rd(counter) + vl::logic<8>(0x1ULL));
        } else {
            vl::wr(counter, vl::logic<8>(0x0ULL));
        }
    } else {
        vl::wr(counter, vl::logic<8>(0x0ULL));
    }
}

void mipi_4_rx_dphy_ns_bist_machine_rx::proc_always_450() {
    if (vl::rd(rst).test()) {
        vl::wr(sync_strobes, vl::logic<1>(0x0ULL));
    } else if (vl::eq(vl::rd(sync_strobes), vl::logic<1>(0x1ULL)).test()) {
        vl::wr(sync_strobes, vl::logic<1>(0x0ULL));
    } else {
        if (vl::rd(sync_strobes).bit(0).truth_val() == 0) {
            vl::wr(sync_strobes, vl::rd(ch_pat_strobe_pulse).bit(0));
        } else {
            vl::wr(sync_strobes, vl::logic<1>(0x1ULL));
        }
    }
}

// gating in error is also controlled with comp_after_strobes to see errors coming after the pat strobes detection
void mipi_4_rx_dphy_ns_bist_machine_rx::proc_always_471() {
    if (vl::rd(rst).test()) {
        vl::wr(error_gating, vl::logic<2>(0x0ULL));
    } else {
        if (vl::rd(comp_after_strobes).truth_val() == 0) {
            // errors are only visible in patstrobes detction states
            vl::wr(error_gating, vl::cat(vl::rd(error_gating).bit(0), vl::eq(vl::rd(bist_state), vl::logic<4>(0x5ULL)) | vl::eq(vl::rd(bist_state), vl::logic<4>(0x6ULL))));
        } else {
            // errors are visible in the strobe detection states and in the Finish state
            {
                const vl::logic<4> sel_2 = vl::rd(bist_state);
                if (vl::case_eq(sel_2, vl::logic<4>(0x5ULL))) {
                    vl::wr(error_gating, vl::cat(vl::rd(error_gating).bit(0), vl::logic<1>(0x1ULL)));
                } else if (vl::case_eq(sel_2, vl::logic<4>(0x6ULL))) {
                    vl::wr(error_gating, vl::cat(vl::rd(error_gating).bit(0), vl::logic<1>(0x1ULL)));
                } else if (vl::case_eq(sel_2, vl::logic<4>(0x7ULL))) {
                    vl::wr(error_gating, vl::cat(vl::rd(error_gating).bit(0), vl::logic<1>(0x1ULL)));
                } else {
                    vl::wr(error_gating, vl::logic<2>(0x0ULL));
                }
            }
        }
    }
}

// BIST machine outputs
// if(rst)
// bistpath_en <= 1'b0;
// else
// //bistpath_en <= ((bist_state != IDLE) & (bist_state != WAIT4PLL) & (bist_state != FINISH));
// bistpath_en <= ((bist_state != IDLE) & (bist_state != WAIT4PLL));
// BIST machine outputs
void mipi_4_rx_dphy_ns_bist_machine_rx::proc_always_525() {
    if (vl::rd(rst).test()) {
        vl::wr(onpll, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_3 = vl::rd(bist_state);
            if (vl::case_eq(sel_3, vl::logic<4>(0x0ULL))) {
                vl::wr(onpll, vl::logic<1>(0x0ULL));
            } else {
                vl::wr(onpll, vl::logic<1>(0x1ULL));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_bist_machine_rx::proc_always_535() {
    if (vl::rd(rst).test()) {
        vl::wr(trigger_pg, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_4 = vl::rd(bist_state);
            if (vl::case_eq(sel_4, vl::logic<4>(0x0ULL))) {
                vl::wr(trigger_pg, vl::logic<1>(0x0ULL));
            } else if (vl::case_eq(sel_4, vl::logic<4>(0x1ULL))) {
                vl::wr(trigger_pg, vl::logic<1>(0x0ULL));
            } else {
                vl::wr(trigger_pg, vl::logic<1>(0x1ULL));
            }
        }
    }
}

// hsdirect control
void mipi_4_rx_dphy_ns_bist_machine_rx::proc_always_551() {
    if (vl::rd(rst).test()) {
        vl::wr(bist_sync_en, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_5 = vl::rd(bist_state);
            if (vl::case_eq(sel_5, vl::logic<4>(0x4ULL))) {
                vl::wr(bist_sync_en, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_5, vl::logic<4>(0x5ULL))) {
                vl::wr(bist_sync_en, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_5, vl::logic<4>(0x6ULL))) {
                vl::wr(bist_sync_en, vl::logic<1>(0x1ULL));
            } else if (vl::case_eq(sel_5, vl::logic<4>(0x7ULL))) {
                if (vl::rd(comp_after_strobes).test()) {
                    vl::wr(bist_sync_en, vl::logic<1>(0x1ULL));
                } else {
                    vl::wr(bist_sync_en, vl::logic<1>(0x0ULL));
                }
            } else {
                vl::wr(bist_sync_en, vl::logic<1>(0x0ULL));
            }
        }
    }
}

// bist_sync_en <= (bist_state == SYNC_BIST);
// assign bist_done    = (bist_state == FINISH);
void mipi_4_rx_dphy_ns_bist_machine_rx::proc_always_574() {
    if (vl::rd(rst).test()) {
        vl::wr(bist_done, vl::logic<1>(0x0ULL));
    } else {
        vl::wr(bist_done, vl::eq(vl::rd(bist_state), vl::logic<4>(0x7ULL)));
    }
}

void mipi_4_rx_dphy_ns_bist_machine_rx::proc_always_581() {
    if (vl::rd(rst).test()) {
        vl::wr(bist_ok, vl::logic<1>(0x1ULL));
    } else if (vl::eq(vl::rd(bist_state), vl::logic<4>(0x0ULL)).test()) {
        vl::wr(bist_ok, vl::logic<1>(0x1ULL));
    } else if (vl::land(vl::lor(vl::eq(vl::rd(bist_state), vl::logic<4>(0x5ULL)), vl::eq(vl::rd(bist_state), vl::logic<4>(0x6ULL))), vl::eq(vl::rd(bist_ok), vl::logic<1>(0x1ULL))).test()) {
        // bist_ok <= (~|error_latch_sync);
        vl::wr(bist_ok, ~vl::ror_(rd_ch_error_pulse() & vl::rd(ch_bist_run)));
    } else {
        vl::wr(bist_ok, vl::rd(bist_ok));
    }
}

// clock lane boot
void mipi_4_rx_dphy_ns_bist_machine_rx::proc_always_593() {
    if (vl::rd(rst).test()) {
        vl::wr(ser_shift_boot_seq, vl::logic<6>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_6 = vl::rd(bist_state);
            if (vl::case_eq(sel_6, vl::logic<4>(0x2ULL))) {
                vl::wr(ser_shift_boot_seq, vl::cat(vl::rd(ser_shift_boot_seq).slice<5>(0), vl::logic<1>(0x1ULL)));
            } else if (vl::case_eq(sel_6, vl::logic<4>(0xbULL))) {
                vl::wr(ser_shift_boot_seq, vl::cat(vl::logic<1>(0x0ULL), vl::rd(ser_shift_boot_seq).slice<5>(1)));
            } else if (vl::case_eq(sel_6, vl::logic<4>(0x0ULL))) {
                vl::wr(ser_shift_boot_seq, vl::logic<6>(0x0ULL));
            } else {
                vl::wr(ser_shift_boot_seq, vl::rd(ser_shift_boot_seq));
            }
        }
    }
}

void mipi_4_rx_dphy_ns_bist_machine_rx::proc_always_607() {
    if (vl::rd(rst).test()) {
        vl::wr(ser_boot_done, vl::logic<1>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_7 = vl::rd(bist_state);
            if (vl::case_eq(sel_7, vl::logic<4>(0x2ULL))) {
                vl::wr(ser_boot_done, vl::rand_(vl::rd(ser_shift_boot_seq)));
            } else if (vl::case_eq(sel_7, vl::logic<4>(0x3ULL))) {
                vl::wr(ser_boot_done, vl::logic<1>(0x0ULL));
            } else if (vl::case_eq(sel_7, vl::logic<4>(0x9ULL))) {
                vl::wr(ser_boot_done, vl::rand_(vl::rd(ser_shift_boot_seq_datalane)));
            } else if (vl::case_eq(sel_7, vl::logic<4>(0x0ULL))) {
                vl::wr(ser_boot_done, vl::logic<1>(0x0ULL));
            } else {
                vl::wr(ser_boot_done, vl::rd(ser_boot_done));
            }
        }
    }
}

// datalane boot
void mipi_4_rx_dphy_ns_bist_machine_rx::proc_always_630() {
    if (vl::rd(rst).test()) {
        vl::wr(ser_shift_boot_seq_datalane, vl::logic<2>(0x0ULL));
    } else {
        {
            const vl::logic<4> sel_8 = vl::rd(bist_state);
            if (vl::case_eq(sel_8, vl::logic<4>(0x9ULL))) {
                vl::wr(ser_shift_boot_seq_datalane, vl::cat(vl::rd(ser_shift_boot_seq_datalane).bit(0), vl::logic<1>(0x1ULL)));
            } else if (vl::case_eq(sel_8, vl::logic<4>(0xaULL))) {
                vl::wr(ser_shift_boot_seq_datalane, vl::cat(vl::logic<1>(0x0ULL), vl::rd(ser_shift_boot_seq_datalane).bit(1)));
            } else if (vl::case_eq(sel_8, vl::logic<4>(0x0ULL))) {
                vl::wr(ser_shift_boot_seq_datalane, vl::logic<2>(0x0ULL));
            } else {
                vl::wr(ser_shift_boot_seq_datalane, vl::rd(ser_shift_boot_seq_datalane));
            }
        }
    }
}
