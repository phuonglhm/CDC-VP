#pragma once
//
// 1:1 port of RTL common/fifo_memory_ff.sv (instantiated directly by feed_xy_lane.sv).
//
// WIDE-IN / NARROW-OUT FIFO: pushes are IN_W bits, pops are OUT_W bits, so each
// pushed word is drained over N_ELEMENTS = IN_W/OUT_W pops. The pointer only
// decrements once the last sub-element of the bottom word has been popped
// (down_flag), which is what keeps push and pop on different granularities.
//
// NON-OBVIOUS RTL DETAILS PRESERVED (do not "simplify"):
//  1. `empty` uses (ptr_q == 0) || empty_start_q -- the commented-out alternative
//     in the RTL (`(ptr_q<=1) && (out_woffs==N_ELEMENTS-1)`) is NOT active, and
//     o_empty exports `empty`, NOT the `empty_public & empty` that is also
//     commented out. Three dead alternatives sit in that file; the live ones are
//     ported here and the dead ones are deliberately absent.
//  2. empty_start_q resets to 1 (RTL's async reset sets it 1, not 0) and is only
//     cleared by the first push -- so a freshly cleared FIFO reads empty even
//     though ptr_q is also 0.
//  3. The register chain shifts ALL positions when down_flag fires (reg_en = '1),
//     independently of push; on a simultaneous push+down_flag the incoming word
//     is steered one position further (mux_sel[i+1]) to land correctly after the
//     shift.
//  4. ptr_q's register has NO enable and NO clear branch -- i_clearfifo is folded
//     into ptr_d combinationally instead.
//  5. outbuf_q / empty_public update only on i_pop (i_pop acts as read enable).
//
#include <array>
#include <cstdint>

// FIFO output: RTL `assign o_dout = outbuf_q` gives the value from BEFORE the clock edge. Reading after commit would be
// one cycle early (the second empty slot at start-up would be missing). On by default here, not in the defaults
// header (this file does not include it). Off: -DFX1_A3_FIFO_DOUT_PREV_OFF
#ifndef FX1_A3_FIFO_DOUT_PREV_OFF
#define FX1_A3_FIFO_DOUT_PREV 1
#endif

namespace sauria_rtl
{
using namespace sauria; // A1 port: shared vector/mask types (act_vector_t, wei_vector_t, psum_vector_t, sramc_mask_t, host_data_t, ...) stay in ::sauria, only the class bodies move to ::sauria_rtl. See rtl_ref_context_switch_controller.h header comment for the general namespace rationale.

    template <int FIFO_POSITIONS = 8, int N_ELEMENTS = 3>
    class FifoMemoryFf
    {
    public:
        using elem_t = int32_t;
        // One FIFO slot holds a full pushed word = N_ELEMENTS output elements.
        using word_t = std::array<elem_t, N_ELEMENTS>;

        struct Inputs
        {
            word_t din{};           // i_din (already split into N_ELEMENTS lanes)
            bool push = false;      // i_push
            bool pop = false;       // i_pop
            bool clearfifo = false; // i_clearfifo
        };

        struct Outputs
        {
            bool full = false;   // o_full
            bool empty = false;  // o_empty
            elem_t dout = 0;     // o_dout
        };

        void reset()
        {
            for (auto &w : data_q_)
                w.fill(0);
            ptr_q_ = 0;
            out_woffs_ = 0;
            empty_start_q_ = true; // RTL: async reset sets this to 1
            outbuf_q_ = 0;
            empty_public_ = false;
        }

