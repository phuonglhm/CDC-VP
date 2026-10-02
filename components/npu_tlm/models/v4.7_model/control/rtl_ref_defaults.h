#pragma once
//
// Minimal `FX1_A3_*` macro defaults needed by the ported `sauria_rtl::*` classes (control/rtl_ref_*.h,
// data_feeder/rtl_ref_*.h, psm/rtl_ref_*.h, systolic_array/rtl_ref_*.h), taken from sauria_model's
// fx1_a3_defaults.h -- NOT a full copy of that file.
//
// The reference file also holds many debug / trace / probe-only macros that no file here reads; copying it
// blindly could enable a probe with no support in this tree. Only macros consumed by the rtl_ref_*.h files in
// this tree are defined. Each block names its consumer(s) and the line of the default in the reference
// fx1_a3_defaults.h so it can be re-checked.
//
// All defaults below are the REFERENCE tree's own defaults (i.e. "guard-on", matching
// fx1_a3_defaults.h's un-guarded state) -- this file does NOT implement a `FX1_A3_GUARD_OFF`
// escape hatch, unlike the reference; if a guard-off comparison is ever needed for A1 debugging,
// add it explicitly then, don't preemptively build unused infrastructure.
//
// Must be #include'd BEFORE any control/rtl_ref_*.h file (mirrors sauria_types.h's own
// first-include-wins convention for fx1_a3_defaults.h in the reference tree).

// ============================================================================
// Phase A -- control-side (rtl_ref_context_fsm.h, rtl_ref_feeders_fsm.h,
// rtl_ref_context_switch_controller.h, rtl_ref_main_controller.h)
// ============================================================================

// Disables rtl_ref_main_controller.h's optional fx1::PerfCounters state-histogram bump
// (ctrl_process()'s ctx_status-indexed perf->ctrl_state_cycles[...]++ block). This tree's own
// instrumentation/perf_counters.h does not declare the NUM_CTRL_STATES/ctrl_state_cycles members the reference
// perf integration expects; this is optional profiling only, so disabling it does not affect correctness.
//
// A narrow macro of its own, not the tree-wide `FX1_NO_PERF`: that one also controls NpuTop::attach_perf(),
// sauria_dma.h, sa_array.h, obp_top.h, re_rce.h... and defining it here would switch those off in every
// translation unit that includes native_lane_a_core.h (i.e. npu_top.h), RTL-ref backend or not.
// It is set exactly when -DFX1_NO_PERF is given: the hook blocks use the member `perf`, which only exists without
// FX1_NO_PERF (rtl_ref_main_controller.h, rtl_ref_*_feeder.h: `#ifndef FX1_NO_PERF`). A metrics build (without
// FX1_NO_PERF, e.g. PERF_DEFS="" with FE_METRICS) gets the hooks; -DFX1_A3_CTRL_HISTOGRAM_NO_PERF still turns them off.
#ifdef FX1_NO_PERF
#ifndef FX1_A3_CTRL_HISTOGRAM_NO_PERF
#define FX1_A3_CTRL_HISTOGRAM_NO_PERF
#endif
#endif

// Same for the 7 perf hooks of rtl_ref_ifmap_feeder.h / rtl_ref_wei_feeder.h (act/wei l1_read_words,
// stall_cycles, feed_cycles): a narrow macro, set exactly when -DFX1_NO_PERF is given.
#ifdef FX1_NO_PERF
#ifndef FX1_A3_FEEDER_PERF_NO_PERF
#define FX1_A3_FEEDER_PERF_NO_PERF
#endif
#endif

// Selects the real ContextFsm+ContextSwitchController wiring inside Control
// (rtl_ref_main_controller.h:16-19). Reference default: fx1_a3_defaults.h:44-45.
#ifndef FX1_A3_CONTEXT_FSM
#define FX1_A3_CONTEXT_FSM
#endif

