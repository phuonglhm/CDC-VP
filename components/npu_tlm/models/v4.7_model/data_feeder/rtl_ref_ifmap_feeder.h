// SystemC Model for SAURIA NPU Core
// Activation (IFmap) Feeder Block with Parameterized FIFO Depth

#ifndef SAURIA_RTL_IFMAP_FEEDER_H
#define SAURIA_RTL_IFMAP_FEEDER_H

#include "sauria_types.h"
#ifdef FX1_A3_IFMAP_FEEDER_RTL
#include "data_feeder/rtl_ref_ifmap_feeder_rtl.h"
#endif
#include <sstream>
#include "debug.h"
#include "data_feeder/rtl_ref_feed_data_manager.h"
#ifdef FX1_A2_IDXCNT_THROTTLE
#include "data_feeder/rtl_ref_ifmap_idxcnt.h"
#endif
#include <queue>
#include <vector>
#include <fstream>
#include <string>
#include <fstream>
#include <map>
#include <set>

#ifndef FX1_NO_PERF
#include "instrumentation/rtl_ref_perf_counters.h"
#endif

namespace sauria_rtl
{
using namespace sauria; // A1 port: shared vector/mask types (act_vector_t, wei_vector_t, psum_vector_t, sramc_mask_t, host_data_t, ...) stay in ::sauria, only the class bodies move to ::sauria_rtl. See rtl_ref_context_switch_controller.h header comment for the general namespace rationale.

    template <
        int Y_DIM = 32,
        typename T_ACT = float,
        int SRAMA_CAP = 1024,
        int FIFO_DEPTH = 16>
    class IfmapFeeder : public sc_module
    {
    public:
        // Clock & Reset
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

        // Control Inputs from Global FSM
        sc_in<bool> i_feeder_en{"i_feeder_en"};
        sc_in<bool> i_feeder_clear{"i_feeder_clear"};
        sc_in<bool> i_start{"i_start"};
        sc_in<bool> i_valid{"i_valid"};
        sc_in<bool> i_finalpush{"i_finalpush"};
        sc_in<bool> i_cnt_en{"i_cnt_en"};
        sc_in<bool> i_cnt_clear{"i_cnt_clear"};
        sc_in<bool> i_clearfifo{"i_clearfifo"};
        sc_in<bool> i_pop_en{"i_pop_en"};
        // systolic-array pipeline enable (RTL i_pipeline_en). Absent from
        // this model until now, which forced the RTL ports to assume `true`.
        sc_in<bool> i_pipeline_en{"i_pipeline_en"};
        sc_in<bool> i_finalctx{"i_finalctx"};

        // Config Parameters
        sc_in<uint32_t> i_act_incntlim{"i_act_incntlim"};
        sc_in<uint32_t> i_act_incntstep{"i_act_incntstep"};
        sc_in<uint32_t> i_act_outcntlim{"i_act_outcntlim"};
        sc_in<uint32_t> i_act_outcntstep{"i_act_outcntstep"};
        sc_in<sc_bv<DILP_W>> i_act_dil_pat{"i_act_dil_pat"};
        // per-row enable. RTL ifmap_feeder.sv gates each lane's
        // i_feeder_en with i_rows_active[jj] (271) and lets a zero row override
        // that lane's fifo_empty (322-330). The behavioural path does not read
        // this yet -- IfmapFeederRtl is its first consumer, exactly the
        // situation that hid the cols_active defect for many sessions.
        sc_in<sramc_mask_t<Y_DIM>> i_rows_active{"i_rows_active"};
        // per-row word offsets (RTL i_loc_woffs, encodes stride).
        sc_in<act_vector_t<Y_DIM, uint32_t>> i_loc_woffs{"i_loc_woffs"};
        // Full SAURIA IFMAP address-generator runtime config
        sc_in<uint32_t> i_act_xlim{"i_act_xlim"};
        sc_in<uint32_t> i_act_xstep{"i_act_xstep"};
        sc_in<uint32_t> i_act_ylim{"i_act_ylim"};
        sc_in<uint32_t> i_act_ystep{"i_act_ystep"};
        sc_in<uint32_t> i_act_chlim{"i_act_chlim"};
        sc_in<uint32_t> i_act_chstep{"i_act_chstep"};
        sc_in<uint32_t> i_act_til_xlim{"i_act_til_xlim"};
        sc_in<uint32_t> i_act_til_xstep{"i_act_til_xstep"};
        sc_in<uint32_t> i_act_til_ylim{"i_act_til_ylim"};
        sc_in<uint32_t> i_act_til_ystep{"i_act_til_ystep"};
        sc_in<uint32_t> i_context_id{"i_context_id"};
        sc_in<uint32_t> i_ncontexts{"i_ncontexts"};
        sc_in<uint32_t> i_act_reps{"i_act_reps"};
        sc_in<uint32_t> i_mvm_k{"i_mvm_k"};
        // Memory Interface to SRAM A
        sc_out<uint32_t> o_srama_addr{"o_srama_addr"};
        sc_out<bool> o_srama_rden{"o_srama_rden"};
        sc_in<act_vector_t<Y_DIM, T_ACT>> i_srama_data{"i_srama_data"};
        sc_in<uint32_t> i_act_base_addr{"i_act_base_addr"};

        // Wavefront Output Vector towards Systolic Array (A ports)
        sc_out<act_vector_t<Y_DIM, T_ACT>> o_act_arr{"o_act_arr"};

        // Feeder Status Outputs
        sc_out<bool> o_act_done{"o_act_done"};
        sc_out<bool> o_act_til_done{"o_act_til_done"};
        sc_out<bool> o_fifo_empty{"o_fifo_empty"};
        sc_out<bool> o_fifo_full{"o_fifo_full"};
        sc_out<bool> o_stall{"o_stall"};

#ifndef FX1_NO_PERF
        // A1 -- non-owning perf pointer (set by NpuTop::attach_perf, like array/ctrl).
        // Feeder writes act_l1_read_words / act_feed_cycles / act_stall_cycles.
        sauria_rtl::PerfCounters *perf{nullptr};
#endif

        SC_CTOR(IfmapFeeder)
        {
#ifdef FX1_A3_SEAM_ORDER
            // npu_top calls seam_step() in order.
#else
            SC_METHOD(feeder_process);
            sensitive << i_clk.pos();
#endif
        }

    private:
        std::vector<T_ACT> act_flat_buf;
        std::map<uint32_t, act_vector_t<Y_DIM, T_ACT>> act_word_cache;
        std::set<uint32_t> act_requested_words;

        bool act_stream_init{false};
        bool last_appended_word_valid{false};
        uint32_t last_appended_word_addr{0};

        // ---------------------------------------------------------------
        // Plan A A1 (Option A, unified/): parallel timing
        // shadow of RTL feed_data_manager.sv's elm_number/n_free_regs stall
        // mechanism. Ticked once per cycle in the sauria_addr_mode read-issue
        // path (see below), keyed off the SAME "a new SRAM word was issued"
        // event the existing word-cache logic already computes -- does NOT
        // touch act_word_cache/act_req_k/act_emit_count or any other
        // data-correctness state, only contributes an additional OR term to
        // act_should_stall. I_W=1,SRAM_W=Y_DIM => SRAM_N=Y_DIM elements/word,
        // matching real HW (sauria_pkg.sv SRAMA_W=IA_W*Y, one word broadcasts
        // Y_DIM lane positions). Element VALUES are irrelevant to this timing
        // model (only i_act_dil_pat occupancy matters), so dummy 0s are fed.
#ifdef FX1_A3_IFMAP_FEEDER_RTL
        // full structural RTL feeder (ifmap_idxcnt + Y lanes M=3 + skew).
        using RtlFeeder = sauria_rtl::IfmapFeederRtl<Y_DIM, T_ACT, SRAMA_CAP,
                                                 FIFO_DEPTH, SAURIA_ACT_IDX_W,
                                                 DILP_W, 3>;
        RtlFeeder rtl_feeder_;

    public:
        // for npu_top to call in sequence.
        void seam_step() { feeder_process(); }

        // set by the top level; only read under FX1_A3_CNT_EN_DIRECT.
        std::function<bool()> seam_peek_cnt_en_{};
        std::function<bool()> seam_peek_pipe_en_{};
        std::function<bool()> seam_peek_valid_{};
        std::function<bool()> seam_peek_start_{};
        std::function<bool(int)> seam_peek_fd_{};

    private:

    public:
        // this cycle's fifo_empty and stall, read directly by main_controller (an sc_signal between two SC_METHODs on the
        // same clock is one beat late).
        bool peek_stall_now() const
        {
            std::array<bool, Y_DIM> ra{};
            sramc_mask_t<Y_DIM> m = i_rows_active.read();
            for (int y = 0; y < Y_DIM; y++) ra[y] = m[y];
            return rtl_feeder_.peek_stall_any(i_feeder_en.read(), ra);
        }

        bool peek_fifo_empty_now() const
        {
            std::array<bool, Y_DIM> ra{};
            sramc_mask_t<Y_DIM> m = i_rows_active.read();
            for (int y = 0; y < Y_DIM; y++) ra[y] = m[y];
            return rtl_feeder_.peek_fifo_empty(ra);
        }

