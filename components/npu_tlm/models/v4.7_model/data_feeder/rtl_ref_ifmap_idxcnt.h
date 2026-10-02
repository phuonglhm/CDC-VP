#pragma once
//
// 1:1 port of RTL sauria_core/data_feeder/ifmap_idxcnt.sv + sauria_core/common/cnt_generic.sv.
// This is the address-generation counter cascade that produces the real K%32 pointwise stall (not
// feed_data_manager.sv). Do not "clean up" or reinterpret without re-checking against the RTL source.
//
// RTL structure: 5 instances of cnt_generic (X/Y/Ch/TilX/TilY) chained so each
// counter only advances (i_en) when ALL faster counters have overflowed THIS cycle:
//   x_flag    = x_ov_flag
//   xy_flag   = x_ov_flag & y_ov_flag
//   xyc_flag  = x_ov_flag & y_ov_flag & ch_ov_flag
//   tilx_flag = xyc_flag & til_x_ov_flag
//   tilxy_flag= tilx_flag & til_y_ov_flag
// Every cnt_en cycle produces exactly ONE address (sram_idx_d = xy_idx+chg_idx,
// registered into sram_idx_q next cycle) -- the X counter's "3 steps to overflow"
// (UNALIGNED xlim) are 3 REAL cnt_en cycles, each emitting a distinct
// address; Y/Ch/TilX/TilY only advance on the cycle where all faster counters
// overflow simultaneously. This is what makes an UNALIGNED xlim cost 3x the cycles
// of an ALIGNED one for the same logical Ch(K) advance -- it is not an abstract
// "cost", it is literally 3 address-gen cycles per K instead of 1.
//
// Two-phase tick(): Phase 1 computes every combinational value (this cycle's flags
// AND every register's next-state) purely from pre-tick register state + this
// cycle's inputs, mirroring how SV always_comb blocks all see the same pre-edge
// register values regardless of textual order. Phase 2 commits all next-state
// values, as if a single clock edge fired. (Same convention as feed_data_manager.h.)
//
// cnt_generic.sv semantics (exact): every i_en cycle, unconditionally
// (i_clear takes priority): cnt_d = cnt_q + i_step; if (cnt_d >= i_lim) { o_flag=1;
// cnt_d=0; }. o_flag is COMBINATIONAL from the PRE-tick cnt_q (i.e. "would this
// increment overflow"), independent of whether i_en is actually high this cycle --
// downstream users gate on the AND of flags (x_ov_flag & y_ov_flag & ...), so a
// slower counter's flag being combinationally "already true" before it has actually
// ticked is intentional (it answers "if asked to tick now, would I also overflow").

#include <cstdint>

namespace sauria_rtl
{
using namespace sauria; // A1 port: shared vector/mask types (act_vector_t, wei_vector_t, psum_vector_t, sramc_mask_t, host_data_t, ...) stay in ::sauria, only the class bodies move to ::sauria_rtl. See rtl_ref_context_switch_controller.h header comment for the general namespace rationale.

    // IDX_W: index/limit/step register width (RTL default 11). ADRA_W: SRAM address
    // width (RTL default 8). WOFS_W: word-offset width (RTL default 3, so
    // ADRA_W+WOFS_W must cover IDX_W's useful range -- RTL just splits sram_idx_q
    // into [IDX_W-1:WOFS_W] (address) and [WOFS_W-1:0] (word offset), no width
    // check in the RTL itself).
    template <int IDX_W = 11, int ADRA_W = 8, int WOFS_W = 3>
    class IfmapIdxCnt
    {
    public:
        using idx_t = uint32_t; // widened container; real field is IDX_W-bit unsigned

        struct Inputs
        {
            bool cnt_en = false;   // i_cnt_en
            bool cnt_clear = false; // i_cnt_clear
            bool finalctx = false;  // i_finalctx

            idx_t xlim = 0, xstep = 0;
            idx_t ylim = 0, ystep = 0;
            idx_t chlim = 0, chstep = 0;
            idx_t til_xlim = 0, til_xstep = 0;
            idx_t til_ylim = 0, til_ystep = 0;

