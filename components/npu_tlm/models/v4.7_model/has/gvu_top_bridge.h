// gvu_top_bridge.h -- optional execution of the vector-unit work of the instruction path on GvuTop (has/rtl_ref_gvu_top.h),
// the port of the top level of the vector unit. Off unless the environment variable HAS_GVU_TOP is set to a non-zero
// value; with it off nothing computed by the model changes. When on it takes the place of HAS_OBP_RTL / HAS_ELEM_RTL.
//   HAS_GVU_TOP=1
//     GEMM_FUSED epilogue  every beat of the PSM -> SRAM-C write path (has::HasObp::apply_inline) goes through GvuTop in
//                          stream mode: the model first computes its own result, then the beat (32 psums, lane mask,
//                          word address, Out_Scale / Out_Shift of its output channel) is run through the stream pipeline
//                          (kDepth cycles) to completion inside the call, so the value lands in the cycle of the PSM write as in the model.
//                          The zero point is the 8-bit field of the GEMM_FUSED register, sign-extended into the 16-bit adder
//                          (zp_fits / zp_reg); a zero point outside int8 keeps the model result.
//                          The activation table of the layer is loaded through GvuTop's own path: the 256 bytes in rows
//                          0 and 1 of vbank1 buffer 1, then LUT_ACT_INIT (i_gemm_lut_reload). The tail behind the last
//                          write of a core pass is the stream depth (kDepth).
//     residual add         every chunk of has::HasElemwise::add runs on GvuTop in runs of at most 128 rows (the int8 rows
//                          of vbank1), the lanes past the tensor masked in the last row; the compute cycles of the chunk are
//                          GvuTop's, from the start of a run to its done output.
//     max pool             every channel group of has::HasElemwise::maxpool (direct mode) runs on GvuTop in POOLING_MAX (port
//                          of the latest hardware RTL drop): the tensor padded with -128 is written straight into vbank0 buffer 0 as
//                          rows of 32 bytes (the role of the DMA with npu_pad2d; who pads is question Q-MP1), in runs that
//                          fit vbank0 (800 rows) and vbank2 port 1 (1 024 rows), each run from a reset of the unit (bug
//                          B12: the pooling feeder keeps first_load_cnt between runs and a run that follows another without
//                          a reset is wrong); the result is read from vbank2 port 1. Compute cycles of the group: GvuTop's,
//                          from the start of each run to its done output. A (k, s) the RTL does not pool (it has 2x2 s1/s2,
//                          3x3 s1, 4x4 s1/s2/s4, 5x5 s1) stays on GvuMaxPoolPipe (has::RtlElem::maxpool), counted.
//   HAS_GVU_TOP=2          as 1, and in has/has_npu_top.h the activation table of a layer costs the cycles of that path (a
//                          DMA of 256 bytes into vbank1 plus LUT_ACT_INIT, table_cycles()) instead of 32 copies over the
//                          host port of the model's epilogue.
// The Scratchpad DMA port of GvuTop does nothing (as in the RTL), so the operands are written straight into its banks and
// the DMA cycles stay those of the model. Every result is compared with the model's and counted; values outside the
// containers of GvuTop keep the model result and are counted as "not expressible". One GvuTop serves all three operations
// (they run one after the other): before an add or a max pool the epilogue run is stopped, before a beat the controller is
// put back into GEMM_FUSED_ST1 (after the reset of a max pool run the activation table is loaded again). A table is
// printed when the program exits ("[gvu-top] ..." lines).
#ifndef HAS_GVU_TOP_BRIDGE_H
#define HAS_GVU_TOP_BRIDGE_H

#include <algorithm>
#include <cstdint>
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "has/gvu_quant.h"
#include "has/rtl_ref_gvu_top.h"

