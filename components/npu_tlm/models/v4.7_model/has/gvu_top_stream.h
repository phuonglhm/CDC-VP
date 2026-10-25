// gvu_top_stream.h -- HAS_GVU_TOP=3: GvuTop (has/rtl_ref_gvu_top.h) connected to the core with CLOCKED signals, along the
// PSM -> GVU stream of the latest hardware RTL drop (neo_core/psm/psm_top.sv:290-360, neo_core/gvu/gvu_pipe_controller.sv). Two modules on
// the clock of the core:
//   PsmStreamTap   core side. Takes the packets of the PSM write hook, keeps the 4-packet FIFO of psm_top.sv and drives
//                  o_psum_valid, o_psum_data, o_oc_last, o_oc_tile_last, o_psum_empty and o_stream_stall; takes i_psum_ready.
//   GvuTopStream   vector unit side. Holds one rtl_ref_gvu::GvuTop. Drives i_psum_ready (= the controller is in the state that
//                  takes beats), latches a packet at the clock edge when valid && ready, runs the kDepth-stage stream
//                  pipeline of GEMM_FUSED, and writes every int8 result back into the NPU half of SRAM-C (the results of the
//                  RTL go to vbank2, which does not work in the latest hardware RTL drop -- B1 -- and has 256 rows).
// Timing as in the RTL and independent of the order of processes: a packet written by the hook at edge E is in the FIFO (valid
// = 1) in the cycle after E and latched by GvuTop at E+1; its result comes out at E+1+kDepth (E+10 with the default depth 9,
// the same pipeline as the call-style connection HAS_GVU_TOP=1).
// Signals of the RTL boundary: o_psum_valid / i_psum_ready / o_psum_data / o_oc_last / o_oc_tile_last / o_psum_empty /
// o_stream_stall. Side channel of the MODEL ONLY (not in the RTL, named o_side_* / i_side_*): the word address and lane mask of
// the write, the output channel, the Out_Scale / Out_Shift of that channel (the RTL reads them from the sbank, finding F2; the sbank layout is an open question), the
// model result of the beat (to compare) and a flag for beats that GvuTop cannot express.
// Back-pressure: by default the core is not stopped by o_stream_stall (the packet is accepted anyway and the stall is
// counted: stall_needed); with HAS_GVU_STALL=1 the core stops as psm_top.sv does (HasNpuTop::stream_room, rtl_ref_npu_top.h).
// Not done (counted and printed): the exit of GEMM_FUSED_OBP by oc_last & oc_tile_last (F1), the scale index of the AGU (F2)
// and the write of vbank2 (B1) are only observed and counted, they do not drive values.
// Residual add and max pool run on the same GvuTop. By default as in has/gvu_top_bridge.h, without clock process (add(),
// maxpool()): the schedule of has::HasElemwise is an analytic overlap of DMA and compute, so the cycles of the unit are an input
// to it. With HAS_GVU_ELEM_CLK=1 (HasNpuTop) they run as jobs on the clock of the core (start_add(), start_maxpool()): one tick
// per edge, the same runs and cycles, and HasElemwise runs its overlap of DMA and compute along the clock.
#ifndef HAS_GVU_TOP_STREAM_H
#define HAS_GVU_TOP_STREAM_H

#include <systemc.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <vector>
#include "sauria_types.h"
#include "has/gvu_top_bridge.h"
#include "has/gvu_reg_master.h"
#include "has/gvu_regs_dfc.h"
#include "has/rtl_ref_gvu_config_regs.h"
#include "has/rtl_ref_gvu_top.h"

namespace has
{
    using sauria::psum_vector_t;

    // One write of the PSM to SRAM-C as it travels through the stream.
    struct StreamPacket
    {
        psum_vector_t<32, int32_t> data;    // 32 x int32 psum (RTL: o_psum_data)
        uint32_t mask{0};                   // lane mask of the write (RTL: wmask; the lanes without a mask are 0 in the packet)
        bool oc_last{false}, oc_tile_last{false};
        bool call_last{false};              // the packet holds the last element of the tile (last channel, last position): the last write of the call
        // ---- side channel of the model
        uint32_t addr{0};                   // word address in SRAM-C
        uint32_t ch{0};                     // output channel of the packet (one per packet)
        int64_t S{1};                       // Out_Scale of the channel (decoded)
        int32_t s{0};                       // Out_Shift of the channel
        bool bypass{false};                 // parameters do not fit GvuTop: the model result goes back instead
        psum_vector_t<32, int32_t> model;   // model result (int8 values in int32) to compare with and to use for a bypassed beat
    };

    // Counters of the whole run (printed at exit).
    struct StreamStats
    {
        uint64_t pushed{0}, bypassed{0}, accepted{0}, outputs{0}, lanes{0}, differ{0}, meta_bad{0};
        uint64_t fifo_max{0}, stall_needed{0}, multi_push{0}, first_stall_edge{0};
        uint64_t lat_push_accept_min{~0ull}, lat_push_accept_max{0}, lat_accept_out_min{~0ull}, lat_accept_out_max{0};
        uint64_t drain_extra{0}, drain_waits{0}, layers{0}, tables{0}, resets{0};
        uint64_t rtl_exit_events{0}, rtl_at_risk{0}, rtl_agu_mismatch{0};
        uint64_t add_calls{0}, add_not_expr{0}, add_lanes{0}, add_differ{0}, add_cycles{0}, add_model_cycles{0};
        uint64_t mp_calls{0}, mp_not_expr{0}, mp_lanes{0}, mp_differ{0}, mp_cycles{0}, mp_model_cycles{0}, mp_runs{0};
        // HAS_GVU_ELEM_CLK=1: residual add and max pool as jobs on the clock
        uint64_t elem_jobs{0}, elem_jobs_failed{0}, elem_ticks{0}, elem_job_cycles{0}, elem_idle_ticks{0}, elem_start_late{0};
        // HAS_GVU_REGS=1: the DFC drives the unit through the register file (has/gvu_regs_dfc.h)
        uint64_t regs_layers{0}, regs_exit_flags{0}, regs_exit_fallback{0}, regs_exit_timeout{0}, regs_writes{0}, regs_seqs{0}, regs_busy_edges{0};
        uint64_t regs_pool_runs{0}, regs_pool_call_style{0}, regs_flag_bypass{0}, regs_lut_toggles{0};
        uint64_t regs_add_runs{0}, regs_add_cfg_mismatch{0}, regs_add_b_buf1{0};
        // F1' in the tap (psm_top.sv stream_ctx_q): calls that pushed packets, calls whose oc_last packets differ from i_ncontexts, packets with oc_tile_last
        uint64_t f1_calls{0}, f1_ctx_mismatch{0}, f1_tile_last_packets{0}, f1_flag_bad{0};
        // HAS_GVU_STALL (see HasNpuTop): 1 = the core stops on o_stream_stall; 2 = test only, the stall is computed but not applied
        int stall_mode{0};
        uint64_t core_stall_cycles{0}, stall_multi{0}, peek_checked{0}, peek_mismatch{0};
        uint64_t overflow_edges{0}, overflow_hash{0}, predicted_edges{0}, predicted_hash{0};
        static uint64_t hash_step(uint64_t h, uint64_t t) { return (h ^ t) * 1099511628211ull + 1469598103934665603ull; }
        char first[160]{0};
        template <typename... A> void miss(const char *fmt, A... a) { if (!differ) std::snprintf(first, sizeof first, fmt, a...); differ++; }
    };
    inline StreamStats &stream_stats() { static StreamStats s; return s; }

    // ---------------------------------------------------------------------------------------------------- PsmStreamTap
    class PsmStreamTap : public sc_core::sc_module
    {
    public:
        static constexpr int kFifoDepth = 4;     // STREAM_FIFO_DEPTH of psm_top.sv

        sc_core::sc_in<bool> i_clk{"i_clk"}, i_rstn{"i_rstn"};
        sc_core::sc_in<bool> i_psum_ready{"i_psum_ready"};
        sc_core::sc_out<bool> o_psum_valid{"o_psum_valid"}, o_oc_last{"o_oc_last"}, o_oc_tile_last{"o_oc_tile_last"};
        sc_core::sc_out<bool> o_psum_empty{"o_psum_empty"}, o_stream_stall{"o_stream_stall"};
        sc_core::sc_out<psum_vector_t<32, int32_t>> o_psum_data{"o_psum_data"};
        // side channel of the model (head packet)
        sc_core::sc_out<uint32_t> o_side_addr{"o_side_addr"}, o_side_mask{"o_side_mask"}, o_side_ch{"o_side_ch"};
        sc_core::sc_out<int64_t> o_side_scale{"o_side_scale"};
        sc_core::sc_out<int32_t> o_side_shift{"o_side_shift"};
        sc_core::sc_out<bool> o_side_bypass{"o_side_bypass"};
        sc_core::sc_out<psum_vector_t<32, int32_t>> o_side_model{"o_side_model"};
        sc_core::sc_out<uint64_t> o_side_push{"o_side_push"};     // edge at which the head packet entered the FIFO

        SC_HAS_PROCESS(PsmStreamTap);
        explicit PsmStreamTap(sc_core::sc_module_name n) : sc_core::sc_module(n)
        {
            SC_METHOD(edge_proc);
            sensitive << i_clk.pos();
            dont_initialize();
            SC_METHOD(commit_proc);
            sensitive << commit_;
            dont_initialize();
        }