        // this cycle's FULL flag (combinational to feeders_fsm in the RTL).
        bool peek_fifo_full_now() const { return rtl_feeder_.peek_fifo_full(); }
#ifdef FX1_A3_TILDONE_Q_GATE
        // RTL o_til_done of THIS cycle: ifmap_idxcnt.sv:274 til_done_q & i_cnt_en,
        // with ifmap_feeder.sv:133 cnt_en = i_cnt_en & !fifo_full_any & !stall_any.
        // Uses the same cnt_en/feeder_en sources as the feeder process. Valid only
        // before this cycle's feeder tick (SEAM_ORDER: Control runs first).
        bool peek_til_done_rtl_now() const
        {
#ifdef FX1_A3_CNT_EN_DIRECT
            const bool ce = seam_peek_cnt_en_ ? seam_peek_cnt_en_() : i_cnt_en.read();
#else
            const bool ce = i_cnt_en.read();
#endif
#ifdef FX1_A3_FDFSM_DIRECT
            const bool fe = seam_peek_fd_ && (FX1_A3_FDFSM_MASK & (1u<<0)) ? seam_peek_fd_(0) : (i_feeder_en.read());
#else
            const bool fe = i_feeder_en.read();
#endif
            std::array<bool, Y_DIM> ra{};
            sramc_mask_t<Y_DIM> m = i_rows_active.read();
            for (int y = 0; y < Y_DIM; y++) ra[y] = m[y];
            return rtl_feeder_.peek_til_done_q() && ce &&
                   !rtl_feeder_.peek_fifo_full() && !rtl_feeder_.peek_stall_any(fe, ra);
        }
        // value computed inside the LAST feeder tick (observer, for cross-check)
        bool dbg_last_til_done_rtl() const { return rtl_feeder_.dbg_til_done_rtl(); }
        bool dbg_cnt_en_rtl() const { return rtl_feeder_.dbg_cnt_en(); }
        bool dbg_push0_rtl() const { return rtl_feeder_.dbg_push0(); }
        bool dbg_empty0_rtl() const { return rtl_feeder_.dbg_empty0(); }
        uint32_t dbg_elm0_rtl() const { return rtl_feeder_.dbg_elm0_now(); }
        uint32_t dbg_nfree0_rtl() const { return rtl_feeder_.dbg_nfree0_now(); }
        bool dbg_stall_any_rtl() const { return rtl_feeder_.dbg_stall_any(); }
        bool dbg_finalpush_rtl() const { return rtl_feeder_.dbg_finalpush(); }
#endif
        // Debug hooks (read-only pass-through).
        bool dbg_outbounds_now() const { return rtl_feeder_.dbg_outbounds_now(); }
        bool dbg_outbounds_q1_now() const { return rtl_feeder_.dbg_outbounds_q1_now(); }
        // Debug hooks (read-only): live local_ch() / local_wave() / context_y_offset -- the values the behavioural
        // address path (calc_ifmap_word_range_for_k / emit block) uses this cycle, without pipeline delay.
        uint32_t dbg_local_ch() const { return local_ch(); }
        uint32_t dbg_local_wave() const { return local_wave(); }
        uint32_t dbg_context_y_offset() const
        {
            return local_ch() * i_act_til_ystep.read() + local_wave() * i_act_til_xstep.read();
        }
        uint32_t dbg_i_context_id_raw() const { return i_context_id.read(); }

    private:
        int64_t rtl_context_seen_{-1};   // last context seen, to clear the latches on a context change
        bool rtl_init_{false};
#endif
        sauria_rtl::FeedDataManager<1, Y_DIM, DILP_W, 3> act_fdm;
        bool act_fdm_stall{false};

        // Plan A A2: 1:1 port of ifmap_idxcnt.sv's 5-counter
        // address-gen cascade (X/Y/Ch/TilX/TilY), run as a PARALLEL cycle-cost meter
        // -- does NOT replace the existing calc_ifmap_word_range_for_k/act_word_cache
        // prefetch path (that path is bit-exact and stays untouched). Only gates
        // WHEN act_emit_count is allowed to advance (see emit loop below), throttling
        // to RTL's real cadence: 1 tick/K when o_xlim is ALIGNED, up to 3 ticks/K
        // when UNALIGNED (K%32!=0), matching the o_done pulse timing. Width uses the
        // SAME SAURIA_ACT_IDX_W build macro tb_evaluate.cpp/tb_demo.cpp already read
        // (geometry-dependent: 17 for 32x32, 18 for 64x64, etc, per tools/dse_sweep.py
        // GEO table -- NOT the RTL module's declared default=11,).
        // Guarded OFF by default (FX1_A2_IDXCNT_THROTTLE) so existing behavior is
        // byte-for-byte unchanged until this passes its own verify tiers (3-4).
#ifdef FX1_A2_IDXCNT_THROTTLE
#ifndef SAURIA_ACT_IDX_W
#define SAURIA_ACT_IDX_W 15
#endif
        // ADRA_W/WOFS_W are DERIVED in RTL (ifmap_feeder.sv:98-99,
        // sauria_pkg.sv:53/56), NOT the module defaults 8/3 this used to inherit
        // by passing only IDX_W. SRAMA_N = SRAMA_W/IA_W = Y_DIM (one activation
        // per lane), so WOFS_W = clog2(Y_DIM); SRAMA_DEPTH = capacity in bytes
        // divided by the word size in bytes = SRAMA_CAP/(sizeof(T_ACT)*Y_DIM).
        static constexpr int clog2_ct(int n)
        {
            int b = 0;
            while ((1 << b) < n)
                b++;
            return b;
        }
        static constexpr int ACT_WOFS_W = clog2_ct(Y_DIM);
        static constexpr int ACT_SRAM_DEPTH =
            (SRAMA_CAP / (int)(sizeof(T_ACT) * Y_DIM)) > 0
                ? (SRAMA_CAP / (int)(sizeof(T_ACT) * Y_DIM))
                : 1;
        static constexpr int ACT_ADRA_W = clog2_ct(ACT_SRAM_DEPTH);
        sauria_rtl::IfmapIdxCnt<SAURIA_ACT_IDX_W, ACT_ADRA_W, ACT_WOFS_W> act_idxcnt;
        // Tracks which context act_idxcnt's cascade state belongs to. The
        // "!act_stream_init" block re-executes MANY times per real context
        // (existing demand-fetch retry/re-entry behavior, harmless for the
        // untouched code since act_emit_count/act_word_cache re-fill near-
        // instantly there) -- resetting act_idxcnt on every one of those
        // re-entries (as a first wiring attempt did) wipes its ~300-tick
        // progress before "done" can ever fire, deadlocking the throttle
        // gate forever (found via debug trace,). Reset only when
        // the context actually changes.
        int64_t act_idxcnt_ctx_seen{-1};
#endif

        uint32_t act_stream_base_idx{0};
        uint32_t act_word_req_idx{0};
        uint32_t act_last_word_idx{0};

        uint32_t act_emit_abs_idx{0};
        uint32_t act_emit_count{0};
        uint32_t act_pop_count{0};
        // Prefetch/request pointer, decoupled from act_emit_count so SRAM reads
        // pipeline (issue-ahead) instead of one-word-at-a-time demand fetch.
        // This makes the feeder supply ~1 operand/cycle (II=1) like the RTL,
        // versus the old serial request->wait->emit cadence (II=3).
        uint32_t act_req_k{0};

        std::queue<uint32_t> pending_word_addr_q;
        std::queue<uint32_t> pending_glob_woffs_q;

        static constexpr uint32_t MEMA_N = Y_DIM;

        act_vector_t<Y_DIM, T_ACT> cur_word;
        act_vector_t<Y_DIM, T_ACT> next_word;

        bool cur_word_valid{false};
        bool next_word_valid{false};

        uint32_t pending_glob_woffs{0};
        uint32_t pending_word_addr{0};
        bool pending_read_valid{false};

        uint32_t act_req_idx{0};

        // Internal FIFOs for each of the Y rows
        std::queue<T_ACT> row_fifos[Y_DIM];

        // Dynamic skew delay registers for systolic wavefront (A-side)
        std::vector<T_ACT> skew_regs[Y_DIM];

        // Address tracking register
        uint32_t addr_reg{0};

        // local variable
        uint32_t incnt{0};
        uint32_t dil_idx{0};

        // SAURIA-style IFMAP address generator counters (5-counter, RTL convention)
        uint32_t act_x_cnt{0};
        uint32_t act_y_cnt{0};
        uint32_t act_ch_cnt{0};
        uint32_t act_tx_cnt{0}; // tiling x (output pixel x)
        uint32_t act_ty_cnt{0}; // tiling y (output pixel y)

        // SRAM read latency matching registers
        bool rden_q1{false};
        bool rden_q2{false};
        bool cnt_clear_q{false};

        bool use_sauria_ifmap_addr_gen()
        {
            return (
                i_act_xlim.read() != 0 &&
                i_act_xstep.read() != 0 &&
                i_act_ylim.read() != 0 &&
                i_act_ystep.read() != 0 &&
                i_act_chlim.read() != 0 &&
                i_act_chstep.read() != 0);
        }

        // 5-counter nested advance (RTL convention): x -> y -> ch -> til_x -> til_y.
        // Each counter wraps when (cnt + step) >= lim -> reset to 0, carry to the outer one.
        // Verified against im2col golden in addr_gen_rtl.py.
        void advance_sauria_ifmap_addr_gen()
        {
            uint32_t next_x = act_x_cnt + i_act_xstep.read();
            if (next_x < i_act_xlim.read())
            {
                act_x_cnt = next_x;
                return;
            }
            act_x_cnt = 0;

            uint32_t next_y = act_y_cnt + i_act_ystep.read();
            if (next_y < i_act_ylim.read())
            {
                act_y_cnt = next_y;
                return;
            }
            act_y_cnt = 0;

            uint32_t next_ch = act_ch_cnt + i_act_chstep.read();
            if (next_ch < i_act_chlim.read())
            {
                act_ch_cnt = next_ch;
                return;
            }
            act_ch_cnt = 0;

            // Context boundary: 3 inner counters wrapped -> move to next output pixel
            uint32_t next_tx = act_tx_cnt + i_act_til_xstep.read();
            if (next_tx < i_act_til_xlim.read())
            {
                act_tx_cnt = next_tx;
                return;
            }
            act_tx_cnt = 0;

            uint32_t next_ty = act_ty_cnt + i_act_til_ystep.read();
            if (next_ty < i_act_til_ylim.read())
            {
                act_ty_cnt = next_ty;
                return;
            }
            act_ty_cnt = 0;
        }

        uint32_t get_effective_k() const
        {
            uint32_t k = i_mvm_k.read();

            if (k == 0)
            {
                k = i_act_incntlim.read();
            }

            if (k == 0)
            {
                k = 1;
            }

            return k;
        }
        // FIX: when the real output width Cw exceeds
        // what one array pass can cover (Y_used), SAURIA folds an extra "wave" index
        // into ncontexts on TOP of the real Ch index (confirmed against the golden
        // reference's own context-unpacking loop, execution_model.py::
        // compute_partial_macs: "for k: for y(=ch): for x(=wave)", x/wave fastest-
        // varying/innermost). x_int_tiles() recovers the wave COUNT from the
        // activation-side tiling registers (o_til_xlim=ceil(Cw/Y_used)*Y_used*s,
        // o_til_xstep=Y_used*s per config_helper.py -> ratio = ceil(Cw/Y_used),
        // independent of any K-tiling/external-tile dimension, so this is safe to
        // derive from a single register ratio). local_ch()/local_wave() split
        // ctx_per_tile's raw index into (real ch, wave) -- previously local_ch()
        // returned the raw undivided index, which was only ever correct when
        // x_int_tiles()==1 (every case tested before this fix: demo_gemm_32x32,
        // ktile-gate -- both have Cw<=Y_used, so this reduces to the old identity
        // behavior there, bit-exact unchanged).
        uint32_t x_int_tiles() const
        {
            uint32_t txlim = i_act_til_xlim.read();
            uint32_t txstep = i_act_til_xstep.read();
            uint32_t n = (txstep != 0) ? (txlim / txstep) : 1;
            if (n == 0) n = 1;
            return n;
        }