namespace has
{
    class GvuTopBridge
    {
    public:
        // stream depth of GEMM_FUSED in GvuTop: the one shared value rtl_ref_gvu::GEMM_STREAM_DEPTH (default 9, the register
        // cuts of the MAS; -DGVU_GEMM_STREAM_DEPTH=11 for the depth the hardware RTL measures)
        static constexpr int kDepth = rtl_ref_gvu::GEMM_STREAM_DEPTH;

        static bool on() { return inst().level_ > 0; }
        // HAS_GVU_TOP=3 (has/gvu_top_stream.h): residual add and max pool run on the unit of the clocked connection instead of
        // this class's own unit; the epilogue never comes here at level 3 (it travels the clocked stream).
        typedef std::function<bool(const int8_t *, const int8_t *, int8_t *, size_t, int, int, int, int64_t, int64_t, int64_t, int, int, int, int,
                                   bool, bool, uint64_t, uint64_t *)> AddFn;
        typedef std::function<bool(const int8_t *, int8_t *, int, int, int, int, int, int, uint64_t, uint64_t *)> MaxpoolFn;
        static void attach_elem(AddFn a, MaxpoolFn m) { inst().ext_add_ = std::move(a); inst().ext_maxpool_ = std::move(m); }
        // HAS_GVU_TOP_DUMP=<file>: one line per epilogue beat in the order of the beats, "B <channel> <mask> <32 hex bytes>"; the clocked
        // connection writes the same lines, so the two files can be compared byte for byte.
        static std::FILE *dump_file()
        {
            static std::FILE *f = nullptr;
            static bool tried = false;
            if (!tried) { tried = true; if (const char *p = std::getenv("HAS_GVU_TOP_DUMP")) f = std::fopen(p, "w"); }
            return f;
        }
        static int level() { return inst().level_; }
        // Cycles of loading one activation table through GvuTop: DMA of the 256 bytes (dma_cycles, from the caller's DMA
        // estimate) plus the controller from IDLE through LUT_ACT_INIT into GEMM_FUSED_ST1 (measured once on a spare unit).
        static uint64_t table_cycles(uint64_t dma_cycles) { return dma_cycles + uint64_t(inst().init_cycles_); }

        // Narrowing of the requant knob (has::Knobs::req_narrow) on GvuTop: SAT16 -> NARROW_SATURATE, WRAP16 -> NARROW_WRAP.
        // Returns false for a narrowing GvuTop does not have (NARROW_NONE).
        static bool map_narrow(int req_narrow, rtl_ref_gvu::NarrowMode *nm)
        {
            if (req_narrow == SAT16) { *nm = rtl_ref_gvu::NARROW_SATURATE; return true; }
            if (req_narrow == WRAP16) { *nm = rtl_ref_gvu::NARROW_WRAP; return true; }
            return false;
        }

        // Zero point of GEMM_FUSED: the RTL holds 8 bits (gvu_config_regs.sv, GEMM_FUSED register [15:8]) and sign-extends them
        // into the 16-bit adder (gvu_top.sv, adder2_b = {{8{z[7]}}, z}). zp_fits: the value has those 8 bits; zp_reg: what
        // the adder gets from the low byte of the value.
        static bool zp_fits(int64_t zp) { return fit(zp, 8); }
        static int32_t zp_reg(int64_t zp) { return static_cast<int8_t>(static_cast<uint8_t>(zp & 0xFF)); }