        // Called by the PSM write hook in the clock edge E (any order against the other processes of that edge).
        void push(const StreamPacket &q)
        {
            StreamPacket p = q;
            if (f1_on_)
            {
                // F1' (F1p.diff, psm_top.sv stream_ctx_q / stream_ctx_last): oc_tile_last of a packet = the configured final-OC-tile bit of
                // the call AND the packet belongs to the last context; the counter of contexts counts the packets that end a context (oc_last)
                // F1' (a) of the RTL counts the packets that carry oc_last against i_ncontexts; with a row of the last channel that straddles two
                // SRAM-C words of one context (positions per context not a multiple of the word) the last channel has more than one oc_last packet
                // in a context and the count reaches ncontexts too early. The model sets the flag on the packet that holds the last element of
                // the tile (the last write of the call), which is where the unit must leave (call_last, from the geometry of the tile).
                p.oc_tile_last = call_tile_last_ && p.call_last;
                if (p.oc_tile_last) call_flags_++;
                call_last_flag_ = p.oc_tile_last;
                if (p.oc_last) call_ctx_++;
                call_packets_++;
                if (p.oc_tile_last) stream_stats().f1_tile_last_packets++;
            }
            pending_.push_back(p);
            stream_stats().pushed++;
            if (p.bypass) stream_stats().bypassed++;
        }
        // HAS_GVU_REGS: F1' on. begin_call is the reset between two invocations of the core (i_fsm_reset) with the contexts of the call
        // and the configured oc_tile_last bit; end_call checks that the packets that ended a context are as many as the contexts.
        void enable_f1() { f1_on_ = true; }
        bool f1_on() const { return f1_on_; }
        void begin_call(uint32_t ncontexts, bool tile_last) { end_call(); call_nctx_ = ncontexts; call_tile_last_ = tile_last; call_ctx_ = 0; call_packets_ = 0; call_flags_ = 0; call_last_flag_ = false; }
        void end_call()
        {
            if (!f1_on_ || call_packets_ == 0) { call_packets_ = 0; return; }
            stream_stats().f1_calls++;
            if (call_ctx_ != call_nctx_) stream_stats().f1_ctx_mismatch++;
            // the flag must sit on exactly one packet, the last of the call, in the last call of a layer, and nowhere else
            if (call_tile_last_ ? !(call_flags_ == 1 && call_last_flag_) : call_flags_ != 0) stream_stats().f1_flag_bad++;
            call_packets_ = 0; call_ctx_ = 0; call_flags_ = 0; call_last_flag_ = false;
        }
        bool empty() const { return fifo_.empty() && pending_.empty(); }
        // Room in the FIFO for a write of the core in this cycle that pushes n packets (psm_top.sv:309: stream_fifo_in_ready =
        // count < 4 || pop, pop = o_psum_valid && i_psum_ready). Read in delta 0 of the edge, before the write hook of the edge:
        // the FIFO and i_psum_ready hold the values of the cycle before the edge, whatever the order of the processes. The RTL
        // pushes one packet per write; the model pushes one per output channel of the write, so n > 1 only for a write whose
        // lanes span two channels (not seen on YOLOv8m): it needs room for all n packets and is counted (stall_multi).
        bool in_ready_now(size_t n) const
        {
            const size_t pop = (!fifo_.empty() && i_psum_ready.read()) ? 1u : 0u;
            return fifo_.size() - pop + n <= size_t(kFifoDepth);
        }
        size_t depth() const { return fifo_.size(); }
        uint64_t edge() const { return edge_; }

    private:
        std::deque<StreamPacket> pending_, fifo_;
        std::deque<uint64_t> fifo_edge_;
        sc_core::sc_event commit_;
        bool ready_s_{false}, rstn_s_{false};
        uint64_t edge_{0};
        bool f1_on_{false}, call_tile_last_{false};
        uint32_t call_nctx_{1}, call_ctx_{0}, call_packets_{0}, call_flags_{0};
        bool call_last_flag_{false};

        // delta 0 of the edge: sample what the edge sees (the signals have the values of the cycle before the edge)
        void edge_proc()
        {
            edge_++;
            ready_s_ = i_psum_ready.read();
            rstn_s_ = i_rstn.read();
            commit_.notify(sc_core::SC_ZERO_TIME);
        }
        // delta 1 of the edge: the hooks of this edge have run. Pop (valid && ready of the previous cycle), push, new outputs.
        void commit_proc()
        {
            StreamStats &st = stream_stats();
            if (!rstn_s_)
            {
                fifo_.clear(); pending_.clear(); fifo_edge_.clear();
            }
            bool stall = false;
            if (!fifo_.empty() && ready_s_)
            {
                fifo_.pop_front();
                fifo_edge_.pop_front();
            }
            if (pending_.size() > 1) st.multi_push++;
            for (size_t k = 0; k < pending_.size(); k++)
            {
                if (fifo_.size() >= size_t(kFifoDepth))
                {
                    // psm_top.sv:309-310: no room -> write_pipeline_stall holds the PSM. The core is NOT stopped here (S2): the packet
                    // is taken and the stall counted.
                    stall = true;
                    st.stall_needed++;
                    if (!st.first_stall_edge) st.first_stall_edge = edge_;
                }
                fifo_.push_back(pending_[k]);
                fifo_edge_.push_back(edge_);       // the edge of the commit, the same whatever the order of the processes
            }
            pending_.clear();
            if (stall)
            {
                st.overflow_edges++;
                st.overflow_hash = StreamStats::hash_step(st.overflow_hash, sc_core::sc_time_stamp().value());
            }
            st.fifo_max = std::max<uint64_t>(st.fifo_max, fifo_.size());
            const bool v = !fifo_.empty();
            static const StreamPacket none;
            const StreamPacket &h = v ? fifo_.front() : none;
            o_psum_valid.write(v);
            o_psum_data.write(v ? h.data : psum_vector_t<32, int32_t>());
            o_oc_last.write(v && h.oc_last);
            o_oc_tile_last.write(v && h.oc_tile_last);
            o_psum_empty.write(!v);
            o_stream_stall.write(stall);
            o_side_addr.write(h.addr); o_side_mask.write(h.mask); o_side_ch.write(h.ch);
            o_side_scale.write(h.S); o_side_shift.write(h.s); o_side_bypass.write(v && h.bypass);
            o_side_model.write(h.model);
            o_side_push.write(v ? fifo_edge_.front() : 0);
        }
    };

    // ---------------------------------------------------------------------------------------------------- GvuTopStream
    class GvuTopStream : public sc_core::sc_module
    {
    public:
        // stages behind the latch of a beat (first result at tick kDepth + 1): the shared rtl_ref_gvu::GEMM_STREAM_DEPTH
        static constexpr int kDepth = rtl_ref_gvu::GEMM_STREAM_DEPTH;
        // the RTL controller leaves GEMM_FUSED_OBP this many beats after oc_tile_last & oc_last (a fixed delay of its last-read
        // flag in gvu_pipe_controller.sv), whatever the depth of the model's pipeline; used by the observer only
        static constexpr int kRtlExitDelay = 9;

        sc_core::sc_in<bool> i_clk{"i_clk"}, i_rstn{"i_rstn"};
        sc_core::sc_in<bool> i_psum_valid{"i_psum_valid"}, i_oc_last{"i_oc_last"}, i_oc_tile_last{"i_oc_tile_last"};
        sc_core::sc_in<bool> i_psum_empty{"i_psum_empty"}, i_stream_stall{"i_stream_stall"};
        sc_core::sc_in<psum_vector_t<32, int32_t>> i_psum_data{"i_psum_data"};
        sc_core::sc_out<bool> o_psum_ready{"o_psum_ready"};
        // side channel of the model
        sc_core::sc_in<uint32_t> i_side_addr{"i_side_addr"}, i_side_mask{"i_side_mask"}, i_side_ch{"i_side_ch"};
        sc_core::sc_in<int64_t> i_side_scale{"i_side_scale"};
        sc_core::sc_in<int32_t> i_side_shift{"i_side_shift"};
        sc_core::sc_in<bool> i_side_bypass{"i_side_bypass"};
        sc_core::sc_in<psum_vector_t<32, int32_t>> i_side_model{"i_side_model"};
        sc_core::sc_in<uint64_t> i_side_push{"i_side_push"};

        // Result of a beat to the core: word address, lane mask, 32 x int8 (as int32). Set by HasNpuTop (SRAM-C of the NPU half).
        std::function<void(uint32_t, uint32_t, const psum_vector_t<32, int32_t> &)> result_sink;
        // Optional: one line per beat to this file, in the order of the beats: "B ch mask 32 hex bytes" (HAS_GVU_TOP_DUMP).
        std::FILE *dump{nullptr};
        // Test only: hold o_psum_ready low for the edges [hold_from, hold_until) (edge numbers of edge()).
        uint64_t hold_from{0}, hold_until{0};
        uint64_t edge() const { return edge_; }
        // Trace of the edges of every packet (for the test bench).
        struct Trace { uint64_t push, accept, out; uint32_t addr; };
        bool tracing{false};
        std::vector<Trace> trace;

        SC_HAS_PROCESS(GvuTopStream);
        explicit GvuTopStream(sc_core::sc_module_name n) : sc_core::sc_module(n)
        {
            SC_METHOD(clock_proc);
            sensitive << i_clk.pos();
            dont_initialize();
            reset_unit();
            // Test only: HAS_GVU_STREAM_HOLD=<from>,<until> holds o_psum_ready low for the edges [from, until), to fill the FIFO
            // of the tap on a real program. The unit exists only with HAS_GVU_TOP=3.
            if (const char *h = std::getenv("HAS_GVU_STREAM_HOLD"))
            {
                unsigned long long a = 0, b = 0;
                if (std::sscanf(h, "%llu,%llu", &a, &b) == 2 && b > a) { hold_from = a; hold_until = b; }
            }
        }

