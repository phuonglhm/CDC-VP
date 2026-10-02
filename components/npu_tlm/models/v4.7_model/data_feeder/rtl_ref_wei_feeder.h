// SystemC Model for SAURIA NPU Core
// Weight Feeder Block with Parameterized FIFO Depth

#ifndef SAURIA_RTL_WEI_FEEDER_H
#define SAURIA_RTL_WEI_FEEDER_H

#include "sauria_types.h"
#include <map>
#include <sstream>
#ifdef FX1_A3_WEI_FEEDER_RTL
#include "data_feeder/rtl_ref_wei_feeder_rtl.h"
#endif
#ifdef FX1_A3_WEI_FEED_LANE_SHADOW
#include "data_feeder/rtl_ref_feed_xy_lane.h"
#include "data_feeder/rtl_ref_wei_idxcnt.h"
#include <fstream>
#endif
#include "debug.h"
#include <queue>
#include <vector>
#include <fstream>
#include <string>

#ifndef FX1_NO_PERF
#include "instrumentation/rtl_ref_perf_counters.h"
#endif

namespace sauria_rtl
{
using namespace sauria; // A1 port: shared vector/mask types (act_vector_t, wei_vector_t, psum_vector_t, sramc_mask_t, host_data_t, ...) stay in ::sauria, only the class bodies move to ::sauria_rtl. See rtl_ref_context_switch_controller.h header comment for the general namespace rationale.

    template <
        int X_DIM = 32,
        typename T_WEI = float,
        int SRAMB_CAP = 1024,
        int FIFO_DEPTH = 16,
        // With the wavefront skew working correctly (triangular fill on both
        // feeders), A and B already enter the array in phase from t=0, so no
        // extra static weight delay is needed. Kept as a tunable knob: if a
        // residual A-vs-B offset is measured, set this to that offset.
        int WEI_POP_DELAY = 0>
    class WeightFeeder : public sc_module
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
        sc_in<bool> i_cswitch{"i_cswitch"};

        // Config Parameters
        sc_in<uint32_t> i_wei_incntlim{"i_wei_incntlim"};
        sc_in<uint32_t> i_wei_incntstep{"i_wei_incntstep"};
        sc_in<uint32_t> i_wei_base_addr{"i_wei_base_addr"};
        // Full SAURIA WEIGHT address-generator runtime config
        sc_in<uint32_t> i_wei_wlim{"i_wei_wlim"};
        sc_in<uint32_t> i_wei_wstep{"i_wei_wstep"};
        sc_in<uint32_t> i_wei_klim{"i_wei_klim"};
        sc_in<uint32_t> i_wei_kstep{"i_wei_kstep"};
        sc_in<uint32_t> i_wei_til_klim{"i_wei_til_klim"};
        sc_in<uint32_t> i_wei_til_kstep{"i_wei_til_kstep"};
        sc_in<uint64_t> i_wei_cols_active{"i_wei_cols_active"};
        sc_in<uint32_t> i_wei_waligned{"i_wei_waligned"};
        sc_in<uint32_t> i_context_id{"i_context_id"};
        sc_in<uint32_t> i_ncontexts{"i_ncontexts"};
        sc_in<uint32_t> i_out_tile_id{"i_out_tile_id"};
        sc_in<uint32_t> i_mvm_k{"i_mvm_k"};

        // Memory Interface to SRAM B
        sc_out<uint32_t> o_sramb_addr{"o_sramb_addr"};
        sc_out<bool> o_sramb_rden{"o_sramb_rden"};
        sc_in<wei_vector_t<X_DIM, T_WEI>> i_sramb_data{"i_sramb_data"};

        // Wavefront Output Vector towards Systolic Array (B ports)
        sc_out<wei_vector_t<X_DIM, T_WEI>> o_wei_arr{"o_wei_arr"};

        // Feeder Status Outputs
        sc_out<bool> o_wei_done{"o_wei_done"};
        sc_out<bool> o_wei_til_done{"o_wei_til_done"};
        sc_out<bool> o_fifo_empty{"o_fifo_empty"};
        sc_out<bool> o_fifo_full{"o_fifo_full"};
        sc_out<bool> o_stall{"o_stall"};

#ifndef FX1_NO_PERF
        // A1 -- non-owning perf pointer (set by NpuTop::attach_perf, like array/ctrl).
        // Feeder writes wei_l1_read_words / wei_feed_cycles / wei_stall_cycles.
        sauria_rtl::PerfCounters *perf{nullptr};
#endif

        SC_CTOR(WeightFeeder)
        {
#ifdef FX1_A3_SEAM_ORDER
            // npu_top calls seam_step() in order.
#else
            SC_METHOD(feeder_process);
            sensitive << i_clk.pos();
#endif
        }

    private:
        // Weight queues for each of the X columns
#ifdef FX1_A3_WEI_FEEDER_RTL
        // full structural RTL feeder (wei_idxcnt + X lanes + skew).
        using RtlFeeder = sauria_rtl::WeiFeederRtl<X_DIM, T_WEI, SRAMB_CAP,
                                               FIFO_DEPTH, SAURIA_WEI_IDX_W>;
        RtlFeeder rtl_feeder_;

    public:
        // for npu_top to call in sequence.
        void seam_step() { feeder_process(); }

        // set by the top level; only read under FX1_A3_CNT_EN_DIRECT.
        std::function<bool()> seam_peek_cnt_en_{};
        std::function<bool()> seam_peek_pipe_en_{};
        std::function<bool()> seam_peek_valid_{};
        std::function<bool(int)> seam_peek_fd_{};

    private:

    public:
        // Bit order of cols_active: the RTL (wei_feeder.sv:48, port declared [0:X-1]) and the stimulus generator use
        // column i <-> bit X-1-i. (With an all-ones mask the order is invisible; it matters for partial columns and
        // 64x64.) Every consumer MUST use this helper, including peek_fifo_empty_now (a mismatch locks pipeline_en).
        // -DFX1_A3_COLS_ACTIVE_LSB_OLD selects column x <-> bit x.
        static inline bool cols_active_bit(uint64_t ca, int x)
        {
#ifdef FX1_A3_COLS_ACTIVE_LSB_OLD
            return ((ca >> x) & 1ULL) != 0ULL;
#else
            const int b = X_DIM - 1 - x;
            return ((ca >> b) & 1ULL) != 0ULL;
#endif
        }

        bool peek_stall_now() const
        {
            std::array<bool, X_DIM> ca{};
            const uint64_t m = i_wei_cols_active.read();
            for (int x = 0; x < X_DIM; x++) ca[x] = cols_active_bit(m, x);
            return rtl_feeder_.peek_stall_any(i_feeder_en.read(), ca);
        }

        bool peek_fifo_empty_now() const
        {
            std::array<bool, X_DIM> ca{};
            const uint64_t m = i_wei_cols_active.read();
            for (int x = 0; x < X_DIM; x++) ca[x] = cols_active_bit(m, x);
            return rtl_feeder_.peek_fifo_empty(ca);
        }

