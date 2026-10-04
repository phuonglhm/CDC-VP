#pragma once
//
// 1:1 copy of sauria_model's control/feeders_fsm.h, itself a 1:1 port of RTL
// sauria_core/control/feeders_fsm.sv.
//
// ONLY CHANGE from the reference file: namespace `sauria` -> `sauria_rtl` (see
// rtl_ref_context_switch_controller.h's header comment for why). Logic below is otherwise
// byte-for-byte identical to the reference file.
//
// Reference header (functional notes):
//
// The THIRD of the three sibling modules instantiated by main_controller.sv (with context_fsm.sv and
// context_switch_controller.sv). It closes the RTL causal chain of the data feed:
//
//     feeders_fsm -> o_act_pop_en/o_wei_pop_en -> context_switch_controller's
//     incnt counter -> o_cdone -> context_fsm -> cswitch
//
// so the context boundary and the data feed run on the same (RTL) timing.
//
// Outputs owned by this module:
//   1. o_pipeline_en -- RTL gates it on `pipeline_en` (a per-state signal, LOW
//      until FIFO_FILL) AND stalls it whenever either feeder FIFO is empty.
//      This feeds BACK into both other
//      modules (main_controller.sv lines 132/160), so it is not a leaf signal.
//   2. o_act_pop_en / o_wei_pop_en -- RTL holds these LOW through
//      CNT_START/DATA_START/FIFO_FILL_WAIT (the real FIFO warm-up), then
//      `act_pop_en & i_pop_gate`.
//   3. o_act_finalctx / o_wei_cswitch -- driven from the repetition counters'
//      overflow flags.
//   4. o_feeders_done -- ContextFsm's i_feeders_done.
//
// TWO-PHASE tick(): same convention as context_fsm.h / context_switch_
// controller.h / ifmap_idxcnt.h -- compute_outputs() derives every
// combinational value from PRE-tick register state, commit() applies the
// clock edge. The split is REQUIRED here (not just stylistic): this module's
// o_pipeline_en is an INPUT to context_switch_controller and to context_fsm's
// own commit(), so the wiring layer must settle combinationally across all
// three modules before committing any of them. Verified safe: context_fsm.h's
// compute_outputs() does NOT read in.pipeline_en (only its commit() does, at
// context_fsm.h:681), and its out.pipeline_gate is a pure registered-state
// function (context_fsm.h:625) -- so there is no combinational loop.
//
// Settle order the wiring layer must use:
//     ctx.compute_outputs() -> feeders.compute_outputs() -> cs_ctrl.tick()
//     -> ctx.commit() -> feeders.commit()
//
// NOTE on i_act_done: declared as a port in the RTL (line 51) but genuinely
// UNUSED inside it -- the activation repetition counter keys off
// i_act_til_done, not i_act_done (RTL line 203). Kept in Inputs for interface
// fidelity and marked [UNUSED], same convention the rest of this port uses.
//
#include <cstdint>
#ifdef FX1_A3_FSM_PROBE
#include <fstream>
#endif

namespace sauria_rtl
{

    template <int X_DIM = 32,
              int Y_DIM = 32,
              int ACT_FIFO_POSITIONS = 8,
              int WEI_FIFO_POSITIONS = 8>
    class FeedersFsm
    {
    public:
        // RTL enum, same order => same numeric o_feed_status codes (0..13).
        enum State
        {
            IDLE = 0,
            CNT_START,
            DATA_START,
            FIFO_FILL_WAIT,
            FIFO_FILL,
            FINAL_PUSH_EARLY,
            FEEDING,
            ACT_FINISHED,
            WEI_FINISHED,
            FINAL_PUSH_BOTH,
            FINAL_PUSH_WAIT,
            EMPTY_WAIT,
            FIFO_EMPTYING,
            FINISH
        };

        // --- RTL localparams (lines 121-133) ---
        static constexpr int FIFO_FILL_CYCLES = 1;
        static constexpr int FINAL_PUSH_PRE_WEI = 3;
        static constexpr int FINAL_PUSH_PRE_ACT = 3; // Must be >= FINAL_PUSH_PRE_WEI
        static constexpr int FINAL_PUSH_LATENCY = 4;

        static constexpr int cmax2(int a, int b) { return (a > b) ? a : b; }

        static constexpr int FIFO_MAX_POS =
            cmax2(cmax2(cmax2(ACT_FIFO_POSITIONS, WEI_FIFO_POSITIONS),
                        cmax2(FINAL_PUSH_PRE_ACT, FINAL_PUSH_LATENCY)),
                  FIFO_FILL_CYCLES);
        static constexpr int PROP_MAX_POS = cmax2(X_DIM, Y_DIM);

