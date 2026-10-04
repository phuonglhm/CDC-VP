#pragma once
//
// 1:1 STRUCTURAL port of RTL sauria_core/data_feeder/wei_feeder.sv.
//
// This is the FEEDER TOP. It wires the ported leaves into the RTL's own topology:
//     wei_idxcnt  -> address + glob_woffs + transn + done/til_done
//     X x feed_xy_lane (M=1) -> per-column gather + FIFO
//     X x feed_registers (N_REGS = column index) -> the x-cycle skew line
//     reductions  -> fifo_empty_any / fifo_full_any / stall_any
// replacing wei_feeder.h's behavioural col_fifos/skew_regs/wei_flat_buf.
//
// The feeder top is structural because an RTL-timed control layer does not combine with a behaviourally timed
// feeder: the seam between them breaks, not an individual module.
//
// EVALUATION ORDER inside tick(): RTL is concurrent; C++ is not. The order below
// is forced by the real dependency graph, which was checked for loops first:
//   - o_stall / o_fifo_full come out of each lane COMBINATIONALLY from register
//     state only (feed_data_manager's o_stall reads regs_active_q and the
//     REGISTERED shifted_dil_pat_q; fifo full reads ptr_q) -- they do NOT depend
//     on i_update, so there is no combinational loop with feeders_update.
//   - Therefore: peek the lanes' pre-tick flags -> derive pipeline_regs_en /
//     cnt_en / feeders_update -> tick idxcnt -> tick lanes -> tick skew regs.
//
// Per-lane o_stall in the reduction: the PREVIOUS cycle's value by default; FX1_A3_STALL_SAME_CYCLE uses this
// cycle's value (feed_data_manager.h peek). Lane stall matters with unaligned weights (i_waligned = 0) or the aux
// counter (auxlim > 1).
//
#include <fstream>
#include "data_feeder/rtl_ref_feed_xy_lane.h"
#include "data_feeder/rtl_ref_wei_idxcnt.h"

#include <array>
#include <cstdint>

namespace sauria_rtl
{
using namespace sauria; // A1 port: shared vector/mask types (act_vector_t, wei_vector_t, psum_vector_t, sramc_mask_t, host_data_t, ...) stay in ::sauria, only the class bodies move to ::sauria_rtl. See rtl_ref_context_switch_controller.h header comment for the general namespace rationale.

    template <int X_DIM, typename T_WEI, int SRAMB_CAP, int FIFO_DEPTH,
              int IDX_W = 17>
    class WeiFeederRtl
    {
    public:
        static constexpr int IB_W = 8 * (int)sizeof(T_WEI);
        static constexpr int SRAMB_W = IB_W * X_DIM; // sauria_pkg: SRAMB_W = IB_W*X
        static constexpr int SRAMB_N = X_DIM;        // SRAMB_W/IB_W
        static constexpr int DILP_W = SRAMB_N;

        static constexpr int clog2c(int n)
        {
            int b = 0;
            while ((1 << b) < n)
                b++;
            return b;
        }
        static constexpr int WOFS_W = clog2c(SRAMB_N);
        static constexpr int SRAMB_DEPTH =
            (SRAMB_CAP / (int)(sizeof(T_WEI) * X_DIM)) > 0
                ? (SRAMB_CAP / (int)(sizeof(T_WEI) * X_DIM))
                : 1;
        static constexpr int ADRB_W = clog2c(SRAMB_DEPTH);

        using Lane = FeedXyLane<FIFO_DEPTH, IB_W, SRAMB_W, DILP_W, /*M=*/1>;
        using Idx = WeiIdxCnt<IDX_W, ADRB_W, WOFS_W>;
        using elem_t = int32_t;

