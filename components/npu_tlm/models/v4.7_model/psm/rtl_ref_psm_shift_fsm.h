#pragma once
//
// 1:1 port of RTL sauria_core/psm/psm_shift_fsm.sv: the 12-state scan-out FSM. Its PREWRITE_SHIFT phase decides
// when each column's i_c_arr sample is captured (o_cscan_en's assertion pattern) into the ring buffer
// (psm_shift_register.h); psm_idxcnt.h then releases the captured words to SRAM-C. The FSM also sets the
// READING / WRITING state durations from i_done / i_til_done.
//
// The PSM data path around it (psm_shift_register.h, psm_wdata_manager.h, psm_rdata_manager.h) is ported too.
//
// Do not "clean up" or reinterpret without re-checking against the RTL source (port 1:1, don't re-derive).
//
// Two-phase tick(): same convention as the other ports (ifmap_idxcnt.h, feed_data_manager.h, context_fsm.h,
// context_switch_controller.h) -- Phase 1 computes every combinational value
// from pre-tick register state, Phase 2 commits, mirroring a single clock
// edge.
//
#include <cstdint>

// counts entries into RD_CNT_START per ctx_cnt (observation only)
inline unsigned long long g_fx1_rdstart[64] = {0};
inline unsigned long long g_fx1_rdstart_hi = 0;   // ctx_cnt >= 64

namespace sauria_rtl
{
using namespace sauria; // A1 port: shared vector/mask types stay in ::sauria, class bodies move to ::sauria_rtl.

    // X: array width (columns) -- governs how long PREREAD_SHIFT/PREWRITE_SHIFT
    // hold (scan_cnt reaches X-1). IDX_W: i_ncontexts/ctx_cnt width (RTL default 8).
    // PARAMS_W: i_inactive_cols width (RTL default 2).
    template <int X_DIM = 32, int IDX_W = 8, int PARAMS_W = 2>
    class PsmShiftFsm
    {
    public:
        enum class MainState
        {
            IDLE,
            PREREAD_SHIFT,
            RD_CNT_START,
            READING,
            READING_LAT_WAIT,
            POSTREAD_SHIFT,
            PREWRITE_SHIFT,
            WR_CNT_START,
            WRITING,
            WREN_HOLD_LAST,
            WRITING_LAT_WAIT,
            FINISH
        };

        static constexpr int WR_LAT = 1;
        static constexpr int RD_LAT = 3;

        struct Inputs
        {
            bool fsm_start = false;    // i_fsm_start
            bool fsm_reset = false;    // i_fsm_reset
            uint32_t ncontexts = 0;    // i_ncontexts
            bool preload_en = false;   // i_preload_en
            uint32_t inactive_cols = 0; // i_inactive_cols
            bool pipeline_en = false;  // i_pipeline_en
            bool done = false;         // i_done (from PsmIdxCnt::Outputs::done)
            bool til_done = false;     // i_til_done (from PsmIdxCnt::Outputs::til_done)

            // NOT an RTL signal -- deliberate, documented wiring-layer
            // simplification (user's explicit choice,).
            // Real RTL's IDLE state branches on ctx_cnt<3 to run a
            // preload-only warmup (no write) for the FSM's first 3
            // invocations, assuming a pipelined preload+drain dataflow (the
            // ctx_cnt==ncontexts+2/+3 offsets in o_finalwrite/completion_flag
            // imply a few EXTRA trailing invocations exist to flush a
            // multi-context pipeline). The pre-existing (guard-off) psm_top.h
            // implements NO such pipeline -- it writes every context
            // immediately and i_preload_en is unused/dead in that model. If
            // this class's REAL ctx_cnt<3 branching were wired literally with
            // preload_en=false, a single-context test (ctx_cnt never reaches
            // 3) would NEVER take the write path -- a silent, total loss of
            // output, strictly worse than the bug this port exists to fix.
            // Setting force_write_path=true makes IDLE always go straight to
            // PREWRITE_SHIFT (matching the old model's "write every context"
            // behavior) regardless of ctx_cnt -- bypassing the preload-warmup
            // branch entirely rather than trying to replicate its pipeline
            // semantics, which is out of scope for this fix.
            bool force_write_path = false;
        };