        // ---- layer control by the DFC (a register write in the RTL): not during beats of the previous layer
        // Starts a GEMM_FUSED stream run. lut: the 256 bytes of the activation table (index = int8 value + 128) or nullptr
        // (no activation); zp: zero point, its low byte sign-extended as in the RTL (GvuTopBridge::zp_reg; the caller sends the
        // beats of a layer whose zero point is outside int8 as bypassed); round_mode: has::RoundMode; act: activation on; req_narrow: has::ReqNarrow,
        // the narrowing to int16 after the round-shift (SAT16 or WRAP16; a narrowing GvuTop does not have keeps saturation,
        // the beats of such a layer come flagged as not expressible).
        void begin_layer(const int8_t *lut, int zp, int round_mode, bool act, int req_narrow = SAT16)
        {
            namespace R = rtl_ref_gvu;
            StreamStats &st = stream_stats();
            st.layers++;
            if (top_.state() != R::IDLE) reset_unit();
            gin_ = R::GvuTop::In();
            gin_.i_gemm_round = map_round(round_mode); gin_.i_gemm_zp16 = GvuTopBridge::zp_reg(zp); gin_.i_gemm_skip_act = !act;
            R::NarrowMode nm = R::NARROW_SATURATE;
            if (GvuTopBridge::map_narrow(req_narrow, &nm)) gin_.i_gemm_narrow = nm;
            if (lut || !table_loaded_)
            {
                for (unsigned w = 0; w < 2; w++)
                {
                    R::SpBytes row(128, 0);
                    if (lut) for (unsigned k = 0; k < 128; k++) row[k] = static_cast<uint8_t>(lut[128 * w + k]);
                    top_.scratchpad().sram(1, 1).poke(w, row);
                }
                gin_.i_vbank1_buf_sel = true;
                gin_.i_gemm_lut_reload = table_loaded_;   // the first load needs no reload (the controller has not loaded a table yet)
                table_loaded_ = true;
                st.tables++;
            }
            gin_.i_gvu_op = R::GVU_OP_GEMM_FUSED; gin_.i_gemm_stream = true; gin_.i_gemm_stream_stop = false;
            gin_.i_pipeline_en = true;
            running_ = true; stopping_ = false;
            rtl_agu_ = 0; rtl_window_ = 0;
        }
        // ---- HAS_GVU_REGS=1: the DFC drives the unit through the register file (gvu_config_regs, has/rtl_ref_gvu_config_regs.h) with the
        // writes of has/gvu_regs_dfc.h, issued by has::GvuRegMaster (AXI-Lite master, lat edges per write). The model-only inputs
        // (rounding, narrowing, stream flag, scale / shift on the side channel) stay in gin_. enable_regs is called once, at construction.
        void enable_regs(unsigned lat, bool add_by_regs = true)
        {
            regs_on_ = true;
            add_regs_ = add_by_regs;
            master_.set_lat(lat);
            rtl_ref_gvu::GvuConfigParams cp; cp.b13_fixed = true; cp.word_addressed = true;   // patches B13 / B17 as sent to the hardware team
            cp.num_regs = 32;                                                                  // GVU_REG_AW 5: Reg 9..15 hold the residual add
            regs_ = rtl_ref_gvu::GvuConfigRegs(cp);
        }
        bool regs_on() const { return regs_on_; }
        const rtl_ref_gvu::GvuConfigRegs &regs() const { return regs_; }
        const GvuRegMaster &reg_master() const { return master_; }
        // The thread of the DFC waits until the sequence in flight is over.
        void reg_wait() { while (!reg_idle_) sc_core::wait(reg_ev_); }
        // Layer start by registers: the table of the layer into the vbank1 buffer the register selects (the unit toggles the bit when
        // the table is in), the model-only inputs, then the sequence of gvu_regs_dfc::layer_begin; the caller calls reg_wait().
        void begin_layer_regs(const int8_t *lut, int zp, int round_mode, bool act, int req_narrow = SAT16)
        {
            namespace R = rtl_ref_gvu;
            StreamStats &st = stream_stats();
            st.layers++; st.regs_layers++;
            if (top_.state() != R::IDLE) reset_unit();
            gin_ = R::GvuTop::In();
            gin_.i_gemm_round = map_round(round_mode); gin_.i_gemm_stream = true; gin_.i_gemm_stream_stop = false;
            gin_.i_gemm_exit_flags = true;
            R::NarrowMode nm = R::NARROW_SATURATE;
            if (GvuTopBridge::map_narrow(req_narrow, &nm)) gin_.i_gemm_narrow = nm;
            bool table_reset = false;
            if (lut || !table_loaded_)
            {
                const bool buf = regs_.out().vbank1_buf_sel;
                for (unsigned w = 0; w < 2; w++)
                {
                    R::SpBytes row(128, 0);
                    if (lut) for (unsigned k = 0; k < 128; k++) row[k] = static_cast<uint8_t>(lut[128 * w + k]);
                    top_.scratchpad().sram(1, buf ? 1 : 0).poke(w, row);
                }
                table_reset = table_loaded_;
                table_loaded_ = true;
                st.tables++;
            }
            running_ = true; stopping_ = false; pipe_done_gemm_ = false;
            rtl_agu_ = 0; rtl_window_ = 0;
            request_regs(gvu_regs_dfc::layer_begin(zp, !act, table_reset));
        }
        // End of the layer by registers: the unit has left GEMM_FUSED by itself (the beat of the last call carried oc_last & oc_tile_last);
        // wait for pipe_done (a wire to the DFC in the RTL), then clear the done flag. Not leaving in time is counted (fallback: the stop of the model).
        void end_layer_regs()
        {
            StreamStats &st = stream_stats();
            if (!running_) return;
            int g = 0;
            while (!pipe_done_gemm_ && g++ < 1000) sc_core::wait(i_clk.posedge_event());
            if (!pipe_done_gemm_)
            {
                st.regs_exit_fallback++;
                gin_.i_gemm_stream_stop = true;
                g = 0;
                while (!pipe_done_gemm_ && g++ < 1000) sc_core::wait(i_clk.posedge_event());
                if (!pipe_done_gemm_) st.regs_exit_timeout++;
            }
            reg_wait();
            request_regs(gvu_regs_dfc::clear_done());
            reg_wait();
            running_ = false;
            gin_.i_gemm_stream_stop = false;
        }
        // The controller takes beats (RTL: o_psum_ready = state == GEMM_FUSED_OBP).
        bool ready_for_beats() const { return running_ && !stopping_ && top_.state() == rtl_ref_gvu::GEMM_FUSED_ST1; }
        // End of the layer: stop once the pipeline is empty, back to IDLE.
        void end_layer() { if (running_) stopping_ = true; }
        bool layer_closed() const { return !running_; }
        // No beat anywhere between the PSM and SRAM-C: FIFO empty, nothing latched, pipeline empty, no bypassed result waiting.
        bool drained(const PsmStreamTap &tap) const { return tap.empty() && in_flight_ == 0 && bypass_q_.empty(); }

        // ---- residual add and max pool on the same unit (see the header comment)
        // An operation is a sequence of runs of the unit: residual add in runs of at most 128 rows (the int8 rows of vbank1), max
        // pool in runs of at most cmax channels, each from a reset of the unit (B12, QD-09). add() / maxpool() run every run to
        // completion inside the call (no simulated time); start_add() / start_maxpool() (HAS_GVU_ELEM_CLK=1) give the same runs to
        // clock_proc, one tick of the unit per clock edge. Both count the cycles of a run from its first tick to the tick that gives
        // done (the unit is IDLE in that tick), so both give the same cycles for the same operands.
        static bool add_fits(int zpA, int zpB, int zpO, int64_t SA, int64_t SB, int64_t SO, int sA, int sB, int sO, int round_mode,
                             bool sub_before, bool sat16)
        {
            return sub_before && sat16 && fit(SA, 32) && fit(SB, 32) && fit(SO, 32) && fit(zpA, 8) && fit(zpB, 8) && fit(zpO, 8) &&
                   sA >= 0 && sA <= 63 && sB >= 0 && sB <= 63 && sO >= 0 && sO <= 63 && round_mode >= 0 && round_mode <= 6;
        }
        static bool maxpool_fits(int g, int H, int W, int kk, int s, int p) { return pool_geom(g, H, W, kk, s, p).ok; }
        // With the register file the tile moves have 10 bits each (POOLING_1), the other fields fit by construction of pool_geom.
        bool maxpool_fits_now(int g, int H, int W, int kk, int s, int p) const
        {
            const PoolGeom q = pool_geom(g, H, W, kk, s, p);
            return q.ok && (!regs_on_ || (q.Wt * q.Hp <= 1023 && q.Wt * q.s <= 1023));
        }

        bool add(const int8_t *A, const int8_t *B, int8_t *out, size_t n, int zpA, int zpB, int zpO, int64_t SA, int64_t SB, int64_t SO,
                 int sA, int sB, int sO, int round_mode, bool sub_before, bool sat16, uint64_t model_cycles, uint64_t *cycles)
        {
            namespace R = rtl_ref_gvu;
            StreamStats &st = stream_stats();
            st.add_calls++;
            if (!add_fits(zpA, zpB, zpO, SA, SB, SO, sA, sB, sO, round_mode, sub_before, sat16)) { st.add_not_expr++; return false; }
            leave_for_job();
            const AddCfg a{zpA, zpB, zpO, SA, SB, SO, sA, sB, sO, round_mode};
            const size_t rows_total = (n + 31) / 32;
            uint64_t cyc = 0;
            for (size_t r0 = 0; r0 < rows_total; r0 += kAddRows)
            {
                const size_t rows = std::min(kAddRows, rows_total - r0);
                R::GvuTop::In i = add_load(A, B, n, r0, rows, rows_total, a);
                bool done = false;
                uint64_t c = 0;
                for (; c < 4 * rows + 400 && !done; c++) done = top_.tick(i, true).o_residual_pipe_done;
                i.i_pipeline_en = false;
                for (int k = 0; k < 4 && top_.state() != R::IDLE; k++) top_.tick(i, true);
                if (!done || top_.state() != R::IDLE) { st.add_not_expr++; reset_unit(); return false; }
                cyc += c;
                add_read(out, n, r0, rows);
            }
            st.add_cycles += cyc; st.add_model_cycles += model_cycles;
            *cycles = cyc;
            return true;
        }
        bool maxpool(const int8_t *in, int8_t *out, int g, int H, int W, int kk, int s, int p, uint64_t model_cycles, uint64_t *cycles)
        {
            namespace R = rtl_ref_gvu;
            StreamStats &st = stream_stats();
            st.mp_calls++;
            const PoolGeom q = pool_geom(g, H, W, kk, s, p);
            if (!q.ok) { st.mp_not_expr++; return false; }
            leave_for_job();
            std::vector<int8_t> res(static_cast<size_t>(g) * q.Ho * q.Wo);
            uint64_t cyc = 0;
            for (int c0 = 0; c0 < g; c0 += q.cmax)
            {
                const int n = std::min(q.cmax, g - c0);
                reset_unit();                                // B12: a run starts from a reset of the pooling feeder
                R::GvuTop::In i = pool_load(in, q, c0, n);
                bool done = false;
                uint64_t c = 0;
                const uint64_t guard = pool_guard(q, n);
                for (; c < guard && !done; c++) done = top_.tick(i, true).o_maxpool_pipe_done;
                i.i_pipeline_en = false;
                for (int k = 0; k < 4 && top_.state() != R::IDLE; k++) top_.tick(i, true);
                if (!done || top_.state() != R::IDLE) { st.mp_not_expr++; reset_unit(); return false; }
                cyc += c;
                st.mp_runs++;
                pool_read(res, q, c0, n);
            }
            reset_unit();
            pool_finish(res, out);
            st.mp_cycles += cyc; st.mp_model_cycles += model_cycles;
            *cycles = cyc;
            return true;
        }

