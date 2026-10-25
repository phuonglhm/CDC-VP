// gvu_elemwise.h -- ELEM_WISE per the HW drawing "ELEM_WISE" (has/HAS_IFACE.md §6.2): Residual ADD (2 stages through the
// Scratchpad), MAX_POOL (comparator + accumulator register, "end of window") and AVG_POOL (int16 window sum, Avg_Scale,
// Avg_Shift; ELEM_WISE mode 5).
//
// Called from an SC_THREAD of the testbench; it consumes simulated cycles with wait(). Data flow per chunk:
//   DMA in -> Scratchpad -> pipeline stage(s) (1 vector of 32 per cycle + fixed latency) -> Scratchpad -> DMA out.
// Stage latencies are counted from the dashed pipeline cuts of the drawing (ESTIMATE until the MAS). DMA in/out cost is an
// ESTIMATE too (bytes / AXI_BYTES_PER_CYCLE + DMA_LATENCY): the drawings do not say who moves operands into the
// Scratchpad (question H9). Compute and DMA are serial here unless the banked scratchpad (Knobs::sp_banked) pipelines them.
#ifndef HAS_ELEMWISE_H
#define HAS_ELEMWISE_H

#include <systemc.h>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>
#include "has/gvu_quant.h"
#include "has/gvu_scratchpad.h"
#include "has/gvu_lsu.h"
#include "has/gvu_rtl_elem.h"
#include "has/gvu_top_bridge.h"

namespace has
{
    struct AddParams
    {
        int zpA{0}, zpB{0}, zpO{0};
        uint32_t SA{1}, SB{1}, SO{1}; // raw 32-bit scale words, decoded with Knobs::scale_fmt
        int sA{0}, sB{0}, sO{0};
    };

    // HAS_GVU_ELEM_CLK=1 (set by HasNpuTop with HAS_GVU_TOP=3): a residual add chunk or a max pool group that GvuTop takes runs as
    // a job on the clock of has::GvuTopStream instead of the call to GvuTopBridge::add / maxpool. fits_*: GvuTop takes the
    // parameters (if not, the call style runs as before and counts them); start_*: the job starts at the next clock edge (the
    // operands are copied, the result is written into out when the job is done); finish: waits in the calling thread until the
    // job is done and gives its GvuTop cycles.
    struct ElemClock
    {
        std::function<bool(int, int, int, int64_t, int64_t, int64_t, int, int, int, int, bool, bool)> fits_add;
        std::function<void(const int8_t *, const int8_t *, int8_t *, size_t, int, int, int, int64_t, int64_t, int64_t, int, int, int, int,
                           uint64_t)> start_add;
        std::function<bool(int, int, int, int, int, int)> fits_maxpool;
        std::function<void(const int8_t *, int8_t *, int, int, int, int, int, int, uint64_t)> start_maxpool;
        std::function<uint64_t()> finish;
        bool on() const { return static_cast<bool>(finish); }
    };

    struct ElemStats
    {
        uint64_t add_calls{0}, add_elems{0}, max_calls{0}, max_elems{0}, avg_calls{0}, avg_elems{0};
        uint64_t cyc_dma{0}, cyc_compute{0}, cyc_overlap_saved{0};
        QuantCounters qc_deq, qc_req;
    };

    class HasElemwise
    {
    public:
        static constexpr int W = 32;
        static constexpr int LAT_ADD1 = 6;  // read, pipeline reg, sub zp, dequant (mul, round-shift), add / write
        static constexpr int LAT_ADD2 = 5;  // read, pipeline reg, requant (mul, round-shift, zp/clamp), write
        static constexpr int LAT_MAX = 3;   // read, pipeline reg, comparator
        static constexpr int AXI_BYTES_PER_CYCLE = 16; // 128-bit AXI (HAS §8.1)
        static constexpr int DMA_LATENCY = 20;          // ESTIMATE per transfer; default of Knobs::elem_dma_lat (used)

        HasElemwise(const Knobs &k) : k_(k), sp_(k.sp_banked ? k.sp_v0 + k.sp_v1 + k.sp_v2 : k.scratch_bytes) {}

        HasScratchpad &scratch() { return sp_; }
        const ElemStats &stats() const { return st_; }
        // Scratchpad transfers per bank and address mode (load/store unit accounting, has/gvu_lsu.h).
        GvuLsu lsu;
        const Knobs &knobs() const { return k_; }
        // HAS_GVU_ELEM_CLK=1: residual add and max pool on the clock of GvuTopStream (empty = off)
        ElemClock clk;