        // RTL: FIFO_CNT_BITS = $clog2(FIFO_MAX_POS + PROP_MAX_POS). Modeled as a
        // wrap mask so the counters truncate exactly like the RTL vectors do.
        static constexpr int clog2c(int n)
        {
            int b = 0;
            while ((1 << b) < n)
                b++;
            return b;
        }
        static constexpr int FIFO_CNT_BITS = clog2c(FIFO_MAX_POS + PROP_MAX_POS);
        static constexpr uint32_t CNT_MASK =
            (FIFO_CNT_BITS >= 32) ? 0xFFFFFFFFu : ((1u << FIFO_CNT_BITS) - 1u);

        struct Inputs
        {
            bool pipeline_gate = false; // i_pipeline_gate (ContextFsm o_pipeline_gate)
            bool feeders_start = false; // i_feeders_start (ContextFsm o_feeders_start)
            bool fsm_reset = false;     // i_fsm_reset (feeders_reset | soft_reset)
            bool pop_gate = false;      // i_pop_gate (ContextFsm o_pop_gate)

            uint32_t act_reps = 1; // i_act_reps
            uint32_t wei_reps = 1; // i_wei_reps

            bool act_done = false; // i_act_done  [UNUSED in RTL -- see header note]
            bool act_til_done = false;
            bool act_fifo_empty = false;
            bool act_fifo_full = false;
            // SAME-CYCLE copy, only used at the FINAL_PUSH gates (RTL :456/:499/:783).
            // Guard off: main_controller assigns act_fifo_full itself.
            bool act_fifo_full_now = false;
            bool act_stall = false;
#ifdef FX1_A3_TILDONE_Q_GATE
            // SAME-cycle full/stall of the ifmap feeder (RTL ifmap_feeder.sv:350-351
            // assign o_fifo_full = fifo_full_any; o_feeder_stall = stall_any), used
            // only for the act til-done shim enable (RTL feeders_fsm.sv:288/304).
            bool act_fifo_full_peek = false;
            bool act_stall_peek = false;
#endif

            bool wei_done = false;
            bool wei_til_done = false;
            bool wei_fifo_empty = false;
            bool wei_fifo_full = false;
            bool wei_fifo_full_now = false;
            bool wei_stall = false;
        };

        struct Outputs
        {
            // Activation feeder controls
            bool act_feeder_en = false;
            bool act_feeder_clear = true;
            bool act_start = false;
            bool act_valid = false;
            bool act_finalpush = false;
            bool act_cnt_en = false;   // already ANDed with !act_cnt_hold_q
            bool act_cnt_clear = true;
            bool act_clearfifo = true;
            bool act_pop_en = false;   // already ANDed with i_pop_gate
            bool act_finalctx = false; // = act_ov_flag

            // Weight feeder controls
            bool wei_feeder_en = false;
            bool wei_feeder_clear = true;
            bool wei_start = false;
            bool wei_valid = false;
            bool wei_finalpush = false;
            bool wei_cnt_en = false;
            bool wei_cnt_clear = true;
            bool wei_clearfifo = true;
            bool wei_pop_en = false;
            bool wei_cswitch = false; // = wei_ov_flag

            // Global
            bool pipeline_en = false;  // o_pipeline_en -> array AND back into both siblings
            bool feeders_done = false; // o_feeders_done -> ContextFsm i_feeders_done
            uint32_t feed_status = 30; // o_feed_status
        };

        void reset()
        {
            main_state_q_ = IDLE;
            a_cnt_ = 0;
            b_cnt_ = 0;
            act_done_q_ = false;
            wei_done_q_ = false;
            act_rep_cnt_q_ = 0;
            wei_rep_cnt_q_ = 0;
            act_til_done_q_ = false;
            wei_til_done_q_ = false;
            act_ov_flag_shim_ = false;
            wei_ov_flag_shim_ = false;
            act_cnt_hold_q_ = false;
            wei_cnt_hold_q_ = false;
            pending_ = Pending{};
        }

        State state() const { return main_state_q_; }

