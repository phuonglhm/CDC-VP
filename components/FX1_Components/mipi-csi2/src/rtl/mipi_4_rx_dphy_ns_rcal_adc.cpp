//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_rcal_adc
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_rcal_adc.h"

mipi_4_rx_dphy_ns_rcal_adc::mipi_4_rx_dphy_ns_rcal_adc(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    tie0__drv.o(tie0_);
    SC_METHOD(assign_cfgclk_rcal_int_scanable);
    sensitive << ck_ref_s;
    SC_METHOD(assign_ck_ref_g);
    sensitive << cfgclk_rcal_int_scanable;
    SC_METHOD(assign_clk_cnt_done);
    sensitive << clk_cnt;
    SC_METHOD(assign_stage_cnt_done);
    sensitive << stage_cnt << clk_cnt_done;
    SC_METHOD(assign_bit_cnt_done);
    sensitive << bit_cnt << stage_cnt_done;
    SC_METHOD(assign_sar_bit);
    sensitive << bit_cnt;
    SC_METHOD(assign_rcal_trigger);
    sensitive << rcal_enb_d2 << rcal_enb_d3;
    SC_METHOD(assign_adc_trigger);
    sensitive << adc_enb_d2 << adc_enb_d3;
    SC_METHOD(assign_adc_val);
    sensitive << sup_dac;
    SC_METHOD(assign_tx_rescal_done);
    sensitive << d_tx_term << clk_cnt;
    SC_METHOD(glue_sync_rcal_enb_sync_rst);
    sensitive << async_reset;
    // synchronous reset gating
    // gen_sync_rst #(.reset_value(1), .reset_edge(1)) hand_cfg_srst_sync(.rst(async_reset), .clk(cfgclk_rcal_int_scanable), .srst(cfg_srst));
    // gen_mux hand_cfg_srst_mux(.z(cfg_srst_scanable), .sel(scanmode), .d0(cfg_srst), .d1(scanrst));
    // gen_sync #(.reset_edge(1)) sync_rcal_enb_sync(.data_out(rcal_enb_sync), .data_out_dly(), .scanmode(1'b0), .clk(cfgclk_rcal_int_scanable), .rst(cfg_srst_scanable), .data_in(rcal_enb));
    // gen_sync #(.reset_edge(1)) sync_adc_enb_sync(.data_out(adc_enb_sync), .data_out_dly(), .scanmode(1'b0), .clk(cfgclk_rcal_int_scanable), .rst(cfg_srst_scanable), .data_in(adc_enb));
    sync_rcal_enb_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("sync_rcal_enb_sync"));
    sync_rcal_enb_sync->clk(cfgclk_rcal_int_scanable);
    sync_rcal_enb_sync->rst(c_sync_rcal_enb_sync_rst);
    sync_rcal_enb_sync->data_in(rcal_enb);
    sync_rcal_enb_sync->scanmode(tie0_);
    sync_rcal_enb_sync->data_out(rcal_enb_sync);
    sync_rcal_enb_sync->data_out_dly(nc_sync_rcal_enb_sync_data_out_dly);
    SC_METHOD(glue_sync_adc_enb_sync_rst);
    sensitive << async_reset;
    sync_adc_enb_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("sync_adc_enb_sync"));
    sync_adc_enb_sync->clk(cfgclk_rcal_int_scanable);
    sync_adc_enb_sync->rst(c_sync_adc_enb_sync_rst);
    sync_adc_enb_sync->data_in(adc_enb);
    sync_adc_enb_sync->scanmode(tie0_);
    sync_adc_enb_sync->data_out(adc_enb_sync);
    sync_adc_enb_sync->data_out_dly(nc_sync_adc_enb_sync_data_out_dly);
    SC_METHOD(proc_always_210);
    sensitive << cfgclk_rcal_int_scanable.posedge_event() << async_reset.pos();
    dont_initialize();
    SC_METHOD(proc_always_223);
    sensitive << cfgclk_rcal_int_scanable.posedge_event() << async_reset.pos();
    dont_initialize();
    SC_METHOD(proc_always_256);
    sensitive << ck_ref_g.posedge_event() << async_reset.pos();
    dont_initialize();
    SC_METHOD(proc_always_276);
    sensitive << ck_ref_g.posedge_event() << async_reset.pos();
    dont_initialize();
    SC_METHOD(proc_always_296);
    sensitive << ck_ref_g.posedge_event() << async_reset.pos();
    dont_initialize();
    SC_METHOD(proc_always_360);
    sensitive << sup_comp_mode_i << rcal_state << rcal_trigger << adc_trigger << stage_cnt_done << bit_cnt_done << d_tx_term << tx_rescal_done;
    SC_METHOD(proc_always_438);
    sensitive << ck_ref_g.posedge_event() << async_reset.pos();
    dont_initialize();
    SC_METHOD(proc_always_462);
    sensitive << ck_ref_g.posedge_event() << async_reset.pos();
    dont_initialize();
    SC_METHOD(proc_always_471);
    sensitive << ck_ref_g.posedge_event() << async_reset.pos();
    dont_initialize();
    SC_METHOD(proc_always_488);
    sensitive << ck_ref_g.posedge_event() << async_reset.pos();
    dont_initialize();
    SC_METHOD(proc_always_507);
    sensitive << ck_ref_g.posedge_event() << async_reset.pos();
    dont_initialize();
}