        // One beat of the GEMM_FUSED epilogue. out holds the model result on entry and GvuTop's result on return (masked
        // lanes). lut: the layer's table (256 entries, index = int8 value + 128); table_new: it changed since the last beat.
        // round_mode: has::RoundMode; req_narrow: has::ReqNarrow (the requant knob). Returns false (out untouched) when the
        // parameters do not fit GvuTop.
        template <class Vec>
        static bool gemm_beat_narrow(const Vec &in, Vec &out, uint32_t mask, uint32_t addr, int64_t S, int s, int zp, int round_mode,
                                     int req_narrow, bool act, const int8_t *lut, bool table_new)
        {
            namespace R = rtl_ref_gvu;
            GvuTopBridge &b = inst();
            Stat &st = b.st_[0];
            st.calls++;
            R::NarrowMode nm = R::NARROW_SATURATE;
            if (!map_narrow(req_narrow, &nm) || !fit(S, 32) || s < 0 || s > 63 || !zp_fits(zp) || round_mode < 0 || round_mode > 6) { st.not_expr++; return false; }
            if (table_new || !b.table_loaded_) b.load_table(lut);
            b.enter_gemm(map_round(round_mode), nm, zp, act);
            R::GvuTop::In &i = b.gin_;
            i.i_psum_valid = true;
            for (size_t l = 0; l < size_t(R::GVU_LANES); l++) i.i_psum_vec[l] = static_cast<int32_t>(in[l]);
            i.i_psum_scale = static_cast<int32_t>(S); i.i_psum_shift = static_cast<uint8_t>(s); i.i_psum_mask = mask; i.i_psum_addr = addr;
            b.top_.tick(i, true);
            i.i_psum_valid = false;
            R::GvuTop::Out o;
            int n = 0;
            do { o = b.top_.tick(i, true); n++; } while (!o.o_gemm_fused_valid && n < 4 * kDepth);
            if (!o.o_gemm_fused_valid) { st.no_out++; st.not_expr++; return false; }
            if (n != kDepth) st.lat_other++;
            if (o.o_gemm_fused_mask != mask || o.o_gemm_fused_addr != addr) st.meta_bad++;
            for (size_t l = 0; l < size_t(R::GVU_LANES); l++)
            {
                if (!((mask >> l) & 1u)) continue;
                const int h = o.o_gemm_fused_data[l], m = static_cast<int>(out[l]);
                st.lanes++;
                if (h != m) st.miss("x=%d S=%lld s=%d zp=%d act=%d model=%d GvuTop=%d", static_cast<int>(in[l]), static_cast<long long>(S), s, zp, int(act), m, h);
                out[l] = h;
            }
            st.cycles += uint64_t(n);
            if (std::FILE *df = dump_file())
            {
                std::fprintf(df, "B %u %08x", addr, mask);
                for (int l = 0; l < 32; l++) std::fprintf(df, " %02x", ((mask >> l) & 1u) ? unsigned(static_cast<uint8_t>(static_cast<int8_t>(out[size_t(l)]))) : 0u);
                std::fprintf(df, "\n");
            }
            return true;
        }
        // As gemm_beat_narrow with sat16 = (req_narrow == has::SAT16); any other narrowing is not expressible here.
        template <class Vec>
        static bool gemm_beat(const Vec &in, Vec &out, uint32_t mask, uint32_t addr, int64_t S, int s, int zp, int round_mode, bool sat16,
                              bool act, const int8_t *lut, bool table_new)
        {
            return gemm_beat_narrow(in, out, mask, addr, S, s, zp, round_mode, sat16 ? int(SAT16) : int(NARROW_NONE), act, lut, table_new);
        }