        // ------------------------------------------------------------------
        // Phase 1 -- pure combinational, from PRE-tick register state only.
        // ------------------------------------------------------------------
        Outputs compute_outputs(const Inputs &in)
        {
            Outputs out;
            Pending p;

            // ---- Internal (pre-gating) signals, RTL lines 148-156 ----
            bool pipeline_en = false;
            bool act_pop_en_raw = false, wei_pop_en_raw = false;
            bool act_cnt_en_raw = false, wei_cnt_en_raw = false;
            bool pre_feeding_flag = true;
            bool a_cnt_en = false, a_cnt_clear = true;
            bool b_cnt_en = false, b_cnt_clear = true;

            // ==================================================================
            // FSM Output Logic (RTL lines 545-1156)
            // Defaults first (RTL lines 548-573), then per-state overrides.
            // ==================================================================
            out.act_feeder_en = false;
            out.act_feeder_clear = true;
            out.act_start = false;
            out.act_finalpush = false;
            out.act_valid = false;
            out.act_cnt_clear = true;
            out.act_clearfifo = true;
            out.wei_feeder_en = false;
            out.wei_feeder_clear = true;
            out.wei_start = false;
            out.wei_finalpush = false;
            out.wei_valid = false;
            out.wei_cnt_clear = true;
            out.wei_clearfifo = true;
            out.feeders_done = false;
            out.feed_status = 30;

            switch (main_state_q_)
            {
            case IDLE: // RTL 578-610
                out.feed_status = 0;
                break;

            case CNT_START: // RTL 613-645
                out.act_feeder_en = true;
                out.act_feeder_clear = false;
                act_cnt_en_raw = true;
                out.act_cnt_clear = false;
                out.act_clearfifo = false;
                out.wei_feeder_en = true;
                out.wei_feeder_clear = false;
                wei_cnt_en_raw = true;
                out.wei_cnt_clear = false;
                out.wei_clearfifo = false;
                out.feed_status = 1;
                break;

            case DATA_START: // RTL 648-680
                out.act_feeder_en = true;
                out.act_feeder_clear = false;
                out.act_start = true;
                act_cnt_en_raw = true;
                out.act_cnt_clear = false;
                out.act_clearfifo = false;
                out.wei_feeder_en = true;
                out.wei_feeder_clear = false;
                out.wei_start = true;
                wei_cnt_en_raw = true;
                out.wei_cnt_clear = false;
                out.wei_clearfifo = false;
                out.feed_status = 2;
                break;

            case FIFO_FILL_WAIT: // RTL 683-715
                out.act_feeder_en = true;
                out.act_feeder_clear = false;
                act_cnt_en_raw = true;
                out.act_valid = true;
                out.act_cnt_clear = false;
                out.act_clearfifo = false;
                out.wei_feeder_en = true;
                out.wei_feeder_clear = false;
                wei_cnt_en_raw = true;
                out.wei_valid = true;
                out.wei_cnt_clear = false;
                out.wei_clearfifo = false;
                out.feed_status = 3;
                break;

            case FIFO_FILL: // RTL 718-764
                out.act_feeder_en = true;
                out.act_feeder_clear = false;
                act_cnt_en_raw = true;
                out.act_valid = true;
                out.act_cnt_clear = false;
                out.act_clearfifo = false;
                act_pop_en_raw = in.pop_gate; // combinational transition (RTL 727)

                out.wei_feeder_en = true;
                out.wei_feeder_clear = false;
                wei_cnt_en_raw = true;
                out.wei_valid = true;
                out.wei_cnt_clear = false;
                out.wei_clearfifo = false;
                wei_pop_en_raw = in.pop_gate; // RTL 737

                pipeline_en = true;

                if (a_cnt_ == (uint32_t)(FIFO_FILL_CYCLES - 1))
                {
                    a_cnt_en = false;
                    a_cnt_clear = in.pop_gate ? true : false; // RTL 742-751
                }
                else
                {
                    a_cnt_en = true;
                    a_cnt_clear = false;
                }
                out.feed_status = 4;
                break;

            case FINAL_PUSH_EARLY: // RTL 767-819
                out.act_feeder_en = true;
                out.act_feeder_clear = false;
                out.act_valid = false;

                a_cnt_en = true;
                a_cnt_clear = false;

                if (a_cnt_ == (uint32_t)cmax2(FINAL_PUSH_PRE_WEI, FINAL_PUSH_PRE_ACT))
                {
                    a_cnt_en = false;
                    if (!in.act_fifo_full_now && !in.wei_fifo_full_now && !in.act_stall && !in.wei_stall)
                        a_cnt_clear = true;
                }
                if ((a_cnt_ == (uint32_t)FINAL_PUSH_PRE_ACT) && !in.act_fifo_full_now && !in.act_stall)
                    out.act_finalpush = true;
                if ((a_cnt_ == (uint32_t)FINAL_PUSH_PRE_WEI) && !in.wei_fifo_full_now && !in.wei_stall)
                    out.wei_finalpush = true;

                out.act_cnt_clear = false;
                out.act_clearfifo = false;
                act_pop_en_raw = true;

                out.wei_feeder_en = true;
                out.wei_feeder_clear = false;
                out.wei_valid = false;
                out.wei_cnt_clear = false;
                out.wei_clearfifo = false;
                wei_pop_en_raw = true;

                pipeline_en = true;
                out.feeders_done = true;
                pre_feeding_flag = false;
                out.feed_status = 5;
                break;

            case FEEDING: // RTL 822-854
                out.act_feeder_en = true;
                out.act_feeder_clear = false;
                act_cnt_en_raw = true;
                out.act_valid = true;
                out.act_cnt_clear = false;
                out.act_clearfifo = false;
                act_pop_en_raw = true;

                out.wei_feeder_en = true;
                out.wei_feeder_clear = false;
                wei_cnt_en_raw = true;
                out.wei_valid = true;
                out.wei_cnt_clear = false;
                out.wei_clearfifo = false;
                wei_pop_en_raw = true;

                pipeline_en = true;
                pre_feeding_flag = false;
                out.feed_status = 6;
                break;

            case ACT_FINISHED: // RTL 857-889 -- act counter off, weight still running
                out.act_feeder_en = true;
                out.act_feeder_clear = false;
                act_cnt_en_raw = false;
                out.act_valid = true;
                out.act_cnt_clear = false;
                out.act_clearfifo = false;
                act_pop_en_raw = true;

                out.wei_feeder_en = true;
                out.wei_feeder_clear = false;
                wei_cnt_en_raw = true;
                out.wei_valid = true;
                out.wei_cnt_clear = false;
                out.wei_clearfifo = false;
                wei_pop_en_raw = true;

                pipeline_en = true;
                pre_feeding_flag = false;
                out.feed_status = 7;
                break;

            case WEI_FINISHED: // RTL 892-924
                out.act_feeder_en = true;
                out.act_feeder_clear = false;
                act_cnt_en_raw = true;
                out.act_valid = true;
                out.act_cnt_clear = false;
                out.act_clearfifo = false;
                act_pop_en_raw = true;

                out.wei_feeder_en = true;
                out.wei_feeder_clear = false;
                wei_cnt_en_raw = false;
                out.wei_valid = true;
                out.wei_cnt_clear = false;
                out.wei_clearfifo = false;
                wei_pop_en_raw = true;

                pipeline_en = true;
                pre_feeding_flag = false;
                out.feed_status = 8;
                break;

            case FINAL_PUSH_BOTH: // RTL 927-978 -- same as EARLY but valid=1
                out.act_feeder_en = true;
                out.act_feeder_clear = false;
                out.act_valid = true;

                a_cnt_en = true;
                a_cnt_clear = false;

                if (a_cnt_ == (uint32_t)cmax2(FINAL_PUSH_PRE_WEI, FINAL_PUSH_PRE_ACT))
                {
                    a_cnt_en = false;
                    if (!in.act_fifo_full_now && !in.wei_fifo_full_now && !in.act_stall && !in.wei_stall)
                        a_cnt_clear = true;
                }
                if ((a_cnt_ == (uint32_t)FINAL_PUSH_PRE_ACT) && !in.act_fifo_full_now && !in.act_stall)
                    out.act_finalpush = true;
                if ((a_cnt_ == (uint32_t)FINAL_PUSH_PRE_WEI) && !in.wei_fifo_full_now && !in.wei_stall)
                    out.wei_finalpush = true;

                out.act_cnt_clear = false;
                out.act_clearfifo = false;
                act_pop_en_raw = true;

                out.wei_feeder_en = true;
                out.wei_feeder_clear = false;
                out.wei_valid = true;
                out.wei_cnt_clear = false;
                out.wei_clearfifo = false;
                wei_pop_en_raw = true;

                pipeline_en = true;
                out.feeders_done = true;
                pre_feeding_flag = false;
                out.feed_status = 9;
                break;

            case FINAL_PUSH_WAIT: // RTL 982-1014
                out.act_feeder_en = true;
                out.act_feeder_clear = false;
                out.act_valid = false;
                out.act_cnt_clear = false;
                out.act_clearfifo = false;
                act_pop_en_raw = true;

                out.wei_feeder_en = true;
                out.wei_feeder_clear = false;
                out.wei_valid = false;
                out.wei_cnt_clear = false;
                out.wei_clearfifo = false;
                wei_pop_en_raw = true;

                pipeline_en = true;
                out.feeders_done = true;

                a_cnt_en = true;
                a_cnt_clear = false;

                pre_feeding_flag = false;
                out.feed_status = 10;
                break;

            case EMPTY_WAIT: // RTL 1017-1049
                out.act_feeder_en = true;
                out.act_feeder_clear = false;
                out.act_cnt_clear = false;
                out.act_clearfifo = false;
                act_pop_en_raw = true;

                out.wei_feeder_en = true;
                out.wei_feeder_clear = false;
                out.wei_cnt_clear = false;
                out.wei_clearfifo = false;
                wei_pop_en_raw = true;

                pipeline_en = true;
                out.feeders_done = true;
                pre_feeding_flag = false;
                out.feed_status = 11;
                break;

            case FIFO_EMPTYING: // RTL 1052-1084 -- pops OFF, b_cnt runs
                out.act_feeder_en = true;
                out.act_feeder_clear = false;
                out.act_cnt_clear = false;
                out.act_clearfifo = false;

                out.wei_feeder_en = true;
                out.wei_feeder_clear = false;
                out.wei_cnt_clear = false;
                out.wei_clearfifo = false;

                pipeline_en = true;
                out.feeders_done = true;

                b_cnt_en = true;
                b_cnt_clear = false;

                pre_feeding_flag = false;
                out.feed_status = 12;
                break;

            case FINISH: // RTL 1087-1119
                out.act_feeder_en = true;
                out.act_feeder_clear = true;
                out.act_cnt_clear = true;
                out.act_clearfifo = true;

                out.wei_feeder_en = true;
                out.wei_feeder_clear = true;
                out.wei_cnt_clear = true;
                out.wei_clearfifo = true;

                pipeline_en = true;
                out.feeders_done = true;
                pre_feeding_flag = false;
                out.feed_status = 13;
                break;

            default: // RTL 1122-1153 -- note clears are 0 here, unlike the top defaults
                out.act_feeder_clear = false;
                out.act_cnt_clear = false;
                out.act_clearfifo = false;
                out.wei_feeder_clear = false;
                out.wei_cnt_clear = false;
                out.wei_clearfifo = false;
                pre_feeding_flag = true;
                out.feed_status = 31;
                break;
            }

            // ==================================================================
            // Repetition counters (RTL 200-244) -- combinational next state.
            // NOTE the asymmetry, faithful to RTL line 203/204: the ACTIVATION
            // edge keys off i_act_til_done while the WEIGHT edge keys off
            // i_wei_done (NOT i_wei_til_done).
            // ==================================================================
            bool act_done_edge = in.act_til_done;
            bool wei_done_edge = in.wei_done;

            uint32_t act_reps = (in.act_reps == 0) ? 1u : in.act_reps;
            uint32_t wei_reps = (in.wei_reps == 0) ? 1u : in.wei_reps;

            p.act_rep_cnt_d = act_rep_cnt_q_;
            p.wei_rep_cnt_d = wei_rep_cnt_q_;
            bool act_ov_flag = false, wei_ov_flag = false;

            if (act_rep_cnt_q_ == (act_reps - 1))
            {
                act_ov_flag = true;
                if (act_done_edge)
                    p.act_rep_cnt_d = 0;
            }
            else if (act_done_edge)
            {
                p.act_rep_cnt_d = act_rep_cnt_q_ + 1;
            }

            if (wei_rep_cnt_q_ == (wei_reps - 1))
            {
                wei_ov_flag = true;
                if (wei_done_edge)
                    p.wei_rep_cnt_d = 0;
            }
            else if (wei_done_edge)
            {
                p.wei_rep_cnt_d = wei_rep_cnt_q_ + 1;
            }

            out.wei_cswitch = wei_ov_flag;  // RTL 269
            out.act_finalctx = act_ov_flag; // RTL 270

            // ==================================================================
            // Gated counter / pop enables (RTL 186-194).
            // These must be computed BEFORE the til-done shim, which reads the
            // ALREADY-GATED o_act_cnt_en / o_act_cnt_clear (RTL 288/297/304/305).
            // ==================================================================
            out.act_cnt_en = act_cnt_en_raw && !act_cnt_hold_q_;
            out.wei_cnt_en = wei_cnt_en_raw && !wei_cnt_hold_q_;
            out.act_pop_en = act_pop_en_raw && in.pop_gate;
            out.wei_pop_en = wei_pop_en_raw && in.pop_gate;

            // ==================================================================
            // Tile-done shimming (RTL 276-305) -- SRAM latency equalization.
            // Register next-state, gated exactly like the feeders' own counters.
            // ==================================================================
#ifdef FX1_A3_TILDONE_Q_GATE
            bool act_shim_en = out.act_cnt_en && !in.act_fifo_full_peek && !in.act_stall_peek;
#else
            bool act_shim_en = out.act_cnt_en && !in.act_fifo_full && !in.act_stall;
#endif
            bool wei_shim_en = out.wei_cnt_en && !in.wei_fifo_full && !in.wei_stall;

            p.act_til_done_q_d = act_til_done_q_;
            p.act_ov_flag_shim_d = act_ov_flag_shim_;
            if (out.act_cnt_clear)
            {
                p.act_til_done_q_d = false;
                p.act_ov_flag_shim_d = false;
            }
            else if (act_shim_en)
            {
                p.act_til_done_q_d = in.act_til_done;
                p.act_ov_flag_shim_d = act_ov_flag;
            }

            p.wei_til_done_q_d = wei_til_done_q_;
            p.wei_ov_flag_shim_d = wei_ov_flag_shim_;
            if (out.wei_cnt_clear)
            {
                p.wei_til_done_q_d = false;
                p.wei_ov_flag_shim_d = false;
            }
            else if (wei_shim_en)
            {
                p.wei_til_done_q_d = in.wei_til_done;
                p.wei_ov_flag_shim_d = wei_ov_flag;
            }

            // Combinational shim outputs (RTL 304-305)
            bool act_til_done_shim = act_til_done_q_ && act_shim_en;
            bool wei_til_done_shim = wei_til_done_q_ && wei_shim_en;

            // "Feeder finished" composite used by both the hold flag and the
            // FEEDING-exit transitions (RTL 318/331/465/469/473/482/491).
#ifdef FX1_A3_ACT_FIN_ONE_REP
            // symmetric to FX1_A3_WEI_FIN_ONE_REP. ContextFsm rolls the contexts OUTSIDE the feeder, so `act_reps` passes
            // would be counted twice; with act_reps = 1 this reduces exactly to the default expression.
            bool act_fin = act_til_done_shim;
#else
            bool act_fin = act_til_done_shim && act_ov_flag_shim_;
#endif
            dbg_act_fin_ = act_fin; // (observer only)
#ifdef FX1_A3_WEI_FIN_ONE_REP
            // finish after ONE sweep. wei_reps = 8 makes the FSM wait for 8 sweeps because the RTL rolls 8 contexts INSIDE
            // the feeder; when ContextFsm rolls the contexts outside (one arm per context), those sweeps would be counted
            // twice. Only the FINISH condition changes; wei_cswitch (til_k gate) is unchanged.
            bool wei_fin = wei_til_done_shim;
#else
            bool wei_fin = wei_til_done_shim && wei_ov_flag_shim_;
#endif

#ifdef FX1_A3_FSM_PROBE
            // Debug hook (default off): verbatim port of sauria_model's probe block (control/feeders_fsm.h:675-696) plus 7
            // read-only WEI-path columns from this tick(). A reference tree needs the same 7 columns for a tape-to-tape
            // comparison.
            {
                static std::ofstream fpr("trace_sysc/fsm.csv");
                static bool fpr_hdr = false;
                static unsigned long long fpr_n = 0;
                if (!fpr_hdr)
                {
                    fpr << "n,main_state_q,act_hold_q,wei_hold_q,act_fin,wei_fin,"
                           "act_fifo_empty,wei_fifo_empty,"
                           "wei_cnt_en,wei_full,wei_stall,wei_tdq,wei_shim_en,"
                           "wei_tds,wei_ov\n";
                    fpr_hdr = true;
                }
                fpr << fpr_n << "," << (int)main_state_q_ << ","
                    << (int)act_cnt_hold_q_ << "," << (int)wei_cnt_hold_q_ << ","
                    << (int)act_fin << "," << (int)wei_fin << ","
                    << (int)in.act_fifo_empty << "," << (int)in.wei_fifo_empty << ","
                    << (int)out.wei_cnt_en << "," << (int)in.wei_fifo_full << ","
                    << (int)in.wei_stall << "," << (int)wei_til_done_q_ << ","
                    << (int)wei_shim_en << "," << (int)wei_til_done_shim << ","
                    << (int)wei_ov_flag_shim_ << "\n";
                fpr_n++;
            }
#endif

            // ==================================================================
            // Counter hold flag (RTL 311-342)
            // ==================================================================
            p.act_cnt_hold_d = act_cnt_hold_q_;
            p.wei_cnt_hold_d = wei_cnt_hold_q_;

            if (!act_cnt_hold_q_)
            {
                if (pre_feeding_flag && act_fin)
                    p.act_cnt_hold_d = true;
            }
            else if (main_state_q_ == FINAL_PUSH_BOTH)
            {
                p.act_cnt_hold_d = false;
            }

            if (!wei_cnt_hold_q_)
            {
                if (pre_feeding_flag && wei_fin)
                    p.wei_cnt_hold_d = true;
            }
            else if (main_state_q_ == FINAL_PUSH_BOTH)
            {
                p.wei_cnt_hold_d = false;
            }

            // ==================================================================
            // o_pipeline_en (RTL 162-180). Three distinct branches -- note the
            // RTL's own two comments here are copy-paste duplicates of each
            // other; the CODE is what is ported.
            // ==================================================================
            if (main_state_q_ == EMPTY_WAIT || main_state_q_ == FIFO_EMPTYING ||
                main_state_q_ == FINISH)
            {
                out.pipeline_en = in.pipeline_gate && pipeline_en;
            }
            else if (main_state_q_ == FINAL_PUSH_BOTH || main_state_q_ == FINAL_PUSH_WAIT)
            {
                out.pipeline_en = in.pipeline_gate && pipeline_en &&
                                  !(in.act_fifo_empty || in.wei_fifo_empty);
            }
            else
            {
                out.pipeline_en = in.pipeline_gate && pipeline_en &&
                                  !((in.act_fifo_empty && !act_cnt_hold_q_) ||
                                    (in.wei_fifo_empty && !wei_cnt_hold_q_));
            }

            // ==================================================================
            // FSM transition logic (RTL 403-539)
            // ==================================================================
            State next = main_state_q_;
            if (in.fsm_reset)
            {
                next = IDLE;
            }
            else
            {
                switch (main_state_q_)
                {
                case IDLE:
                    if (in.feeders_start)
                        next = CNT_START;
                    break;

                case CNT_START:
                    next = DATA_START;
                    break;

                case DATA_START:
                    next = FIFO_FILL_WAIT;
                    break;

                case FIFO_FILL_WAIT: // RTL 435
                    if ((!in.act_fifo_empty || act_cnt_hold_q_) &&
                        (!in.wei_fifo_empty || wei_cnt_hold_q_))
                        next = FIFO_FILL;
                    break;

                case FIFO_FILL: // RTL 444-450
                    if (act_cnt_hold_q_ && wei_cnt_hold_q_)
                        next = FINAL_PUSH_EARLY;
                    else if (in.pop_gate && (a_cnt_ == (uint32_t)(FIFO_FILL_CYCLES - 1)))
                        next = FEEDING;
                    break;

                case FINAL_PUSH_EARLY: // RTL 455
                    if ((a_cnt_ == (uint32_t)cmax2(FINAL_PUSH_PRE_WEI, FINAL_PUSH_PRE_ACT)) &&
                        !in.act_fifo_full_now && !in.wei_fifo_full_now &&
                        !in.act_stall && !in.wei_stall)
                        next = FINAL_PUSH_WAIT;
                    break;

                case FEEDING: // RTL 465-475
                    if ((act_fin || act_cnt_hold_q_) && (wei_fin || wei_cnt_hold_q_))
                        next = FINAL_PUSH_BOTH;
                    else if (act_fin || act_cnt_hold_q_)
                        next = ACT_FINISHED;
                    else if (wei_fin || wei_cnt_hold_q_)
                        next = WEI_FINISHED;
                    break;

                case ACT_FINISHED: // RTL 482 -- note: bare wei_fin, no hold term
                    if (wei_fin)
                        next = FINAL_PUSH_BOTH;
                    break;

                case WEI_FINISHED: // RTL 491
                    if (act_fin)
                        next = FINAL_PUSH_BOTH;
                    break;

                case FINAL_PUSH_BOTH: // RTL 498
                    if ((a_cnt_ == (uint32_t)cmax2(FINAL_PUSH_PRE_WEI, FINAL_PUSH_PRE_ACT)) &&
                        !in.act_fifo_full_now && !in.wei_fifo_full_now &&
                        !in.act_stall && !in.wei_stall)
                        next = FINAL_PUSH_WAIT;
                    break;

                case FINAL_PUSH_WAIT: // RTL 506
                    if (a_cnt_ == (uint32_t)(FINAL_PUSH_LATENCY - 1))
                        next = EMPTY_WAIT;
                    break;

                case EMPTY_WAIT: // RTL 513
                    if (in.act_fifo_empty && in.wei_fifo_empty &&
                        in.pop_gate && in.pipeline_gate)
                        next = FIFO_EMPTYING;
                    break;

                case FIFO_EMPTYING: // RTL 520
                    if (b_cnt_ == (uint32_t)(PROP_MAX_POS + FIFO_MAX_POS - 1))
                        next = FINISH;
                    break;

                case FINISH: // RTL 527 -- fsm_reset already handled above
                    break;

                default:
                    next = IDLE;
                    break;
                }
            }

            p.next_state = next;
            p.a_cnt_en = a_cnt_en;
            p.a_cnt_clear = a_cnt_clear;
            p.b_cnt_en = b_cnt_en;
            p.b_cnt_clear = b_cnt_clear;
            p.pipeline_gate = in.pipeline_gate;
            p.pop_gate = in.pop_gate;
            p.fsm_reset = in.fsm_reset;
            p.act_til_done_in = in.act_til_done;
            p.wei_done_in = in.wei_done;
            p.valid = true;
            pending_ = p;

#ifdef FX1_A3_WEI_SEQ_PROBE
            // Debug hook (default off): same probe as sauria_model (feeders_fsm.h:854/998; variables wseq_*, inserted right
            // after pending_ = p;), so files compare line by line. 11 WEI columns, then 4 ACT columns at the END
            // (act_fifo_empty, act_cnt_en, act_shim_en, pipeline_en + pop_gate).
            if (in.feeders_start && !probe_seen_start_)
            {
                probe_seen_start_ = true;
                probe_seq_ = 0;
            }
            else if (probe_seen_start_)
            {
                probe_seq_++;
            }
            if (probe_seen_start_)
            {
                static std::ofstream wsq("trace_sysc/wei_seq_probe.csv");
                static bool wsq_hdr = false;
                if (!wsq_hdr)
                {
                    wsq << "seq,tsim,wei_fifo_full,wei_stall,wei_cnt_en,wei_shim_en,"
                           "wei_til_done_q,wei_ov_flag_shim,wei_fin,pre_feeding_flag,"
                           "wei_cnt_hold_d,act_fifo_empty,act_cnt_en,act_shim_en,"
                           "pipeline_en,pop_gate" << std::endl;
                    wsq_hdr = true;
                }
                wsq << probe_seq_ << "," << sc_core::sc_time_stamp().value() << ","
                    << (int)in.wei_fifo_full << "," << (int)in.wei_stall << ","
                    << (int)out.wei_cnt_en << "," << (int)wei_shim_en << ","
                    << (int)wei_til_done_q_ << "," << (int)wei_ov_flag_shim_ << ","
                    << (int)wei_fin << "," << (int)pre_feeding_flag << ","
                    << (int)p.wei_cnt_hold_d << "," << (int)in.act_fifo_empty << ","
                    << (int)out.act_cnt_en << "," << (int)act_shim_en << ","
                    << (int)out.pipeline_en << "," << (int)in.pop_gate << std::endl;
            }
#endif

            return out;
        }