// removed divison by 4 in the clock for DPHY GEN3
// Division by 4 to keep rcal clock under 42.5MHz
// if(sync_reset)
// cfgclk_div4 <= 2'b00;
// else
// cfgclk_div4 <= {~cfgclk_div4[0], cfgclk_div4[1]};
// gen_mux hand_cfgclk_rcal_div_mux(.z(cfgclk_rcal_int_scanable), .sel(scanmode), .d0(/*cfgclk_div4[0]*/ck_ref_s), .d1(scanclk));
void mipi_4_rx_dphy_ns_rcal_adc::assign_cfgclk_rcal_int_scanable() {
    vl::wr(cfgclk_rcal_int_scanable, vl::rd(ck_ref_s));
}

// gen_clock_gating #(.sync(0), .reset_edge(1)) cfgclk_rcal_gate(.clk(cfgclk_rcal_int_scanable), .rst(cfg_srst_scanable), .en(rcal_enb_sync | adc_enb_sync), .clk_gated(cfgclk_rcal_gated));
// gen_mux hand_cfgclk_rcal_mux(.z(ck_ref_g), .sel(scanmode), .d0(cfgclk_rcal_gated), .d1(scanclk));
void mipi_4_rx_dphy_ns_rcal_adc::assign_ck_ref_g() {
    vl::wr(ck_ref_g, vl::rd(cfgclk_rcal_int_scanable));
}

void mipi_4_rx_dphy_ns_rcal_adc::assign_clk_cnt_done() {
    vl::wr(clk_cnt_done, vl::eq(vl::rd(clk_cnt), vl::logic<4>(0xcULL)));
}

void mipi_4_rx_dphy_ns_rcal_adc::assign_stage_cnt_done() {
    vl::wr(stage_cnt_done, vl::eq(vl::rd(stage_cnt), vl::logic<2>(0x2ULL)) & vl::rd(clk_cnt_done));
}

void mipi_4_rx_dphy_ns_rcal_adc::assign_bit_cnt_done() {
    vl::wr(bit_cnt_done, vl::eq(vl::rd(bit_cnt), vl::logic<4>(0x0ULL)) & vl::rd(stage_cnt_done));
}

// sar-bit mapping (one-hot)
void mipi_4_rx_dphy_ns_rcal_adc::assign_sar_bit() {
    vl::wr(sar_bit, vl::mux(vl::eq(vl::rd(bit_cnt), vl::logic<4>(0x9ULL)), vl::logic<10>(0x200ULL), vl::mux(vl::eq(vl::rd(bit_cnt), vl::logic<4>(0x8ULL)), vl::logic<10>(0x100ULL), vl::mux(vl::eq(vl::rd(bit_cnt), vl::logic<4>(0x7ULL)), vl::logic<10>(0x80ULL), vl::mux(vl::eq(vl::rd(bit_cnt), vl::logic<4>(0x6ULL)), vl::logic<10>(0x40ULL), vl::mux(vl::eq(vl::rd(bit_cnt), vl::logic<4>(0x5ULL)), vl::logic<10>(0x20ULL), vl::mux(vl::eq(vl::rd(bit_cnt), vl::logic<4>(0x4ULL)), vl::logic<10>(0x10ULL), vl::mux(vl::eq(vl::rd(bit_cnt), vl::logic<4>(0x3ULL)), vl::logic<10>(0x8ULL), vl::mux(vl::eq(vl::rd(bit_cnt), vl::logic<4>(0x2ULL)), vl::logic<10>(0x4ULL), vl::mux(vl::eq(vl::rd(bit_cnt), vl::logic<4>(0x1ULL)), vl::logic<10>(0x2ULL), vl::logic<10>(0x1ULL)))))))))));
}

