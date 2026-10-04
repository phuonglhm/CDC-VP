#pragma once
//
// 1:1 port of RTL sauria_core/data_feeder/feed_xy_lane.sv plus common/feed_registers.sv.
//
// feed_xy_lane is ONE feeder lane: the dilation-gather engine
// (feed_data_manager, already ported as feed_data_manager.h) feeding a
// wide-in/narrow-out FIFO (fifo_memory_ff.h), plus the pop-propagation and
// empty/full shimming that sit between them and the systolic array.
//
// This is the BUFFERING layer: the RTL reads activations once (act_reps = 1) and re-serves them across the contexts
// out of these per-lane FIFOs (a behavioural feeder that re-reads SRAM every context has different timing).
//
// NON-OBVIOUS RTL DETAILS PRESERVED:
//  1. o_data is GATED: `(fifo_pop && !fifo_empty_q2) ? fifo_dout : 0` -- the
//     lane emits ZERO, not stale data, whenever it is not genuinely popping.
//  2. fifo_empty_q1/q2 form a 2-deep shim that advances ONLY on fifo_pop (not
//     every cycle), so it tracks pops rather than time.
//  3. fifo_pop = pop_en_q && i_pipeline_en, where pop_en_q is i_pop_en delayed
//     by one cycle and gated by i_pipeline_en -- i.e. the pop actually applied
//     to the FIFO is one cycle behind the request.
//  4. fifo_full_shim exists in RTL and is registered every cycle unconditionally
//     but is NOT read by anything in feed_xy_lane.sv. Kept and marked [UNUSED]
//     for interface fidelity, same convention the rest of this port uses.
//  5. o_fifo_empty/o_fifo_full expose the RAW fifo flags, not the shimmed ones.
//
#include "rtl_ref_feed_data_manager.h"
#include "rtl_ref_fifo_memory_ff.h"

#include <array>
#include <cstdint>

namespace sauria_rtl
{
using namespace sauria; // A1 port: shared vector/mask types (act_vector_t, wei_vector_t, psum_vector_t, sramc_mask_t, host_data_t, ...) stay in ::sauria, only the class bodies move to ::sauria_rtl. See rtl_ref_context_switch_controller.h header comment for the general namespace rationale.

    // Simple N-deep shift register (RTL feed_registers.sv). reg_q[0] is the
    // input, o_dout is reg_q[N_REGS]; every stage advances on i_pipeline_en and
    // is zeroed by i_clear. N_REGS == 0 degenerates to a wire, matching RTL.
    template <int N_REGS, typename T = int32_t>
    class FeedRegisters
    {
    public:
        void reset() { regs_.fill(T{}); }

        T tick(T din, bool clear, bool pipeline_en)
        {
            T dout = (N_REGS == 0) ? din : regs_[N_REGS - 1];
            if (clear)
            {
                regs_.fill(T{});
            }
            else if (pipeline_en)
            {
                for (int k = N_REGS - 1; k > 0; k--)
                    regs_[k] = regs_[k - 1];
                if (N_REGS > 0)
                    regs_[0] = din;
            }
            return dout;
        }

    private:
        std::array<T, (N_REGS > 0 ? N_REGS : 1)> regs_{};
    };

    // I_W/SRAM_W/DILP_W/M mirror feed_data_manager's parameters.
    template <int FIFO_POSITIONS, int I_W, int SRAM_W, int DILP_W, int M,
              int WOFS_W = 3, int PARAMS_W = 8>
    class FeedXyLane
    {
    public:
        using FDM = FeedDataManager<I_W, SRAM_W, DILP_W, M, WOFS_W, PARAMS_W>;
        using FIFO = FifoMemoryFf<FIFO_POSITIONS, M>;
        using elem_t = typename FDM::elem_t;
        static constexpr int SRAM_N = FDM::SRAM_N;

        struct Inputs
        {
            std::array<elem_t, SRAM_N> sram_data{}; // i_sram_data
            bool feeder_en = false;                 // i_feeder_en
            bool update = false;                    // i_update
            bool clearbuff = false;                 // i_clearbuff
            bool valid_data = false;                // i_valid_data
            bool x_ov_flag = false;                 // i_x_ov_flag
            uint32_t glob_woffs = 0;                // i_glob_woffs
            uint32_t loc_woffs = 0;                 // i_loc_woffs
            uint64_t dil_pat = 0;                   // i_Dil_pat
            bool finalpush = false;                 // i_finalpush
            bool clearfifo = false;                 // i_clearfifo
            bool pipeline_en = false;               // i_pipeline_en
            bool pop_en = false;                    // i_pop_en
        };

        struct Outputs
        {
            bool stall = false;      // o_stall
            bool fifo_empty = false; // o_fifo_empty (RAW flag)
            bool fifo_full = false;  // o_fifo_full  (RAW flag)
            bool fifo_push = false;  // internal: feed_data_manager o_fifo_push (diagnostic)
            elem_t din0 = 0;         // diagnostics: o_fifo_din[0] (= regs_q_ pre-tick)
            elem_t din1 = 0, din2 = 0; // elements 1, 2 (only meaningful when M > 1)
            elem_t data = 0;         // o_data
        };