        // ------------------------------------------------------------------
        // Phase 2 -- apply the clock edge using values cached by
        // compute_outputs(). Must be called exactly once per compute_outputs().
        // ------------------------------------------------------------------
        void commit()
        {
            if (!pending_.valid)
                return;
            const Pending &p = pending_;

            // --- State register (RTL 391-397) ---
            main_state_q_ = p.next_state;

            // --- Multipurpose counters (RTL 366-385). No fsm_reset here: the
            // output logic drives both clears high in IDLE instead. ---
            if (p.a_cnt_clear)
                a_cnt_ = 0;
            else if (p.a_cnt_en)
                a_cnt_ = (a_cnt_ + 1) & CNT_MASK;

            if (p.b_cnt_clear)
                b_cnt_ = 0;
            else if (p.b_cnt_en && p.pipeline_gate && p.pop_gate)
                b_cnt_ = (b_cnt_ + 1) & CNT_MASK;

            // --- Repetition counters (RTL 246-267). Free-running: no
            // pipeline gating, only the synchronous fsm_reset. ---
            if (p.fsm_reset)
            {
                act_done_q_ = false;
                wei_done_q_ = false;
                act_rep_cnt_q_ = 0;
                wei_rep_cnt_q_ = 0;
            }
            else
            {
                act_done_q_ = p.act_til_done_in;
                wei_done_q_ = p.wei_done_in;
                act_rep_cnt_q_ = p.act_rep_cnt_d;
                wei_rep_cnt_q_ = p.wei_rep_cnt_d;
            }

            // --- Til-done shim registers (RTL 276-302). NOT reset by
            // fsm_reset -- only by the cnt_clear path folded into the _d
            // values during compute_outputs(). ---
            act_til_done_q_ = p.act_til_done_q_d;
            act_ov_flag_shim_ = p.act_ov_flag_shim_d;
            wei_til_done_q_ = p.wei_til_done_q_d;
            wei_ov_flag_shim_ = p.wei_ov_flag_shim_d;

            // --- Counter hold flags (RTL 345-360) ---
            if (p.fsm_reset)
            {
                act_cnt_hold_q_ = false;
                wei_cnt_hold_q_ = false;
            }
            else
            {
                act_cnt_hold_q_ = p.act_cnt_hold_d;
                wei_cnt_hold_q_ = p.wei_cnt_hold_d;
            }

            pending_.valid = false;
        }