        // ---- HAS_GVU_AGU_ND (slice / concat on the GVU, model proposal, has/gvu_agu_nd.h): where the residual add and the max pool keep
        // their data in the Scratchpad. 0 / unset: as before (nothing bound). 1: AGU_ND bound with programs that give the same rows as
        // before. 2: shifted programs -- residual add: A at vbank0 rows 37 + 2r (source, stride 2), result at rows 400 + r (destination);
        // max pool: output rows permuted [C][OH][Wt] -> [OH][C][Wt] (destination). Loads and read-backs follow the programs, so the
        // values and the cycles of GvuTop are those of 0.
        static int agu_nd_level()
        {
            static const int v = [] { const char *e = std::getenv("HAS_GVU_AGU_ND"); return e ? std::atoi(e) : 1; }();   // default 1
            return v;
        }
        static unsigned agu_nd_a_row(size_t r) { return static_cast<unsigned>(agu_nd_level() == 2 ? 37 + 2 * r : r); }
        static unsigned agu_nd_out_row(size_t r) { return static_cast<unsigned>(agu_nd_level() == 2 ? 400 + r : r); }
        // residual add run of `rows` rows (at most 128: A fits vbank0 below row 400 in level 2)
        static void agu_nd_add(rtl_ref_gvu::GvuTop::In &i, size_t rows)
        {
            namespace R = rtl_ref_gvu;
            if (!agu_nd_level()) return;
            const bool sh = agu_nd_level() == 2;
            i.i_agu_nd_ctrl.src_port = R::AguNdCtrl::VBANK0; i.i_agu_nd_ctrl.src_on = true;
            i.i_agu_nd_ctrl.dst_port = R::AguNdCtrl::VBANK0; i.i_agu_nd_ctrl.dst_on = true;
            i.i_agu_src.base = sh ? 37u : 0u; i.i_agu_src.n = {{static_cast<unsigned>(rows), 1, 1, 1}}; i.i_agu_src.s = {{sh ? 2 : 1, 0, 0, 0}};
            i.i_agu_dst.base = sh ? 400u : 0u; i.i_agu_dst.n = {{static_cast<unsigned>(rows), 1, 1, 1}}; i.i_agu_dst.s = {{1, 0, 0, 0}};
        }
        // max pool run of c channels, OH output rows, Wt W tiles: c * OH * Wt output rows of vbank2 port 1
        static void agu_nd_pool(rtl_ref_gvu::GvuTop::In &i, unsigned wt, unsigned oh, unsigned c)
        {
            namespace R = rtl_ref_gvu;
            if (!agu_nd_level()) return;
            i.i_agu_nd_ctrl.dst_port = R::AguNdCtrl::VBANK2; i.i_agu_nd_ctrl.dst_on = true;
            i.i_agu_dst.base = 0;
            if (agu_nd_level() == 2) { i.i_agu_dst.n = {{wt, oh, c, 1}}; i.i_agu_dst.s = {{1, static_cast<int>(c * wt), static_cast<int>(wt), 0}}; }
            else { i.i_agu_dst.n = {{c * oh * wt, 1, 1, 1}}; i.i_agu_dst.s = {{1, 0, 0, 0}}; }
        }
        // vbank2 port 1 row (int8 rows) where output row `row` of the run lies
        static unsigned agu_nd_pool_row(const rtl_ref_gvu::GvuTop::In &i, unsigned row)
        {
            return i.i_agu_nd_ctrl.dst_on ? i.i_agu_dst.addr_at(row, 10) : row;
        }

