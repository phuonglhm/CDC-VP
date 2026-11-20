// rtl_ref_gvu_lut_r2.h -- the LUT unit of the vector unit in RTL revision 2 (neo_lite, HAS_NEO_REV = 2): gvu_lut_mem.sv,
// gvu_lut_lookup_lane.sv and gvu_lut_unit.sv. The values are those of rtl_ref_gvu_lut.h (the oracle on the RTL shows no difference
// of a looked-up value between revisions 1 and 2); what changed is the timing, one signal of the lookup lane and the table load:
//   * the table memory has three elastic read stages (a request taken at edge N answers at edge N+2 when nothing stalls; the answer
//     stays until the consumer takes it); the lookup lane keeps the metadata of every request (4 slots) until its answer is taken.
//     Latency with the default parameters (lat_lut_r2()): direct (act / exp) 3, log-scale 6, PWL 7 (+1 with LERP_SHIFT_PIPE);
//   * o_e_shift of the lookup lane: log-scale takes it from pipe2 (E_shift_2r), PWL from pipe4; revision 1 took it from pipe4 in
//     both modes, so its value in log-scale mode was that of a later request;
//   * o_pipe_almost_done: log-scale = the valid input of pipe2, PWL = the valid output of pipe3 (no longer pipe0 in log-scale);
//   * table load: the write counter wr_cnt_r (WR_CNT_W bits) selects a fixed chunk of the table; o_init_done = i_lut_wen &&
//     wr_cnt_r == max; there is no running write address (lut_waddr_r) any more. For a load that starts at 0 and runs the whole
//     table the tables are the same as with revision 1.
// Pure C++17. The interface mirrors rtl_ref_gvu_lut.h (eval / tick); a unit with NUM_LANE lanes is LutUnitT<NUM_LANE>.
#ifndef RTL_REF_GVU_LUT_R2_H
#define RTL_REF_GVU_LUT_R2_H

#include <array>
#include <cstdint>
#include <vector>
#include "has/rtl_ref_gvu_lut.h"

namespace rtl_ref_gvu
{
    // Latency from an accepted request to the output valid, revision 2 (parameters as lat_lut of rtl_ref_gvu_lut.h).
    inline int lat_lut_r2(LutType t, LutIdxMode m, const LutParams &p = LutParams())
    {
        if (lut_is_direct(t)) return 3;
        if (m == LUT_LOG_SCALE) return 6;                                      // pipe0, pipe1, memory (3), pipe2
        return 6 + 1 + (p.LERP_MUL_PIPE ? 1 : 0) + (p.LERP_SHIFT_PIPE ? 1 : 0) - 1;   // log-scale + pipe3 (+ pipe4): 7 with LERP_MUL_PIPE
    }