        // Local context (Ch index) within one output tile. output_tiles are folded
        // into ncontexts (= Ch x n_tiles); the activation for (tile,ch) depends only
        // on ch, so use context_id % ctx_per_tile. Single-tile (act_reps=1) => identity.
        uint32_t local_ch() const
        {
            uint32_t nt = i_act_reps.read(); if (nt == 0) nt = 1;
            uint32_t nc = i_ncontexts.read(); if (nc == 0) nc = 1;
            uint32_t cpt = nc / nt; if (cpt == 0) cpt = 1;
            uint32_t ctx_in_tile = i_context_id.read() % cpt;
            return ctx_in_tile / x_int_tiles();
        }

        // Local "wave" index (which Y_used-wide slice of the output-W span this
        // context covers) within one output tile -- see local_ch() comment above.
        uint32_t local_wave() const
        {
            uint32_t nt = i_act_reps.read(); if (nt == 0) nt = 1;
            uint32_t nc = i_ncontexts.read(); if (nc == 0) nc = 1;
            uint32_t cpt = nc / nt; if (cpt == 0) cpt = 1;
            uint32_t ctx_in_tile = i_context_id.read() % cpt;
            return ctx_in_tile % x_int_tiles();
        }

        void derive_ifmap_mvm_params(
            uint32_t effective_k,
            uint32_t &kernel_elems,
            uint32_t &kernel_w,
            uint32_t &n_channels,
            uint32_t &kx_step) const
        {
            uint32_t chstep = i_act_chstep.read();
            uint32_t ystep = i_act_ystep.read();
            uint32_t til_ystep = i_act_til_ystep.read();

            uint32_t cfg_n_channels = 1;

            if (chstep != 0)
            {
                cfg_n_channels = i_act_chlim.read() / chstep;
            }

            if (cfg_n_channels == 0)
            {
                cfg_n_channels = 1;
            }

            // IMPORTANT:
            // kernel_elems must come from original config limit, not mvm_k.
            // Test 2:
            //   cfg_incntlim = 225
            //   cfg_n_channels = 25
            //   kernel_elems = 9
            kernel_elems = i_act_incntlim.read() / cfg_n_channels;

            if (kernel_elems == 0)
            {
                kernel_elems = 1;
            }

            kernel_w = 1;
            for (uint32_t r = 1; r * r <= kernel_elems; r++)
            {
                if (r * r == kernel_elems)
                {
                    kernel_w = r;
                }
            }

            // Test 2:
            //   ystep = 234
            //   til_ystep = 26
            //   kx_step = 9
            kx_step = 1;

            if (til_ystep != 0 && ystep != 0 && (ystep % til_ystep) == 0)
            {
                kx_step = ystep / til_ystep;
            }

            if (kx_step == 0)
            {
                kx_step = 1;
            }

            // Test 2:
            //   effective_k = 450
            //   kernel_elems = 9
            //   n_channels = 50
            n_channels = effective_k / kernel_elems;

            if (n_channels == 0)
            {
                n_channels = 1;
            }
        }

        bool calc_ifmap_word_range_for_k(
            uint32_t k,
            uint32_t effective_k,
            uint32_t &start_abs_idx,
            uint32_t &first_word,
            uint32_t &last_word,
            uint32_t &ch_id,
            uint32_t &ky,
            uint32_t &kx) const
        {
            if (k >= effective_k)
            {
                return false;
            }

            uint32_t chstep = i_act_chstep.read();
            uint32_t ystep = i_act_ystep.read();

            uint32_t kernel_elems = 1;
            uint32_t kernel_w = 1;
            uint32_t n_channels = 1;
            uint32_t kx_step = 1;

            derive_ifmap_mvm_params(
                effective_k,
                kernel_elems,
                kernel_w,
                n_channels,
                kx_step);

            if (kernel_elems == 0)
            {
                kernel_elems = 1;
            }

            if (kernel_w == 0)
            {
                kernel_w = 1;
            }

            ch_id = k / kernel_elems;

            uint32_t rem = k % kernel_elems;

            ky = rem / kernel_w;
            kx = rem % kernel_w;

            uint32_t context_y_offset =
                (local_ch() * i_act_til_ystep.read() + local_wave() * i_act_til_xstep.read());

            start_abs_idx =
                context_y_offset + ch_id * chstep + ky * ystep + kx * kx_step;

            // FIX: account for the real stride, matching
            // the emit-side "STRIDE FIX" formula (~line 807-810). The old formula
            // below (`start_abs_idx + Y_DIM - 1`) assumes Y_DIM-1 CONSECUTIVE
            // elements, which only holds for stride=1 (GeMM/1x1). For a real
            // strided conv, adjacent output rows are `s_row` input columns apart,
            // so the true span needed is `(active_rows-1)*s_row`, which can reach
            // several SRAM words beyond what the old formula ever requested --
            // permanently starving act_word_cache of a word the emit side's
            // enough_data check needed (confirmed via direct word-range tracing,
            // test3's real Bw=7/s=3 shape: emit needed word 2, prefetch never
            // requested past word 1, causing an infinite stall). When s_row==1
            // (every case previously exercised by make check/ktile-gate) this
            // reduces to exactly the old formula -- bit-exact unchanged there.
            uint32_t s_row = 1;
            if (ystep != 0)
            {
                uint32_t s_tys = i_act_til_ystep.read();
                if (s_tys != 0 && (s_tys % ystep) == 0)
                    s_row = s_tys / ystep;
            }
            if (s_row == 0)
                s_row = 1;
            uint32_t s_txstep = i_act_til_xstep.read();
            uint32_t active_rows = (s_txstep != 0) ? (s_txstep / s_row) : (uint32_t)Y_DIM;
            if (active_rows == 0 || active_rows > (uint32_t)Y_DIM)
                active_rows = (uint32_t)Y_DIM;

            first_word = start_abs_idx / Y_DIM;
            last_word = (start_abs_idx + (active_rows - 1) * s_row) / Y_DIM;

            return true;
        }

        void feeder_process()
        {
#ifdef FX1_A3_ACT_PORT_AUDIT
            {
                // runtime values of the declared-but-unread ports.
                static std::ofstream au("trace_sysc/act_port_audit.csv");
                static bool au_hdr = false;
                static uint64_t au_cyc = 0;
                static std::map<std::string, std::string> au_last;
                if (!au_hdr)
                {
                    au << "cyc,inst,rstn,fclear,clrfifo,fen,cen,cclr,pop,"
                          "start,valid,finalpush,finalctx,ctxid,dil_hex,dil_pop,"
                          "dil_first" << "\n";
                    au_hdr = true;
                }
                const uint64_t dv = i_act_dil_pat.read().to_uint64();
                int dpop = 0, dfirst = -1;
                for (int b = 0; b < 64; b++)
                    if ((dv >> b) & 1ULL)
                    {
                        dpop++;
                        if (dfirst < 0)
                            dfirst = b;
                    }
                std::ostringstream key;
                key << (int)i_rstn.read() << (int)i_feeder_clear.read()
                    << (int)i_clearfifo.read()
                    << (int)i_feeder_en.read() << (int)i_cnt_en.read()
                    << (int)i_cnt_clear.read() << (int)i_pop_en.read()
                    << (int)i_start.read()
                    << (int)i_valid.read() << (int)i_finalpush.read()
                    << (int)i_finalctx.read() << "_" << i_context_id.read()
                    << "_" << std::hex << dv;
                std::string nm = this->name();
                if (au_last[nm] != key.str())
                {
                    au_last[nm] = key.str();
                    au << au_cyc << "," << nm << ","
                       << (int)i_rstn.read() << "," << (int)i_feeder_clear.read() << ","
                       << (int)i_clearfifo.read() << ","
                       << (int)i_feeder_en.read() << "," << (int)i_cnt_en.read() << ","
                       << (int)i_cnt_clear.read() << "," << (int)i_pop_en.read() << ","
                       << (int)i_start.read() << ","
                       << (int)i_valid.read() << "," << (int)i_finalpush.read() << ","
                       << (int)i_finalctx.read() << "," << i_context_id.read() << ",0x"
                       << std::hex << dv << std::dec << "," << dpop << "," << dfirst
                       << "\n";
                    au.flush();
                }
                au_cyc++;
            }
#endif
#ifdef FX1_A3_IFMAP_FEEDER_RTL
            {
                if (!i_rstn.read())
                {
                    rtl_feeder_.reset();
                    rtl_context_seen_ = -1;
                    rtl_init_ = true;
                    o_srama_addr.write(0);
                    o_srama_rden.write(false);
                    o_act_arr.write(act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0)));
                    o_act_done.write(false);
                    o_act_til_done.write(false);
                    o_fifo_empty.write(true);
                    o_fifo_full.write(false);
                    o_stall.write(false);
                    return;
                }
                if (!rtl_init_)
                {
                    rtl_feeder_.reset();
                    rtl_context_seen_ = -1;
                    rtl_init_ = true;
                }

                typename RtlFeeder::Inputs ri;
                act_vector_t<Y_DIM, T_ACT> ad = i_srama_data.read();
                for (int y = 0; y < Y_DIM; y++)
                    ri.srama_data[y] = static_cast<int32_t>(ad[y]);