// Selects the real FeedersFsm wiring inside Control -- requires FX1_A3_CONTEXT_FSM
// (rtl_ref_main_controller.h:20-23, #error's otherwise). Reference default: fx1_a3_defaults.h:47-48.
#ifndef FX1_A3_FEEDERS_FSM
#define FX1_A3_FEEDERS_FSM
#endif

// Real RTL value (sauria_pkg.sv:37) -- used both by
// ContextSwitchController's template instantiation inside Control (rtl_ref_main_controller.h:221)
// and by the cswitch_arr staggered-generation formula inside ContextSwitchController itself.
// Reference default: fx1_a3_defaults.h:218-219.
#ifndef FX1_A3_EXTRA_CSREG
#define FX1_A3_EXTRA_CSREG 1
#endif

// Default validated against the sauria_model reference
// -- FeedersFsm runs ONCE per convolution, rolling all contexts internally via its own repetition
// counters, instead of being re-armed at every context boundary. Consumed in
// rtl_ref_main_controller.h's ctrl_process() (fd_ctx_edge branch). Reference default:
// fx1_a3_defaults.h:131-132.
#ifndef FX1_A3_NO_CTX_REARM
#define FX1_A3_NO_CTX_REARM
#endif

// -- RTL-exact act til_done into FeedersFsm (repetition counter + til-done shim): same-cycle
// til_done_q & cnt_en (RTL ifmap_idxcnt.sv:274, feeders_fsm.sv:203/288/304) instead of the
// sc_signal value gated by the previous cycle's cnt_en. Reference default: fx1_a3_defaults.h
// (FX1_A3_TILDONE_Q_GATE block, after FX1_A3_CNTCLEAR_RTL).
#ifndef FX1_A3_TILDONE_Q_GATE
#define FX1_A3_TILDONE_Q_GATE
#endif

// Control's o_incntlim/i_mvm_k priority fix (rtl_ref_main_controller.h:506-522) -- RTL feeds
// ContextSwitchController's i_incntlim straight from config_regs.o_incntlim with no i_mvm_k
// fallback; this flag restores that priority (this model previously had it inverted). Reference
// default: fx1_a3_defaults.h:56-57.
#ifndef FX1_A3_INCNTLIM_FIX
#define FX1_A3_INCNTLIM_FIX
#endif

// FX1_A3_WEI_FIN_ONE_REP: wei_fin = wei_til_done_shim only (drops the wei_ov_flag_shim_ term), so a repetition is
// not counted twice (once by FeedersFsm's wei_rep_cnt_q_, once by ContextFsm) when wei_reps = ncontexts.
// OFF by default, as in sauria_model's fx1_a3_defaults.h (which does not define it); with it ON a multi-context
// synthetic shape fails at FIFO depth 16. Opt-in for comparisons: -DFX1_A3_WEI_FIN_ONE_REP_ON.
#if !defined(FX1_A3_WEI_FIN_ONE_REP) && defined(FX1_A3_WEI_FIN_ONE_REP_ON)
#define FX1_A3_WEI_FIN_ONE_REP
#endif

// The following 5 flags all gate same-cycle "peek" reads between Control and its sibling
// modules (feeders/PSM), avoiding a spurious 1-cycle sc_signal lag that real RTL's combinational
// wiring wouldn't have. Consumed inside rtl_ref_main_controller.h's ctrl_process() at the fd_in.*/
// cs_in.* assignment sites; the actual peek_*_ function pointers themselves must be wired in by
// whatever npu_top.h-level code instantiates sauria_rtl::Control (see plan doc 4c, D1). Reference
// defaults: fx1_a3_defaults.h:70-80 (EMPTY_DIRECT/SEAM_ORDER/PIPE_EN_DIRECT), :168-174
// (VALID_DIRECT/START_DIRECT), :208-212 (FDFSM_DIRECT/FDFSM_MASK).
#ifndef FX1_A3_EMPTY_DIRECT
#define FX1_A3_EMPTY_DIRECT
#endif
#ifndef FX1_A3_SEAM_ORDER
#define FX1_A3_SEAM_ORDER
#endif
#ifndef FX1_A3_PIPE_EN_DIRECT
#define FX1_A3_PIPE_EN_DIRECT
#endif
#ifndef FX1_A3_VALID_DIRECT
#define FX1_A3_VALID_DIRECT
#endif
#ifndef FX1_A3_START_DIRECT
#define FX1_A3_START_DIRECT
#endif
#ifndef FX1_A3_FDFSM_DIRECT
#define FX1_A3_FDFSM_DIRECT
#endif
#ifndef FX1_A3_FDFSM_MASK
#define FX1_A3_FDFSM_MASK 0xFF
#endif

