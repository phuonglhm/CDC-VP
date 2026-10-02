#pragma once
//
// 1:1 copy of sauria_model's control/context_fsm.h, itself a 1:1 port of RTL
// sauria_core/control/context_fsm.sv.
//
// ONLY CHANGE from the reference file: namespace `sauria` -> `sauria_rtl` (see
// rtl_ref_context_switch_controller.h's header comment for why -- avoids colliding with this
// tree's own `sauria::Control`/`IfmapFeeder`/`WeightFeeder`/`Psm`, all included alongside this
// file in control/native_lane_a_core.h). Logic below is otherwise byte-for-byte identical to the
// reference file.
//
// Reference header (functional notes):
//
// This is the RTL's TOP-LEVEL context control FSM -- the module whose Verilog
// enum names main_controller.h's `ctrl_state_t` already borrows (22 states) but
// whose actual state machine main_controller.h's switch(state) never implements
// (only ~11 of 22 states are reachable there; OBUF_BUSY/ARRAY_BUSY/ALL_BUSY are
// dead code). This class is the REAL 22-state FSM plus RTL's separate 2-state
// stall sub-FSM (ARRAY_ACTIVE/ARRAY_STALL), which the RTL keeps as a second,
// independent always_comb block gating o_pipeline_gate off `computation_ready`
// (computed by the main FSM's output logic) crossed with an i_cdone edge.
//
// Do not "clean up" or reinterpret the logic without re-checking against the
// RTL source (port 1:1, do not re-derive). Every block below is traceable to a specific line range in
// context_fsm.sv (noted in comments).
//
// Two-phase tick(): Phase 1 computes every combinational value (this cycle's
// outputs AND every register's next-state) purely from pre-tick register state
// + this cycle's inputs -- mirroring how SV always_comb blocks all see the same
// pre-edge register values regardless of textual order. Phase 2 commits all
// next-state values, as if a single clock edge fired. (Same convention as
// ifmap_idxcnt.h / feed_data_manager.h.)
//
// -----------------------------------------------------------------------------
// Handshake-signal mapping (RTL context_fsm.sv is a standalone module fed by
// OTHER RTL modules; SC's Control (main_controller.h) is a single monolithic
// process with no such cross-module ports). This port derives each RTL input
// from state Control already has, instead of adding cross-module SC ports:
//
// i_cdone / i_cswitch_done come from a SEPARATE module, context_switch_controller.sv
// (main_controller.sv instantiates context_fsm.sv + context_switch_controller.sv +
// feeders_fsm.sv side by side), which also produces the real o_cswitch_arr
// array-swap pulse (context_fsm.sv does not). It is ported as
// ContextSwitchController (control/rtl_ref_context_switch_controller.h). This class's compute_outputs()/
// commit() split exists SPECIFICALLY so the wiring layer can run
// ContextSwitchController::tick() in between, using THIS tick's cswitch_en/
// cswitch_force/cswitch_cnt_clear outputs, before feeding the real cdone/
// cswitch_done back in for commit(). See context_switch_controller.h's own
// header comment for that module's mapping details.
//
//   i_cdone         <- REAL now: sauria::ContextSwitchController::Outputs::cdone
//   i_cswitch_done  <- REAL now: sauria::ContextSwitchController::Outputs::cswitch_done
//   i_outbuf_done   <- Control's `i_shift_done.read()` (psm_top.h's o_shift_done
//                       pulse, fires once at the end of each context's PSM
//                       scan). Ported RTL's own outbuf_done_hold latch (lines
//                       114-142) INSIDE this class -- do NOT feed a pre-latched
//                       value in, feed the RAW pulse, matching RTL exactly.
//   i_shift_done    <- SAME i_shift_done as above (deliberate simplification:
//                       RTL distinguishes an early "first-ready"
//                       i_shift_done from a later, fuller i_outbuf_done; SC's
//                       psm_top.h only has one signal (o_shift_done, fires at
//                       the END of the scan -- closer to RTL's i_outbuf_done
//                       semantics). This conflation only affects the ONE-TIME
//                       FIRST_SHIFT startup wait, not any per-context recurring
//                       cost.
//   i_finalwrite &&
//   i_feeders_done  <- Control's `(context_cnt+1) >= total_contexts` (already
//                       used by the pre-port LAST_WAIT code to decide DONE vs
//                       next-context) -- same "is this the last context in the
//                       layer" meaning, just consulted at the RIGHT point
//                       (ARRAY_CSWITCH's branch) instead of only at LAST_WAIT.
//   i_pipeline_en   <- Control's `!(i_act_stall.read() || i_wei_stall.read())`.
//                       In real RTL this is cross-module feedback (context_fsm's
//                       own o_pipeline_gate feeds feeders_fsm's i_pipeline_gate,
//                       which ANDs in feeder-FIFO-empty checks and feeds back
//                       as context_fsm's i_pipeline_en). Control's EXISTING
//                       feeder_stall check (i_act_stall||i_wei_stall, already
//                       read every cycle in the pre-port START_COMP code) is
//                       the same underlying "are feeders able to supply data
//                       right now" condition, reused instead of modeling the
//                       cross-module loop literally.
// -----------------------------------------------------------------------------

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace sauria_rtl
{

    template <int X_DIM = 32, int Y_DIM = 32>
    class ContextFsm
    {
    public:
        enum class MainState
        {
            IDLE,
            START_FLAGS,
            ARRAY_PREP,
            FIRST_SHIFT,
            START_COMP,
            WAIT_CSWITCH,
            WAIT_CSWITCH_STALL,
            WAIT_OBUF,
            WAIT_OBUF_STALL,
            SCND_SHIFT,
            SCND_SHIFT_STALL,
            ALL_BUSY_SHIFT,
            ALL_BUSY,
            ARRAY_BUSY,
            OBUF_BUSY_SHIFT,
            FORCE_STALL,
            OBUF_BUSY,
            ARRAY_CSWITCH,
            ARRAY_CSWITCH_STALL,
            LAST_SHIFT,
            LAST_WAIT,
            DONE
        };

        enum class StallState
        {
            ARRAY_ACTIVE,
            ARRAY_STALL
        };

        struct Inputs
        {
            bool soft_reset = false; // i_soft_reset
            bool start = false;      // i_start
            bool outbuf_done = false; // i_outbuf_done (RAW pulse, see mapping note)
            bool cdone = false;       // i_cdone -- REAL input now:
                                      // from sauria::ContextSwitchController::Outputs::cdone,
                                      // NOT a simple comp_cycles>=incntlim level. Fed by the
                                      // wiring layer AFTER calling compute_outputs() (see below).
            bool cswitch_done = false; // i_cswitch_done -- REAL input now, same source
                                       // (ContextSwitchController::Outputs::cswitch_done).
                                       // Previously self-computed inside this class via an
                                       // internal margin counter -- WRONG (fired at the wrong
                                       // point, confirmed by tracing demo_gemm_32x32 to near-zero
                                       // MAC snapshots). Removed; see context_switch_controller.h.
            bool shift_done = false;  // i_shift_done (see mapping note: same signal as outbuf_done)
            bool finalwrite = false;  // i_finalwrite
            bool feeders_done = false; // i_feeders_done
            bool pipeline_en = false;  // i_pipeline_en (feedback, see mapping note)
        };

        struct Outputs
        {
            // Feeders FSM control
            bool pipeline_gate = false; // o_pipeline_gate
            bool feeders_start = false; // o_feeders_start
            bool feeders_reset = true;  // o_feeders_reset
            bool pop_gate = false;      // o_pop_gate

            // Output Buffer control
            bool outbuf_start = false; // o_outbuf_start
            bool outbuf_reset = true; // o_outbuf_reset

            // Context Switch Controller control -- these three are INPUTS to
            // sauria::ContextSwitchController (i_cswitch_en/i_cswitch_force/
            // i_clear), which OWNS the real o_cswitch_arr/o_cdone/o_cswitch_done
            // generation. This class does NOT produce a cswitch_arr pulse
            // itself (real context_fsm.sv doesn't either -- see main_controller.sv,
            // the RTL wrapper that instantiates both modules side by side).
            bool cswitch_en = false;      // o_cswitch_en
            bool cswitch_force = false;   // o_cswitch_force
            bool cswitch_cnt_clear = true; // o_cswitch_cnt_clear

            // Systolic Array control
            bool sa_clear = true; // o_sa_clear

            // Status
            uint32_t ctx_status = 30; // o_ctx_status

            // External
            bool done = false; // o_done
        };

        // `outbuf_enable_d` depends on the STATE only (all 22 states checked against the RTL). Split out so the PSM can
        // PEEK it BEFORE Control ticks -- the RTL wires o_outbuf_start to the PSM combinationally.
        static bool outbuf_enable_of(MainState s)
        {
            switch (s)
            {
            case MainState::START_FLAGS:
            case MainState::FIRST_SHIFT:
            case MainState::SCND_SHIFT:
            case MainState::SCND_SHIFT_STALL:
            case MainState::ALL_BUSY_SHIFT:
            case MainState::ALL_BUSY:
            case MainState::OBUF_BUSY_SHIFT:
            case MainState::OBUF_BUSY:
            case MainState::LAST_SHIFT:
                return true;
            default:
                return false;
            }
        }
        // o_outbuf_start of THIS cycle, from two pre-tick REGISTERS.
        bool peek_outbuf_start() const
        {
            return outbuf_enable_of(main_state_) && !outbuf_enable_q_;
        }

        void reset()
        {
            main_state_ = MainState::IDLE;
            stall_state_ = StallState::ARRAY_STALL;
            outbuf_done_q_ = false;
            outbuf_enable_q_ = false;
        }

        // Phase 1: output logic for the CURRENT (pre-tick) state. Does NOT
        // read in.cdone/in.cswitch_done (real RTL's output_logic doesn't
        // either -- only the transition logic does, see commit() below).
        // Caches everything commit() will need, since the wiring layer must
        // feed this call's cswitch_en/cswitch_force/cswitch_cnt_clear outputs
        // into a sauria::ContextSwitchController BEFORE it can know the real
        // cdone/cswitch_done values that commit() needs.
        Outputs compute_outputs(const Inputs &in)
        {
            // ---------- Phase 1: combinational (pre-tick register state) ----------
            //
            // NOTE on i_soft_reset: RTL's output_logic (context_fsm.sv lines
            // 488-1023) has NO soft_reset branch at all -- it is a plain
            // `case(main_state_q)` that always reflects whatever state we were
            // PREVIOUSLY in, regardless of i_soft_reset. Only the TRANSITION
            // logic (lines 246-486) force-jumps main_state_d = IDLE under
            // i_soft_reset, so IDLE's outputs only appear on the FOLLOWING
            // tick, once main_state_q has actually become IDLE. Do NOT mutate
            // main_state_ here in Phase 1 -- that would make this tick's own
            // outputs jump to IDLE early, one cycle ahead of real RTL. The
            // soft_reset effect is applied ONLY in the transition-logic and
            // register-commit sections below (next_state / outbuf_done_q_).

            // --- outbuf_done_hold (RTL lines 114-142) ---
            // Combinational hold: latches i_outbuf_done until the NEXT
            // o_outbuf_start or o_outbuf_reset pulse. Computed here from the
            // PRE-tick outbuf_done_q_ + THIS cycle's raw i_outbuf_done, matching
            // RTL's `outbuf_done_hold = (i_outbuf_done || outbuf_done_q) &&
            // !(o_outbuf_start || o_outbuf_reset)` -- but o_outbuf_start/
            // o_outbuf_reset are THIS tick's OWN outputs (computed below in the
            // same phase), so we compute outbuf_done_hold AFTER main_state_d's
            // output block, using that block's outbuf_start/outbuf_reset values,
            // exactly mirroring RTL's same-cycle combinational dependency.

            // --- Main FSM: output logic for the CURRENT (pre-tick) state ---
            // (RTL lines 488-1023, "Context FSM - Output Logic" -- always_comb,
            // defaults-then-per-state-override pattern ported as a switch that
            // fully re-sets every field each branch, matching RTL's structure.)
            bool computation_ready = false;
            Outputs out{};
            out.feeders_reset = true;
            out.outbuf_reset = true;
            out.cswitch_cnt_clear = true;
            out.sa_clear = true;
            out.ctx_status = 30;
            bool outbuf_enable_d = false; // for o_outbuf_start edge-detect (RTL line 157)
            bool stall_state_reset = false;
            bool force_stall = false;

            switch (main_state_)
            {
            case MainState::IDLE:
                computation_ready = false;
                out.feeders_start = false;
                out.feeders_reset = true;
                out.pop_gate = false;
                outbuf_enable_d = false;
                out.outbuf_reset = true;
                out.cswitch_en = false;
                out.cswitch_force = false;
                out.cswitch_cnt_clear = true;
                out.sa_clear = true;
                out.done = true;
                stall_state_reset = false;
                force_stall = false;
                out.ctx_status = 0;
                break;

            case MainState::START_FLAGS:
                computation_ready = false;
                out.feeders_start = true;
                out.feeders_reset = false;
                out.pop_gate = false;
                outbuf_enable_d = true;
                out.outbuf_reset = false;
                out.cswitch_en = false;
                out.cswitch_force = false;
                out.cswitch_cnt_clear = false;
                out.sa_clear = false;
                out.done = false;
                stall_state_reset = false;
                force_stall = false;
                out.ctx_status = 1;
                break;

            case MainState::ARRAY_PREP:
                computation_ready = true;
                out.feeders_start = false;
                out.feeders_reset = false;
                out.pop_gate = false;
                outbuf_enable_d = false;
                out.outbuf_reset = false;
                out.cswitch_en = false;
                out.cswitch_force = false;
                out.cswitch_cnt_clear = false;
                out.sa_clear = false;
                out.done = false;
                stall_state_reset = false;
                force_stall = false;
                out.ctx_status = 2;
                break;

            case MainState::FIRST_SHIFT:
                computation_ready = true;
                out.feeders_start = false;
                out.feeders_reset = false;
                out.pop_gate = false;
                outbuf_enable_d = true;
                out.outbuf_reset = false;
                out.cswitch_en = false;
                out.cswitch_force = false;
                out.cswitch_cnt_clear = false;
                out.sa_clear = false;
                out.done = false;
                stall_state_reset = false;
                force_stall = false;
                out.ctx_status = 3;
                break;

            case MainState::START_COMP:
                computation_ready = true;
                out.feeders_start = false;
                out.feeders_reset = false;
                out.pop_gate = true;
                outbuf_enable_d = false;
                out.outbuf_reset = false;
                out.cswitch_en = true;
                out.cswitch_force = true;
                out.cswitch_cnt_clear = false;
                out.sa_clear = false;
                out.done = false;
                stall_state_reset = false;
                force_stall = false;
                out.ctx_status = 4;
                break;

            case MainState::WAIT_CSWITCH:
                computation_ready = true;
                out.feeders_start = false;
                out.feeders_reset = false;
                out.pop_gate = true;
                outbuf_enable_d = false;
                out.outbuf_reset = false;
                out.cswitch_en = true;
                out.cswitch_force = false;
                out.cswitch_cnt_clear = false;
                out.sa_clear = false;
                out.done = false;
                stall_state_reset = false;
                force_stall = false;
                out.ctx_status = 5;
                break;

            case MainState::WAIT_CSWITCH_STALL:
                computation_ready = true;
                out.feeders_start = false;
                out.feeders_reset = false;
                out.pop_gate = false;
                outbuf_enable_d = false;
                out.outbuf_reset = false;
                out.cswitch_en = true;
                out.cswitch_force = false;
                out.cswitch_cnt_clear = false;
                out.sa_clear = false;
                out.done = false;
                stall_state_reset = false;
                force_stall = false;
                out.ctx_status = 6;
                break;

            case MainState::WAIT_OBUF:
                computation_ready = true;
                out.feeders_start = false;
                out.feeders_reset = false;
                out.pop_gate = true;
                outbuf_enable_d = false;
                out.outbuf_reset = false;
                out.cswitch_en = false;
                out.cswitch_force = false;
                out.cswitch_cnt_clear = false;
                out.sa_clear = false;
                out.done = false;
                stall_state_reset = false;
                force_stall = false;
                out.ctx_status = 7;
                break;

            case MainState::WAIT_OBUF_STALL:
                computation_ready = true;
                out.feeders_start = false;
                out.feeders_reset = false;
                out.pop_gate = false;
                outbuf_enable_d = false;
                out.outbuf_reset = false;
                out.cswitch_en = false;
                out.cswitch_force = false;
                out.cswitch_cnt_clear = false;
                out.sa_clear = false;
                out.done = false;
                stall_state_reset = false;
                force_stall = false;
                out.ctx_status = 8;
                break;

            case MainState::SCND_SHIFT:
                computation_ready = true;
                out.feeders_start = false;
                out.feeders_reset = false;
                out.pop_gate = true;
                outbuf_enable_d = true;
                out.outbuf_reset = false;
                out.cswitch_en = false;
                out.cswitch_force = false;
                out.cswitch_cnt_clear = false;
                out.sa_clear = false;
                out.done = false;
                stall_state_reset = false;
                force_stall = false;
                out.ctx_status = 9;
                break;

            case MainState::SCND_SHIFT_STALL:
                computation_ready = true;
                out.feeders_start = false;
                out.feeders_reset = false;
                out.pop_gate = false;
                outbuf_enable_d = true;
                out.outbuf_reset = false;
                out.cswitch_en = false;
                out.cswitch_force = false;
                out.cswitch_cnt_clear = false;
                out.sa_clear = false;
                out.done = false;
                stall_state_reset = false;
                force_stall = false;
                out.ctx_status = 10;
                break;

            case MainState::ALL_BUSY_SHIFT:
                computation_ready = true;
                out.feeders_start = false;
                out.feeders_reset = false;
                out.pop_gate = true;
                outbuf_enable_d = true;
                out.outbuf_reset = false;
                out.cswitch_en = false;
                out.cswitch_force = false;
                out.cswitch_cnt_clear = false;
                out.sa_clear = false;
                out.done = false;
                stall_state_reset = false;
                force_stall = false;
                out.ctx_status = 11;
                break;

            case MainState::ALL_BUSY:
                computation_ready = false;
                out.feeders_start = false;
                out.feeders_reset = false;
                out.pop_gate = true;
                outbuf_enable_d = true;
                out.outbuf_reset = false;
                out.cswitch_en = false;
                out.cswitch_force = false;
                out.cswitch_cnt_clear = false;
                out.sa_clear = false;
                out.done = false;
                stall_state_reset = false;
                force_stall = false;
                out.ctx_status = 12;
                break;

            case MainState::ARRAY_BUSY:
                computation_ready = true;
                out.feeders_start = false;
                out.feeders_reset = false;
                out.pop_gate = true;
                outbuf_enable_d = false;
                out.outbuf_reset = false;
                out.cswitch_en = true;
                out.cswitch_force = false;
                out.cswitch_cnt_clear = false;
                out.sa_clear = false;
                out.done = false;
                stall_state_reset = false;
                force_stall = false;
                out.ctx_status = 13;
                break;

            case MainState::OBUF_BUSY_SHIFT:
                computation_ready = true;
                out.feeders_start = false;
                out.feeders_reset = false;
                out.pop_gate = false;
                outbuf_enable_d = true;
                out.outbuf_reset = false;
                out.cswitch_en = false;
                out.cswitch_force = false;
                out.cswitch_cnt_clear = false;
                out.sa_clear = false;
                out.done = false;
                stall_state_reset = false;
                force_stall = false;
                out.ctx_status = 14;
                break;

            case MainState::FORCE_STALL:
                computation_ready = false;
                out.feeders_start = false;
                out.feeders_reset = false;
                out.pop_gate = false;
                outbuf_enable_d = false;
                out.outbuf_reset = false;
                out.cswitch_en = false;
                out.cswitch_force = false;
                out.cswitch_cnt_clear = false;
                out.sa_clear = false;
                out.done = false;
                stall_state_reset = false;
                force_stall = true;
                out.ctx_status = 15;
                break;

            case MainState::OBUF_BUSY:
                computation_ready = false;
                out.feeders_start = false;
                out.feeders_reset = false;
                out.pop_gate = true;
                outbuf_enable_d = true;
                out.outbuf_reset = false;
                out.cswitch_en = false;
                out.cswitch_force = false;
                out.cswitch_cnt_clear = false;
                out.sa_clear = false;
                out.done = false;
                stall_state_reset = false;
                force_stall = false;
                out.ctx_status = 16;
                break;

            case MainState::ARRAY_CSWITCH:
                computation_ready = true;
                out.feeders_start = false;
                out.feeders_reset = false;
                out.pop_gate = true;
                outbuf_enable_d = false;
                out.outbuf_reset = false;
                out.cswitch_en = true;
                out.cswitch_force = false;
                out.cswitch_cnt_clear = false;
                out.sa_clear = false;
                out.done = false;
                stall_state_reset = false;
                force_stall = false;
                out.ctx_status = 17;
                break;

            case MainState::ARRAY_CSWITCH_STALL:
                computation_ready = true;
                out.feeders_start = false;
                out.feeders_reset = false;
                out.pop_gate = false;
                outbuf_enable_d = false;
                out.outbuf_reset = false;
                out.cswitch_en = true;
                out.cswitch_force = false;
                out.cswitch_cnt_clear = false;
                out.sa_clear = false;
                out.done = false;
                stall_state_reset = false;
                force_stall = false;
                out.ctx_status = 18;
                break;

            case MainState::LAST_SHIFT:
                computation_ready = true;
                out.feeders_start = false;
                out.feeders_reset = false;
                out.pop_gate = false;
                outbuf_enable_d = true;
                out.outbuf_reset = false;
                out.cswitch_en = false;
                out.cswitch_force = false;
                out.cswitch_cnt_clear = false;
                out.sa_clear = false;
                out.done = false;
                stall_state_reset = false;
                force_stall = false;
                out.ctx_status = 19;
                break;

            case MainState::LAST_WAIT:
                computation_ready = true;
                out.feeders_start = false;
                out.feeders_reset = false;
                out.pop_gate = false;
                outbuf_enable_d = false;
                out.outbuf_reset = false;
                out.cswitch_en = false;
                out.cswitch_force = false;
                out.cswitch_cnt_clear = false;
                out.sa_clear = false;
                out.done = false;
                stall_state_reset = false;
                force_stall = false;
                out.ctx_status = 20;
                break;

            case MainState::DONE:
                computation_ready = false;
                out.feeders_start = false;
                out.feeders_reset = true;
                out.pop_gate = false;
                outbuf_enable_d = false;
                out.outbuf_reset = true;
                out.cswitch_en = false;
                out.cswitch_force = false;
                out.cswitch_cnt_clear = false;
                out.sa_clear = false;
                out.done = true;
                stall_state_reset = true;
                force_stall = false;
                out.ctx_status = 21;
                break;
            }

            // --- o_outbuf_start edge-detect (RTL line 157: outbuf_enable_d &
            // !outbuf_enable_q) ---
            out.outbuf_start = outbuf_enable_d && !outbuf_enable_q_;
            // SELF-CHECK: the split-out table must match the switch block exactly.
            if (outbuf_enable_of(main_state_) != outbuf_enable_d)
            {
                std::fprintf(stderr,
                    "[FX1_A3_PSM_START_DIRECT] outbuf_enable_of table MISMATCH at "
                    "ctx_status=%d (bang=%d, that=%d)\n",
                    (int)out.ctx_status, (int)outbuf_enable_of(main_state_),
                    (int)outbuf_enable_d);
                std::abort();
            }

            // --- outbuf_done_hold, computed AFTER this tick's outbuf_start/
            // outbuf_reset are known (RTL line 142) ---
            bool outbuf_done_hold = (in.outbuf_done || outbuf_done_q_) &&
                                     !(out.outbuf_start || out.outbuf_reset);

            // --- o_pipeline_gate reflects the PRE-tick stall_state_ (RTL's
            // stalls_output_logic reads stall_state_q, not stall_state_d) --
            // computed here since it does not depend on cdone/cswitch_done at
            // all, unlike the stall sub-FSM's TRANSITION (computed in commit()). ---
            out.pipeline_gate = (stall_state_ == StallState::ARRAY_ACTIVE);

            // Cache everything commit() will need (real cdone/cswitch_done
            // are not known yet -- the wiring layer must run
            // ContextSwitchController::tick() using THIS call's out.cswitch_en/
            // cswitch_force/cswitch_cnt_clear first, then call commit()).
            cached_computation_ready_ = computation_ready;
            cached_outbuf_enable_d_ = outbuf_enable_d;
            cached_stall_state_reset_ = stall_state_reset;
            cached_force_stall_ = force_stall;
            cached_outbuf_done_hold_ = outbuf_done_hold;
            cached_outbuf_start_ = out.outbuf_start;
            cached_outbuf_reset_ = out.outbuf_reset;

            return out;
        }

        // Phase 2: transition logic + register commit. Requires in.cdone and
        // in.cswitch_done to be the REAL values (from ContextSwitchController,
        // fed into it using THIS SAME cycle's cswitch_en/cswitch_force from the
        // compute_outputs() call immediately before this one) -- everything
        // else needed is either in `in` or cached from that same call.
        void commit(const Inputs &in)
        {
            bool outbuf_done_hold = cached_outbuf_done_hold_;

            // --- Main FSM: transition logic (RTL lines 246-486) ---
            MainState next_state = main_state_;
            if (in.soft_reset)
            {
                next_state = MainState::IDLE;
            }
            else
            {
                switch (main_state_)
                {
                case MainState::IDLE:
                    if (in.start)
                        next_state = MainState::START_FLAGS;
                    break;

                case MainState::START_FLAGS:
                    next_state = MainState::ARRAY_PREP;
                    break;

                case MainState::ARRAY_PREP:
                    if (outbuf_done_hold)
                        next_state = MainState::FIRST_SHIFT;
                    break;

                case MainState::FIRST_SHIFT:
                    if (in.shift_done)
                        next_state = MainState::START_COMP;
                    break;

                case MainState::START_COMP:
                    if (in.pipeline_en)
                        next_state = MainState::WAIT_CSWITCH;
                    break;

                case MainState::WAIT_CSWITCH:
                    if (in.cdone && in.cswitch_done)
                        next_state = MainState::WAIT_OBUF_STALL;
                    else if (in.cdone)
                        next_state = MainState::WAIT_CSWITCH_STALL;
                    else if (in.cswitch_done)
                        next_state = MainState::WAIT_OBUF;
                    break;

                case MainState::WAIT_CSWITCH_STALL:
                    if (in.cswitch_done)
                        next_state = MainState::WAIT_OBUF_STALL;
                    break;

                case MainState::WAIT_OBUF:
                    if (in.cdone && outbuf_done_hold)
                        next_state = MainState::SCND_SHIFT_STALL;
                    else if (in.cdone)
                        next_state = MainState::WAIT_OBUF_STALL;
                    else if (outbuf_done_hold)
                        next_state = MainState::SCND_SHIFT;
                    break;

                case MainState::WAIT_OBUF_STALL:
                    if (outbuf_done_hold)
                        next_state = MainState::SCND_SHIFT_STALL;
                    break;

                case MainState::SCND_SHIFT:
                    if (in.cdone)
                        next_state = MainState::OBUF_BUSY_SHIFT;
                    else
                        next_state = MainState::ALL_BUSY_SHIFT;
                    break;

                case MainState::SCND_SHIFT_STALL:
                    if (outbuf_done_hold)
                        next_state = MainState::ARRAY_CSWITCH;
                    break;

                case MainState::ALL_BUSY_SHIFT:
                    if (in.shift_done && in.cdone)
                        next_state = MainState::FORCE_STALL;
                    else if (in.cdone)
                        next_state = MainState::OBUF_BUSY_SHIFT;
                    else if (in.shift_done)
                        next_state = MainState::ALL_BUSY;
                    break;

                case MainState::ALL_BUSY:
                    // *** This is the branch this whole port exists for. ***
                    if (outbuf_done_hold && in.cdone)
                        next_state = MainState::ARRAY_CSWITCH;
                    else if (outbuf_done_hold)
                        next_state = MainState::ARRAY_BUSY; // desirable: outbuf finished first
                    else if (in.cdone)
                        next_state = MainState::OBUF_BUSY; // undesirable: compute finished first, must wait
                    break;

                case MainState::ARRAY_BUSY:
                    if (in.cdone)
                        next_state = MainState::ARRAY_CSWITCH;
                    break;

                case MainState::OBUF_BUSY_SHIFT:
                    if (outbuf_done_hold)
                        next_state = MainState::ARRAY_CSWITCH;
                    else if (in.shift_done)
                        next_state = MainState::FORCE_STALL;
                    break;

                case MainState::FORCE_STALL:
                    if (outbuf_done_hold)
                        next_state = MainState::ARRAY_CSWITCH;
                    else
                        next_state = MainState::OBUF_BUSY;
                    break;

                case MainState::OBUF_BUSY:
                    if (outbuf_done_hold)
                        next_state = MainState::ARRAY_CSWITCH;
                    break;

                case MainState::ARRAY_CSWITCH:
                {
                    bool is_last_context = in.finalwrite && in.feeders_done;
                    if (in.cdone && in.cswitch_done)
                    {
                        next_state = is_last_context ? MainState::LAST_SHIFT
                                                      : MainState::OBUF_BUSY_SHIFT;
                    }
                    else if (in.cdone)
                    {
                        next_state = MainState::ARRAY_CSWITCH_STALL;
                    }
                    else if (in.cswitch_done)
                    {
                        next_state = is_last_context ? MainState::LAST_SHIFT
                                                      : MainState::ALL_BUSY_SHIFT;
                    }
                    break;
                }

                case MainState::ARRAY_CSWITCH_STALL:
                {
                    bool is_last_context = in.finalwrite && in.feeders_done;
                    if (in.cswitch_done)
                    {
                        next_state = is_last_context ? MainState::LAST_SHIFT
                                                      : MainState::OBUF_BUSY_SHIFT;
                    }
                    break;
                }

                case MainState::LAST_SHIFT:
                    next_state = MainState::LAST_WAIT;
                    break;

                case MainState::LAST_WAIT:
                    if (outbuf_done_hold)
                        next_state = MainState::DONE;
                    break;

                case MainState::DONE:
                    next_state = MainState::IDLE;
                    break;
                }
            }

            // --- Stall sub-FSM (RTL lines 177-229): transition only here --
            // its OUTPUT (o_pipeline_gate) was already computed in
            // compute_outputs() from the PRE-tick stall_state_, matching real
            // RTL's stalls_output_logic reading stall_state_q not stall_state_d. ---
            StallState next_stall = stall_state_;
            if (cached_stall_state_reset_ || in.soft_reset)
            {
                next_stall = StallState::ARRAY_STALL;
            }
            else
            {
                switch (stall_state_)
                {
                case StallState::ARRAY_ACTIVE:
                    if (cached_force_stall_ || (in.cdone && !cached_computation_ready_))
                        next_stall = StallState::ARRAY_STALL;
                    break;
                case StallState::ARRAY_STALL:
                    if (cached_computation_ready_)
                        next_stall = StallState::ARRAY_ACTIVE;
                    break;
                }
            }

            // ---------- Phase 2: commit ----------
            main_state_ = next_state;
            stall_state_ = next_stall;
            // outbuf_done_q_ (RTL "hold_reg", lines 124-139): i_soft_reset takes
            // priority (forces 0) over the normal set/clear priority-if-chain
            // below, matching RTL's own `if (i_soft_reset) outbuf_done_q<=0;
            // else begin if(i_outbuf_done) <=1; else if(o_outbuf_start||
            // o_outbuf_reset) <=0; end` structure exactly.
            if (in.soft_reset)
            {
                outbuf_done_q_ = false;
            }
            else if (in.outbuf_done)
            {
                outbuf_done_q_ = true;
            }
            else if (cached_outbuf_start_ || cached_outbuf_reset_)
            {
                outbuf_done_q_ = false;
            }
            outbuf_enable_q_ = cached_outbuf_enable_d_;
        }

        // Convenience wrapper for standalone use (e.g. unit tests) where
        // in.cdone/in.cswitch_done are already known upfront -- combines both
        // phases using the SAME Inputs. The real wiring layer (main_controller.h)
        // does NOT use this; it calls compute_outputs()/commit() separately
        // with a sauria::ContextSwitchController::tick() in between (see
        //).
        Outputs tick(const Inputs &in)
        {
            Outputs out = compute_outputs(in);
            commit(in);
            return out;
        }

        // Debug hook (read-only): outbuf_done_hold, field 18 of the RTL tape. A combinational value of this cycle (RTL
        // `assign`), so no pre-tick variant is needed -- same semantics as cscnt_flag.
        bool peek_outbuf_done_hold() const { return cached_outbuf_done_hold_; }

        // Debug hook (read-only): main_state_ as int, to compare with enum MainState (e.g. ARRAY_PREP).
        int dbg_main_state() const { return static_cast<int>(main_state_); }

    private:
        MainState main_state_{MainState::IDLE};
        StallState stall_state_{StallState::ARRAY_STALL};
        bool outbuf_done_q_{false};
        bool outbuf_enable_q_{false};

        // Cached between compute_outputs() and commit() (see split rationale
        // in the class-level header comment and).
        bool cached_computation_ready_{false};
        bool cached_outbuf_enable_d_{false};
        bool cached_stall_state_reset_{false};
        bool cached_force_stall_{false};
        bool cached_outbuf_done_hold_{false};
        bool cached_outbuf_start_{false};
        bool cached_outbuf_reset_{false};
    };

} // namespace sauria_rtl