                sramc_mask_t<Y_DIM> ra = i_rows_active.read();
                act_vector_t<Y_DIM, uint32_t> lw = i_loc_woffs.read();
                for (int y = 0; y < Y_DIM; y++)
                {
                    ri.rows_active[y] = ra[y];
                    ri.loc_woffs[y] = lw[y];
                }

#ifdef FX1_A3_CNT_EN_DIRECT
                // read Control's shadow directly (Control has already stepped, see SEAM_ORDER).
                ri.cnt_en = seam_peek_cnt_en_ ? seam_peek_cnt_en_() : i_cnt_en.read();
#else
                ri.cnt_en = i_cnt_en.read();
#endif
#ifdef FX1_A3_FDFSM_DIRECT
                ri.cnt_clear = seam_peek_fd_ && (FX1_A3_FDFSM_MASK & (1u<<4)) ? seam_peek_fd_(4) : (i_cnt_clear.read());
#else
                ri.cnt_clear = i_cnt_clear.read();
#endif
#ifdef FX1_A3_FDFSM_DIRECT
                ri.finalctx = seam_peek_fd_ && (FX1_A3_FDFSM_MASK & (1u<<7)) ? seam_peek_fd_(7) : (i_finalctx.read());
#else
                ri.finalctx = i_finalctx.read();
#endif
                ri.xlim = i_act_xlim.read();
                ri.xstep = i_act_xstep.read();
                ri.ylim = i_act_ylim.read();
                ri.ystep = i_act_ystep.read();
                ri.chlim = i_act_chlim.read();
                ri.chstep = i_act_chstep.read();
                ri.til_xlim = i_act_til_xlim.read();
                ri.til_xstep = i_act_til_xstep.read();
                ri.til_ylim = i_act_til_ylim.read();
                ri.til_ystep = i_act_til_ystep.read();
#ifdef FX1_A3_TILCNT_SEED_PER_CTX
                // exactly the pair the RTL cascade would hold at this
                // context. local_wave()/local_ch() are the SAME helpers the old
                // address path used at line 492 -- verified 8/8 against a
                // free-running IfmapIdxCnt in /tmp/t_224.cpp.
                ri.til_x_seed = local_wave() * i_act_til_xstep.read();
                ri.til_y_seed = local_ch() * i_act_til_ystep.read();
#endif
#ifdef FX1_A3_FDFSM_DIRECT
                ri.feeder_en = seam_peek_fd_ && (FX1_A3_FDFSM_MASK & (1u<<0)) ? seam_peek_fd_(0) : (i_feeder_en.read());
#else
                ri.feeder_en = i_feeder_en.read();
#endif
#ifdef FX1_A3_FDFSM_DIRECT
                ri.feeder_clear = seam_peek_fd_ && (FX1_A3_FDFSM_MASK & (1u<<1)) ? seam_peek_fd_(1) : (i_feeder_clear.read());
#else
                ri.feeder_clear = i_feeder_clear.read();
#endif
#ifdef FX1_A3_VALID_DIRECT
                // valid is combinational in the RTL -- read directly, not through sc_signal.
                ri.act_valid = seam_peek_valid_ ? seam_peek_valid_() : i_valid.read();
#else
                ri.act_valid = i_valid.read();
#endif
#ifdef FX1_A3_START_DIRECT
                ri.start = seam_peek_start_ ? seam_peek_start_() : i_start.read();
#else
                ri.start = i_start.read();
#endif
#ifdef FX1_A3_FDFSM_DIRECT
                ri.finalpush = seam_peek_fd_ && (FX1_A3_FDFSM_MASK & (1u<<3)) ? seam_peek_fd_(3) : (i_finalpush.read());
#else
                ri.finalpush = i_finalpush.read();
#endif
                ri.dil_pat = i_act_dil_pat.read().to_uint64();
#ifdef FX1_A3_FDFSM_DIRECT
                ri.clearfifo = seam_peek_fd_ && (FX1_A3_FDFSM_MASK & (1u<<2)) ? seam_peek_fd_(2) : (i_clearfifo.read());
#else
                ri.clearfifo = i_clearfifo.read();
#endif
#ifdef FX1_A3_PIPE_EN_DIRECT
                // read Control's shadow directly (Control has already stepped, see SEAM_ORDER).
                // pipeline_en gates every lane's fifo_pop, so it is on the main path.
                ri.pipeline_en = seam_peek_pipe_en_ ? seam_peek_pipe_en_() : i_pipeline_en.read();
#else
                ri.pipeline_en = i_pipeline_en.read(); // real signal
#endif
#ifdef FX1_A3_FDFSM_DIRECT
                ri.pop_en = seam_peek_fd_ && (FX1_A3_FDFSM_MASK & (1u<<5)) ? seam_peek_fd_(5) : (i_pop_en.read());
#else
                ri.pop_en = i_pop_en.read();
#endif

                // As sauria_model/data_feeder/ifmap_feeder.h:773-801: between two contexts `cnt_en` goes low; the
                // til_done / outbounds latches of the previous context are cleared then, or they would carry over. Only
                // cleared while `!cnt_en` -- the first cycle of an active joint belongs to the valid sequence.
                {
                    const int64_t ctx_now = static_cast<int64_t>(i_context_id.read());
                    if (rtl_context_seen_ >= 0 && rtl_context_seen_ != ctx_now && !ri.cnt_en)
                        rtl_feeder_.clear_context_flags();
                    rtl_context_seen_ = ctx_now;
                }

                auto ro = rtl_feeder_.tick(ri);
#ifdef FX1_A3_FEED_TAPE
                // Debug hook (default off): one line EVERY cycle (unfiltered), std instance only; joins fsm_tape.csv on tsim.
                if (std::string(this->name()).find("NpuTop_std") !=
                    std::string::npos)
                {
                    static std::ofstream ft("trace_sysc/feed_tape.csv");
                    static bool ft_hdr = false;
                    if (!ft_hdr)
                    {
                        ft << "tsim,cnt_en,pop_en,pipe_en,cnt_clear,feeder_en,"
                              "ctx,srama_addr,rden,push,fifo_empty,valid,"
                              "rptr0,elm0,a0,a1,woffs,outb,xtr\n";
                        ft_hdr = true;
                    }
                    ft << sc_core::sc_time_stamp().value() << ","
                       << (int)ri.cnt_en << "," << (int)ri.pop_en << ","
                       << (int)ri.pipeline_en << "," << (int)ri.cnt_clear << ","
                       << (int)ri.feeder_en << ","
                       << i_context_id.read() << ","
                       << ro.srama_addr << "," << (int)ro.srama_rden << ","
                       << (int)ro.dbg_any_push << ","
                       << (int)ro.fifo_empty << ","
                       << (int)ro.dbg_valid_data << ","
                       << ro.dbg_rptr0 << "," << ro.dbg_elm0 << ","
                       << (long long)ro.a_arr[0] << ","
                       << (long long)ro.a_arr[1] << ","
                       // element index of lane 0 = srama_addr*Y_DIM + woffs (sub-word resolution; the next
                       // two columns are context only).
                       << ro.dbg_glob_woffs << ","
                       << (int)ro.dbg_outbounds << ","
                       << (int)ro.dbg_x_transition << "\n";
                }
#endif

#ifdef FX1_A3_FDM_IN_TRACE
                // Debug hook (default off): the four inputs that decide the per-lane push ORDER.
                // Expected lw[y] = y * act_stride (make_loc_woffs, config_regs.h:170).
                {
                    static std::ofstream ft("trace_sysc/fdm_in.csv");
                    static bool ft_hdr = false;
                    static uint64_t ft_cyc = 0;
                    if (std::string(this->name()).find("NpuTop_std") !=
                        std::string::npos)
                    {
                        if (!ft_hdr)
                        {
                            ft << "cyc,ctx,fen,x_ov,glob_woffs,rptr0,patpop0,"
                                  "lw0,lw1,lw2,lw3,lw31\n";
                            ft_hdr = true;
                        }
                        if (ri.feeder_en)
                        {
                            ft << ft_cyc << "," << i_context_id.read() << ","
                               << (int)ri.feeder_en << ","
                               << (int)ro.dbg_x_transition << ","
                               << ro.dbg_glob_woffs << ","
                               << ro.dbg_rptr0 << "," << ro.dbg_patpop0 << ","
                               << ri.loc_woffs[0] << "," << ri.loc_woffs[1] << ","
                               << ri.loc_woffs[2] << "," << ri.loc_woffs[3] << ","
                               << ri.loc_woffs[Y_DIM - 1] << "\n";
                        }
                        ft_cyc++;
                    }
                }
#endif

#ifdef FX1_A3_ACT_RTL_ADDR_TRACE
                // Debug hook (default off): UNCOMPRESSED counterpart of act_raw_addr.csv -- one line per cycle with rden,
                // same format and address reference (offset without base), so it compares line by line with the
                // behavioural path (gather_trace compresses repeated addresses).
                if (ro.srama_rden)
                {
                    static std::ofstream rtl_addr_trace(
                        "trace_sysc/act_rtl_addr.csv");
                    static bool rtl_addr_hdr = false;
                    static uint32_t rtl_addr_seq = 0;
                    if (std::string(this->name()).find("NpuTop_std") !=
                        std::string::npos)
                    {
                        if (!rtl_addr_hdr)
                        {
                            rtl_addr_trace << "context,word_addr,seq\n";
                            rtl_addr_hdr = true;
                        }
                        rtl_addr_trace << i_context_id.read() << ","
                                       << ro.srama_addr << ","
                                       << rtl_addr_seq++ << "\n";
                    }
                }
#endif

#ifdef FX1_A3_GATHER_TRACE
                {
                    static std::ofstream gt("trace_sysc/gather_trace.csv");
                    static bool gt_hdr = false;
                    static uint64_t gt_cyc = 0;
                    static std::map<std::string, std::string> gt_last;
                    static std::map<std::string, uint64_t> n_push, n_rden, n_stall,
                        n_tdone, n_fupd, n_vdata;
                    if (!gt_hdr)
                    {
                        gt << "cyc,inst,fen,cnten,pipen,popen,vdata,outb,"
                              "stall,n_stall,fupd,n_fupd,full,empty,"
                              "push,n_push,rden,n_rden,addr,woffs,"
                              "elm0,nfree0,vq1_0,patpop0,rptr0,"
                              "done,tdone,n_tdone,n_vdata,"
                              "n_full,n_empty,first_full,first_empty,elm_full,"
                              "ctx,seedx,seedy"
#ifdef FX1_A3_PATPOP_ALL
                              ",patsum,patmax"
#endif
#ifdef FX1_A3_LANESTALL
                              ",n_stall_lanes,first_stall"
#endif
                           << "\n";
                        gt_hdr = true;
                    }
                    std::string gn = this->name();
                    if (ro.dbg_any_push) n_push[gn]++;
                    if (ro.srama_rden) n_rden[gn]++;
                    if (ro.dbg_stall_any) n_stall[gn]++;
                    if (ro.til_done) n_tdone[gn]++;
                    if (ro.dbg_feeders_update) n_fupd[gn]++;
                    if (ro.dbg_valid_data) n_vdata[gn]++;

                    std::ostringstream gk;
                    gk << (int)ri.feeder_en << (int)ri.cnt_en << (int)ri.pipeline_en
                       << (int)ri.pop_en << (int)ro.dbg_valid_data
                       << (int)ro.dbg_outbounds << (int)ro.dbg_stall_any
                       << (int)ro.dbg_feeders_update << (int)ro.dbg_fifo_full_any
                       << (int)ro.fifo_empty << (int)ro.dbg_any_push
                       << (int)ro.srama_rden << (int)ro.done << (int)ro.til_done
                       << "_" << ro.srama_addr << "_" << ro.dbg_elm0
                       << "_" << ro.dbg_nfree0 << "_" << ro.dbg_patpop0
                       << "_" << ro.dbg_n_full << "_" << ro.dbg_n_empty
                       << "_" << i_context_id.read();
                    if (gt_last[gn] != gk.str())
                    {
                        gt_last[gn] = gk.str();
                        gt << gt_cyc << "," << gn << ","
                           << (int)ri.feeder_en << "," << (int)ri.cnt_en << ","
                           << (int)ri.pipeline_en << "," << (int)ri.pop_en << ","
                           << (int)ro.dbg_valid_data << "," << (int)ro.dbg_outbounds << ","
                           << (int)ro.dbg_stall_any << "," << n_stall[gn] << ","
                           << (int)ro.dbg_feeders_update << "," << n_fupd[gn] << ","
                           << (int)ro.dbg_fifo_full_any << "," << (int)ro.fifo_empty << ","
                           << (int)ro.dbg_any_push << "," << n_push[gn] << ","
                           << (int)ro.srama_rden << "," << n_rden[gn] << ","
                           << ro.srama_addr << "," << ro.dbg_glob_woffs << ","
                           << ro.dbg_elm0 << "," << ro.dbg_nfree0 << ","
                           << (int)ro.dbg_vq1_0 << "," << ro.dbg_patpop0 << ","
                           << ro.dbg_rptr0 << ","
                           << (int)ro.done << "," << (int)ro.til_done << ","
                           << n_tdone[gn] << "," << n_vdata[gn] << ","
                           << ro.dbg_n_full << "," << ro.dbg_n_empty << ","
                           << ro.dbg_first_full << "," << ro.dbg_first_empty << ","
                           << ro.dbg_elm_full << ","
                           << i_context_id.read() << ","
                           << ri.til_x_seed << "," << ri.til_y_seed
#ifdef FX1_A3_PATPOP_ALL
                           << "," << ro.dbg_patsum << "," << ro.dbg_patmax
#endif
#ifdef FX1_A3_LANESTALL
                           << "," << ro.dbg_n_stall << "," << ro.dbg_first_stall
#endif
                           << "\n";
                        gt.flush();
                    }
                    gt_cyc++;
                }
#endif
                o_srama_addr.write(i_act_base_addr.read() + ro.srama_addr);
                o_srama_rden.write(ro.srama_rden);
                act_vector_t<Y_DIM, T_ACT> ov;
                for (int y = 0; y < Y_DIM; y++)
                    ov[y] = static_cast<T_ACT>(ro.a_arr[y]);
                o_act_arr.write(ov);
                o_act_done.write(ro.done);
                o_act_til_done.write(ro.til_done);
                o_fifo_empty.write(ro.fifo_empty);
                o_fifo_full.write(ro.fifo_full);
                o_stall.write(ro.feeder_stall);
                return;
            }
#endif
            if (!i_rstn.read() || i_feeder_clear.read() || i_clearfifo.read())
            {
                o_srama_addr.write(0);
                o_srama_rden.write(false);
                o_act_arr.write(act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0)));
                o_act_done.write(false);
                o_act_til_done.write(false);
                o_fifo_empty.write(true);
                o_fifo_full.write(false);
                o_stall.write(false);
                addr_reg = 0;
                incnt = 0;
                dil_idx = 0;

                act_x_cnt = 0;
                act_y_cnt = 0;
                act_ch_cnt = 0;
                act_tx_cnt = 0;
                act_ty_cnt = 0;
                act_req_idx = 0;
                act_pop_count = 0;

                act_fdm.reset();
                act_fdm_stall = false;