        struct Inputs
        {
            std::array<elem_t, SRAMB_N> sramb_data{}; // i_sramb_data
            bool cnt_en = false;
            bool cnt_clear = false;
            bool cswitch = false;
            std::array<bool, X_DIM> cols_active{};
            bool waligned = false;
            uint32_t auxlim = 0, auxstep = 0;
            uint32_t wlim = 0, wstep = 0;
            uint32_t til_klim = 0, til_kstep = 0;
            bool feeder_en = false;
            bool feeder_clear = false;
            bool wei_valid = false;
            bool finalpush = false;
            bool clearfifo = false;
            bool pipeline_en = false;
            bool pop_en = false;
        };

        struct Outputs
        {
            bool done = false;
            bool til_done = false;
            uint32_t sramb_addr = 0;
            bool sramb_rden = false;
            bool fifo_empty = false;
            bool fifo_full = false;
            bool feeder_stall = false;
            bool wei_deadlock = false;
            std::array<elem_t, X_DIM> b_arr{}; // o_b_arr
        };

        void reset()
        {
            idx_.reset();
            for (int x = 0; x < X_DIM; x++)
            {
                lane_[x].reset();
                skew_[x].fill(0);
                stall_prev_[x] = false;
                full_prev_[x] = false;
                // FIX: a FIFO is EMPTY after reset. Initialising this to
                // false made fifo_empty_any read 0 from cycle 0, i.e. the whole
                // system believed the feeder already had data.
                empty_prev_[x] = true;
            }
            sram_data_q_.fill(0);
            sramb_s2_.fill(0);
#ifdef FX1_A3_RDEN_PHASE_INIT1
            upd_prev_ = true;
#else
            upd_prev_ = false;
#endif
            finalpush_q1_ = finalpush_q2_ = false;
            valid_q1_ = valid_q2_ = false;
            outbounds_q1_ = outbounds_q2_ = outbounds_q3_ = false;
            glob_woffs_ = 0;
            transn_ = false;
            outbounds_ = false;
            sramb_addr_ = 0;
            done_ = til_done_ = false;
            full_any_shim_ = false;
        }