// ============================================================================
// Phase B -- feeder-side (data_feeder/rtl_ref_ifmap_feeder.h + rtl_ref_wei_feeder.h, which
// internally compose rtl_ref_ifmap_feeder_rtl.h / rtl_ref_wei_feeder_rtl.h / rtl_ref_ifmap_idxcnt.h
// / rtl_ref_wei_idxcnt.h / rtl_ref_feed_data_manager.h / rtl_ref_feed_xy_lane.h /
// rtl_ref_fifo_memory_ff.h)
// ============================================================================

// Selects the real structural RTL feeder engine (IfmapFeederRtl/WeiFeederRtl, composed of
// idxcnt+feed_xy_lane+skew) inside the IfmapFeeder/WeightFeeder SC_MODULE wrapper, INCLUDING the
// wrapper's own seam_step() method definition -- without this, the wrapper falls back to ~1200
// lines of older non-RTL-accurate behavioral code AND has no seam_step() at all (build error
// "has no member named 'seam_step'"). Reference defaults:
// fx1_a3_defaults.h:53-55 (IFMAP) / :59-61 (WEI).
#ifndef FX1_A3_IFMAP_FEEDER_RTL
#define FX1_A3_IFMAP_FEEDER_RTL
#endif
#ifndef FX1_A3_WEI_FEEDER_RTL
#define FX1_A3_WEI_FEEDER_RTL
#endif

// RTL-accurate FIFO depth split (ACT_FIFO_POSITIONS=5, WEI_FIFO_POSITIONS=4) instead of one
// FIFO_DEPTH=16 for both -- consumed wherever the feeder wrapper/engine sizes its internal FIFO.
// Reference default: fx1_a3_defaults.h:50-52.
#ifndef FX1_A3_FIFO_DEPTH_SPLIT
#define FX1_A3_FIFO_DEPTH_SPLIT
#endif

// Read fifo_empty/fifo_full/stall combinationally (same-cycle) inside rtl_ref_ifmap_feeder_rtl.h /
// rtl_ref_wei_feeder_rtl.h rather than via a 1-cycle-lagged sc_signal. Reference default:
// fx1_a3_defaults.h:62-69.
#ifndef FX1_A3_STALL_SAME_CYCLE
#define FX1_A3_STALL_SAME_CYCLE
#endif
#ifndef FX1_A3_FIFOFLAG_SAME_CYCLE
#define FX1_A3_FIFOFLAG_SAME_CYCLE
#endif

// sram/rtl_ref_sram_top.h (the SRAM port used by the RtlRefLaneACoreA / rtl_ref_npu_top path, not v4.5's
// sram/sram_top.h): SRAMA_CAP / SRAMB_CAP are BYTES, the same unit the feeders use (rtl_ref_ifmap_feeder.h /
// rtl_ref_wei_feeder.h); in rows the capacity would be 32x (Y_DIM * sizeof(T_ACT)) too large and `% CAP` would
// never wrap. SRAM-C stays in rows (FX1_A3_SRAM_CAP_C_ROWS, SRAMC_CAP = 1536). Reference default:
// fx1_a3_defaults.h:155-158.
#ifndef FX1_A3_SRAM_CAP_BYTES
#define FX1_A3_SRAM_CAP_BYTES
#endif
#ifndef FX1_A3_SRAM_CAP_C_ROWS
#define FX1_A3_SRAM_CAP_C_ROWS
#endif

