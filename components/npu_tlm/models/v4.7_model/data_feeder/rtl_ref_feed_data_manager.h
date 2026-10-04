#pragma once
//
// 1:1 port of RTL sauria_core/data_feeder/feed_data_manager.sv. Every block below is traceable to a specific RTL
// always_comb/always_ff block; do not "clean up" or reinterpret the logic without re-checking against the
// RTL source (port 1:1, do not re-derive).
//
// Structure: tick() has two phases. Phase 1 computes EVERY combinational value (both
// this cycle's outputs AND every register's next-state "_next") purely from pre-tick
// register state + this cycle's inputs -- mirroring how SystemVerilog always_comb
// blocks all see the same pre-edge register values regardless of textual order (C++
// has no such automatic reordering, so this must be done explicitly). Phase 2 commits
// all "_next" values into the registers, as if a single clock edge fired.
//
// Known non-obvious RTL detail preserved here: read_ptr_q's clock-enable is
// `i_feeder_en && !i_fifo_full` -- it does NOT require i_update, unlike every other
// register in this module (valid_data_q1, woffs_init_q, shift_idx_cnt_q,
// shifted_dil_pat_q all gate on `i_feeder_en && i_update && !i_fifo_full`). This is
// what lets read_ptr_q keep advancing through the SAME SRAM word across multiple stall
// cycles while the other registers hold still until a new word arrives.
//
// Bit-ordering convention for dil_pat: RTL declares `input logic [0:DILP_W-1] i_Dil_pat`
// (ascending range -> i_Dil_pat[0] is the first/leftmost bit). This port stores the
// pattern as a plain uint64_t where bit (DILP_W-1-k) of the uint64_t corresponds to
// RTL's i_Dil_pat[k] (i.e. treat the uint64_t as a DILP_W-bit big-endian bit-string,
// MSB = i_Dil_pat[0]). Callers constructing Inputs::dil_pat MUST follow this convention.

#include <array>
#include <cstddef>
#include <cstdint>

namespace sauria_rtl
{
using namespace sauria; // A1 port: shared vector/mask types (act_vector_t, wei_vector_t, psum_vector_t, sramc_mask_t, host_data_t, ...) stay in ::sauria, only the class bodies move to ::sauria_rtl. See rtl_ref_context_switch_controller.h header comment for the general namespace rationale.

    // I_W: per-element data width (bits). SRAM_W: SRAM word width (bits, must be a
    // multiple of I_W). DILP_W: dilation/kernel pattern width (bits). M: number of
    // intermediate Feed Registers per lane (sauria_pkg.sv `M`, = 3 for int8_32x32
    // (RTL/hw_versions/int8_32x32.svh), NOT the unrelated
    // Python HOPTS["M"] approximate-computing field). WOFS_W/PARAMS_W: counter widths,
    // default to RTL's own defaults.
    template <int I_W, int SRAM_W, int DILP_W, int M, int WOFS_W = 3, int PARAMS_W = 8>
    class FeedDataManager
    {
    public:
        static constexpr int SRAM_N = SRAM_W / I_W; // RTL localparam SRAM_N (L71)
        static_assert(SRAM_W % I_W == 0, "SRAM_W must be a multiple of I_W");
        static_assert(SRAM_N >= 1 && SRAM_N <= 64, "SRAM_N must fit uint64_t bit ops");
        static_assert(M >= 1 && M <= 32, "M must fit uint32_t active-mask ops");
        static_assert(DILP_W >= 1 && DILP_W <= 64, "DILP_W must fit uint64_t");

        using elem_t = int32_t; // widened container; real element is I_W-bit signed

        struct Inputs
        {
            std::array<elem_t, SRAM_N> sram_data{}; // i_sram_data, pre-split (RTL sram_elements, L80/L140)
            bool feeder_en = false;                 // i_feeder_en
            bool update = false;                    // i_update
            bool clearbuff = false;                 // i_clearbuff
            bool valid_data = false;                // i_valid_data
            bool fifo_full = false;                 // i_fifo_full
            bool x_ov_flag = false;                 // i_x_ov_flag
            uint32_t glob_woffs = 0;                // i_glob_woffs (WOFS_W bits)
            uint32_t loc_woffs = 0;                 // i_loc_woffs (PARAMS_W bits)
            uint64_t dil_pat = 0;                   // i_Dil_pat (DILP_W bits, see bit-order note above)
            bool finalpush = false;                 // i_finalpush
        };