        // ---- jobs on the clock (HAS_GVU_ELEM_CLK=1; has::HasElemwise through HasNpuTop): the runs of one residual add chunk or of one
        // max pool group on the clock of the core, the parameters checked by the caller (add_fits / maxpool_fits). The operands are
        // copied at the request. The job starts at the first edge after the edge of the request, whatever the order of the processes;
        // then the unit takes exactly one tick per edge and a run follows the previous one at the next edge. The reset before a max pool
        // run and the load of the banks take no time, as in the call style (QD-09: the fix of B12 clears the counter at
        // o_pooling_last_wr; the load is the DMA of the schedule). At the done tick of the last run the results are compared with the
        // model and written into out, job_done() turns true and job_event() is notified (delta); job_cycles() = the sum of the runs.
        void start_add(const int8_t *A, const int8_t *B, int8_t *out, size_t n, int zpA, int zpB, int zpO, int64_t SA, int64_t SB, int64_t SO,
                       int sA, int sB, int sO, int round_mode, uint64_t model_cycles)
        {
            stream_stats().add_calls++;
            job_ = Job();
            job_.kind = Job::ADD;
            job_.a.assign(A, A + n); job_.b.assign(B, B + n);
            job_.out = out; job_.n = n; job_.model_cycles = model_cycles;
            job_.cfg = AddCfg{zpA, zpB, zpO, SA, SB, SO, sA, sB, sO, round_mode};
            job_.rows_total = (n + 31) / 32;
            request_job();
        }
        void start_maxpool(const int8_t *in, int8_t *out, int g, int H, int W, int kk, int s, int p, uint64_t model_cycles)
        {
            stream_stats().mp_calls++;
            job_ = Job();
            job_.kind = Job::MAXPOOL;
            job_.q = pool_geom(g, H, W, kk, s, p);
            job_.a.assign(in, in + static_cast<size_t>(g) * H * W);
            job_.out = out; job_.model_cycles = model_cycles;
            job_.res.assign(static_cast<size_t>(g) * job_.q.Ho * job_.q.Wo, 0);
            request_job();
        }
        bool job_done() const { return job_.state == Job::IDLE || job_.state == Job::DONE; }
        const sc_core::sc_event &job_event() const { return job_ev_; }
        uint64_t job_cycles() const { return job_.cyc; }

        const rtl_ref_gvu::GvuTop &unit() const { return top_; }
        static void report()
        {
            const StreamStats &s = stream_stats();
            std::printf("[gvu-stream] HAS_GVU_TOP=3: GvuTop on the clock of the core behind a PSM stream tap (FIFO %d), results back into SRAM-C of the NPU half\n",
                        PsmStreamTap::kFifoDepth);
            std::printf("[gvu-stream] packets pushed %llu (not expressible %llu, model result back), latched %llu, results %llu, lanes %llu, differ from the model %llu%s%s\n",
                        (unsigned long long)s.pushed, (unsigned long long)s.bypassed, (unsigned long long)s.accepted, (unsigned long long)s.outputs,
                        (unsigned long long)s.lanes, (unsigned long long)s.differ, s.differ ? ", first: " : "", s.differ ? s.first : "");
            std::printf("[gvu-stream] latency push->latch %llu..%llu edges, latch->result %llu..%llu edges, address / mask not carried %llu\n",
                        (unsigned long long)(s.lat_push_accept_min == ~0ull ? 0 : s.lat_push_accept_min), (unsigned long long)s.lat_push_accept_max,
                        (unsigned long long)(s.lat_accept_out_min == ~0ull ? 0 : s.lat_accept_out_min), (unsigned long long)s.lat_accept_out_max,
                        (unsigned long long)s.meta_bad);
            if (s.stall_mode == 1)
                std::printf("[gvu-stream] FIFO max depth %llu, STALL NEEDED %llu (packets taken with the FIFO full; the core stops on o_stream_stall)%s, writes with more than one segment %llu\n",
                            (unsigned long long)s.fifo_max, (unsigned long long)s.stall_needed, s.stall_needed ? " <== FAIL" : "", (unsigned long long)s.multi_push);
            else
                std::printf("[gvu-stream] FIFO max depth %llu, STALL NEEDED %llu (the core is not stopped by o_stream_stall: gap S1)%s, writes with more than one segment %llu\n",
                            (unsigned long long)s.fifo_max, (unsigned long long)s.stall_needed, s.stall_needed ? " <== FAIL" : "", (unsigned long long)s.multi_push);
            if (s.stall_mode == 1)
                std::printf("[gvu-stream] core stall cycles %llu (o_stream_stall held the core), held writes with more than one packet %llu, "
                            "write read before the step checked %llu, mismatched %llu%s\n",
                            (unsigned long long)s.core_stall_cycles, (unsigned long long)s.stall_multi, (unsigned long long)s.peek_checked,
                            (unsigned long long)s.peek_mismatch, s.peek_mismatch ? " <== FAIL" : "");
            if (s.stall_mode == 2)
                std::printf("[gvu-stream] stall observed, not applied (test only): edges with a stall computed %llu (hash %016llx), edges with a packet taken "
                            "with the FIFO full %llu (hash %016llx) -> %s; write read before the step checked %llu, mismatched %llu\n",
                            (unsigned long long)s.predicted_edges, (unsigned long long)s.predicted_hash, (unsigned long long)s.overflow_edges,
                            (unsigned long long)s.overflow_hash, (s.predicted_edges == s.overflow_edges && s.predicted_hash == s.overflow_hash) ? "same edges" : "DIFFERENT",
                            (unsigned long long)s.peek_checked, (unsigned long long)s.peek_mismatch);
            std::printf("[gvu-stream] layers %llu, activation tables loaded %llu, drain waits behind the cores %llu cycles in %llu passes, unit resets %llu\n",
                        (unsigned long long)s.layers, (unsigned long long)s.tables, (unsigned long long)s.drain_extra, (unsigned long long)s.drain_waits, (unsigned long long)s.resets);
            std::printf("[gvu-stream] RTL stream observed, values NOT driven by it: exits of GEMM_FUSED_OBP (oc_last & oc_tile_last, F1) %llu, packets within %d beats of an exit %llu, "
                        "scale index of the AGU != channel (F2) %llu\n", (unsigned long long)s.rtl_exit_events, kRtlExitDelay, (unsigned long long)s.rtl_at_risk, (unsigned long long)s.rtl_agu_mismatch);
            if (s.add_calls)
                std::printf("[gvu-stream] residual add | calls %llu | not expressible %llu | lanes %llu | differ %llu | GvuTop cycles %llu | model cycles %llu\n",
                            (unsigned long long)s.add_calls, (unsigned long long)s.add_not_expr, (unsigned long long)s.add_lanes, (unsigned long long)s.add_differ,
                            (unsigned long long)s.add_cycles, (unsigned long long)s.add_model_cycles);
            if (s.mp_calls)
                std::printf("[gvu-stream] max pool     | calls %llu | not expressible %llu | lanes %llu | differ %llu | runs %llu | GvuTop cycles %llu | model cycles %llu\n",
                            (unsigned long long)s.mp_calls, (unsigned long long)s.mp_not_expr, (unsigned long long)s.mp_lanes, (unsigned long long)s.mp_differ,
                            (unsigned long long)s.mp_runs, (unsigned long long)s.mp_cycles, (unsigned long long)s.mp_model_cycles);
            if (s.regs_seqs)
                std::printf("[gvu-stream] register file (HAS_GVU_REGS=1): layers %llu, sequences %llu, register writes %llu, edges the DFC waited on the write port %llu, "
                            "layer exits by oc_last & oc_tile_last %llu (model stop as fallback %llu, timeouts %llu), max pool runs %llu, "
                            "table-load toggles of the vbank1 buffer bit %llu, bypassed beats that carried the flags %llu%s\n",
                            (unsigned long long)s.regs_layers, (unsigned long long)s.regs_seqs, (unsigned long long)s.regs_writes, (unsigned long long)s.regs_busy_edges,
                            (unsigned long long)s.regs_exit_flags, (unsigned long long)s.regs_exit_fallback, (unsigned long long)s.regs_exit_timeout,
                            (unsigned long long)s.regs_pool_runs, (unsigned long long)s.regs_lut_toggles, (unsigned long long)s.regs_flag_bypass,
                            (s.regs_exit_fallback || s.regs_exit_timeout || s.elem_jobs_failed) ? " <== FAIL" : "");
            if (s.regs_add_runs || s.regs_add_cfg_mismatch)
                std::printf("[gvu-stream] residual add by registers: runs %llu, runs whose configuration read back from the register file differs from the call style %llu%s, "
                            "runs with B in the vbank1 buffer 1 %llu\n",
                            (unsigned long long)s.regs_add_runs, (unsigned long long)s.regs_add_cfg_mismatch, s.regs_add_cfg_mismatch ? " <== FAIL" : "",
                            (unsigned long long)s.regs_add_b_buf1);
            if (s.f1_calls)
                std::printf("[gvu-stream] F1' in the tap: calls with packets %llu, calls whose oc_last packets != contexts %llu (the count rule of F1p.diff applied to these packets would misfire there), packets with oc_tile_last %llu, calls with the flag not on the one last packet %llu%s\n",
                            (unsigned long long)s.f1_calls, (unsigned long long)s.f1_ctx_mismatch, (unsigned long long)s.f1_tile_last_packets, (unsigned long long)s.f1_flag_bad,
                            s.f1_flag_bad ? " <== FAIL" : "");
            if (s.elem_jobs)
                std::printf("[gvu-stream] residual add and max pool on the clock (HAS_GVU_ELEM_CLK=1): jobs %llu (failed %llu), ticks %llu, GvuTop cycles of the jobs %llu, "
                            "ticks after done to IDLE %llu, jobs not started at the edge after the request %llu%s\n",
                            (unsigned long long)s.elem_jobs, (unsigned long long)s.elem_jobs_failed, (unsigned long long)s.elem_ticks,
                            (unsigned long long)s.elem_job_cycles, (unsigned long long)s.elem_idle_ticks, (unsigned long long)s.elem_start_late,
                            (s.elem_jobs_failed || s.elem_idle_ticks || s.elem_start_late || s.elem_ticks != s.elem_job_cycles) ? " <== FAIL" : "");
            std::fflush(stdout);
        }
        // Drain waits of the DFC behind a core pass (counted here so that the report has them).
        static void count_drain(uint64_t cycles) { stream_stats().drain_waits++; stream_stats().drain_extra += cycles; }

