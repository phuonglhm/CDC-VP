#pragma once
//
// 1:1 port of RTL sauria_core/psm/psm_idxcnt.sv + sauria_core/common/cnt_generic.sv + cnt_dualctx.sv.
//
// Includes o_sram_addr / o_mask / o_wr_fifo_pop: the RTL's SRAM-C address (psm_top.sv o_sramc_addr) comes directly
// from this module's o_sram_addr (through 3 register stages, psm_top.sv:292-321), not from a per-column / shift_cnt
// formula, so psm_wdata_manager.h is driven from here. o_done / o_til_done follow the RTL as well.
//
#include <cstdint>

namespace sauria_rtl
{
using namespace sauria; // A1 port: shared vector/mask types stay in ::sauria, class bodies move to ::sauria_rtl.

    template <int IDX_W = 11, int SRAMC_N = 2>
    class PsmIdxCnt
    {
    public:
        struct Inputs
        {
            bool cnt_en = false;    // i_cnt_en
            bool cnt_clear = false; // i_cnt_clear
            bool wr_flag = false;   // i_wr_flag (dual-context select for tiling counters)
            bool start = false;     // i_start (: needed for o_mask's first-position logic)

            uint32_t cxlim = 0, cxstep = 0;         // i_cxlim, i_cxstep
            uint32_t cklim = 0, ckstep = 0;         // i_cklim, i_ckstep
            uint32_t til_cylim = 0, til_cystep = 0; // i_til_cylim, i_til_cystep
            uint32_t til_cklim = 0, til_ckstep = 0; // i_til_cklim, i_til_ckstep
        };

        struct Outputs
        {
            bool done = false;         // o_done
            bool til_done = false;     // o_til_done
            uint32_t sram_addr = 0;    // o_sram_addr (word address towards SRAM-C)
            uint64_t mask = 0;         // o_mask, bit i = SRAMC_N word-slot i is active
            bool wr_fifo_pop = false;  // o_wr_fifo_pop
        };

        void reset()
        {
            x_q_ = 0;
            k_q_ = 0;
            til_xy_q1_ = til_xy_q2_ = 0;
            til_k_q1_ = til_k_q2_ = 0;
            done_q_ = false;
            til_done_q_ = false;
            transition_flag_q_ = false;
            sram_addr_q_ = 0;
            mask_q_ = 0;
            done_outshim_ = false;
            til_done_outshim_ = false;
            mask_outshim_ = 0;
            wr_fifo_pop_outshim_ = false;
        }