        struct Outputs
        {
            bool cnt_en = false;        // o_cnt_en
            bool cnt_clear = true;      // o_cnt_clear (= i_fsm_reset, RTL "assign")
            bool wr_flag = false;       // o_wr_flag
            bool cnt_start = false;     // o_cnt_start

            bool buff_clear = false;    // o_buff_clear (RTL "assign", see below)

            bool rd_feeder_en = false;  // o_rd_feeder_en
            bool rd_feeder_clear = true; // o_rd_feeder_clear

            bool wr_feeder_en = false;  // o_wr_feeder_en
            bool wr_feeder_clear = true; // o_wr_feeder_clear

            bool sramc_wren = false;    // o_sramc_wren
            bool sramc_rden = false;    // o_sramc_rden
            bool cscan_en = false;      // o_cscan_en -- THE signal that gates when
                                        // each column's i_c_arr sample is captured;
                                        // this is the one this whole port exists for.
            bool buff_shift = false;    // o_buff_shift

            uint32_t out_status = 30;   // o_out_status
            bool shift_done = false;    // o_shift_done
            bool done = false;          // o_done
            bool finalwrite = false;    // o_finalwrite (RTL "assign", see below)
        };

        // Debug hooks (read-only accessors, no effect on behaviour).
        uint32_t dbg_scan_cnt() const { return scan_cnt_; }
        uint32_t dbg_ctx_cnt() const { return ctx_cnt_; }
        uint32_t dbg_cyc_cnt() const { return cyc_cnt_; }
        // Debug hook: main_state_ for external tracing.
        int dbg_main_state() const { return (int)main_state_; }

        // shift_done / done as pure functions of main_state_ (RTL always_comb on main_state_q), so Control can read
        // them BEFORE Psm ticks.
        bool peek_shift_done() const
        {
            switch (main_state_)
            {
            case MainState::RD_CNT_START:
            case MainState::READING:
            case MainState::READING_LAT_WAIT:
            case MainState::WR_CNT_START:
            case MainState::WRITING:
            case MainState::WREN_HOLD_LAST:
            case MainState::WRITING_LAT_WAIT:
            case MainState::FINISH:
                return true;
            default:
                return false;
            }
        }
        bool peek_done() const { return main_state_ == MainState::FINISH; }

        void reset()
        {
            main_state_ = MainState::IDLE;
            cyc_cnt_ = 0;
            scan_cnt_ = 0;
            ctx_cnt_ = 0;
        }