// SRAM-A/B read-enable is 2-stage delayed in real RTL; the un-guarded path is a real functional
// bug (drops/duplicates one FIFO-fill element, per the reference's own header comment), not
// cosmetic -- mandatory for correctness. Reference default: fx1_a3_defaults.h:161-166.
// FX1_A3_SRAMC_RDEN_PHASE stays off: sauria_model shows it is output-neutral (internal trace changes, identical
// output md5), and its validated default is off.
#ifndef FX1_A3_SRAMA_RDEN_PHASE
#define FX1_A3_SRAMA_RDEN_PHASE
#endif
#ifndef FX1_A3_SRAMB_RDEN_PHASE
#define FX1_A3_SRAMB_RDEN_PHASE
#endif

// FX1_A3_FEEDER_RDEN_MATCH_SRAM (off by default): the feeders use a 1-stage rden delay instead of `rden_q2`
// (2 stages) when the SRAM already applies RDEN_PHASE (raw pass-through, see the header of
// sram/rtl_ref_sram_top.h). Kept as an opt-in for A/B comparisons against sauria_model tapes; the default is the
// validated 2-stage path.
#ifndef FX1_A3_FEEDER_RDEN_MATCH_SRAM
// #define FX1_A3_FEEDER_RDEN_MATCH_SRAM
#endif

// SAURIA_ACT_IDX_W / SAURIA_WEI_IDX_W: idxcnt index-counter bit-width, consumed at
// rtl_ref_ifmap_feeder.h:145 / rtl_ref_wei_feeder.h:118 BEFORE either file's own late
// `#ifndef SAURIA_ACT_IDX_W / #define ... 15` fallback (rtl_ref_ifmap_feeder.h:204-205) is
// reached -- a real ordering bug in the reference source itself (their real build supplies this
// via a Makefile -D flag, e.g. tb_evaluate.cpp/tb_demo.cpp per the reference's own comment, not
// via this header). Must be defined before rtl_ref_ifmap_feeder.h/rtl_ref_wei_feeder.h are included.
//
// Value 18: sram_idx_q is IDX_W bits and the SRAM row address is sram_idx_q >> WOFS_W (WOFS_W = 5 at Y_DIM = 32),
// so at most 2^(IDX_W - WOFS_W) rows are addressable, independent of SRAMA_CAP. 15 gives 1024 rows, too few for
// real layers (e.g. Cin = 192 needs rows > 1157, and 3x3 weights of one tile need K * Cout_t = 1728 * 32 bits),
// which silently wraps the stimulus. 18 gives 8192 rows >= 5056 (ROWS_A) and matches the "IDX 18/18/16" preset of
// sauria_model's real-job flow (gen_stim_tiled_real.py). SAURIA_WEI_IDX_W follows SAURIA_ACT_IDX_W.
#ifndef SAURIA_ACT_IDX_W
#define SAURIA_ACT_IDX_W 18
#endif
#ifndef SAURIA_WEI_IDX_W
#define SAURIA_WEI_IDX_W 18
#endif

// ============================================================================
// Phase C -- PSM-side (psm/rtl_ref_psm_top.h, which internally composes
// rtl_ref_psm_shift_fsm.h / rtl_ref_psm_idxcnt.h / rtl_ref_psm_shift_register.h /
// rtl_ref_psm_wdata_manager.h / rtl_ref_psm_rdata_manager.h)
// ============================================================================