        // ---- ADD: out[i] = Requant(sat32(Dequant(A[i]) + Dequant(B[i])), SO, sO, zpO)
        // Chunk = 4096 elements: A 4 KB + B 4 KB + Sum 16 KB = 24 KB (HAS_IFACE §6.2); scaled down if the Scratchpad is smaller.
        void add(const int8_t *A, const int8_t *B, int8_t *out, size_t n, const AddParams &p)
        {
            // banked (table 4): A in vbank#0, B in vbank#1 (port#2, int8), Sum in vbank#2 (int32), result back in vbank#0
            const uint32_t chunk = k_.sp_banked
                ? std::max<uint32_t>(W, std::min(std::min(k_.sp_v0, k_.sp_v1), k_.sp_v2 / 4) / W * W)
                : std::max<uint32_t>(W, std::min<uint32_t>(4096, sp_.capacity() / 6 / W * W));
            const uint32_t offA = 0, offB = k_.sp_banked ? k_.sp_v0 : chunk,
                           offS = k_.sp_banked ? k_.sp_v0 + k_.sp_v1 : 2 * chunk; // Sum region: 4 bytes per element
            sched_begin();
            const int64_t SA = decode_scale(p.SA, k_.scale_fmt), SB = decode_scale(p.SB, k_.scale_fmt),
                          SO = decode_scale(p.SO, k_.scale_fmt);
            st_.add_calls++;
            // stage 1: A (vbank#0) and B (vbank#1) as v32int8, sum to vbank#2 as v32int32; stage 2: sum back, result to
            // vbank#0; In/Out scales (int32) and shifts (int8) from the sbank
            lsu.load(LsuBank::SBANK, LsuMode::S32, 3);
            lsu.load(LsuBank::SBANK, LsuMode::S8, 3);
            lsu.load(LsuBank::VBANK0, LsuMode::V32I8, n);
            lsu.load(LsuBank::VBANK1, LsuMode::V32I8, n);
            lsu.store(LsuBank::VBANK2, LsuMode::V32I32, n);
            lsu.load(LsuBank::VBANK2, LsuMode::V32I32, n);
            lsu.store(LsuBank::VBANK0, LsuMode::V32I8, n);
            for (size_t base = 0; base < n; base += chunk)
            {
                const uint32_t m = static_cast<uint32_t>(std::min<size_t>(chunk, n - base));
                chunk_begin();
                sp_.write(offA, A + base, m);
                sp_.write(offB, B + base, m);
                dma(2 * m);
                // stage 1: dequant both operands, add, write Sum (int32)
                for (uint32_t i = 0; i < m; i++)
                {
                    const int a = sp_.get<int8_t>(offA + i), b = sp_.get<int8_t>(offB + i);
                    const int32_t da = dequant(a, p.zpA, SA, p.sA, k_, &st_.qc_deq);
                    const int32_t db = dequant(b, p.zpB, SB, p.sB, k_, &st_.qc_deq);
                    sp_.put<int32_t>(offS + 4 * i, add_sat32(da, db, &st_.qc_deq));
                }
                // stage 2: requant Sum -> int8, written over region A
                const RangeProbe::Scope probe_scope(RangeProbe::C_ADD);
                for (uint32_t i = 0; i < m; i++)
                    sp_.put<int8_t>(offA + i, requant(sp_.get<int32_t>(offS + 4 * i), SO, p.sO, p.zpO, k_, &st_.qc_req));
                uint64_t cyc1 = vectors(m) + uint64_t(k_.lat_add1), cyc2 = vectors(m) + uint64_t(k_.lat_add2);
                // HAS_GVU_ELEM_CLK: the compute of the chunk is a job on the clock of GvuTopStream, which writes GvuTop's values into
                // out when it is done (the model values go there first; the operands are copied first, out may be A)
                if (clk.on() && clk.fits_add(p.zpA, p.zpB, p.zpO, SA, SB, SO, p.sA, p.sB, p.sO, k_.round_mode, k_.deq_zp_order == SUB_BEFORE,
                                             k_.req_narrow == SAT16))
                {
                    std::vector<int8_t> a(A + base, A + base + m), b(B + base, B + base + m);
                    sp_.read(offA, out + base, m);
                    int8_t *o = out + base;
                    const AddParams q = p;
                    const int rm = k_.round_mode;
                    const uint64_t mc = cyc1 + cyc2;
                    compute_job([this, a, b, o, m, q, SA, SB, SO, rm, mc]
                                { clk.start_add(a.data(), b.data(), o, m, q.zpA, q.zpB, q.zpO, SA, SB, SO, q.sA, q.sB, q.sO, rm, mc); });
                    dma(m);
                    continue;
                }
                // HAS_GVU_TOP (GvuTop) or HAS_ELEM_RTL (GvuResAddPipe): values and compute cycles of the chunk from the cycle-level unit
                if (GvuTopBridge::on() || RtlElem::on())
                {
                    std::vector<int8_t> res(m);
                    for (uint32_t i = 0; i < m; i++) res[i] = sp_.get<int8_t>(offA + i);
                    uint64_t pc = 0;
                    if ((GvuTopBridge::on() ? &GvuTopBridge::add : &RtlElem::add)(A + base, B + base, res.data(), m, p.zpA, p.zpB, p.zpO, SA, SB, SO, p.sA, p.sB, p.sO, k_.round_mode,
                                     k_.deq_zp_order == SUB_BEFORE, k_.req_narrow == SAT16, cyc1 + cyc2, &pc))
                    {
                        for (uint32_t i = 0; i < m; i++) sp_.put<int8_t>(offA + i, res[i]);
                        cyc1 = pc - pc / 2; cyc2 = pc / 2;
                    }
                }
                compute(cyc1);
                compute(cyc2);
                sp_.read(offA, out + base, m);
                dma(m);
            }
            sched_end();
            st_.add_elems += n;
        }