#ifdef FX1_A2_IDXCNT_THROTTLE
                act_idxcnt.reset();
                act_idxcnt_ctx_seen = -1;
#endif

                cnt_clear_q = false;
                rden_q1 = false;
                rden_q2 = false;

                last_appended_word_valid = false;
                last_appended_word_addr = 0;

                act_flat_buf.clear();
                act_word_cache.clear();
                act_requested_words.clear();

                cur_word = act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0));
                next_word = act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0));
                cur_word_valid = false;
                next_word_valid = false;
                pending_glob_woffs = 0;
                pending_word_addr = 0;
                pending_read_valid = false;

                while (!pending_word_addr_q.empty())
                    pending_word_addr_q.pop();

                while (!pending_glob_woffs_q.empty())
                    pending_glob_woffs_q.pop();

                for (int i = 0; i < Y_DIM; i++)
                {
                    while (!row_fifos[i].empty())
                        row_fifos[i].pop();
                    skew_regs[i].assign(i, static_cast<T_ACT>(0)); // Delay length matches row index i
                }
                return;
            }

            if (!i_feeder_en.read())
            {
                o_srama_rden.write(false);
                o_act_arr.write(act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0)));
                return;
            }

            o_srama_rden.write(false);

            bool cnt_clear_now = i_cnt_clear.read();
            bool cnt_clear_pulse = cnt_clear_now && !cnt_clear_q;
            cnt_clear_q = cnt_clear_now;

            if (cnt_clear_pulse)
            {
                addr_reg = 0;
                incnt = 0;
                dil_idx = 0;

                act_x_cnt = 0;
                act_y_cnt = 0;
                act_ch_cnt = 0;
                act_tx_cnt = 0;
                act_ty_cnt = 0;
                act_req_idx = 0;

                rden_q1 = false;
                rden_q2 = false;
                o_srama_rden.write(false);

                last_appended_word_valid = false;
                last_appended_word_addr = 0;

                cur_word = act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0));
                next_word = act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0));
                cur_word_valid = false;
                next_word_valid = false;
                pending_glob_woffs = 0;
                pending_word_addr = 0;
                pending_read_valid = false;

                act_flat_buf.clear();
                act_word_cache.clear();
                act_requested_words.clear();

                act_stream_init = false;

                act_fdm.reset();
                act_fdm_stall = false;
#ifdef FX1_A2_IDXCNT_THROTTLE
                act_idxcnt.reset();
                act_idxcnt_ctx_seen = -1;
#endif

                act_stream_base_idx = 0;
                act_word_req_idx = 0;
                act_last_word_idx = 0;

                act_emit_abs_idx = 0;
                act_emit_count = 0;
                act_pop_count = 0;
                act_req_k = 0;

                while (!pending_word_addr_q.empty())
                    pending_word_addr_q.pop();

                while (!pending_glob_woffs_q.empty())
                    pending_glob_woffs_q.pop();

                for (int i = 0; i < Y_DIM; i++)
                {
                    while (!row_fifos[i].empty())
                        row_fifos[i].pop();

                    skew_regs[i].assign(i, static_cast<T_ACT>(0));
                }
            }

#ifdef FX1_A3_FEEDER_RDEN_MATCH_SRAM
            // sram/rtl_ref_sram_top.h's FX1_A3_SRAMA_RDEN_PHASE block (default ON) is a raw
            // combinational passthrough: o_srama_data always reflects mem[i_srama_addr], valid to
            // the feeder exactly 1 cycle after rden/addr are issued (normal sc_signal visibility
            // delay) -- NOT 2. The rden_q1/rden_q2 2-stage gate below double-counts that latency
            // (see sram/rtl_ref_sram_top.h's header comment). With this macro on (opt-in, off by default) the gate uses
            // rden_q1 alone (1 stage) instead of rden_q2 (2 stages).
            bool mem_data_valid = rden_q1;
            rden_q1 = false;
#else
            bool mem_data_valid = rden_q2;
            rden_q2 = rden_q1;
            rden_q1 = false;
#endif
            // 1. Fetch activation vector from SRAM A into FIFOs
            if (mem_data_valid)
            {
                act_vector_t<Y_DIM, T_ACT> mem_data = i_srama_data.read();

                uint32_t use_word_addr = 0;

                if (!pending_word_addr_q.empty())
                {
                    use_word_addr = pending_word_addr_q.front();
                    pending_word_addr_q.pop();
                }

                if (!pending_glob_woffs_q.empty())
                {
                    pending_glob_woffs_q.pop();
                }

                bool duplicate_word = last_appended_word_valid && (use_word_addr == last_appended_word_addr);

                act_word_cache[use_word_addr] = mem_data;
                if (!duplicate_word)
                {
                    for (int y = 0; y < Y_DIM; y++)
                    {
                        act_flat_buf.push_back(mem_data[y]);
                    }

                    // last_appended_word_valid = true;
                    // last_appended_word_addr = use_word_addr;
                }

                static int dbg_mem_word_count = 0;
                // bool dbg_word_interesting =
                //     (use_word_addr < 40) ||
                //     (use_word_addr >= 470 && use_word_addr <= 490) ||
                //     (use_word_addr >= 3350 && use_word_addr <= 3420);

                std::string inst_name = this->name();

                bool dbg_word_interesting = (use_word_addr >= 520 && use_word_addr <= 560);

                if (inst_name.find("NpuTop_std") != std::string::npos &&
                    dbg_word_interesting)
                {
                    DBG_COUT << "[IFMAP MEM WORD]"
                              << " word_addr=" << use_word_addr
                              << " data=[";

                    for (int y = 0; y < Y_DIM; y++)
                    {
                        DBG_COUT << static_cast<int32_t>(mem_data[y]);
                        if (y < Y_DIM - 1)
                            DBG_COUT << ", ";
                    }

                    DBG_COUT << "]" << std::endl;
                }

                dbg_mem_word_count++;

                // Emit all sliding vectors that are now available
                // while (act_emit_count < i_act_incntlim.read())
                // {
                //     uint32_t chstep = i_act_chstep.read();
                //     uint32_t ystep = i_act_ystep.read();

                //     uint32_t n_channels = 1;
                //     if (chstep != 0)
                //     {
                //         n_channels = i_act_chlim.read() / chstep;
                //     }

                //     if (n_channels == 0)
                //     {
                //         n_channels = 1;
                //     }

                //     uint32_t kernel_elems = i_act_incntlim.read() / n_channels;

                //     // For current SAURIA conv tests: kernel_elems = 9 => 3x3.
                //     uint32_t kernel_w = 1;
                //     for (uint32_t r = 1; r * r <= kernel_elems; r++)
                //     {
                //         if (r * r == kernel_elems)
                //         {
                //             kernel_w = r;
                //         }
                //     }

                //     uint32_t k = act_emit_count;

                //     uint32_t ch_id = k / kernel_elems;
                //     uint32_t rem = k % kernel_elems;

                //     uint32_t ky = rem / kernel_w;
                //     uint32_t kx = rem % kernel_w;

                //     uint32_t context_y_offset =
                //         (local_ch() * i_act_til_ystep.read() + local_wave() * i_act_til_xstep.read());

                //     uint32_t start_abs_idx =
                //         context_y_offset +
                //         ch_id * chstep +
                //         ky * ystep +
                //         kx;

                //     if (start_abs_idx < act_stream_base_idx)
                //     {
                //         break;
                //     }

                //     uint32_t local_start =
                //         start_abs_idx - act_stream_base_idx;

                //     if ((local_start + Y_DIM) > act_flat_buf.size())
                //     {
                //         break;
                //     }

                //     act_vector_t<Y_DIM, T_ACT> feed_vec;

                //     for (int y = 0; y < Y_DIM; y++)
                //     {
                //         T_ACT v = act_flat_buf[local_start + y];
                //         feed_vec[y] = v;
                //         row_fifos[y].push(v);
                //     }

                //     static int dbg_feed_vec_count = 0;
                //     std::string inst_name = this->name();

                //     if (inst_name.find("NpuTop_std") != std::string::npos &&
                //         dbg_feed_vec_count < 64)
                //     {
                //         DBG_COUT << "[IFMAP FEED VEC]"
                //                   << " count=" << dbg_feed_vec_count
                //                   << " ch=" << ch_id
                //                   << " ky=" << ky
                //                   << " kx=" << kx
                //                   << " start_abs_idx=" << start_abs_idx
                //                   << " local_start=" << local_start
                //                   << " feed_vec=[";

                //         for (int y = 0; y < Y_DIM; y++)
                //         {
                //             DBG_COUT << static_cast<int32_t>(feed_vec[y]);
                //             if (y < Y_DIM - 1)
                //                 DBG_COUT << ", ";
                //         }

                //         DBG_COUT << "]" << std::endl;
                //     }

                //     dbg_feed_vec_count++;
                //     act_emit_count++;
                // }
            }

            // ---------------------------------------------------------
            // Emit one IFMAP logical vector per cycle when enough data
            // is available in act_flat_buf.
            // This must be outside mem_data_valid so tail vectors can drain
            // after the final SRAM word has arrived.
            // ---------------------------------------------------------
            uint32_t effective_k = get_effective_k();

