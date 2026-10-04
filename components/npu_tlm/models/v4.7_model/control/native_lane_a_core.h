// Lane A core block, selectable at build time.
//
// NativeLaneACoreA groups ctrl_inst_a + act_feeder_a + wei_feeder_a + psm_inst_a of this checkout (native
// sauria:: modules). RtlRefLaneACoreA is the same boundary built from the RTL-accurate sauria_model port
// (sauria_rtl::). LaneACoreBlockA (end of file) picks one with -DSAURIA_CORE_BACKEND_RTL_REF.
//
// Scope: array_inst is NOT part of the block. The PE array is one module shared by Lane A and Lane B (rows split
// with i_nsplit, see systolic_array/sa_array.h), and several Makefile targets (test_dual_lane_fsm,
// test_lane_b_isolation, test_nsplit_barrier, test_rich_isa) build at 64x64 and really use Lane B. array_inst
// stays in npu_top.h, shared as before.
//
// Extraction rule: the wiring of npu_top.h is kept 1:1 -- every port below carries exactly the npu_top.h signal
// name (no renaming, no merging). The port list is therefore long: several ports are internal handshakes between
// the sub-modules (e.g. o_context_id, o_pipeline_en) that still have to be visible because array_inst,
// config_regs_inst or npu_top's debug monitors read them.
//
// The debug-injection mux (debug_ref_stream_mux() in npu_top.h) sits entirely outside this block (between
// o_act_arr/o_wei_arr and the array inputs); with dbg_ref_stream_en = false (default) it is a pass-through.
#ifndef SAURIA_NATIVE_LANE_A_CORE_H
#define SAURIA_NATIVE_LANE_A_CORE_H

#include <systemc.h>
#include "sauria_types.h"
#include "control/main_controller.h"
#include "data_feeder/ifmap_feeder.h"
#include "data_feeder/wei_feeder.h"
#include "psm/psm_top.h"

// RTL-accurate core ported from sauria_model, in its own namespace sauria_rtl (see the header comment of
// control/rtl_ref_context_switch_controller.h for why the namespace is separate).
#include "control/rtl_ref_defaults.h"
#include "control/rtl_ref_main_controller.h"
#include "data_feeder/rtl_ref_ifmap_feeder.h"
#include "data_feeder/rtl_ref_wei_feeder.h"
#include "psm/rtl_ref_psm_top.h"

namespace sauria
{
    template <
        int X_DIM = 32,
        int Y_DIM = 32,
        typename T_ACT = int8_t,
        typename T_WEI = int8_t,
        typename T_PSUM = int32_t,
        int FIFO_DEPTH = 16,
        int PE_LAT = 64,
        int EXTRA_CSREG = 1,
        int SRAMC_CAP = 1536>
    class NativeLaneACoreA : public sc_module
    {
    public:
        // ---- Clock/reset/start ----
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};
        sc_in<bool> i_soft_reset{"i_soft_reset"};   // = npu_top's s_ctrl_reset_internal
        sc_in<bool> i_start{"i_start"};             // = npu_top's s_start_internal_a

        // ---- Geometry / context (shared from npu_top) ----
        sc_in<uint32_t> i_mvm_k{"i_mvm_k"};
        sc_in<uint32_t> i_total_contexts{"i_total_contexts"};
        sc_in<uint32_t> i_nsplit{"i_nsplit"};

        // ---- Configuration from config_regs_inst (shared by the whole NpuTop) ----
        sc_in<uint32_t> i_incntlim{"i_incntlim"};
        sc_in<uint32_t> i_act_reps{"i_act_reps"};
        sc_in<uint32_t> i_wei_reps{"i_wei_reps"};
        sc_in<uint32_t> i_out_ncontexts{"i_out_ncontexts"};

        sc_in<uint32_t> i_act_incntlim{"i_act_incntlim"};
        sc_in<uint32_t> i_act_incntstep{"i_act_incntstep"};
        sc_in<uint32_t> i_act_outcntlim{"i_act_outcntlim"};
        sc_in<uint32_t> i_act_outcntstep{"i_act_outcntstep"};
        sc_in<sc_bv<DILP_W>> i_act_dil_pat{"i_act_dil_pat"};
        sc_in<uint32_t> i_act_xlim{"i_act_xlim"};
        sc_in<uint32_t> i_act_xstep{"i_act_xstep"};
        sc_in<uint32_t> i_act_ylim{"i_act_ylim"};
        sc_in<uint32_t> i_act_ystep{"i_act_ystep"};
        sc_in<uint32_t> i_act_chlim{"i_act_chlim"};
        sc_in<uint32_t> i_act_chstep{"i_act_chstep"};
        sc_in<uint32_t> i_act_til_xlim{"i_act_til_xlim"};
        sc_in<uint32_t> i_act_til_xstep{"i_act_til_xstep"};
        sc_in<uint32_t> i_act_til_ylim{"i_act_til_ylim"};
        sc_in<uint32_t> i_act_til_ystep{"i_act_til_ystep"};
        sc_in<uint32_t> i_act_base_addr{"i_act_base_addr"};   // = npu_top's s_act_base_addr_a

        sc_in<uint32_t> i_wei_incntlim{"i_wei_incntlim"};
        sc_in<uint32_t> i_wei_incntstep{"i_wei_incntstep"};
        sc_in<uint32_t> i_wei_wlim{"i_wei_wlim"};
        sc_in<uint32_t> i_wei_wstep{"i_wei_wstep"};
        sc_in<uint32_t> i_wei_klim{"i_wei_klim"};
        sc_in<uint32_t> i_wei_kstep{"i_wei_kstep"};
        sc_in<uint32_t> i_wei_til_klim{"i_wei_til_klim"};
        sc_in<uint32_t> i_wei_til_kstep{"i_wei_til_kstep"};
        sc_in<uint32_t> i_wei_cols_active{"i_wei_cols_active"};
        sc_in<uint32_t> i_wei_waligned{"i_wei_waligned"};
        sc_in<uint32_t> i_wei_base_addr{"i_wei_base_addr"};   // = npu_top's s_wei_base_addr_a

        sc_in<uint32_t> i_cxlim{"i_cxlim"};
        sc_in<uint32_t> i_cxstep{"i_cxstep"};
        sc_in<uint32_t> i_cklim{"i_cklim"};
        sc_in<uint32_t> i_ckstep{"i_ckstep"};
        sc_in<uint32_t> i_out_til_cylim{"i_out_til_cylim"};
        sc_in<uint32_t> i_out_til_cystep{"i_out_til_cystep"};
        sc_in<uint32_t> i_out_til_cklim{"i_out_til_cklim"};
        sc_in<uint32_t> i_out_til_ckstep{"i_out_til_ckstep"};
        sc_in<bool> i_out_preload_en{"i_out_preload_en"};
        sc_in<uint32_t> i_out_base_addr{"i_out_base_addr"};
        sc_in<sramc_mask_t<Y_DIM>> i_rows_active{"i_rows_active"};

        // ---- SRAM-A/B access (shared sram_inst, outside this block) ----
        sc_out<uint32_t> o_srama_addr{"o_srama_addr"};
        sc_out<bool> o_srama_rden{"o_srama_rden"};
        sc_in<act_vector_t<Y_DIM, T_ACT>> i_srama_data{"i_srama_data"};
        sc_out<uint32_t> o_sramb_addr{"o_sramb_addr"};
        sc_out<bool> o_sramb_rden{"o_sramb_rden"};
        sc_in<wei_vector_t<X_DIM, T_WEI>> i_sramb_data{"i_sramb_data"};

        // ---- SRAM-C raw-PSUM drain (goes straight to obp_inst_a; contract verified by the lane A tests) ----
        sc_out<uint32_t> o_sramc_addr{"o_sramc_addr"};
        sc_out<bool> o_sramc_wren{"o_sramc_wren"};
        sc_out<bool> o_sramc_rden{"o_sramc_rden"};
        sc_out<sramc_mask_t<Y_DIM>> o_sramc_wmask{"o_sramc_wmask"};
        sc_out<psum_vector_t<Y_DIM, T_PSUM>> o_sramc_wdata{"o_sramc_wdata"};
        sc_in<psum_vector_t<Y_DIM, T_PSUM>> i_sramc_rdata{"i_sramc_rdata"};

