#pragma once
//
// 1:1 port of RTL sauria_core/data_feeder/wei_idxcnt.sv + common/cnt_generic.sv: the WEIGHT-side address-generation
// cascade (companion to ifmap_idxcnt.h).
//
// It carries the RTL's context-rolling mechanism: the Tiling-K counter is enabled by
//     i_cnt_en && w_flag && i_cswitch
// where i_cswitch is feeders_fsm's o_wei_cswitch (= wei_ov_flag, from the weight repetition counter), instead of a
// per-context cnt_clear restart.
//
// STRUCTURAL DIFFERENCES FROM ifmap_idxcnt (do NOT copy that file's logic
// blindly -- these are real, verified against the two .sv files side by side):
//   1. THREE counters (aux / w / til_k), not five.
//   2. Has i_cswitch; ifmap_idxcnt has i_finalctx instead, and uses it ONLY for
//      the outbounds flag -- here i_cswitch gates BOTH the til_k counter AND
//      the outbounds set condition.
//   3. Has a TRANSITION FLAG mechanism (unaligned-weight support) that
//      ifmap_idxcnt has no equivalent of: when the auxiliary counter is unused
//      (i_auxlim <= 1), transition_flag detects the SRAM word boundary being
//      crossed and, unless i_waligned, suppresses the w counter and the
//      done/til_done flags for that cycle. o_transn is the INVERTED, twice-
//      shimmed version of it.
//   4. done_d/til_done_d are gated by (!transition_flag || i_aux_cnt); the
//      ifmap side has no such gate (done_d = xyc_flag outright).
//   5. Only ONE address register stage feeds o_sram_addr, but o_woffs and
//      o_transn come through the extra out-shimming stage, same as ifmap.
//
// cnt_generic.sv semantics (identical to the ifmap port, restated so this file
// is self-contained): cnt_d = cnt_q + i_step; if (cnt_d >= i_lim) { o_flag = 1;
// cnt_d = 0; }. o_flag is COMBINATIONAL off the pre-tick cnt_q and does NOT
// depend on i_en -- it answers "if asked to tick now, would I overflow". The
// register commits only when (i_en || i_clear).
//
// Two-phase inside a single tick(): Phase 1 derives every combinational value
// and every register's next state from PRE-tick state only; Phase 2 commits.
// Outputs are read AFTER the commit, matching ifmap_idxcnt.h's existing
// convention (its header notes RTL's plain continuous assigns off registers) --
// keep the two consistent so the feeders can drive them from one call site.
//
#include <cstdint>
#ifdef FX1_A3_DEBUG_DUMPS
#include <fstream>
#endif

namespace sauria_rtl
{
using namespace sauria; // A1 port: shared vector/mask types (act_vector_t, wei_vector_t, psum_vector_t, sramc_mask_t, host_data_t, ...) stay in ::sauria, only the class bodies move to ::sauria_rtl. See rtl_ref_context_switch_controller.h header comment for the general namespace rationale.

    template <int IDX_W = 11, int ADRB_W = 8, int WOFS_W = 3>
    class WeiIdxCnt
    {
    public:
        using idx_t = uint32_t;

        struct Inputs
        {
            bool cnt_en = false;    // i_cnt_en
            bool cnt_clear = false; // i_cnt_clear
            bool cswitch = false;   // i_cswitch  (feeders_fsm o_wei_cswitch)
            bool waligned = false;  // i_waligned

            idx_t auxlim = 0, auxstep = 0;
            idx_t wlim = 0, wstep = 0;
            idx_t til_klim = 0, til_kstep = 0;
        };

        struct Outputs
        {
            bool transn = false;    // o_transn  (INVERTED transition, twice shimmed)
            uint32_t sram_addr = 0; // o_sram_addr
            uint32_t woffs = 0;     // o_woffs
            bool outbounds = false; // o_outbounds
            bool done = false;      // o_done
            bool til_done = false;  // o_til_done
        };

        void reset()
        {
            aux_idx_ = w_idx_ = til_k_idx_ = 0;
            sram_idx_q_ = 0;
            done_q_ = false;
            til_done_q_ = false;
            transition_q1_ = false;
            transition_q2_ = false;
            cnt_clear_q_ = false;
            outbounds_q_ = false;
            woffs_outshim_ = 0;
            outbounds_outshim_ = false;
        }