        Outputs tick(const Inputs &in)
        {
            // ---- reductions from the lanes' PRE-tick flags (RTL 295-306) ----
            bool fifo_empty_any = false, fifo_full_any = false, stall_any = false;
            for (int x = 0; x < X_DIM; x++)
            {
#ifdef FX1_A3_FIFOFLAG_SAME_CYCLE
                //, symmetric. The WEI side is the one that needs the extra cnt_en beat most.
                if (lane_[x].peek_empty() && in.cols_active[x])
                    fifo_empty_any = true;
                if (lane_[x].peek_full())
                    fifo_full_any = true;
#else
                if (empty_prev_[x] && in.cols_active[x]) // cols_active overrides
                    fifo_empty_any = true;
                if (full_prev_[x])
                    fifo_full_any = true;
#endif
#ifdef FX1_A3_STALL_SAME_CYCLE
                // symmetric to ifmap_feeder_rtl.h. It matters more on the WEI side, where stall blocks cnt_en and lengthens
                // the wei_til_done loop.
                if (lane_[x].peek_stall(in.feeder_en && in.cols_active[x], finalpush_q2_))
                    stall_any = true;
#else
                if (stall_prev_[x])
                    stall_any = true;
#endif
            }

            // ---- enables (RTL 137-139) ----
            const bool pipeline_regs_en = in.feeder_en && !fifo_full_any && !stall_any;
            const bool cnt_en = in.cnt_en && !fifo_full_any && !stall_any;
            const bool feeders_update = !fifo_full_any && !stall_any;

            // ---- valid_data (RTL 178) ----
            const bool valid_data =
                (in.wei_valid && !outbounds_q1_) || in.finalpush || stall_any;

            // ---- index counter (RTL 204-229) ----
            typename Idx::Inputs ii;
            ii.cnt_en = cnt_en;
            ii.cnt_clear = in.cnt_clear;
            ii.cswitch = in.cswitch;
            ii.waligned = in.waligned;
            ii.auxlim = in.auxlim;
            ii.auxstep = in.auxstep;
            ii.wlim = in.wlim;
            ii.wstep = in.wstep;
            ii.til_klim = in.til_klim;
            ii.til_kstep = in.til_kstep;
            auto io_ = idx_.tick(ii);
#ifdef FX1_A3_WPOP_PROBE
            // Debug hook (default off): one line per POP cycle, to catch the repeated beat.
            {
                static std::ofstream wq("trace_sysc/wpop.csv");
                static bool wq_hdr = false;
                static unsigned long long wq_n = 0, wq_rows = 0;
                if (!wq_hdr)
                {
                    wq << "n,data0,empty0,push0,stall0,w,pipe_en,pop_en,"
                          "patfirst,elm,rptr,woffs,sidx,addr,fen,cen,"
                          "sdata0,sq0\n";
                    wq_hdr = true;
                }
                if (in.pipeline_en && in.pop_en && wq_rows < 4000)
                {
                    wq_rows++;
                    const auto &wq_fd = lane_[0].dbg_fdm();
                    wq << wq_n << "," << (int)dbg_data0_ << ","
                       << (int)dbg_empty0_ << "," << (int)dbg_push0_ << ","
                       << (int)dbg_stall0_ << "," << (unsigned)idx_.dbg_w() << ","
                       << (int)in.pipeline_en << "," << (int)in.pop_en << ","
                       << wq_fd.dbg_pat_first() << "," << wq_fd.dbg_elm_number() << ","
                       << wq_fd.dbg_read_ptr() << "," << wq_fd.dbg_woffs_init() << ","
                       << wq_fd.dbg_shift_idx() << "," << sramb_addr_ << ","
                       << (int)in.feeder_en << "," << (int)in.cnt_en << ","
                       << (int)in.sramb_data[0] << ","
                       << (int)sram_data_q_[0] << "\n";
                }
                wq_n++;
            }
#endif
#ifdef FX1_A3_WSTEP_PROBE
            // Debug hook (default off): counts the real ADDRESS STEPS of the weight counter cascade.
            {
                static std::ofstream ws("trace_sysc/wstep.csv");
                static bool ws_hdr = false;
                static unsigned long long ws_n = 0, ws_chg = 0, ws_rows = 0;
                static unsigned ws_prev = 0xFFFFFFFFu;
                static bool ws_cs_prev = false;
                if (!ws_hdr)
                {
                    ws << "n,w,nchg,cswitch,done,tildone,push,npush,empty0,nempty0\n";
                    ws_hdr = true;
                }
                static unsigned long long ws_push = 0, ws_empty = 0;
                if (dbg_push0_)  ws_push++;   // count PUSH of lane 0
                if (dbg_empty0_) ws_empty++;  // lane 0's OWN empty flag
                const unsigned ws_w = (unsigned)idx_.dbg_w();
                const bool ws_cs_rise = in.cswitch && !ws_cs_prev;
                if (ws_w != ws_prev) ws_chg++;
                if ((ws_w != ws_prev || ws_cs_rise || io_.done || io_.til_done) &&
                    ws_rows < 400000)
                {
                    ws_rows++;
                    ws << ws_n << "," << ws_w << "," << ws_chg << ","
                       << (int)ws_cs_rise << "," << (int)io_.done << ","
                       << (int)io_.til_done << ","
                       << (int)dbg_push0_ << "," << ws_push << ","
                       << (int)dbg_empty0_ << "," << ws_empty << "\n";
                }
                ws_prev = ws_w;
                ws_cs_prev = in.cswitch;
                ws_n++;
            }
#endif
#ifdef FX1_A3_WFEED_PROBE
            // Debug hook (default off): separates the three branches of the weight feed.
            {
                static std::ofstream wp("trace_sysc/wfeed_probe.csv");
                static bool wp_hdr = false;
                static unsigned long long wp_n = 0, wp_rows = 0;
                static int wp_key = -1;
                if (!wp_hdr)
                {
                    wp << "n,ev,in_cnt_en,fifo_full,stall,cnt_en,cswitch,done,"
                          "tildone,aux,w,tilk,auxlim,wlim,tilklim\n";
                    wp_hdr = true;
                }
                const int wp_k = ((int)in.cnt_en << 3) | ((int)fifo_full_any << 2) |
                                 ((int)stall_any << 1) | (int)in.cswitch;
                const char *wp_ev = 0;
                if (io_.done)            wp_ev = "done";
                else if (io_.til_done)   wp_ev = "tdone";
                else if (wp_k != wp_key) wp_ev = "chg";
                else if ((wp_n % 20000) == 0) wp_ev = "hb";
                if (wp_ev && wp_rows < 20000)
                {
                    wp_rows++;
                    wp << wp_n << "," << wp_ev << "," << (int)in.cnt_en << ","
                       << (int)fifo_full_any << "," << (int)stall_any << ","
                       << (int)cnt_en << "," << (int)in.cswitch << ","
                       << (int)io_.done << "," << (int)io_.til_done << ","
                       << (unsigned)idx_.dbg_aux() << "," << (unsigned)idx_.dbg_w()
                       << "," << (unsigned)idx_.dbg_tilk() << ","
                       << (unsigned)in.auxlim << "," << (unsigned)in.wlim << ","
                       << (unsigned)in.til_klim << "\n";
                    wp.flush();
                }
                wp_key = wp_k;
                wp_n++;
            }
#endif
            sramb_addr_ = io_.sram_addr;
            glob_woffs_ = io_.woffs;
            transn_ = io_.transn;
            outbounds_ = io_.outbounds;
            done_ = io_.done;
            til_done_ = io_.til_done;

            // ---- final-push muxes (RTL 287-289) ----
            std::array<elem_t, SRAMB_N> sramb_data_mux{};
            if (!finalpush_q2_)
                sramb_data_mux = sram_data_q_;
            const uint32_t glob_woffs_mux = in.finalpush ? 0u : glob_woffs_;
            const bool transn_mux = in.finalpush ? false : transn_;

            // ---- lanes (RTL 236-265) ----
            Outputs out;
            for (int x = 0; x < X_DIM; x++)
            {
                typename Lane::Inputs li;
                li.sram_data = sramb_data_mux;
                li.feeder_en = in.feeder_en && in.cols_active[x];
                li.update = feeders_update;
                li.clearbuff = in.feeder_clear;
                li.valid_data = valid_data;
                li.x_ov_flag = transn_mux;
                li.glob_woffs = glob_woffs_mux;
                li.loc_woffs = (uint32_t)x; // RTL loc_woffs[x] = x
                li.dil_pat = (uint64_t)1 << (DILP_W - 1); // RTL i_Dil_pat[0] only
                li.finalpush = finalpush_q2_;
                li.clearfifo = in.clearfifo;
                li.pipeline_en = in.pipeline_en;
                li.pop_en = in.pop_en;

                auto lo = lane_[x].tick(li);
                empty_prev_[x] = lo.fifo_empty;
                if (x == 0) { dbg_push0_ = lo.fifo_push; dbg_empty0_ = lo.fifo_empty;
                              dbg_stall0_ = lo.stall; dbg_data0_ = lo.data; }
                if (x == 0) { dbg_din0_ = lo.din0; }
                if (x == 0) { dbg_ptr0_ = lane_[0].dbg_ptr();
                              dbg_pop0_ = lane_[0].dbg_pop();
                              dbg_full0_ = lo.fifo_full;
                              dbg_eq1_ = lane_[0].dbg_fifo_empty_q1();
                              dbg_eq2_ = lane_[0].dbg_fifo_empty_q2();
                              dbg_estart_ = lane_[0].dbg_fifo_empty_start(); }
                if (x == 0)
                {
                    const auto &fd = lane_[0].dbg_fdm();
                    dbg_vq1_ = fd.dbg_valid_q1();
                    dbg_woffs_ = fd.dbg_woffs_init();
                    dbg_sidx_ = fd.dbg_shift_idx();
                    dbg_rptr_ = fd.dbg_read_ptr();
                    dbg_patn_ = fd.dbg_pat_popcount();
                    dbg_patf_ = fd.dbg_pat_first();
                    dbg_ract_ = fd.dbg_regs_active();
                    dbg_upd_ = feeders_update;
                    dbg_fen0_ = in.feeder_en && in.cols_active[0];
                    dbg_vdata_ = valid_data;
                    dbg_elm_ = fd.dbg_elm_number();
                    dbg_nfree_ = fd.dbg_n_free();
                    dbg_fdmpipe_ = fd.dbg_pipe_en();
                    dbg_li_fen_ = li.feeder_en;      // what the LANE actually got
                    dbg_li_upd_ = li.update;
                    dbg_li_vd_ = li.valid_data;
                    dbg_li_xov_ = li.x_ov_flag;
                    dbg_li_gw_ = li.glob_woffs;
                    dbg_li_lw_ = li.loc_woffs;
                    dbg_in_fen_ = in.feeder_en;      // what WeiFeederRtl was GIVEN
                    dbg_in_ca0_ = in.cols_active[0];
                    dbg_full_any_ = fifo_full_any;
                    dbg_stall_any_ = stall_any;
                    dbg_pren_ = pipeline_regs_en;
                    dbg_cnten_ = cnt_en;
                }
                full_prev_[x] = lo.fifo_full;
                stall_prev_[x] = lo.stall;

                // ---- skew line: column x is delayed by x cycles (RTL 267-278).
                // feed_registers is instantiated with N_REGS=jj, i.e. a DIFFERENT
                // depth per column, so this needs runtime depth -- a single
                // template parameter cannot express it.
                out.b_arr[x] = skew_tick(x, lo.data, in.clearfifo, in.pipeline_en);
            }

#ifdef FX1_A3_WPTR_PROBE
            // Debug hook (default off, read-only): reduced port of sauria_model's WPTR_PROBE
            // (data_feeder/wei_feeder_rtl.h:379-406) -- only the columns available here, with the original column names
            // (the reference is a superset). Logs the weight FIFO fill (ptr0) + fifo_full_any / stall_any, the two inputs
            // of wei_shim_en.
            {
                static std::ofstream wpr("trace_sysc/wptr.csv");
                static bool wpr_hdr = false;
                static unsigned long long wpr_n = 0;
                if (!wpr_hdr)
                {
                    wpr << "n,pop_en,pipe_en,fifo_pop,fifo_empty,ptr0,clearfifo,"
                           "cswitch,wei_cnt_en_local,stall_any,fifo_full_any\n";
                    wpr_hdr = true;
                }
                wpr << wpr_n << "," << (int)in.pop_en << "," << (int)in.pipeline_en
                    << "," << (int)dbg_pop0_ << "," << (int)dbg_empty0_ << ","
                    << dbg_ptr0_ << "," << (int)in.clearfifo << ","
                    << (int)in.cswitch << "," << (int)cnt_en << ","
                    << (int)stall_any << "," << (int)fifo_full_any << "\n";
                wpr_n++;
            }
#endif
#ifdef FX1_A3_WDIN_PROBE
            // Debug hook (default off): one line per cycle with feeder_en -- the value PUSHED into the FIFO
            // (din0 = regs_q_ pre-tick) next to sq0 (latched SRAM-B data) and addr.
            {
                static std::ofstream wd("trace_sysc/wdin.csv");
                static bool wd_hdr = false;
                static unsigned long long wd_n = 0, wd_rows = 0;
                if (!wd_hdr)
                {
                    wd << "n,fen,upd,full,stall,vdata,elm,nfree,ract,push,"
                          "din0,sq0,addr,cen,pipe_en,pop_en,empty0,data0\n";
                    wd_hdr = true;
                }
                if (in.feeder_en && wd_rows < 6000)
                {
                    wd_rows++;
                    const auto &fdp = lane_[0].dbg_fdm();
                    wd << wd_n << "," << (int)in.feeder_en << ","
                       << (int)feeders_update << "," << (int)fifo_full_any << ","
                       << (int)stall_any << "," << (int)valid_data << ","
                       << fdp.dbg_elm_number() << "," << fdp.dbg_n_free() << ","
                       << fdp.dbg_regs_active() << "," << (int)dbg_push0_ << ","
                       << (int)dbg_din0_ << "," << (int)sram_data_q_[0] << ","
                       << sramb_addr_ << "," << (int)cnt_en << ","
                       << (int)in.pipeline_en << "," << (int)in.pop_en << ","
                       << (int)dbg_empty0_ << "," << (int)dbg_data0_ << "\n";
                }
                wd_n++;
            }
#endif
            // ---- shim registers (RTL 145-176) ----
            if (in.feeder_clear)
            {
                finalpush_q1_ = finalpush_q2_ = false;
                valid_q1_ = valid_q2_ = false;
                outbounds_q1_ = outbounds_q2_ = outbounds_q3_ = false;
            }
            else if (pipeline_regs_en)
            {
                finalpush_q2_ = finalpush_q1_;
                finalpush_q1_ = in.finalpush;
                valid_q2_ = valid_q1_;
                valid_q1_ = in.wei_valid;
                outbounds_q3_ = outbounds_q2_;
                outbounds_q2_ = outbounds_q1_;
                outbounds_q1_ = outbounds_;
            }

            // ---- SRAM data register (RTL 184-197) ----
#ifdef FX1_A3_SRAMB_RDEN_PHASE
            // second stage of the SRAM-B read path, gated as in the RTL: i_sramb_data(t) = mem[A(t-2)] gated by rden(t-1)
            // (gating by rden(t-2) would drop one element per FIFO fill and repeat the next one).
            if (upd_prev_)
                sramb_s2_ = in.sramb_data;
            if (in.feeder_clear)
                sram_data_q_.fill(0);
            else if (pipeline_regs_en)
                sram_data_q_ = sramb_s2_;
            upd_prev_ = feeders_update;
#else
            if (in.feeder_clear)
                sram_data_q_.fill(0);
            else if (pipeline_regs_en)
                sram_data_q_ = in.sramb_data;
#endif

            full_any_shim_ = fifo_full_any; // RTL 312-318 (exists, unread here)

            // ---- outputs (RTL 320-329) ----
            out.done = done_;
            out.til_done = til_done_;
            out.sramb_addr = sramb_addr_;
            out.sramb_rden = feeders_update;
            out.fifo_empty = fifo_empty_any;
            out.fifo_full = fifo_full_any;
            out.feeder_stall = stall_any;
            out.wei_deadlock = fifo_empty_any && fifo_full_any;
            return out;
        }