        // ---- MAX_POOL k x k, stride s, pad p (pad value -128 = -inf for INT8) on a [C, H, Wd] tensor.
        // Channels are processed in groups whose padded input (+ row-max intermediate for SEPARABLE) fits the Scratchpad.
        // Lanes = 32 consecutive output positions of one row; DIRECT reads k*k vectors per output vector, SEPARABLE reads k
        // (row pass, result kept in Scratchpad) + k (column pass). Both must give identical results (question H6).
        void maxpool(const int8_t *in, int8_t *out, int C, int H, int Wd, int kk, int s, int p)
        {
            const int Hp = H + 2 * p, Wp = Wd + 2 * p;
            const int Ho = (Hp - kk) / s + 1, Wo = (Wp - kk) / s + 1;
            const uint32_t per_ch = static_cast<uint32_t>(Hp * Wp) + (k_.mp_mode == MP_SEPARABLE ? Hp * Wo : 0);
            // banked (table 4): padded input in vbank#0, row-max intermediate in vbank#2, result in vbank#1 (port#2)
            int group = static_cast<int>(sp_.capacity() / per_ch);
            if (k_.sp_banked)
            {
                group = static_cast<int>(k_.sp_v0 / static_cast<uint32_t>(Hp * Wp));
                if (k_.mp_mode == MP_SEPARABLE) group = std::min(group, static_cast<int>(k_.sp_v2 / static_cast<uint32_t>(Hp * Wo)));
                group = std::min(group, static_cast<int>(k_.sp_v1 / static_cast<uint32_t>(Ho * Wo)));
            }
            if (group < 1)
                throw std::runtime_error("HasElemwise::maxpool: one padded channel does not fit the Scratchpad");
            const int vec_per_row = (Wo + W - 1) / W;
            st_.max_calls++;
            // window reads of v32int8 rows from vbank#0, results to vbank#1 (port#2)
            lsu.load(LsuBank::VBANK0, LsuMode::V32I8, static_cast<uint64_t>(C) * Ho * vec_per_row * W * kk * kk);
            lsu.store(LsuBank::VBANK1, LsuMode::V32I8, static_cast<uint64_t>(C) * Ho * Wo);
            std::vector<int8_t> padded(static_cast<size_t>(Hp) * Wp);
            sched_begin();
            for (int c0 = 0; c0 < C; c0 += group)
            {
                const int g = std::min(group, C - c0);
                chunk_begin();
                for (int c = 0; c < g; c++)
                {
                    std::fill(padded.begin(), padded.end(), static_cast<int8_t>(-128));
                    for (int y = 0; y < H; y++)
                        std::copy(in + (static_cast<size_t>(c0 + c) * H + y) * Wd, in + (static_cast<size_t>(c0 + c) * H + y + 1) * Wd,
                                  padded.begin() + static_cast<size_t>(y + p) * Wp + p);
                    sp_.write(k_.sp_banked ? static_cast<uint32_t>(c * Hp * Wp) : static_cast<uint32_t>(c) * per_ch, padded.data(),
                              static_cast<uint32_t>(padded.size()));
                }
                dma(static_cast<uint64_t>(g) * H * Wd);
                uint64_t group_model_cycles = 0;   // HAS_GVU_TOP: compute cycles of the model for the group (for the report)
                for (int c = 0; c < g; c++)
                {
                    const uint32_t base = k_.sp_banked ? static_cast<uint32_t>(c * Hp * Wp) : static_cast<uint32_t>(c) * per_ch;
                    const uint32_t rbase = k_.sp_banked ? k_.sp_v0 + k_.sp_v1 : base + Hp * Wp;
                    if (k_.mp_mode == MP_SEPARABLE)
                    {
                        for (int y = 0; y < Hp; y++) // row pass: 1 x k
                            for (int x = 0; x < Wo; x++)
                            {
                                int8_t mx = -128;
                                for (int kx = 0; kx < kk; kx++) mx = std::max(mx, sp_.get<int8_t>(base + y * Wp + x * s + kx));
                                sp_.put<int8_t>(rbase + y * Wo + x, mx);
                            }
                        for (int y = 0; y < Ho; y++) // column pass: k x 1
                            for (int x = 0; x < Wo; x++)
                            {
                                int8_t mx = -128;
                                for (int ky = 0; ky < kk; ky++) mx = std::max(mx, sp_.get<int8_t>(rbase + (y * s + ky) * Wo + x));
                                out[(static_cast<size_t>(c0 + c) * Ho + y) * Wo + x] = mx;
                            }
                        if (RtlElem::on() || GvuTopBridge::on()) RtlElem::maxpool_not_expressible();
                        compute(static_cast<uint64_t>(Hp) * vec_per_row * kk + static_cast<uint64_t>(Ho) * vec_per_row * kk + 2 * uint64_t(k_.lat_max));
                    }
                    else
                    {
                        for (int y = 0; y < Ho; y++)
                            for (int x = 0; x < Wo; x++)
                            {
                                int8_t mx = -128;
                                for (int ky = 0; ky < kk; ky++)
                                    for (int kx = 0; kx < kk; kx++)
                                        mx = std::max(mx, sp_.get<int8_t>(base + (y * s + ky) * Wp + x * s + kx));
                                out[(static_cast<size_t>(c0 + c) * Ho + y) * Wo + x] = mx;
                            }
                        uint64_t cyc = static_cast<uint64_t>(Ho) * vec_per_row * kk * kk + uint64_t(k_.lat_max), pc = 0;
                        // HAS_GVU_TOP: the whole group runs on GvuTop below, after the model values of every channel
                        if (GvuTopBridge::on()) { group_model_cycles += cyc; continue; }
                        // HAS_ELEM_RTL: values and compute cycles of the channel from GvuMaxPoolPipe
                        if (RtlElem::on() && RtlElem::maxpool(in + static_cast<size_t>(c0 + c) * H * Wd, out + static_cast<size_t>(c0 + c) * Ho * Wo,
                                                              H, Wd, kk, s, p, cyc, &pc))
                            cyc = pc;
                        compute(cyc);
                    }
                }
                // HAS_GVU_TOP, direct max pool: the group on GvuTop (POOLING_MAX of the latest hardware RTL drop), its values and compute
                // cycles; a (k, s) that GvuTop does not pool goes per channel to GvuMaxPoolPipe as before
                if (GvuTopBridge::on() && k_.mp_mode != MP_SEPARABLE)
                {
                    uint64_t pc = 0;
                    // HAS_GVU_ELEM_CLK: the group is a job on the clock of GvuTopStream (the input of the group copied first)
                    if (clk.on() && clk.fits_maxpool(g, H, Wd, kk, s, p))
                    {
                        std::vector<int8_t> grp(in + static_cast<size_t>(c0) * H * Wd, in + static_cast<size_t>(c0 + g) * H * Wd);
                        int8_t *o = out + static_cast<size_t>(c0) * Ho * Wo;
                        const uint64_t mc = group_model_cycles;
                        compute_job([this, grp, o, g, H, Wd, kk, s, p, mc] { clk.start_maxpool(grp.data(), o, g, H, Wd, kk, s, p, mc); });
                    }
                    else if (GvuTopBridge::maxpool(in + static_cast<size_t>(c0) * H * Wd, out + static_cast<size_t>(c0) * Ho * Wo, g, H, Wd, kk, s, p,
                                              group_model_cycles, &pc))
                        compute(pc);
                    else
                        for (int c = 0; c < g; c++)
                        {
                            uint64_t cyc = static_cast<uint64_t>(Ho) * vec_per_row * kk * kk + uint64_t(k_.lat_max), pc1 = 0;
                            GvuTopBridge::maxpool_elsewhere();
                            if (RtlElem::maxpool(in + static_cast<size_t>(c0 + c) * H * Wd, out + static_cast<size_t>(c0 + c) * Ho * Wo,
                                                 H, Wd, kk, s, p, cyc, &pc1))
                                cyc = pc1;
                            compute(cyc);
                        }
                }
                dma(static_cast<uint64_t>(g) * Ho * Wo);
            }
            sched_end();
            st_.max_elems += static_cast<uint64_t>(C) * Ho * Wo;
        }