    // ---- gvu_lut_mem.sv: tables and write port -------------------------------------------------------------------------
    class LutMemR2
    {
    public:
        explicit LutMemR2(const LutParams &p = LutParams()) : P_(p) { act_.fill(0); exp_.fill(0); recip_.fill(0); rsqrt_.fill(0); }
        unsigned act_exp_chunks() const { return static_cast<unsigned>(256 * 8 / P_.INIT_DW); }
        unsigned recip_chunks() const { return static_cast<unsigned>(256 * 16 / P_.INIT_DW); }
        unsigned rsqrt_chunks() const { return static_cast<unsigned>(512 * 16 / P_.INIT_DW); }
        unsigned wr_cnt_mask() const { unsigned w = 1; while ((1u << w) < rsqrt_chunks()) w++; return (1u << w) - 1u; }   // WR_CNT_W = clog2(RSQRT_CHUNKS)
        unsigned max_wr_cnt(LutType wsel) const { return (wsel == LUT_RSQRT ? rsqrt_chunks() : (wsel == LUT_RECIP ? recip_chunks() : act_exp_chunks())) - 1u; }
        bool o_init_done(LutType wsel, bool wen) const { return wen && wr_cnt_r_ == max_wr_cnt(wsel); }
        unsigned wr_cnt() const { return wr_cnt_r_; }
        // One clock edge of the write port. i_lut_wdata: INIT_DW / 8 bytes, least significant first (read only when i_lut_wen).
        void tick(bool i_rstn, bool i_lut_wen, LutType i_lut_wsel, const uint8_t *i_lut_wdata)
        {
            if (!i_rstn) { wr_cnt_r_ = 0; return; }                       // the tables have no reset
            if (!i_lut_wen) return;
            store(i_lut_wsel, wr_cnt_r_, i_lut_wdata);
            wr_cnt_r_ = wr_cnt_r_ == max_wr_cnt(i_lut_wsel) ? 0u : ((wr_cnt_r_ + 1u) & wr_cnt_mask());
        }
        void load8(LutType t, const std::vector<uint8_t> &v) { for (size_t i = 0; i < v.size() && i < 256; i++) (t == LUT_ACT ? act_ : exp_)[i] = v[i]; }
        void load16(LutType t, const std::vector<uint16_t> &v)
        {
            for (size_t i = 0; i < v.size(); i++) { if (t == LUT_RECIP) { if (i < 256) recip_[i] = v[i]; } else if (i < 512) rsqrt_[i] = v[i]; }
        }
        uint8_t peek8(LutType t, unsigned a) const { return (t == LUT_ACT ? act_ : exp_)[a & 0xFFu]; }
        uint16_t peek16(LutType t, unsigned a) const { return t == LUT_RECIP ? recip_[a & 0xFFu] : rsqrt_[a & 0x1FFu]; }
        // Read of lut[i] and lut[i + 1] (the address of the second held at 255 / 511); recip ignores address bit 8.
        uint8_t rdata8(LutType rsel, unsigned a) const { return (rsel == LUT_ACT ? act_ : exp_)[a & 0xFFu]; }
        uint16_t rdata16(LutType rsel, unsigned a) const { return rsel == LUT_RECIP ? recip_[a & 0xFFu] : rsqrt_[a & 0x1FFu]; }
        uint16_t rdata16_nxt(LutType rsel, unsigned a) const
        {
            const unsigned a8 = a & 0xFFu, a9 = a & 0x1FFu;
            return rsel == LUT_RECIP ? recip_[a8 < 255 ? a8 + 1 : a8] : rsqrt_[a9 < 511 ? a9 + 1 : a9];
        }

    private:
        LutParams P_;
        std::array<uint8_t, 256> act_, exp_;
        std::array<uint16_t, 256> recip_;
        std::array<uint16_t, 512> rsqrt_;
        unsigned wr_cnt_r_{0};
        // gen_act_exp_chunks / gen_recip_chunks / gen_rsqrt_chunks: chunk number = write counter; a counter past the last chunk writes nothing.
        // The chunk of the recip / rsqrt tables is deinterleaved: 32-bit word j of the beat gives the even entry (low half) and the odd entry (high half).
        void store(LutType wsel, unsigned cnt, const uint8_t *wd)
        {
            const unsigned w8 = static_cast<unsigned>(P_.INIT_DW / 8);
            if (wsel == LUT_ACT || wsel == LUT_EXP)
            {
                if (cnt >= act_exp_chunks()) return;
                for (unsigned i = 0; i < w8; i++) (wsel == LUT_ACT ? act_ : exp_)[cnt * w8 + i] = wd[i];
                return;
            }
            const unsigned bw = static_cast<unsigned>(P_.INIT_DW >= 32 ? P_.INIT_DW / 32 : 1);
            const unsigned chunks = (wsel == LUT_RECIP ? 128u : 256u) / bw;
            if (cnt >= chunks) return;
            for (unsigned j = 0; j < bw; j++)
            {
                const unsigned e = 2u * (cnt * bw + j);
                const uint16_t ev = static_cast<uint16_t>(wd[4 * j] | (wd[4 * j + 1] << 8)), od = static_cast<uint16_t>(wd[4 * j + 2] | (wd[4 * j + 3] << 8));
                if (wsel == LUT_RECIP) { recip_[e] = ev; recip_[e + 1] = od; } else { rsqrt_[e] = ev; rsqrt_[e + 1] = od; }
            }
        }
    };