        Outputs tick(const Inputs &in)
        {
            // ---------- Phase 1: combinational (pre-tick register state) ----------

            // --- X counter (cnt_generic.sv): i_en=cnt_en, i_clear=cnt_clear||!cnt_en ---
            const bool x_clear = in.cnt_clear || !in.cnt_en;
            bool x_ov_flag;
            uint32_t x_next;
            generic_counter(x_q_, in.cxstep, in.cxlim, x_clear, x_ov_flag, x_next);

            // --- K counter (cnt_generic.sv): i_en=cnt_en&&x_flag, same clear as X ---
            const bool x_flag = x_ov_flag;
            bool k_ov_flag;
            uint32_t k_next;
            generic_counter(k_q_, in.ckstep, in.cklim, x_clear, k_ov_flag, k_next);

            const bool xk_flag = x_flag && k_ov_flag;

            // --- Tiling-XY counter (cnt_dualctx.sv): i_en=cnt_en&&xk_flag,
            // i_clear=cnt_clear (NOT including !cnt_en), i_sel=wr_flag ---
            bool til_xy_ov_flag;
            uint32_t til_xy_q1_next, til_xy_q2_next;
            dualctx_counter(til_xy_q1_, til_xy_q2_, in.wr_flag, in.til_cystep,
                             in.til_cylim, in.cnt_clear, til_xy_ov_flag,
                             til_xy_q1_next, til_xy_q2_next);

            const bool til_xy_flag = xk_flag && til_xy_ov_flag;

            // --- Tiling-K counter (cnt_dualctx.sv): i_en=cnt_en&&til_xy_flag,
            // i_clear=cnt_clear, i_sel=wr_flag ---
            bool til_k_ov_flag;
            uint32_t til_k_q1_next, til_k_q2_next;
            dualctx_counter(til_k_q1_, til_k_q2_, in.wr_flag, in.til_ckstep,
                             in.til_cklim, in.cnt_clear, til_k_ov_flag,
                             til_k_q1_next, til_k_q2_next);

            const bool til_xyk_flag = til_xy_flag && til_k_ov_flag;

            // --- done/til_done (RTL lines 266-267, 269-292: gen_reg) ---
            const bool done_d = xk_flag;
            const bool til_done_d = til_xyk_flag;

            bool done_q_next, til_done_q_next;
            if (in.cnt_clear || !in.cnt_en)
            {
                done_q_next = false;
                til_done_q_next = false;
            }
            else // in.cnt_en (the only remaining branch that updates, per RTL)
            {
                done_q_next = done_d;
                til_done_q_next = til_done_d;
            }

            // ----------: address & mask generation (RTL lines
            // 163-260) -- newly added, previously omitted. ----------

            // o_cnt of cnt_dualctx.sv reads the CURRENT (pre-tick) register
            // selected by i_sel, per cnt_dualctx.sv:115
            // (`assign o_cnt = (i_sel) ? cnt_q1 : cnt_q2;`) -- NOT the next-
            // state value.
            const uint32_t x_idx = x_q_;
            const uint32_t k_idx = k_q_;
            const uint32_t til_xy_idx = in.wr_flag ? til_xy_q1_ : til_xy_q2_;
            const uint32_t til_k_idx = in.wr_flag ? til_k_q1_ : til_k_q2_;

            // RTL: til_idx=til_xy_idx+til_k_idx; kk_idx=til_idx+k_idx;
            // sram_idx_d=x_idx+kk_idx. Using int64_t throughout (RTL keeps
            // a +1 guard bit on every one of these sums specifically to
            // avoid wraparound corrupting the last_pos_flag comparison).
            const int64_t til_idx = (int64_t)til_xy_idx + (int64_t)til_k_idx;
            const int64_t kk_idx = til_idx + (int64_t)k_idx;
            const int64_t sram_idx_d = (int64_t)x_idx + kk_idx;

            const int64_t sram_addr_d = sram_idx_d >> WOFS_W_;
            const int64_t woffs_d = sram_idx_d & ((int64_t(1) << WOFS_W_) - 1);

            // --- Transition flag (RTL lines 176-193) ---
            const bool transition_flag_d = x_flag && in.cnt_en;

            // --- Mask generation (RTL lines 199-260) ---
            const int64_t idx_zero_current = sram_addr_d << WOFS_W_;
            const int64_t idx_end = kk_idx + (int64_t)in.cxlim - (int64_t)(SRAMC_N + 1);
            const int64_t woffs_end = idx_end & ((int64_t(1) << WOFS_W_) - 1);
            const bool last_pos_flag = (idx_zero_current + (SRAMC_N - 1)) >= idx_end;

            uint64_t mask_d = 0;
            if (idx_zero_current > idx_end)
            {
                mask_d = 0;
            }
            else
            {
                mask_d = (SRAMC_N >= 64) ? ~0ULL : ((1ULL << SRAMC_N) - 1ULL);
                const bool trans_or_start = transition_flag_q_ || in.start;
                if (trans_or_start && last_pos_flag)
                {
                    mask_d = 0;
                    for (int i = 0; i < SRAMC_N; i++)
                        if (i >= (int)woffs_d && i <= (int)woffs_end)
                            mask_d |= (1ULL << i);
                }
                else if (trans_or_start)
                {
                    mask_d = 0;
                    for (int i = 0; i < SRAMC_N; i++)
                        if (i >= (int)woffs_d)
                            mask_d |= (1ULL << i);
                }
                else if (last_pos_flag)
                {
                    mask_d = 0;
                    for (int i = 0; i < SRAMC_N; i++)
                        if (i <= (int)woffs_end)
                            mask_d |= (1ULL << i);
                }
            }

            // ---------- Outputs (RTL lines 327-335: read the PRE-tick _q
            // registers directly, gated by THIS cycle's i_cnt_en/i_start/
            // i_wr_flag -- computed BEFORE the commits below overwrite
            // them, same discipline as the existing done/til_done output
            // (self-caught bug class, see original file header). ----------
            Outputs out;
            out.done = done_q_ && in.cnt_en;
            out.til_done = til_done_q_ && in.cnt_en;
            out.sram_addr = sram_addr_q_;
            out.wr_fifo_pop = wr_fifo_pop_outshim_;
            out.mask = (((!in.cnt_en) && !done_outshim_) || (in.start && in.wr_flag))
                           ? 0
                           : mask_outshim_;

            // ---------- Register commits (each counter's OWN enable, per RTL) ----------
            // X/K registers: `if (i_en || i_clear) cnt_q <= cnt_d;` -- i_en=cnt_en
            // (X) / cnt_en&&x_flag (K), i_clear=x_clear (same for both here).
            if (in.cnt_en || x_clear)
                x_q_ = x_next;
            if ((in.cnt_en && x_flag) || x_clear)
                k_q_ = k_next;

            // Tiling registers: `if (i_en || i_clear) {cnt_q1<=cnt_d1;cnt_q2<=cnt_d2;}`
            // -- i_en=cnt_en&&xk_flag (til_xy) / cnt_en&&til_xy_flag (til_k),
            // i_clear=cnt_clear (NOT gated by !cnt_en, unlike X/K above).
            if ((in.cnt_en && xk_flag) || in.cnt_clear)
            {
                til_xy_q1_ = til_xy_q1_next;
                til_xy_q2_ = til_xy_q2_next;
            }
            if ((in.cnt_en && til_xy_flag) || in.cnt_clear)
            {
                til_k_q1_ = til_k_q1_next;
                til_k_q2_ = til_k_q2_next;
            }

#ifdef FX1_A3_IDXCNT_SHIM_PRETICK
            // Capture PRE-TICK values before each commit -- see the file header comment.
            const uint64_t pre_mask_q_ = mask_q_;
            const bool pre_done_q_ = done_q_;
            const bool pre_til_done_q_ = til_done_q_;
            const bool pre_trans_q_ = transition_flag_q_;
#endif
            done_q_ = done_q_next;
            til_done_q_ = til_done_q_next;

            // --- transition_flag_q (RTL lines 182-193) ---
            if (in.cnt_en && !in.cnt_clear)
                transition_flag_q_ = transition_flag_d;
            else
                transition_flag_q_ = false;

            // --- gen_reg (RTL lines 269-292): sram_addr_q, mask_q ---
            if (in.cnt_clear || !in.cnt_en)
            {
                sram_addr_q_ = 0;
                mask_q_ = 0;
            }
            else if (in.cnt_en)
            {
                sram_addr_q_ = (uint32_t)sram_addr_d;
                mask_q_ = mask_d;
            }

            // --- out_shimming_reg (RTL lines 298-321) ---
            if (in.cnt_clear || !in.cnt_en)
            {
                mask_outshim_ = 0;
                done_outshim_ = false;
                til_done_outshim_ = false;
                wr_fifo_pop_outshim_ = false;
            }
            else if (in.cnt_en)
            {
#ifdef FX1_A3_IDXCNT_SHIM_PRETICK
                mask_outshim_ = pre_mask_q_;
                done_outshim_ = pre_done_q_;
                til_done_outshim_ = pre_til_done_q_;
                wr_fifo_pop_outshim_ = pre_trans_q_ && in.wr_flag;
#else
                mask_outshim_ = mask_q_;
                done_outshim_ = done_q_;
                til_done_outshim_ = til_done_q_;
                wr_fifo_pop_outshim_ = transition_flag_q_ && in.wr_flag;
#endif
            }

            return out;
        }