// Generate triggers for RCAL and ADC operation by detecting a rising edge of the request.
// t_synchronizer_apr #(1'b0, 1'b0) rcal_trigger_sync (
// .data_out(rcal_enb_d3),
// .data_out_2(rcal_enb_d2),
// .data_in(rcal_enb_async[1]),
// .clk(ck_ref_s),
// .reset(async_reset)
// );
// t_synchronizer_apr #(1'b0, 1'b0) adc_trigger_sync (
// .data_out(adc_enb_d3),
// .data_out_2(adc_enb_d2),
// .data_in(adc_enb_async[1]),
// .clk(ck_ref_s),
// .reset(async_reset)
// );
void mipi_4_rx_dphy_ns_rcal_adc::assign_rcal_trigger() {
    vl::wr(rcal_trigger, vl::rd(rcal_enb_d2) & vl::lnot(vl::rd(rcal_enb_d3)));
}

void mipi_4_rx_dphy_ns_rcal_adc::assign_adc_trigger() {
    vl::wr(adc_trigger, vl::rd(adc_enb_d2) & vl::lnot(vl::rd(adc_enb_d3)));
}

void mipi_4_rx_dphy_ns_rcal_adc::assign_adc_val() {
    vl::wr(adc_val, vl::rd(sup_dac));
}

void mipi_4_rx_dphy_ns_rcal_adc::assign_tx_rescal_done() {
    vl::wr(tx_rescal_done, ((((((vl::eq(vl::rd(d_tx_term), vl::logic<3>(0x0ULL)) | (vl::eq(vl::rd(d_tx_term), vl::logic<3>(0x1ULL)) & vl::eq(vl::rd(clk_cnt), vl::logic<4>(0x0ULL)))) | (vl::eq(vl::rd(d_tx_term), vl::logic<3>(0x2ULL)) & vl::eq(vl::rd(clk_cnt), vl::logic<4>(0x1ULL)))) | (vl::eq(vl::rd(d_tx_term), vl::logic<3>(0x3ULL)) & vl::eq(vl::rd(clk_cnt), vl::logic<4>(0x2ULL)))) | (vl::eq(vl::rd(d_tx_term), vl::logic<3>(0x4ULL)) & vl::eq(vl::rd(clk_cnt), vl::logic<4>(0x3ULL)))) | (vl::eq(vl::rd(d_tx_term), vl::logic<3>(0x5ULL)) & vl::eq(vl::rd(clk_cnt), vl::logic<4>(0x4ULL)))) | (vl::eq(vl::rd(d_tx_term), vl::logic<3>(0x6ULL)) & vl::eq(vl::rd(clk_cnt), vl::logic<4>(0x5ULL)))) | vl::eq(vl::rd(d_tx_term), vl::logic<3>(0x7ULL)));
}

void mipi_4_rx_dphy_ns_rcal_adc::glue_sync_rcal_enb_sync_rst() { vl::wr(c_sync_rcal_enb_sync_rst, ~vl::rd(async_reset)); }

void mipi_4_rx_dphy_ns_rcal_adc::glue_sync_adc_enb_sync_rst() { vl::wr(c_sync_adc_enb_sync_rst, ~vl::rd(async_reset)); }

void mipi_4_rx_dphy_ns_rcal_adc::proc_always_210() {
    if (vl::rd(async_reset).test()) {
        vl::wr(rcal_enb_d2, vl::logic<1>(0x0ULL));
        vl::wr(rcal_enb_d3, vl::logic<1>(0x0ULL));
    } else {
        vl::wr(rcal_enb_d2, vl::rd(rcal_enb_sync));
        vl::wr(rcal_enb_d3, vl::rd(rcal_enb_d2));
    }
}