        // Residual add of one chunk of n elements, out holds the model result on entry and GvuTop's on return. cycles: the
        // compute cycles of GvuTop for the chunk. Returns false (out untouched) when the parameters do not fit GvuTop.
        static bool add(const int8_t *A, const int8_t *B, int8_t *out, size_t n, int zpA, int zpB, int zpO, int64_t SA, int64_t SB, int64_t SO,
                        int sA, int sB, int sO, int round_mode, bool sub_before, bool sat16, uint64_t model_cycles, uint64_t *cycles)
        {
            namespace R = rtl_ref_gvu;
            GvuTopBridge &b = inst();
            if (b.ext_add_) return b.ext_add_(A, B, out, n, zpA, zpB, zpO, SA, SB, SO, sA, sB, sO, round_mode, sub_before, sat16, model_cycles, cycles);
            Stat &st = b.st_[1];
            st.calls++;
            if (!sub_before || !sat16 || !fit(SA, 32) || !fit(SB, 32) || !fit(SO, 32) || !fit(zpA, 8) || !fit(zpB, 8) || !fit(zpO, 8) ||
                sA < 0 || sA > 63 || sB < 0 || sB > 63 || sO < 0 || sO > 63 || round_mode < 0 || round_mode > 6)
            { st.not_expr++; return false; }
            b.leave_gemm();
            const size_t rows_total = (n + 31) / 32, per_run = 128;
            uint64_t cyc = 0;
            for (size_t r0 = 0; r0 < rows_total; r0 += per_run)
            {
                const size_t rows = std::min(per_run, rows_total - r0);
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
                    b.top_.scratchpad().sram(0, 0).poke(agu_nd_a_row(r), ra);
                }
                for (size_t k = 0; k < v1.size(); k++) b.top_.scratchpad().sram(1, 0).poke(static_cast<unsigned>(k), v1[k]);
                R::GvuTop::In i;
                i.i_gvu_op = R::GVU_OP_RESIDUAL; i.i_num_tile = 1; i.i_token_len = static_cast<uint32_t>(rows);
                i.i_res_scale_a = static_cast<int32_t>(SA); i.i_res_shift_a = static_cast<uint8_t>(sA); i.i_res_zp_a = static_cast<int8_t>(zpA);
                i.i_res_scale_b = static_cast<int32_t>(SB); i.i_res_shift_b = static_cast<uint8_t>(sB); i.i_res_zp_b = static_cast<int8_t>(zpB);
                i.i_res_scale_out = static_cast<int32_t>(SO); i.i_res_shift_out = static_cast<uint8_t>(sO); i.i_res_zp_out = static_cast<int8_t>(zpO);
                i.i_res_round = map_round(round_mode); i.i_res_out_int16 = false;
                const size_t tail = n - (r0 + rows - 1) * 32;
                i.i_res_last_lane_mask = (r0 + rows == rows_total && tail < 32) ? ((1u << tail) - 1u) : 0xFFFFFFFFu;
                agu_nd_add(i, rows);
                i.i_pipeline_en = true;
                bool done = false;
                uint64_t c = 0;
                for (; c < 4 * rows + 400 && !done; c++) done = b.top_.tick(i, true).o_residual_pipe_done;
                i.i_pipeline_en = false;
                for (int k = 0; k < 4 && b.top_.state() != R::IDLE; k++) b.top_.tick(i, true);
                if (!done || b.top_.state() != R::IDLE) { st.no_out++; st.not_expr++; b.reset(); return false; }
                cyc += c;
                st.runs++;
                for (size_t r = 0; r < rows; r++)
                {
                    const R::SpBytes orow = b.top_.scratchpad().sram(0, 0).word(agu_nd_out_row(r));
                    for (size_t l = 0; l < 32; l++)
                    {
                        const size_t e = (r0 + r) * 32 + l;
                        if (e >= n) break;
                        const int8_t h = static_cast<int8_t>(orow[l]);
                        st.lanes++;
                        if (h != out[e]) st.miss("element %zu a=%d b=%d model=%d GvuTop=%d", e, A[e], B[e], out[e], h);
                        out[e] = h;
                    }
                }
            }
            st.cycles += cyc; st.model_cycles += model_cycles;
            *cycles = cyc;
            return true;
        }
        // Max pool of a group of g channels [g][H][W], padding p with -128, k x k, stride s. out holds the model result
        // [g][Ho][Wo] on entry and GvuTop's on return. cycles: the GvuTop cycles of the runs of the group. Returns false (out
        // untouched) when GvuTop cannot pool this shape: a (k, s) the RTL does not have, a padded channel larger than vbank0,
        // fields wider than the 8-bit registers, or a run that does not finish.
        static bool maxpool(const int8_t *in, int8_t *out, int g, int H, int W, int kk, int s, int p, uint64_t model_cycles, uint64_t *cycles)
        {
            namespace R = rtl_ref_gvu;
            GvuTopBridge &b = inst();
            if (b.ext_maxpool_) return b.ext_maxpool_(in, out, g, H, W, kk, s, p, model_cycles, cycles);
            Stat &st = b.st_[2];
            st.calls++;
            const bool shape = (kk == 2 && (s == 1 || s == 2)) || (kk == 3 && s == 1) || (kk == 4 && (s == 1 || s == 2 || s == 4)) || (kk == 5 && s == 1);
            const int Hp = H + 2 * p, Wp = W + 2 * p, Wt = (Wp + 31) / 32;
            const int Ho = (Hp - kk) / s + 1, Wo = (Wp - kk) / s + 1;
            const int OL = s == 1 ? 32 : (kk == s ? (32 - kk) / s + 1 : (32 - kk) / s + 2);   // output lanes of one W tile
            const int cmax = std::min({800 / std::max(1, Hp * Wt), 1024 / std::max(1, Ho * Wt), 255});
            if (!shape || p < 0 || Ho < 1 || Wo < 1 || Ho > 255 || Wt > 255 || Wo > Wt * OL || cmax < 1) { st.not_expr++; return false; }
            b.leave_gemm();
            std::vector<int8_t> res(static_cast<size_t>(g) * Ho * Wo);
            uint64_t cyc = 0;
            for (int c0 = 0; c0 < g; c0 += cmax)
            {
                const int n = std::min(cmax, g - c0);
                b.reset();                                   // B12: a run must start from a reset of the pooling feeder
                for (int c = 0; c < n; c++)
                    for (int y = 0; y < Hp; y++)
                        for (int t = 0; t < Wt; t++)
                        {
                            R::SpBytes row(32, 0);
                            for (int l = 0; l < 32 && 32 * t + l < Wp; l++)
                            {
                                const int yy = y - p, xx = 32 * t + l - p;
                                row[static_cast<size_t>(l)] = (yy < 0 || yy >= H || xx < 0 || xx >= W)
                                    ? static_cast<uint8_t>(0x80) : static_cast<uint8_t>(in[(static_cast<size_t>(c0 + c) * H + yy) * W + xx]);
                            }
                            b.top_.scratchpad().sram(0, 0).poke(static_cast<unsigned>((c * Hp + y) * Wt + t), row);
                        }
                R::GvuTop::In i;
                i.i_gvu_op = R::GVU_OP_MAXPOOL;
                i.i_pooling_mode = kk - 2; i.i_pooling_stride = static_cast<unsigned>(s); i.i_pooling_num_w_tile = static_cast<unsigned>(Wt);
                i.i_pooling_c = static_cast<unsigned>(n); i.i_pooling_oh = static_cast<unsigned>(Ho);
                i.i_pooling_c_move = static_cast<unsigned>(Wt * Hp); i.i_pooling_h_move = static_cast<unsigned>(Wt * s);
                agu_nd_pool(i, static_cast<unsigned>(Wt), static_cast<unsigned>(Ho), static_cast<unsigned>(n));
                i.i_pipeline_en = true;
                bool done = false;
                uint64_t c = 0;
                const uint64_t guard = static_cast<uint64_t>(n) * Ho * Wt * 64 + 1000;
                for (; c < guard && !done; c++) done = b.top_.tick(i, true).o_maxpool_pipe_done;
                i.i_pipeline_en = false;
                for (int q = 0; q < 4 && b.top_.state() != R::IDLE; q++) b.top_.tick(i, true);
                if (!done || b.top_.state() != R::IDLE) { st.no_out++; st.not_expr++; b.reset(); return false; }
                cyc += c;
                st.runs++;
                for (int cc = 0; cc < n; cc++)
                    for (int oh = 0; oh < Ho; oh++)
                        for (int ow = 0; ow < Wo; ow++)
                        {
                            const unsigned row = agu_nd_pool_row(i, static_cast<unsigned>((cc * Ho + oh) * Wt + ow / OL));
                            const R::SpBytes w = b.top_.scratchpad().sram(2, 0).word(row >> 2);
                            res[(static_cast<size_t>(c0 + cc) * Ho + oh) * Wo + ow] = static_cast<int8_t>(w[(row & 3u) * 32u + static_cast<unsigned>(ow % OL)]);
                        }
            }
            b.reset();                                       // leave the unit as a fresh one for the next operation
            for (size_t e = 0; e < res.size(); e++)
            {
                st.lanes++;
                if (res[e] != out[e]) st.miss("element %zu of the group model=%d GvuTop=%d", e, out[e], res[e]);
                out[e] = res[e];
            }
            st.cycles += cyc; st.model_cycles += model_cycles;
            *cycles = cyc;
            return true;
        }
        // Max pool channels handed to GvuMaxPoolPipe instead (a shape GvuTop does not pool): counted for the report only.
        static void maxpool_elsewhere() { inst().mp_elsewhere_++; }