#ifdef FX1_A2_IDXCNT_THROTTLE
            // Legacy behavioural path only (FX1_A2_IDXCNT_THROTTLE, default off; the default uses the RTL feeder):
            // runs the ported address-gen cascade as a parallel cycle-cost meter while emit work is pending, and holds
            // each emission until out.x_ov_flag (ifmap_idxcnt.sv's per-K pulse: 1 tick/K when o_xlim is ALIGNED, up to
            // 3 ticks/K when UNALIGNED). Only delays WHEN the emit body runs; the data path is untouched.
            bool idxcnt_gate_open = true;
            if (act_stream_init && act_emit_count < effective_k)
            {
                typename sauria_rtl::IfmapIdxCnt<SAURIA_ACT_IDX_W, ACT_ADRA_W, ACT_WOFS_W>::Inputs idxcnt_in;
                idxcnt_in.cnt_en = true;
                idxcnt_in.cnt_clear = false;
                idxcnt_in.finalctx = false;
                idxcnt_in.xlim = i_act_xlim.read();
                idxcnt_in.xstep = i_act_xstep.read();
                idxcnt_in.ylim = i_act_ylim.read();
                idxcnt_in.ystep = i_act_ystep.read();
                idxcnt_in.chlim = i_act_chlim.read();
                idxcnt_in.chstep = i_act_chstep.read();
                idxcnt_in.til_xlim = i_act_til_xlim.read();
                idxcnt_in.til_xstep = i_act_til_xstep.read();
                idxcnt_in.til_ylim = i_act_til_ylim.read();
                idxcnt_in.til_ystep = i_act_til_ystep.read();

                auto idxcnt_out = act_idxcnt.tick(idxcnt_in);
                idxcnt_gate_open = idxcnt_out.x_ov_flag;
            }