        // ---- array_inst boundary (shared PE array, outside this block) ----
        sc_out<act_vector_t<Y_DIM, T_ACT>> o_act_arr{"o_act_arr"};
        sc_out<wei_vector_t<X_DIM, T_WEI>> o_wei_arr{"o_wei_arr"};
        sc_in<psum_vector_t<Y_DIM, T_PSUM>> i_c_arr{"i_c_arr"};    // tu array_inst->o_c_arr_a
        sc_out<psum_vector_t<Y_DIM, T_PSUM>> o_c_arr{"o_c_arr"};   // toi array_inst->i_c_arr_a
        sc_out<bool> o_pipeline_en{"o_pipeline_en"};
        sc_out<bool> o_cscan_en{"o_cscan_en"};
        sc_out<sc_bv<X_DIM>> o_cswitch_arr{"o_cswitch_arr"};
        sc_out<bool> o_sa_clear{"o_sa_clear"};
        sc_out<uint32_t> o_context_id{"o_context_id"};   // -> array_inst->i_context_id_a

        // ---- Status (npu_top merges it with Lane B outside) ----
        sc_out<bool> o_done{"o_done"};
        sc_out<bool> o_deadlock{"o_deadlock"};
        sc_out<bool> o_active{"o_active"};

        SC_HAS_PROCESS(NativeLaneACoreA);
        NativeLaneACoreA(sc_module_name nm) : sc_module(nm)
        {
            ctrl_inst_a = new Control<X_DIM, Y_DIM, PE_LAT, EXTRA_CSREG>("ctrl_inst_a");
            act_feeder_a = new IfmapFeeder<Y_DIM, T_ACT, 1024, FIFO_DEPTH, false>("act_feeder_a");
            wei_feeder_a = new WeightFeeder<X_DIM, T_WEI, 1024, FIFO_DEPTH, 0, false>("wei_feeder_a");
            psm_inst_a = new Psm<X_DIM, Y_DIM, T_PSUM, SRAMC_CAP>("psm_inst_a");

            // Clock/reset
            ctrl_inst_a->i_clk(i_clk);
            ctrl_inst_a->i_rstn(i_rstn);
            ctrl_inst_a->i_soft_reset(i_soft_reset);
            act_feeder_a->i_clk(i_clk);
            act_feeder_a->i_rstn(i_rstn);
            wei_feeder_a->i_clk(i_clk);
            wei_feeder_a->i_rstn(i_rstn);
            psm_inst_a->i_clk(i_clk);
            psm_inst_a->i_rstn(i_rstn);

            // ctrl_inst_a bindings (identical to npu_top.h section "3. FSM Controller A bindings")
            ctrl_inst_a->i_start(i_start);
            ctrl_inst_a->o_done(o_done);
            ctrl_inst_a->o_feed_deadlock(o_deadlock);
            ctrl_inst_a->o_active(o_active);
            ctrl_inst_a->i_outbuf_done(s_psm_done_a);
            ctrl_inst_a->i_finalwrite(s_psm_finalwrite_a);
            ctrl_inst_a->i_shift_done(s_psm_shift_done_a);
            ctrl_inst_a->i_act_done(s_act_done_a);
            ctrl_inst_a->i_act_til_done(s_act_til_done_a);
            ctrl_inst_a->i_act_fifo_empty(s_act_fifo_empty_a);
            ctrl_inst_a->i_act_fifo_full(s_act_fifo_full_a);
            ctrl_inst_a->i_act_stall(s_act_stall_a);
            ctrl_inst_a->i_wei_done(s_wei_done_a);
            ctrl_inst_a->i_wei_til_done(s_wei_til_done_a);
            ctrl_inst_a->i_wei_fifo_empty(s_wei_fifo_empty_a);
            ctrl_inst_a->i_wei_fifo_full(s_wei_fifo_full_a);
            ctrl_inst_a->i_wei_stall(s_wei_stall_a);
            ctrl_inst_a->i_mvm_k(i_mvm_k);
            ctrl_inst_a->i_total_contexts(i_total_contexts);
            ctrl_inst_a->o_act_feeder_en(s_act_feeder_en_a);
            ctrl_inst_a->o_act_feeder_clear(s_act_feeder_clear_a);
            ctrl_inst_a->o_act_start(s_act_start_a);
            ctrl_inst_a->o_act_valid(s_act_valid_a);
            ctrl_inst_a->o_act_finalpush(s_act_finalpush_a);
            ctrl_inst_a->o_act_cnt_en(s_act_cnt_en_a);
            ctrl_inst_a->o_act_cnt_clear(s_act_cnt_clear_a);
            ctrl_inst_a->o_act_clearfifo(s_act_clearfifo_a);
            ctrl_inst_a->o_act_pop_en(s_act_pop_en_a);
            ctrl_inst_a->o_act_finalctx(s_act_finalctx_a);
            ctrl_inst_a->o_wei_feeder_en(s_wei_feeder_en_a);
            ctrl_inst_a->o_wei_feeder_clear(s_wei_feeder_clear_a);
            ctrl_inst_a->o_wei_start(s_wei_start_a);
            ctrl_inst_a->o_wei_valid(s_wei_valid_a);
            ctrl_inst_a->o_wei_finalpush(s_wei_finalpush_a);
            ctrl_inst_a->o_wei_cnt_en(s_wei_cnt_en_a);
            ctrl_inst_a->o_wei_cnt_clear(s_wei_cnt_clear_a);
            ctrl_inst_a->o_wei_clearfifo(s_wei_clearfifo_a);
            ctrl_inst_a->o_wei_pop_en(s_wei_pop_en_a);
            ctrl_inst_a->o_wei_cswitch(s_wei_cswitch_a);
            ctrl_inst_a->o_outbuf_start(s_psm_start_a);
            ctrl_inst_a->o_outbuf_reset(s_psm_reset_a);
            ctrl_inst_a->o_context_id(o_context_id);
            ctrl_inst_a->o_local_context_id(s_local_context_id_a);
            ctrl_inst_a->o_out_tile_id(s_out_tile_id_a);
            ctrl_inst_a->o_global_context_id(s_global_context_id_a);
            ctrl_inst_a->o_sa_clear(o_sa_clear);
            ctrl_inst_a->o_pipeline_en(o_pipeline_en);
            ctrl_inst_a->o_cswitch_arr(o_cswitch_arr);
            ctrl_inst_a->i_incntlim(i_incntlim);
            ctrl_inst_a->i_act_reps(i_act_reps);
            ctrl_inst_a->i_wei_reps(i_wei_reps);
            ctrl_inst_a->i_ncontexts(i_out_ncontexts);

            // act_feeder_a bindings (identical to npu_top.h section "4. Feeder A bindings")
            act_feeder_a->i_feeder_en(s_act_feeder_en_a);
            act_feeder_a->i_feeder_clear(s_act_feeder_clear_a);
            act_feeder_a->i_start(s_act_start_a);
            act_feeder_a->i_valid(s_act_valid_a);
            act_feeder_a->i_finalpush(s_act_finalpush_a);
            act_feeder_a->i_cnt_en(s_act_cnt_en_a);
            act_feeder_a->i_cnt_clear(s_act_cnt_clear_a);
            act_feeder_a->i_clearfifo(s_act_clearfifo_a);
            act_feeder_a->i_pop_en(s_act_pop_en_a);
            act_feeder_a->i_finalctx(s_act_finalctx_a);
            act_feeder_a->i_context_id(s_local_context_id_a);
            act_feeder_a->i_ncontexts(i_out_ncontexts);
            act_feeder_a->i_act_reps(i_act_reps);
            act_feeder_a->i_mvm_k(i_mvm_k);
            act_feeder_a->o_act_done(s_act_done_a);
            act_feeder_a->o_act_til_done(s_act_til_done_a);
            act_feeder_a->o_fifo_empty(s_act_fifo_empty_a);
            act_feeder_a->o_fifo_full(s_act_fifo_full_a);
            act_feeder_a->o_stall(s_act_stall_a);
            act_feeder_a->o_srama_addr(o_srama_addr);
            act_feeder_a->o_srama_rden(o_srama_rden);
            act_feeder_a->i_srama_data(i_srama_data);
            act_feeder_a->o_act_arr(o_act_arr);
            act_feeder_a->i_act_base_addr(i_act_base_addr);
            act_feeder_a->i_act_incntlim(i_act_incntlim);
            act_feeder_a->i_act_incntstep(i_act_incntstep);
            act_feeder_a->i_act_outcntlim(i_act_outcntlim);
            act_feeder_a->i_act_outcntstep(i_act_outcntstep);
            act_feeder_a->i_act_dil_pat(i_act_dil_pat);
            act_feeder_a->i_act_xlim(i_act_xlim);
            act_feeder_a->i_act_xstep(i_act_xstep);
            act_feeder_a->i_act_ylim(i_act_ylim);
            act_feeder_a->i_act_ystep(i_act_ystep);
            act_feeder_a->i_act_chlim(i_act_chlim);
            act_feeder_a->i_act_chstep(i_act_chstep);
            act_feeder_a->i_act_til_xlim(i_act_til_xlim);
            act_feeder_a->i_act_til_xstep(i_act_til_xstep);
            act_feeder_a->i_act_til_ylim(i_act_til_ylim);
            act_feeder_a->i_act_til_ystep(i_act_til_ystep);
            act_feeder_a->i_nsplit(i_nsplit);

            // wei_feeder_a bindings (identical to npu_top.h section "Weight Feeder A bindings")
            wei_feeder_a->i_wei_incntlim(i_wei_incntlim);
            wei_feeder_a->i_wei_incntstep(i_wei_incntstep);
            wei_feeder_a->i_wei_wlim(i_wei_wlim);
            wei_feeder_a->i_wei_wstep(i_wei_wstep);
            wei_feeder_a->i_wei_klim(i_wei_klim);
            wei_feeder_a->i_wei_kstep(i_wei_kstep);
            wei_feeder_a->i_wei_til_klim(i_wei_til_klim);
            wei_feeder_a->i_wei_til_kstep(i_wei_til_kstep);
            wei_feeder_a->i_wei_cols_active(i_wei_cols_active);
            wei_feeder_a->i_wei_waligned(i_wei_waligned);
            wei_feeder_a->i_feeder_en(s_wei_feeder_en_a);
            wei_feeder_a->i_feeder_clear(s_wei_feeder_clear_a);
            wei_feeder_a->i_start(s_wei_start_a);
            wei_feeder_a->i_valid(s_wei_valid_a);
            wei_feeder_a->i_finalpush(s_wei_finalpush_a);
            wei_feeder_a->i_cnt_en(s_wei_cnt_en_a);
            wei_feeder_a->i_cnt_clear(s_wei_cnt_clear_a);
            wei_feeder_a->i_clearfifo(s_wei_clearfifo_a);
            wei_feeder_a->i_pop_en(s_wei_pop_en_a);
            wei_feeder_a->i_cswitch(s_wei_cswitch_a);
            wei_feeder_a->o_wei_done(s_wei_done_a);
            wei_feeder_a->o_wei_til_done(s_wei_til_done_a);
            wei_feeder_a->o_fifo_empty(s_wei_fifo_empty_a);
            wei_feeder_a->o_fifo_full(s_wei_fifo_full_a);
            wei_feeder_a->o_stall(s_wei_stall_a);
            wei_feeder_a->o_sramb_addr(o_sramb_addr);
            wei_feeder_a->o_sramb_rden(o_sramb_rden);
            wei_feeder_a->i_sramb_data(i_sramb_data);
            wei_feeder_a->o_wei_arr(o_wei_arr);
            wei_feeder_a->i_context_id(s_local_context_id_a);
            wei_feeder_a->i_out_tile_id(s_out_tile_id_a);
            wei_feeder_a->i_ncontexts(i_out_ncontexts);
            wei_feeder_a->i_mvm_k(i_mvm_k);
            wei_feeder_a->i_wei_base_addr(i_wei_base_addr);
            wei_feeder_a->i_nsplit(i_nsplit);

            // psm_inst_a bindings (identical to npu_top.h section "8. PSM A bindings", Lane A part)
            psm_inst_a->i_c_arr(i_c_arr);
            psm_inst_a->o_c_arr(o_c_arr);
            psm_inst_a->i_sramc_rdata(i_sramc_rdata);
            psm_inst_a->o_sramc_addr(o_sramc_addr);
            psm_inst_a->o_sramc_wren(o_sramc_wren);
            psm_inst_a->o_sramc_rden(o_sramc_rden);
            psm_inst_a->o_sramc_wmask(o_sramc_wmask);
            psm_inst_a->o_sramc_wdata(o_sramc_wdata);
            psm_inst_a->i_fsm_start(s_psm_start_a);
            psm_inst_a->i_fsm_reset(s_psm_reset_a);
            psm_inst_a->i_pipeline_en(o_pipeline_en);
            psm_inst_a->o_done(s_psm_done_a);
            psm_inst_a->o_finalwrite(s_psm_finalwrite_a);
            psm_inst_a->o_shift_done(s_psm_shift_done_a);
            psm_inst_a->o_cscan_en(o_cscan_en);
            psm_inst_a->i_out_base_addr(i_out_base_addr);
            psm_inst_a->i_cxlim(i_cxlim);
            psm_inst_a->i_cxstep(i_cxstep);
            psm_inst_a->i_cklim(i_cklim);
            psm_inst_a->i_ckstep(i_ckstep);
            psm_inst_a->i_til_cylim(i_out_til_cylim);
            psm_inst_a->i_til_cystep(i_out_til_cystep);
            psm_inst_a->i_til_cklim(i_out_til_cklim);
            psm_inst_a->i_til_ckstep(i_out_til_ckstep);
            psm_inst_a->i_ncontexts(i_out_ncontexts);
            psm_inst_a->i_preload_en(i_out_preload_en);
            psm_inst_a->i_rows_active(i_rows_active);
            psm_inst_a->i_context_id(s_global_context_id_a);
            psm_inst_a->i_total_contexts(i_total_contexts);
        }