        // Phase 1: output logic for the CURRENT (pre-tick) state. Does NOT
        // read in.done/in.til_done (matches RTL: output_logic's always_comb
        // never references i_done/i_til_done, only the transition logic
        // does). Split exists so the wiring layer can run a
        // sauria_rtl::PsmIdxCnt::tick() in between, using THIS call's cnt_en/
        // cnt_clear/wr_flag outputs, before feeding the real done/til_done
        // back in for commit() -- same chicken-and-egg reason as
        // context_fsm.h's own compute_outputs()/commit() split.
        Outputs compute_outputs(const Inputs &in)
        {
            // ---------- Phase 1: combinational (pre-tick register state) ----------

            // --- RTL "assign" lines (lines 131/185/191/197) ---
            const bool inactive_cols_flag = (in.inactive_cols != 0);
            Outputs out{};
            out.cnt_clear = in.fsm_reset; // ctx_cnt_clear = o_cnt_clear = i_fsm_reset
            out.buff_clear = (main_state_ == MainState::IDLE) &&
                              (!in.preload_en || (ctx_cnt_ > in.ncontexts));
            out.finalwrite = (ctx_cnt_ == (in.ncontexts + 2));
            const bool completion_flag = (ctx_cnt_ == (in.ncontexts + 3));
            cached_inactive_cols_flag_ = inactive_cols_flag;
            cached_completion_flag_ = completion_flag;

            // --- Output logic for the CURRENT (pre-tick) state (RTL lines
            // 342-713, "Output Logic" -- defaults-then-per-state-override,
            // ported as a switch that fully re-sets every field each branch). ---
            switch (main_state_)
            {
            case MainState::IDLE:
                out.cnt_en = false;
                out.wr_flag = false;
                out.cnt_start = false;
                out.rd_feeder_en = false;
                out.rd_feeder_clear = true;
                out.wr_feeder_en = false;
                out.wr_feeder_clear = true;
                out.done = false;
                out.shift_done = false;
                out.sramc_wren = false;
                out.sramc_rden = false;
                out.cscan_en = false;
                out.buff_shift = false;
                out.out_status = 0;
                break;

            case MainState::PREREAD_SHIFT:
                out.cnt_en = false;
                out.wr_flag = false;
                out.cnt_start = false;
                out.rd_feeder_en = false;
                out.rd_feeder_clear = true;
                out.wr_feeder_en = false;
                out.wr_feeder_clear = true;
                out.done = false;
                out.shift_done = false;
                out.sramc_wren = false;
                out.sramc_rden = false;
                out.cscan_en = true;
                out.buff_shift = false;
                out.out_status = 1;
                break;

            case MainState::RD_CNT_START:
                out.cnt_en = true;
                out.wr_flag = false;
                out.cnt_start = true;
                out.rd_feeder_en = true;
                out.rd_feeder_clear = false;
                out.wr_feeder_en = false;
                out.wr_feeder_clear = true;
                out.done = false;
                out.shift_done = true;
                out.sramc_wren = false;
                out.sramc_rden = true;
                out.cscan_en = false;
                out.buff_shift = false;
                out.out_status = 2;
                break;

            case MainState::READING:
                out.cnt_en = true;
                out.wr_flag = false;
                out.cnt_start = false;
                out.rd_feeder_en = true;
                out.rd_feeder_clear = false;
                out.wr_feeder_en = false;
                out.wr_feeder_clear = true;
                out.done = false;
                out.shift_done = true;
                out.sramc_wren = false;
                out.sramc_rden = true;
                out.cscan_en = false;
                out.buff_shift = false;
                out.out_status = 3;
                break;

            case MainState::READING_LAT_WAIT:
                out.cnt_en = false;
                out.wr_flag = false;
                out.cnt_start = false;
                out.rd_feeder_en = true;
                out.rd_feeder_clear = false;
                out.wr_feeder_en = false;
                out.wr_feeder_clear = true;
                out.done = false;
                out.shift_done = true;
                out.sramc_wren = false;
                out.sramc_rden = true;
                out.cscan_en = false;
                out.buff_shift = false;
                out.out_status = 4;
                break;

            case MainState::POSTREAD_SHIFT:
                out.cnt_en = false;
                out.wr_flag = false;
                out.cnt_start = false;
                out.rd_feeder_en = false;
                out.rd_feeder_clear = true;
                out.wr_feeder_en = false;
                out.wr_feeder_clear = true;
                out.done = false;
                out.shift_done = false;
                out.sramc_wren = false;
                out.sramc_rden = false;
                out.cscan_en = false;
                out.buff_shift = true;
                out.out_status = 5;
                break;

            case MainState::PREWRITE_SHIFT:
                out.cnt_en = false;
                out.wr_flag = false;
                out.cnt_start = false;
                out.rd_feeder_en = false;
                out.rd_feeder_clear = true;
                out.wr_feeder_en = false;
                out.wr_feeder_clear = true;
                out.done = false;
                out.shift_done = false;
                out.sramc_wren = false;
                out.sramc_rden = false;
                out.cscan_en = true;
                out.buff_shift = false;
                out.out_status = 6;
                break;

            case MainState::WR_CNT_START:
                out.cnt_en = true;
                out.wr_flag = true;
                out.cnt_start = true;
                out.rd_feeder_en = false;
                out.rd_feeder_clear = true;
                out.wr_feeder_en = true;
                out.wr_feeder_clear = false;
                out.done = false;
                out.shift_done = true;
                out.sramc_wren = false;
                out.sramc_rden = false;
                out.cscan_en = false;
                out.buff_shift = false;
                out.out_status = 7;
                break;

            case MainState::WRITING:
                out.cnt_en = true;
                out.wr_flag = true;
                out.cnt_start = false;
                out.rd_feeder_en = false;
                out.rd_feeder_clear = true;
                out.wr_feeder_en = true;
                out.wr_feeder_clear = false;
                out.done = false;
                out.shift_done = true;
                out.sramc_wren = true;
                out.sramc_rden = false;
                out.cscan_en = false;
                out.buff_shift = false;
                out.out_status = 8;
                break;

            case MainState::WREN_HOLD_LAST:
                out.cnt_en = false;
                out.wr_flag = true;
                out.cnt_start = false;
                out.rd_feeder_en = false;
                out.rd_feeder_clear = true;
                out.wr_feeder_en = true;
                out.wr_feeder_clear = false;
                out.done = false;
                out.shift_done = true;
                out.sramc_wren = true;
                out.sramc_rden = false;
                out.cscan_en = false;
                out.buff_shift = false;
                out.out_status = 9;
                break;

            case MainState::WRITING_LAT_WAIT:
                out.cnt_en = false;
                out.wr_flag = true;
                out.cnt_start = false;
                out.rd_feeder_en = false;
                out.rd_feeder_clear = true;
                out.wr_feeder_en = true;
                out.wr_feeder_clear = false;
                out.done = false;
                out.shift_done = true;
                out.sramc_wren = false;
                out.sramc_rden = false;
                out.cscan_en = false;
                out.buff_shift = false;
                out.out_status = 10;
                break;

            case MainState::FINISH:
                out.cnt_en = false;
                out.wr_flag = false;
                out.cnt_start = false;
                out.rd_feeder_en = false;
                out.rd_feeder_clear = true;
                out.wr_feeder_en = false;
                out.wr_feeder_clear = true;
                out.done = true;
                out.shift_done = true;
                out.sramc_wren = false;
                out.sramc_rden = false;
                out.cscan_en = false;
                out.buff_shift = false;
                out.out_status = 11;
                break;
            }

            return out;
        }