    // ---- gvu_lut_lookup_lane.sv (revision 2): the lane with the request / response handshake to the table memory ---------
    struct LutMemData { uint8_t d8{0}; uint16_t d16{0}, d16n{0}; };      // o_8b_lut_rdata, o_16b_lut_rdata, o_16b_lut_rdata_nxt
    struct LutMemIf { bool req_ready{true}, rsp_valid{false}; LutMemData data; };   // what the table memory shows the lane in a cycle

    class LutLookupLaneR2
    {
    public:
        using In = LutLookupLane::In;
        struct Early { bool req_valid, rsp_ready; uint32_t raddr; };
        struct Out
        {
            bool o_lookup_ready, o_lookup_valid, o_pipe_almost_done;
            uint8_t o_8b_lookup_data;
            uint16_t o_16b_lookup_data;
            uint32_t o_e_shift, o_lut_raddr;
        };
        explicit LutLookupLaneR2(const LutParams &p = LutParams()) { p3_.bypass = !p.LERP_MUL_PIPE; p4_.bypass = !p.LERP_SHIFT_PIPE; }

        // The valid held in pipe3: o_pipe_almost_done in PWL mode with LERP_MUL_PIPE = 1 (what GvuTop reads without an In of the cycle).
        bool almost_done_pwl_state() const { return p3_.valid; }
        // The outputs that the table memory needs before it can answer ready: request valid, response ready, read address.
        Early early(const In &i, bool mem_rsp_valid) const { Wires w; LutMemIf m; m.rsp_valid = mem_rsp_valid; comb_front(i, m, w); return {w.req_valid, w.rsp_ready, w.raddr}; }
        Out eval(const In &i, const LutMemIf &m) const { Wires w; return comb(i, m, w); }
        void tick(const In &i, const LutMemIf &m, bool i_rstn = true)
        {
            Wires w;
            comb(i, m, w);
            p0_.tick(i_rstn, i.i_pipe_flush, w.v0_in, w.d0_in, w.r1);
            p1_.tick(i_rstn, i.i_pipe_flush, w.v1_in, w.d1_in, w.p1_ready_in);
            p2_.tick(i_rstn, i.i_pipe_flush, w.v2_in, w.d2_in, w.p2_ready_in);
            p3_.tick(i_rstn, i.i_pipe_flush, w.v3_in, w.d3_in, w.r4);
            p4_.tick(i_rstn, i.i_pipe_flush, w.v4_in, w.d4_in, i.i_lookup_ready);
            if (!i_rstn) { rd_ = wr_ = 0; count_ = 0; killed_.fill(false); return; }
            if (i.i_pipe_flush) killed_.fill(true);
            if (w.meta_push) { meta_[wr_] = i.i_lut_sel == LUT_ACT || i.i_lut_sel == LUT_EXP ? Meta{} : Meta{w.q1.in_zero, w.q1.E_shift, w.q1.pwl_frac}; killed_[wr_] = false; wr_ = (wr_ + 1u) & 3u; }
            if (w.meta_pop) rd_ = (rd_ + 1u) & 3u;
            if (w.meta_push && !w.meta_pop) count_++; else if (!w.meta_push && w.meta_pop) count_--;
        }