// Selects the real PsmShiftFsm+PsmIdxCnt RTL-accurate timing path inside Psm; without it,
// psm_top.h falls back to its own older 6-phase psm_scan_phase state machine (structurally the
// same style of code as this tree's OWN existing psm_top.h). Reference default:
// fx1_a3_defaults.h:83-85.
#ifndef FX1_A3_PSM_SHIFT_FSM
#define FX1_A3_PSM_SHIFT_FSM
#endif
// Nested inside FX1_A3_PSM_SHIFT_FSM: selects the real PsmShiftRegister+PsmWdataManager+
// PsmRdataManager output/input datapath. Reference default: fx1_a3_defaults.h:86-88.
#ifndef FX1_A3_PSM_WDATA_MGR
#define FX1_A3_PSM_WDATA_MGR
#endif
// Wires the real i_preload_en into PsmShiftFsm and sets force_write_path=false (enables the real
// READ states) -- WITHOUT it, force_write_path stays true (every context writes immediately, no
// preload-warmup pipeline), which the reference's own comment calls a deliberate simplification
// for single-context testing, not the real RTL path. Reference default: fx1_a3_defaults.h:89-91.
#ifndef FX1_A3_PSM_REAL_PRELOAD
#define FX1_A3_PSM_REAL_PRELOAD
#endif
// i_inactive_cols input of Psm (psm/rtl_ref_psm_top.h): without it the FSM never shifts all columns when
// X_used < X, and the preload contribution is lost on every tile with X_used < 32. Reference default:
// fx1_a3_defaults.h:~92.
#ifndef FX1_A3_PSM_INACTIVE_COLS
#define FX1_A3_PSM_INACTIVE_COLS
#endif
// Same seam-ordered convention as Control/IfmapFeeder/WeightFeeder -- Psm does NOT self-register
// an SC_METHOD; the top-level orchestrator must call psm.seam_step() in order (after Control and
// both feeders). Psm's own header comment explains why: without this, Psm and the PE array would
// be two independent SC_METHODs reading each other's sc_signal one cycle late, a real measured
// correctness bug. Reference default: fx1_a3_defaults.h:105-109.
#ifndef FX1_A3_PSM_SEAM_ORDER
#define FX1_A3_PSM_SEAM_ORDER
#endif
// PsmIdxCnt's output-shimming register reads pre-tick _q values (fixes a missing pipeline stage).
// Reference default: fx1_a3_defaults.h:115-117.
#ifndef FX1_A3_IDXCNT_SHIM_PRETICK
#define FX1_A3_IDXCNT_SHIM_PRETICK
#endif
// Write address uses a 4th shim stage (sramc_addr_q4_) instead of q3. Reference default:
// fx1_a3_defaults.h:123-127.
#ifndef FX1_A3_WRADDR_Q4
#define FX1_A3_WRADDR_Q4
#endif
// PSM tracks its own write-side context counter instead of capturing i_context_id directly.
// Reference default: fx1_a3_defaults.h:128-130.
#ifndef FX1_A3_PSM_CTX_OWNCNT
#define FX1_A3_PSM_CTX_OWNCNT
#endif
// Scan-window/gather-beat timing refinements (capture window tracks the permitted-beat domain
// rather than raw cycles; flush an outstanding gather beat when the window closes; separate copy
// of the scan window for the array side). Reference defaults: fx1_a3_defaults.h:176-186.
#ifndef FX1_A3_CSCAN_CAP_PIPE
#define FX1_A3_CSCAN_CAP_PIPE
#endif
#ifndef FX1_A3_CSCAN_CAP_FLUSH
#define FX1_A3_CSCAN_CAP_FLUSH
#endif
#ifndef FX1_A3_CSCAN_ARR_PIPE
#define FX1_A3_CSCAN_ARR_PIPE
#endif
// i_fsm_start treated as RTL's combinational wire (via a peek_fsm_start_ function pointer the
// npu_top-level code wires to Control's own pre-tick outbuf_start, same pattern as
// FX1_A3_EMPTY_DIRECT for feeders) instead of a 1-cycle-delayed sc_signal; keeps a pending-start
// latch across the reset window. Reference default: fx1_a3_defaults.h:225-227.
#ifndef FX1_A3_PSM_START_DIRECT
#define FX1_A3_PSM_START_DIRECT
#endif
// o_c_arr / o_cscan_en output-timing refinements: o_c_arr takes the shift register's POST-tick
// value; the scan-beat shadow doesn't OR with the previous beat; o_c_arr's mux condition matches
// o_cscan_en's own RTL-literal gating; o_cscan_en/o_c_arr are actually driven on the live path
// (without FX1_A3_CARR_DRIVE this was previously dead code -- the PE never latched preload data
// at all); holds the last real o_c_arr value during gaps instead of emitting 0. Reference
// defaults: fx1_a3_defaults.h:262-291.
#ifndef FX1_A3_CARR_POSTTICK
#define FX1_A3_CARR_POSTTICK
#endif
#ifndef FX1_A3_CSCAN_BEAT_NOOR
#define FX1_A3_CSCAN_BEAT_NOOR
#endif
#ifndef FX1_A3_CARR_MUX_MATCH
#define FX1_A3_CARR_MUX_MATCH
#endif
#ifndef FX1_A3_CARR_DRIVE
#define FX1_A3_CARR_DRIVE
#endif
#ifndef FX1_A3_CARR_GAP_HOLD
#define FX1_A3_CARR_GAP_HOLD
#endif

