#pragma once
//
// 1:1 STRUCTURAL port of RTL sauria_core/data_feeder/ifmap_feeder.sv.
//
// Companion to wei_feeder_rtl.h. The topology is the same shape:
//     ifmap_idxcnt -> sram_addr + glob_woffs + x_ov_flag + outbounds + done
//     Y x feed_xy_lane (M=3) -> per-row dilation gather + FIFO
//     Y x feed_registers (N_REGS = row index) -> the y-cycle skew line
//     reductions -> fifo_empty_any / fifo_full_any / stall_any
//
// Both feeders must use the RTL structure together: incnt in context_switch_controller is gated by
// pipeline_en && pop_shim_q2, derived from wei_pop_en && act_pop_en, so the two pop_en must stay in phase.
//
// FIVE DIFFERENCES FROM wei_feeder.sv (read from the RTL):
//   1. i_rows_active[jj] gates each lane's i_feeder_en AND overrides that
//      lane's fifo_empty in the reduction (RTL 271, 322-330). wei uses
//      cols_active the same way.
//   2. loc_woffs arrives as a PORT (config register o_loc_woffs, per-row word
//      offset encoding stride: lwoffs[y] = y*s). wei hardcodes loc_woffs[x]=x.
//   3. x_transition_flag = x_ov_flag_q | start_q | i_finalpush  (RTL 196) --
//      uses start_q, a registered i_start. wei instead uses !transition_q2.
//   4. Dil_pat_mux = finalpush_q1 ? ALL ONES : i_Dil_pat  (RTL 313). Note the
//      RTL default is 1, i.e. every bit set, NOT zero.
//   5. M=3 (real dilation gather) vs wei's "M fix to 1".
// There is no waligned / aux / transn logic here, so in that respect this
// feeder is SIMPLER than the weight one.
//
// Per-lane o_stall in the reduction: the PREVIOUS cycle's value by default; FX1_A3_STALL_SAME_CYCLE uses this
// cycle's value through feed_data_manager.h's peek (same as wei_feeder_rtl.h).
//
#include "data_feeder/rtl_ref_feed_xy_lane.h"
#include "data_feeder/rtl_ref_ifmap_idxcnt.h"

#include <array>
#include <cstdint>

// Diagnostic knob (default 3 = RTL): change M from the command line. NOT RTL-faithful when != 3.
#ifndef FX1_A3_M_DIAG
#define FX1_A3_M_DIAG 3
#endif

namespace sauria_rtl
{
using namespace sauria; // A1 port: shared vector/mask types (act_vector_t, wei_vector_t, psum_vector_t, sramc_mask_t, host_data_t, ...) stay in ::sauria, only the class bodies move to ::sauria_rtl. See rtl_ref_context_switch_controller.h header comment for the general namespace rationale.

    template <int Y_DIM, typename T_ACT, int SRAMA_CAP, int FIFO_DEPTH,
              int IDX_W = 17, int DILP_W = 64, int M_GATHER = FX1_A3_M_DIAG>
    class IfmapFeederRtl
    {
    public:
        static constexpr int IA_W = 8 * (int)sizeof(T_ACT);
        static constexpr int SRAMA_W = IA_W * Y_DIM; // sauria_pkg: SRAMA_W = IA_W*Y
        static constexpr int SRAMA_N = Y_DIM;        // SRAMA_W/IA_W

        static constexpr int clog2c(int n)
        {
            int b = 0;
            while ((1 << b) < n)
                b++;
            return b;
        }
        static constexpr int WOFS_W = clog2c(SRAMA_N);
        static constexpr int SRAMA_DEPTH =
            (SRAMA_CAP / (int)(sizeof(T_ACT) * Y_DIM)) > 0
                ? (SRAMA_CAP / (int)(sizeof(T_ACT) * Y_DIM))
                : 1;
        static constexpr int ADRA_W = clog2c(SRAMA_DEPTH);