        ~NativeLaneACoreA()
        {
            delete ctrl_inst_a;
            delete act_feeder_a;
            delete wei_feeder_a;
            delete psm_inst_a;
        }

        Control<X_DIM, Y_DIM, PE_LAT, EXTRA_CSREG> *ctrl_inst_a{nullptr};
        IfmapFeeder<Y_DIM, T_ACT, 1024, FIFO_DEPTH, false> *act_feeder_a{nullptr};
        WeightFeeder<X_DIM, T_WEI, 1024, FIFO_DEPTH, 0, false> *wei_feeder_a{nullptr};
        Psm<X_DIM, Y_DIM, T_PSUM, SRAMC_CAP> *psm_inst_a{nullptr};

    private:
        // ---- Purely internal signals (only between the sub-modules above; nothing outside this block reads or
        // writes them) ----
        sc_signal<bool> s_act_feeder_en_a{"s_act_feeder_en_a"};
        sc_signal<bool> s_act_feeder_clear_a{"s_act_feeder_clear_a"};
        sc_signal<bool> s_act_start_a{"s_act_start_a"};
        sc_signal<bool> s_act_valid_a{"s_act_valid_a"};
        sc_signal<bool> s_act_finalpush_a{"s_act_finalpush_a"};
        sc_signal<bool> s_act_cnt_en_a{"s_act_cnt_en_a"};
        sc_signal<bool> s_act_cnt_clear_a{"s_act_cnt_clear_a"};
        sc_signal<bool> s_act_clearfifo_a{"s_act_clearfifo_a"};
        sc_signal<bool> s_act_pop_en_a{"s_act_pop_en_a"};
        sc_signal<bool> s_act_finalctx_a{"s_act_finalctx_a"};
        sc_signal<bool> s_act_done_a{"s_act_done_a"};
        sc_signal<bool> s_act_til_done_a{"s_act_til_done_a"};
        sc_signal<bool> s_act_fifo_empty_a{"s_act_fifo_empty_a"};
        sc_signal<bool> s_act_fifo_full_a{"s_act_fifo_full_a"};
        sc_signal<bool> s_act_stall_a{"s_act_stall_a"};

        sc_signal<bool> s_wei_feeder_en_a{"s_wei_feeder_en_a"};
        sc_signal<bool> s_wei_feeder_clear_a{"s_wei_feeder_clear_a"};
        sc_signal<bool> s_wei_start_a{"s_wei_start_a"};
        sc_signal<bool> s_wei_valid_a{"s_wei_valid_a"};
        sc_signal<bool> s_wei_finalpush_a{"s_wei_finalpush_a"};
        sc_signal<bool> s_wei_cnt_en_a{"s_wei_cnt_en_a"};
        sc_signal<bool> s_wei_cnt_clear_a{"s_wei_cnt_clear_a"};
        sc_signal<bool> s_wei_clearfifo_a{"s_wei_clearfifo_a"};
        sc_signal<bool> s_wei_pop_en_a{"s_wei_pop_en_a"};
        sc_signal<bool> s_wei_cswitch_a{"s_wei_cswitch_a"};
        sc_signal<bool> s_wei_done_a{"s_wei_done_a"};
        sc_signal<bool> s_wei_til_done_a{"s_wei_til_done_a"};
        sc_signal<bool> s_wei_fifo_empty_a{"s_wei_fifo_empty_a"};
        sc_signal<bool> s_wei_fifo_full_a{"s_wei_fifo_full_a"};
        sc_signal<bool> s_wei_stall_a{"s_wei_stall_a"};

        sc_signal<bool> s_psm_start_a{"s_psm_start_a"};
        sc_signal<bool> s_psm_reset_a{"s_psm_reset_a"};
        sc_signal<bool> s_psm_done_a{"s_psm_done_a"};
        sc_signal<bool> s_psm_finalwrite_a{"s_psm_finalwrite_a"};
        sc_signal<bool> s_psm_shift_done_a{"s_psm_shift_done_a"};