            // value the TILING counters take on i_cnt_clear
            // instead of 0. Only consumed under FX1_A3_TILCNT_SEED_PER_CTX;
            // left at 0 they reproduce the RTL/old behaviour exactly.
            idx_t til_x_seed = 0, til_y_seed = 0;
        };

        struct Outputs
        {
            bool x_ov_flag = false;    // o_x_ov_flag
            uint32_t sram_addr = 0;    // o_sram_addr (IDX_W-1:WOFS_W of sram_idx_q)
            uint32_t woffs = 0;        // o_woffs
            bool outbounds = false;    // o_outbounds
            bool done = false;         // o_done (all positions finished, current context)
            bool til_done = false;     // o_til_done (all positions finished, all contexts)
        };

        // Per-context reset (mirrors RTL i_rstn -- call once before first tick()).
        void reset()
        {
            x_idx_ = y_idx_ = ch_idx_ = til_x_idx_ = til_y_idx_ = 0;
            sram_idx_q_ = 0;
            x_ov_flag_q_ = false;
            done_q_ = false;
            til_done_q_ = false;
            cnt_clear_q_ = false;
            outbounds_q_ = false;
            woffs_outshim_ = 0;
            x_ov_flag_outshim_ = false;
            outbounds_outshim_ = false;
        }

        // The harness keeps one feeder instance while ContextFsm advances
        // outer contexts. Preserve address counters, but discard edge/status
        // latches held while cnt_en was low, so a later finalctx cannot
        // consume a stale til_done_q.
#ifdef FX1_A3_TILDONE_Q_GATE
        // Registered til_done_q (RTL ifmap_idxcnt.sv til_done_q) BEFORE this cycle's
        // tick(). RTL o_til_done = til_done_q & i_cnt_en (sv:274) uses this value.
        bool peek_til_done_q() const { return til_done_q_; }
#endif

        void clear_context_flags()
        {
            // Preserve address/x-transition phase; only clear the edge
            // latches whose stale value can trigger outbounds at finalctx.
            til_done_q_ = false;
            outbounds_q_ = false;
            outbounds_outshim_ = false;
        }