        struct Outputs
        {
            bool fifo_push = false;           // o_fifo_push
            bool stall = false;               // o_stall
            std::array<elem_t, M> fifo_din{}; // o_fifo_din (RTL regs_obus, L123/L148)
        };

        void reset()
        {
            valid_data_q1_ = false;
            woffs_init_q_ = 0;
            shift_idx_cnt_q_ = 0;
            shifted_dil_pat_q_.fill(false);
            read_ptr_q_ = 0;
            regs_active_q_.fill(false);
            regs_q_.fill(0);
        }

        Outputs tick(const Inputs &in)
        {
            // =========================================================
            // PHASE 1 — combinational logic, using ONLY pre-tick register
            // state (the _q_ members) and `in`. Computes this cycle's
            // outputs plus every register's next value ("*_next").
            // =========================================================

            const bool pipeline_regs_en = in.feeder_en && in.update && !in.fifo_full; // RTL L130

            // ---- FIFO push logic & Free registers (RTL L521-545) ----
            uint32_t n_free_regs = 0;
            for (int j = 0; j < M; j++)
                if (!regs_active_q_[j]) n_free_regs++;

            bool fifo_push = false;
            if (!in.fifo_full && in.feeder_en)
            {
                if (n_free_regs == 0)
                {
                    fifo_push = true;
                    n_free_regs = M; // RTL: "when we issue a push, registers become free for the next cycle"
                }
            }

            // ---- Final dilation pattern (RTL L322-334) ----
            std::array<bool, SRAM_N> final_dil_pat{};
            for (int i = 0; i < SRAM_N; i++)
                final_dil_pat[i] = (read_ptr_q_ <= (uint32_t)i) ? shifted_dil_pat_q_[i] : false;

            // ---- Element Index Array generation (RTL L339-368) ----
            std::array<uint32_t, SRAM_N> elm_idx_array{};
            std::array<uint32_t, SRAM_N> par_sums{};
            for (int i = 0; i < SRAM_N; i++)
            {
                if (i == 0)
                {
                    elm_idx_array[0] = 0;
                    par_sums[0] = final_dil_pat[0] ? 1u : 0u;
                }
                else if (final_dil_pat[i])
                {
                    elm_idx_array[i] = par_sums[i - 1];
                    par_sums[i] = par_sums[i - 1] + 1;
                }
                else
                {
                    elm_idx_array[i] = 0;
                    par_sums[i] = par_sums[i - 1];
                }
            }
            const uint32_t elm_number = par_sums[SRAM_N - 1];

            // ---- Stall output generation (RTL L373) ----
            // computed through peek_stall() so tick() and peek_stall() can never disagree (same expression).
            const bool stall_out = peek_stall(in.feeder_en, in.fifo_full, in.finalpush);

            // ---- Registers Used Index (RTL L389-396) ----
            uint32_t regs_used_idx = 0;
            for (int i = 1; i < M; i++)
                if (!regs_active_q_[i] && regs_active_q_[i - 1]) regs_used_idx = i;

            // ---- elm_number_sat / new_active_idx / regs_active_new (RTL L397-412) ----
            const uint32_t elm_number_sat = (elm_number > n_free_regs) ? n_free_regs : elm_number;
            const uint32_t new_active_idx = regs_used_idx + elm_number_sat;
            std::array<bool, M> regs_active_new{};
            for (int b = 0; b < M; b++)
                regs_active_new[b] = (b >= (int)regs_used_idx) && (b < (int)new_active_idx);

            // ---- Register and status maintenance (RTL L418-441) ----
            std::array<bool, M> regs_active_next = regs_active_q_; // default: hold
            std::array<bool, M> regs_en_next{};                    // default: all 0
            if (in.clearbuff)
            {
                regs_active_next.fill(false);
            }
            else if (in.feeder_en && !in.fifo_full)
            {
                if (fifo_push) regs_active_next.fill(false); // fifo_push forces all positions to zero
                regs_en_next = regs_active_new;
                for (int b = 0; b < M; b++)
                    regs_active_next[b] = regs_active_next[b] || regs_active_new[b];
            }

            // ---- Generation of multiplexor control signals (RTL L447-478) ----
            std::array<uint32_t, SRAM_N> target_array{};
            std::array<uint32_t, M> mux_control_array{};
            uint32_t last_rd = 0;
            for (int i = 0; i < SRAM_N; i++)
            {
                if (final_dil_pat[i])
                {
                    target_array[i] = elm_idx_array[i] + regs_used_idx + 1; // +1 distinguishes from "unused" zero
                    if (target_array[i] <= (uint32_t)M) last_rd = i;
                }
                else
                {
                    target_array[i] = 0;
                }
            }
            for (int j = 0; j < M; j++)
                for (int i = 0; i < SRAM_N; i++)
                    if (target_array[i] == (uint32_t)(j + 1)) mux_control_array[j] = i;

            // ---- Registers and Muxes: input mux per position (RTL L487-503) ----
            std::array<elem_t, M> regs_d{};
            for (int jj = 0; jj < M; jj++)
            {
                regs_d[jj] = in.sram_data[0]; // default to first SRAM position
                for (int i = 1; i < SRAM_N; i++)
                    if (mux_control_array[jj] == (uint32_t)i) regs_d[jj] = in.sram_data[i];
            }

            // ---- regs_q next-state (RTL L510-517: FF, enabled by regs_en_next) ----
            std::array<elem_t, M> regs_q_next = regs_q_;
            for (int jj = 0; jj < M; jj++)
                if (regs_en_next[jj]) regs_q_next[jj] = regs_d[jj];

            // ---- Valid Data Register next-state (RTL L156-168) ----
            bool valid_data_q1_next = valid_data_q1_;
            if (in.clearbuff) valid_data_q1_next = false;
            else if (pipeline_regs_en) valid_data_q1_next = in.valid_data;

            // ---- Word Offset next-state (RTL L174-188) ----
            const uint32_t woffs_init_d = (in.glob_woffs + in.loc_woffs) & PARAMS_MASK;
            uint32_t woffs_init_q_next = woffs_init_q_;
            if (in.clearbuff) woffs_init_q_next = 0;
            else if (pipeline_regs_en && in.x_ov_flag) woffs_init_q_next = woffs_init_d;

            // ---- Dilated pattern shift index Counter next-state (RTL L194-215) ----
            uint32_t shift_idx_cnt_d;
            if (in.x_ov_flag) shift_idx_cnt_d = 0;
            else shift_idx_cnt_d = (shift_idx_cnt_q_ + SRAM_N) & PARAMS_MASK;
            uint32_t shift_idx_cnt_q_next = shift_idx_cnt_q_;
            if (in.clearbuff) shift_idx_cnt_q_next = 0;
            else if (pipeline_regs_en) shift_idx_cnt_q_next = shift_idx_cnt_d;

            // ---- Final shift index & sign flag (RTL L218-225) ----
            // Uses PRE-tick shift_idx_cnt_q_/woffs_init_q_ (this cycle's values, before
            // the next-state updates above take effect).
            const int32_t dil_shift_idx = (int32_t)shift_idx_cnt_q_ - (int32_t)woffs_init_q_;
            const bool dil_shift_idx_sign = dil_shift_idx < 0;

            // ---- Dilation pattern shifts and muxing (RTL L231-266) ----
            // Gated by valid_data_q1_ (the REGISTER's pre-tick value, not in.valid_data).
            auto dil_bit = [&](int k) -> bool {
                if (k < 0 || k >= DILP_W) return false; // RTL loop bounds never let this happen
                return (in.dil_pat >> (DILP_W - 1 - k)) & 1u;
            };
            std::array<bool, SRAM_N> shifted_dil_pat_d{};
            {
                const int dsi = dil_shift_idx;
                const int dsi_inv = -dsi;
                for (int i = 0; i < SRAM_N; i++)
                {
                    bool rshifted = (dsi_inv > i) ? false : dil_bit(i - dsi_inv);
                    bool lshifted = (dsi > (DILP_W - 1 - i)) ? false : dil_bit(i + dsi);
                    bool sel = dil_shift_idx_sign ? rshifted : lshifted;
                    shifted_dil_pat_d[i] = valid_data_q1_ ? sel : false;
                }
            }
            std::array<bool, SRAM_N> shifted_dil_pat_q_next = shifted_dil_pat_q_;
            if (in.clearbuff) shifted_dil_pat_q_next.fill(false);
            else if (pipeline_regs_en) shifted_dil_pat_q_next = shifted_dil_pat_d;

            // ---- Read pointer counter next-state (RTL L288-309) ----
            uint32_t read_ptr_d;
            if (in.update) read_ptr_d = 0;
            else
            {
                bool any_written = false;
                for (int jj = 0; jj < M; jj++)
                    if (regs_en_next[jj]) { any_written = true; break; }
                read_ptr_d = any_written ? (last_rd + 1) : read_ptr_q_;
            }
            uint32_t read_ptr_q_next = read_ptr_q_;
            if (in.feeder_en && !in.fifo_full) read_ptr_q_next = read_ptr_d & PARAMS_MASK;

            // ---- Output management (RTL L557-560) ----
            // o_fifo_din reads regs_q (the register's PRE-tick/this-cycle value).
            Outputs out;
            out.fifo_push = fifo_push;
            out.stall = stall_out;
            // diagnostics: capture the COMBINATIONAL values too.
            dbg_elm_number_ = elm_number;
            dbg_n_free_ = n_free_regs;
            dbg_pipe_en_ = pipeline_regs_en;
            out.fifo_din = regs_q_;

            // =========================================================
            // PHASE 2 — commit all next-state values, as one clock edge.
            // =========================================================
            valid_data_q1_ = valid_data_q1_next;
            woffs_init_q_ = woffs_init_q_next;
            shift_idx_cnt_q_ = shift_idx_cnt_q_next;
            shifted_dil_pat_q_ = shifted_dil_pat_q_next;
            read_ptr_q_ = read_ptr_q_next;
            regs_active_q_ = regs_active_next;
            regs_q_ = regs_q_next;

            return out;
        }