        // symmetric to ifmap (cols_active masks empty).
        bool peek_stall_any(bool feeder_en, const std::array<bool, X_DIM> &cols_active) const
        {
            for (int x = 0; x < X_DIM; x++)
                if (lane_[x].peek_stall(feeder_en && cols_active[x], finalpush_q2_))
                    return true;
            return false;
        }

        bool peek_fifo_empty(const std::array<bool, X_DIM> &cols_active) const
        {
            for (int x = 0; x < X_DIM; x++)
                if (lane_[x].peek_empty() && cols_active[x])
                    return true;
            return false;
        }

        // symmetric: OR over every lane, not masked by cols_active.
        bool peek_fifo_full() const
        {
            for (int x = 0; x < X_DIM; x++)
                if (lane_[x].peek_full())
                    return true;
            return false;
        }

    private:
        // Runtime-depth equivalent of feed_registers.sv (N_REGS = column index):
        // reg_q[0] is the input, o_dout = reg_q[N_REGS]; stages advance on
        // i_pipeline_en and are zeroed by i_clear. Depth 0 is a plain wire.
        elem_t skew_tick(int x, elem_t din, bool clear, bool pipeline_en)
        {
            const int depth = x;
            elem_t dout = (depth == 0) ? din : skew_[x][depth - 1];
            if (clear)
            {
                for (int k = 0; k < depth; k++)
                    skew_[x][k] = 0;
            }
            else if (pipeline_en && depth > 0)
            {
                for (int k = depth - 1; k > 0; k--)
                    skew_[x][k] = skew_[x][k - 1];
                skew_[x][0] = din;
            }
            return dout;
        }

