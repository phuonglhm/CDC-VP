// SPDX-License-Identifier: Apache-2.0
//
// The matrix-only Sauria composition: the extraction Phase 5 exists to produce.
//
// ## What is here, and what is not
//
// `TPU_V3_PHASE5_AUDIT.md` §4 lists what the plan says to keep and drop, and the
// closure check in that section is what makes this file possible: of the 46
// signals a kept module reads that `NpuTop` drove from elsewhere, 41 come from
// `ConfigRegs` and 5 from `Sram`. Nothing comes from `Obp`, `Rce`,
// `ReductionEngine`, `SauriaDma` or `InstructionDecoder`. So the excluded blocks
// need no stubs — they are simply absent.
//
//   kept       ConfigRegs, Control x2, IfmapFeeder x2, WeightFeeder x2,
//              SystolicArray, Psm x2
//   replaced   Sram -> tile_staging_store (decision record D17)
//   absent     SauriaDma, InstructionDecoder, Obp x2, Rce x2,
//              ReductionEngine x2, NpuTop itself
//
// ## The wiring was generated, not typed
//
// The 429 port bindings and 204 signal declarations below were extracted from
// `npu_top.h` by script and filtered to the kept instances, rather than
// retyped. Four hundred hand-copied bindings would contain mistakes, and the
// ones that matter would not stop the build — a swapped pair of same-typed
// signals elaborates cleanly and computes the wrong answer, which is the
// failure mode this phase keeps having to design against.
//
// Two edits were made to the generated text, and both are recorded here because
// they are the only places this composition departs from the source's own:
//
//   * `sram_inst`'s 35 bindings are replaced by the staging store's. That is
//     D17: the store presents the same signal-level ports with the same
//     one-cycle registered read, and is filled and drained over
//     `neo_local_sram_if` instead of by a host port.
//   * the per-lane base-address mux collapses. `NpuTop` selects between the
//     instruction decoder's base addresses and `ConfigRegs`' ones; with the
//     decoder excluded there is nothing to select, so `ConfigRegs`' value goes
//     to both lanes directly.
//
// ## The glue `NpuTop` kept for itself
//
// Generating the bindings was not enough, and the way that surfaced is worth
// recording. `NpuTop` is not only a container: five of its `SC_METHOD`s drive
// internal signals that the kept modules read. Filtering *instances* keeps every
// binding those signals appear in and none of the processes that write them, so
// the first version of this file elaborated with eight signals that had a reader
// and no writer — including `s_act_arr_to_array_*` and `s_wei_arr_to_array_*`,
// the array's operand inputs. The array was being fed zeros.
//
// `test_matrix_composition.cpp` could not have caught it: SystemC requires
// *ports* to be bound, and an `sc_signal` with no writer is legal — it sits at
// its default value. So `driver_coverage_gate.cmake` checks the property
// directly, and every one of the five processes below is ported from
// `npu_top.h`:
//
//   start_reset_logic     `i_start || s_cfg_start`, with the decoder's
//                         `s_trigger_start_*` terms dropped
//   nsplit_mux_logic      collapses like the base-address mux above
//   done_latch_logic      clocked; the composition's only output handshake
//   deadlock_merge_logic  `s_deadlock_a || s_deadlock_b`
//   array_operand_feed    the pass-through half of `debug_ref_stream_mux`
//
// The last one is the one to be careful about. In the source it is a mux with a
// debug reference-stream injector on the other leg, gated by a member that is
// initialised `false` and assigned nowhere — so the injector is dead code and
// only the pass-through is reproduced. But it is an `SC_METHOD` on `i_clk.pos()`,
// which makes it a *register* in the array's operand path. A combinational
// pass-through here would compile, elaborate, and shift the array's pipeline by
// one cycle relative to the feeders' `rden_q1`/`rden_q2` recovery — arriving as
// wrong arithmetic rather than as a failure.

#pragma once

#include <cstdint>

#include <systemc>

#include "tpu_v3/sauria/sauria_geometry.h"
#include "tpu_v3/sauria/tile_staging_store.h"

#include "config_regs.h"
#include "control/main_controller.h"
#include "data_feeder/ifmap_feeder.h"
#include "data_feeder/wei_feeder.h"
#include "psm/psm_top.h"
#include "systolic_array/sa_array.h"

namespace cdc::components::tpu_v3::sauria {

/// One matrix engine: the Sauria array with the minimum feeder, sequencer and
/// result-collection logic around it, and TPU_V3 storage underneath.
///
/// Template parameters come from the extracted profile via `sauria_geometry.h`;
/// they are never defaulted. `SystolicArray` defaults to 32x64 and `NpuTop` to
/// 32x32, so a default here would silently build a different engine.
template <int X_DIM, int Y_DIM, typename T_ACT, typename T_WEI, typename T_PSUM,
          int SRAMA_CAP, int SRAMB_CAP, int SRAMC_CAP, int FIFO_DEPTH = 16,
          int PE_LAT = X_DIM + Y_DIM, int EXTRA_CSREG = 1>
class matrix_composition : public sc_core::sc_module {
public:
    using store_t =
        tile_staging_store<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM>;

    // ── the composition's own ports ──────────────────────────────────────────
    //
    // `NpuTop` exposed a host bus, a start/done handshake and a DRAM pointer.
    // This exposes the clock, reset, the config host port that loads
    // `ConfigRegs`, and the few runtime values that were `NpuTop` inputs. The
    // adapter above drives them from the frozen `SA_CONTROL` register map.
    sc_core::sc_in<bool> i_clk{"i_clk"};
    sc_core::sc_in<bool> i_rstn{"i_rstn"};
    sc_core::sc_in<bool> i_soft_reset{"i_soft_reset"};

    /// Level-sensitive start, ORed with the one the config register produces.
    ///
    /// Both exist in the source and both are kept, because they are not
    /// interchangeable: the register write is what firmware uses, and this port
    /// is what a testbench uses to start the engine without going through
    /// `ConfigRegs`' auto-clearing `r_start`. The adapter drives the register.
    sc_core::sc_in<bool> i_start{"i_start"};

    /// Latched completion, and the merged feeder-deadlock flag.
    sc_core::sc_out<bool> o_done{"o_done"};
    sc_core::sc_out<bool> o_deadlock{"o_deadlock"};

    sc_core::sc_in<std::uint32_t> i_host_addr{"i_host_addr"};
    sc_core::sc_in<bool> i_host_wren{"i_host_wren"};
    sc_core::sc_in<bool> i_host_rden{"i_host_rden"};
    sc_core::sc_in<::sauria::host_data_t> i_host_wdata{"i_host_wdata"};
    sc_core::sc_in<::sauria::host_mask_t> i_host_wmask{"i_host_wmask"};

    sc_core::sc_in<float> i_threshold{"i_threshold"};
    sc_core::sc_in<std::uint32_t> i_mvm_k{"i_mvm_k"};
    sc_core::sc_in<std::uint32_t> i_total_contexts{"i_total_contexts"};

    /// Filled and drained by the adapter's prefetch/writeback controllers.
    store_t store;

    SC_HAS_PROCESS(matrix_composition);