#endif

            if (act_stream_init && act_emit_count < effective_k
#ifdef FX1_A2_IDXCNT_THROTTLE
                && idxcnt_gate_open
#endif
            )
            {
                uint32_t chstep = i_act_chstep.read();
                uint32_t ystep = i_act_ystep.read();

                uint32_t kernel_elems = 1;
                uint32_t kernel_w = 1;
                uint32_t n_channels = 1;
                uint32_t kx_step = 1;

                derive_ifmap_mvm_params(effective_k, kernel_elems, kernel_w, n_channels, kx_step);

                uint32_t k = act_emit_count;

                uint32_t ch_id = k / kernel_elems;
                uint32_t rem = k % kernel_elems;

                uint32_t ky = rem / kernel_w;
                uint32_t kx = rem % kernel_w;

                uint32_t context_y_offset = (local_ch() * i_act_til_ystep.read() + local_wave() * i_act_til_xstep.read());
                uint32_t start_abs_idx = context_y_offset + ch_id * chstep + ky * ystep + kx * kx_step;

                if (start_abs_idx >= act_stream_base_idx)
                {
                    // STRIDE FIX: each Y-row is one output-W position; for a strided
                    // conv adjacent output positions are s input-columns apart
                    // (s=1 for GeMM/1x1). Derive s = til_ystep/ystep (holds for d=1).
                    // Only the first active_rows (= til_xstep/s = Y_used) lanes are
                    // real outputs; require words only for that active span.
                    uint32_t s_ys  = i_act_ystep.read();
                    uint32_t s_tys = i_act_til_ystep.read();
                    uint32_t s_row = 1;
                    if (s_ys != 0 && s_tys != 0 && (s_tys % s_ys) == 0)
                        s_row = s_tys / s_ys;
                    if (s_row == 0)
                        s_row = 1;
                    uint32_t s_txstep = i_act_til_xstep.read();
                    uint32_t active_rows = (s_txstep != 0) ? (s_txstep / s_row) : (uint32_t)Y_DIM;
                    if (active_rows == 0 || active_rows > (uint32_t)Y_DIM)
                        active_rows = (uint32_t)Y_DIM;

                    uint32_t first_word = start_abs_idx / Y_DIM;
                    uint32_t last_word = (start_abs_idx + (active_rows - 1) * s_row) / Y_DIM;

                    bool enough_data = true;
                    for (uint32_t w = first_word; w <= last_word; w++)
                    {
                        if (act_word_cache.find(w) == act_word_cache.end())
                        {
                            enough_data = false;
                            break;
                        }
                    }

                    if (enough_data)
                    {
                        act_vector_t<Y_DIM, T_ACT> feed_vec;

                        for (int y = 0; y < Y_DIM; y++)
                        {
                            uint32_t abs_idx = ((uint32_t)y < active_rows) ? (start_abs_idx + y * s_row) : start_abs_idx;
                            uint32_t word_id = abs_idx / Y_DIM;
                            uint32_t lane_id = abs_idx % Y_DIM;

                            T_ACT v = act_word_cache[word_id][lane_id];

                            feed_vec[y] = v;
                            row_fifos[y].push(v);
                        }

                        static int dbg_feed_vec_count = 0;
                        std::string inst_name = this->name();

                        if (inst_name.find("NpuTop_std") != std::string::npos &&
                            act_emit_count >= 60 &&
                            act_emit_count <= 80)
                        {
                            DBG_COUT << "[IFMAP FEED VEC]"
                                      << " context=" << i_context_id.read()
                                      << " k=" << act_emit_count
                                      << " ch=" << ch_id
                                      << " ky=" << ky
                                      << " kx=" << kx
                                      << " start_abs_idx=" << start_abs_idx
                                      << " first_word=" << first_word
                                      << " last_word=" << last_word
                                      << " feed_vec=[";

                            for (int y = 0; y < Y_DIM; y++)
                            {
                                DBG_COUT << static_cast<int32_t>(feed_vec[y]);
                                if (y < Y_DIM - 1)
                                    DBG_COUT << ", ";
                            }

                            DBG_COUT << "]" << std::endl;
                        }

                        dbg_feed_vec_count++;
                        act_emit_count++;
                    }
                }
            }

            // Plan A A1 bridge: default to false every cycle so a stall reported on a
            // PREVIOUS cycle can never persist once the tick block below stops running
            // (e.g. i_cnt_en drops, or within_limit/dil_allow/sauria_addr_mode go
            // false) -- only the tick() call further down can set this true again,
            // strictly for the cycle it actually ran on.
            act_fdm_stall = false;

            if (i_cnt_en.read())
            {
                bool sauria_addr_mode = use_sauria_ifmap_addr_gen();
                bool within_limit = false;
                if (sauria_addr_mode)
                {
                    within_limit = (!act_stream_init) || (act_word_req_idx <= act_last_word_idx);
                }
                else
                {
                    within_limit = (incnt < effective_k);
                }
                bool dil_allow = true;
                if (!sauria_addr_mode)
                {
                    sc_bv<DILP_W> dil = i_act_dil_pat.read();

                    bool dil_all_zero = true;
                    for (int i = 0; i < DILP_W; i++)
                    {
                        if (dil[i] == sc_dt::Log_1)
                        {
                            dil_all_zero = false;
                            break;
                        }
                    }

                    if (!dil_all_zero)
                        dil_allow = (dil[dil_idx % DILP_W] == sc_dt::Log_1);
                    // if(DILP_W >0){
                    //     dil_allow = (dil[dil_idx % DILP_W] == sc_dt::Log_1);
                    // }
                }

                if (within_limit && dil_allow)
                {
                    uint32_t final_addr = 0;

                    if (sauria_addr_mode)
                    {
                        // Captured BEFORE act_stream_init potentially flips true below --
                        // marks the one x_ov_flag-equivalent pulse for this context, fed
                        // to act_fdm's tick() further down (Plan A A1 bridge).
                        bool act_fdm_ctx_first = !act_stream_init;

                        uint32_t context_y_offset =
                            (local_ch() * i_act_til_ystep.read() + local_wave() * i_act_til_xstep.read());

                        if (!act_stream_init)
                        {
                            act_stream_base_idx = (context_y_offset / Y_DIM) * Y_DIM;

                            act_word_req_idx = act_stream_base_idx / Y_DIM;
                            act_emit_abs_idx = context_y_offset;
                            act_emit_count = 0;
                            act_pop_count = 0;
                            act_req_k = 0;

                            act_word_cache.clear();
                            act_requested_words.clear();

                            uint32_t effective_k_init = get_effective_k();

                            uint32_t dummy_start = 0;
                            uint32_t first_word = 0;
                            uint32_t last_word = 0;
                            uint32_t ch_id = 0;
                            uint32_t ky = 0;
                            uint32_t kx = 0;

                            if (calc_ifmap_word_range_for_k(
                                    effective_k_init - 1,
                                    effective_k_init,
                                    dummy_start,
                                    first_word,
                                    last_word,
                                    ch_id,
                                    ky,
                                    kx))
                            {
                                act_last_word_idx = last_word;
                            }
                            else
                            {
                                act_last_word_idx = act_word_req_idx;
                            }

                            act_stream_init = true;
#ifdef FX1_A2_IDXCNT_THROTTLE
                            // New context (K-sequence) starts its own X/Y/Ch/TilX/
                            // TilY sweep from 0. This "!act_stream_init" block
                            // re-executes MANY times for the SAME context (existing
                            // demand-fetch retry/re-entry behavior -- confirmed via
                            // debug trace,) -- only reset act_idxcnt when
                            // i_context_id actually changes, not on every re-entry,
                            // otherwise its ~300-tick progress never survives long
                            // enough for out.done to fire (deadlocks the throttle).
                            {
                                int64_t ctx_now =
                                    static_cast<int64_t>(i_context_id.read());
                                if (act_idxcnt_ctx_seen != ctx_now)
                                {
                                    act_idxcnt.reset();
                                    act_idxcnt_ctx_seen = ctx_now;
                                }
                            }
#endif

                            DBG_COUT << "[IFMAP STREAM INIT]"
                                      << " context=" << i_context_id.read()
                                      << " context_y_offset=" << context_y_offset
                                      << " first_word=" << act_word_req_idx
                                      << " last_word=" << act_last_word_idx
                                      << " effective_k=" << effective_k_init
                                      << std::endl;
                        }

                        bool issued_read = false;

                        // PREFETCH: advance the request pointer (act_req_k) ahead of
                        // the emit pointer (act_emit_count), issuing at most one SRAM
                        // read per cycle. Reads pipeline through rden_q1/q2 and the
                        // order-preserving pending_word_addr_q, so the word cache fills
                        // ~1/cycle and the array is fed at II=1 (like the RTL feeder).
                        // Only WHEN reads are issued changes; the word set and data are
                        // identical to the old demand fetch, so results stay bit-exact.
                        while (act_req_k < effective_k && !issued_read)
                        {
                            uint32_t start_abs_idx = 0;
                            uint32_t first_word = 0;
                            uint32_t last_word = 0;
                            uint32_t ch_id = 0;
                            uint32_t ky = 0;
                            uint32_t kx = 0;

                            bool ok = calc_ifmap_word_range_for_k(
                                act_req_k,
                                effective_k,
                                start_abs_idx,
                                first_word,
                                last_word,
                                ch_id,
                                ky,
                                kx);

                            if (!ok)
                            {
                                act_req_k++;
                                continue;
                            }

                            auto have_word = [&](uint32_t w) {
                                return act_word_cache.find(w) != act_word_cache.end() ||
                                       act_requested_words.find(w) != act_requested_words.end();
                            };

                            // FIX: walk every word in
                            // [first_word, last_word], not just the two endpoints --
                            // now that last_word can span more than 2 words for a
                            // real stride (fix in calc_ifmap_word_range_for_k above),
                            // the old first/last-only check silently skipped any word
                            // strictly between them, which a stride=1 (<=2-word) span
                            // could never expose. One still-missing word is requested
                            // per cycle, same "at most one SRAM read per cycle" pacing
                            // as before.
                            uint32_t req_word_addr = first_word;
                            bool need_word = false;
                            for (uint32_t w = first_word; w <= last_word; w++)
                            {
                                if (!have_word(w))
                                {
                                    req_word_addr = w;
                                    need_word = true;
                                    break;
                                }
                            }

                            if (!need_word)
                            {
                                // All words for this k already cached/in-flight:
                                // fast-forward without issuing a read.
                                act_req_k++;
                                continue;
                            }

                            uint32_t req_final_addr =
                                i_act_base_addr.read() + req_word_addr;

                            pending_word_addr_q.push(req_word_addr);
                            pending_glob_woffs_q.push(0);
                            act_requested_words.insert(req_word_addr);

#ifdef FX1_A3_ACT_ADDR_TRACE
                            // mirror of weight_raw_buf.csv for the
                            // activation side -- the ONLY missing piece needed to
                            // cross-check IfmapIdxCnt against the real feeder.
                            {
                                static std::ofstream act_addr_trace(
                                    "trace_sysc/act_raw_addr.csv");
                                static bool act_addr_hdr = false;
                                static uint32_t act_addr_seq = 0;
                                if (std::string(this->name()).find("NpuTop_std") !=
                                    std::string::npos)
                                {
                                    if (!act_addr_hdr)
                                    {
                                        act_addr_trace << "context,word_addr,seq\n";
                                        act_addr_hdr = true;
                                    }
                                    act_addr_trace << i_context_id.read() << ","
                                                   << req_word_addr << ","
                                                   << act_addr_seq++ << "\n";
                                }
                            }
#endif

                            o_srama_rden.write(true);
                            o_srama_addr.write(req_final_addr);

                            rden_q1 = true;
                            issued_read = true;
#ifndef FX1_A3_FEEDER_PERF_NO_PERF
                            if (perf) perf->act_l1_read_words++;   // A1: 1 SRAMA word read
#endif

                            // Advance to next k only when EVERY word in range is now
                            // satisfied (the word just requested this cycle counts as
                            // satisfied too).
                            bool all_satisfied = true;
                            for (uint32_t w = first_word; w <= last_word; w++)
                            {
                                if (w == req_word_addr)
                                    continue;
                                if (!have_word(w))
                                {
                                    all_satisfied = false;
                                    break;
                                }
                            }
                            if (all_satisfied)
                                act_req_k++;

                            static int dbg_ifmap_demand_req_count = 0;
                            std::string inst_name = this->name();

                            if (inst_name.find("NpuTop_std") != std::string::npos &&
                                dbg_ifmap_demand_req_count < 128)
                            {
                                DBG_COUT << "[IFMAP DEMAND REQ]"
                                          << " req=" << dbg_ifmap_demand_req_count
                                          << " context=" << i_context_id.read()
                                          << " k=" << act_req_k
                                          << " ch=" << ch_id
                                          << " ky=" << ky
                                          << " kx=" << kx
                                          << " start_abs_idx=" << start_abs_idx
                                          << " first_word=" << first_word
                                          << " last_word=" << last_word
                                          << " req_word_addr=" << req_word_addr
                                          << " req_final_addr=" << req_final_addr
                                          << std::endl;

                                dbg_ifmap_demand_req_count++;
                            }
                        }

                        // ---------------------------------------------------------
                        // Plan A A1 bridge: tick the parallel
                        // feed_data_manager.sv timing shadow, keyed off `issued_read`
                        // (true exactly when a genuinely NEW SRAM word was issued
                        // this cycle -- the same event the word-cache logic above
                        // already computes). Does not read/write any data-path state
                        // (act_word_cache/act_req_k/act_emit_count untouched); only
                        // produces act_fdm_stall, OR'd into act_should_stall below.
                        // glob_woffs/loc_woffs kept at 0 (documented simplification --
                        // this model doesn't track a separate word-offset register the
                        // way RTL's woffs_init_q does; acceptable since dsi=shift_idx_cnt
                        // alone still walks the DILP_W-wide pattern across words).
                        {
                            typename sauria_rtl::FeedDataManager<1, Y_DIM, DILP_W, 3>::Inputs fdm_in;
                            fdm_in.feeder_en = true;
                            fdm_in.update = issued_read;
                            fdm_in.clearbuff = false;
                            fdm_in.valid_data = true;
                            fdm_in.fifo_full = false;
                            fdm_in.x_ov_flag = act_fdm_ctx_first;
                            fdm_in.glob_woffs = 0;
                            fdm_in.loc_woffs = 0;
                            sc_bv<DILP_W> dil_bv = i_act_dil_pat.read();
                            uint64_t dil_u64 = 0;
                            // FIX: direct copy, NOT reversed.
                            // FeedDataManager::dil_bit(k) already reverses internally
                            // (reads dil_pat bit (DILP_W-1-k) to decode logical position
                            // k); the old code here reversed dil_bv AGAIN before storing
                            // it, producing a double reversal so dil_bit(k) ended up
                            // reading i_Dil_pat[DILP_W-1-k] instead of i_Dil_pat[k] --
                            // confirmed via direct bit-index tracing (dil_bv set at
                            // indices 57-63 for a Bw=7 kernel, matching the validated
                            // i_Dil_pat[k]->register-bit(63-k) convention from the
                            // pointwise raw-register dump). dil_u64 must be a faithful
                            // bit-for-bit copy of dil_bv so dil_bit()'s own single
                            // reversal is the only one applied.
                            for (int b = 0; b < DILP_W; b++)
                                if (dil_bv[b] == sc_dt::Log_1) dil_u64 |= (1ULL << b);
                            fdm_in.dil_pat = dil_u64;
                            fdm_in.finalpush = false;

                            auto fdm_out = act_fdm.tick(fdm_in);
                            act_fdm_stall = fdm_out.stall;
                        }

                        if (!issued_read)
                        {
                            o_srama_rden.write(false);
                            rden_q1 = false;
                        }
                    }
                    else
                    {
                        final_addr = i_act_base_addr.read() + addr_reg;
                    }
                    static int dbg_word_req_count = 0;
                    std::string inst_name = this->name();

                    if (inst_name.find("NpuTop_std") != std::string::npos &&
                        dbg_word_req_count < 32)
                    {
                        DBG_COUT << "[IFMAP WORD REQ]"
                                  << " req=" << dbg_word_req_count
                                  << " act_word_req_idx=" << act_word_req_idx
                                  << " final_addr=" << final_addr
                                  << " last_word=" << act_last_word_idx
                                  << std::endl;
                    }

                    dbg_word_req_count++;

                    static int dbg_ifmap_read_count = 0;

                    if (dbg_ifmap_read_count < 64)
                    {
                        DBG_COUT << "[DEBUG IFMAP] read_count=" << dbg_ifmap_read_count
                                  << " mode=" << (sauria_addr_mode ? "SAURIA" : "LINEAR")
                                  << "context_id=" << i_context_id.read()
                                  << "context_y_offset" << ((local_ch() * i_act_til_ystep.read() + local_wave() * i_act_til_xstep.read()))
                                  << " feeder_en=" << i_feeder_en.read()
                                  << " cnt_en=" << i_cnt_en.read()
                                  << " cnt_clear=" << i_cnt_clear.read()
                                  << " base=" << i_act_base_addr.read()
                                  << " addr_reg=" << addr_reg
                                  << " x_cnt=" << act_x_cnt
                                  << " y_cnt=" << act_y_cnt
                                  << " ch_cnt=" << act_ch_cnt
                                  << " final_addr=" << final_addr
                                  << " incnt=" << incnt
                                  << " limit=" << i_act_incntlim.read()
                                  << " xlim=" << i_act_xlim.read()
                                  << " xstep=" << i_act_xstep.read()
                                  << " ylim=" << i_act_ylim.read()
                                  << " ystep=" << i_act_ystep.read()
                                  << " chlim=" << i_act_chlim.read()
                                  << " chstep=" << i_act_chstep.read()
                                  << std::endl;
                    }

                    dbg_ifmap_read_count++;

                    if (!sauria_addr_mode)
                    {
                        addr_reg += i_act_incntstep.read();
                        incnt++;
                    }
                }
                else
                {
                    o_srama_rden.write(false);
                    rden_q1 = false;

                    // DBG_COUT << "[DEBUG IFMAP SKIP] incnt=" << incnt
                    //           << " dil_idx=" << dil_idx
                    //           << " within_limit=" << within_limit
                    //           << " dil_allow=" << dil_allow
                    //           << std::endl;
                }
                dil_idx++;
            }
            else
            {
                o_srama_rden.write(false);
                // rden_q = false;
            }

            uint32_t effective_k_for_stall = get_effective_k();

            bool act_next_vec_ready = false;

            if (act_stream_init && act_emit_count < effective_k_for_stall)
            {
                uint32_t chstep = i_act_chstep.read();
                uint32_t ystep = i_act_ystep.read();

                uint32_t kernel_elems = 1;
                uint32_t kernel_w = 1;
                uint32_t n_channels = 1;
                uint32_t kx_step = 1;

                derive_ifmap_mvm_params(
                    effective_k_for_stall,
                    kernel_elems,
                    kernel_w,
                    n_channels,
                    kx_step);

                uint32_t k = act_emit_count;
                uint32_t ch_id = k / kernel_elems;
                uint32_t rem = k % kernel_elems;
                uint32_t ky = rem / kernel_w;
                uint32_t kx = rem % kernel_w;

                uint32_t context_y_offset =
                    (local_ch() * i_act_til_ystep.read() + local_wave() * i_act_til_xstep.read());

                uint32_t start_abs_idx =
                    context_y_offset +
                    ch_id * chstep +
                    ky * ystep +
                    kx * kx_step;

                uint32_t first_word = start_abs_idx / Y_DIM;
                uint32_t last_word = (start_abs_idx + Y_DIM - 1) / Y_DIM;

                act_next_vec_ready =
                    (act_word_cache.find(first_word) != act_word_cache.end()) &&
                    (last_word == first_word ||
                     act_word_cache.find(last_word) != act_word_cache.end());
            }
            else if (act_emit_count >= effective_k_for_stall)
            {
                act_next_vec_ready = true;
            }

            // ---------------------------------------------------------
            // IFMAP full-prefetch barrier.
            //
            // Current SystemC model uses demand-read for SAURIA/im2col IFMAP.
            // The IFMAP address stream is sparse and can jump far in SRAM,
            // so the feeder may not keep up with one vector/cycle compute.
            //
            // For correctness-first model:
            //   - Stall compute until IFMAP has emitted all effective_k logical vectors.
            //   - Controller still keeps cnt_en=1 while stalled.
            //   - Feeder continues demand-reading and pushing row_fifos.
            //   - Once act_emit_count == effective_k, compute can pop 450 valid vectors.
            //
            // This prevents A/B K misalignment.
            // ---------------------------------------------------------
            bool act_can_pop = true;

            for (int y = 0; y < Y_DIM; y++)
            {
                if (row_fifos[y].empty())
                {
                    act_can_pop = false;
                    break;
                }
            }

            bool act_need_more =
                i_feeder_en.read() &&
                (act_pop_count < effective_k_for_stall);

            // Hold controller until all IFMAP vectors for this context have been emitted.
            bool act_prefetch_not_done =
                act_stream_init &&
                (act_emit_count < effective_k_for_stall);
            (void)act_prefetch_not_done; // retained for debug; no longer gates stall

            // STREAMING STALL (II=1): stall only when compute still needs data and
            // the row FIFOs are empty - identical to the weight feeder's proven
            // logic (wei_need_more && !wei_can_pop). The old full-prefetch barrier
            // (stall until act_emit_count >= effective_k) forced emit-all-then-pop
            // = 2 passes/context; it was an over-conservative crutch for the old
            // demand-read path. Now that the request pointer prefetches ~1 word/
            // cycle, act_can_pop alone guards against popping an empty FIFO, so the
            // array can stream at one vector/cycle while emit runs concurrently.
            bool act_should_stall =
                act_need_more &&
                !act_can_pop;

            // Legacy behavioural path only: optional stall model, off by default (-DFX1_A1_FEEDER_STALL).