        // Convenience single-shot form. The wiring layer does NOT use this --
        // it needs the split so o_pipeline_en can settle across all three
        // sibling modules before any of them commits.
        Outputs tick(const Inputs &in)
        {
            Outputs out = compute_outputs(in);
            commit();
            return out;
        }

        // read-only observers. Additive; no logic change.
        int dbg_state() const { return (int)main_state_q_; }
        bool dbg_act_hold() const { return act_cnt_hold_q_; }
        bool dbg_wei_hold() const { return wei_cnt_hold_q_; }
        bool dbg_act_fin() const { return dbg_act_fin_; }
        // Debug hooks: repetition counters + overflow flags (read-only).
        uint32_t dbg_act_rep() const { return act_rep_cnt_q_; }
        uint32_t dbg_wei_rep() const { return wei_rep_cnt_q_; }
        bool dbg_act_ov() const { return act_ov_flag_shim_; }
        bool dbg_wei_ov() const { return wei_ov_flag_shim_; }

    private:
        bool dbg_act_fin_{false};

        struct Pending
        {
            bool valid = false;
            State next_state = IDLE;
            bool a_cnt_en = false, a_cnt_clear = true;
            bool b_cnt_en = false, b_cnt_clear = true;
            bool pipeline_gate = false, pop_gate = false, fsm_reset = false;
            uint32_t act_rep_cnt_d = 0, wei_rep_cnt_d = 0;
            bool act_til_done_in = false, wei_done_in = false;
            bool act_til_done_q_d = false, wei_til_done_q_d = false;
            bool act_ov_flag_shim_d = false, wei_ov_flag_shim_d = false;
            bool act_cnt_hold_d = false, wei_cnt_hold_d = false;
        };

        State main_state_q_{IDLE};
        uint32_t a_cnt_{0}, b_cnt_{0};
        bool act_done_q_{false}, wei_done_q_{false};
        uint32_t act_rep_cnt_q_{0}, wei_rep_cnt_q_{0};
        bool act_til_done_q_{false}, wei_til_done_q_{false};
        bool act_ov_flag_shim_{false}, wei_ov_flag_shim_{false};
        bool act_cnt_hold_q_{false}, wei_cnt_hold_q_{false};
        Pending pending_{};
#ifdef FX1_A3_WEI_SEQ_PROBE
        // Debug hook (default off): same macro name and columns as sauria_model's probe (feeders_fsm.h::compute_outputs(),
        // anchored at the first in.feeders_start == true), for a sequence-by-sequence comparison; 4 ACT columns at the END.
        bool probe_seen_start_{false};
        long long probe_seq_{0};
#endif
    };

} // namespace sauria_rtl