        void reset()
        {
            fdm_.reset();
            fifo_.reset();
            pop_en_q_ = false;
            fifo_empty_q1_ = false;
            fifo_empty_q2_ = false;
            fifo_full_shim_ = false;
            fifo_full_prev_ = false;
        }

        Outputs tick(const Inputs &in)
        {
            // The data manager needs the FIFO's full flag from THIS cycle; RTL
            // wires fifo_full combinationally from the FIFO instance, so read the
            // pre-tick value the FIFO would be presenting now.
#ifdef FX1_A3_FIFOFLAG_SAME_CYCLE
            // the RTL wires i_fifo_full straight from the FIFO instance (feed_xy_lane.sv:110), i.e. THIS cycle's flag;
            // fifo_full_prev_ is the PREVIOUS cycle's flag.
            const bool fifo_full_now = fifo_.peek_full();
#else
            const bool fifo_full_now = fifo_full_prev_;
#endif

            typename FDM::Inputs fi;
            fi.sram_data = in.sram_data;
            fi.feeder_en = in.feeder_en;
            fi.update = in.update;
            fi.clearbuff = in.clearbuff;
            fi.valid_data = in.valid_data;
            fi.fifo_full = fifo_full_now;
            fi.x_ov_flag = in.x_ov_flag;
            fi.glob_woffs = in.glob_woffs;
            fi.loc_woffs = in.loc_woffs;
            fi.dil_pat = in.dil_pat;
            fi.finalpush = in.finalpush;
            auto fo = fdm_.tick(fi);

            // RTL 158: the pop actually applied is the REGISTERED request.
            const bool fifo_pop = pop_en_q_ && in.pipeline_en;
            dbg_pop_ = fifo_pop;

            typename FIFO::Inputs qi;
            qi.din = fo.fifo_din;
            qi.push = fo.fifo_push;
            qi.pop = fifo_pop;
            qi.clearfifo = in.clearfifo;
            auto qo = fifo_.tick(qi);

            // RTL 198: gated output -- zero unless genuinely popping.
            Outputs out;
            out.data = (fifo_pop && (!fifo_empty_q2_)) ? qo.dout : elem_t{0};
            out.fifo_empty = qo.empty;
            out.fifo_full = qo.full;
            out.stall = fo.stall;
            out.fifo_push = fo.fifo_push;
            out.din0 = fo.fifo_din[0];
            if (M > 1) out.din1 = fo.fifo_din[M > 1 ? 1 : 0];
            if (M > 2) out.din2 = fo.fifo_din[M > 2 ? 2 : 0];

            // --- pop propagation register (RTL 144-156) ---
            if (in.clearfifo)
                pop_en_q_ = false;
            else if (in.pipeline_en)
                pop_en_q_ = in.pop_en;

            // --- empty shim, advances ONLY on fifo_pop (RTL 165-180) ---
            if (in.clearfifo)
            {
                fifo_empty_q1_ = false;
                fifo_empty_q2_ = false;
            }
            else if (fifo_pop)
            {
                fifo_empty_q2_ = fifo_empty_q1_;
                fifo_empty_q1_ = qo.empty;
            }

            // --- full shim (RTL 186-192): registered every cycle, [UNUSED] ---
            fifo_full_shim_ = qo.full;
            fifo_full_prev_ = qo.full;

            return out;
        }

        // o_stall of this cycle. fifo_full_prev_ is the value the FIFO presents at the start of the cycle (see the note
        // in tick()), i.e. exactly what tick() passes to the data manager.
        bool peek_stall(bool feeder_en, bool finalpush) const
        {
#ifdef FX1_A3_FIFOFLAG_SAME_CYCLE
            return fdm_.peek_stall(feeder_en, fifo_.peek_full(), finalpush);
#else
            return fdm_.peek_stall(feeder_en, fifo_full_prev_, finalpush);
#endif
        }

        // this cycle's FIFO flags, so both feeders can reduce them on the right beat.
        bool peek_empty() const { return fifo_.peek_empty(); }
        bool peek_full() const { return fifo_.peek_full(); }
        uint32_t dbg_ptr() const { return fifo_.dbg_ptr(); }
        bool dbg_pop() const { return dbg_pop_; }

        bool dbg_full_shim() const { return fifo_full_shim_; }
        // pass-through to the data manager's internals.
        const FDM &dbg_fdm() const { return fdm_; }

        // Debug hooks (read-only): the empty shim. fifo_empty_q2_ directly decides
        // out.data = (fifo_pop && !fifo_empty_q2_) ? qo.dout : 0.
        bool dbg_fifo_empty_q1() const { return fifo_empty_q1_; }
        bool dbg_fifo_empty_q2() const { return fifo_empty_q2_; }
        uint32_t dbg_fifo_ptr() const { return fifo_.dbg_ptr(); }
        bool dbg_fifo_empty_start() const { return fifo_.dbg_empty_start(); }

    private:
        FDM fdm_;
        FIFO fifo_;
        bool pop_en_q_{false};
        bool fifo_empty_q1_{false};
        bool fifo_empty_q2_{false};
        bool fifo_full_shim_{false};
        bool fifo_full_prev_{false};
        bool dbg_pop_{false};
    };

} // namespace sauria_rtl