        sc_signal<uint32_t> s_local_context_id_a{"s_local_context_id_a"};
        sc_signal<uint32_t> s_out_tile_id_a{"s_out_tile_id_a"};
        sc_signal<uint32_t> s_global_context_id_a{"s_global_context_id_a"};
    };

    // RtlRefLaneACoreA: the RTL-accurate core of sauria_model with the same boundary (ctrl + feeders + psm) as
    // NativeLaneACoreA, assembled from four ported, unit-verified classes: control/rtl_ref_main_controller.h
    // (Control), data_feeder/rtl_ref_{ifmap,wei}_feeder.h and psm/rtl_ref_psm_top.h, all in namespace sauria_rtl
    // (distinct from sauria::, although many class names are the same).
    //
    // Scheduling: NativeLaneACoreA lets its sub-modules register their own SC_METHODs (each sensitive to
    // i_clk.pos(), cross-module reads through sc_signal with the natural 1-cycle delay). The sauria_rtl classes
    // follow sauria_model's "seam-ordered" convention (FX1_A3_SEAM_ORDER / FX1_A3_PSM_SEAM_ORDER, on by default in
    // rtl_ref_defaults.h): none registers an SC_METHOD; they are stepped explicitly with seam_step(). This block
    // therefore registers ONE SC_METHOD (tick_process() below) that calls seam_step() in sauria_model's npu_top.h
    // order:
    //     ctrl_inst_a->seam_step() -> act_feeder_a->seam_step() -> wei_feeder_a->seam_step()
    //     -> psm_inst_a->seam_step()
    // which avoids spurious 1-cycle-late cross-module reads that would not exist in the RTL.
    //
    // Peek hooks (std::function) give same-cycle combinational reads, as in the RTL; without them the reads fall
    // back to sc_signal and arrive one cycle late:
    //   ctrl_inst_a->peek_act_empty_/peek_wei_empty_          <- the feeders' peek_fifo_empty_now()
    //   ctrl_inst_a->peek_act_stall_/peek_wei_stall_          <- only under FX1_A3_STALL_DIRECT
    //   ctrl_inst_a->peek_act_full_ (+ til_done hooks)        <- only under FX1_A3_TILDONE_Q_GATE
    //   psm_inst_a->peek_fsm_start_   <- ctrl_inst_a->peek_outbuf_start_now()
    //   psm_inst_a->peek_pipe_en_     <- ctrl_inst_a->seam_pipeline_en_ (shadow written together with
    //                                     o_pipeline_en by seam_set_pipeline_en_())
    // psm_inst_a->peek_fsm_start_pre_ is left unconnected: the PSM then uses `false` (rtl_ref_psm_top.h), which only
    // matters on a rare diagnostic branch.
    //
    // Lane scope: `i_nsplit` is on the port list (same contract as NativeLaneACoreA) but not connected: the
    // sauria_rtl feeders are single-lane and have no row split. This core is therefore correct only when Lane A
    // owns all Y_DIM rows (nsplit = Y_DIM, sauria_model's own single-lane mode).
    //
    // o_active: sauria_rtl::Control has no such port; it is derived with a latch (set on i_start, cleared on o_done).
    // SRAMA_CAP / SRAMB_CAP: SRAM-A/B capacity in bytes seen by the feeders' address counters (the address wraps at
    // the capacity). The default 1024 (32 rows at Y_DIM = 32, int8) keeps small unit tests unchanged; testbenches
    // with the real SRAM sizes must pass them explicitly.
    template <
        int X_DIM = 32, int Y_DIM = 32,
        typename T_ACT = int8_t, typename T_WEI = int8_t, typename T_PSUM = int32_t,
        int FIFO_DEPTH = 16, int PE_LAT = 64, int EXTRA_CSREG = 1, int SRAMC_CAP = 1536,
        int SRAMA_CAP = 1024, int SRAMB_CAP = 1024>
    class RtlRefLaneACoreA : public sc_module
    {
    public:
        // ---- Port list identical to NativeLaneACoreA (names and types must not change) ----
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};
        sc_in<bool> i_soft_reset{"i_soft_reset"};
        sc_in<bool> i_start{"i_start"};

        sc_in<uint32_t> i_mvm_k{"i_mvm_k"};
        sc_in<uint32_t> i_total_contexts{"i_total_contexts"};
        sc_in<uint32_t> i_nsplit{"i_nsplit"}; // see the lane-scope note above: not connected internally

        sc_in<uint32_t> i_incntlim{"i_incntlim"};
        sc_in<uint32_t> i_act_reps{"i_act_reps"};
        sc_in<uint32_t> i_wei_reps{"i_wei_reps"};
        sc_in<uint32_t> i_out_ncontexts{"i_out_ncontexts"};

        sc_in<uint32_t> i_act_incntlim{"i_act_incntlim"};
        sc_in<uint32_t> i_act_incntstep{"i_act_incntstep"};
        sc_in<uint32_t> i_act_outcntlim{"i_act_outcntlim"};
        sc_in<uint32_t> i_act_outcntstep{"i_act_outcntstep"};
        sc_in<sc_bv<DILP_W>> i_act_dil_pat{"i_act_dil_pat"};
        sc_in<uint32_t> i_act_xlim{"i_act_xlim"};
        sc_in<uint32_t> i_act_xstep{"i_act_xstep"};
        sc_in<uint32_t> i_act_ylim{"i_act_ylim"};
        sc_in<uint32_t> i_act_ystep{"i_act_ystep"};
        sc_in<uint32_t> i_act_chlim{"i_act_chlim"};
        sc_in<uint32_t> i_act_chstep{"i_act_chstep"};
        sc_in<uint32_t> i_act_til_xlim{"i_act_til_xlim"};
        sc_in<uint32_t> i_act_til_xstep{"i_act_til_xstep"};
        sc_in<uint32_t> i_act_til_ylim{"i_act_til_ylim"};
        sc_in<uint32_t> i_act_til_ystep{"i_act_til_ystep"};
        sc_in<uint32_t> i_act_base_addr{"i_act_base_addr"};

        sc_in<uint32_t> i_wei_incntlim{"i_wei_incntlim"};
        sc_in<uint32_t> i_wei_incntstep{"i_wei_incntstep"};
        sc_in<uint32_t> i_wei_wlim{"i_wei_wlim"};
        sc_in<uint32_t> i_wei_wstep{"i_wei_wstep"};
        sc_in<uint32_t> i_wei_klim{"i_wei_klim"};
        sc_in<uint32_t> i_wei_kstep{"i_wei_kstep"};
        sc_in<uint32_t> i_wei_til_klim{"i_wei_til_klim"};
        sc_in<uint32_t> i_wei_til_kstep{"i_wei_til_kstep"};
        sc_in<uint32_t> i_wei_cols_active{"i_wei_cols_active"};
        sc_in<uint32_t> i_wei_waligned{"i_wei_waligned"};
        sc_in<uint32_t> i_wei_base_addr{"i_wei_base_addr"};

        sc_in<uint32_t> i_cxlim{"i_cxlim"};
        sc_in<uint32_t> i_cxstep{"i_cxstep"};
        sc_in<uint32_t> i_cklim{"i_cklim"};
        sc_in<uint32_t> i_ckstep{"i_ckstep"};
        sc_in<uint32_t> i_out_til_cylim{"i_out_til_cylim"};
        sc_in<uint32_t> i_out_til_cystep{"i_out_til_cystep"};
        sc_in<uint32_t> i_out_til_cklim{"i_out_til_cklim"};
        sc_in<uint32_t> i_out_til_ckstep{"i_out_til_ckstep"};
        sc_in<bool> i_out_preload_en{"i_out_preload_en"};
        sc_in<uint32_t> i_out_base_addr{"i_out_base_addr"};
        sc_in<sramc_mask_t<Y_DIM>> i_rows_active{"i_rows_active"};
#ifdef FX1_A3_PSM_INACTIVE_COLS
        // Testbench input X_DIM - X_used, wired to psm_inst_a->i_inactive_cols (see psm/rtl_ref_psm_top.h).
        // sauria_model's npu_top.h drives it from config_regs_inst->o_out_inactive_cols (a host-written register);
        // this block has no config_regs, so the testbench computes and drives it like the other configuration inputs.
        sc_in<uint32_t> i_inactive_cols{"i_inactive_cols"};