        using Lane = FeedXyLane<FIFO_DEPTH, IA_W, SRAMA_W, DILP_W, M_GATHER>;
        using Idx = IfmapIdxCnt<IDX_W, ADRA_W, WOFS_W>;
        using elem_t = int32_t;

        struct Inputs
        {
            std::array<elem_t, SRAMA_N> srama_data{}; // i_srama_data
            bool cnt_en = false;
            bool cnt_clear = false;
            bool finalctx = false;
            std::array<bool, Y_DIM> rows_active{};
            uint32_t xlim = 0, xstep = 0;
            uint32_t ylim = 0, ystep = 0;
            uint32_t chlim = 0, chstep = 0;
            uint32_t til_xlim = 0, til_xstep = 0;
            uint32_t til_ylim = 0, til_ystep = 0;
            uint32_t til_x_seed = 0, til_y_seed = 0; // (option C)
            bool feeder_en = false;
            bool feeder_clear = false;
            bool act_valid = false;
            bool start = false;
            bool finalpush = false;
            std::array<uint32_t, Y_DIM> loc_woffs{}; // i_loc_woffs (lwoffs[y]=y*s)
            uint64_t dil_pat = 0;                    // i_Dil_pat
            bool clearfifo = false;
            bool pipeline_en = false;
            bool pop_en = false;
        };

        struct Outputs
        {
            bool done = false;
            bool til_done = false;
            uint32_t srama_addr = 0;
            bool srama_rden = false;
            bool fifo_empty = false;
            bool fifo_full = false;
            bool feeder_stall = false;
            bool act_deadlock = false;
            std::array<elem_t, Y_DIM> a_arr{}; // o_a_arr
            // diagnostics (not RTL ports) -- for the-style audit
            bool dbg_any_push = false;
            uint32_t dbg_glob_woffs = 0;
            // gather-layer observability. Populated in tick(); no logic change.
            bool dbg_valid_data = false;
            bool dbg_outbounds = false;
            bool dbg_stall_any = false;
            int dbg_first_stall = -1;   // first lane that stalled (-1 = none)
            int dbg_n_stall = 0;        // number of stalled lanes
            bool dbg_feeders_update = false;
            bool dbg_fifo_full_any = false;
            uint32_t dbg_elm0 = 0;    // lane0 elm_number
            uint32_t dbg_nfree0 = 0;  // lane0 n_free_regs
            bool dbg_vq1_0 = false;   // lane0 valid_data_q1
            uint32_t dbg_patpop0 = 0; // lane0 dilation popcount
            uint32_t dbg_patsum = 0;  // popcount summed over all 32 lanes
            uint32_t dbg_patmax = 0;  // popcount lon nhat trong 32 lane
            uint32_t dbg_rptr0 = 0;   // lane0 read_ptr
            bool dbg_x_transition = false; // x_transition_flag (RTL 196)
            // per-lane distribution of the empty/full flags.
            uint32_t dbg_n_full = 0;      // how many lanes report full
            uint32_t dbg_n_empty = 0;     // how many lanes report empty
            int dbg_first_full = -1;      // lowest lane index reporting full
            int dbg_first_empty = -1;     // lowest lane index reporting empty
            uint32_t dbg_elm_full = 0;    // elm_number of that first full lane
        };

        void reset()
        {
            idx_.reset();
            for (int y = 0; y < Y_DIM; y++)
            {
                lane_[y].reset();
                skew_[y].fill(0);
                stall_prev_[y] = false;
                full_prev_[y] = false;
                empty_prev_[y] = false;
            }
            sram_data_q_.fill(0);
            srama_s2_.fill(0);
#ifdef FX1_A3_RDEN_PHASE_INIT1
            upd_prev_ = true;
#else
            upd_prev_ = false;
#endif
            finalpush_q1_ = finalpush_q2_ = false;
            start_q_ = false;
            valid_q1_ = valid_q2_ = false;
            outbounds_q1_ = outbounds_q2_ = outbounds_q3_ = false;
            x_ov_flag_q_ = false;
            glob_woffs_ = 0;
            outbounds_ = false;
            srama_addr_ = 0;
            done_ = til_done_ = false;
            full_any_shim_ = false;
        }