void mipi_4_rx_dphy_ns_rcal_adc::proc_always_223() {
    if (vl::rd(async_reset).test()) {
        vl::wr(adc_enb_d2, vl::logic<1>(0x0ULL));
        vl::wr(adc_enb_d3, vl::logic<1>(0x0ULL));
    } else {
        vl::wr(adc_enb_d2, vl::rd(adc_enb_sync));
        vl::wr(adc_enb_d3, vl::rd(adc_enb_d2));
    }
}

// Enable ck_ref_g
// dng_synchronizer_apr #(1'b0, 1'b0) ck_ref_enb_sync (
// .data_out(ck_ref_enb),
// .data_in(rcal_enb_async[1] | adc_enb_async[1]),
// .clk_n(scan_mode? ~ck_ref_s : ck_ref_s),
// .reset(async_reset)
// );
// // Gate ck_ref_s based on rcal_enb or adc_enb
// mipi_4_rx_dphy_ns_rtl_and hand_refclk_and (
// .A(ck_ref_s),
// .B(ck_ref_enb | scan_mode),
// .Y(ck_ref_g)
// );
// Clock counter for tuning/ADC
void mipi_4_rx_dphy_ns_rcal_adc::proc_always_256() {
    if (vl::rd(async_reset).test()) {
        vl::wr(clk_cnt, vl::logic<4>(0x0ULL));
    } else {
        if (vl::rd(initialize).test()) {
            vl::wr(clk_cnt, vl::logic<4>(0x0ULL));
        } else if (vl::rd(clk_cnt_enb).test()) {
            if (vl::eq(vl::rd(clk_cnt), vl::logic<4>(0xcULL)).test()) {
                vl::wr(clk_cnt, vl::logic<4>(0x0ULL));
            } else {
                vl::wr(clk_cnt, vl::rd(clk_cnt) + vl::logic<4>(0x1ULL));
            }
        }
    }
}

// Bit-staging counter for tuning/ADC
// Wraps after 3 periods of clk_cnt
void mipi_4_rx_dphy_ns_rcal_adc::proc_always_276() {
    if (vl::rd(async_reset).test()) {
        vl::wr(stage_cnt, vl::logic<2>(0x0ULL));
    } else {
        if (vl::rd(initialize).test()) {
            vl::wr(stage_cnt, vl::logic<2>(0x0ULL));
        } else if ((vl::rd(stage_cnt_enb) & vl::rd(clk_cnt_done)).test()) {
            if (vl::eq(vl::rd(stage_cnt), vl::logic<2>(0x2ULL)).test()) {
                vl::wr(stage_cnt, vl::logic<2>(0x0ULL));
            } else {
                vl::wr(stage_cnt, vl::rd(stage_cnt) + vl::logic<2>(0x1ULL));
            }
        }
    }
}

// Bit counter for tuning/ADC
// Initializes to 9 for ADC; 6 for tuning.
void mipi_4_rx_dphy_ns_rcal_adc::proc_always_296() {
    if (vl::rd(async_reset).test()) {
        vl::wr(bit_cnt, vl::logic<4>(0x9ULL));
    } else {
        if (vl::rd(initialize).test()) {
            vl::wr(bit_cnt, vl::mux(vl::rd(rcal_enb_d2), vl::logic<4>(0x6ULL), vl::logic<4>(0x9ULL)));
        } else if ((vl::rd(bit_cnt_enb) & vl::rd(stage_cnt_done)).test()) {
            if (vl::ne(vl::rd(bit_cnt), vl::logic<4>(0x0ULL)).test()) {
                vl::wr(bit_cnt, vl::rd(bit_cnt) - vl::logic<4>(0x1ULL));
            }
        }
    }
}