#ifdef FX1_A1_FEEDER_STALL
            act_should_stall = act_should_stall || act_fdm_stall;
#endif

            o_stall.write(act_should_stall);
#ifndef FX1_A3_FEEDER_PERF_NO_PERF
            if (perf && act_should_stall) perf->act_stall_cycles++;   // A1: SRAM-A backpressure
#endif

#ifdef FX1_A3_FEEDER_STALL_TRACE
            // timestamped dump of THIS module's own
            // act_should_stall computation, to compare against when
            // main_controller.h's A3TRACE sees i_act_stall.read() -- checking
            // whether the same 1-cycle sc_signal cross-SC_METHOD lag found for
            // o_c_arr/i_c_arr also affects o_stall/i_act_stall.
            {
                std::string inst_name_stalltrace = this->name();
                if (inst_name_stalltrace.find("NpuTop_std") != std::string::npos)
                {
                    static std::ofstream fst("trace_sysc/ifmap_stall_trace.csv");
                    static bool fst_header = false;
                    if (!fst_header)
                    {
                        fst << "time,i_pop_en,act_should_stall,act_can_pop,act_need_more,act_emit_count,act_pop_count,effective_k\n";
                        fst_header = true;
                    }
                    fst << sc_core::sc_time_stamp() << ","
                        << i_pop_en.read() << ","
                        << act_should_stall << ","
                        << act_can_pop << ","
                        << act_need_more << ","
                        << act_emit_count << ","
                        << act_pop_count << ","
                        << effective_k_for_stall << "\n";
                    fst.flush();
                }
            }
#endif

            // Optional debug
            static int dbg_ifmap_stall_count = 0;
            std::string inst_name_for_stall = this->name();

            if (inst_name_for_stall.find("NpuTop_std") != std::string::npos &&
                act_should_stall &&
                dbg_ifmap_stall_count < 80)
            {
                DBG_COUT << "[IFMAP STALL STATUS]"
                          << " context=" << i_context_id.read()
                          << " emit_count=" << act_emit_count
                          << " pop_count=" << act_pop_count
                          << " effective_k=" << effective_k_for_stall
                          << " act_can_pop=" << act_can_pop
                          << std::endl;

                dbg_ifmap_stall_count++;
            }

            // 2. Pop and Shift Activations (Wavefront Skew Lines)
            // ---------------------------------------------------------
            // Pop/shift IFMAP physical stream.
            //
            // Important:
            //   After all K logical vectors have been popped, row_fifos become empty,
            //   but skew_regs[y] still contain delayed tail values for y > 0.
            //   During controller DRAIN_FEED, i_pop_en remains true, so we must keep
            //   shifting zeros into skew_regs to flush the physical tail.
            //
            // If we write zero directly when row_fifos are empty, PE(0,0) may be
            // correct but all other PEs lose their tail products.
            // ---------------------------------------------------------
            if (i_pop_en.read())
            {
                act_vector_t<Y_DIM, T_ACT> popped_vec;
                act_vector_t<Y_DIM, T_ACT> act_out;

                uint32_t effective_k = get_effective_k();

                bool has_logical_vector =
                    (act_pop_count < effective_k);

                for (int y = 0; y < Y_DIM; y++)
                {
                    if (row_fifos[y].empty())
                    {
                        has_logical_vector = false;
                        break;
                    }
                }

                bool had_real_vector = has_logical_vector;
#ifndef FX1_A3_FEEDER_PERF_NO_PERF
                if (perf && had_real_vector) perf->act_feed_cycles++;   // A1: real act vector -> array
#endif

                for (int y = 0; y < Y_DIM; y++)
                {
                    T_ACT popped = static_cast<T_ACT>(0);

                    if (has_logical_vector)
                    {
                        popped = row_fifos[y].front();
                        row_fifos[y].pop();
                    }

                    popped_vec[y] = popped;

                    if (y == 0)
                    {
                        act_out[y] = popped;
                    }
                    else
                    {
                        // Keep exactly y-cycle skew.
                        while (skew_regs[y].size() < static_cast<size_t>(y))
                        {
                            skew_regs[y].insert(
                                skew_regs[y].begin(),
                                static_cast<T_ACT>(0));
                        }

                        // This push must happen even during tail flush, with popped=0.
                        skew_regs[y].push_back(popped);

                        act_out[y] = skew_regs[y].front();

                        skew_regs[y].erase(skew_regs[y].begin());
                    }
                }

                o_act_arr.write(act_out);

                // Dump only real logical vectors, not tail flush zeros.
                std::string inst_name = this->name();

                if (inst_name.find("NpuTop_std") != std::string::npos)
                {
                    static std::ofstream ifmap_pop_trace("trace_sysc/ifmap_pop_logical.csv");
                    static bool ifmap_pop_header = false;
                    static uint32_t dbg_ifmap_rows = 0;

                    if (!ifmap_pop_header)
                    {
                        ifmap_pop_trace << "context,k";

                        for (int yy = 0; yy < Y_DIM; yy++)
                        {
                            ifmap_pop_trace << ",act" << yy;
                        }

                        ifmap_pop_trace << "\n";
                        ifmap_pop_header = true;
                    }

                    uint32_t trace_k = act_pop_count;

                    if (had_real_vector && dbg_ifmap_rows < 8192)
                    {
                        ifmap_pop_trace << i_context_id.read()
                                        << "," << trace_k;

                        for (int yy = 0; yy < Y_DIM; yy++)
                        {
                            ifmap_pop_trace << ","
                                            << static_cast<int32_t>(popped_vec[yy]);
                        }

                        ifmap_pop_trace << "\n";
                        ifmap_pop_trace.flush();

                        dbg_ifmap_rows++;
                    }
                }

                if (had_real_vector && act_pop_count < effective_k)
                {
                    act_pop_count++;
                }
            }
            else
            {
                o_act_arr.write(act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0)));
            }

            // uint32_t effective_k = get_effective_k();
            bool act_pop_done = act_stream_init && (act_pop_count >= effective_k);

            o_act_done.write(act_pop_done);
            o_act_til_done.write(act_pop_done);

            static int dbg_act_done_count = 0;
            if (this->name() && dbg_act_done_count < 128)
            {
                std::string inst_name = this->name();

                if (inst_name.find("NpuTop_std") != std::string::npos)
                {
                    DBG_COUT << "[IFMAP DONE STATUS]"
                              << " context=" << i_context_id.read()
                              << " emit_count=" << act_emit_count
                              << " pop_count=" << act_pop_count
                              << " limit=" << effective_k
                              << " cfg_limit=" << i_act_incntlim.read()
                              << " done=" << act_pop_done
                              << std::endl;
                }

                dbg_act_done_count++;
            }

            // 3. Update status flags
            bool empty = row_fifos[0].empty();
            bool full = row_fifos[0].size() >= FIFO_DEPTH;
            o_fifo_empty.write(empty);
            o_fifo_full.write(full);
        }
    };

} // namespace sauria_rtl

#endif // SAURIA_RTL_IFMAP_FEEDER_H