        // ContextFsm owns the outer context loop in this harness. Clear only
        // status/shim latches at that seam; keep counters, SRAM data and FIFOs.
        void clear_context_flags()
        {
            idx_.clear_context_flags();
            outbounds_ = false;
            til_done_ = false;
            outbounds_q1_ = outbounds_q2_ = outbounds_q3_ = false;
        }

        Outputs tick(const Inputs &in)
        {
            // ---- reductions from the lanes' PRE-tick flags (RTL 322-330) ----
            bool fifo_empty_any = false, fifo_full_any = false, stall_any = false;
#ifdef FX1_A3_LANESTALL
            int ls_first_ = -1, ls_n_ = 0;
#endif
            for (int y = 0; y < Y_DIM; y++)
            {
#ifdef FX1_A3_FIFOFLAG_SAME_CYCLE
                // RTL 319-330: always_comb on the SAME-cycle flags.
                if (lane_[y].peek_empty() && in.rows_active[y])
                    fifo_empty_any = true;
                if (lane_[y].peek_full())
                    fifo_full_any = true;
#else
                if (empty_prev_[y] && in.rows_active[y]) // rows_active overrides
                    fifo_empty_any = true;
                if (full_prev_[y])
                    fifo_full_any = true;
#endif
#ifdef FX1_A3_STALL_SAME_CYCLE
                // RTL 319-330: always_comb on the SAME-cycle o_stall. rows_active does not mask stall (only empty, line 326),
                // but each lane's feeder_en does (line 272).
                if (lane_[y].peek_stall(in.feeder_en && in.rows_active[y], finalpush_q2_))
                {
                    stall_any = true;
#ifdef FX1_A3_LANESTALL
                    // Debug hook (default off): record WHICH lane stalls.
                    if (ls_first_ < 0) ls_first_ = y;
                    ls_n_++;
#endif
                }
#else
                if (stall_prev_[y])
                    stall_any = true;
#endif
            }

            // Debug hook: capture the per-lane distribution HERE, from the same *_prev_ the three aggregates above used
            // (after the lane loop they are already overwritten).
            int snap_n_full = 0, snap_n_empty = 0;
            int snap_first_full = -1, snap_first_empty = -1;
            for (int y = 0; y < Y_DIM; y++)
            {
                if (full_prev_[y])
                {
                    snap_n_full++;
                    if (snap_first_full < 0) snap_first_full = y;
                }
                if (empty_prev_[y] && in.rows_active[y])
                {
                    snap_n_empty++;
                    if (snap_first_empty < 0) snap_first_empty = y;
                }
            }
            const uint32_t snap_rptr0 = lane_[0].dbg_fdm().dbg_read_ptr();

            // ---- enables (RTL 130-132) ----
            const bool pipeline_regs_en = in.feeder_en && !fifo_full_any && !stall_any;
            const bool cnt_en = in.cnt_en && !fifo_full_any && !stall_any;
            const bool feeders_update = !fifo_full_any && !stall_any;

            // ---- valid_data (RTL 172) ----
            const bool valid_data =
                (in.act_valid && !outbounds_q1_) || in.finalpush || stall_any;

            // ---- index counter (RTL 219-247) ----
            typename Idx::Inputs ii;
            ii.cnt_en = cnt_en;
            ii.cnt_clear = in.cnt_clear;
            ii.finalctx = in.finalctx;
            ii.xlim = in.xlim;
            ii.xstep = in.xstep;
            ii.ylim = in.ylim;
            ii.ystep = in.ystep;
            ii.chlim = in.chlim;
            ii.chstep = in.chstep;
            ii.til_xlim = in.til_xlim;
            ii.til_xstep = in.til_xstep;
            ii.til_ylim = in.til_ylim;
            ii.til_ystep = in.til_ystep;
            ii.til_x_seed = in.til_x_seed;
            ii.til_y_seed = in.til_y_seed;
#ifdef FX1_A3_TILDONE_Q_GATE
            // observer: RTL o_til_done of THIS cycle (ifmap_idxcnt.sv:274), taken
            // from the registered til_done_q before idx_.tick() commits.
            dbg_til_done_rtl_ = idx_.peek_til_done_q() && cnt_en;
            dbg_cnt_en_ = cnt_en;
            dbg_stall_any_ = stall_any;
            dbg_finalpush_ = in.finalpush;
#endif
            auto io_ = idx_.tick(ii);
            srama_addr_ = io_.sram_addr;
            glob_woffs_ = io_.woffs;
            outbounds_ = io_.outbounds;
            done_ = io_.done;
            til_done_ = io_.til_done;
            const bool x_ov_flag_d = io_.x_ov_flag;

            // ---- x transition flag (RTL 196) -- uses the REGISTERED flag ----
            const bool x_transition_flag = x_ov_flag_q_ || start_q_ || in.finalpush;

            // ---- muxes (RTL 310-313), all off PRE-tick register values ----
            std::array<elem_t, SRAMA_N> srama_data_mux{};
            if (!finalpush_q2_)
                srama_data_mux = sram_data_q_;
            const uint32_t glob_woffs_mux = in.finalpush ? 0u : glob_woffs_;
            // RTL '{default: 1} == every bit set, NOT zero.
            const uint64_t dil_pat_mux = finalpush_q1_ ? ~0ULL : in.dil_pat;

            // ---- lanes (RTL 253-292) ----
            Outputs out;
            for (int y = 0; y < Y_DIM; y++)
            {
                typename Lane::Inputs li;
                li.sram_data = srama_data_mux;
                li.feeder_en = in.feeder_en && in.rows_active[y];
                li.update = feeders_update;
                li.clearbuff = in.feeder_clear;
                li.valid_data = valid_data;
                li.x_ov_flag = x_transition_flag;
                li.glob_woffs = glob_woffs_mux;
                li.loc_woffs = in.finalpush ? 0u : in.loc_woffs[y];
                li.dil_pat = dil_pat_mux;
                li.finalpush = finalpush_q2_;
                li.clearfifo = in.clearfifo;
                li.pipeline_en = in.pipeline_en;
                li.pop_en = in.pop_en;

                auto lo = lane_[y].tick(li);
                if (y == 0) { dbg_push0_ = lo.fifo_push; dbg_empty0_ = lo.fifo_empty;
                              dbg_data0_ = lo.data; dbg_din0_ = lo.din0; }
                if (y == 0) { dbg_din1_ = lo.din1; dbg_din2_ = lo.din2; }
                empty_prev_[y] = lo.fifo_empty;
                full_prev_[y] = lo.fifo_full;
                stall_prev_[y] = lo.stall;
                if (lo.fifo_push)
                    out.dbg_any_push = true;

                // skew line: row y is delayed by y cycles (RTL 283-292).
                // feed_registers is instantiated with N_REGS=jj, a DIFFERENT
                // depth per row, so this needs runtime depth.
                out.a_arr[y] = skew_tick(y, lo.data, in.clearfifo, in.pipeline_en);
            }

            // ---- shim registers (RTL 137-170) ----
            if (in.feeder_clear)
            {
                finalpush_q1_ = finalpush_q2_ = false;
                start_q_ = false;
                valid_q1_ = valid_q2_ = false;
                outbounds_q1_ = outbounds_q2_ = outbounds_q3_ = false;
            }
            else if (pipeline_regs_en)
            {
                finalpush_q2_ = finalpush_q1_;
                finalpush_q1_ = in.finalpush;
                start_q_ = in.start;
                valid_q2_ = valid_q1_;
                valid_q1_ = in.act_valid;
                outbounds_q3_ = outbounds_q2_;
                outbounds_q2_ = outbounds_q1_;
                outbounds_q1_ = outbounds_;
            }

            // ---- x_ov_flag shim register (RTL 181-194) ----
            if (in.feeder_clear)
                x_ov_flag_q_ = false;
            else if (pipeline_regs_en)
                x_ov_flag_q_ = x_ov_flag_d;

#ifdef FX1_A3_ADIN_PROBE
            // Debug hook (default off), symmetric to FX1_A3_WDIN_PROBE: one line per cycle with feeder_en, to check the
            // SRAM-A read path directly (sq0 vs A[addr*32]).
            {
                static std::ofstream ad("trace_sysc/adin.csv");
                static bool ad_hdr = false;
                static unsigned long long ad_n = 0, ad_rows = 0;
                if (!ad_hdr)
                {
                    ad << "n,fen,upd,full,stall,sdata0,sq0,addr,cen,pipe_en,pop_en,"
                          "push0,din0,empty0,data0,din1,din2,elm,nfree,ract\n";
                    ad_hdr = true;
                }
                if (in.feeder_en && ad_rows < 6000)
                {
                    ad_rows++;
                    ad << ad_n << "," << (int)in.feeder_en << ","
                       << (int)feeders_update << "," << (int)fifo_full_any << ","
                       << (int)stall_any << "," << (int)in.srama_data[0] << ","
                       << (int)sram_data_q_[0] << "," << srama_addr_ << ","
                       << (int)cnt_en << "," << (int)in.pipeline_en << ","
                       << (int)in.pop_en << ","
                       << (int)dbg_push0_ << "," << (int)dbg_din0_ << ","
                       << (int)dbg_empty0_ << "," << (int)dbg_data0_ << ","
                       << (int)dbg_din1_ << "," << (int)dbg_din2_ << ","
                       << lane_[0].dbg_fdm().dbg_elm_number() << ","
                       << lane_[0].dbg_fdm().dbg_n_free() << ","
                       << lane_[0].dbg_fdm().dbg_regs_active() << "\n";
                }
                ad_n++;
            }
#endif
            // ---- SRAM data register (RTL 202-215) ----
#ifdef FX1_A3_SRAMA_RDEN_PHASE
            // i_srama_data(t) = mem[A(t-2)] gated by rden(t-1) (not rden(t-2)). Symmetric to SRAM-B.
            if (upd_prev_)
                srama_s2_ = in.srama_data;
            if (in.feeder_clear)
                sram_data_q_.fill(0);
            else if (pipeline_regs_en)
                sram_data_q_ = srama_s2_;
            upd_prev_ = feeders_update;
#else
            if (in.feeder_clear)
                sram_data_q_.fill(0);
            else if (pipeline_regs_en)
                sram_data_q_ = in.srama_data;
#endif

            full_any_shim_ = fifo_full_any; // RTL 336-342 (exists, unread here)

            // ---- outputs (RTL 348-355) ----
            out.done = done_;
            out.til_done = til_done_;
            out.srama_addr = srama_addr_;
            out.srama_rden = feeders_update;
            out.fifo_empty = fifo_empty_any;
            out.fifo_full = fifo_full_any;
            out.feeder_stall = stall_any;
            out.act_deadlock = fifo_empty_any && fifo_full_any;
            out.dbg_glob_woffs = glob_woffs_;
            out.dbg_x_transition = x_transition_flag;
            out.dbg_valid_data = valid_data;
            out.dbg_outbounds = outbounds_;
            out.dbg_stall_any = stall_any;
#ifdef FX1_A3_LANESTALL
            out.dbg_first_stall = ls_first_;
            out.dbg_n_stall = ls_n_;
#endif
            out.dbg_feeders_update = feeders_update;
            out.dbg_fifo_full_any = fifo_full_any;
            {
                // all taken from the snapshot at the START of the tick => same cycle N as fifo_empty / fifo_full / stall_any
                // above.
                out.dbg_n_full = (uint32_t)snap_n_full;
                out.dbg_n_empty = (uint32_t)snap_n_empty;
                out.dbg_first_full = snap_first_full;
                out.dbg_first_empty = snap_first_empty;
                out.dbg_rptr0 = snap_rptr0;
                if (snap_first_full >= 0)
                    out.dbg_elm_full = lane_[snap_first_full].dbg_fdm().dbg_elm_number();

                // The three columns below are combinational values the data manager computed IN this tick => already cycle N;
                // kept as is for later.
                const auto &fdm0 = lane_[0].dbg_fdm();
                out.dbg_elm0 = fdm0.dbg_elm_number();
                out.dbg_nfree0 = fdm0.dbg_n_free();
                out.dbg_vq1_0 = fdm0.dbg_valid_q1();
                out.dbg_patpop0 = fdm0.dbg_pat_popcount();
#ifdef FX1_A3_PATPOP_ALL
                // scan ALL 32 lanes, not only lane 0.
                for (int q = 0; q < Y_DIM; q++)
                {
                    const uint32_t pc = lane_[q].dbg_fdm().dbg_pat_popcount();
                    out.dbg_patsum += pc;
                    if (pc > out.dbg_patmax) out.dbg_patmax = pc;
                }
#endif
            }
#ifdef FX1_A3_UNIFIED_PROBE
            // Debug hook (default off, read-only): self-anchored probe, identical in sauria_model and this port, so the
            // two traces align without external alignment. `probe_seq_` is reset once, on the FIRST cycle with
            // in.start == true (no absolute tsim / cycle -- each tree has its own time origin).
            if (in.start && !probe_seen_start_)
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
                static std::ofstream probe_csv("trace_sysc/unified_probe.csv");
                static bool probe_hdr = false;
                if (!probe_hdr)
                {
                    probe_csv << "seq,tsim,cnt_en,valid_data,stall_any,finalpush,elm0,nfree0,"
                                 "push0,empty0\n";
                    probe_hdr = true;
                }
                probe_csv << probe_seq_ << "," << sc_core::sc_time_stamp().value() << ","
                          << (int)cnt_en << "," << (int)valid_data << "," << (int)stall_any
                          << "," << (int)in.finalpush << "," << out.dbg_elm0 << ","
                          << out.dbg_nfree0 << "," << (int)dbg_push0_ << ","
                          << (int)dbg_empty0_ << "\n";
                probe_csv.flush();
            }
#endif
            return out;
        }