    private:
        struct Meta { uint32_t addr, mask, ch; uint64_t push, accept; bool bypass{false}; };
        struct Held { uint64_t due; uint32_t addr, mask; psum_vector_t<32, int32_t> data; uint32_t ch; uint64_t push, accept; };
        rtl_ref_gvu::GvuTop top_;
        rtl_ref_gvu::GvuTop::In gin_;
        bool running_{false}, stopping_{false}, table_loaded_{false}, ready_q_{false};
        uint64_t edge_{0};
        std::deque<Meta> meta_;                 // beats latched into the pipeline, in order
        std::deque<Held> bypass_q_;             // model results of beats that GvuTop does not take, due at latch edge + kDepth
        std::deque<psum_vector_t<32, int32_t>> model_q_;   // model result of each latched beat, in order
        uint64_t in_flight_{0};
        // HAS_GVU_REGS
        bool regs_on_{false}, add_regs_{false}, reg_idle_{true}, pipe_done_gemm_{false};
        rtl_ref_gvu::GvuConfigRegs regs_;
        GvuRegMaster master_;
        sc_core::sc_event reg_ev_;
        void request_regs(const std::vector<GvuRegMaster::W> &seq)
        {
            reg_idle_ = false;
            master_.request(seq, sc_core::sc_time_stamp().value());
            stream_stats().regs_seqs++;
        }
        // The inputs the register file drives (gvu_core.sv: i_gvu_pipe_en ... i_pooling_h_move), valid in the cycle after the edge of the write.
        void regs_to_in(rtl_ref_gvu::GvuTop::In &in) const
        {
            const rtl_ref_gvu::GvuConfigOut r = regs_.out();
            in.i_pipeline_en = r.pipe_en;
            in.i_gvu_op = static_cast<rtl_ref_gvu::GvuOpcode>(r.op);
            in.i_lut_act_reset = r.lut_act_reset;
            in.i_sbank_buf_sel = r.sbank_buf_sel; in.i_vbank0_buf_sel = r.vbank0_buf_sel; in.i_vbank1_buf_sel = r.vbank1_buf_sel; in.i_vbank2_buf_sel = r.vbank2_buf_sel;
            in.i_gemm_skip_act = r.gemm_fused_skip_act;
            in.i_gemm_zp16 = r.gemm_fused_z;                       // 8 bits sign-extended into the 16-bit adder (gvu_top.sv:705)
            in.i_num_tile = r.num_tile; in.i_token_len = r.token_len;
            in.i_attn_z = r.attn_z; in.i_attn_scale = r.attn_scale; in.i_attn_shift = static_cast<uint8_t>(r.attn_shift);
            in.i_pooling_mode = static_cast<int>(r.pooling_mode); in.i_pooling_stride = r.pooling_stride;
            in.i_pooling_num_w_tile = r.pooling_num_w_tile; in.i_pooling_c = r.pooling_c; in.i_pooling_oh = r.pooling_oh;
            in.i_pooling_c_move = r.pooling_c_move; in.i_pooling_h_move = r.pooling_h_move;
            in.i_res_scale_a = r.res_scale_a; in.i_res_scale_b = r.res_scale_b; in.i_res_scale_out = r.res_scale_out;
            in.i_res_shift_a = static_cast<uint8_t>(r.res_shift_a); in.i_res_shift_b = static_cast<uint8_t>(r.res_shift_b);
            in.i_res_shift_out = static_cast<uint8_t>(r.res_shift_out);
            in.i_res_zp_a = r.res_zp_a; in.i_res_zp_b = r.res_zp_b; in.i_res_zp_out = r.res_zp_out;
            in.i_res_round = static_cast<rtl_ref_gvu::RoundMode>(r.res_round); in.i_res_out_int16 = r.res_out_int16;
            in.i_res_a_row = r.res_a_row; in.i_res_b_row = r.res_b_row; in.i_res_sum_row = r.res_sum_row;
            in.i_res_last_lane_mask = r.res_last_lane_mask;
            // AGU_ND (Reg 16..24, proposal): reset value 0 = nothing bound
            if (regs_.params().num_regs == 32)
            {
                namespace R = rtl_ref_gvu;
                in.i_agu_nd_ctrl = R::AguNdRegs::ctrl(regs_.reg(R::AguNdRegs::CTRL));
                in.i_agu_src = R::AguNdRegs::unpack({{regs_.reg(R::AguNdRegs::SRC), regs_.reg(R::AguNdRegs::SRC + 1), regs_.reg(R::AguNdRegs::SRC + 2), regs_.reg(R::AguNdRegs::SRC + 3)}});
                in.i_agu_dst = R::AguNdRegs::unpack({{regs_.reg(R::AguNdRegs::DST), regs_.reg(R::AguNdRegs::DST + 1), regs_.reg(R::AguNdRegs::DST + 2), regs_.reg(R::AguNdRegs::DST + 3)}});
            }
        }
        std::array<uint32_t, 9> agu_shadow_{};   // the Reg 16..24 values the DFC wrote last (HAS_GVU_AGU_ND with HAS_GVU_REGS)
        // the AGU_ND writes ahead of an operation's sequence (HAS_GVU_AGU_ND on)
        std::vector<GvuRegMaster::W> with_agu_nd(const rtl_ref_gvu::GvuTop::In &in, std::vector<GvuRegMaster::W> seq)
        {
            if (!GvuTopBridge::agu_nd_level()) return seq;
            std::vector<GvuRegMaster::W> s = gvu_regs_dfc::agu_nd(in.i_agu_nd_ctrl, in.i_agu_src, in.i_agu_dst, agu_shadow_);
            s.insert(s.end(), seq.begin(), seq.end());
            return s;
        }
        // After the tick of the unit in this edge: the register file takes the write of the master, pipe_done (clears the enable bit and sets
        // the done flag) and the end of the table load (toggles the vbank1 buffer bit), in the order of gvu_config_regs.sv.
        void regs_post(const rtl_ref_gvu::GvuTop::Out &o, rtl_ref_gvu::control_fsm_t st_before, bool ticked, const GvuRegMaster::Step &ms,
                       const rtl_ref_gvu::GvuTop::In &in)
        {
            namespace R = rtl_ref_gvu;
            StreamStats &st = stream_stats();
            bool done = false;
            if (ticked)
            {
                if (in.i_gvu_op == R::GVU_OP_GEMM_FUSED) done = o.o_gemm_fused_pipe_done;
                else if (in.i_gvu_op == R::GVU_OP_MAXPOOL) done = o.o_maxpool_pipe_done;
                else if (in.i_gvu_op == R::GVU_OP_RESIDUAL || in.i_gvu_op == R::GVU_OP_ELEMWISE) done = o.o_residual_pipe_done;
            }
            const bool pulse = ticked && st_before == R::LUT_ACT_INIT && top_.state() != R::LUT_ACT_INIT;
            if (ms.apply || done || pulse)
            {
                R::GvuConfigIn c;
                if (ms.apply) { c.addr = regs_.reg_index(ms.w.byte_addr); c.wren = true; c.wdata = ms.w.data; c.wmask = R::GvuConfigRegs::strb_to_mask(ms.w.strb); st.regs_writes++; }
                c.gvu_pipe_done = done; c.lut_init_done = pulse;
                regs_.tick(c);
                if (pulse) st.regs_lut_toggles++;
            }
            if (done && in.i_gvu_op == R::GVU_OP_GEMM_FUSED) { pipe_done_gemm_ = true; if (!in.i_gemm_stream_stop) st.regs_exit_flags++; }
        }
        // observer of the RTL stream
        uint32_t rtl_agu_{0};
        int rtl_window_{0};