    private:
        struct Meta { bool in_zero{false}; uint32_t E_shift{0}, pwl_frac{0}; };
        PipelineReg<LutP0> p0_;
        PipelineReg<LutP1> p1_;
        PipelineReg<LutP2> p2_;
        PipelineReg<LutP3> p3_;
        PipelineReg<LutP4> p4_;
        std::array<Meta, 4> meta_{};
        std::array<bool, 4> killed_{};
        unsigned rd_{0}, wr_{0}, count_{0};
        struct Wires
        {
            bool direct, meta_valid, meta_space, rsp_active, req_valid, rsp_ready, meta_push, meta_pop;
            bool v0_in, v1_in, v2_in, v3_in, v4_in, v0_out, v1_out, v2_out, v3_out, v4_out, r1, r2, r3, r4, p1_ready_in, p2_ready_in;
            LutP0 d0_in; LutP1 d1_in; LutP2 d2_in; LutP3 d3_in; LutP4 d4_in; LutP1 q1; LutP2 q2; LutP3 q3; LutP4 q4;
            uint32_t raddr;
        };
        // Everything that does not depend on the ready of the table memory.
        void comb_front(const In &i, const LutMemIf &m, Wires &w) const
        {
            const bool flush = i.i_pipe_flush;
            w.direct = lut_is_direct(i.i_lut_sel);
            w.meta_valid = count_ != 0;
            w.meta_space = count_ < 4;
            const bool killed_rd = killed_[rd_];
            w.rsp_active = m.rsp_valid && w.meta_valid && !killed_rd && !flush;
            const Meta &mq = meta_[rd_];
            // pipe0 -> pipe1: index generation
            w.d0_in = lut_stage0(i.i_32b_lookup_idx);
            w.v0_in = i.i_lookup_valid && !w.direct && !flush;
            const LutP0 q0 = p0_.o_data(w.d0_in);
            w.d1_in = lut_stage1(q0, i.i_lut_sel, i.i_lut_idx_mode, i.i_base, i.i_W);
            w.v0_out = p0_.o_valid(w.v0_in);
            w.v1_in = w.v0_out && !flush;
            w.q1 = p1_.o_data(w.d1_in);
            w.v1_out = p1_.o_valid(w.v1_in);
            // request to the memory
            w.req_valid = !flush && w.meta_space && (w.direct ? i.i_lookup_valid : w.v1_out);
            const uint32_t pwl_addr = w.q1.lut_idx & 0x1FFu;
            w.raddr = w.direct ? (((i.i_8b_lookup_idx & 0xFFu) + (i.i_lut_compens ? 128u : 0u)) & 0xFFu) : pwl_addr;
            // memory response -> pipe2 (log-scale ends here), pipe3, pipe4
            const int32_t lerp_sub = static_cast<int32_t>(static_cast<int8_t>(static_cast<uint8_t>(m.data.d16n - m.data.d16)));
            w.d2_in = LutP2{mq.E_shift, m.data.d16, lerp_sub, mq.pwl_frac, mq.in_zero};
            w.v2_in = w.rsp_active && !w.direct;
            w.q2 = p2_.o_data(w.d2_in);
            w.v2_out = p2_.o_valid(w.v2_in);
            w.d3_in = lut_stage3(w.q2);
            w.v3_in = w.v2_out && i.i_lut_idx_mode == LUT_PWL && !flush;
            w.q3 = p3_.o_data(w.d3_in);
            w.v3_out = p3_.o_valid(w.v3_in);
            w.d4_in = lut_stage4(w.q3, i.i_lerp_shift);
            w.v4_in = w.v3_out && !flush;
            w.q4 = p4_.o_data(w.d4_in);
            w.v4_out = p4_.o_valid(w.v4_in);
            // ready chain from the consumer back to pipe2 and the response of the memory
            w.r4 = p4_.o_ready(w.v4_in, i.i_lookup_ready);
            w.r3 = p3_.o_ready(w.v3_in, w.r4);
            w.p2_ready_in = i.i_lut_idx_mode == LUT_LOG_SCALE ? i.i_lookup_ready : w.r3;
            w.r2 = p2_.o_ready(w.v2_in, w.p2_ready_in);
            w.rsp_ready = w.meta_valid && (flush || killed_rd || (w.direct ? i.i_lookup_ready : w.r2));
        }
        Out comb(const In &i, const LutMemIf &m, Wires &w) const
        {
            comb_front(i, m, w);
            const bool flush = i.i_pipe_flush;
            w.p1_ready_in = w.meta_space && m.req_ready && !flush;
            w.r1 = p1_.o_ready(w.v1_in, w.p1_ready_in);
            const bool r0 = p0_.o_ready(w.v0_in, w.r1);
            w.meta_push = w.req_valid && m.req_ready;
            w.meta_pop = m.rsp_valid && w.rsp_ready;
            Out o;
            o.o_lut_raddr = w.raddr;
            o.o_8b_lookup_data = m.data.d8;
            o.o_16b_lookup_data = i.i_lut_idx_mode == LUT_LOG_SCALE ? (w.q2.in_zero ? static_cast<uint16_t>(0) : w.q2.lut_16b)
                                                                     : (w.q4.in_zero ? static_cast<uint16_t>(0) : lut_result(w.q4));
            o.o_lookup_valid = !flush && (w.direct ? w.rsp_active : (i.i_lut_idx_mode == LUT_LOG_SCALE ? w.v2_out : w.v4_out));
            o.o_e_shift = w.direct ? 0u : ((i.i_lut_idx_mode == LUT_LOG_SCALE ? w.q2.E_shift : w.q4.E_shift) & 31u);
            o.o_pipe_almost_done = !flush && !w.direct && (i.i_lut_idx_mode == LUT_LOG_SCALE ? w.v2_in : w.v3_out);
            o.o_lookup_ready = !flush && (w.direct ? (w.meta_space && m.req_ready) : r0);
            return o;
        }
    };