    private:
        struct Stat
        {
            uint64_t calls{0}, not_expr{0}, no_out{0}, lat_other{0}, meta_bad{0}, runs{0}, lanes{0}, differ{0}, cycles{0}, model_cycles{0};
            char first[160]{0};
            template <typename... A> void miss(const char *fmt, A... a) { if (!differ) std::snprintf(first, sizeof first, fmt, a...); differ++; }
        };
        int level_{0};
        int init_cycles_{0};
        bool table_loaded_{false};
        uint64_t tables_{0}, enters_{0}, stops_{0}, resets_{0}, mp_elsewhere_{0};
        rtl_ref_gvu::GvuTop top_;
        rtl_ref_gvu::GvuTop::In gin_;     // inputs held between the beats of an epilogue run
        AddFn ext_add_;
        MaxpoolFn ext_maxpool_;
        Stat st_[3];

        static GvuTopBridge &inst() { static GvuTopBridge b; return b; }
        GvuTopBridge()
        {
            const char *v = std::getenv("HAS_GVU_TOP");
            level_ = v ? std::atoi(v) : 3;   // default 3 (GvuTop on the clock of the core); HAS_GVU_TOP=0 switches it off
            if (level_ <= 0) { level_ = 0; return; }
            reset();
            // cycles of one table load on a spare unit: IDLE -> LUT_ACT_INIT -> GEMM_FUSED_ST1
            rtl_ref_gvu::GvuTop spare;
            rtl_ref_gvu::GvuTop::In i;
            for (int c = 0; c < 5; c++) spare.tick(i, false);
            spare.tick(i, true);
            i.i_gvu_op = rtl_ref_gvu::GVU_OP_GEMM_FUSED; i.i_gemm_stream = true; i.i_pipeline_en = true;
            while (spare.state() != rtl_ref_gvu::GEMM_FUSED_ST1 && init_cycles_ < 100) { spare.tick(i, true); init_cycles_++; }
            std::atexit(&GvuTopBridge::report);
        }
        void reset()
        {
            rtl_ref_gvu::GvuTop::In i;
            for (int c = 0; c < 5; c++) top_.tick(i, false);
            top_.tick(i, true);
            gin_ = rtl_ref_gvu::GvuTop::In();
            table_loaded_ = false;
            resets_++;
        }
        static bool fit(int64_t v, int bits) { const int64_t h = static_cast<int64_t>(1) << (bits - 1); return v >= -h && v < h; }
        static rtl_ref_gvu::RoundMode map_round(int m)
        {
            namespace R = rtl_ref_gvu;
            static const R::RoundMode t[7] = {R::RND_FLOOR, R::RND_NEAR_UP, R::RND_NEAR_AWAY, R::RND_NEAR_EVEN, R::RND_CEIL, R::RND_TRUNC, R::RND_AWAY};
            return t[m];
        }
        // Ends an epilogue run: stop, then IDLE once the pipeline is empty.
        void leave_gemm()
        {
            namespace R = rtl_ref_gvu;
            if (top_.state() == R::IDLE) return;
            gin_.i_psum_valid = false; gin_.i_gemm_stream_stop = true;
            for (int k = 0; k < 4 * kDepth && top_.state() != R::IDLE; k++) top_.tick(gin_, true);
            gin_.i_pipeline_en = false; gin_.i_gemm_stream_stop = false;
            top_.tick(gin_, true);
            stops_++;
            if (top_.state() != R::IDLE) reset();
        }
        // Puts the controller into GEMM_FUSED_ST1 with the held parameters of the layer (rounding, narrowing, zero point,
        // activation).
        void enter_gemm(rtl_ref_gvu::RoundMode rm, rtl_ref_gvu::NarrowMode nm, int zp, bool act)
        {
            namespace R = rtl_ref_gvu;
            gin_.i_gemm_round = rm; gin_.i_gemm_narrow = nm; gin_.i_gemm_zp16 = zp_reg(zp); gin_.i_gemm_skip_act = !act;
            if (top_.state() == R::GEMM_FUSED_ST1) return;
            leave_gemm();
            gin_.i_gvu_op = R::GVU_OP_GEMM_FUSED; gin_.i_gemm_stream = true; gin_.i_gemm_stream_stop = false; gin_.i_psum_valid = false;
            gin_.i_pipeline_en = true;
            for (int k = 0; k < 100 && top_.state() != R::GEMM_FUSED_ST1; k++)
            {
                top_.tick(gin_, true);
                if (top_.state() == R::LUT_ACT_INIT) gin_.i_gemm_lut_reload = false;
            }
            gin_.i_gemm_lut_reload = false;
            enters_++;
        }
        // A new table: the 256 bytes into rows 0 and 1 of vbank1 buffer 1, then a pass through LUT_ACT_INIT.
        void load_table(const int8_t *lut)
        {
            leave_gemm();
            for (unsigned w = 0; w < 2; w++)
            {
                rtl_ref_gvu::SpBytes row(128);
                for (unsigned k = 0; k < 128; k++) row[k] = static_cast<uint8_t>(lut[128 * w + k]);
                top_.scratchpad().sram(1, 1).poke(w, row);
            }
            gin_.i_vbank1_buf_sel = true;
            gin_.i_gemm_lut_reload = table_loaded_;   // the first load needs no reload: the controller has not loaded a table yet
            table_loaded_ = true;
            tables_++;
        }
        static void report()
        {
            const GvuTopBridge &b = inst();
            static const char *fn[3] = {"GEMM epilogue", "residual add", "max pool"};
            std::printf("[gvu-top] vector unit on GvuTop (HAS_GVU_TOP=%d), stream depth %d, table load %d cycles + DMA: operation | calls | "
                        "not expressible | lanes | differ from the model | GvuTop cycles | model cycles | first difference\n", b.level_, kDepth, b.init_cycles_);
            for (int i = 0; i < 3; i++)
            {
                const Stat &s = b.st_[i];
                if (i == 2 && b.mp_elsewhere_)
                    std::printf("[gvu-top] %-13s | channels %llu on GvuMaxPoolPipe (a shape GvuTop does not pool)\n", fn[i], static_cast<unsigned long long>(b.mp_elsewhere_));
                if (s.calls == 0) continue;
                std::printf("[gvu-top] %-13s | calls %10llu | not expressible %6llu | lanes %12llu | differ %8llu | GvuTop cycles %12llu | model cycles %12llu | %s\n",
                            fn[i], static_cast<unsigned long long>(s.calls), static_cast<unsigned long long>(s.not_expr), static_cast<unsigned long long>(s.lanes),
                            static_cast<unsigned long long>(s.differ), static_cast<unsigned long long>(s.cycles), static_cast<unsigned long long>(s.model_cycles),
                            s.differ ? s.first : (s.no_out ? "a beat or run did not finish" : "-"));
            }
            std::printf("[gvu-top] epilogue beats with another depth %llu, address / mask not carried %llu; tables loaded %llu, runs entered %llu, stopped %llu, "
                        "unit resets %llu\n", static_cast<unsigned long long>(b.st_[0].lat_other), static_cast<unsigned long long>(b.st_[0].meta_bad),
                        static_cast<unsigned long long>(b.tables_), static_cast<unsigned long long>(b.enters_), static_cast<unsigned long long>(b.stops_),
                        static_cast<unsigned long long>(b.resets_));
            std::fflush(stdout);
        }
    };
} // namespace has

#endif // HAS_GVU_TOP_BRIDGE_H