        static bool fit(int64_t v, int bits) { const int64_t h = static_cast<int64_t>(1) << (bits - 1); return v >= -h && v < h; }
        static rtl_ref_gvu::RoundMode map_round(int m)
        {
            namespace R = rtl_ref_gvu;
            static const R::RoundMode t[7] = {R::RND_FLOOR, R::RND_NEAR_UP, R::RND_NEAR_AWAY, R::RND_NEAR_EVEN, R::RND_CEIL, R::RND_TRUNC, R::RND_AWAY};
            return t[m < 0 ? 0 : (m > 6 ? 6 : m)];
        }
        // ---- residual add and max pool: parameters, loads and read-backs shared by the call style and the jobs on the clock
        static constexpr size_t kAddRows = 128;          // rows of one residual add run (the int8 rows of vbank1)
        struct AddCfg { int zpA, zpB, zpO; int64_t SA, SB, SO; int sA, sB, sO, round_mode; };
        struct PoolGeom { int g, H, W, kk, s, p, Hp, Wp, Wt, Ho, Wo, OL, cmax; bool ok; };
        static PoolGeom pool_geom(int g, int H, int W, int kk, int s, int p)
        {
            PoolGeom q{};
            q.g = g; q.H = H; q.W = W; q.kk = kk; q.s = s; q.p = p;
            const bool shape = (kk == 2 && (s == 1 || s == 2)) || (kk == 3 && s == 1) || (kk == 4 && (s == 1 || s == 2 || s == 4)) || (kk == 5 && s == 1);
            q.Hp = H + 2 * p; q.Wp = W + 2 * p; q.Wt = (q.Wp + 31) / 32;
            q.Ho = (q.Hp - kk) / s + 1; q.Wo = (q.Wp - kk) / s + 1;
            q.OL = s == 1 ? 32 : (kk == s ? (32 - kk) / s + 1 : (32 - kk) / s + 2);
            q.cmax = std::min({800 / std::max(1, q.Hp * q.Wt), 1024 / std::max(1, q.Ho * q.Wt), 255});
            q.ok = shape && p >= 0 && q.Ho >= 1 && q.Wo >= 1 && q.Ho <= 255 && q.Wt <= 255 && q.Wo <= q.Wt * q.OL && q.cmax >= 1;
            return q;
        }
        static uint64_t pool_guard(const PoolGeom &q, int n) { return static_cast<uint64_t>(n) * q.Ho * q.Wt * 64 + 1000; }
        // Residual add run of `rows` rows from row r0: A into vbank0 buffer 0 (one row of 32 per word), B into vbank1 buffer 0 (four rows
        // per word); returns the inputs of the run.
        rtl_ref_gvu::GvuTop::In add_load(const int8_t *A, const int8_t *B, size_t n, size_t r0, size_t rows, size_t rows_total, const AddCfg &a,
                                         unsigned b_buf = 0)
        {
            namespace R = rtl_ref_gvu;
            std::vector<R::SpBytes> v1((rows + 3) / 4, R::SpBytes(128, 0));
            for (size_t r = 0; r < rows; r++)
            {
                R::SpBytes ra(32, 0);
                for (size_t l = 0; l < 32; l++)
                {
                    const size_t e = (r0 + r) * 32 + l;
                    if (e >= n) break;
                    ra[l] = static_cast<uint8_t>(A[e]);
                    v1[r / 4][32 * (r % 4) + l] = static_cast<uint8_t>(B[e]);
                }
                top_.scratchpad().sram(0, 0).poke(GvuTopBridge::agu_nd_a_row(r), ra);
            }
            for (size_t k = 0; k < v1.size(); k++) top_.scratchpad().sram(1, b_buf).poke(static_cast<unsigned>(k), v1[k]);
            R::GvuTop::In i;
            i.i_gvu_op = R::GVU_OP_RESIDUAL; i.i_num_tile = 1; i.i_token_len = static_cast<uint32_t>(rows);
            i.i_res_scale_a = static_cast<int32_t>(a.SA); i.i_res_shift_a = static_cast<uint8_t>(a.sA); i.i_res_zp_a = static_cast<int8_t>(a.zpA);
            i.i_res_scale_b = static_cast<int32_t>(a.SB); i.i_res_shift_b = static_cast<uint8_t>(a.sB); i.i_res_zp_b = static_cast<int8_t>(a.zpB);
            i.i_res_scale_out = static_cast<int32_t>(a.SO); i.i_res_shift_out = static_cast<uint8_t>(a.sO); i.i_res_zp_out = static_cast<int8_t>(a.zpO);
            i.i_res_round = map_round(a.round_mode); i.i_res_out_int16 = false;
            const size_t tail = n - (r0 + rows - 1) * 32;
            i.i_res_last_lane_mask = (r0 + rows == rows_total && tail < 32) ? ((1u << tail) - 1u) : 0xFFFFFFFFu;
            GvuTopBridge::agu_nd_add(i, rows);
            i.i_pipeline_en = true;
            return i;
        }
        // Result of a finished residual add run (vbank0 buffer 0): compared with the model result in out, GvuTop's written into out.
        void add_read(int8_t *out, size_t n, size_t r0, size_t rows)
        {
            StreamStats &st = stream_stats();
            for (size_t r = 0; r < rows; r++)
            {
                const rtl_ref_gvu::SpBytes orow = top_.scratchpad().sram(0, 0).word(GvuTopBridge::agu_nd_out_row(r));
                for (size_t l = 0; l < 32; l++)
                {
                    const size_t e = (r0 + r) * 32 + l;
                    if (e >= n) break;
                    const int8_t h = static_cast<int8_t>(orow[l]);
                    st.add_lanes++;
                    if (h != out[e]) st.add_differ++;
                    out[e] = h;
                }
            }
        }
        // Max pool run of n channels from channel c0 (the unit reset just before): the tensor padded with -128 into vbank0 buffer 0 as
        // rows of 32 bytes; returns the inputs of the run.
        rtl_ref_gvu::GvuTop::In pool_load(const int8_t *in, const PoolGeom &q, int c0, int n)
        {
            namespace R = rtl_ref_gvu;
            for (int c = 0; c < n; c++)
                for (int y = 0; y < q.Hp; y++)
                    for (int t = 0; t < q.Wt; t++)
                    {
                        R::SpBytes row(32, 0);
                        for (int l = 0; l < 32 && 32 * t + l < q.Wp; l++)
                        {
                            const int yy = y - q.p, xx = 32 * t + l - q.p;
                            row[static_cast<size_t>(l)] = (yy < 0 || yy >= q.H || xx < 0 || xx >= q.W)
                                ? static_cast<uint8_t>(0x80) : static_cast<uint8_t>(in[(static_cast<size_t>(c0 + c) * q.H + yy) * q.W + xx]);
                        }
                        top_.scratchpad().sram(0, 0).poke(static_cast<unsigned>((c * q.Hp + y) * q.Wt + t), row);
                    }
            R::GvuTop::In i;
            i.i_gvu_op = R::GVU_OP_MAXPOOL;
            i.i_pooling_mode = q.kk - 2; i.i_pooling_stride = static_cast<unsigned>(q.s); i.i_pooling_num_w_tile = static_cast<unsigned>(q.Wt);
            i.i_pooling_c = static_cast<unsigned>(n); i.i_pooling_oh = static_cast<unsigned>(q.Ho);
            i.i_pooling_c_move = static_cast<unsigned>(q.Wt * q.Hp); i.i_pooling_h_move = static_cast<unsigned>(q.Wt * q.s);
            GvuTopBridge::agu_nd_pool(i, static_cast<unsigned>(q.Wt), static_cast<unsigned>(q.Ho), static_cast<unsigned>(n));
            i.i_pipeline_en = true;
            return i;
        }
        // Result of a finished max pool run (vbank2 port 1) into res.
        void pool_read(std::vector<int8_t> &res, const PoolGeom &q, int c0, int n)
        {
            rtl_ref_gvu::GvuTop::In ag;                      // the AGU_ND destination of the run (HAS_GVU_AGU_ND)
            GvuTopBridge::agu_nd_pool(ag, static_cast<unsigned>(q.Wt), static_cast<unsigned>(q.Ho), static_cast<unsigned>(n));
            for (int cc = 0; cc < n; cc++)
                for (int oh = 0; oh < q.Ho; oh++)
                    for (int ow = 0; ow < q.Wo; ow++)
                    {
                        const unsigned row = GvuTopBridge::agu_nd_pool_row(ag, static_cast<unsigned>((cc * q.Ho + oh) * q.Wt + ow / q.OL));
                        const rtl_ref_gvu::SpBytes w = top_.scratchpad().sram(2, 0).word(row >> 2);
                        res[(static_cast<size_t>(c0 + cc) * q.Ho + oh) * q.Wo + ow] = static_cast<int8_t>(w[(row & 3u) * 32u + static_cast<unsigned>(ow % q.OL)]);
                    }
        }
        // The group result compared with the model result in out, GvuTop's written into out.
        static void pool_finish(const std::vector<int8_t> &res, int8_t *out)
        {
            StreamStats &st = stream_stats();
            for (size_t e = 0; e < res.size(); e++)
            {
                st.mp_lanes++;
                if (res[e] != out[e]) st.mp_differ++;
                out[e] = res[e];
            }
        }