        Outputs tick(const Inputs &in)
        {
            const idx_t mask = (IDX_W >= 32) ? 0xFFFFFFFFu : ((1u << IDX_W) - 1u);

            // ---------- Phase 1: combinational (pre-tick register state) ----------

            auto ov = [&](idx_t cnt_q, idx_t step, idx_t lim) -> bool
            {
                // cnt_generic.sv: o_flag = ((cnt_q + step) & mask) >= lim, i_clear
                // forces cnt_d=0 but does not affect o_flag's combinational formula
                // itself (RTL still computes cnt_d=0/o_flag=1 branch under i_clear
                // in the same always_comb -- but i_clear is only ever pulsed together
                // with cnt_en=0 in this module's caller, and downstream commit below
                // handles i_clear separately, so we only need the non-clear formula
                // here for flag purposes, matching how o_flag is actually consumed).
                idx_t next = (cnt_q + step) & mask;
                return next >= lim;
            };

            const bool x_ov_flag = ov(x_idx_, in.xstep, in.xlim);
            const bool y_ov_flag = ov(y_idx_, in.ystep, in.ylim);
            const bool ch_ov_flag = ov(ch_idx_, in.chstep, in.chlim);
            const bool til_x_ov_flag = ov(til_x_idx_, in.til_xstep, in.til_xlim);
            const bool til_y_ov_flag = ov(til_y_idx_, in.til_ystep, in.til_ylim);

            const bool x_flag = x_ov_flag;
            const bool xy_flag = x_ov_flag && y_ov_flag;
            const bool xyc_flag = xy_flag && ch_ov_flag;
            const bool tilx_flag = xyc_flag && til_x_ov_flag;
            const bool tilxy_flag = tilx_flag && til_y_ov_flag;

            // Address = sum of all 5 counters' CURRENT (pre-tick) values -- this is
            // the address for the position being emitted THIS cycle (registers into
            // sram_idx_q for use NEXT cycle, matching RTL's 1-cycle SRAM read latency
            // shimming).
            const idx_t xy_idx = (x_idx_ + y_idx_) & mask;
            const idx_t chg_idx = (ch_idx_ + til_x_idx_ + til_y_idx_) & mask;
            const idx_t sram_idx_d = (xy_idx + chg_idx) & mask;

            const bool done_d = xyc_flag;
            const bool til_done_d = tilxy_flag;

            // Next-state for each counter (cnt_generic.sv commit rule: only updates
            // cnt_q when i_en || i_clear; i_en for each counter = cnt_en && <its
            // cascade-enable flag>, EXCEPT X which is just cnt_en).
            auto next_cnt = [&](idx_t cnt_q, idx_t step, idx_t lim, bool en,
                                idx_t seed = 0) -> idx_t
            {
                if (in.cnt_clear)
                {
#ifdef FX1_A3_CNTCLEAR_HOLD
                    // The RTL never pulses cnt_clear between contexts: all five stages accumulate, so the xyc loop crosses
                    // context boundaries naturally. Holding the value removes the per-context clear a harness would add.
                    // (void)seed: the seed is unused while holding.
                    (void)seed;
                    return cnt_q;
#else
                    return seed;
#endif
                }
                if (!en)
                    return cnt_q;
                idx_t next = (cnt_q + step) & mask;
                return (next >= lim) ? 0 : next;
            };

            const idx_t x_idx_next = next_cnt(x_idx_, in.xstep, in.xlim, in.cnt_en);
            const idx_t y_idx_next = next_cnt(y_idx_, in.ystep, in.ylim, in.cnt_en && x_flag);
            const idx_t ch_idx_next = next_cnt(ch_idx_, in.chstep, in.chlim, in.cnt_en && xy_flag);
#ifdef FX1_A3_TILCNT_SEED_PER_CTX
            // the harness restarts the feeder at every context boundary, but
            // RTL rolls all contexts inside ONE run, so til_x/til_y must resume at
            // the current context's position rather than at 0. proved the
            // seeds equal the RTL cascade values for all 8 contexts.
            const idx_t tx_seed = in.til_x_seed;
            const idx_t ty_seed = in.til_y_seed;
#else
            const idx_t tx_seed = 0;
            const idx_t ty_seed = 0;
#endif
            const idx_t til_x_idx_next = next_cnt(til_x_idx_, in.til_xstep, in.til_xlim, in.cnt_en && xyc_flag, tx_seed);
            const idx_t til_y_idx_next = next_cnt(til_y_idx_, in.til_ystep, in.til_ylim, in.cnt_en && tilx_flag, ty_seed);

            // sram_idx_q / x_ov_flag_q / done_q / til_done_q: gated by cnt_clear then
            // cnt_en (woffs_reg always_ff block).
            idx_t sram_idx_q_next;
            bool x_ov_flag_q_next, done_q_next, til_done_q_next;
            if (in.cnt_clear)
            {
                sram_idx_q_next = 0;
                x_ov_flag_q_next = false;
                done_q_next = false;
                til_done_q_next = false;
            }
            else if (in.cnt_en)
            {
                sram_idx_q_next = sram_idx_d;
                x_ov_flag_q_next = x_ov_flag;
                done_q_next = done_d;
                til_done_q_next = til_done_d;
            }
            else
            {
                sram_idx_q_next = sram_idx_q_;
                x_ov_flag_q_next = x_ov_flag_q_;
                done_q_next = done_q_;
                til_done_q_next = til_done_q_;
            }

            // outbounds_reg always_ff block: cnt_clear_q is UNCONDITIONAL every
            // cycle (no cnt_en gate); outbounds_q sets 1 cycle after final til_done
            // (reads the PRE-tick til_done_q_, per RTL `i_cnt_en && til_done_q &&
            // i_finalctx`), resets only on cnt_clear, otherwise holds.
            const bool cnt_clear_q_next = in.cnt_clear;
            bool outbounds_q_next;
            if (in.cnt_clear)
                outbounds_q_next = false;
#ifdef FX1_A3_OUTBOUNDS_ONE_REP
            // With ContextFsm rolling contexts OUTSIDE the feeder, `finalctx` (= act_ov_flag, true only every act_reps
            // contexts) would wrongly block setting outbounds on the other contexts. With act_reps = 1 `finalctx` is
            // always true and this reduces exactly to the default expression.
            else if (in.cnt_en && til_done_q_)
                outbounds_q_next = true;
#else
            else if (in.cnt_en && til_done_q_ && in.finalctx)
                outbounds_q_next = true;
#endif
            else
                outbounds_q_next = outbounds_q_;

            // out_shimming_reg always_ff block: gated by cnt_clear then cnt_en, one
            // more cycle of delay on top of sram_idx_q/x_ov_flag_q/outbounds_q.
            uint32_t woffs_outshim_next;
            bool x_ov_flag_outshim_next, outbounds_outshim_next;
            if (in.cnt_clear)
            {
                woffs_outshim_next = 0;
                x_ov_flag_outshim_next = false;
                outbounds_outshim_next = false;
            }
            else if (in.cnt_en)
            {
                const idx_t wofs_mask = (WOFS_W >= 32) ? 0xFFFFFFFFu : ((1u << WOFS_W) - 1u);
                woffs_outshim_next = sram_idx_q_ & wofs_mask;
                x_ov_flag_outshim_next = x_ov_flag_q_;
                outbounds_outshim_next = outbounds_q_;
            }
            else
            {
                woffs_outshim_next = woffs_outshim_;
                x_ov_flag_outshim_next = x_ov_flag_outshim_;
                outbounds_outshim_next = outbounds_outshim_;
            }

            // ---------- Phase 2: commit ----------


            x_idx_ = x_idx_next;
            y_idx_ = y_idx_next;
            ch_idx_ = ch_idx_next;
            til_x_idx_ = til_x_idx_next;
            til_y_idx_ = til_y_idx_next;

            sram_idx_q_ = sram_idx_q_next;
            x_ov_flag_q_ = x_ov_flag_q_next;
            done_q_ = done_q_next;
            til_done_q_ = til_done_q_next;

            cnt_clear_q_ = cnt_clear_q_next;
            outbounds_q_ = outbounds_q_next;

            woffs_outshim_ = woffs_outshim_next;
            x_ov_flag_outshim_ = x_ov_flag_outshim_next;
            outbounds_outshim_ = outbounds_outshim_next;

            // ---------- Outputs (RTL "assign" lines, combinational from _q regs
            // that were JUST updated above -- RTL's assigns read the post-edge q
            // values since they are plain continuous assigns off registers) ----------

            Outputs out;
            const idx_t addr_mask = (ADRA_W >= 32) ? 0xFFFFFFFFu : ((1u << ADRA_W) - 1u);
            out.sram_addr = (sram_idx_q_ >> WOFS_W) & addr_mask;
            out.woffs = woffs_outshim_;
            out.outbounds = outbounds_q_;
            out.x_ov_flag = x_ov_flag_outshim_ && in.cnt_en;
            out.done = done_q_ && in.cnt_en;
            out.til_done = til_done_q_ && in.cnt_en;
            return out;
        }

        // Debug hooks (read-only): the 5 internal counters (ripple carry x -> y -> ch -> til_x -> til_y).
        idx_t dbg_til_x_idx() const { return til_x_idx_; }
        idx_t dbg_til_y_idx() const { return til_y_idx_; }
        idx_t dbg_ch_idx() const { return ch_idx_; }

    private:
        // X/Y/Ch/TilX/TilY counter registers (cnt_generic.sv's cnt_q, one per
        // instance).
        idx_t x_idx_ = 0, y_idx_ = 0, ch_idx_ = 0, til_x_idx_ = 0, til_y_idx_ = 0;

        // ifmap_idxcnt.sv's own registers.
        idx_t sram_idx_q_ = 0;
        bool x_ov_flag_q_ = false;
        bool done_q_ = false;
        bool til_done_q_ = false;
        bool cnt_clear_q_ = false;
        bool outbounds_q_ = false;
        uint32_t woffs_outshim_ = 0;
        bool x_ov_flag_outshim_ = false;
        bool outbounds_outshim_ = false;
    };

} // namespace sauria_rtl