    // ---- gvu_lut_unit.sv (revision 2): NUM_LANE lanes on one table memory with three read stages ----------------------------
    // The handshake of the memory is that of lane 0 (all lanes advance together). Memory read stages: valid bits v1 / v2 / ov, data per lane.
    template <int NUM_LANE>
    class LutUnitT
    {
    public:
        struct In
        {
            bool i_pipe_flush{false}, i_lookup_valid{false};
            bool i_lut_wen{false};
            LutType i_lut_wsel{LUT_ACT};
            std::array<uint8_t, 128> i_lut_wdata{};
            LutType i_lut_rsel{LUT_RECIP};
            LutIdxMode i_lut_idx_mode{LUT_PWL};
            bool i_lut_compens{false};
            unsigned i_base{0}, i_W{16}, i_lerp_shift{8};
            std::array<uint32_t, NUM_LANE> i_8b_lookup_idx{}, i_32b_lookup_idx{};
            bool i_lookup_ready{true};
        };
        struct Out
        {
            bool o_lookup_ready, o_lookup_valid, o_init_done, o_pipe_almost_done;
            std::array<uint8_t, NUM_LANE> o_8b_lookup_data;
            std::array<uint16_t, NUM_LANE> o_16b_lookup_data;
            std::array<uint32_t, NUM_LANE> o_e_shift;
        };
        explicit LutUnitT(const LutParams &p = LutParams()) : mem(p) { for (auto &l : lane_) l = LutLookupLaneR2(p); }
        LutMemR2 mem;
        // State read for callers without an In of the cycle: PWL only (the valid of pipe3 of lane 0); the log-scale almost-done needs the inputs of the cycle.
        bool o_pipe_almost_done(LutIdxMode m) const { return m == LUT_PWL ? lane_[0].almost_done_pwl_state() : false; }
        Out eval(const In &i) const
        {
            Out o{};
            Step s = step(i);
            o.o_init_done = mem.o_init_done(i.i_lut_wsel, i.i_lut_wen);
            for (int l = 0; l < NUM_LANE; l++)
            {
                const LutLookupLaneR2::Out lo = lane_[static_cast<size_t>(l)].eval(lane_in(i, l), s.mif[static_cast<size_t>(l)]);
                if (l == 0) { o.o_lookup_valid = lo.o_lookup_valid; o.o_lookup_ready = lo.o_lookup_ready; o.o_pipe_almost_done = lo.o_pipe_almost_done; }
                o.o_8b_lookup_data[static_cast<size_t>(l)] = lo.o_8b_lookup_data;
                o.o_16b_lookup_data[static_cast<size_t>(l)] = lo.o_16b_lookup_data;
                o.o_e_shift[static_cast<size_t>(l)] = lo.o_e_shift;
            }
            return o;
        }
        void tick(const In &i, bool i_rstn = true)
        {
            const Step s = step(i);
            for (int l = 0; l < NUM_LANE; l++) lane_[static_cast<size_t>(l)].tick(lane_in(i, l), s.mif[static_cast<size_t>(l)], i_rstn);
            // the read stages of the memory, from the output backwards (everything changes at the same edge)
            if (!i_rstn) { v1_ = v2_ = ov_ = false; }
            else
            {
                const bool take1 = s.req_valid0 && s.ready1, take2 = v1_ && s.ready2, take3 = v2_ && s.ready3;
                if (take3) out_ = s2_;
                if (take2) s2_ = s1_;
                if (take1)
                    for (int l = 0; l < NUM_LANE; l++)
                    {
                        const unsigned a = s.raddr[static_cast<size_t>(l)];
                        const bool rec = i.i_lut_rsel == LUT_RECIP;
                        const unsigned a16 = rec ? (a & 0xFFu) : (a & 0x1FFu);
                        LutMemData d;
                        d.d8 = mem.rdata8(i.i_lut_rsel, a & 0xFFu);
                        d.d16 = mem.rdata16(i.i_lut_rsel, a16);
                        d.d16n = mem.rdata16_nxt(i.i_lut_rsel, a16);
                        s1_[static_cast<size_t>(l)] = d;
                    }
                if (s.ready3) ov_ = v2_;
                if (s.ready2) v2_ = v1_;
                if (s.ready1) v1_ = s.req_valid0;
            }
            mem.tick(i_rstn, i.i_lut_wen, i.i_lut_wsel, i.i_lut_wdata.data());   // the tables are read before the edge
        }