        // Phase 2: transition logic + register commit. Requires in.done and
        // in.til_done to be the REAL values (from sauria_rtl::PsmIdxCnt, fed into
        // it using THIS SAME cycle's cnt_en/cnt_clear/wr_flag from the
        // compute_outputs() call immediately before this one).
        void commit(const Inputs &in)
        {
            const bool inactive_cols_flag = cached_inactive_cols_flag_;
            const bool completion_flag = cached_completion_flag_;

            // --- Transition logic (RTL lines 203-336) ---
            MainState next_state = main_state_;

            switch (main_state_)
            {
            case MainState::IDLE:
                if (in.fsm_start && !completion_flag)
                {
                    if (in.force_write_path)
                    {
                        // See Inputs::force_write_path's own comment -- bypasses
                        // the ctx_cnt<3 preload-warmup branch entirely.
                        next_state = MainState::PREWRITE_SHIFT;
                    }
                    else if (ctx_cnt_ < 3)
                    {
                        if (in.preload_en && (in.ncontexts >= ctx_cnt_))
                        {
                            next_state = (ctx_cnt_ == 0) ? MainState::RD_CNT_START
                                                          : MainState::PREREAD_SHIFT;
                            if (ctx_cnt_ < 64) ++g_fx1_rdstart[ctx_cnt_];
                            else ++g_fx1_rdstart_hi;
                        }
                        else
                        {
                            next_state = MainState::FINISH;
                        }
                    }
                    else
                    {
                        next_state = MainState::PREWRITE_SHIFT;
                    }
                }
                break;

            case MainState::PREREAD_SHIFT:
                if (in.pipeline_en && (scan_cnt_ == (uint32_t)(X_DIM - 1)))
                {
                    next_state = (ctx_cnt_ == in.ncontexts) ? MainState::FINISH
                                                             : MainState::RD_CNT_START;
                }
                break;

            case MainState::RD_CNT_START:
                next_state = MainState::READING;
                break;

            case MainState::READING:
                if (in.done)
                    next_state = MainState::READING_LAT_WAIT;
                break;

            case MainState::READING_LAT_WAIT:
                if (cyc_cnt_ == (uint32_t)RD_LAT)
                {
                    next_state = inactive_cols_flag ? MainState::POSTREAD_SHIFT
                                                     : MainState::FINISH;
                }
                break;

            case MainState::POSTREAD_SHIFT:
                // RTL: i_inactive_cols-1 -- guard against underflow if somehow 0
                // (inactive_cols_flag already false in that case, so this state
                // would not have been entered; kept literal otherwise).
                if (in.inactive_cols > 0 && cyc_cnt_ == (in.inactive_cols - 1))
                {
                    next_state = MainState::FINISH;
                }
                break;

            case MainState::PREWRITE_SHIFT:
                if (in.pipeline_en && (scan_cnt_ == (uint32_t)(X_DIM - 1)))
                {
                    next_state = MainState::WR_CNT_START;
                }
                break;

            case MainState::WR_CNT_START:
                next_state = MainState::WRITING;
                break;

            case MainState::WRITING:
                if (in.done)
                    next_state = MainState::WREN_HOLD_LAST;
                break;

            case MainState::WREN_HOLD_LAST:
                next_state = MainState::WRITING_LAT_WAIT;
                break;

            case MainState::WRITING_LAT_WAIT:
                if (cyc_cnt_ == (uint32_t)WR_LAT)
                {
                    next_state = (in.preload_en && (ctx_cnt_ < in.ncontexts))
                                     ? MainState::RD_CNT_START
                                     : MainState::FINISH;
                    if (in.preload_en && (ctx_cnt_ < in.ncontexts)) {
                        if (ctx_cnt_ < 64) ++g_fx1_rdstart[ctx_cnt_];
                        else ++g_fx1_rdstart_hi;
                    }
                }
                break;

            case MainState::FINISH:
                next_state = MainState::IDLE;
                break;
            }

            // --- Counter next-state (RTL lines 140-167, "counters_reg") ---
            // cyc_cnt: cyc_cnt_clear takes priority over cyc_cnt_en (both driven
            // per-state above, "avoid latches" defaults make cyc_cnt_clear=1 the
            // default whenever a state doesn't explicitly override it -- but every
            // state above already sets one of {en=0,clear=1 implicitly via not
            // setting en} or {en=1,clear=0} pairs matching the RTL exactly; ported
            // literally as separate en/clear locals here since psm_shift_fsm.sv's
            // own always_comb sets BOTH cyc_cnt_en and cyc_cnt_clear per state,
            // unlike out.cnt_clear which is a top-level "assign").
            bool cyc_cnt_en, cyc_cnt_clear;
            bool scan_cnt_en, scan_cnt_clear;
            bool ctx_cnt_en;
            compute_counter_enables(main_state_, cyc_cnt_, cyc_cnt_en, cyc_cnt_clear,
                                     scan_cnt_en, scan_cnt_clear, ctx_cnt_en);

            uint32_t cyc_cnt_next = cyc_cnt_clear ? 0
                                     : cyc_cnt_en  ? (cyc_cnt_ + 1)
                                                   : cyc_cnt_;
            uint32_t scan_cnt_next = scan_cnt_clear ? 0
                                      : (scan_cnt_en && in.pipeline_en) ? (scan_cnt_ + 1)
                                                                        : scan_cnt_;
            uint32_t ctx_cnt_next = in.fsm_reset ? 0
                                     : ctx_cnt_en ? (ctx_cnt_ + 1)
                                                  : ctx_cnt_;

            // ---------- Phase 2: commit ----------
            main_state_ = next_state;
            cyc_cnt_ = cyc_cnt_next;
            scan_cnt_ = scan_cnt_next;
            ctx_cnt_ = ctx_cnt_next;
        }