// Tuning/ADC FSM
// The state machine is triggered by the rising edges of rcal_enb or adc_enb.
// At the end of tuning/ADC operation, the FSM asserts rcal_adc_done which
// can be used by the logic generating rcal_enb/adc_enb to
// deassert the enables. The rcal/adc operations can also be initiated using
// control register requests. In this case the rcal_adc_done signal can be used
// to asynchronously set the "done" status flag in the control registers. The
// I2C master should monitor this status bit for completion status. Before
// initiating another rcal/adc request, the I2C master should clear the status
// bit by writing a 0 into it.
void mipi_4_rx_dphy_ns_rcal_adc::proc_always_360() {
    vl::logic<1> bit_cnt_enb_sh = vl::rd(bit_cnt_enb);
    vl::logic<1> clk_cnt_enb_sh = vl::rd(clk_cnt_enb);
    vl::logic<1> initialize_sh = vl::rd(initialize);
    vl::logic<1> load_tx_rescal_sh = vl::rd(load_tx_rescal);
    vl::logic<2> next_rcal_state_sh = vl::rd(next_rcal_state);
    vl::logic<1> rcal_adc_done_int_sh = vl::rd(rcal_adc_done_int);
    vl::logic<1> stage_cnt_enb_sh = vl::rd(stage_cnt_enb);
    vl::logic<1> sup_comp_mode_int_sh = vl::rd(sup_comp_mode_int);
    vl::logic<1> sup_comp_rt_pwron_int_sh = vl::rd(sup_comp_rt_pwron_int);
    vl::logic<1> tx_rescal_enb_sh = vl::rd(tx_rescal_enb);
    // Defaults
    clk_cnt_enb_sh = vl::logic<1>(0x0ULL);
    vl::wr(clk_cnt_enb, clk_cnt_enb_sh);
    stage_cnt_enb_sh = vl::logic<1>(0x0ULL);
    vl::wr(stage_cnt_enb, stage_cnt_enb_sh);
    bit_cnt_enb_sh = vl::logic<1>(0x0ULL);
    vl::wr(bit_cnt_enb, bit_cnt_enb_sh);
    initialize_sh = vl::logic<1>(0x0ULL);
    vl::wr(initialize, initialize_sh);
    rcal_adc_done_int_sh = vl::logic<1>(0x0ULL);
    vl::wr(rcal_adc_done_int, rcal_adc_done_int_sh);
    sup_comp_rt_pwron_int_sh = vl::logic<1>(0x0ULL);
    vl::wr(sup_comp_rt_pwron_int, sup_comp_rt_pwron_int_sh);
    load_tx_rescal_sh = vl::logic<1>(0x0ULL);
    vl::wr(load_tx_rescal, load_tx_rescal_sh);
    tx_rescal_enb_sh = vl::logic<1>(0x0ULL);
    vl::wr(tx_rescal_enb, tx_rescal_enb_sh);
    sup_comp_mode_int_sh = vl::rd(sup_comp_mode_i);
    vl::wr(sup_comp_mode_int, sup_comp_mode_int_sh);
    // Next state logic
    {
        const vl::logic<2> sel = vl::rd(rcal_state);
        if (vl::case_eq(sel, vl::logic<2>(0x0ULL))) {
            next_rcal_state_sh = vl::logic<2>(0x0ULL);
            vl::wr(next_rcal_state, next_rcal_state_sh);
            initialize_sh = vl::logic<1>(0x1ULL);
            vl::wr(initialize, initialize_sh);
            if (vl::rd(rcal_trigger).test()) {
                sup_comp_mode_int_sh = vl::logic<1>(0x1ULL);
                vl::wr(sup_comp_mode_int, sup_comp_mode_int_sh);
            } else {
                sup_comp_mode_int_sh = vl::logic<1>(0x0ULL);
                vl::wr(sup_comp_mode_int, sup_comp_mode_int_sh);
            }
            if ((vl::rd(rcal_trigger) | vl::rd(adc_trigger)).test()) {
                next_rcal_state_sh = vl::logic<2>(0x1ULL);
                vl::wr(next_rcal_state, next_rcal_state_sh);
            }
        } else if (vl::case_eq(sel, vl::logic<2>(0x1ULL))) {
            next_rcal_state_sh = vl::logic<2>(0x1ULL);
            vl::wr(next_rcal_state, next_rcal_state_sh);
            clk_cnt_enb_sh = vl::logic<1>(0x1ULL);
            vl::wr(clk_cnt_enb, clk_cnt_enb_sh);
            stage_cnt_enb_sh = vl::logic<1>(0x1ULL);
            vl::wr(stage_cnt_enb, stage_cnt_enb_sh);
            sup_comp_rt_pwron_int_sh = vl::logic<1>(0x1ULL);
            vl::wr(sup_comp_rt_pwron_int, sup_comp_rt_pwron_int_sh);
            if (vl::rd(stage_cnt_done).test()) {
                next_rcal_state_sh = vl::logic<2>(0x2ULL);
                vl::wr(next_rcal_state, next_rcal_state_sh);
            }
        } else if (vl::case_eq(sel, vl::logic<2>(0x2ULL))) {
            next_rcal_state_sh = vl::logic<2>(0x2ULL);
            vl::wr(next_rcal_state, next_rcal_state_sh);
            clk_cnt_enb_sh = vl::logic<1>(0x1ULL);
            vl::wr(clk_cnt_enb, clk_cnt_enb_sh);
            stage_cnt_enb_sh = vl::logic<1>(0x1ULL);
            vl::wr(stage_cnt_enb, stage_cnt_enb_sh);
            bit_cnt_enb_sh = vl::logic<1>(0x1ULL);
            vl::wr(bit_cnt_enb, bit_cnt_enb_sh);
            sup_comp_rt_pwron_int_sh = vl::logic<1>(0x1ULL);
            vl::wr(sup_comp_rt_pwron_int, sup_comp_rt_pwron_int_sh);
            if (vl::rd(bit_cnt_done).test()) {
                if (vl::rd(sup_comp_mode_i).test()) {
                    next_rcal_state_sh = vl::logic<2>(0x3ULL);
                    vl::wr(next_rcal_state, next_rcal_state_sh);
                    load_tx_rescal_sh = vl::logic<1>(0x1ULL);
                    vl::wr(load_tx_rescal, load_tx_rescal_sh);
                } else {
                    next_rcal_state_sh = vl::logic<2>(0x0ULL);
                    vl::wr(next_rcal_state, next_rcal_state_sh);
                    rcal_adc_done_int_sh = vl::logic<1>(0x1ULL);
                    vl::wr(rcal_adc_done_int, rcal_adc_done_int_sh);
                }
            }
        } else if (vl::case_eq(sel, vl::logic<2>(0x3ULL))) {
            next_rcal_state_sh = vl::logic<2>(0x3ULL);
            vl::wr(next_rcal_state, next_rcal_state_sh);
            clk_cnt_enb_sh = vl::logic<1>(0x1ULL);
            vl::wr(clk_cnt_enb, clk_cnt_enb_sh);
            // Do not compute tx_rescal when d_tx_term = 000b
            tx_rescal_enb_sh = vl::ne(vl::rd(d_tx_term), vl::logic<3>(0x0ULL));
            vl::wr(tx_rescal_enb, tx_rescal_enb_sh);
            if (vl::rd(tx_rescal_done).test()) {
                next_rcal_state_sh = vl::logic<2>(0x0ULL);
                vl::wr(next_rcal_state, next_rcal_state_sh);
                rcal_adc_done_int_sh = vl::logic<1>(0x1ULL);
                vl::wr(rcal_adc_done_int, rcal_adc_done_int_sh);
            }
        } else {
            next_rcal_state_sh = vl::logic<2>(0x0ULL);
            vl::wr(next_rcal_state, next_rcal_state_sh);
        }
    }
}