        //, symmetric.
        bool peek_fifo_full_now() const { return rtl_feeder_.peek_fifo_full(); }

        // Debug hooks (read-only pass-through of the weight path).
        uint32_t dbg_sramb_addr() const { return rtl_feeder_.dbg_sramb_addr(); }
        bool dbg_finalpush_q2() const { return rtl_feeder_.dbg_finalpush_q2(); }
        int32_t dbg_sram_data_q0() const { return rtl_feeder_.dbg_sram_data_q0(); }
        bool dbg_done() const { return rtl_feeder_.dbg_done(); }
        bool dbg_til_done() const { return rtl_feeder_.dbg_til_done(); }
        bool dbg_push0() const { return rtl_feeder_.dbg_push0_; }
        int32_t dbg_data0() const { return rtl_feeder_.dbg_data0_; }
        // Debug hook: the value about to be PUSHED into FIFO lane 0 (din0 = regs_d[0] pre-tick; dbg_data0() is the
        // value POPPED).
        int32_t dbg_din0() const { return rtl_feeder_.dbg_din0_; }
        bool dbg_vdata() const { return rtl_feeder_.dbg_vdata_; }
        bool dbg_cnten() const { return rtl_feeder_.dbg_cnten_; }
        // Debug hooks: whether cols_active[0] reaches lane 0 (feeder_en / cols_active chain).
        bool dbg_fen0() const { return rtl_feeder_.dbg_fen0_; }
        bool dbg_in_ca0() const { return rtl_feeder_.dbg_in_ca0_; }
        bool dbg_in_fen() const { return rtl_feeder_.dbg_in_fen_; }
        bool dbg_li_fen() const { return rtl_feeder_.dbg_li_fen_; }
        bool dbg_vq1() const { return rtl_feeder_.dbg_vq1_; }
        uint32_t dbg_patn() const { return rtl_feeder_.dbg_patn_; }
        int dbg_patf() const { return rtl_feeder_.dbg_patf_; }
        uint32_t dbg_ract() const { return rtl_feeder_.dbg_ract_; }
        uint32_t dbg_elm() const { return rtl_feeder_.dbg_elm_; }
        uint32_t dbg_nfree() const { return rtl_feeder_.dbg_nfree_; }
        bool dbg_pren() const { return rtl_feeder_.dbg_pren_; }
        bool dbg_full_any() const { return rtl_feeder_.dbg_full_any_; }
        bool dbg_stall_any() const { return rtl_feeder_.dbg_stall_any_; }
        // Debug hooks: FIFO empty shim (see rtl_ref_wei_feeder_rtl.h).
        bool dbg_fifo_empty_q1() const { return rtl_feeder_.dbg_eq1_; }
        bool dbg_fifo_empty_q2() const { return rtl_feeder_.dbg_eq2_; }
        bool dbg_fifo_empty_start() const { return rtl_feeder_.dbg_estart_; }
        uint32_t dbg_fifo_ptr0() const { return rtl_feeder_.dbg_ptr0_; }
        // Debug hooks: aux_idx_ / w_idx_ / til_k_idx_.
        uint32_t dbg_aux() const { return rtl_feeder_.dbg_aux(); }
        uint32_t dbg_w() const { return rtl_feeder_.dbg_w(); }
        uint32_t dbg_tilk() const { return rtl_feeder_.dbg_tilk(); }

    private:
        bool rtl_init_{false};
        uint64_t ca_last_{0xDEADBEEFu};  // per-instance cols_active watcher
        uint64_t ca_cyc_{0};
#endif
        std::queue<T_WEI> col_fifos[X_DIM];
#ifdef FX1_A3_WEI_FEED_LANE_SHADOW
        // shadow: RTL feed_xy_lane array, M=1 per wei_feeder.sv:243.
        static constexpr int SH_I_W = 8 * (int)sizeof(T_WEI);
        static constexpr int SH_SRAM_W = SH_I_W * X_DIM;   // SRAMB_W = IB_W*X
        static constexpr int SH_DILP_W = X_DIM;            // DILP_W = SRAMB_N
        using ShLane = sauria_rtl::FeedXyLane<FIFO_DEPTH, SH_I_W, SH_SRAM_W, SH_DILP_W, 1>;
        ShLane sh_lane[X_DIM];
        bool sh_init{false};
        // Push request captured at the emit site, applied when the lanes tick.
        bool sh_push_pending{false};
        wei_vector_t<X_DIM, T_WEI> sh_push_vec{};
        uint64_t sh_cycle{0};
        // real ported wei_idxcnt driving glob_woffs / transn.
        // Derived widths per -- NOT the RTL module defaults.
        static constexpr int sh_clog2(int n)
        {
            int b = 0;
            while ((1 << b) < n)
                b++;
            return b;
        }
        static constexpr int SH_SRAMB_DEPTH =
            (SRAMB_CAP / (int)(sizeof(T_WEI) * X_DIM)) > 0
                ? (SRAMB_CAP / (int)(sizeof(T_WEI) * X_DIM))
                : 1;
        static constexpr int SH_WOFS_W = sh_clog2(X_DIM);
        static constexpr int SH_ADRB_W = sh_clog2(SH_SRAMB_DEPTH);
        sauria_rtl::WeiIdxCnt<SAURIA_WEI_IDX_W, SH_ADRB_W, SH_WOFS_W> sh_idx;
        uint32_t sh_glob_woffs{0};
        bool sh_transn{false};
#endif
        std::vector<T_WEI> skew_regs[X_DIM];

        // Address tracking register
        uint32_t addr_reg{0};
        uint32_t incnt{0};
        // SAURIA-style WEIGHT address generator counters
        uint32_t wei_w_cnt{0};
        uint32_t wei_k_cnt{0};
        uint32_t wei_til_k_cnt{0};
        uint32_t wei_pop_count{0};

        bool delay_first_weight_pop{false};

        uint32_t wei_out_count{0};
        bool wei_out_started{false};

        // Static A/B phase-compensation counter (see WEI_POP_DELAY).
        // Counts down zero-output cycles before the first real weight pop.
        // Armed once per context when pop starts and the FIFO has real data.
        uint32_t wei_pop_delay_cnt{0};
        bool wei_pop_delay_armed{false};

        // Debug/functional SAURIA weight stream reconstruction.
        // SRAMB flat layout is treated as flat[x*K + k].
        // SA logical stream needs B[k][x].
        std::vector<T_WEI> wei_flat_buf;

        bool wei_stream_init{false};

        uint32_t wei_k_len{0};
        uint32_t wei_total_elems{0};
        uint32_t wei_total_words{0};

        uint32_t wei_word_req_idx{0};
        uint32_t wei_emit_count{0};

        std::queue<uint32_t> pending_word_addr_q;

        // Detect rising edge of cnt_clear
        bool cnt_clear_q{false};

        // SRAM read latency matching registers
        bool rden_q1{false};
        bool rden_q2{false};
        bool rdata_valid{false};