        // ---- AVG_POOL k x k, stride s, no padding (drawing "Avg_Pool"): v32int16 accumulate over the window, then
        // Mul int32 x int16 by Avg_Scale -> v32int64 -> Round-Shift by Avg_Shift. The drawing shows no clamp / zero point after
        // the shift; this model saturates to int8 and COUNTS it (clamp8), and counts int16 accumulator overflow (acc16_ovf,
        // question H5: windows > 258 elements of |x| = 127 can overflow). ACC16 wraps like a 16-bit register would.
        uint64_t acc16_ovf{0};
        void avgpool(const int8_t *in, int8_t *out, int C, int H, int Wd, int kk, int s, uint32_t avg_scale_raw, int avg_shift)
        {
            const int Ho = (H - kk) / s + 1, Wo = (Wd - kk) / s + 1;
            const int64_t S = decode_scale(avg_scale_raw, k_.scale_fmt);
            const uint32_t per_ch = static_cast<uint32_t>(H * Wd);
            st_.avg_calls++;
            st_.avg_elems += static_cast<uint64_t>(C) * Ho * Wo;
            // window reads of v32int8 rows from vbank#0, results to vbank#1 (port#2); Avg_Scale / Avg_Shift from the decoder
            lsu.load(LsuBank::VBANK0, LsuMode::V32I8, static_cast<uint64_t>(C) * Ho * ((Wo + W - 1) / W) * W * kk * kk);
            lsu.store(LsuBank::VBANK1, LsuMode::V32I8, static_cast<uint64_t>(C) * Ho * Wo);
            const int group = static_cast<int>(sp_.capacity() / per_ch);
            if (group < 1)
                throw std::runtime_error("HasElemwise::avgpool: one channel does not fit the Scratchpad");
            const int vec_per_row = (Wo + W - 1) / W;
            for (int c0 = 0; c0 < C; c0 += group)
            {
                const int g = std::min(group, C - c0);
                sp_.write(0, in + static_cast<size_t>(c0) * per_ch, static_cast<uint32_t>(g) * per_ch);
                dma(static_cast<uint64_t>(g) * per_ch);
                for (int c = 0; c < g; c++)
                    for (int y = 0; y < Ho; y++)
                        for (int x = 0; x < Wo; x++)
                        {
                            int32_t wide = 0;
                            int16_t acc = 0;
                            for (int ky = 0; ky < kk; ky++)
                                for (int kx = 0; kx < kk; kx++)
                                {
                                    const int v = sp_.get<int8_t>(static_cast<uint32_t>(c) * per_ch + (y * s + ky) * Wd + x * s + kx);
                                    wide += v;
                                    acc = static_cast<int16_t>(static_cast<uint16_t>(acc + v));
                                }
                            if (wide != acc) acc16_ovf++;
                            if (RangeProbe::on())
                            {
                                RangeProbe::pt(RangeProbe::AVG_ACC, wide); RangeProbe::pt(RangeProbe::AVG_S, S);
                                RangeProbe::pt(RangeProbe::AVG_PROD, static_cast<i128>(acc) * S); RangeProbe::pt(RangeProbe::AVG_SHIFT, avg_shift);
                            }
                            const i128 r = rshift(static_cast<i128>(acc) * S, avg_shift, k_.round_mode);
                            const i128 o = clamp128(r, -128, 127);
                            if (o != r) st_.qc_req.clamp8++;
                            out[(static_cast<size_t>(c0 + c) * Ho + y) * Wo + x] = static_cast<int8_t>(o);
                        }
                compute(static_cast<uint64_t>(g) * Ho * vec_per_row * kk * kk + uint64_t(k_.lat_add2));
                dma(static_cast<uint64_t>(g) * Ho * Wo);
            }
        }