        // ---- jobs on the clock
        struct Job
        {
            enum Kind { ADD, MAXPOOL } kind{ADD};
            enum State { IDLE, PENDING, ACTIVE, DONE } state{IDLE};
            enum Phase { WRITE, RUN, CLEAR_WAIT, CLEAR } phase{WRITE};   // max pool and residual add by registers
            uint64_t wait_edges{0};
            unsigned prev_rows{0};               // residual add by registers: rows and last-row lane mask of the run before (values in the registers)
            uint32_t prev_mask{0};
            std::vector<int8_t> a, b, res;       // operands (copies) and, for max pool, the result of the group
            int8_t *out{nullptr};
            size_t n{0}, rows_total{0}, r0{0}, rows{0};
            AddCfg cfg{};
            PoolGeom q{};
            int c0{0}, nc{0};
            rtl_ref_gvu::GvuTop::In in;          // inputs of the current run
            uint64_t c{0}, guard{0}, cyc{0}, model_cycles{0};
            sc_core::sc_time req;
        };
        Job job_;
        sc_core::sc_event job_ev_;
        sc_core::sc_time last_edge_{sc_core::SC_ZERO_TIME};
        void request_job()
        {
            job_.req = sc_core::sc_time_stamp();
            job_.state = Job::PENDING;
            stream_stats().elem_jobs++;
        }
        // Loads the run that starts at job_.r0 / job_.c0; its first tick is at the next edge.
        void job_begin_run()
        {
            Job &j = job_;
            if (j.kind == Job::ADD)
            {
                j.rows = std::min(kAddRows, j.rows_total - j.r0);
                j.in = add_load(j.a.data(), j.b.data(), j.n, j.r0, j.rows, j.rows_total, j.cfg);
                j.guard = 4 * j.rows + 400;
            }
            else
            {
                j.nc = std::min(j.q.cmax, j.q.g - j.c0);
                reset_unit();                            // B12: a run starts from a reset of the pooling feeder
                j.in = pool_load(j.a.data(), j.q, j.c0, j.nc);
                j.guard = pool_guard(j.q, j.nc);
            }
            j.c = 0;
        }
        void job_end(bool ok)
        {
            StreamStats &st = stream_stats();
            Job &j = job_;
            if (ok && j.kind == Job::ADD) { st.add_cycles += j.cyc; st.add_model_cycles += j.model_cycles; }
            if (ok && j.kind == Job::MAXPOOL) { st.mp_cycles += j.cyc; st.mp_model_cycles += j.model_cycles; }
            if (!ok) st.elem_jobs_failed++;
            st.elem_job_cycles += j.cyc;
            j.state = Job::DONE;
            j.a.clear(); j.b.clear(); j.res.clear();
            job_ev_.notify(sc_core::SC_ZERO_TIME);
        }
        // HAS_GVU_REGS: a max pool run by registers. The banks are loaded and the unit reset (the model's B12 fix; the register file is not reset),
        // then gvu_regs_dfc::pool_run (POOLING_0, POOLING_1, start) is issued; the unit runs when the start write lands; at pipe_done the done flag is cleared
        // by a write before the next run. The GvuTop cycles of a run are the ticks with the enable bit set, as in the call style.
        void job_begin_run_regs(const sc_core::sc_time &now)
        {
            Job &j = job_;
            j.nc = std::min(j.q.cmax, j.q.g - j.c0);
            reset_unit();
            j.in = pool_load(j.a.data(), j.q, j.c0, j.nc);
            j.guard = pool_guard(j.q, j.nc);
            j.c = 0; j.wait_edges = 0; j.phase = Job::WRITE;
            master_.request(with_agu_nd(j.in, gvu_regs_dfc::pool_run(static_cast<unsigned>(j.q.kk - 2), static_cast<unsigned>(j.q.s), static_cast<unsigned>(j.q.Wt),
                                                   static_cast<unsigned>(j.nc), static_cast<unsigned>(j.q.Ho), static_cast<unsigned>(j.q.Wt * j.q.Hp),
                                                   static_cast<unsigned>(j.q.Wt * j.q.s))), now.value());
            reg_idle_ = false;
            stream_stats().regs_seqs++;
        }
        void job_step_pool_regs(const sc_core::sc_time &now, const sc_core::sc_time &prev, const GvuRegMaster::Step &ms)
        {
            namespace R = rtl_ref_gvu;
            StreamStats &st = stream_stats();
            Job &j = job_;
            if (j.state == Job::PENDING)
            {
                if (!(now > j.req)) return;
                if (j.req != prev) st.elem_start_late++;
                leave_for_job();
                j.state = Job::ACTIVE; j.r0 = 0; j.c0 = 0; j.cyc = 0;
                job_begin_run_regs(now);
                return;                                  // the sequence counts from the first edge after this one
            }
            R::GvuTop::In in = j.in;
            regs_to_in(in);
            const bool run = in.i_pipeline_en;
            const R::control_fsm_t sb = top_.state();
            R::GvuTop::Out o;
            bool done = false;
            if (run) { o = top_.tick(in, true); j.c++; st.elem_ticks++; done = o.o_maxpool_pipe_done; }
            regs_post(o, sb, run, ms, in);
            auto fail = [&]()
            {
                st.mp_not_expr++;
                reset_unit();
                job_end(false);
            };
            if (j.phase == Job::WRITE)
            {
                if (run) j.phase = Job::RUN;
                else if (++j.wait_edges > 4000) { fail(); return; }
            }
            if (j.phase == Job::RUN)
            {
                if (done)
                {
                    if (top_.state() != R::IDLE) { fail(); return; }
                    j.cyc += j.c; st.mp_runs++; st.regs_pool_runs++;
                    pool_read(j.res, j.q, j.c0, j.nc);
                    j.phase = Job::CLEAR_WAIT;
                }
                else if (j.c >= j.guard) { fail(); return; }
            }
            if (j.phase == Job::CLEAR_WAIT && !master_.busy())
            {
                master_.request(gvu_regs_dfc::clear_done(), now.value());
                reg_idle_ = false; st.regs_seqs++;
                j.phase = Job::CLEAR;
                return;
            }
            if (j.phase == Job::CLEAR && ms.drained)
            {
                j.c0 += j.q.cmax;
                if (j.c0 < j.q.g) { job_begin_run_regs(now); return; }
                reset_unit();
                pool_finish(j.res, j.out);
                job_end(true);
            }
        }
        // HAS_GVU_REGS: a residual add run by registers (gvu_regs_dfc::add_run): A into vbank0 buffer 0, B into the vbank1 buffer that Reg 2
        // selects (a table load toggles it), the sequence of writes, the unit runs when the start write has landed, the done flag is cleared
        // by a write before the next run. The configuration the unit sees is the register file's; it is compared with the configuration of
        // the call style (add_load) when the run starts.
        void job_begin_run_regs_add(const sc_core::sc_time &now)
        {
            Job &j = job_;
            j.rows = std::min(kAddRows, j.rows_total - j.r0);
            const bool b1 = regs_.out().vbank1_buf_sel;
            if (b1) stream_stats().regs_add_b_buf1++;
            j.in = add_load(j.a.data(), j.b.data(), j.n, j.r0, j.rows, j.rows_total, j.cfg, b1 ? 1u : 0u);
            j.guard = 4 * j.rows + 400;
            j.c = 0; j.wait_edges = 0; j.phase = Job::WRITE;
            gvu_regs_dfc::AddRegs c;
            c.scale_a = j.in.i_res_scale_a; c.scale_b = j.in.i_res_scale_b; c.scale_out = j.in.i_res_scale_out;
            c.shift_a = j.in.i_res_shift_a; c.shift_b = j.in.i_res_shift_b; c.shift_out = j.in.i_res_shift_out;
            c.round = static_cast<unsigned>(j.in.i_res_round); c.out_int16 = j.in.i_res_out_int16;
            c.zp_a = j.in.i_res_zp_a; c.zp_b = j.in.i_res_zp_b; c.zp_out = j.in.i_res_zp_out;
            c.a_row = j.in.i_res_a_row; c.b_row = j.in.i_res_b_row; c.sum_row = j.in.i_res_sum_row;
            const bool first = j.r0 == 0;
            master_.request(with_agu_nd(j.in, gvu_regs_dfc::add_run(first, c, static_cast<unsigned>(j.rows), j.in.i_res_last_lane_mask, j.prev_rows, j.prev_mask)), now.value());
            j.prev_rows = static_cast<unsigned>(j.rows); j.prev_mask = j.in.i_res_last_lane_mask;
            reg_idle_ = false;
            stream_stats().regs_seqs++;
        }
        void job_step_add_regs(const sc_core::sc_time &now, const sc_core::sc_time &prev, const GvuRegMaster::Step &ms)
        {
            namespace R = rtl_ref_gvu;
            StreamStats &st = stream_stats();
            Job &j = job_;
            if (j.state == Job::PENDING)
            {
                if (!(now > j.req)) return;
                if (j.req != prev) st.elem_start_late++;
                leave_for_job();
                j.state = Job::ACTIVE; j.r0 = 0; j.c0 = 0; j.cyc = 0; j.prev_rows = 0; j.prev_mask = 0;
                job_begin_run_regs_add(now);
                return;                                  // the sequence counts from the first edge after this one
            }
            R::GvuTop::In in = j.in;
            regs_to_in(in);
            const bool run = in.i_pipeline_en;
            const R::control_fsm_t sb = top_.state();
            R::GvuTop::Out o;
            bool done = false;
            if (run)
            {
                if (j.phase == Job::WRITE)
                {
                    const R::GvuTop::In &w = j.in;
                    if (in.i_gvu_op != R::GVU_OP_RESIDUAL || in.i_token_len != w.i_token_len || in.i_num_tile != w.i_num_tile ||
                        in.i_res_scale_a != w.i_res_scale_a || in.i_res_scale_b != w.i_res_scale_b || in.i_res_scale_out != w.i_res_scale_out ||
                        in.i_res_shift_a != w.i_res_shift_a || in.i_res_shift_b != w.i_res_shift_b || in.i_res_shift_out != w.i_res_shift_out ||
                        in.i_res_zp_a != w.i_res_zp_a || in.i_res_zp_b != w.i_res_zp_b || in.i_res_zp_out != w.i_res_zp_out ||
                        in.i_res_round != w.i_res_round || in.i_res_out_int16 != w.i_res_out_int16 || in.i_res_a_row != w.i_res_a_row ||
                        in.i_res_b_row != w.i_res_b_row || in.i_res_sum_row != w.i_res_sum_row || in.i_res_last_lane_mask != w.i_res_last_lane_mask ||
                        in.i_vbank0_buf_sel)
                        st.regs_add_cfg_mismatch++;
                }
                o = top_.tick(in, true); j.c++; st.elem_ticks++; done = o.o_residual_pipe_done;
            }
            regs_post(o, sb, run, ms, in);
            auto fail = [&]()
            {
                st.add_not_expr++;
                reset_unit();
                job_end(false);
            };
            if (j.phase == Job::WRITE)
            {
                if (run) j.phase = Job::RUN;
                else if (++j.wait_edges > 4000) { fail(); return; }
            }
            if (j.phase == Job::RUN)
            {
                if (done)
                {
                    if (top_.state() != R::IDLE) { fail(); return; }
                    j.cyc += j.c; st.regs_add_runs++;
                    add_read(j.out, j.n, j.r0, j.rows);
                    j.phase = Job::CLEAR_WAIT;
                }
                else if (j.c >= j.guard) { fail(); return; }
            }
            if (j.phase == Job::CLEAR_WAIT && !master_.busy())
            {
                master_.request(gvu_regs_dfc::clear_done(), now.value());
                reg_idle_ = false; st.regs_seqs++;
                j.phase = Job::CLEAR;
                return;
            }
            if (j.phase == Job::CLEAR && ms.drained)
            {
                j.r0 += kAddRows;
                if (j.r0 < j.rows_total) { job_begin_run_regs_add(now); return; }
                job_end(true);
            }
        }
        // One edge of a job (clock_proc). now / prev: the time of this edge and of the edge before.
        void job_step(const sc_core::sc_time &now, const sc_core::sc_time &prev)
        {
            namespace R = rtl_ref_gvu;
            StreamStats &st = stream_stats();
            Job &j = job_;
            if (j.state == Job::PENDING)
            {
                if (!(now > j.req)) return;              // the edge of the request: the job starts at the next one
                if (j.req != prev) st.elem_start_late++; // the request was not made at the edge before this one
                leave_for_job();
                j.state = Job::ACTIVE; j.r0 = 0; j.c0 = 0; j.cyc = 0;
                job_begin_run();
            }
            const R::GvuTop::Out o = top_.tick(j.in, true);
            j.c++;
            st.elem_ticks++;
            const bool done = j.kind == Job::ADD ? o.o_residual_pipe_done : o.o_maxpool_pipe_done;
            if (!done && j.c < j.guard) return;
            j.in.i_pipeline_en = false;
            int k = 0;
            for (; k < 4 && top_.state() != R::IDLE; k++) top_.tick(j.in, true);
            st.elem_idle_ticks += uint64_t(k);
            if (!done || top_.state() != R::IDLE)
            {
                if (j.kind == Job::ADD) st.add_not_expr++; else st.mp_not_expr++;
                reset_unit();
                job_end(false);
                return;
            }
            j.cyc += j.c;
            if (j.kind == Job::ADD)
            {
                add_read(j.out, j.n, j.r0, j.rows);
                j.r0 += kAddRows;
                if (j.r0 < j.rows_total) { job_begin_run(); return; }
            }
            else
            {
                st.mp_runs++;
                pool_read(j.res, j.q, j.c0, j.nc);
                j.c0 += j.q.cmax;
                if (j.c0 < j.q.g) { job_begin_run(); return; }
                reset_unit();
                pool_finish(j.res, j.out);
            }
            job_end(true);
        }
        void reset_unit()
        {
            rtl_ref_gvu::GvuTop::In i;
            for (int c = 0; c < 5; c++) top_.tick(i, false);
            top_.tick(i, true);
            gin_ = rtl_ref_gvu::GvuTop::In();
            table_loaded_ = false;
            running_ = stopping_ = false;
            meta_.clear(); model_q_.clear(); in_flight_ = 0;
            stream_stats().resets++;
        }
        // The unit is idle (no layer) when an ELEM_WISE instruction starts; a leftover run is dropped.
        void leave_for_job()
        {
            if (running_ || top_.state() != rtl_ref_gvu::IDLE) reset_unit();
        }