        // per-lane observers for the lane-22 investigation. Additive.
        const Lane &dbg_lane(int y) const { return lane_[y]; }
        // fifo_empty of THIS cycle with the RTL 326 reduction (rows_active masks empty); valid before tick() because
        // lane_[y].peek_empty() reads the pre-tick ptr_q_ -- the combinational value the RTL presents this cycle.
        // stall_any of THIS cycle (RTL ifmap_feeder.sv:328; not masked by rows_active, each lane's feeder_en is, RTL 272).
        bool peek_stall_any(bool feeder_en, const std::array<bool, Y_DIM> &rows_active) const
        {
            for (int y = 0; y < Y_DIM; y++)
                if (lane_[y].peek_stall(feeder_en && rows_active[y], finalpush_q2_))
                    return true;
            return false;
        }

        bool peek_fifo_empty(const std::array<bool, Y_DIM> &rows_active) const
        {
            for (int y = 0; y < Y_DIM; y++)
                if (lane_[y].peek_empty() && rows_active[y])
                    return true;
            return false;
        }

        // FULL flag of THIS cycle, same reduction as in tick(): OR over every lane, not masked by rows_active.
        bool peek_fifo_full() const
        {
            for (int y = 0; y < Y_DIM; y++)
                if (lane_[y].peek_full())
                    return true;
            return false;
        }

#ifdef FX1_A3_TILDONE_Q_GATE
        bool peek_til_done_q() const { return idx_.peek_til_done_q(); }
        bool dbg_til_done_rtl() const { return dbg_til_done_rtl_; }
        bool dbg_cnt_en() const { return dbg_cnt_en_; }
        bool dbg_push0() const { return dbg_push0_; }
        bool dbg_empty0() const { return dbg_empty0_; }
        uint32_t dbg_elm0_now() const { return lane_[0].dbg_fdm().dbg_elm_number(); }
        uint32_t dbg_nfree0_now() const { return lane_[0].dbg_fdm().dbg_n_free(); }
        bool dbg_stall_any() const { return dbg_stall_any_; }
        bool dbg_finalpush() const { return dbg_finalpush_; }
#endif
        bool dbg_empty(int y) const { return empty_prev_[y]; }
        bool dbg_full(int y) const { return full_prev_[y]; }
        // Debug hooks (read-only, no effect on behaviour).
        bool dbg_outbounds_now() const { return outbounds_; }
        bool dbg_outbounds_q1_now() const { return outbounds_q1_; }