        Idx idx_;
        Lane lane_[X_DIM];
        std::array<std::array<elem_t, (X_DIM > 0 ? X_DIM : 1)>, X_DIM> skew_{};
        std::array<elem_t, SRAMB_N> sram_data_q_{};
        // second stage of the SRAM-B read path (only under FX1_A3_SRAMB_RDEN_PHASE)
        std::array<elem_t, SRAMB_N> sramb_s2_{};
#ifdef FX1_A3_RDEN_PHASE_INIT1
        bool upd_prev_{true};
#else
        bool upd_prev_{false};
#endif
        bool finalpush_q1_{false}, finalpush_q2_{false};
        bool valid_q1_{false}, valid_q2_{false};
        bool outbounds_q1_{false}, outbounds_q2_{false}, outbounds_q3_{false};
        bool stall_prev_[X_DIM]{}, full_prev_[X_DIM]{};
        bool empty_prev_[X_DIM];  // set true in reset() -- FIFO starts EMPTY
        uint32_t glob_woffs_{0}, sramb_addr_{0};
        bool transn_{false}, outbounds_{false}, done_{false}, til_done_{false};
        bool full_any_shim_{false};
        // diagnostics for lane 0 (not RTL ports)
    public:
        bool dbg_push0_{false}, dbg_empty0_{true}, dbg_stall0_{false};
        bool dbg_vq1_{false}, dbg_upd_{false}, dbg_fen0_{false}, dbg_vdata_{false};
        uint32_t dbg_woffs_{0}, dbg_sidx_{0}, dbg_rptr_{0}, dbg_patn_{0}, dbg_ract_{0};
        int dbg_patf_{-1};
        uint32_t dbg_elm_{0}, dbg_nfree_{0}, dbg_li_gw_{0}, dbg_li_lw_{0};
        bool dbg_fdmpipe_{false}, dbg_li_fen_{false}, dbg_li_upd_{false};
        bool dbg_li_vd_{false}, dbg_li_xov_{false}, dbg_in_fen_{false};
        bool dbg_in_ca0_{false}, dbg_full_any_{false}, dbg_stall_any_{false};
        bool dbg_pren_{false}, dbg_cnten_{false};
        // Debug hooks (read-only): the real K counters of the shared WeiIdxCnt (idx_, not per lane).
        uint32_t dbg_aux() const { return idx_.dbg_aux(); }
        uint32_t dbg_w() const { return idx_.dbg_w(); }
        uint32_t dbg_tilk() const { return idx_.dbg_tilk(); }
        elem_t dbg_data0_{0};
        elem_t dbg_din0_{0};
        uint32_t dbg_ptr0_{0};
        bool dbg_pop0_{false}, dbg_full0_{false};
        // Debug hooks (read-only): fifo_empty_q2_ (the register that decides real data vs 0 on each beat), and
        // empty_start_q / ptr_q separately.
        bool dbg_eq1_{false}, dbg_eq2_{false}, dbg_estart_{false};
        // Debug hooks (read-only): sram_data_q_ / finalpush_q2_ / sramb_addr_.
        uint32_t dbg_sramb_addr() const { return sramb_addr_; }
        bool dbg_finalpush_q2() const { return finalpush_q2_; }
        elem_t dbg_sram_data_q0() const { return sram_data_q_[0]; }
        bool dbg_done() const { return done_; }
        bool dbg_til_done() const { return til_done_; }
    private:
    };

} // namespace sauria_rtl