#endif

        sc_out<uint32_t> o_srama_addr{"o_srama_addr"};
        sc_out<bool> o_srama_rden{"o_srama_rden"};
        sc_in<act_vector_t<Y_DIM, T_ACT>> i_srama_data{"i_srama_data"};
        sc_out<uint32_t> o_sramb_addr{"o_sramb_addr"};
        sc_out<bool> o_sramb_rden{"o_sramb_rden"};
        sc_in<wei_vector_t<X_DIM, T_WEI>> i_sramb_data{"i_sramb_data"};

        sc_out<uint32_t> o_sramc_addr{"o_sramc_addr"};
        sc_out<bool> o_sramc_wren{"o_sramc_wren"};
        sc_out<bool> o_sramc_rden{"o_sramc_rden"};
        sc_out<sramc_mask_t<Y_DIM>> o_sramc_wmask{"o_sramc_wmask"};
        sc_out<psum_vector_t<Y_DIM, T_PSUM>> o_sramc_wdata{"o_sramc_wdata"};
        sc_in<psum_vector_t<Y_DIM, T_PSUM>> i_sramc_rdata{"i_sramc_rdata"};

        sc_out<act_vector_t<Y_DIM, T_ACT>> o_act_arr{"o_act_arr"};
        sc_out<wei_vector_t<X_DIM, T_WEI>> o_wei_arr{"o_wei_arr"};
        sc_in<psum_vector_t<Y_DIM, T_PSUM>> i_c_arr{"i_c_arr"};
        sc_out<psum_vector_t<Y_DIM, T_PSUM>> o_c_arr{"o_c_arr"};
        sc_out<bool> o_pipeline_en{"o_pipeline_en"};
        sc_out<bool> o_cscan_en{"o_cscan_en"};
        sc_out<sc_bv<X_DIM>> o_cswitch_arr{"o_cswitch_arr"};
        sc_out<bool> o_sa_clear{"o_sa_clear"};
        sc_out<uint32_t> o_context_id{"o_context_id"};

        sc_out<bool> o_done{"o_done"};
        sc_out<bool> o_deadlock{"o_deadlock"};
        sc_out<bool> o_active{"o_active"};

        SC_HAS_PROCESS(RtlRefLaneACoreA);
        RtlRefLaneACoreA(sc_module_name nm) : sc_module(nm)
        {
            ctrl_inst_a = new sauria_rtl::Control<X_DIM, Y_DIM, PE_LAT, EXTRA_CSREG>("ctrl_inst_a");
            act_feeder_a = new sauria_rtl::IfmapFeeder<Y_DIM, T_ACT, SRAMA_CAP, ACT_FIFO_POS>("act_feeder_a");
            wei_feeder_a = new sauria_rtl::WeightFeeder<X_DIM, T_WEI, SRAMB_CAP, WEI_FIFO_POS, 0>("wei_feeder_a");
            psm_inst_a = new sauria_rtl::Psm<X_DIM, Y_DIM, T_PSUM, SRAMC_CAP>("psm_inst_a");

            ctrl_inst_a->i_clk(i_clk);
            ctrl_inst_a->i_rstn(i_rstn);
            ctrl_inst_a->i_soft_reset(i_soft_reset);
            act_feeder_a->i_clk(i_clk);
            act_feeder_a->i_rstn(i_rstn);
            wei_feeder_a->i_clk(i_clk);
            wei_feeder_a->i_rstn(i_rstn);
            psm_inst_a->i_clk(i_clk);
            psm_inst_a->i_rstn(i_rstn);

            ctrl_inst_a->i_start(i_start);
            ctrl_inst_a->o_done(o_done);
            ctrl_inst_a->o_feed_deadlock(o_deadlock);
            ctrl_inst_a->i_outbuf_done(s_psm_done_a);
            ctrl_inst_a->i_finalwrite(s_psm_finalwrite_a);
            ctrl_inst_a->i_shift_done(s_psm_shift_done_a);
            ctrl_inst_a->i_act_done(s_act_done_a);
            ctrl_inst_a->i_act_til_done(s_act_til_done_a);
            ctrl_inst_a->i_act_fifo_empty(s_act_fifo_empty_a);
            ctrl_inst_a->i_act_fifo_full(s_act_fifo_full_a);
            ctrl_inst_a->i_act_stall(s_act_stall_a);
            ctrl_inst_a->i_wei_done(s_wei_done_a);
            ctrl_inst_a->i_wei_til_done(s_wei_til_done_a);
            ctrl_inst_a->i_wei_fifo_empty(s_wei_fifo_empty_a);
            ctrl_inst_a->i_wei_fifo_full(s_wei_fifo_full_a);
            ctrl_inst_a->i_wei_stall(s_wei_stall_a);
            ctrl_inst_a->i_mvm_k(i_mvm_k);
            ctrl_inst_a->i_total_contexts(i_total_contexts);
            ctrl_inst_a->o_act_feeder_en(s_act_feeder_en_a);
            ctrl_inst_a->o_act_feeder_clear(s_act_feeder_clear_a);
            ctrl_inst_a->o_act_start(s_act_start_a);
            ctrl_inst_a->o_act_valid(s_act_valid_a);
            ctrl_inst_a->o_act_finalpush(s_act_finalpush_a);
            ctrl_inst_a->o_act_cnt_en(s_act_cnt_en_a);
            ctrl_inst_a->o_act_cnt_clear(s_act_cnt_clear_a);
            ctrl_inst_a->o_act_clearfifo(s_act_clearfifo_a);
            ctrl_inst_a->o_act_pop_en(s_act_pop_en_a);
            ctrl_inst_a->o_act_finalctx(s_act_finalctx_a);
            ctrl_inst_a->o_wei_feeder_en(s_wei_feeder_en_a);
            ctrl_inst_a->o_wei_feeder_clear(s_wei_feeder_clear_a);
            ctrl_inst_a->o_wei_start(s_wei_start_a);
            ctrl_inst_a->o_wei_valid(s_wei_valid_a);
            ctrl_inst_a->o_wei_finalpush(s_wei_finalpush_a);
            ctrl_inst_a->o_wei_cnt_en(s_wei_cnt_en_a);
            ctrl_inst_a->o_wei_cnt_clear(s_wei_cnt_clear_a);
            ctrl_inst_a->o_wei_clearfifo(s_wei_clearfifo_a);
            ctrl_inst_a->o_wei_pop_en(s_wei_pop_en_a);
            ctrl_inst_a->o_wei_cswitch(s_wei_cswitch_a);
            ctrl_inst_a->o_outbuf_start(s_psm_start_a);
            ctrl_inst_a->o_outbuf_reset(s_psm_reset_a);
            ctrl_inst_a->o_context_id(o_context_id);
            ctrl_inst_a->o_local_context_id(s_local_context_id_a);
            ctrl_inst_a->o_out_tile_id(s_out_tile_id_a);
            ctrl_inst_a->o_global_context_id(s_global_context_id_a);
            ctrl_inst_a->o_sa_clear(o_sa_clear);
            ctrl_inst_a->o_pipeline_en(o_pipeline_en);
            ctrl_inst_a->o_cswitch_arr(o_cswitch_arr);
            ctrl_inst_a->o_softstall(s_softstall_unused_a);
            ctrl_inst_a->i_incntlim(i_incntlim);
            ctrl_inst_a->i_act_reps(i_act_reps);
            ctrl_inst_a->i_wei_reps(i_wei_reps);
            ctrl_inst_a->i_ncontexts(i_out_ncontexts);

            act_feeder_a->i_feeder_en(s_act_feeder_en_a);
            act_feeder_a->i_feeder_clear(s_act_feeder_clear_a);
            act_feeder_a->i_start(s_act_start_a);
            act_feeder_a->i_valid(s_act_valid_a);
            act_feeder_a->i_finalpush(s_act_finalpush_a);
            act_feeder_a->i_cnt_en(s_act_cnt_en_a);
            act_feeder_a->i_cnt_clear(s_act_cnt_clear_a);
            act_feeder_a->i_clearfifo(s_act_clearfifo_a);
            act_feeder_a->i_pop_en(s_act_pop_en_a);
            act_feeder_a->i_finalctx(s_act_finalctx_a);
            act_feeder_a->i_context_id(s_local_context_id_a);
            act_feeder_a->i_ncontexts(i_out_ncontexts);
            act_feeder_a->i_act_reps(i_act_reps);
            act_feeder_a->i_mvm_k(i_mvm_k);
            act_feeder_a->o_act_done(s_act_done_a);
            act_feeder_a->o_act_til_done(s_act_til_done_a);
            act_feeder_a->o_fifo_empty(s_act_fifo_empty_a);
            act_feeder_a->o_fifo_full(s_act_fifo_full_a);
            act_feeder_a->o_stall(s_act_stall_a);
            act_feeder_a->o_srama_addr(o_srama_addr);
            act_feeder_a->o_srama_rden(o_srama_rden);
            act_feeder_a->i_srama_data(i_srama_data);
            act_feeder_a->o_act_arr(o_act_arr);
            act_feeder_a->i_act_base_addr(i_act_base_addr);
            act_feeder_a->i_act_incntlim(i_act_incntlim);
            act_feeder_a->i_act_incntstep(i_act_incntstep);
            act_feeder_a->i_act_outcntlim(i_act_outcntlim);
            act_feeder_a->i_act_outcntstep(i_act_outcntstep);
            act_feeder_a->i_act_dil_pat(i_act_dil_pat);
            act_feeder_a->i_act_xlim(i_act_xlim);
            act_feeder_a->i_act_xstep(i_act_xstep);
            act_feeder_a->i_act_ylim(i_act_ylim);
            act_feeder_a->i_act_ystep(i_act_ystep);
            act_feeder_a->i_act_chlim(i_act_chlim);
            act_feeder_a->i_act_chstep(i_act_chstep);
            act_feeder_a->i_act_til_xlim(i_act_til_xlim);
            act_feeder_a->i_act_til_xstep(i_act_til_xstep);
            act_feeder_a->i_act_til_ylim(i_act_til_ylim);
            act_feeder_a->i_act_til_ystep(i_act_til_ystep);
            act_feeder_a->i_rows_active(i_rows_active);
            act_feeder_a->i_pipeline_en(o_pipeline_en);
            act_feeder_a->i_loc_woffs(s_act_loc_woffs_zero_a);

            wei_feeder_a->i_wei_incntlim(i_wei_incntlim);
            wei_feeder_a->i_wei_incntstep(i_wei_incntstep);
            wei_feeder_a->i_wei_wlim(i_wei_wlim);
            wei_feeder_a->i_wei_wstep(i_wei_wstep);
            wei_feeder_a->i_wei_klim(i_wei_klim);
            wei_feeder_a->i_wei_kstep(i_wei_kstep);
            wei_feeder_a->i_wei_til_klim(i_wei_til_klim);
            wei_feeder_a->i_wei_til_kstep(i_wei_til_kstep);
            wei_feeder_a->i_wei_cols_active(s_wei_cols_active64_a);
            wei_feeder_a->i_wei_waligned(i_wei_waligned);
            wei_feeder_a->i_feeder_en(s_wei_feeder_en_a);
            wei_feeder_a->i_feeder_clear(s_wei_feeder_clear_a);
            wei_feeder_a->i_start(s_wei_start_a);
            wei_feeder_a->i_valid(s_wei_valid_a);
            wei_feeder_a->i_finalpush(s_wei_finalpush_a);
            wei_feeder_a->i_cnt_en(s_wei_cnt_en_a);
            wei_feeder_a->i_cnt_clear(s_wei_cnt_clear_a);
            wei_feeder_a->i_clearfifo(s_wei_clearfifo_a);
            wei_feeder_a->i_pop_en(s_wei_pop_en_a);
            wei_feeder_a->i_cswitch(s_wei_cswitch_a);
            wei_feeder_a->o_wei_done(s_wei_done_a);
            wei_feeder_a->o_wei_til_done(s_wei_til_done_a);
            wei_feeder_a->o_fifo_empty(s_wei_fifo_empty_a);
            wei_feeder_a->o_fifo_full(s_wei_fifo_full_a);
            wei_feeder_a->o_stall(s_wei_stall_a);
            wei_feeder_a->o_sramb_addr(o_sramb_addr);
            wei_feeder_a->o_sramb_rden(o_sramb_rden);
            wei_feeder_a->i_sramb_data(i_sramb_data);
            wei_feeder_a->o_wei_arr(o_wei_arr);
            wei_feeder_a->i_context_id(s_local_context_id_a);
            wei_feeder_a->i_out_tile_id(s_out_tile_id_a);
            wei_feeder_a->i_ncontexts(i_out_ncontexts);
            wei_feeder_a->i_mvm_k(i_mvm_k);
            wei_feeder_a->i_wei_base_addr(i_wei_base_addr);
            wei_feeder_a->i_pipeline_en(o_pipeline_en);

            psm_inst_a->i_c_arr(i_c_arr);
            psm_inst_a->o_c_arr(o_c_arr);
            psm_inst_a->i_sramc_rdata(i_sramc_rdata);
            psm_inst_a->o_sramc_addr(o_sramc_addr);
            psm_inst_a->o_sramc_wren(o_sramc_wren);
            psm_inst_a->o_sramc_rden(o_sramc_rden);
            psm_inst_a->o_sramc_wmask(o_sramc_wmask);
            psm_inst_a->o_sramc_wdata(o_sramc_wdata);
            psm_inst_a->i_fsm_start(s_psm_start_a);
            psm_inst_a->i_fsm_reset(s_psm_reset_a);
            psm_inst_a->i_pipeline_en(o_pipeline_en);
            psm_inst_a->o_done(s_psm_done_a);
            psm_inst_a->o_finalwrite(s_psm_finalwrite_a);
            psm_inst_a->o_shift_done(s_psm_shift_done_a);
            psm_inst_a->o_cscan_en(o_cscan_en);
            psm_inst_a->i_out_base_addr(i_out_base_addr);
            psm_inst_a->i_cxlim(i_cxlim);
            psm_inst_a->i_cxstep(i_cxstep);
            psm_inst_a->i_cklim(i_cklim);
            psm_inst_a->i_ckstep(i_ckstep);
            psm_inst_a->i_til_cylim(i_out_til_cylim);
            psm_inst_a->i_til_cystep(i_out_til_cystep);
            psm_inst_a->i_til_cklim(i_out_til_cklim);
            psm_inst_a->i_til_ckstep(i_out_til_ckstep);
            psm_inst_a->i_ncontexts(i_out_ncontexts);
            psm_inst_a->i_preload_en(i_out_preload_en);
            psm_inst_a->i_rows_active(i_rows_active);
#ifdef FX1_A3_PSM_INACTIVE_COLS
            psm_inst_a->i_inactive_cols(i_inactive_cols);
#endif
            psm_inst_a->i_context_id(s_global_context_id_a);
            psm_inst_a->i_total_contexts(i_total_contexts);

            // i_wei_cols_active is uint32_t on this port list but sauria_rtl::WeightFeeder takes uint64_t (as the RTL).
            // tick_process() zero-extends it EVERY cycle before wei_feeder_a reads it (a one-time write in the
            // constructor would freeze it at 0 and block all weight data).

            // i_loc_woffs: sauria_rtl::IfmapFeeder input with no NativeLaneACoreA counterpart (local word offset per row).
            // Initialised to 0 here; tick_process() computes the real value every cycle (see there).
            s_act_loc_woffs_zero_a.write(act_vector_t<Y_DIM, uint32_t>());

            // Peek hooks (see the class comment). The set must match sauria_model/npu_top.h:145-195: stall hooks only
            // under FX1_A3_STALL_DIRECT, the full hook only for act under FX1_A3_TILDONE_Q_GATE. Extra hooks are
            // harmless with deep FIFOs but deadlock with the RTL depths (act 5 / wei 4).
            ctrl_inst_a->peek_act_empty_ = [this]() { return act_feeder_a->peek_fifo_empty_now(); };
            ctrl_inst_a->peek_wei_empty_ = [this]() { return wei_feeder_a->peek_fifo_empty_now(); };
#ifdef FX1_A3_STALL_DIRECT
            ctrl_inst_a->peek_act_stall_ = [this]() { return act_feeder_a->peek_stall_now(); };
            ctrl_inst_a->peek_wei_stall_ = [this]() { return wei_feeder_a->peek_stall_now(); };
#endif
#if defined(FX1_A3_TILDONE_Q_GATE) && defined(FX1_A3_IFMAP_FEEDER_RTL)
            // RTL-exact act til_done path into feeders_fsm (see rtl_ref_main_controller.h).
            ctrl_inst_a->peek_act_til_done_rtl_ = [this]() { return act_feeder_a->peek_til_done_rtl_now(); };
            ctrl_inst_a->peek_act_til_done_rtl_last_ = [this]() { return act_feeder_a->dbg_last_til_done_rtl(); };
            ctrl_inst_a->peek_act_full_ = [this]() { return act_feeder_a->peek_fifo_full_now(); };
            if (!ctrl_inst_a->peek_act_stall_)
                ctrl_inst_a->peek_act_stall_ = [this]() { return act_feeder_a->peek_stall_now(); };
#endif
            psm_inst_a->peek_fsm_start_ = [this]() { return ctrl_inst_a->peek_outbuf_start_now(); };
            psm_inst_a->peek_pipe_en_ = [this]() { return ctrl_inst_a->seam_pipeline_en_; };

            // Five SAME-CYCLE reads from Control into the feeders, as in sauria_model/npu_top.h:152-179 (same macros).
            // Without them the feeders read cnt_en / feeder_en / pipeline_en / valid / start through sc_signal, one
            // cycle late, and the counters stop one tap early (the last tap is lost). The seam_* members and
            // seam_fd() live in rtl_ref_main_controller.h.
#ifdef FX1_A3_CNT_EN_DIRECT
            act_feeder_a->seam_peek_cnt_en_ = [this]() { return ctrl_inst_a->seam_act_cnt_en_; };
            wei_feeder_a->seam_peek_cnt_en_ = [this]() { return ctrl_inst_a->seam_wei_cnt_en_; };
#endif
#ifdef FX1_A3_PIPE_EN_DIRECT
            act_feeder_a->seam_peek_pipe_en_ = [this]() { return ctrl_inst_a->seam_pipeline_en_; };
            wei_feeder_a->seam_peek_pipe_en_ = [this]() { return ctrl_inst_a->seam_pipeline_en_; };
#endif
#ifdef FX1_A3_VALID_DIRECT
            act_feeder_a->seam_peek_valid_ = [this]() { return ctrl_inst_a->seam_act_valid_; };
            wei_feeder_a->seam_peek_valid_ = [this]() { return ctrl_inst_a->seam_wei_valid_; };
#endif
#ifdef FX1_A3_START_DIRECT
            act_feeder_a->seam_peek_start_ = [this]() { return ctrl_inst_a->seam_act_start_; };
#endif
#ifdef FX1_A3_FDFSM_DIRECT
            act_feeder_a->seam_peek_fd_ = [this](int i) { return ctrl_inst_a->seam_fd(false, i); };
            wei_feeder_a->seam_peek_fd_ = [this](int i) { return ctrl_inst_a->seam_fd(true, i); };
#endif

            SC_METHOD(tick_process);
            sensitive << i_clk.pos();
        }

        ~RtlRefLaneACoreA()
        {
            delete ctrl_inst_a;
            delete act_feeder_a;
            delete wei_feeder_a;
            delete psm_inst_a;
        }

        sauria_rtl::Control<X_DIM, Y_DIM, PE_LAT, EXTRA_CSREG> *ctrl_inst_a{nullptr};
        // FIFO depths. The RTL uses different depths for act / wei (sauria_pkg.sv ACT_FIFO_POSITIONS = 5,
        // WEI_FIFO_POSITIONS = 4, as sauria_model/npu_top.h:52-58); -DFX1_A3_PORT_FIFO_SPLIT selects them.
        // Default: FIFO_DEPTH for both. With the RTL depths, multi-context tile shapes deadlock in this port (a
        // remaining difference to sauria_model on the stall/full path), so the split stays opt-in.