    private:
        std::array<LutLookupLaneR2, NUM_LANE> lane_;
        bool v1_{false}, v2_{false}, ov_{false};
        std::array<LutMemData, NUM_LANE> s1_{}, s2_{}, out_{};
        struct Step
        {
            bool req_valid0, rsp_ready0, ready1, ready2, ready3;
            std::array<uint32_t, NUM_LANE> raddr;
            std::array<LutMemIf, NUM_LANE> mif;
        };
        Step step(const In &i) const
        {
            Step s{};
            const LutLookupLaneR2::Early e0 = lane_[0].early(lane_in(i, 0), ov_);
            s.req_valid0 = e0.req_valid; s.rsp_ready0 = e0.rsp_ready;
            s.ready3 = !ov_ || e0.rsp_ready;
            s.ready2 = !v2_ || s.ready3;
            s.ready1 = !v1_ || s.ready2;
            for (int l = 0; l < NUM_LANE; l++)
            {
                const size_t k = static_cast<size_t>(l);
                s.raddr[k] = lane_[k].early(lane_in(i, l), ov_).raddr;
                s.mif[k].req_ready = s.ready1; s.mif[k].rsp_valid = ov_; s.mif[k].data = out_[k];
            }
            return s;
        }
        static LutLookupLane::In lane_in(const In &i, int l)
        {
            LutLookupLane::In li;
            li.i_pipe_flush = i.i_pipe_flush; li.i_lookup_valid = i.i_lookup_valid; li.i_lut_sel = i.i_lut_rsel;
            li.i_lut_idx_mode = i.i_lut_idx_mode; li.i_lut_compens = i.i_lut_compens; li.i_base = i.i_base; li.i_W = i.i_W; li.i_lerp_shift = i.i_lerp_shift;
            li.i_8b_lookup_idx = i.i_8b_lookup_idx[static_cast<size_t>(l)]; li.i_32b_lookup_idx = i.i_32b_lookup_idx[static_cast<size_t>(l)];
            li.i_lookup_ready = i.i_lookup_ready;
            return li;
        }
    };
    using LutUnitR2 = LutUnitT<GVU_LANES>;
} // namespace rtl_ref_gvu

#endif // RTL_REF_GVU_LUT_R2_H