// ============================================================================
// Systolic array (systolic_array/rtl_ref_sa_array.h) and one controller macro
// ============================================================================
//
// Of the reference macros not defined above, only these 2 are read by files ported into this tree. The others
// (FX1_A3_ACT_DILPAT_HI32, FX1_A3_SRAMC_TILE_GATHER, FX1_A3_SRAMC_LOAD_STRIDE, FX1_A3_SRAMC_LOAD_I32_EXACT,
// FX1_A3_SRAMC_RESEED_I32_EXACT, FX1_A3_SRAMC_SEED_TILE0) govern code that was not ported, so they stay undefined.
#ifndef FX1_A3_CSDELAY_MODE
// rtl_ref_sa_array.h's own #ifndef fallback (only reached if this file leaves it undefined) sets
// mode 0 -- the OLD hand-tuned formula, not RTL-derived. sauria_model's real default is mode 3
// (fx1_a3_defaults.h:141-142) -- the full RTL formula (y + EXTRA_CSREG), the one the array's own
// header comment (see rtl_ref_sa_array.h's FX1_A3_CONTEXT_FSM block) says is what
// ContextSwitchController's staggered cswitch_arr timing actually expects.
#define FX1_A3_CSDELAY_MODE 3
#endif
#ifndef FX1_A3_CNTCLEAR_RTL
// Referenced in rtl_ref_main_controller.h:1230. Default on, matching sauria_model's configuration.
#define FX1_A3_CNTCLEAR_RTL
#endif

// FX1_DUMPS: gates rtl_ref_psm_top.h's CSV trace-dump helper (dump_psm_trace()) -- this is a
// plain int, not a boolean #ifdef, and rtl_ref_psm_top.h reads it unconditionally (no #ifndef
// guard around the read site itself), so it must always be defined to SOMETHING. Reference
// pattern: fx1_a3_defaults.h:239-243 -- `#ifdef FX1_A3_DEBUG_DUMPS / #define FX1_DUMPS 1 / #else
// / #define FX1_DUMPS 0 / #endif`. Debug/trace only, not core logic -- default OFF (0) here,
// matching the reference's own default (FX1_A3_DEBUG_DUMPS is not defined above).
#ifndef FX1_DUMPS
#ifdef FX1_A3_DEBUG_DUMPS
#define FX1_DUMPS 1
#else
#define FX1_DUMPS 0
#endif
#endif