// FSM outputs
void mipi_4_rx_dphy_ns_rcal_adc::proc_always_438() {
    if (vl::rd(async_reset).test()) {
        vl::wr(rcal_state, vl::logic<2>(0x0ULL));
        vl::wr(sup_comp_rt_pwron_i, vl::logic<1>(0x0ULL));
        vl::wr(rcal_adc_done, vl::logic<1>(0x0ULL));
        vl::wr(sup_comp_mode_i, vl::logic<1>(0x0ULL));
        vl::wr(sup_comp_rt_r_i, vl::logic<1>(0x1ULL));
    } else {
        vl::wr(rcal_state, vl::rd(next_rcal_state));
        vl::wr(sup_comp_rt_pwron_i, vl::rd(sup_comp_rt_pwron_int));
        vl::wr(rcal_adc_done, vl::rd(rcal_adc_done_int));
        vl::wr(sup_comp_mode_i, vl::rd(sup_comp_mode_int));
        if (vl::rd(initialize).test()) {
            vl::wr(sup_comp_rt_r_i, vl::logic<1>(0x1ULL));
        } else if (vl::rd(clk_cnt_done).test()) {
            vl::wr(sup_comp_rt_r_i, vl::eq(vl::rd(stage_cnt), vl::logic<2>(0x2ULL)));
        }
    }
}