        void clock_proc()
        {
            namespace R = rtl_ref_gvu;
            StreamStats &st = stream_stats();
            edge_++;
            const sc_core::sc_time now = sc_core::sc_time_stamp(), prev = last_edge_;
            last_edge_ = now;
            if (!i_rstn.read())
            {
                R::GvuTop::In empty;
                top_.tick(empty, false);
                gin_ = R::GvuTop::In();
                running_ = stopping_ = false; table_loaded_ = false;
                meta_.clear(); model_q_.clear(); bypass_q_.clear(); in_flight_ = 0;
                ready_q_ = false;
                o_psum_ready.write(false);
                if (job_.state == Job::PENDING || job_.state == Job::ACTIVE) job_end(false);
                return;
            }
            // HAS_GVU_REGS: one edge of the register write port of the DFC
            GvuRegMaster::Step ms;
            if (regs_on_)
            {
                ms = master_.edge(now.value());
                if (ms.drained) { reg_idle_ = true; reg_ev_.notify(sc_core::SC_ZERO_TIME); }
                stream_stats().regs_busy_edges = master_.busy_edges();
            }
            // HAS_GVU_ELEM_CLK: a residual add or max pool job owns the unit (no GEMM_FUSED layer runs during an ELEM_WISE instruction)
            if (job_.state == Job::PENDING || job_.state == Job::ACTIVE)
            {
                if (regs_on_ && job_.kind == Job::MAXPOOL) job_step_pool_regs(now, prev, ms);
                else if (regs_on_ && add_regs_ && job_.kind == Job::ADD) job_step_add_regs(now, prev, ms);
                else job_step(now, prev);
                ready_q_ = false;
                o_psum_ready.write(false);
                return;
            }
            // ---- sample the boundary (values of the cycle before this edge)
            const bool ready_pre = ready_q_;                        // what o_psum_ready showed during that cycle
            const bool take = i_psum_valid.read() && ready_pre;
            R::GvuTop::In in = gin_;
            if (regs_on_) regs_to_in(in);
            if (running_ && stopping_)
            {
                if (top_.state() == R::IDLE) { running_ = stopping_ = false; gin_.i_pipeline_en = false; gin_.i_gemm_stream_stop = false; in = gin_; }
                else { gin_.i_gemm_stream_stop = true; in = gin_; }
            }
            in.i_psum_valid = false;
            Meta m{};
            psum_vector_t<32, int32_t> model;
            bool bypass_beat = false;
            if (take)
            {
                const psum_vector_t<32, int32_t> d = i_psum_data.read();
                model = i_side_model.read();
                m.addr = i_side_addr.read(); m.mask = i_side_mask.read(); m.ch = i_side_ch.read();
                if (i_side_bypass.read())
                {
                    bypass_beat = true;
                    // with the register file the beat still travels through the unit (empty lane mask) so that the flags oc_last / oc_tile_last
                    // reach it; its result is the model's, written by bypass_q_
                    if (regs_on_) { in.i_psum_valid = true; in.i_psum_mask = 0; in.i_psum_addr = m.addr; m.bypass = true; }
                }
                else
                {
                    in.i_psum_valid = true;
                    for (size_t l = 0; l < size_t(R::GVU_LANES); l++) in.i_psum_vec[l] = d[l];
                    in.i_psum_scale = static_cast<int32_t>(i_side_scale.read()); in.i_psum_shift = static_cast<uint8_t>(i_side_shift.read());
                    in.i_psum_mask = m.mask; in.i_psum_addr = m.addr;
                }
                if (regs_on_ && in.i_psum_valid)
                {
                    in.i_oc_last = i_oc_last.read(); in.i_oc_tile_last = i_oc_tile_last.read();
                    if (bypass_beat && in.i_oc_last && in.i_oc_tile_last) st.regs_flag_bypass++;
                }
                st.accepted++;
                m.push = i_side_push.read(); m.accept = edge_;
                const uint64_t lat = m.accept - m.push;
                st.lat_push_accept_min = std::min(st.lat_push_accept_min, lat); st.lat_push_accept_max = std::max(st.lat_push_accept_max, lat);
                // observer of the RTL
                if (rtl_agu_ != m.ch) st.rtl_agu_mismatch++;
                rtl_agu_++;
                if (rtl_window_ > 0) st.rtl_at_risk++;
                if (i_oc_last.read() && i_oc_tile_last.read()) { st.rtl_exit_events++; rtl_window_ = kRtlExitDelay; rtl_agu_ = 0; }
            }
            if (rtl_window_ > 0) rtl_window_--;
            // ---- the unit
            // A tick with no beat in a quiet unit changes nothing (IDLE with no run, or GEMM_FUSED_ST1 with an empty pipeline): skipped, as
            // the call-style connection never ticks outside the beats. Without this a whole network costs one GvuTop tick per cycle.
            const bool quiet = regs_on_
                ? (!in.i_psum_valid && in_flight_ == 0 && !gin_.i_gemm_stream_stop &&
                   ((top_.state() == R::IDLE && !in.i_pipeline_en && !in.i_lut_act_reset) ||
                    (top_.state() == R::GEMM_FUSED_ST1 && in.i_pipeline_en)))
                : (!in.i_psum_valid && in_flight_ == 0 &&
                   ((!running_ && top_.state() == R::IDLE) ||
                    (running_ && !stopping_ && top_.state() == R::GEMM_FUSED_ST1)));
            const R::control_fsm_t st_before = top_.state();
            const R::GvuTop::Out o = quiet ? R::GvuTop::Out() : top_.tick(in, true);
            if (regs_on_) regs_post(o, st_before, !quiet, ms, in);
            if (in.i_gemm_lut_reload && top_.state() == R::LUT_ACT_INIT) gin_.i_gemm_lut_reload = false;
            if (in.i_psum_valid) { meta_.push_back(m); model_q_.push_back(model); in_flight_++; }
            if (bypass_beat) bypass_q_.push_back(Held{edge_ + kDepth, m.addr, m.mask, model, m.ch, m.push, m.accept});
            if (o.o_gemm_fused_valid && in_flight_)
            {
                const Meta mt = meta_.front(); meta_.pop_front();
                const psum_vector_t<32, int32_t> md = model_q_.front(); model_q_.pop_front();
                in_flight_--;
                if (!mt.bypass) {
                st.outputs++;
                if (o.o_gemm_fused_mask != mt.mask || o.o_gemm_fused_addr != mt.addr) st.meta_bad++;
                const uint64_t lat = edge_ - mt.accept;
                st.lat_accept_out_min = std::min(st.lat_accept_out_min, lat); st.lat_accept_out_max = std::max(st.lat_accept_out_max, lat);
                psum_vector_t<32, int32_t> r;
                for (size_t l = 0; l < size_t(R::GVU_LANES); l++)
                {
                    if (!((mt.mask >> l) & 1u)) continue;
                    const int h = o.o_gemm_fused_data[l];
                    st.lanes++;
                    if (h != md[l]) st.miss("ch=%u addr=%u lane=%zu model=%d GvuTop=%d", mt.ch, mt.addr, l, int(md[l]), h);
                    r[l] = h;
                }
                emit(mt.addr, mt.mask, mt.ch, r);
                if (tracing) trace.push_back(Trace{mt.push, mt.accept, edge_, mt.addr});
                }
            }
            while (!bypass_q_.empty() && bypass_q_.front().due <= edge_)
            {
                const Held h = bypass_q_.front(); bypass_q_.pop_front();
                emit(h.addr, h.mask, h.ch, h.data);
                if (tracing) trace.push_back(Trace{h.push, h.accept, edge_, h.addr});
            }
            // o_psum_ready for the next cycle: the state after this edge
            ready_q_ = ready_for_beats() && !(edge_ >= hold_from && edge_ < hold_until);
            o_psum_ready.write(ready_q_);
        }
        void emit(uint32_t addr, uint32_t mask, uint32_t ch, const psum_vector_t<32, int32_t> &v)
        {
            if (result_sink) result_sink(addr, mask, v);
            if (dump)
            {
                std::fprintf(dump, "B %u %08x", ch, mask);
                for (int l = 0; l < 32; l++) std::fprintf(dump, " %02x", ((mask >> l) & 1u) ? unsigned(static_cast<uint8_t>(static_cast<int8_t>(v[size_t(l)]))) : 0u);
                std::fprintf(dump, "\n");
            }
        }
    };
} // namespace has

#endif // HAS_GVU_TOP_STREAM_H