    public:
        // DIAGNOSTICS ONLY -- const accessors, additive, zero behaviour change.
        // Added to locate why o_fifo_push never fires in the real wiring while the
        // SAME module pushes 37/40 cycles in isolation.
        uint32_t dbg_elm_number() const { return dbg_elm_number_; }
        uint32_t dbg_n_free() const { return dbg_n_free_; }
        bool dbg_pipe_en() const { return dbg_pipe_en_; }
        bool dbg_valid_q1() const { return valid_data_q1_; }
        uint32_t dbg_woffs_init() const { return woffs_init_q_; }
        uint32_t dbg_shift_idx() const { return shift_idx_cnt_q_; }
        uint32_t dbg_read_ptr() const { return read_ptr_q_; }
        uint32_t dbg_pat_popcount() const
        {
            uint32_t n = 0;
            for (int i = 0; i < SRAM_N; i++) if (shifted_dil_pat_q_[i]) n++;
            return n;
        }
        int dbg_pat_first() const
        {
            for (int i = 0; i < SRAM_N; i++) if (shifted_dil_pat_q_[i]) return i;
            return -1;
        }
        uint32_t dbg_regs_active() const
        {
            uint32_t m = 0;
            for (int i = 0; i < M; i++) if (regs_active_q_[i]) m |= (1u << i);
            return m;
        }