// Latch comparator result in 2nd stage of a bit.
// Comparator output is guarranteed to be stable during the
// capture cycle. Therefore, no need to re-sync the input.
void mipi_4_rx_dphy_ns_rcal_adc::proc_always_462() {
    if (vl::rd(async_reset).test()) {
        vl::wr(rt_result_latch, vl::logic<1>(0x0ULL));
    } else if ((vl::eq(vl::rd(stage_cnt), vl::logic<2>(0x1ULL)) & vl::rd(clk_cnt_done)).test()) {
        vl::wr(rt_result_latch, vl::rd(sup_comp_rt_result));
    }
}

// rescal generation
void mipi_4_rx_dphy_ns_rcal_adc::proc_always_471() {
    if (vl::rd(async_reset).test()) {
        vl::wr(sup_dac, vl::logic<10>(0x0ULL));
    } else if (vl::rd(rcal_trigger).test()) {
        vl::wr(sup_dac, vl::logic<10>(0x40ULL));
    } else if (vl::rd(adc_trigger).test()) {
        vl::wr(sup_dac, vl::logic<10>(0x200ULL));
    } else if ((vl::rd(bit_cnt_enb) & vl::rd(stage_cnt_done)).test()) {
        vl::wr(sup_dac, (vl::rd(sup_dac) & ((~vl::rd(sar_bit)) | vl::rep<10>(vl::rd(rt_result_latch)))) | vl::cat(vl::logic<1>(0x0ULL), vl::rd(sar_bit).slice<9>(1)));
    }
}

// Latch the sup_dac output on rescal_rep at the end of rescal operation.
// Also latch the tx_rescal computed result at end of computation.
// The computed result is rounded off to reduce the quatization error.
void mipi_4_rx_dphy_ns_rcal_adc::proc_always_488() {
    if (vl::rd(async_reset).test()) {
        vl::wr(rescal_rep_i, vl::logic<7>(0x0ULL));
        vl::wr(tx_rescal_i, vl::logic<7>(0x0ULL));
    } else {
        if ((vl::rd(rcal_enb_d2) & vl::rd(rcal_adc_done)).test()) {
            vl::wr(rescal_rep_i, vl::rd(sup_dac).slice<7>(0));
            vl::wr(tx_rescal_i, vl::rd(tx_rescal_acc).slice<7>(3) + vl::cat(vl::logic<6>(0x0ULL), vl::rd(tx_rescal_acc).bit(2)));
        }
    }
}

// tx_rescal accumulator
// tx_rescal = rescal_rep (1 - 0.125 * d_tx_term); 0 <= d_tx_term <= 6
// tx_rescal = 0000000b when d_tx_term == 111b
// Insert extra LSbs to preform /8 subtractions.
void mipi_4_rx_dphy_ns_rcal_adc::proc_always_507() {
    if (vl::rd(async_reset).test()) {
        vl::wr(tx_rescal_acc, vl::logic<10>(0x0ULL));
    } else if (vl::eq(vl::rd(d_tx_term), vl::logic<3>(0x7ULL)).test()) {
        vl::wr(tx_rescal_acc, vl::logic<10>(0x0ULL));
    } else if (vl::rd(load_tx_rescal).test()) {
        vl::wr(tx_rescal_acc, vl::cat(vl::rd(sup_dac).slice<6>(1), vl::rd(rt_result_latch), vl::logic<3>(0x0ULL)));
    } else if (vl::rd(tx_rescal_enb).test()) {
        vl::wr(tx_rescal_acc, vl::rd(tx_rescal_acc) - vl::cat(vl::logic<3>(0x0ULL), vl::rd(sup_dac).slice<7>(0)));
    }
}