        Outputs tick(const Inputs &in)
        {
            // ---------- Phase 1: combinational from pre-tick state ----------
            const bool full = (ptr_q_ == (uint32_t)FIFO_POSITIONS);
            const bool empty = (ptr_q_ == 0) || empty_start_q_;

            const bool down_flag =
                (out_woffs_ == (uint32_t)(N_ELEMENTS - 1)) && in.pop && (!empty);

            // --- pointer up/down counter (RTL 168-188) ---
            uint32_t ptr_d;
            if (in.clearfifo)
            {
                ptr_d = 0;
            }
            else
            {
                ptr_d = ptr_q_;
                if (in.push && (!down_flag) && (!full))
                    ptr_d = ptr_q_ + 1;
                if ((!in.push) && down_flag)
                    ptr_d = ptr_q_ - 1;
            }

            // --- register chain control (RTL 202-235) ---
            std::array<bool, FIFO_POSITIONS> mux_sel{};
            std::array<bool, FIFO_POSITIONS> reg_en{};
            for (int i = 0; i < FIFO_POSITIONS; i++)
            {
                if (ptr_q_ == (uint32_t)(FIFO_POSITIONS - 1 - i) && in.push)
                {
                    if (down_flag && (i < FIFO_POSITIONS - 1))
                        mux_sel[i + 1] = true;
                    else
                        mux_sel[i] = true;
                }
            }
            for (int i = 0; i < FIFO_POSITIONS; i++)
                if (mux_sel[i] && (!full))
                    reg_en[i] = true;
            if (down_flag)
                reg_en.fill(true);

            // --- chain input muxes (RTL 108-130) ---
            std::array<word_t, FIFO_POSITIONS> data_mux{};
            for (int i = 0; i < FIFO_POSITIONS; i++)
            {
                if (i == 0)
                {
                    if (mux_sel[0])
                        data_mux[0] = in.din;
                    else
                        data_mux[0].fill(0);
                }
                else
                {
                    data_mux[i] = mux_sel[i] ? in.din : data_q_[i - 1];
                }
            }

            // --- empty-start flag (RTL 240-268) ---
            bool empty_start_d;
            if (in.clearfifo)
                empty_start_d = true;
            else
            {
                empty_start_d = empty_start_q_;
                if (in.push)
                    empty_start_d = false;
            }

            // --- output mux (RTL 279-300): always off the LAST chain slot ---
            elem_t outbuf_d = 0;
            for (int i = 0; i < N_ELEMENTS; i++)
                if (out_woffs_ == (uint32_t)i)
                    outbuf_d = data_q_[FIFO_POSITIONS - 1][i];

            // --- output word-offset counter (RTL 141-160) ---
            uint32_t woffs_next = out_woffs_;
            if (in.clearfifo)
                woffs_next = 0;
            else if (in.pop && (!empty))
                woffs_next = (out_woffs_ == (uint32_t)(N_ELEMENTS - 1)) ? 0u : out_woffs_ + 1u;

            // ---------- Phase 2: commit ----------
            std::array<word_t, FIFO_POSITIONS> next = data_q_;
            for (int i = 0; i < FIFO_POSITIONS; i++)
            {
                if (in.clearfifo)
                    next[i].fill(0);
                else if (reg_en[i])
                    next[i] = data_mux[i];
            }
            data_q_ = next;

            ptr_q_ = ptr_d;              // no enable, no clear branch (see note 4)
            out_woffs_ = woffs_next;
            empty_start_q_ = empty_start_d;

#ifdef FX1_A3_FIFO_DOUT_PREV
            // RTL `assign o_dout = outbuf_q` -> value BEFORE the clock edge.
            const elem_t dout_prev_ = outbuf_q_;
#endif
            if (in.clearfifo)
            {
                outbuf_q_ = 0;
                empty_public_ = true;
            }
            else if (in.pop) // i_pop acts as read enable
            {
                outbuf_q_ = outbuf_d;
                empty_public_ = empty;
            }

            Outputs out;
            out.full = full;
            out.empty = empty;   // RTL exports `empty`, NOT empty_public & empty
#ifdef FX1_A3_FIFO_DOUT_PREV
            out.dout = dout_prev_;
#else
            out.dout = outbuf_q_;
#endif
            return out;
        }

        // full / empty of THIS cycle, read before tick(). Same expression as Phase 1 of tick() (pre-tick state), so by
        // definition equal to what tick() is about to return.
        bool peek_full() const { return ptr_q_ == (uint32_t)FIFO_POSITIONS; }
        bool peek_empty() const { return (ptr_q_ == 0) || empty_start_q_; }
        // Debug hooks (read-only).
        uint32_t dbg_ptr() const { return ptr_q_; }
        // Debug hooks (read-only): ptr_q_ and empty_start_q_ separately (peek_empty() combines both).
        bool dbg_empty_start() const { return empty_start_q_; }

    private:
        std::array<word_t, FIFO_POSITIONS> data_q_{};
        uint32_t ptr_q_{0};
        uint32_t out_woffs_{0};
        bool empty_start_q_{true};
        elem_t outbuf_q_{0};
        bool empty_public_{false};
    };

} // namespace sauria_rtl