        bool use_sauria_weight_addr_gen()
        {
            return (
                i_wei_wlim.read() != 0 &&
                i_wei_wstep.read() != 0);
        }

        void advance_sauria_weight_addr_gen()
        {
            uint32_t next_w = wei_w_cnt + i_wei_wstep.read();

            if (next_w < i_wei_wlim.read())
            {
                wei_w_cnt = next_w;
                return;
            }

            wei_w_cnt = 0;

            uint32_t next_k = wei_k_cnt + i_wei_kstep.read();

            if (i_wei_klim.read() != 0 && next_k < i_wei_klim.read())
            {
                wei_k_cnt = next_k;
                return;
            }

            wei_k_cnt = 0;

            uint32_t next_til_k = wei_til_k_cnt + i_wei_til_kstep.read();

            if (i_wei_til_klim.read() != 0 && next_til_k < i_wei_til_klim.read())
            {
                wei_til_k_cnt = next_til_k;
            }
            else
            {
                wei_til_k_cnt = 0;
            }
        }
        uint32_t get_effective_k() const
        {
            uint32_t k = i_mvm_k.read();

            if (k != 0)
            {
                return k;
            }

            uint32_t total_elems = i_wei_incntlim.read();

            if (total_elems == 0)
            {
                total_elems = i_wei_wlim.read();
            }

            if (total_elems != 0)
            {
                k = total_elems / X_DIM;
            }

            if (k == 0)
            {
                k = 1;
            }

            return k;
        }
        void feeder_process()
        {
#ifdef FX1_A3_WEI_PORT_AUDIT
            {
                static std::ofstream wa("trace_sysc/wei_port_audit.csv");
                static bool wa_hdr = false;
                static uint64_t wa_cyc = 0;
                static std::map<std::string, std::string> wa_last;
                static std::map<std::string, uint64_t> wa_cen_run;   // consecutive cnt_en
                static std::map<std::string, uint64_t> wa_cen_max;
                if (!wa_hdr)
                {
                    wa << "cyc,inst,rstn,fclear,clrfifo,fen,cen,cclr,pop,valid,"
                          "finalpush,cswitch,ctxid,cen_run,cen_max" << "\n";
                    wa_hdr = true;
                }
                std::string wnm = this->name();
                // Longest unbroken cnt_en stretch -- the quantity that decides
                // whether o_done (needs 784 consecutive) can ever fire.
                if (i_cnt_en.read())
                    wa_cen_run[wnm]++;
                else
                    wa_cen_run[wnm] = 0;
                if (wa_cen_run[wnm] > wa_cen_max[wnm])
                    wa_cen_max[wnm] = wa_cen_run[wnm];

                std::ostringstream wk;
                wk << (int)i_rstn.read() << (int)i_feeder_clear.read()
                   << (int)i_clearfifo.read() << (int)i_feeder_en.read()
                   << (int)i_cnt_en.read() << (int)i_cnt_clear.read()
                   << (int)i_pop_en.read() << (int)i_valid.read()
                   << (int)i_finalpush.read() << (int)i_cswitch.read()
                   << "_" << i_context_id.read();
                if (wa_last[wnm] != wk.str())
                {
                    wa_last[wnm] = wk.str();
                    wa << wa_cyc << "," << wnm << ","
                       << (int)i_rstn.read() << "," << (int)i_feeder_clear.read() << ","
                       << (int)i_clearfifo.read() << "," << (int)i_feeder_en.read() << ","
                       << (int)i_cnt_en.read() << "," << (int)i_cnt_clear.read() << ","
                       << (int)i_pop_en.read() << "," << (int)i_valid.read() << ","
                       << (int)i_finalpush.read() << "," << (int)i_cswitch.read() << ","
                       << i_context_id.read() << "," << wa_cen_run[wnm] << ","
                       << wa_cen_max[wnm] << "\n";
                    wa.flush();
                }
                wa_cyc++;
            }
#endif

#ifdef FX1_A3_WEI_FEEDER_RTL
            {
                if (!i_rstn.read())
                {
                    rtl_feeder_.reset();
                    rtl_init_ = true;
                    o_sramb_addr.write(0);
                    o_sramb_rden.write(false);
                    o_wei_arr.write(wei_vector_t<X_DIM, T_WEI>(static_cast<T_WEI>(0)));
                    o_wei_done.write(false);
                    o_wei_til_done.write(false);
                    o_fifo_empty.write(true);
                    o_fifo_full.write(false);
                    o_stall.write(false);
                    return;
                }
                if (!rtl_init_)
                {
                    rtl_feeder_.reset();
                    rtl_init_ = true;
                }

                typename RtlFeeder::Inputs ri;
                wei_vector_t<X_DIM, T_WEI> md = i_sramb_data.read();
                for (int x = 0; x < X_DIM; x++)
                    ri.sramb_data[x] = static_cast<int32_t>(md[x]);

                const uint64_t ca = i_wei_cols_active.read();
                for (int x = 0; x < X_DIM; x++)
                    ri.cols_active[x] = cols_active_bit(ca, x);

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
                ri.cswitch = seam_peek_fd_ && (FX1_A3_FDFSM_MASK & (1u<<7)) ? seam_peek_fd_(7) : (i_cswitch.read());
#else
                ri.cswitch = i_cswitch.read();
#endif
                ri.waligned = (i_wei_waligned.read() != 0);
                ri.auxlim = i_wei_klim.read();
                ri.auxstep = i_wei_kstep.read();
                ri.wlim = i_wei_wlim.read();
                ri.wstep = i_wei_wstep.read();
                ri.til_klim = i_wei_til_klim.read();
                ri.til_kstep = i_wei_til_kstep.read();
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
                ri.wei_valid = seam_peek_valid_ ? seam_peek_valid_() : i_valid.read();
#else
                ri.wei_valid = i_valid.read();
#endif
#ifdef FX1_A3_FDFSM_DIRECT
                ri.finalpush = seam_peek_fd_ && (FX1_A3_FDFSM_MASK & (1u<<3)) ? seam_peek_fd_(3) : (i_finalpush.read());
#else
                ri.finalpush = i_finalpush.read();
#endif
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

                // watch i_wei_cols_active on EVERY NPU instance (std/approx/
                // gated), NOT just NpuTop_std --'s leading hypothesis is that
                // the config write does not reach all three config_regs.
                {
                    const uint64_t ca_now = i_wei_cols_active.read();
                    if (ca_now != ca_last_)
                    {
#if FX1_DUMPS
                        static std::ofstream cw("trace_sysc/cols_active_watch.csv");
                        static bool ch = false;
                        if (!ch) { cw << "inst,cyc,old,new\n"; ch = true; }
                        cw << this->name() << "," << ca_cyc_ << ","
                           << ca_last_ << "," << ca_now << "\n";
                        cw.flush();
#endif
                        ca_last_ = ca_now;
                    }
                    ca_cyc_++;
                }

                auto ro = rtl_feeder_.tick(ri);
#ifdef FX1_A3_WEI_TAPE
                // Debug hook (default off), symmetric to FX1_A3_FEED_TAPE: one line EVERY cycle, unfiltered, std instance
                // only; joins on tsim.
                if (std::string(this->name()).find("NpuTop_std") !=
                    std::string::npos)
                {
                    static std::ofstream wt("trace_sysc/wei_tape.csv");
                    static bool wt_hdr = false;
                    if (!wt_hdr)
                    {
                        wt << "tsim,cnt_en,pop_en,pipe_en,cnt_clear,feeder_en,"
                              "ctx,sramb_addr,b0,b1,rden,clrf,ptr,push,pop,full\n";
                        wt_hdr = true;
                    }
                    wt << sc_core::sc_time_stamp().value() << ","
                       << (int)ri.cnt_en << "," << (int)ri.pop_en << ","
                       << (int)ri.pipeline_en << "," << (int)ri.cnt_clear << ","
                       << (int)ri.feeder_en << ","
                       << i_context_id.read() << ","
                       << ro.sramb_addr << ","
                       << (long long)ro.b_arr[0] << ","
                       << (long long)ro.b_arr[1] << ","
                       << (int)ro.sramb_rden << ","
                       << (int)ri.clearfifo << ","
                       << rtl_feeder_.dbg_ptr0_ << ","
                       << (int)rtl_feeder_.dbg_push0_ << ","
                       << (int)rtl_feeder_.dbg_pop0_ << ","
                       << (int)rtl_feeder_.dbg_full0_ << "\n";
                }
#endif

#ifdef FX1_A3_WEI_RTL_TRACE
                // per-cycle (address out, word in) so the RTL path can be
                // compared against the behavioural path's weight_raw_buf.csv.
                if (std::string(this->name()).find("NpuTop_std") != std::string::npos)
                {
                    static std::ofstream rt("trace_sysc/wei_rtl_addr.csv");
                    static bool h = false;
                    static uint64_t rc = 0;
                    if (!h)
                    {
                        rt << "cyc,context,addr,rden,empty,full,stall,"
                              "cnt_en,pop_en,feeder_en,valid,cnt_clear,clrfifo,cswitch,"
                              "push0,rawempty0,rawstall0,lanedata0,"
                              "vq1,upd,fen0,vdata,woffs,sidx,rptr,patn,patf,ract,"
                              "ca_raw,in_fen,in_ca0,li_fen,li_upd,li_vd,li_xov,li_gw,li_lw,"
                              "elm,nfree,fdmpipe,pren,cnten,fullany,stallany";
                        for (int q = 0; q < 4; q++)
                            rt << ",in" << q;
                        for (int q = 0; q < 4; q++)
                            rt << ",out" << q;
                        rt << "\n";
                        h = true;
                    }
                    rt << rc++ << "," << i_context_id.read() << ","
                       << ro.sramb_addr << "," << (int)ro.sramb_rden << ","
                       << (int)ro.fifo_empty << "," << (int)ro.fifo_full << ","
                       << (int)ro.feeder_stall << ","
                       << (int)i_cnt_en.read() << "," << (int)i_pop_en.read() << ","
                       << (int)i_feeder_en.read() << "," << (int)i_valid.read() << ","
                       << (int)i_cnt_clear.read() << "," << (int)i_clearfifo.read() << ","
                       << (int)i_cswitch.read() << ","
                       << (int)rtl_feeder_.dbg_push0_ << ","
                       << (int)rtl_feeder_.dbg_empty0_ << ","
                       << (int)rtl_feeder_.dbg_stall0_ << ","
                       << (int)rtl_feeder_.dbg_data0_ << ","
                       << (int)rtl_feeder_.dbg_vq1_ << "," << (int)rtl_feeder_.dbg_upd_ << ","
                       << (int)rtl_feeder_.dbg_fen0_ << "," << (int)rtl_feeder_.dbg_vdata_ << ","
                       << rtl_feeder_.dbg_woffs_ << "," << rtl_feeder_.dbg_sidx_ << ","
                       << rtl_feeder_.dbg_rptr_ << "," << rtl_feeder_.dbg_patn_ << ","
                       << rtl_feeder_.dbg_patf_ << "," << rtl_feeder_.dbg_ract_ << ","
                       << i_wei_cols_active.read() << ","
                       << (int)rtl_feeder_.dbg_in_fen_ << "," << (int)rtl_feeder_.dbg_in_ca0_ << ","
                       << (int)rtl_feeder_.dbg_li_fen_ << "," << (int)rtl_feeder_.dbg_li_upd_ << ","
                       << (int)rtl_feeder_.dbg_li_vd_ << "," << (int)rtl_feeder_.dbg_li_xov_ << ","
                       << rtl_feeder_.dbg_li_gw_ << "," << rtl_feeder_.dbg_li_lw_ << ","
                       << rtl_feeder_.dbg_elm_ << "," << rtl_feeder_.dbg_nfree_ << ","
                       << (int)rtl_feeder_.dbg_fdmpipe_ << "," << (int)rtl_feeder_.dbg_pren_ << ","
                       << (int)rtl_feeder_.dbg_cnten_ << "," << (int)rtl_feeder_.dbg_full_any_ << ","
                       << (int)rtl_feeder_.dbg_stall_any_;
                    for (int q = 0; q < 4; q++)
                        rt << "," << (int)md[q];
                    for (int q = 0; q < 4; q++)
                        rt << "," << (int)ro.b_arr[q];
                    rt << "\n";
                }
#endif

                o_sramb_addr.write(i_wei_base_addr.read() + ro.sramb_addr);
                o_sramb_rden.write(ro.sramb_rden);
                wei_vector_t<X_DIM, T_WEI> ov;
                for (int x = 0; x < X_DIM; x++)
                    ov[x] = static_cast<T_WEI>(ro.b_arr[x]);
#ifdef FX1_A3_WEI_DELAY
                // Diagnostic experiment (default off), not a fix: delays WEI by 3 cycles to realign the two streams.
                {
                    // Per INSTANCE: several NpuTop instances (std / approx / gated) share this code; a plain static variable would
                    // merge their pipelines.
#ifndef FX1_A3_WEI_DELAY_N
#define FX1_A3_WEI_DELAY_N 1
#endif
                    using dly_t = std::array<wei_vector_t<X_DIM, T_WEI>,
                                             FX1_A3_WEI_DELAY_N>;
                    static std::map<std::string, dly_t> dly_map;
                    auto it = dly_map.find(std::string(this->name()));
                    if (it == dly_map.end())
                    {
                        dly_t z;
                        for (auto &d : z)
                            d = wei_vector_t<X_DIM, T_WEI>(static_cast<T_WEI>(0));
                        it = dly_map.emplace(std::string(this->name()), z).first;
                    }
                    dly_t &dly = it->second;
#ifdef FX1_A3_WEI_DELAY_GATED
                    // Diagnostic (default off): shift ONLY when the pipeline advances (ri.pipeline_en, the RTL condition for
                    // each lane's fifo_pop); shifting every cycle, stalls included, would skew the stream instead of delaying it.
                    if (ri.pipeline_en)
                    {
                        wei_vector_t<X_DIM, T_WEI> in_now = ov;
                        ov = dly[FX1_A3_WEI_DELAY_N - 1];
                        for (int q = FX1_A3_WEI_DELAY_N - 1; q > 0; q--)
                            dly[q] = dly[q - 1];
                        dly[0] = in_now;
                    }
                    else
                    {
                        // stall: keep the delay line, outputs hold their value
                        ov = dly[FX1_A3_WEI_DELAY_N - 1];
                    }
#else
                    wei_vector_t<X_DIM, T_WEI> out_now = dly[FX1_A3_WEI_DELAY_N - 1];
                    for (int q = FX1_A3_WEI_DELAY_N - 1; q > 0; q--)
                        dly[q] = dly[q - 1];
                    dly[0] = ov;
                    ov = out_now;
#endif
                }
#endif
                o_wei_arr.write(ov);
                o_wei_done.write(ro.done);
                o_wei_til_done.write(ro.til_done);
                o_fifo_empty.write(ro.fifo_empty);
                o_fifo_full.write(ro.fifo_full);
                o_stall.write(ro.feeder_stall);
                return;
            }
#endif
            if (!i_rstn.read() || i_feeder_clear.read() || i_clearfifo.read())
            {
                o_sramb_addr.write(0);
                o_sramb_rden.write(false);
                o_wei_arr.write(wei_vector_t<X_DIM, T_WEI>(static_cast<T_WEI>(0)));
                o_wei_done.write(false);
                o_wei_til_done.write(false);
                o_fifo_empty.write(true);
                o_fifo_full.write(false);
                o_stall.write(false);
                addr_reg = 0;
                incnt = 0;
                wei_w_cnt = 0;
                wei_k_cnt = 0;
                wei_til_k_cnt = 0;
                wei_pop_count = 0;

                cnt_clear_q = false;
                rden_q1 = false;
                rden_q2 = false;

                delay_first_weight_pop = false;
                wei_pop_delay_armed = false;
                wei_pop_delay_cnt = 0;

                wei_flat_buf.clear();

                wei_stream_init = false;
                wei_k_len = 0;
                wei_total_elems = 0;
                wei_total_words = 0;
                wei_word_req_idx = 0;
                wei_emit_count = 0;

                wei_out_count = 0;
                wei_out_started = false;

                while (!pending_word_addr_q.empty())
                    pending_word_addr_q.pop();
                for (int i = 0; i < X_DIM; i++)
                {
                    while (!col_fifos[i].empty())
                        col_fifos[i].pop();

                    // RTL feed_registers: WEIGHT lane x is delayed by x cycles.
                    skew_regs[i].assign(i, static_cast<T_WEI>(0));
                }
            }

            if (!i_feeder_en.read())
            {
                o_sramb_rden.write(false);
                o_wei_arr.write(wei_vector_t<X_DIM, T_WEI>(static_cast<T_WEI>(0)));
                rden_q1 = false;
                rden_q2 = false;
                return;
            }

            // Default SRAM read disable each cycle
            o_sramb_rden.write(false);

            // cnt_clear should behave like a pulse.
            // If controller holds it high for many cycles, feeder only clears once.
            bool cnt_clear_now = i_cnt_clear.read();
            bool cnt_clear_pulse = cnt_clear_now && !cnt_clear_q;
            cnt_clear_q = cnt_clear_now;

            if (cnt_clear_pulse)
            {
                addr_reg = 0;
                incnt = 0;

                wei_w_cnt = 0;
                wei_k_cnt = 0;
                wei_til_k_cnt = 0;

                rden_q1 = false;
                rden_q2 = false;

                wei_flat_buf.clear();
                delay_first_weight_pop = false;
                wei_pop_delay_armed = false;
                wei_pop_delay_cnt = 0;
                wei_stream_init = false;
                wei_k_len = 0;
                wei_total_elems = 0;
                wei_total_words = 0;
                wei_word_req_idx = 0;
                wei_emit_count = 0;
                wei_pop_count = 0;

                wei_out_count = 0;
                wei_out_started = false;

                while (!pending_word_addr_q.empty())
                    pending_word_addr_q.pop();

                for (int i = 0; i < X_DIM; i++)
                {
                    while (!col_fifos[i].empty())
                        col_fifos[i].pop();

                    skew_regs[i].assign(i, static_cast<T_WEI>(0));
                }

                static int dbg_weight_clear_count = 0;
                if (dbg_weight_clear_count < 16)
                {
                    DBG_COUT << "[WEIGHT CLEAR PULSE]"
                              << " feeder_en=" << i_feeder_en.read()
                              << " cnt_en=" << i_cnt_en.read()
                              << " counters reset to 0"
                              << std::endl;
                }
                dbg_weight_clear_count++;

                // return;
            }

            // SRAM read latency matching.
            // Data is valid one cycle after rden -- but the code below used rden_q2 (2-stage),
            // with FX1_A3_SRAMB_RDEN_PHASE (default ON) the SRAM is a raw combinational pass-through (1-cycle sc_signal
            // visibility only). FX1_A3_FEEDER_RDEN_MATCH_SRAM (opt-in, default off) gates on rden_q1 alone, as in
            // rtl_ref_ifmap_feeder.h.
#ifdef FX1_A3_FEEDER_RDEN_MATCH_SRAM
            bool mem_data_valid = rden_q1;
            rden_q1 = false;
#else
            bool mem_data_valid = rden_q2;

            // Advance read-valid pipeline.
            // This matches the SRAM latency behavior we saw in IFMAP.
            rden_q2 = rden_q1;
            rden_q1 = false;
#endif

            if (mem_data_valid)
            {
                wei_vector_t<X_DIM, T_WEI> mem_data = i_sramb_data.read();

                uint32_t use_word_addr = 0;

                if (!pending_word_addr_q.empty())
                {
                    use_word_addr = pending_word_addr_q.front();
                    pending_word_addr_q.pop();
                }

                std::string inst_name = this->name();

                if (inst_name.find("NpuTop_std") != std::string::npos)
                {
                    static std::ofstream wei_raw_trace("trace_sysc/weight_sram_flat.csv");
                    static bool wei_raw_header = false;
                    static uint32_t wei_raw_count = 0;

                    if (!wei_raw_header)
                    {
                        wei_raw_trace << "idx,word_addr,lane,value\n";
                        wei_raw_header = true;
                    }

                    for (int x = 0; x < X_DIM; x++)
                    {
                        wei_raw_trace
                            << wei_raw_count << ","
                            << use_word_addr << ","
                            << x << ","
                            << static_cast<int32_t>(mem_data[x])
                            << "\n";

                        wei_raw_count++;
                    }

                    wei_raw_trace.flush();
                }

                // Append one SRAMB word into flat weight buffer.
                for (int x = 0; x < X_DIM; x++)
                {
                    wei_flat_buf.push_back(mem_data[x]);
                }
                static std::ofstream raw_wei_trace("trace_sysc/weight_raw_buf.csv");
                static bool raw_header = false;
                static uint32_t raw_count = 0;

                if (inst_name.find("NpuTop_std") != std::string::npos)
                {
                    if (!raw_header)
                    {
                        raw_wei_trace << "context,out_tile,word_addr,seq_word";
                        for (int x = 0; x < X_DIM; x++)
                        {
                            raw_wei_trace << ",raw" << x;
                        }
                        raw_wei_trace << "\n";
                        raw_header = true;
                    }

                    raw_wei_trace << i_context_id.read()
                                  << "," << i_out_tile_id.read()
                                  << "," << use_word_addr
                                  << "," << raw_count;

                    for (int x = 0; x < X_DIM; x++)
                    {
                        raw_wei_trace << "," << static_cast<int32_t>(mem_data[x]);
                    }

                    raw_wei_trace << "\n";
                    raw_wei_trace.flush();

                    raw_count++;
                }
                static int dbg_wei_mem_word_count = 0;
                if (inst_name.find("NpuTop_std") != std::string::npos &&
                    dbg_wei_mem_word_count < 32)
                {
                    DBG_COUT << "[WEIGHT MEM WORD]"
                              << " count=" << dbg_wei_mem_word_count
                              << " word_addr=" << use_word_addr
                              << " data=[";

                    for (int x = 0; x < X_DIM; x++)
                    {
                        DBG_COUT << static_cast<int32_t>(mem_data[x]);
                        if (x < X_DIM - 1)
                            DBG_COUT << ", ";
                    }

                    DBG_COUT << "]" << std::endl;
                }

                dbg_wei_mem_word_count++;

                // Emit logical B_Mat_mvm vectors once enough flat data is available.
                // Layout weight follow row-major
                //   flat[k * X_DIM + x]
                // if (wei_stream_init && wei_k_len != 0)
                // {
                //     if (wei_emit_count < wei_k_len)
                //     {
                //         bool enough_data = true;

                //         for (int x = 0; x < X_DIM; x++)
                //         {
                //             uint32_t idx = wei_emit_count * X_DIM + x;

                //             if (idx >= wei_flat_buf.size())
                //             {
                //                 enough_data = false;
                //                 break;
                //             }
                //         }

                //         if (enough_data)
                //         {
                //             wei_vector_t<X_DIM, T_WEI> feed_vec;

                //             for (int x = 0; x < X_DIM; x++)
                //             {
                //                 uint32_t idx = wei_emit_count * X_DIM + x;
                //                 T_WEI v = wei_flat_buf[idx];

                //                 feed_vec[x] = v;
                //                 col_fifos[x].push(v);
                //             }

                //             static int dbg_wei_feed_count = 0;

                //             if (inst_name.find("NpuTop_std") != std::string::npos &&
                //                 dbg_wei_feed_count < 64)
                //             {
                //                 DBG_COUT << "[WEIGHT FEED VEC]"
                //                           << " count=" << dbg_wei_feed_count
                //                           << " k=" << wei_emit_count
                //                           << " feed_vec=[";

                //                 for (int x = 0; x < X_DIM; x++)
                //                 {
                //                     DBG_COUT << static_cast<int32_t>(feed_vec[x]);
                //                     if (x < X_DIM - 1)
                //                         DBG_COUT << ", ";
                //                 }

                //                 DBG_COUT << "]" << std::endl;
                //             }

                //             dbg_wei_feed_count++;
                //             wei_emit_count++;
                //         }
                //     }
                // }
            }

            // ---------------------------------------------------------
            // Emit one WEIGHT logical vector per cycle when enough data
            // is available in wei_flat_buf.
            //
            // Verified SAURIA B_Mat_mvm layout:
            //
            //     B[k][x] = flat[k * X_DIM + x]
            //
            // Do NOT use:
            //     flat[x * wei_k_len + k]
            //
            // That transpose layout is wrong for the current SAURIA test.
            // ---------------------------------------------------------
            if (wei_stream_init && wei_k_len != 0)
            {
                if (wei_emit_count < wei_k_len)
                {
                    bool enough_data = true;

                    for (int x = 0; x < X_DIM; x++)
                    {
                        uint32_t idx =
                            wei_emit_count * X_DIM + static_cast<uint32_t>(x);

                        if (idx >= wei_flat_buf.size())
                        {
                            enough_data = false;
                            break;
                        }
                    }

                    if (enough_data)
                    {
                        wei_vector_t<X_DIM, T_WEI> feed_vec;

                        for (int x = 0; x < X_DIM; x++)
                        {
                            uint32_t idx =
                                wei_emit_count * X_DIM + static_cast<uint32_t>(x);

                            T_WEI v = static_cast<T_WEI>(0);

                            if (idx < wei_flat_buf.size())
                            {
                                v = wei_flat_buf[idx];
                            }

                            feed_vec[x] = v;
                            col_fifos[x].push(v);
#ifdef FX1_A3_WEI_FEED_LANE_SHADOW
                            sh_push_vec[x] = v;
                            sh_push_pending = true;
#endif
                        }

                        std::string inst_name = this->name();

                        static int dbg_wei_feed_count = 0;

                        if (inst_name.find("NpuTop_std") != std::string::npos &&
                            dbg_wei_feed_count < 64)
                        {
                            DBG_COUT << "[WEIGHT FEED VEC]"
                                      << " count=" << dbg_wei_feed_count
                                      << " context=" << i_context_id.read()
                                      << " out_tile=" << i_out_tile_id.read()
                                      << " k=" << wei_emit_count
                                      << " flat_base_idx=" << (wei_emit_count * X_DIM)
                                      << " feed_vec=[";

                            for (int x = 0; x < X_DIM; x++)
                            {
                                DBG_COUT << static_cast<int32_t>(feed_vec[x]);

                                if (x < X_DIM - 1)
                                {
                                    DBG_COUT << ", ";
                                }
                            }

                            DBG_COUT << "]" << std::endl;

                            dbg_wei_feed_count++;
                        }

                        wei_emit_count++;
                    }
                }
            }

            if (i_cnt_en.read())
            {
                bool within_limit = (incnt < i_wei_incntlim.read());

                if (within_limit)
                {
                    bool sauria_weight_mode = use_sauria_weight_addr_gen();

                    if (sauria_weight_mode)
                    {
                        if (!wei_stream_init)
                        {
                            // Current test:
                            //   wei_incntlim = 9216 total elements
                            //   X_DIM = 16
                            //   K = 9216 / 16 = 576
                            wei_k_len = get_effective_k();

                            wei_total_elems = wei_k_len * X_DIM;

                            wei_total_words = wei_k_len;

                            wei_word_req_idx = 0;
                            wei_emit_count = 0;
                            wei_flat_buf.clear();

                            while (!pending_word_addr_q.empty())
                                pending_word_addr_q.pop();

                            wei_stream_init = true;

                            // Arm the static A/B phase-compensation delay for
                            // this context. Applied uniformly every context.
                            wei_pop_delay_armed = true;
                            wei_pop_delay_cnt = (uint32_t)WEI_POP_DELAY;

                            DBG_COUT << "[WEIGHT STREAM INIT]"
                                      << "time=" << sc_time_stamp()
                                      << " context=" << i_context_id.read()
                                      << " total_elems=" << wei_total_elems
                                      << " k_len=" << wei_k_len
                                      << " total_words=" << wei_total_words
                                      << std::endl;

                            // NOTE: delay_first_weight_pop removed.
                            // The per-context phase skew between A and B is now
                            // handled by the controller ARRAY_FILL phase, which
                            // pre-fills BOTH feeder FIFOs in lock-step before any
                            // pop. A weight-only delay here would re-introduce a
                            // B-vs-A phase offset (the +3 cycle CTX>0 error).
                        }

                        if (wei_word_req_idx < wei_total_words)
                        {
                            // uint32_t words_per_out_tile = 0;

                            // if (i_wei_incntlim.read() != 0)
                            // {
                            //     words_per_out_tile = i_wei_incntlim.read() / X_DIM;
                            // }

                            // if (words_per_out_tile == 0)
                            // {
                            //     words_per_out_tile = wei_total_words;
                            // }

                            uint32_t words_per_out_tile = wei_k_len;
                            // MULTI-TILE FIX: output_tiles are folded into ncontexts
                            // (= Ch x n_tiles); i_out_tile_id stays 0. Derive the cout
                            // group from the context: n_tiles = til_klim/til_kstep,
                            // ctx_per_tile = ncontexts/n_tiles, cout_group =
                            // context_id/ctx_per_tile. Single-tile (n_tiles=1) => 0.
                            uint32_t wt_ntiles = (i_wei_til_kstep.read() != 0) ? (i_wei_til_klim.read() / i_wei_til_kstep.read()) : 1;
                            if (wt_ntiles == 0) wt_ntiles = 1;
                            uint32_t wt_nc = i_ncontexts.read() ? i_ncontexts.read() : 1;
                            uint32_t wt_cpt = (wt_ntiles != 0) ? (wt_nc / wt_ntiles) : wt_nc;
                            if (wt_cpt == 0) wt_cpt = 1;
                            uint32_t wt_group = i_context_id.read() / wt_cpt;
                            (void)words_per_out_tile;
                            // Weights for the n_tiles cout groups are INTERLEAVED per
                            // k-element in SRAM B (word 0=k0g0, 1=k0g1, 2=k1g0, ...).
                            // So group g reads words g, g+n_tiles, g+2*n_tiles, ...
                            uint32_t req_word_addr = (i_out_tile_id.read() + wei_word_req_idx * wt_ntiles) + wt_group;
                            uint32_t req_final_addr = i_wei_base_addr.read() + req_word_addr;
                            pending_word_addr_q.push(req_word_addr);

                            o_sramb_rden.write(true);
                            o_sramb_addr.write(req_final_addr);

                            rden_q1 = true;
#ifndef FX1_A3_FEEDER_PERF_NO_PERF
                            if (perf) perf->wei_l1_read_words++;   // A1: 1 SRAMB word read
#endif

                            static int dbg_wei_req_count = 0;
                            std::string inst_name = this->name();

                            if (inst_name.find("NpuTop_std") != std::string::npos &&
                                dbg_wei_req_count < 64)
                            {
                                DBG_COUT << "[WEIGHT WORD REQ]"
                                          << " req=" << dbg_wei_req_count
                                          << " word_addr=" << req_word_addr
                                          << " final_addr=" << req_final_addr
                                          << " total_words=" << wei_total_words
                                          << std::endl;
                            }

                            dbg_wei_req_count++;

                            wei_word_req_idx++;
                        }
                        else
                        {
                            o_sramb_rden.write(false);
                        }
                    }
                    else
                    {
                        uint32_t final_addr =
                            i_wei_base_addr.read() + addr_reg;

                        o_sramb_rden.write(true);
                        o_sramb_addr.write(final_addr);

                        addr_reg += i_wei_incntstep.read();

                        rden_q1 = true;
#ifndef FX1_A3_FEEDER_PERF_NO_PERF
                        if (perf) perf->wei_l1_read_words++;   // A1: 1 SRAMB word read
#endif
                    }

                    incnt++;
                }
                // else
                // {
                //     o_sramb_rden.write(false);
                // }
            }
            else
            {
                o_sramb_rden.write(false);
                rden_q1 = false;
            }

            bool wei_can_pop = true;
            for (int x = 0; x < X_DIM; x++)
            {
                if (col_fifos[x].empty())
                {
                    wei_can_pop = false;
                    break;
                }
            }
            bool wei_need_more = i_feeder_en.read() && (!wei_stream_init || (wei_pop_count < wei_k_len));
            o_stall.write(wei_need_more && !wei_can_pop);
#ifndef FX1_A3_FEEDER_PERF_NO_PERF
            if (perf && wei_need_more && !wei_can_pop) perf->wei_stall_cycles++;   // A1: SRAM-B backpressure
#endif

            // ---------------------------------------------------------
            // Pop/shift WEIGHT physical stream.
            //
            // After K logical weight vectors are popped, col_fifos become empty,
            // but skew_regs[x] still contain delayed tail values for x > 0.
            // During DRAIN_FEED, keep shifting zeros into skew_regs to flush tail.
            // ---------------------------------------------------------
            if (i_pop_en.read())
            {
                wei_vector_t<X_DIM, T_WEI> popped_vec;
                wei_vector_t<X_DIM, T_WEI> wei_out;

                bool has_logical_vector =
                    (wei_pop_count < wei_k_len);

                for (int x = 0; x < X_DIM; x++)
                {
                    if (col_fifos[x].empty())
                    {
                        has_logical_vector = false;
                        break;
                    }
                }

                bool had_real_vector = has_logical_vector;
#ifndef FX1_A3_FEEDER_PERF_NO_PERF
                if (perf && had_real_vector) perf->wei_feed_cycles++;   // A1: real weight vector -> array
#endif

                for (int x = 0; x < X_DIM; x++)
                {
                    T_WEI popped = static_cast<T_WEI>(0);

                    if (has_logical_vector)
                    {
                        popped = col_fifos[x].front();
                        col_fifos[x].pop();
                    }

                    popped_vec[x] = popped;

                    if (x == 0)
                    {
                        wei_out[x] = popped;
                    }
                    else
                    {
                        // Keep exactly x-cycle skew.
                        while (skew_regs[x].size() < static_cast<size_t>(x))
                        {
                            skew_regs[x].insert(
                                skew_regs[x].begin(),
                                static_cast<T_WEI>(0));
                        }

                        // This push must happen even during tail flush, with popped=0.
                        skew_regs[x].push_back(popped);

                        wei_out[x] = skew_regs[x].front();

                        skew_regs[x].erase(skew_regs[x].begin());
                    }
                }

                o_wei_arr.write(wei_out);

                // Dump only real logical vectors, not tail flush zeros.
                std::string inst_name = this->name();

                if (inst_name.find("NpuTop_std") != std::string::npos)
                {
                    static std::ofstream wei_pop_trace("trace_sysc/weight_pop_logical.csv");
                    static bool wei_pop_header = false;
                    static uint32_t dbg_wei_rows = 0;

                    if (!wei_pop_header)
                    {
                        wei_pop_trace << "context,out_tile,k";

                        for (int xx = 0; xx < X_DIM; xx++)
                        {
                            wei_pop_trace << ",wei" << xx;
                        }

                        wei_pop_trace << "\n";
                        wei_pop_header = true;
                    }

                    uint32_t trace_k = wei_pop_count;

                    if (had_real_vector && dbg_wei_rows < 8192)
                    {
                        wei_pop_trace << i_context_id.read()
                                      << "," << i_out_tile_id.read()
                                      << "," << trace_k;

                        for (int xx = 0; xx < X_DIM; xx++)
                        {
                            wei_pop_trace << ","
                                          << static_cast<int32_t>(popped_vec[xx]);
                        }

                        wei_pop_trace << "\n";
                        wei_pop_trace.flush();

                        dbg_wei_rows++;
                    }
                }

                if (had_real_vector && wei_pop_count < wei_k_len)
                {
                    wei_pop_count++;
                }
            }
            else
            {
                o_wei_arr.write(wei_vector_t<X_DIM, T_WEI>(static_cast<T_WEI>(0)));
            }

            uint32_t wei_tail_limit = wei_k_len + (X_DIM - 1);

            bool wei_pop_done = wei_stream_init && wei_out_started && (wei_out_count >= wei_tail_limit);

            o_wei_done.write(wei_pop_done);
            o_wei_til_done.write(wei_pop_done);

            static int dbg_wei_done_count = 0;
            if (this->name() && dbg_wei_done_count < 128)
            {
                std::string inst_name = this->name();

                if (inst_name.find("NpuTop_std") != std::string::npos)
                {
                    DBG_COUT << "[WEIGHT DONE STATUS]"
                              << " context=" << i_context_id.read()
                              << " emit_count=" << wei_emit_count
                              << " pop_count=" << wei_pop_count
                              << " out_count=" << wei_out_count
                              << " k_len=" << wei_k_len
                              << " tail_limit=" << wei_tail_limit
                              << " done=" << wei_pop_done
                              << std::endl;
                }

                dbg_wei_done_count++;
            }

            // 3. Update status flags
            bool empty = col_fifos[0].empty();
            bool full = col_fifos[0].size() >= FIFO_DEPTH;
#ifdef FX1_A3_WEI_FEED_LANE_SHADOW
            {
                if (!sh_init)
                {
                    for (int x = 0; x < X_DIM; x++)
                        sh_lane[x].reset();
                    sh_idx.reset();
                    sh_init = true;
                }

                // --- tick the REAL wei_idxcnt, one step per emitted k ---
                if (sh_push_pending || i_cnt_clear.read())
                {
                    typename sauria_rtl::WeiIdxCnt<SAURIA_WEI_IDX_W, SH_ADRB_W,
                                               SH_WOFS_W>::Inputs ii;
                    ii.cnt_en = sh_push_pending;
                    ii.cnt_clear = i_cnt_clear.read();
                    ii.cswitch = i_cswitch.read();
                    ii.waligned = (i_wei_waligned.read() != 0);
                    ii.auxlim = i_wei_klim.read();
                    ii.auxstep = i_wei_kstep.read();
                    ii.wlim = i_wei_wlim.read();
                    ii.wstep = i_wei_wstep.read();
                    ii.til_klim = i_wei_til_klim.read();
                    ii.til_kstep = i_wei_til_kstep.read();
                    auto io_ = sh_idx.tick(ii);
                    // RTL wei_feeder.sv:288-289 output muxes
                    sh_glob_woffs = i_finalpush.read() ? 0u : io_.woffs;
                    sh_transn = i_finalpush.read() ? false : io_.transn;
                }
                const bool sh_pop = i_pop_en.read();
                typename ShLane::Outputs sh_o[X_DIM];
                for (int x = 0; x < X_DIM; x++)
                {
                    typename ShLane::Inputs li;
                    for (int e = 0; e < ShLane::SRAM_N; e++)
                        li.sram_data[e] = (e < X_DIM)
                            ? static_cast<int32_t>(sh_push_vec[e]) : 0;
                    li.feeder_en = i_feeder_en.read();
                    li.update = sh_push_pending;
                    li.clearbuff = i_feeder_clear.read();
                    li.valid_data = sh_push_pending;
                    li.x_ov_flag = sh_transn;      // RTL transn_mux
                    li.glob_woffs = sh_glob_woffs; // RTL glob_woffs_mux
                    li.loc_woffs = (uint32_t)x;
                    li.dil_pat = (uint64_t)1 << (SH_DILP_W - 1); // RTL i_Dil_pat[0]
                    li.finalpush = i_finalpush.read();
                    li.clearfifo = i_clearfifo.read();
                    li.pipeline_en = true;
                    li.pop_en = sh_pop;
                    sh_o[x] = sh_lane[x].tick(li);
                }
                std::string nm = this->name();
                if (nm.find("NpuTop_std") != std::string::npos)
                {
                    static std::ofstream shtr("trace_sysc/wei_lane_shadow.csv");
                    static bool hdr = false;
                    if (!hdr)
                    {
                        shtr << "cyc,context,push,pop,ref_empty,lane_empty0,"
                                "lane_full0,lane_stall0,gwoffs,transn";
                        for (int x = 0; x < 4; x++)
                            shtr << ",ref" << x << ",lane" << x;
                        shtr << "\n";
                        hdr = true;
                    }
                    // Reference stream = what col_fifos would hand out this cycle.
                    shtr << sh_cycle << "," << i_context_id.read() << ","
                         << (int)sh_push_pending << "," << (int)sh_pop << ","
                         << (int)empty << "," << (int)sh_o[0].fifo_empty << ","
                         << (int)sh_o[0].fifo_full << "," << (int)sh_o[0].stall
                         << "," << sh_glob_woffs << "," << (int)sh_transn;
                    for (int x = 0; x < 4; x++)
                    {
                        int refv = col_fifos[x].empty()
                                       ? 0 : (int)col_fifos[x].front();
                        shtr << "," << refv << "," << (int)sh_o[x].data;
                    }
                    shtr << "\n";
                }
                sh_push_pending = false;
                sh_cycle++;
            }
#endif
            o_fifo_empty.write(empty);
            o_fifo_full.write(full);
        }
    };

} // namespace sauria_rtl

#endif // SAURIA_RTL_WEI_FEEDER_H