    private:
        Knobs k_;
        HasScratchpad sp_;
        ElemStats st_;

        static uint64_t vectors(uint64_t elems) { return (elems + W - 1) / W; }
        // Serial (sp_banked = 0): every DMA / compute step waits in turn. Banked: steps are collected per chunk
        // (DMA before the first compute = input, after = output) and scheduled ping-pong at sched_end():
        //   in(0), then per chunk i max(compute(i), in(i+1) + out(i-1)), then out(last). One AXI port: in + out add up.
        // HAS_GVU_ELEM_CLK: the compute of a chunk can be a job on the clock (job starts it; ElemClock::finish waits for it). The same
        // ping-pong then runs along the clock: chunk i computes while the input of chunk i+1 and the output of chunk i-1 move, and the
        // step takes the longer of the two, as the formula.
        struct Chunk { uint64_t in{0}, comp{0}, out{0}; std::function<void()> job; };
        std::vector<Chunk> chunks_;
        bool sched_{false}, in_phase_{true};
        void sched_begin() { chunks_.clear(); sched_ = k_.sp_banked != 0; }
        void chunk_begin() { if (sched_) { chunks_.push_back(Chunk()); in_phase_ = true; } }
        void sched_end()
        {
            if (!sched_) return;
            sched_ = false;
            if (chunks_.empty()) return;
            bool live = false;
            for (const Chunk &c : chunks_) if (c.job) live = true;
            if (live)
            {
                wait_cycles(chunks_[0].in);
                for (size_t i = 0; i < chunks_.size(); i++)
                {
                    const uint64_t side = (i + 1 < chunks_.size() ? chunks_[i + 1].in : 0) + (i > 0 ? chunks_[i - 1].out : 0);
                    uint64_t comp = chunks_[i].comp;
                    if (chunks_[i].job)
                    {
                        chunks_[i].job();
                        wait_cycles(side);
                        const uint64_t c = clk.finish();
                        st_.cyc_compute += c;
                        comp += c;
                    }
                    else wait_cycles(std::max(comp, side));
                    st_.cyc_overlap_saved += std::min(comp, side);
                }
                wait_cycles(chunks_.back().out);
                chunks_.clear();
                return;
            }
            uint64_t t = chunks_[0].in;
            for (size_t i = 0; i < chunks_.size(); i++)
            {
                const uint64_t side = (i + 1 < chunks_.size() ? chunks_[i + 1].in : 0) + (i > 0 ? chunks_[i - 1].out : 0);
                t += std::max(chunks_[i].comp, side);
                st_.cyc_overlap_saved += std::min(chunks_[i].comp, side);
            }
            t += chunks_.back().out;
            wait_cycles(t);
        }
        void compute(uint64_t cyc)
        {
            st_.cyc_compute += cyc;
            if (sched_ && !chunks_.empty()) { chunks_.back().comp += cyc; in_phase_ = false; return; }
            wait_cycles(cyc);
        }
        // HAS_GVU_ELEM_CLK: the compute of the current chunk as a job on the clock; serial: now, banked: at its place in sched_end.
        void compute_job(std::function<void()> start)
        {
            if (sched_ && !chunks_.empty()) { chunks_.back().job = std::move(start); in_phase_ = false; return; }
            start();
            st_.cyc_compute += clk.finish();
        }
        void dma(uint64_t bytes)
        {
            const uint64_t cyc = uint64_t(k_.elem_dma_lat) + (bytes + AXI_BYTES_PER_CYCLE - 1) / AXI_BYTES_PER_CYCLE;
            st_.cyc_dma += cyc;
            if (sched_ && !chunks_.empty()) { (in_phase_ ? chunks_.back().in : chunks_.back().out) += cyc; return; }
            wait_cycles(cyc);
        }
        static void wait_cycles(uint64_t n)
        {
            if (sc_core::sc_get_status() == sc_core::SC_RUNNING && n) sc_core::wait(static_cast<int>(n));
        }
    };
} // namespace has

#endif // HAS_ELEMWISE_H