        // o_stall of THIS cycle, computed before tick() from pre-tick registers + three inputs only, so
        // ifmap/wei_feeder_rtl can reduce stall_any in the same cycle as the RTL instead of using stall_prev_.
        // RTL feed_data_manager.sv L521-545 (n_free_regs / fifo_push) and L373 (o_stall). It does not read
        // valid_data / update / sram_data / dil_pat, so there is no combinational loop with feeders_update or valid_data.
        bool peek_stall(bool feeder_en, bool fifo_full, bool finalpush) const
        {
            uint32_t n_free_regs = 0;
            for (int j = 0; j < M; j++)
                if (!regs_active_q_[j]) n_free_regs++;

            if (!fifo_full && feeder_en && n_free_regs == 0)
                n_free_regs = M; // RTL: push frees the registers for next cycle

            uint32_t running = 0;
            for (int i = 0; i < SRAM_N; i++)
            {
                const bool bit = (read_ptr_q_ <= (uint32_t)i) ? shifted_dil_pat_q_[i] : false;
                if (bit) running++;
            }
            return (running > n_free_regs) && !finalpush;
        }

    private:
        static constexpr uint32_t PARAMS_MASK = (PARAMS_W >= 32) ? 0xFFFFFFFFu : ((1u << PARAMS_W) - 1u);

        // Registers (RTL flip-flops)
        uint32_t dbg_elm_number_ = 0, dbg_n_free_ = 0;
        bool dbg_pipe_en_ = false;
        bool valid_data_q1_ = false;
        uint32_t woffs_init_q_ = 0;
        uint32_t shift_idx_cnt_q_ = 0;
        std::array<bool, SRAM_N> shifted_dil_pat_q_{};
        uint32_t read_ptr_q_ = 0;
        std::array<bool, M> regs_active_q_{};
        std::array<elem_t, M> regs_q_{};
    };

} // namespace sauria_rtl