    private:
        // cnt_generic.sv, ported inline (matches ifmap_idxcnt.h's established
        // convention of inlining cnt_generic rather than a separate reusable
        // class) -- but UNLIKE ifmap_idxcnt.h's `ov()` helper, o_flag here is
        // NOT computed independently of i_clear: RTL's always_comb genuinely
        // suppresses o_flag to 0 whenever i_clear is asserted (see class-level
        // comment above for why this matters for THIS module specifically).
        static void generic_counter(uint32_t cnt_q, uint32_t step, uint32_t lim,
                                     bool clear, bool &flag, uint32_t &next)
        {
            if (clear)
            {
                flag = false;
                next = 0;
                return;
            }
            uint32_t candidate = cnt_q + step;
            if (candidate >= lim)
            {
                flag = true;
                next = 0;
            }
            else
            {
                flag = false;
                next = candidate;
            }
        }

        // cnt_dualctx.sv, ported inline. sel picks which of the two
        // accumulators (q1/q2) participates in THIS cycle's add/overflow AND
        // which one o_cnt reads (cnt_dualctx.sv:115, `assign o_cnt = (i_sel)
        // ? cnt_q1 : cnt_q2;` -- the CURRENT/pre-tick value of whichever
        // register i_sel currently selects). Both next-state values are
        // still computed/returned so the caller can commit both registers
        // unconditionally-on-enable, matching RTL's own `cnt_q1<=cnt_d1;
        // cnt_q2<=cnt_d2;` (both committed together whenever i_en||i_clear,
        // regardless of which one sel picked this cycle).
        static void dualctx_counter(uint32_t q1, uint32_t q2, bool sel,
                                     uint32_t step, uint32_t lim, bool clear,
                                     bool &flag, uint32_t &next1, uint32_t &next2)
        {
            next1 = q1;
            next2 = q2;
            flag = false;
            if (clear)
            {
                next1 = 0;
                next2 = 0;
                return;
            }
            uint32_t sum_insel = sel ? q1 : q2;
            uint32_t sum_outsel = sum_insel + step;
            if (sum_outsel >= lim)
            {
                flag = true;
                sum_outsel = 0;
            }
            if (sel)
                next1 = sum_outsel;
            else
                next2 = sum_outsel;
        }

        static constexpr int clog2(int n)
        {
            int bits = 0;
            int v = 1;
            while (v < n)
            {
                v <<= 1;
                bits++;
            }
            return bits;
        }

        static constexpr int WOFS_W_ = clog2(SRAMC_N);

        uint32_t x_q_{0};
        uint32_t k_q_{0};
        uint32_t til_xy_q1_{0}, til_xy_q2_{0};
        uint32_t til_k_q1_{0}, til_k_q2_{0};
        bool done_q_{false};
        bool til_done_q_{false};

        // additions:
        bool transition_flag_q_{false};
        uint32_t sram_addr_q_{0};
        uint64_t mask_q_{0};
        bool done_outshim_{false};
        bool til_done_outshim_{false};
        uint64_t mask_outshim_{0};
        bool wr_fifo_pop_outshim_{false};
    };

} // namespace sauria_rtl