    private:
        // Runtime-depth equivalent of feed_registers.sv (N_REGS = row index):
        // o_dout = reg_q[N_REGS]; stages advance on i_pipeline_en, zeroed by
        // i_clear. Depth 0 is a plain wire.
        elem_t skew_tick(int y, elem_t din, bool clear, bool pipeline_en)
        {
            const int depth = y;
            elem_t dout = (depth == 0) ? din : skew_[y][depth - 1];
            if (clear)
            {
                for (int k = 0; k < depth; k++)
                    skew_[y][k] = 0;
            }
            else if (pipeline_en && depth > 0)
            {
                for (int k = depth - 1; k > 0; k--)
                    skew_[y][k] = skew_[y][k - 1];
                skew_[y][0] = din;
            }
            return dout;
        }

        Idx idx_;
        Lane lane_[Y_DIM];
        std::array<std::array<elem_t, (Y_DIM > 0 ? Y_DIM : 1)>, Y_DIM> skew_{};
        std::array<elem_t, SRAMA_N> sram_data_q_{};
        // second stage of the SRAM-A read path (only under FX1_A3_SRAMA_RDEN_PHASE)
        std::array<elem_t, SRAMA_N> srama_s2_{};
        // lane 0 diagnostics (read only under FX1_A3_ADIN_PROBE)
        bool dbg_push0_{false}, dbg_empty0_{true};
        elem_t dbg_data0_{0}, dbg_din0_{0};
        elem_t dbg_din1_{0}, dbg_din2_{0};
#ifdef FX1_A3_RDEN_PHASE_INIT1
        bool upd_prev_{true};
#else
        bool upd_prev_{false};
#endif
        bool finalpush_q1_{false}, finalpush_q2_{false};
        bool start_q_{false};
        bool valid_q1_{false}, valid_q2_{false};
        bool outbounds_q1_{false}, outbounds_q2_{false}, outbounds_q3_{false};
        bool x_ov_flag_q_{false};
        bool stall_prev_[Y_DIM]{}, full_prev_[Y_DIM]{}, empty_prev_[Y_DIM]{};
        uint32_t glob_woffs_{0}, srama_addr_{0};
        bool outbounds_{false}, done_{false}, til_done_{false};
        bool full_any_shim_{false};
#ifdef FX1_A3_TILDONE_Q_GATE
        bool dbg_til_done_rtl_{false};
#endif
        // Debug hooks (read-only): cnt_en / til_done_q_ for comparison with sauria_model traces.
        bool dbg_cnt_en_{false};
        bool dbg_stall_any_{false};
        bool dbg_finalpush_{false};
#ifdef FX1_A3_UNIFIED_PROBE
        bool probe_seen_start_{false};
        long long probe_seq_{0};
#endif
    };

} // namespace sauria_rtl