        Outputs tick(const Inputs &in)
        {
            const idx_t mask = (IDX_W >= 32) ? 0xFFFFFFFFu : ((1u << IDX_W) - 1u);

            // ---------- Phase 1: combinational, from PRE-tick state ----------

            auto ov = [&](idx_t cnt_q, idx_t step, idx_t lim) -> bool
            {
                return ((cnt_q + step) & mask) >= lim;
            };

            const bool aux_ov_flag = ov(aux_idx_, in.auxstep, in.auxlim);
            const bool w_ov_flag = ov(w_idx_, in.wstep, in.wlim);
            const bool til_k_ov_flag = ov(til_k_idx_, in.til_kstep, in.til_klim);

            // RTL line 87: auxiliary counter is "in use" only when auxlim > 1.
            const bool aux_cnt_in_use = (in.auxlim > 1);

            // RTL 93-95
            const bool aux_flag = aux_ov_flag;
            const bool w_flag = aux_ov_flag && w_ov_flag;
            const bool tilk_flag = aux_ov_flag && w_ov_flag && til_k_ov_flag;

            // RTL 147: three-term sum (ifmap side sums five)
            const idx_t sram_idx_d = (aux_idx_ + w_idx_ + til_k_idx_) & mask;

            // RTL 153-161: transition flag. When the aux counter is in use it is
            // simply aux_ov_flag; otherwise it detects the WORD boundary
            // (sram_idx[IDX_W-1:WOFS_W]) changing, and is suppressed entirely
            // when the weights are known to be word-aligned.
            bool transition_flag;
            if (aux_cnt_in_use)
            {
                transition_flag = aux_ov_flag;
            }
            else
            {
                transition_flag = ((sram_idx_q_ >> WOFS_W) != (sram_idx_d >> WOFS_W)) &&
                                  (!in.waligned);
            }

            // RTL 167-168
            const bool gate = (!transition_flag || aux_cnt_in_use);
            const bool done_d = w_flag && gate;
            const bool til_done_d = tilk_flag && gate;

            // --- counter next-states (cnt_generic commit rule) ---
            auto next_cnt = [&](idx_t cnt_q, idx_t step, idx_t lim, bool en) -> idx_t
            {
                if (in.cnt_clear)
                    return 0;
                if (!en)
                    return cnt_q;
                const idx_t next = (cnt_q + step) & mask;
                return (next >= lim) ? 0 : next;
            };

            // RTL 109 / 123 / 137 -- note the w counter's own transition gate and
            // the til_k counter's i_cswitch gate.
            const idx_t aux_next = next_cnt(aux_idx_, in.auxstep, in.auxlim, in.cnt_en);
            const idx_t w_next = next_cnt(
                w_idx_, in.wstep, in.wlim,
                in.cnt_en && aux_flag && (!transition_flag || aux_cnt_in_use));
            const idx_t til_k_next = next_cnt(
                til_k_idx_, in.til_kstep, in.til_klim,
                in.cnt_en && w_flag && in.cswitch);

            // --- gen_reg (RTL 170-193) ---
            idx_t sram_idx_q_next;
            bool done_q_next, til_done_q_next, transition_q1_next;
            if (in.cnt_clear)
            {
                sram_idx_q_next = 0;
                done_q_next = false;
                til_done_q_next = false;
                transition_q1_next = false;
            }
            else if (in.cnt_en)
            {
                sram_idx_q_next = sram_idx_d;
                done_q_next = done_d;
                til_done_q_next = til_done_d;
                transition_q1_next = transition_flag;
            }
            else
            {
                sram_idx_q_next = sram_idx_q_;
                done_q_next = done_q_;
                til_done_q_next = til_done_q_;
                transition_q1_next = transition_q1_;
            }

            // --- outbounds_reg (RTL 199-215). cnt_clear_q is UNCONDITIONAL.
            // Set condition reads the PRE-tick til_done_q and uses i_cswitch
            // (the ifmap module uses i_finalctx here instead).
            const bool cnt_clear_q_next = in.cnt_clear;
            bool outbounds_q_next;
            if (in.cnt_clear)
                outbounds_q_next = false;
            else if (in.cnt_en && til_done_q_ && in.cswitch)
                outbounds_q_next = true;
            else
                outbounds_q_next = outbounds_q_;

            // --- out_shimming_reg (RTL 221-241) ---
            uint32_t woffs_next;
            bool transition_q2_next, outbounds_outshim_next;
            if (in.cnt_clear)
            {
                woffs_next = 0;
                transition_q2_next = false;
                outbounds_outshim_next = false;
            }
            else if (in.cnt_en)
            {
                const idx_t wofs_mask = (WOFS_W >= 32) ? 0xFFFFFFFFu : ((1u << WOFS_W) - 1u);
                woffs_next = sram_idx_q_ & wofs_mask;
                transition_q2_next = transition_q1_;
                outbounds_outshim_next = outbounds_q_;
            }
            else
            {
                woffs_next = woffs_outshim_;
                transition_q2_next = transition_q2_;
                outbounds_outshim_next = outbounds_outshim_;
            }

            // ---------- Phase 2: commit ----------
            aux_idx_ = aux_next;
            w_idx_ = w_next;
#ifdef FX1_A3_DEBUG_DUMPS
            // Debug hook (default off): the three terms of the `til_k` advance condition, every cycle.
            {
                static std::ofstream tk("trace_sysc/wei_tilk_trace.csv");
                static bool tk_hdr = false;
                static long tk_n = 0;
                if (!tk_hdr)
                {
                    tk << "n,cnt_en,aux_ov,w_ov,w_flag,cswitch,en,"
                          "til_k_idx,til_k_next,w_idx,aux_idx\n";
                    tk_hdr = true;
                }
                const bool tk_en = in.cnt_en && w_flag && in.cswitch;
                tk << tk_n++ << "," << (int)in.cnt_en
                   << "," << (int)aux_ov_flag << "," << (int)w_ov_flag
                   << "," << (int)w_flag << "," << (int)in.cswitch
                   << "," << (int)tk_en
                   << "," << (long long)til_k_idx_
                   << "," << (long long)til_k_next
                   << "," << (long long)w_idx_
                   << "," << (long long)aux_idx_ << "\n";
            }
#endif
            til_k_idx_ = til_k_next;

            sram_idx_q_ = sram_idx_q_next;
            done_q_ = done_q_next;
            til_done_q_ = til_done_q_next;
            transition_q1_ = transition_q1_next;

            cnt_clear_q_ = cnt_clear_q_next;
            outbounds_q_ = outbounds_q_next;

            woffs_outshim_ = woffs_next;
            transition_q2_ = transition_q2_next;
            outbounds_outshim_ = outbounds_outshim_next;

            // ---------- Outputs (RTL 247-255) ----------
            Outputs out;
            const idx_t addr_mask = (ADRB_W >= 32) ? 0xFFFFFFFFu : ((1u << ADRB_W) - 1u);
            out.sram_addr = (sram_idx_q_ >> WOFS_W) & addr_mask;
            out.woffs = woffs_outshim_;
            out.outbounds = outbounds_q_;
            out.transn = !transition_q2_;
            out.done = done_q_ && in.cnt_en;
            out.til_done = til_done_q_ && in.cnt_en;
            return out;
        }

        // Introspection for the isolated cross-check harness (not RTL ports).
        idx_t dbg_aux() const { return aux_idx_; }
        idx_t dbg_w() const { return w_idx_; }
        idx_t dbg_tilk() const { return til_k_idx_; }

    private:
        idx_t aux_idx_ = 0, w_idx_ = 0, til_k_idx_ = 0;
        idx_t sram_idx_q_ = 0;
        bool done_q_ = false, til_done_q_ = false;
        bool transition_q1_ = false, transition_q2_ = false;
        bool cnt_clear_q_ = false;
        bool outbounds_q_ = false;
        uint32_t woffs_outshim_ = 0;
        bool outbounds_outshim_ = false;
    };

} // namespace sauria_rtl