        // Convenience wrapper for standalone use (e.g. unit tests) where
        // in.done/in.til_done are already known upfront -- combines both
        // phases using the SAME Inputs. The real wiring layer (psm_top.h)
        // does NOT use this; it calls compute_outputs()/commit() separately
        // with a sauria_rtl::PsmIdxCnt::tick() in between.
        Outputs tick(const Inputs &in)
        {
            Outputs out = compute_outputs(in);
            commit(in);
            return out;
        }

    private:
        // Re-derives the SAME per-state {cyc_cnt_en, cyc_cnt_clear, scan_cnt_en,
        // scan_cnt_clear, ctx_cnt_en} values the RTL's output_logic always_comb
        // sets alongside the Outputs already computed above -- kept as a SEPARATE
        // small switch (not folded into the main Outputs switch) only because
        // Outputs has no fields for these RTL-internal-only counter enables;
        // every branch below matches output_logic's own per-state values exactly
        // (verify-tier 0: re-checked against context_fsm.h line-by-line while
        // writing this).
        static void compute_counter_enables(MainState s, uint32_t cyc_cnt,
                                             bool &cyc_en, bool &cyc_clr,
                                             bool &scan_en, bool &scan_clr, bool &ctx_en)
        {
            // Defaults (RTL lines 358-362: "avoid latches")
            cyc_en = false;
            cyc_clr = true;
            scan_en = false;
            scan_clr = true;
            ctx_en = false;

            switch (s)
            {
            case MainState::IDLE:
                break; // all defaults
            case MainState::PREREAD_SHIFT:
                scan_en = true;
                scan_clr = false;
                break;
            case MainState::RD_CNT_START:
                break; // all defaults
            case MainState::READING:
                break; // all defaults
            case MainState::READING_LAT_WAIT:
                // RTL (lines 488-495): cyc_cnt_en=1 always here, but
                // cyc_cnt_clear is CONDITIONAL ("Reset counter on last cycle") --
                // NOT a constant. POSTREAD_SHIFT (reached when inactive_cols_flag)
                // does NOT itself clear cyc_cnt (its own cyc_cnt_clear=0
                // constant, see below), so it relies on THIS exact reset firing
                // on the tick cyc_cnt reaches RD_LAT, or it would inherit a
                // stale nonzero cyc_cnt and mis-time its own exit condition.
                cyc_en = true;
                cyc_clr = (cyc_cnt == (uint32_t)RD_LAT);
                break;
            case MainState::POSTREAD_SHIFT:
                cyc_en = true;
                cyc_clr = false;
                break;
            case MainState::PREWRITE_SHIFT:
                scan_en = true;
                scan_clr = false;
                break;
            case MainState::WR_CNT_START:
                break; // all defaults
            case MainState::WRITING:
                break; // all defaults
            case MainState::WREN_HOLD_LAST:
                break; // all defaults
            case MainState::WRITING_LAT_WAIT:
                cyc_en = true;
                cyc_clr = false;
                break;
            case MainState::FINISH:
                ctx_en = true;
                break;
            }
        }

        MainState main_state_{MainState::IDLE};
        uint32_t cyc_cnt_{0};
        uint32_t scan_cnt_{0};
        uint32_t ctx_cnt_{0};

        // Cached between compute_outputs() and commit().
        bool cached_inactive_cols_flag_{false};
        bool cached_completion_flag_{false};
    };

} // namespace sauria_rtl