    explicit matrix_composition(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , store("store", SRAMA_CAP, SRAMB_CAP, SRAMC_CAP)
        , config_regs_inst("config_regs_inst")
        , ctrl_inst_a("ctrl_inst_a")
        , ctrl_inst_b("ctrl_inst_b")
        , act_feeder_a("act_feeder_a")
        , act_feeder_b("act_feeder_b")
        , wei_feeder_a("wei_feeder_a")
        , wei_feeder_b("wei_feeder_b")
        , array_inst("array_inst", ::sauria::PeConfig{})
        , psm_inst_a("psm_inst_a")
        , psm_inst_b("psm_inst_b")
    {
        bind_all();
    }

private:
    // ── the generated signal declarations ────────────────────────────────────
    sc_core::sc_signal<::sauria::act_vector_t<Y_DIM, T_ACT>> s_act_arr_a{"s_act_arr_a"};
    sc_core::sc_signal<::sauria::act_vector_t<Y_DIM, T_ACT>> s_act_arr_b{"s_act_arr_b"};
    sc_core::sc_signal<::sauria::act_vector_t<Y_DIM, T_ACT>> s_act_arr_to_array_a{"s_act_arr_to_array_a"};
    sc_core::sc_signal<::sauria::act_vector_t<Y_DIM, T_ACT>> s_act_arr_to_array_b{"s_act_arr_to_array_b"};
    sc_core::sc_signal<uint32_t> s_act_base_addr{"s_act_base_addr"};
    sc_core::sc_signal<uint32_t> s_act_base_addr_a{"s_act_base_addr_a"};
    sc_core::sc_signal<uint32_t> s_act_base_addr_b{"s_act_base_addr_b"};
    sc_core::sc_signal<uint32_t> s_act_chlim{"s_act_chlim"};
    sc_core::sc_signal<uint32_t> s_act_chstep{"s_act_chstep"};
    sc_core::sc_signal<bool> s_act_clearfifo_a{"s_act_clearfifo_a"};
    sc_core::sc_signal<bool> s_act_clearfifo_b{"s_act_clearfifo_b"};
    sc_core::sc_signal<bool> s_act_cnt_clear_a{"s_act_cnt_clear_a"};
    sc_core::sc_signal<bool> s_act_cnt_clear_b{"s_act_cnt_clear_b"};
    sc_core::sc_signal<bool> s_act_cnt_en_a{"s_act_cnt_en_a"};
    sc_core::sc_signal<bool> s_act_cnt_en_b{"s_act_cnt_en_b"};
    sc_core::sc_signal<bool> s_act_done_a{"s_act_done_a"};
    sc_core::sc_signal<bool> s_act_done_b{"s_act_done_b"};
    sc_core::sc_signal<bool> s_act_feeder_clear_a{"s_act_feeder_clear_a"};
    sc_core::sc_signal<bool> s_act_feeder_clear_b{"s_act_feeder_clear_b"};
    sc_core::sc_signal<bool> s_act_feeder_en_a{"s_act_feeder_en_a"};
    sc_core::sc_signal<bool> s_act_feeder_en_b{"s_act_feeder_en_b"};
    sc_core::sc_signal<bool> s_act_fifo_empty_a{"s_act_fifo_empty_a"};
    sc_core::sc_signal<bool> s_act_fifo_empty_b{"s_act_fifo_empty_b"};
    sc_core::sc_signal<bool> s_act_fifo_full_a{"s_act_fifo_full_a"};
    sc_core::sc_signal<bool> s_act_fifo_full_b{"s_act_fifo_full_b"};
    sc_core::sc_signal<bool> s_act_finalctx_a{"s_act_finalctx_a"};
    sc_core::sc_signal<bool> s_act_finalctx_b{"s_act_finalctx_b"};
    sc_core::sc_signal<bool> s_act_finalpush_a{"s_act_finalpush_a"};
    sc_core::sc_signal<bool> s_act_finalpush_b{"s_act_finalpush_b"};
    sc_core::sc_signal<uint32_t> s_act_incntlim{"s_act_incntlim"};
    sc_core::sc_signal<uint32_t> s_act_incntstep{"s_act_incntstep"};
    sc_core::sc_signal<uint32_t> s_act_outcntlim{"s_act_outcntlim"};
    sc_core::sc_signal<uint32_t> s_act_outcntstep{"s_act_outcntstep"};
    sc_core::sc_signal<bool> s_act_pop_en_a{"s_act_pop_en_a"};
    sc_core::sc_signal<bool> s_act_pop_en_b{"s_act_pop_en_b"};
    sc_core::sc_signal<uint32_t> s_act_reps{"s_act_reps"};
    sc_core::sc_signal<bool> s_act_stall_a{"s_act_stall_a"};
    sc_core::sc_signal<bool> s_act_stall_b{"s_act_stall_b"};
    sc_core::sc_signal<bool> s_act_start_a{"s_act_start_a"};
    sc_core::sc_signal<bool> s_act_start_b{"s_act_start_b"};
    sc_core::sc_signal<bool> s_act_til_done_a{"s_act_til_done_a"};
    sc_core::sc_signal<bool> s_act_til_done_b{"s_act_til_done_b"};
    sc_core::sc_signal<uint32_t> s_act_til_xlim{"s_act_til_xlim"};
    sc_core::sc_signal<uint32_t> s_act_til_xstep{"s_act_til_xstep"};
    sc_core::sc_signal<uint32_t> s_act_til_ylim{"s_act_til_ylim"};
    sc_core::sc_signal<uint32_t> s_act_til_ystep{"s_act_til_ystep"};
    sc_core::sc_signal<bool> s_act_valid_a{"s_act_valid_a"};
    sc_core::sc_signal<bool> s_act_valid_b{"s_act_valid_b"};
    sc_core::sc_signal<uint32_t> s_act_xlim{"s_act_xlim"};
    sc_core::sc_signal<uint32_t> s_act_xstep{"s_act_xstep"};
    sc_core::sc_signal<uint32_t> s_act_ylim{"s_act_ylim"};
    sc_core::sc_signal<uint32_t> s_act_ystep{"s_act_ystep"};
    sc_core::sc_signal<uint32_t> s_cfg_profile{"s_cfg_profile"};
    sc_core::sc_signal<bool> s_cfg_soft_reset{"s_cfg_soft_reset"};
    sc_core::sc_signal<bool> s_cfg_start{"s_cfg_start"};
    sc_core::sc_signal<uint32_t> s_cklim{"s_cklim"};
    sc_core::sc_signal<uint32_t> s_ckstep{"s_ckstep"};
    sc_core::sc_signal<uint32_t> s_config_nsplit{"s_config_nsplit"};
    sc_core::sc_signal<uint32_t> s_context_id_a{"s_context_id_a"};
    sc_core::sc_signal<uint32_t> s_context_id_b{"s_context_id_b"};
    sc_core::sc_signal<bool> s_cscan_en_a{"s_cscan_en_a"};
    sc_core::sc_signal<bool> s_cscan_en_b{"s_cscan_en_b"};
    sc_core::sc_signal<sc_dt::sc_bv<X_DIM>> s_cswitch_arr_a{"s_cswitch_arr_a"};
    sc_core::sc_signal<sc_dt::sc_bv<X_DIM>> s_cswitch_arr_b{"s_cswitch_arr_b"};
    sc_core::sc_signal<bool> s_ctrl_active_a{"s_ctrl_active_a"};
    sc_core::sc_signal<bool> s_ctrl_active_b{"s_ctrl_active_b"};
    sc_core::sc_signal<bool> s_ctrl_done_a{"s_ctrl_done_a"};
    sc_core::sc_signal<bool> s_ctrl_done_b{"s_ctrl_done_b"};
    sc_core::sc_signal<bool> s_ctrl_reset_internal{"s_ctrl_reset_internal"};
    sc_core::sc_signal<uint32_t> s_cxlim{"s_cxlim"};
    sc_core::sc_signal<uint32_t> s_cxstep{"s_cxstep"};
    sc_core::sc_signal<bool> s_deadlock_a{"s_deadlock_a"};
    sc_core::sc_signal<bool> s_deadlock_b{"s_deadlock_b"};
    sc_core::sc_signal<sc_dt::sc_bv<::sauria::DILP_W>> s_dil_pat{"s_dil_pat"};
    sc_core::sc_signal<uint32_t> s_dilation{"s_dilation"};
    sc_core::sc_signal<uint32_t> s_global_context_id_a{"s_global_context_id_a"};
    sc_core::sc_signal<uint32_t> s_global_context_id_b{"s_global_context_id_b"};
    sc_core::sc_signal<::sauria::host_data_t> s_host_rdata_cfg{"s_host_rdata_cfg"};
    sc_core::sc_signal<uint32_t> s_in_c{"s_in_c"};
    sc_core::sc_signal<uint32_t> s_in_h{"s_in_h"};
    sc_core::sc_signal<uint32_t> s_in_w{"s_in_w"};
    sc_core::sc_signal<uint32_t> s_incntlim{"s_incntlim"};
    sc_core::sc_signal<uint32_t> s_kernel_h{"s_kernel_h"};
    sc_core::sc_signal<uint32_t> s_kernel_w{"s_kernel_w"};
    sc_core::sc_signal<uint32_t> s_local_context_id_a{"s_local_context_id_a"};
    sc_core::sc_signal<uint32_t> s_local_context_id_b{"s_local_context_id_b"};
    sc_core::sc_signal<uint32_t> s_nsplit{"s_nsplit"};
    sc_core::sc_signal<uint32_t> s_obp_cfg_a{"s_obp_cfg_a"};
    sc_core::sc_signal<uint32_t> s_obp_cfg_b{"s_obp_cfg_b"};
    sc_core::sc_signal<uint32_t> s_out_base_addr{"s_out_base_addr"};
    sc_core::sc_signal<uint32_t> s_out_inactive_cols{"s_out_inactive_cols"};
    sc_core::sc_signal<uint32_t> s_out_ncontexts{"s_out_ncontexts"};
    sc_core::sc_signal<bool> s_out_preload_en{"s_out_preload_en"};
    sc_core::sc_signal<uint32_t> s_out_til_cklim{"s_out_til_cklim"};
    sc_core::sc_signal<uint32_t> s_out_til_ckstep{"s_out_til_ckstep"};
    sc_core::sc_signal<uint32_t> s_out_til_cylim{"s_out_til_cylim"};
    sc_core::sc_signal<uint32_t> s_out_til_cystep{"s_out_til_cystep"};
    sc_core::sc_signal<uint32_t> s_out_tile_id_a{"s_out_tile_id_a"};
    sc_core::sc_signal<uint32_t> s_out_tile_id_b{"s_out_tile_id_b"};
    sc_core::sc_signal<uint32_t> s_padding{"s_padding"};
    sc_core::sc_signal<bool> s_pipeline_en_a{"s_pipeline_en_a"};
    sc_core::sc_signal<bool> s_pipeline_en_b{"s_pipeline_en_b"};
    sc_core::sc_signal<bool> s_psm_done_a{"s_psm_done_a"};
    sc_core::sc_signal<bool> s_psm_done_b{"s_psm_done_b"};
    sc_core::sc_signal<bool> s_psm_finalwrite_a{"s_psm_finalwrite_a"};
    sc_core::sc_signal<bool> s_psm_finalwrite_b{"s_psm_finalwrite_b"};
    sc_core::sc_signal<bool> s_psm_reset_a{"s_psm_reset_a"};
    sc_core::sc_signal<bool> s_psm_reset_b{"s_psm_reset_b"};
    sc_core::sc_signal<bool> s_psm_shift_done_a{"s_psm_shift_done_a"};
    sc_core::sc_signal<bool> s_psm_shift_done_b{"s_psm_shift_done_b"};
    sc_core::sc_signal<bool> s_psm_start_a{"s_psm_start_a"};
    sc_core::sc_signal<bool> s_psm_start_b{"s_psm_start_b"};
    sc_core::sc_signal<::sauria::psum_vector_t<Y_DIM, T_PSUM>> s_psm_to_sa_c_a{"s_psm_to_sa_c_a"};
    sc_core::sc_signal<::sauria::psum_vector_t<Y_DIM, T_PSUM>> s_psm_to_sa_c_b{"s_psm_to_sa_c_b"};
    sc_core::sc_signal<uint32_t> s_requant_scale_a{"s_requant_scale_a"};
    sc_core::sc_signal<uint32_t> s_requant_scale_b{"s_requant_scale_b"};
    sc_core::sc_signal<uint32_t> s_requant_shift_a{"s_requant_shift_a"};
    sc_core::sc_signal<uint32_t> s_requant_shift_b{"s_requant_shift_b"};
    sc_core::sc_signal<::sauria::sramc_mask_t<Y_DIM>> s_rows_active{"s_rows_active"};
    sc_core::sc_signal<bool> s_sa_clear_a{"s_sa_clear_a"};
    sc_core::sc_signal<bool> s_sa_clear_b{"s_sa_clear_b"};
    sc_core::sc_signal<::sauria::psum_vector_t<Y_DIM, T_PSUM>> s_sa_to_psm_c_a{"s_sa_to_psm_c_a"};
    sc_core::sc_signal<::sauria::psum_vector_t<Y_DIM, T_PSUM>> s_sa_to_psm_c_b{"s_sa_to_psm_c_b"};
    sc_core::sc_signal<uint32_t> s_srama_addr_a{"s_srama_addr_a"};
    sc_core::sc_signal<uint32_t> s_srama_addr_b{"s_srama_addr_b"};
    sc_core::sc_signal<::sauria::act_vector_t<Y_DIM, T_ACT>> s_srama_data_a{"s_srama_data_a"};
    sc_core::sc_signal<::sauria::act_vector_t<Y_DIM, T_ACT>> s_srama_data_b{"s_srama_data_b"};
    sc_core::sc_signal<bool> s_srama_rden_a{"s_srama_rden_a"};
    sc_core::sc_signal<bool> s_srama_rden_b{"s_srama_rden_b"};
    sc_core::sc_signal<uint32_t> s_sramb_addr_a{"s_sramb_addr_a"};
    sc_core::sc_signal<uint32_t> s_sramb_addr_b{"s_sramb_addr_b"};
    sc_core::sc_signal<::sauria::wei_vector_t<X_DIM, T_WEI>> s_sramb_data_a{"s_sramb_data_a"};
    sc_core::sc_signal<::sauria::wei_vector_t<X_DIM, T_WEI>> s_sramb_data_b{"s_sramb_data_b"};
    sc_core::sc_signal<bool> s_sramb_rden_a{"s_sramb_rden_a"};
    sc_core::sc_signal<bool> s_sramb_rden_b{"s_sramb_rden_b"};
    sc_core::sc_signal<uint32_t> s_sramc_addr_a{"s_sramc_addr_a"};
    sc_core::sc_signal<uint32_t> s_sramc_addr_b{"s_sramc_addr_b"};
    sc_core::sc_signal<::sauria::psum_vector_t<Y_DIM, T_PSUM>> s_sramc_rdata_a{"s_sramc_rdata_a"};
    sc_core::sc_signal<::sauria::psum_vector_t<Y_DIM, T_PSUM>> s_sramc_rdata_b{"s_sramc_rdata_b"};
    sc_core::sc_signal<bool> s_sramc_rden_a{"s_sramc_rden_a"};
    sc_core::sc_signal<bool> s_sramc_rden_b{"s_sramc_rden_b"};
    sc_core::sc_signal<::sauria::psum_vector_t<Y_DIM, T_PSUM>> s_sramc_wdata_a{"s_sramc_wdata_a"};
    sc_core::sc_signal<::sauria::psum_vector_t<Y_DIM, T_PSUM>> s_sramc_wdata_b{"s_sramc_wdata_b"};
    sc_core::sc_signal<::sauria::sramc_mask_t<Y_DIM>> s_sramc_wmask_a{"s_sramc_wmask_a"};
    sc_core::sc_signal<::sauria::sramc_mask_t<Y_DIM>> s_sramc_wmask_b{"s_sramc_wmask_b"};
    sc_core::sc_signal<bool> s_sramc_wren_a{"s_sramc_wren_a"};
    sc_core::sc_signal<bool> s_sramc_wren_b{"s_sramc_wren_b"};
    sc_core::sc_signal<bool> s_start_internal_a{"s_start_internal_a"};
    sc_core::sc_signal<bool> s_start_internal_b{"s_start_internal_b"};
    sc_core::sc_signal<uint32_t> s_stride{"s_stride"};
    sc_core::sc_signal<uint32_t> s_tile_c{"s_tile_c"};
    sc_core::sc_signal<uint32_t> s_tile_k{"s_tile_k"};
    sc_core::sc_signal<uint32_t> s_tile_x{"s_tile_x"};
    sc_core::sc_signal<uint32_t> s_tile_y{"s_tile_y"};
    sc_core::sc_signal<::sauria::wei_vector_t<X_DIM, T_WEI>> s_wei_arr_a{"s_wei_arr_a"};
    sc_core::sc_signal<::sauria::wei_vector_t<X_DIM, T_WEI>> s_wei_arr_b{"s_wei_arr_b"};
    sc_core::sc_signal<::sauria::wei_vector_t<X_DIM, T_WEI>> s_wei_arr_to_array_a{"s_wei_arr_to_array_a"};
    sc_core::sc_signal<::sauria::wei_vector_t<X_DIM, T_WEI>> s_wei_arr_to_array_b{"s_wei_arr_to_array_b"};
    sc_core::sc_signal<uint32_t> s_wei_base_addr{"s_wei_base_addr"};
    sc_core::sc_signal<uint32_t> s_wei_base_addr_a{"s_wei_base_addr_a"};
    sc_core::sc_signal<uint32_t> s_wei_base_addr_b{"s_wei_base_addr_b"};
    sc_core::sc_signal<bool> s_wei_clearfifo_a{"s_wei_clearfifo_a"};
    sc_core::sc_signal<bool> s_wei_clearfifo_b{"s_wei_clearfifo_b"};
    sc_core::sc_signal<bool> s_wei_cnt_clear_a{"s_wei_cnt_clear_a"};
    sc_core::sc_signal<bool> s_wei_cnt_clear_b{"s_wei_cnt_clear_b"};
    sc_core::sc_signal<bool> s_wei_cnt_en_a{"s_wei_cnt_en_a"};
    sc_core::sc_signal<bool> s_wei_cnt_en_b{"s_wei_cnt_en_b"};
    sc_core::sc_signal<uint32_t> s_wei_cols_active{"s_wei_cols_active"};
    sc_core::sc_signal<bool> s_wei_cswitch_a{"s_wei_cswitch_a"};
    sc_core::sc_signal<bool> s_wei_cswitch_b{"s_wei_cswitch_b"};
    sc_core::sc_signal<bool> s_wei_done_a{"s_wei_done_a"};
    sc_core::sc_signal<bool> s_wei_done_b{"s_wei_done_b"};
    sc_core::sc_signal<bool> s_wei_feeder_clear_a{"s_wei_feeder_clear_a"};
    sc_core::sc_signal<bool> s_wei_feeder_clear_b{"s_wei_feeder_clear_b"};
    sc_core::sc_signal<bool> s_wei_feeder_en_a{"s_wei_feeder_en_a"};
    sc_core::sc_signal<bool> s_wei_feeder_en_b{"s_wei_feeder_en_b"};
    sc_core::sc_signal<bool> s_wei_fifo_empty_a{"s_wei_fifo_empty_a"};
    sc_core::sc_signal<bool> s_wei_fifo_empty_b{"s_wei_fifo_empty_b"};
    sc_core::sc_signal<bool> s_wei_fifo_full_a{"s_wei_fifo_full_a"};
    sc_core::sc_signal<bool> s_wei_fifo_full_b{"s_wei_fifo_full_b"};
    sc_core::sc_signal<bool> s_wei_finalpush_a{"s_wei_finalpush_a"};
    sc_core::sc_signal<bool> s_wei_finalpush_b{"s_wei_finalpush_b"};
    sc_core::sc_signal<uint32_t> s_wei_incntlim{"s_wei_incntlim"};
    sc_core::sc_signal<uint32_t> s_wei_incntstep{"s_wei_incntstep"};
    sc_core::sc_signal<uint32_t> s_wei_klim{"s_wei_klim"};
    sc_core::sc_signal<uint32_t> s_wei_kstep{"s_wei_kstep"};
    sc_core::sc_signal<bool> s_wei_pop_en_a{"s_wei_pop_en_a"};
    sc_core::sc_signal<bool> s_wei_pop_en_b{"s_wei_pop_en_b"};
    sc_core::sc_signal<uint32_t> s_wei_reps{"s_wei_reps"};
    sc_core::sc_signal<bool> s_wei_stall_a{"s_wei_stall_a"};
    sc_core::sc_signal<bool> s_wei_stall_b{"s_wei_stall_b"};
    sc_core::sc_signal<bool> s_wei_start_a{"s_wei_start_a"};
    sc_core::sc_signal<bool> s_wei_start_b{"s_wei_start_b"};
    sc_core::sc_signal<bool> s_wei_til_done_a{"s_wei_til_done_a"};
    sc_core::sc_signal<bool> s_wei_til_done_b{"s_wei_til_done_b"};
    sc_core::sc_signal<uint32_t> s_wei_til_klim{"s_wei_til_klim"};
    sc_core::sc_signal<uint32_t> s_wei_til_kstep{"s_wei_til_kstep"};
    sc_core::sc_signal<bool> s_wei_valid_a{"s_wei_valid_a"};
    sc_core::sc_signal<bool> s_wei_valid_b{"s_wei_valid_b"};
    sc_core::sc_signal<uint32_t> s_wei_waligned{"s_wei_waligned"};
    sc_core::sc_signal<uint32_t> s_wei_wlim{"s_wei_wlim"};
    sc_core::sc_signal<uint32_t> s_wei_wstep{"s_wei_wstep"};
    sc_core::sc_signal<uint32_t> s_x_used{"s_x_used"};
    sc_core::sc_signal<uint32_t> s_y_used{"s_y_used"};
    // ── module instances ─────────────────────────────────────────────────────
    ::sauria::ConfigRegs<32, 32, X_DIM, Y_DIM, 2, 15, 15, 15, ::sauria::DILP_W, 8> config_regs_inst;
    ::sauria::Control<X_DIM, Y_DIM, PE_LAT, EXTRA_CSREG> ctrl_inst_a;
    ::sauria::Control<X_DIM, Y_DIM, PE_LAT, EXTRA_CSREG> ctrl_inst_b;
    ::sauria::IfmapFeeder<Y_DIM, T_ACT, SRAMA_CAP, FIFO_DEPTH, false> act_feeder_a;
    ::sauria::IfmapFeeder<Y_DIM, T_ACT, SRAMA_CAP, FIFO_DEPTH, true> act_feeder_b;
    ::sauria::WeightFeeder<X_DIM, T_WEI, SRAMB_CAP, FIFO_DEPTH, 0, false> wei_feeder_a;
    ::sauria::WeightFeeder<X_DIM, T_WEI, SRAMB_CAP, FIFO_DEPTH, 0, true> wei_feeder_b;
    ::sauria::SystolicArray<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM> array_inst;
    ::sauria::Psm<X_DIM, Y_DIM, T_PSUM, SRAMC_CAP> psm_inst_a;
    ::sauria::Psm<X_DIM, Y_DIM, T_PSUM, SRAMC_CAP> psm_inst_b;

    void bind_all()
    {
    ctrl_inst_a.i_clk(i_clk);
    ctrl_inst_a.i_rstn(i_rstn);
    ctrl_inst_a.i_soft_reset(s_ctrl_reset_internal);
    ctrl_inst_b.i_clk(i_clk);
    ctrl_inst_b.i_rstn(i_rstn);
    ctrl_inst_b.i_soft_reset(s_ctrl_reset_internal);
    act_feeder_a.i_clk(i_clk);
    act_feeder_a.i_rstn(i_rstn);
    act_feeder_b.i_clk(i_clk);
    act_feeder_b.i_rstn(i_rstn);
    wei_feeder_a.i_clk(i_clk);
    wei_feeder_a.i_rstn(i_rstn);
    wei_feeder_b.i_clk(i_clk);
    wei_feeder_b.i_rstn(i_rstn);
    array_inst.i_clk(i_clk);
    array_inst.i_rstn(i_rstn);
    psm_inst_a.i_clk(i_clk);
    psm_inst_a.i_rstn(i_rstn);
    psm_inst_b.i_clk(i_clk);
    psm_inst_b.i_rstn(i_rstn);
    config_regs_inst.i_clk(i_clk);
    config_regs_inst.i_rstn(i_rstn);
    config_regs_inst.i_host_addr(i_host_addr);
    config_regs_inst.i_host_wren(i_host_wren);
    config_regs_inst.i_host_rden(i_host_rden);
    config_regs_inst.i_host_wdata(i_host_wdata);
    config_regs_inst.i_host_wmask(i_host_wmask);
    config_regs_inst.o_host_rdata(s_host_rdata_cfg);
    config_regs_inst.i_done(s_ctrl_done_a);
    config_regs_inst.i_soft_reset_in(i_soft_reset);
    config_regs_inst.o_start(s_cfg_start);
    config_regs_inst.o_soft_reset(s_cfg_soft_reset);
    config_regs_inst.o_profile(s_cfg_profile);
    config_regs_inst.o_act_incntlim(s_act_incntlim);
    config_regs_inst.o_act_incntstep(s_act_incntstep);
    config_regs_inst.o_act_outcntlim(s_act_outcntlim);
    config_regs_inst.o_act_outcntstep(s_act_outcntstep);
    config_regs_inst.o_act_xlim(s_act_xlim);
    config_regs_inst.o_act_xstep(s_act_xstep);
    config_regs_inst.o_act_ylim(s_act_ylim);
    config_regs_inst.o_act_ystep(s_act_ystep);
    config_regs_inst.o_act_chlim(s_act_chlim);
    config_regs_inst.o_act_chstep(s_act_chstep);
    config_regs_inst.o_act_til_xlim(s_act_til_xlim);
    config_regs_inst.o_act_til_xstep(s_act_til_xstep);
    config_regs_inst.o_act_til_ylim(s_act_til_ylim);
    config_regs_inst.o_act_til_ystep(s_act_til_ystep);
    config_regs_inst.o_wei_incntlim(s_wei_incntlim);
    config_regs_inst.o_wei_incntstep(s_wei_incntstep);
    config_regs_inst.o_wei_wlim(s_wei_wlim);
    config_regs_inst.o_wei_wstep(s_wei_wstep);
    config_regs_inst.o_wei_klim(s_wei_klim);
    config_regs_inst.o_wei_kstep(s_wei_kstep);
    config_regs_inst.o_wei_til_klim(s_wei_til_klim);
    config_regs_inst.o_wei_til_kstep(s_wei_til_kstep);
    config_regs_inst.o_wei_cols_active(s_wei_cols_active);
    config_regs_inst.o_wei_waligned(s_wei_waligned);
    config_regs_inst.o_cxlim(s_cxlim);
    config_regs_inst.o_cxstep(s_cxstep);
    config_regs_inst.o_cklim(s_cklim);
    config_regs_inst.o_ckstep(s_ckstep);
    config_regs_inst.o_out_ncontexts(s_out_ncontexts);
    config_regs_inst.o_out_til_cylim(s_out_til_cylim);
    config_regs_inst.o_out_til_cystep(s_out_til_cystep);
    config_regs_inst.o_out_til_cklim(s_out_til_cklim);
    config_regs_inst.o_out_til_ckstep(s_out_til_ckstep);
    config_regs_inst.o_out_inactive_cols(s_out_inactive_cols);
    config_regs_inst.o_out_preload_en(s_out_preload_en);
    config_regs_inst.o_act_base_addr(s_act_base_addr);
    config_regs_inst.o_wei_base_addr(s_wei_base_addr);
    config_regs_inst.o_out_base_addr(s_out_base_addr);
    config_regs_inst.o_incntlim(s_incntlim);
    config_regs_inst.o_act_reps(s_act_reps);
    config_regs_inst.o_wei_reps(s_wei_reps);
    config_regs_inst.o_dil_pat(s_dil_pat);
    config_regs_inst.o_rows_active(s_rows_active);
    config_regs_inst.o_in_h(s_in_h);
    config_regs_inst.o_in_w(s_in_w);
    config_regs_inst.o_in_c(s_in_c);
    config_regs_inst.o_kernel_h(s_kernel_h);
    config_regs_inst.o_kernel_w(s_kernel_w);
    config_regs_inst.o_stride(s_stride);
    config_regs_inst.o_padding(s_padding);
    config_regs_inst.o_dilation(s_dilation);
    config_regs_inst.o_tile_x(s_tile_x);
    config_regs_inst.o_tile_y(s_tile_y);
    config_regs_inst.o_tile_k(s_tile_k);
    config_regs_inst.o_tile_c(s_tile_c);
    config_regs_inst.o_x_used(s_x_used);
    config_regs_inst.o_y_used(s_y_used);
    config_regs_inst.o_nsplit(s_config_nsplit);
    config_regs_inst.o_obp_cfg_a(s_obp_cfg_a);
    config_regs_inst.o_requant_scale_a(s_requant_scale_a);
    config_regs_inst.o_requant_shift_a(s_requant_shift_a);
    config_regs_inst.o_obp_cfg_b(s_obp_cfg_b);
    config_regs_inst.o_requant_scale_b(s_requant_scale_b);
    config_regs_inst.o_requant_shift_b(s_requant_shift_b);
    ctrl_inst_a.i_start(s_start_internal_a);
    ctrl_inst_a.o_done(s_ctrl_done_a);
    ctrl_inst_a.o_feed_deadlock(s_deadlock_a);
    ctrl_inst_a.o_active(s_ctrl_active_a);
    ctrl_inst_a.i_outbuf_done(s_psm_done_a);
    ctrl_inst_a.i_finalwrite(s_psm_finalwrite_a);
    ctrl_inst_a.i_shift_done(s_psm_shift_done_a);
    ctrl_inst_a.i_act_done(s_act_done_a);
    ctrl_inst_a.i_act_til_done(s_act_til_done_a);
    ctrl_inst_a.i_act_fifo_empty(s_act_fifo_empty_a);
    ctrl_inst_a.i_act_fifo_full(s_act_fifo_full_a);
    ctrl_inst_a.i_act_stall(s_act_stall_a);
    ctrl_inst_a.i_wei_done(s_wei_done_a);
    ctrl_inst_a.i_wei_til_done(s_wei_til_done_a);
    ctrl_inst_a.i_wei_fifo_empty(s_wei_fifo_empty_a);
    ctrl_inst_a.i_wei_fifo_full(s_wei_fifo_full_a);
    ctrl_inst_a.i_wei_stall(s_wei_stall_a);
    ctrl_inst_a.i_mvm_k(i_mvm_k);
    ctrl_inst_a.i_total_contexts(i_total_contexts);
    ctrl_inst_a.o_act_feeder_en(s_act_feeder_en_a);
    ctrl_inst_a.o_act_feeder_clear(s_act_feeder_clear_a);
    ctrl_inst_a.o_act_start(s_act_start_a);
    ctrl_inst_a.o_act_valid(s_act_valid_a);
    ctrl_inst_a.o_act_finalpush(s_act_finalpush_a);
    ctrl_inst_a.o_act_cnt_en(s_act_cnt_en_a);
    ctrl_inst_a.o_act_cnt_clear(s_act_cnt_clear_a);
    ctrl_inst_a.o_act_clearfifo(s_act_clearfifo_a);
    ctrl_inst_a.o_act_pop_en(s_act_pop_en_a);
    ctrl_inst_a.o_act_finalctx(s_act_finalctx_a);
    ctrl_inst_a.o_wei_feeder_en(s_wei_feeder_en_a);
    ctrl_inst_a.o_wei_feeder_clear(s_wei_feeder_clear_a);
    ctrl_inst_a.o_wei_start(s_wei_start_a);
    ctrl_inst_a.o_wei_valid(s_wei_valid_a);
    ctrl_inst_a.o_wei_finalpush(s_wei_finalpush_a);
    ctrl_inst_a.o_wei_cnt_en(s_wei_cnt_en_a);
    ctrl_inst_a.o_wei_cnt_clear(s_wei_cnt_clear_a);
    ctrl_inst_a.o_wei_clearfifo(s_wei_clearfifo_a);
    ctrl_inst_a.o_wei_pop_en(s_wei_pop_en_a);
    ctrl_inst_a.o_wei_cswitch(s_wei_cswitch_a);
    ctrl_inst_a.o_outbuf_start(s_psm_start_a);
    ctrl_inst_a.o_outbuf_reset(s_psm_reset_a);
    ctrl_inst_a.o_context_id(s_context_id_a);
    ctrl_inst_a.o_local_context_id(s_local_context_id_a);
    ctrl_inst_a.o_out_tile_id(s_out_tile_id_a);
    ctrl_inst_a.o_global_context_id(s_global_context_id_a);
    ctrl_inst_a.o_sa_clear(s_sa_clear_a);
    ctrl_inst_a.o_pipeline_en(s_pipeline_en_a);
    ctrl_inst_a.o_cswitch_arr(s_cswitch_arr_a);
    ctrl_inst_a.i_incntlim(s_incntlim);
    ctrl_inst_a.i_act_reps(s_act_reps);
    ctrl_inst_a.i_wei_reps(s_wei_reps);
    ctrl_inst_a.i_ncontexts(s_out_ncontexts);
    ctrl_inst_b.i_start(s_start_internal_b);
    ctrl_inst_b.o_done(s_ctrl_done_b);
    ctrl_inst_b.o_feed_deadlock(s_deadlock_b);
    ctrl_inst_b.o_active(s_ctrl_active_b);
    ctrl_inst_b.i_outbuf_done(s_psm_done_b);
    ctrl_inst_b.i_finalwrite(s_psm_finalwrite_b);
    ctrl_inst_b.i_shift_done(s_psm_shift_done_b);
    ctrl_inst_b.i_act_done(s_act_done_b);
    ctrl_inst_b.i_act_til_done(s_act_til_done_b);
    ctrl_inst_b.i_act_fifo_empty(s_act_fifo_empty_b);
    ctrl_inst_b.i_act_fifo_full(s_act_fifo_full_b);
    ctrl_inst_b.i_act_stall(s_act_stall_b);
    ctrl_inst_b.i_wei_done(s_wei_done_b);
    ctrl_inst_b.i_wei_til_done(s_wei_til_done_b);
    ctrl_inst_b.i_wei_fifo_empty(s_wei_fifo_empty_b);
    ctrl_inst_b.i_wei_fifo_full(s_wei_fifo_full_b);
    ctrl_inst_b.i_wei_stall(s_wei_stall_b);
    ctrl_inst_b.i_mvm_k(i_mvm_k);
    ctrl_inst_b.i_total_contexts(i_total_contexts);
    ctrl_inst_b.o_act_feeder_en(s_act_feeder_en_b);
    ctrl_inst_b.o_act_feeder_clear(s_act_feeder_clear_b);
    ctrl_inst_b.o_act_start(s_act_start_b);
    ctrl_inst_b.o_act_valid(s_act_valid_b);
    ctrl_inst_b.o_act_finalpush(s_act_finalpush_b);
    ctrl_inst_b.o_act_cnt_en(s_act_cnt_en_b);
    ctrl_inst_b.o_act_cnt_clear(s_act_cnt_clear_b);
    ctrl_inst_b.o_act_clearfifo(s_act_clearfifo_b);
    ctrl_inst_b.o_act_pop_en(s_act_pop_en_b);
    ctrl_inst_b.o_act_finalctx(s_act_finalctx_b);
    ctrl_inst_b.o_wei_feeder_en(s_wei_feeder_en_b);
    ctrl_inst_b.o_wei_feeder_clear(s_wei_feeder_clear_b);
    ctrl_inst_b.o_wei_start(s_wei_start_b);
    ctrl_inst_b.o_wei_valid(s_wei_valid_b);
    ctrl_inst_b.o_wei_finalpush(s_wei_finalpush_b);
    ctrl_inst_b.o_wei_cnt_en(s_wei_cnt_en_b);
    ctrl_inst_b.o_wei_cnt_clear(s_wei_cnt_clear_b);
    ctrl_inst_b.o_wei_clearfifo(s_wei_clearfifo_b);
    ctrl_inst_b.o_wei_pop_en(s_wei_pop_en_b);
    ctrl_inst_b.o_wei_cswitch(s_wei_cswitch_b);
    ctrl_inst_b.o_outbuf_start(s_psm_start_b);
    ctrl_inst_b.o_outbuf_reset(s_psm_reset_b);
    ctrl_inst_b.o_context_id(s_context_id_b);
    ctrl_inst_b.o_local_context_id(s_local_context_id_b);
    ctrl_inst_b.o_out_tile_id(s_out_tile_id_b);
    ctrl_inst_b.o_global_context_id(s_global_context_id_b);
    ctrl_inst_b.o_sa_clear(s_sa_clear_b);
    ctrl_inst_b.o_pipeline_en(s_pipeline_en_b);
    ctrl_inst_b.o_cswitch_arr(s_cswitch_arr_b);
    ctrl_inst_b.i_incntlim(s_incntlim);
    ctrl_inst_b.i_act_reps(s_act_reps);
    ctrl_inst_b.i_wei_reps(s_wei_reps);
    ctrl_inst_b.i_ncontexts(s_out_ncontexts);
    act_feeder_a.i_feeder_en(s_act_feeder_en_a);
    act_feeder_a.i_feeder_clear(s_act_feeder_clear_a);
    act_feeder_a.i_start(s_act_start_a);
    act_feeder_a.i_valid(s_act_valid_a);
    act_feeder_a.i_finalpush(s_act_finalpush_a);
    act_feeder_a.i_cnt_en(s_act_cnt_en_a);
    act_feeder_a.i_cnt_clear(s_act_cnt_clear_a);
    act_feeder_a.i_clearfifo(s_act_clearfifo_a);
    act_feeder_a.i_pop_en(s_act_pop_en_a);
    act_feeder_a.i_finalctx(s_act_finalctx_a);
    act_feeder_a.i_context_id(s_local_context_id_a);
    act_feeder_a.i_ncontexts(s_out_ncontexts);
    act_feeder_a.i_act_reps(s_act_reps);
    act_feeder_a.i_mvm_k(i_mvm_k);
    act_feeder_a.o_act_done(s_act_done_a);
    act_feeder_a.o_act_til_done(s_act_til_done_a);
    act_feeder_a.o_fifo_empty(s_act_fifo_empty_a);
    act_feeder_a.o_fifo_full(s_act_fifo_full_a);
    act_feeder_a.o_stall(s_act_stall_a);
    act_feeder_a.o_srama_addr(s_srama_addr_a);
    act_feeder_a.o_srama_rden(s_srama_rden_a);
    act_feeder_a.i_srama_data(s_srama_data_a);
    act_feeder_a.o_act_arr(s_act_arr_a);
    act_feeder_a.i_act_base_addr(s_act_base_addr_a);
    act_feeder_a.i_act_incntlim(s_act_incntlim);
    act_feeder_a.i_act_incntstep(s_act_incntstep);
    act_feeder_a.i_act_outcntlim(s_act_outcntlim);
    act_feeder_a.i_act_outcntstep(s_act_outcntstep);
    act_feeder_a.i_act_dil_pat(s_dil_pat);
    act_feeder_a.i_act_xlim(s_act_xlim);
    act_feeder_a.i_act_xstep(s_act_xstep);
    act_feeder_a.i_act_ylim(s_act_ylim);
    act_feeder_a.i_act_ystep(s_act_ystep);
    act_feeder_a.i_act_chlim(s_act_chlim);
    act_feeder_a.i_act_chstep(s_act_chstep);
    act_feeder_a.i_act_til_xlim(s_act_til_xlim);
    act_feeder_a.i_act_til_xstep(s_act_til_xstep);
    act_feeder_a.i_act_til_ylim(s_act_til_ylim);
    act_feeder_a.i_act_til_ystep(s_act_til_ystep);
    act_feeder_a.i_nsplit(s_nsplit);
    act_feeder_b.i_feeder_en(s_act_feeder_en_b);
    act_feeder_b.i_feeder_clear(s_act_feeder_clear_b);
    act_feeder_b.i_start(s_act_start_b);
    act_feeder_b.i_valid(s_act_valid_b);
    act_feeder_b.i_finalpush(s_act_finalpush_b);
    act_feeder_b.i_cnt_en(s_act_cnt_en_b);
    act_feeder_b.i_cnt_clear(s_act_cnt_clear_b);
    act_feeder_b.i_clearfifo(s_act_clearfifo_b);
    act_feeder_b.i_pop_en(s_act_pop_en_b);
    act_feeder_b.i_finalctx(s_act_finalctx_b);
    act_feeder_b.i_context_id(s_local_context_id_b);
    act_feeder_b.i_ncontexts(s_out_ncontexts);
    act_feeder_b.i_act_reps(s_act_reps);
    act_feeder_b.i_mvm_k(i_mvm_k);
    act_feeder_b.o_act_done(s_act_done_b);
    act_feeder_b.o_act_til_done(s_act_til_done_b);
    act_feeder_b.o_fifo_empty(s_act_fifo_empty_b);
    act_feeder_b.o_fifo_full(s_act_fifo_full_b);
    act_feeder_b.o_stall(s_act_stall_b);
    act_feeder_b.o_srama_addr(s_srama_addr_b);
    act_feeder_b.o_srama_rden(s_srama_rden_b);
    act_feeder_b.i_srama_data(s_srama_data_b);
    act_feeder_b.o_act_arr(s_act_arr_b);
    act_feeder_b.i_act_base_addr(s_act_base_addr_b);
    act_feeder_b.i_act_incntlim(s_act_incntlim);
    act_feeder_b.i_act_incntstep(s_act_incntstep);
    act_feeder_b.i_act_outcntlim(s_act_outcntlim);
    act_feeder_b.i_act_outcntstep(s_act_outcntstep);
    act_feeder_b.i_act_dil_pat(s_dil_pat);
    act_feeder_b.i_act_xlim(s_act_xlim);
    act_feeder_b.i_act_xstep(s_act_xstep);
    act_feeder_b.i_act_ylim(s_act_ylim);
    act_feeder_b.i_act_ystep(s_act_ystep);
    act_feeder_b.i_act_chlim(s_act_chlim);
    act_feeder_b.i_act_chstep(s_act_chstep);
    act_feeder_b.i_act_til_xlim(s_act_til_xlim);
    act_feeder_b.i_act_til_xstep(s_act_til_xstep);
    act_feeder_b.i_act_til_ylim(s_act_til_ylim);
    act_feeder_b.i_act_til_ystep(s_act_til_ystep);
    act_feeder_b.i_nsplit(s_nsplit);
    wei_feeder_a.i_wei_incntlim(s_wei_incntlim);
    wei_feeder_a.i_wei_incntstep(s_wei_incntstep);
    wei_feeder_a.i_wei_wlim(s_wei_wlim);
    wei_feeder_a.i_wei_wstep(s_wei_wstep);
    wei_feeder_a.i_wei_klim(s_wei_klim);
    wei_feeder_a.i_wei_kstep(s_wei_kstep);
    wei_feeder_a.i_wei_til_klim(s_wei_til_klim);
    wei_feeder_a.i_wei_til_kstep(s_wei_til_kstep);
    wei_feeder_a.i_wei_cols_active(s_wei_cols_active);
    wei_feeder_a.i_wei_waligned(s_wei_waligned);
    wei_feeder_a.i_feeder_en(s_wei_feeder_en_a);
    wei_feeder_a.i_feeder_clear(s_wei_feeder_clear_a);
    wei_feeder_a.i_start(s_wei_start_a);
    wei_feeder_a.i_valid(s_wei_valid_a);
    wei_feeder_a.i_finalpush(s_wei_finalpush_a);
    wei_feeder_a.i_cnt_en(s_wei_cnt_en_a);
    wei_feeder_a.i_cnt_clear(s_wei_cnt_clear_a);
    wei_feeder_a.i_clearfifo(s_wei_clearfifo_a);
    wei_feeder_a.i_pop_en(s_wei_pop_en_a);
    wei_feeder_a.i_cswitch(s_wei_cswitch_a);
    wei_feeder_a.o_wei_done(s_wei_done_a);
    wei_feeder_a.o_wei_til_done(s_wei_til_done_a);
    wei_feeder_a.o_fifo_empty(s_wei_fifo_empty_a);
    wei_feeder_a.o_fifo_full(s_wei_fifo_full_a);
    wei_feeder_a.o_stall(s_wei_stall_a);
    wei_feeder_a.o_sramb_addr(s_sramb_addr_a);
    wei_feeder_a.o_sramb_rden(s_sramb_rden_a);
    wei_feeder_a.i_sramb_data(s_sramb_data_a);
    wei_feeder_a.o_wei_arr(s_wei_arr_a);
    wei_feeder_a.i_context_id(s_local_context_id_a);
    wei_feeder_a.i_out_tile_id(s_out_tile_id_a);
    wei_feeder_a.i_ncontexts(s_out_ncontexts);
    wei_feeder_a.i_mvm_k(i_mvm_k);
    wei_feeder_a.i_wei_base_addr(s_wei_base_addr_a);
    wei_feeder_a.i_nsplit(s_nsplit);
    wei_feeder_b.i_wei_incntlim(s_wei_incntlim);
    wei_feeder_b.i_wei_incntstep(s_wei_incntstep);
    wei_feeder_b.i_wei_wlim(s_wei_wlim);
    wei_feeder_b.i_wei_wstep(s_wei_wstep);
    wei_feeder_b.i_wei_klim(s_wei_klim);
    wei_feeder_b.i_wei_kstep(s_wei_kstep);
    wei_feeder_b.i_wei_til_klim(s_wei_til_klim);
    wei_feeder_b.i_wei_til_kstep(s_wei_til_kstep);
    wei_feeder_b.i_wei_cols_active(s_wei_cols_active);
    wei_feeder_b.i_wei_waligned(s_wei_waligned);
    wei_feeder_b.i_feeder_en(s_wei_feeder_en_b);
    wei_feeder_b.i_feeder_clear(s_wei_feeder_clear_b);
    wei_feeder_b.i_start(s_wei_start_b);
    wei_feeder_b.i_valid(s_wei_valid_b);
    wei_feeder_b.i_finalpush(s_wei_finalpush_b);
    wei_feeder_b.i_cnt_en(s_wei_cnt_en_b);
    wei_feeder_b.i_cnt_clear(s_wei_cnt_clear_b);
    wei_feeder_b.i_clearfifo(s_wei_clearfifo_b);
    wei_feeder_b.i_pop_en(s_wei_pop_en_b);
    wei_feeder_b.i_cswitch(s_wei_cswitch_b);
    wei_feeder_b.o_wei_done(s_wei_done_b);
    wei_feeder_b.o_wei_til_done(s_wei_til_done_b);
    wei_feeder_b.o_fifo_empty(s_wei_fifo_empty_b);
    wei_feeder_b.o_fifo_full(s_wei_fifo_full_b);
    wei_feeder_b.o_stall(s_wei_stall_b);
    wei_feeder_b.o_sramb_addr(s_sramb_addr_b);
    wei_feeder_b.o_sramb_rden(s_sramb_rden_b);
    wei_feeder_b.i_sramb_data(s_sramb_data_b);
    wei_feeder_b.o_wei_arr(s_wei_arr_b);
    wei_feeder_b.i_context_id(s_local_context_id_b);
    wei_feeder_b.i_out_tile_id(s_out_tile_id_b);
    wei_feeder_b.i_ncontexts(s_out_ncontexts);
    wei_feeder_b.i_mvm_k(i_mvm_k);
    wei_feeder_b.i_wei_base_addr(s_wei_base_addr_b);
    wei_feeder_b.i_nsplit(s_nsplit);
    array_inst.i_threshold(i_threshold);
    array_inst.i_nsplit(s_nsplit);
    array_inst.i_act_arr_a(s_act_arr_to_array_a);
    array_inst.i_act_arr_b(s_act_arr_to_array_b);
    array_inst.i_wei_arr_a(s_wei_arr_to_array_a);
    array_inst.i_wei_arr_b(s_wei_arr_to_array_b);
    array_inst.i_c_arr_a(s_psm_to_sa_c_a);
    array_inst.o_c_arr_a(s_sa_to_psm_c_a);
    array_inst.i_c_arr_b(s_psm_to_sa_c_b);
    array_inst.o_c_arr_b(s_sa_to_psm_c_b);
    array_inst.i_pipeline_en_a(s_pipeline_en_a);
    array_inst.i_pipeline_en_b(s_pipeline_en_b);
    array_inst.i_cscan_en_a(s_cscan_en_a);
    array_inst.i_cscan_en_b(s_cscan_en_b);
    array_inst.i_cswitch_arr_a(s_cswitch_arr_a);
    array_inst.i_cswitch_arr_b(s_cswitch_arr_b);
    array_inst.i_sa_clear_a(s_sa_clear_a);
    array_inst.i_sa_clear_b(s_sa_clear_b);
    array_inst.i_context_id_a(s_context_id_a);
    array_inst.i_context_id_b(s_context_id_b);
    psm_inst_a.i_c_arr(s_sa_to_psm_c_a);
    psm_inst_a.o_c_arr(s_psm_to_sa_c_a);
    psm_inst_a.i_sramc_rdata(s_sramc_rdata_a);
    psm_inst_a.o_sramc_addr(s_sramc_addr_a);
    psm_inst_a.o_sramc_wren(s_sramc_wren_a);
    psm_inst_a.o_sramc_rden(s_sramc_rden_a);
    psm_inst_a.o_sramc_wmask(s_sramc_wmask_a);
    psm_inst_a.o_sramc_wdata(s_sramc_wdata_a);
    psm_inst_a.i_fsm_start(s_psm_start_a);
    psm_inst_a.i_fsm_reset(s_psm_reset_a);
    psm_inst_a.i_pipeline_en(s_pipeline_en_a);
    psm_inst_a.o_done(s_psm_done_a);
    psm_inst_a.o_finalwrite(s_psm_finalwrite_a);
    psm_inst_a.o_shift_done(s_psm_shift_done_a);
    psm_inst_a.o_cscan_en(s_cscan_en_a);
    psm_inst_a.i_out_base_addr(s_out_base_addr);
    psm_inst_a.i_cxlim(s_cxlim);
    psm_inst_a.i_cxstep(s_cxstep);
    psm_inst_a.i_cklim(s_cklim);
    psm_inst_a.i_ckstep(s_ckstep);
    psm_inst_a.i_til_cylim(s_out_til_cylim);
    psm_inst_a.i_til_cystep(s_out_til_cystep);
    psm_inst_a.i_til_cklim(s_out_til_cklim);
    psm_inst_a.i_til_ckstep(s_out_til_ckstep);
    psm_inst_a.i_ncontexts(s_out_ncontexts);
    psm_inst_a.i_preload_en(s_out_preload_en);
    psm_inst_a.i_rows_active(s_rows_active);
    psm_inst_a.i_context_id(s_global_context_id_a);
    psm_inst_a.i_total_contexts(i_total_contexts);
    psm_inst_b.i_c_arr(s_sa_to_psm_c_b);
    psm_inst_b.o_c_arr(s_psm_to_sa_c_b);
    psm_inst_b.i_sramc_rdata(s_sramc_rdata_b);
    psm_inst_b.o_sramc_addr(s_sramc_addr_b);
    psm_inst_b.o_sramc_wren(s_sramc_wren_b);
    psm_inst_b.o_sramc_rden(s_sramc_rden_b);
    psm_inst_b.o_sramc_wmask(s_sramc_wmask_b);
    psm_inst_b.o_sramc_wdata(s_sramc_wdata_b);
    psm_inst_b.i_fsm_start(s_psm_start_b);
    psm_inst_b.i_fsm_reset(s_psm_reset_b);
    psm_inst_b.i_pipeline_en(s_pipeline_en_b);
    psm_inst_b.o_done(s_psm_done_b);
    psm_inst_b.o_finalwrite(s_psm_finalwrite_b);
    psm_inst_b.o_shift_done(s_psm_shift_done_b);
    psm_inst_b.o_cscan_en(s_cscan_en_b);
    psm_inst_b.i_out_base_addr(s_out_base_addr);
    psm_inst_b.i_cxlim(s_cxlim);
    psm_inst_b.i_cxstep(s_cxstep);
    psm_inst_b.i_cklim(s_cklim);
    psm_inst_b.i_ckstep(s_ckstep);
    psm_inst_b.i_til_cylim(s_out_til_cylim);
    psm_inst_b.i_til_cystep(s_out_til_cystep);
    psm_inst_b.i_til_cklim(s_out_til_cklim);
    psm_inst_b.i_til_ckstep(s_out_til_ckstep);
    psm_inst_b.i_ncontexts(s_out_ncontexts);
    psm_inst_b.i_preload_en(s_out_preload_en);
    psm_inst_b.i_rows_active(s_rows_active);
    psm_inst_b.i_context_id(s_global_context_id_b);
    psm_inst_b.i_total_contexts(i_total_contexts);
        // ── the staging store, replacing `Sram` (D17) ────────────────────────
        store.i_clk(i_clk);
        store.i_rstn(i_rstn);
        store.i_srama_addr_a(s_srama_addr_a);   store.i_srama_rden_a(s_srama_rden_a);
        store.o_srama_data_a(s_srama_data_a);
        store.i_srama_addr_b(s_srama_addr_b);   store.i_srama_rden_b(s_srama_rden_b);
        store.o_srama_data_b(s_srama_data_b);
        store.i_sramb_addr_a(s_sramb_addr_a);   store.i_sramb_rden_a(s_sramb_rden_a);
        store.o_sramb_data_a(s_sramb_data_a);
        store.i_sramb_addr_b(s_sramb_addr_b);   store.i_sramb_rden_b(s_sramb_rden_b);
        store.o_sramb_data_b(s_sramb_data_b);
        store.i_sramc_addr_a(s_sramc_addr_a);   store.i_sramc_rden_a(s_sramc_rden_a);
        store.i_sramc_wren_a(s_sramc_wren_a);   store.i_sramc_wdata_a(s_sramc_wdata_a);
        store.i_sramc_wmask_a(s_sramc_wmask_a); store.o_sramc_rdata_a(s_sramc_rdata_a);
        store.i_sramc_addr_b(s_sramc_addr_b);   store.i_sramc_rden_b(s_sramc_rden_b);
        store.i_sramc_wren_b(s_sramc_wren_b);   store.i_sramc_wdata_b(s_sramc_wdata_b);
        store.i_sramc_wmask_b(s_sramc_wmask_b); store.o_sramc_rdata_b(s_sramc_rdata_b);

        // ── the base-address mux, collapsed ──────────────────────────────────
        //
        // `NpuTop` selects between the instruction decoder's base addresses and
        // `ConfigRegs`'. Phase 5 excludes the decoder, so there is nothing to
        // select and the config value reaches both lanes directly.
        SC_METHOD(base_address_passthrough);
        sensitive << s_act_base_addr << s_wei_base_addr;

        // ── the glue ported from `NpuTop` ────────────────────────────────────
        //
        // Sensitivity lists and clocked/combinational nature are copied from
        // `npu_top.h` exactly, minus the excluded decoder's terms. The header
        // comment explains why the last one has to stay clocked.

        SC_METHOD(start_reset_logic);
        sensitive << i_start << s_cfg_start << i_soft_reset << s_cfg_soft_reset;

        SC_METHOD(nsplit_passthrough);
        sensitive << s_config_nsplit;

        SC_METHOD(deadlock_merge_logic);
        sensitive << s_deadlock_a << s_deadlock_b;

        SC_METHOD(done_latch_logic);
        sensitive << i_clk.pos();
        dont_initialize();

        SC_METHOD(array_operand_feed);
        sensitive << i_clk.pos();
        dont_initialize();
    }

    void base_address_passthrough()
    {
        s_act_base_addr_a.write(s_act_base_addr.read());
        s_act_base_addr_b.write(s_act_base_addr.read());
        s_wei_base_addr_a.write(s_wei_base_addr.read());
        s_wei_base_addr_b.write(s_wei_base_addr.read());
    }

    /// `NpuTop::start_reset_logic`, with `s_trigger_start_a/b` dropped.
    ///
    /// Those two came from the instruction decoder, so with it excluded both
    /// lanes see the same start. The source's debug print on the rising edge
    /// used a function-local `static bool`, which would have been shared between
    /// two instances of this composition — it is instrumentation, and it is gone.
    void start_reset_logic()
    {
        const bool start = i_start.read() || s_cfg_start.read();
        s_start_internal_a.write(start);
        s_start_internal_b.write(start);
        s_ctrl_reset_internal.write(i_soft_reset.read()
                                    || s_cfg_soft_reset.read());
    }

    /// `NpuTop::nsplit_mux_logic`, collapsed for the same reason as the
    /// base-address mux: the instruction-mode leg has no source.
    void nsplit_passthrough() { s_nsplit.write(s_config_nsplit.read()); }

    void deadlock_merge_logic()
    {
        o_deadlock.write(s_deadlock_a.read() || s_deadlock_b.read());
    }

    /// `NpuTop::done_latch_logic`, unchanged.
    ///
    /// Both controllers must have reported done before the engine has, unless
    /// `nsplit` says lane B is not participating. The latch is cleared by reset
    /// *and* by a new start, which is what makes a stale `done` from the previous
    /// job impossible to observe as this job's completion.
    void done_latch_logic()
    {
        const bool reset_active = !i_rstn.read() || i_soft_reset.read()
                                  || s_cfg_soft_reset.read();
        const bool new_start = i_start.read() || s_cfg_start.read();

        if (reset_active || new_start) {
            done_latched_a_ = false;
            done_latched_b_ = false;
            o_done.write(false);
            return;
        }

        if (s_ctrl_done_a.read()) {
            done_latched_a_ = true;
        }
        if (s_ctrl_done_b.read()) {
            done_latched_b_ = true;
        }

        if (s_nsplit.read() < static_cast<std::uint32_t>(Y_DIM)) {
            o_done.write(done_latched_a_ && done_latched_b_);
        } else {
            o_done.write(done_latched_a_);
        }
    }

    /// The pass-through leg of `NpuTop::debug_ref_stream_mux`.
    ///
    /// Clocked, because the source's is: this is a pipeline register in the
    /// array's operand path, not a rename. The other leg injected a debug
    /// reference stream under a member initialised `false` and never assigned,
    /// so it is unreachable in the source too and is not reproduced.
    void array_operand_feed()
    {
        if (!i_rstn.read()) {
            s_act_arr_to_array_a.write(::sauria::act_vector_t<Y_DIM, T_ACT>());
            s_act_arr_to_array_b.write(::sauria::act_vector_t<Y_DIM, T_ACT>());
            s_wei_arr_to_array_a.write(::sauria::wei_vector_t<X_DIM, T_WEI>());
            s_wei_arr_to_array_b.write(::sauria::wei_vector_t<X_DIM, T_WEI>());
            return;
        }
        s_act_arr_to_array_a.write(s_act_arr_a.read());
        s_act_arr_to_array_b.write(s_act_arr_b.read());
        s_wei_arr_to_array_a.write(s_wei_arr_a.read());
        s_wei_arr_to_array_b.write(s_wei_arr_b.read());
    }

    bool done_latched_a_ = false;
    bool done_latched_b_ = false;
};

} // namespace cdc::components::tpu_v3::sauria