#ifdef FX1_A3_PORT_FIFO_SPLIT
        static constexpr int ACT_FIFO_POS = 5;   // RTL sauria_pkg.sv ACT_FIFO_POSITIONS
        static constexpr int WEI_FIFO_POS = 4;   // RTL WEI_FIFO_POSITIONS
#else
        static constexpr int ACT_FIFO_POS = FIFO_DEPTH;
        static constexpr int WEI_FIFO_POS = FIFO_DEPTH;
#endif
        sauria_rtl::IfmapFeeder<Y_DIM, T_ACT, SRAMA_CAP, ACT_FIFO_POS> *act_feeder_a{nullptr};
        sauria_rtl::WeightFeeder<X_DIM, T_WEI, SRAMB_CAP, WEI_FIFO_POS, 0> *wei_feeder_a{nullptr};
        sauria_rtl::Psm<X_DIM, Y_DIM, T_PSUM, SRAMC_CAP> *psm_inst_a{nullptr};

        // Debug hooks (read-only accessors, always compiled, no effect on behaviour): internal Control <-> feeder
        // signals for cycle-exact observation from a testbench.
        bool dbg_act_pop_en() const { return s_act_pop_en_a.read(); }
        bool dbg_wei_pop_en() const { return s_wei_pop_en_a.read(); }
        bool dbg_act_valid() const { return s_act_valid_a.read(); }
        bool dbg_wei_valid() const { return s_wei_valid_a.read(); }
        // Added to see inside WeiFeederRtl why b_arr is all zero.
        uint32_t dbg_wei_sramb_addr() const { return wei_feeder_a->dbg_sramb_addr(); }
        bool dbg_wei_finalpush_q2() const { return wei_feeder_a->dbg_finalpush_q2(); }
        int32_t dbg_wei_sram_data_q0() const { return wei_feeder_a->dbg_sram_data_q0(); }
        bool dbg_wei_done() const { return wei_feeder_a->dbg_done(); }
        bool dbg_wei_til_done() const { return wei_feeder_a->dbg_til_done(); }
        bool dbg_wei_push0() const { return wei_feeder_a->dbg_push0(); }
        int32_t dbg_wei_data0() const { return wei_feeder_a->dbg_data0(); }
        int32_t dbg_wei_din0() const { return wei_feeder_a->dbg_din0(); }
        bool dbg_wei_vdata() const { return wei_feeder_a->dbg_vdata(); }
        bool dbg_wei_cnten() const { return wei_feeder_a->dbg_cnten(); }
        bool dbg_wei_fen0() const { return wei_feeder_a->dbg_fen0(); }
        bool dbg_wei_in_ca0() const { return wei_feeder_a->dbg_in_ca0(); }
        bool dbg_wei_in_fen() const { return wei_feeder_a->dbg_in_fen(); }
        bool dbg_wei_li_fen() const { return wei_feeder_a->dbg_li_fen(); }
        bool dbg_wei_vq1() const { return wei_feeder_a->dbg_vq1(); }
        uint32_t dbg_wei_patn() const { return wei_feeder_a->dbg_patn(); }
        int dbg_wei_patf() const { return wei_feeder_a->dbg_patf(); }
        uint32_t dbg_wei_ract() const { return wei_feeder_a->dbg_ract(); }
        uint32_t dbg_wei_elm() const { return wei_feeder_a->dbg_elm(); }
        uint32_t dbg_wei_nfree() const { return wei_feeder_a->dbg_nfree(); }
        bool dbg_wei_pren() const { return wei_feeder_a->dbg_pren(); }
        bool dbg_wei_full_any() const { return wei_feeder_a->dbg_full_any(); }
        bool dbg_wei_stall_any() const { return wei_feeder_a->dbg_stall_any(); }
        // Debug hooks: weight FIFO empty-flag pipeline (see data_feeder/rtl_ref_wei_feeder_rtl.h).
        bool dbg_wei_fifo_empty_q1() const { return wei_feeder_a->dbg_fifo_empty_q1(); }
        bool dbg_wei_fifo_empty_q2() const { return wei_feeder_a->dbg_fifo_empty_q2(); }
        bool dbg_wei_fifo_empty_start() const { return wei_feeder_a->dbg_fifo_empty_start(); }
        uint32_t dbg_wei_fifo_ptr0() const { return wei_feeder_a->dbg_fifo_ptr0(); }
        // Debug hooks: empty/full of BOTH FIFOs, the inputs of Control's o_feed_deadlock
        // (act_deadlock = act_fifo_empty && wei_fifo_full, wei_deadlock = act_fifo_full && wei_fifo_empty).
        bool dbg_act_fifo_empty() const { return s_act_fifo_empty_a.read(); }
        bool dbg_act_fifo_full() const { return s_act_fifo_full_a.read(); }
        bool dbg_wei_fifo_full() const { return s_wei_fifo_full_a.read(); }
        bool dbg_wei_fifo_empty() const { return s_wei_fifo_empty_a.read(); }
        // Debug hooks: pass-through of Control's dbg_wei_hold() / dbg_feeders_state() / dbg_main_state()
        // (wei_cnt_hold_q_ of rtl_ref_feeders_fsm.h and the FSM states).
        bool dbg_ctrl_wei_hold() const { return ctrl_inst_a->dbg_wei_hold(); }
        int dbg_ctrl_feeders_state() const { return ctrl_inst_a->dbg_feeders_state(); }
        int dbg_ctrl_main_state() const { return ctrl_inst_a->dbg_main_state(); }
        // Debug hooks: the K counters of WeiIdxCnt.
        uint32_t dbg_wei_aux() const { return wei_feeder_a->dbg_aux(); }
        uint32_t dbg_wei_w() const { return wei_feeder_a->dbg_w(); }
        uint32_t dbg_wei_tilk() const { return wei_feeder_a->dbg_tilk(); }
        // Debug hooks: PsmShiftFsm ctx_cnt (counts i_fsm_start pulses, not distinct contexts -- see
        // psm/rtl_ref_psm_shift_fsm.h) and the psm_start signal itself.
        uint32_t dbg_psm_ctx_cnt() const { return psm_inst_a->dbg_fsm_ctx_cnt(); }
        bool dbg_psm_start() const { return s_psm_start_a.read(); }
        // Debug hooks: `o_context_id` (public port -> array_inst->i_context_id_a) is a DIFFERENT signal from
        // `s_local_context_id_a` (the one wired to the feeders' i_context_id, used by local_ch()/local_wave() to
        // compute context_y_offset); one accessor each.
        uint32_t dbg_local_context_id() const { return s_local_context_id_a.read(); }
        uint32_t dbg_global_context_id() const { return s_global_context_id_a.read(); }
        // Debug hook: `s_act_cnt_clear_a` (-> act_feeder_a->i_cnt_clear, decides whether IfmapIdxCnt applies
        // til_y_seed), to see whether it pulses between contexts.
        bool dbg_act_cnt_clear() const { return s_act_cnt_clear_a.read(); }
        // Debug hooks: live local_ch() / local_wave() / context_y_offset / i_context_id of act_feeder_a (no
        // pipeline delay).
        uint32_t dbg_act_local_ch() const { return act_feeder_a->dbg_local_ch(); }
        uint32_t dbg_act_local_wave() const { return act_feeder_a->dbg_local_wave(); }
        uint32_t dbg_act_context_y_offset() const { return act_feeder_a->dbg_context_y_offset(); }
        uint32_t dbg_act_i_context_id_raw() const { return act_feeder_a->dbg_i_context_id_raw(); }
        // Debug hooks: the FeedersFsm control signals of the weight branch that have no other accessor
        // (cnt_en, cswitch, finalpush, clearfifo).
        bool dbg_wei_raw_cnt_en() const { return s_wei_cnt_en_a.read(); }
        bool dbg_wei_cswitch() const { return s_wei_cswitch_a.read(); }
        bool dbg_wei_raw_finalpush() const { return s_wei_finalpush_a.read(); }
        bool dbg_wei_clearfifo() const { return s_wei_clearfifo_a.read(); }

    private:
        // seam_step() order of sauria_model's npu_top.h: Control -> act feeder -> wei feeder -> Psm, once per cycle.
        void tick_process()
        {
            // Zero-extend i_wei_cols_active (32 -> 64 bit) EVERY cycle before wei_feeder_a reads it. If it stayed 0,
            // cols_active[x] would be false, feeder_en never set, FeedDataManager::fifo_push never raised, and the
            // weight lanes would emit nothing although the SRAM reads are correct.
            s_wei_cols_active64_a.write((uint64_t)i_wei_cols_active.read());

            // i_loc_woffs per row, as sauria_model/config_regs.h:170 `make_loc_woffs()`: lw[y] = rows_active[y] ?
            // y * act_stride : 0, with act_stride = TIL_XSTEP / Y_used (tb_evaluate.cpp:888-894). A constant 0 would make
            // every output row read the same SRAM-A offset (only correct for Y_used = 1).
            {
                auto ra = i_rows_active.read();
                uint32_t y_used = 0;
                for (int y = 0; y < Y_DIM; y++)
                    if (ra[y]) y_used++;
                const uint32_t stride = y_used ? (i_act_til_xstep.read() / y_used) : 1u;
                act_vector_t<Y_DIM, uint32_t> lw;
                for (int y = 0; y < Y_DIM; y++)
                    lw[y] = ra[y] ? (uint32_t)((uint32_t)y * stride) : 0u;
                s_act_loc_woffs_zero_a.write(lw);
            }

            ctrl_inst_a->seam_step();
            act_feeder_a->seam_step();
            wei_feeder_a->seam_step();
            psm_inst_a->seam_step();

            // o_active: latch set on i_start and cleared on a real o_done pulse (sauria_rtl::Control has no such port).
            if (i_start.read() && !active_latch_)
            {
                active_latch_ = true;
            }
            else if (o_done.read() && active_latch_)
            {
                active_latch_ = false;
            }
            if (!i_rstn.read())
            {
                active_latch_ = false;
            }
            o_active.write(active_latch_);
        }

        bool active_latch_{false};

        sc_signal<bool> s_act_feeder_en_a{"s_act_feeder_en_a"};
        sc_signal<bool> s_act_feeder_clear_a{"s_act_feeder_clear_a"};
        sc_signal<bool> s_act_start_a{"s_act_start_a"};
        sc_signal<bool> s_act_valid_a{"s_act_valid_a"};
        sc_signal<bool> s_act_finalpush_a{"s_act_finalpush_a"};
        sc_signal<bool> s_act_cnt_en_a{"s_act_cnt_en_a"};
        sc_signal<bool> s_act_cnt_clear_a{"s_act_cnt_clear_a"};
        sc_signal<bool> s_act_clearfifo_a{"s_act_clearfifo_a"};
        sc_signal<bool> s_act_pop_en_a{"s_act_pop_en_a"};
        sc_signal<bool> s_act_finalctx_a{"s_act_finalctx_a"};
        sc_signal<bool> s_act_done_a{"s_act_done_a"};
        sc_signal<bool> s_act_til_done_a{"s_act_til_done_a"};
        sc_signal<bool> s_act_fifo_empty_a{"s_act_fifo_empty_a"};
        sc_signal<bool> s_act_fifo_full_a{"s_act_fifo_full_a"};
        sc_signal<bool> s_act_stall_a{"s_act_stall_a"};

        sc_signal<bool> s_wei_feeder_en_a{"s_wei_feeder_en_a"};
        sc_signal<bool> s_wei_feeder_clear_a{"s_wei_feeder_clear_a"};
        sc_signal<bool> s_wei_start_a{"s_wei_start_a"};
        sc_signal<bool> s_wei_valid_a{"s_wei_valid_a"};
        sc_signal<bool> s_wei_finalpush_a{"s_wei_finalpush_a"};
        sc_signal<bool> s_wei_cnt_en_a{"s_wei_cnt_en_a"};
        sc_signal<bool> s_wei_cnt_clear_a{"s_wei_cnt_clear_a"};
        sc_signal<bool> s_wei_clearfifo_a{"s_wei_clearfifo_a"};
        sc_signal<bool> s_wei_pop_en_a{"s_wei_pop_en_a"};
        sc_signal<bool> s_wei_cswitch_a{"s_wei_cswitch_a"};
        sc_signal<bool> s_wei_done_a{"s_wei_done_a"};
        sc_signal<bool> s_wei_til_done_a{"s_wei_til_done_a"};
        sc_signal<bool> s_wei_fifo_empty_a{"s_wei_fifo_empty_a"};
        sc_signal<bool> s_wei_fifo_full_a{"s_wei_fifo_full_a"};
        sc_signal<bool> s_wei_stall_a{"s_wei_stall_a"};

        sc_signal<bool> s_psm_start_a{"s_psm_start_a"};
        sc_signal<bool> s_psm_reset_a{"s_psm_reset_a"};
        sc_signal<bool> s_psm_done_a{"s_psm_done_a"};
        sc_signal<bool> s_psm_finalwrite_a{"s_psm_finalwrite_a"};
        sc_signal<bool> s_psm_shift_done_a{"s_psm_shift_done_a"};

        sc_signal<uint32_t> s_local_context_id_a{"s_local_context_id_a"};
        sc_signal<uint32_t> s_out_tile_id_a{"s_out_tile_id_a"};
        sc_signal<uint32_t> s_global_context_id_a{"s_global_context_id_a"};

        sc_signal<uint64_t> s_wei_cols_active64_a{"s_wei_cols_active64_a"};
        sc_signal<act_vector_t<Y_DIM, uint32_t>> s_act_loc_woffs_zero_a{"s_act_loc_woffs_zero_a"};
        sc_signal<bool> s_softstall_unused_a{"s_softstall_unused_a"};
    };

    // Backend selected at build time with -DSAURIA_CORE_BACKEND_RTL_REF (RtlRefLaneACoreA); default = native.
#if defined(SAURIA_CORE_BACKEND_RTL_REF)
    template <int X_DIM, int Y_DIM, typename T_ACT, typename T_WEI, typename T_PSUM,
              int FIFO_DEPTH, int PE_LAT, int EXTRA_CSREG, int SRAMC_CAP>
    using LaneACoreBlockA = RtlRefLaneACoreA<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, FIFO_DEPTH, PE_LAT, EXTRA_CSREG, SRAMC_CAP>;
#else
    template <int X_DIM, int Y_DIM, typename T_ACT, typename T_WEI, typename T_PSUM,
              int FIFO_DEPTH, int PE_LAT, int EXTRA_CSREG, int SRAMC_CAP>
    using LaneACoreBlockA = NativeLaneACoreA<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, FIFO_DEPTH, PE_LAT, EXTRA_CSREG, SRAMC_CAP>;
#endif

} // namespace sauria

#endif // SAURIA_NATIVE_LANE_A_CORE_H
