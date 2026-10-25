// gvu_rtl_elem.h -- optional execution of ELEM_WISE residual add and max pool on the cycle-level pipelines of
// has/rtl_ref_gvu_elemwise.h (ported load / store unit and Scratchpad around the units ported from the RTL). Off unless
// the environment variable HAS_ELEM_RTL is set to a non-zero value; with it off nothing computed by the model changes.
// When on, has::HasElemwise (has/gvu_elemwise.h) first computes its own result, then calls the functions below:
//   values   the result handed to the instruction is the one the pipeline wrote to the Scratchpad; every element is
//            compared with the model result and counted as "same" or "differs";
//   cycles   the compute cycles of the chunk are the clock cycles the pipeline needed from start to done instead of
//            the formula of the model (vectors + fixed latency).
// What stays as in the model: the split of a tensor into chunks, the DMA cycles of a chunk (bytes of the tensor, not
// of the window-major layout the max pool pipeline reads, see choice C1 of has/rtl_ref_gvu_elemwise.h) and the
// ping-pong schedule between DMA and compute.
// One chunk of the model is run as one or more bursts that fit the bank depths of the RTL: residual add 128 vectors
// (vbank#1 holds 128 int8 rows), max pool floor(800 / window) output vectors (vbank#0 holds 800 rows).
// Not run on the pipelines (the model result and the model cycles are kept, counted as "not expressible"): parameters
// outside the containers of the units, the separable max pool, average pooling.
// A table is printed when the program exits ("[elem-rtl] ..." lines).
#ifndef HAS_RTL_ELEM_H
#define HAS_RTL_ELEM_H

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "has/rtl_ref_gvu_elemwise.h"

namespace has
{
    class RtlElem
    {
    public:
        static bool on() { return inst().on_; }

        // Residual add of one chunk. out holds the model result on entry and the pipeline result on return.
        // round_mode: has::RoundMode. Returns false (out untouched) when the parameters do not fit the units.
        static bool add(const int8_t *A, const int8_t *B, int8_t *out, size_t n, int zpA, int zpB, int zpO, int64_t SA, int64_t SB, int64_t SO,
                        int sA, int sB, int sO, int round_mode, bool sub_before, bool sat16, uint64_t model_cycles, uint64_t *cycles)
        {
            namespace R = rtl_ref_gvu;
            Stat &st = inst().st_[0];
            st.chunks++;
            if (!sub_before || !sat16 || !fit(SA, 32) || !fit(SB, 32) || !fit(SO, 32) || !fit(zpA, 16) || !fit(zpB, 16) || !fit(zpO, 16) ||
                sA < 0 || sA > 63 || sB < 0 || sB > 63 || sO < 0 || sO > 63 || round_mode < 0 || round_mode > 6)
            { st.not_expr++; return false; }
            R::GvuResAddPipe::In cfg;
            cfg.i_zp_a = zpA; cfg.i_zp_b = zpB; cfg.i_zp_out = zpO;
            cfg.i_scale_a = static_cast<int32_t>(SA); cfg.i_scale_b = static_cast<int32_t>(SB); cfg.i_scale_out = static_cast<int32_t>(SO);
            cfg.i_shift_a = static_cast<unsigned>(sA); cfg.i_shift_b = static_cast<unsigned>(sB); cfg.i_shift_out = static_cast<unsigned>(sO);
            cfg.i_round = map_round(round_mode);
            const size_t rows_total = (n + 31) / 32, per_burst = 128;
            uint64_t cyc = 0;
            for (size_t r0 = 0; r0 < rows_total; r0 += per_burst)
            {
                const size_t rows = std::min(per_burst, rows_total - r0);
                R::ElemwiseBench b;
                R::GvuResAddPipe dut;
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
                    b.poke_v0(static_cast<unsigned>(r), ra);
                }
                for (size_t k = 0; k < v1.size(); k++) b.poke_v1(static_cast<unsigned>(k), v1[k]);
                cfg.i_rows = static_cast<unsigned>(rows); cfg.i_a_row = 0; cfg.i_b_row = 0; cfg.i_sum_row = 0;
                const size_t tail = n - (r0 + rows - 1) * 32;
                cfg.i_last_lane_mask = (r0 + rows == rows_total && tail < 32) ? ((1u << tail) - 1u) : 0xFFFFFFFFu;
                b.step(dut, cfg);                                        // configuration stable for one cycle
                cfg.i_start = true; b.step(dut, cfg); cfg.i_start = false;
                const uint64_t c0 = b.cycles;
                bool done = false;
                for (uint64_t guard = 0; guard < 2 * rows + 200 && !done; guard++) done = b.step(dut, cfg).o_done;
                if (!done) { st.no_done++; st.not_expr++; return false; }
                cyc += b.cycles - c0;
                st.bursts++;
                for (size_t r = 0; r < rows; r++)
                {
                    const R::SpBytes orow = b.peek_v0(static_cast<unsigned>(r));
                    for (size_t l = 0; l < 32; l++)
                    {
                        const size_t e = (r0 + r) * 32 + l;
                        if (e >= n) break;
                        const int8_t h = static_cast<int8_t>(orow[l]);
                        if (h != out[e]) st.miss("element %zu a=%d b=%d model=%d pipeline=%d", e, A[e], B[e], out[e], h);
                        out[e] = h;
                    }
                }
            }
            st.elems += n; st.cycles += cyc; st.model_cycles += model_cycles;
            *cycles = cyc;
            return true;
        }

        // Max pool of one channel [H, W] -> [Ho, Wo], padding value -128. out holds the model result on entry and the
        // pipeline result on return.
        static bool maxpool(const int8_t *in, int8_t *out, int H, int W, int kk, int s, int p, uint64_t model_cycles, uint64_t *cycles)
        {
            namespace R = rtl_ref_gvu;
            Stat &st = inst().st_[1];
            st.chunks++;
            const int Hp = H + 2 * p, Wp = W + 2 * p, Ho = (Hp - kk) / s + 1, Wo = (Wp - kk) / s + 1, vpr = (Wo + 31) / 32, win = kk * kk;
            const size_t per_burst = std::min<size_t>(800 / static_cast<size_t>(win), 128);
            if (per_burst == 0) { st.not_expr++; return false; }
            auto at = [&](int y, int x) -> int8_t {
                y -= p; x -= p;
                return (y < 0 || y >= H || x < 0 || x >= W) ? static_cast<int8_t>(-128) : in[static_cast<size_t>(y) * W + static_cast<size_t>(x)];
            };
            struct OutVec { int y, x0; };
            std::vector<OutVec> ov;
            for (int y = 0; y < Ho; y++) for (int v = 0; v < vpr; v++) ov.push_back({y, 32 * v});
            uint64_t cyc = 0;
            for (size_t base = 0; base < ov.size(); base += per_burst)
            {
                const size_t n = std::min(per_burst, ov.size() - base);
                R::ElemwiseBench b;
                R::GvuMaxPoolPipe dut;
                for (size_t r = 0; r < n; r++)                           // loader: window-major rows (choice C1)
                    for (int ky = 0; ky < kk; ky++)
                        for (int kx = 0; kx < kk; kx++)
                        {
                            R::SpBytes row(32, static_cast<uint8_t>(0x80));
                            for (int l = 0; l < 32 && ov[base + r].x0 + l < Wo; l++)
                                row[static_cast<size_t>(l)] = static_cast<uint8_t>(at(ov[base + r].y * s + ky, (ov[base + r].x0 + l) * s + kx));
                            b.poke_v0(static_cast<unsigned>(r * static_cast<size_t>(win) + static_cast<size_t>(ky * kk + kx)), row);
                        }
                R::GvuMaxPoolPipe::In cfg;
                cfg.i_out_rows = static_cast<unsigned>(n); cfg.i_win = static_cast<unsigned>(win); cfg.i_src_row = 0; cfg.i_dst_row = 0;
                b.step(dut, cfg);
                cfg.i_start = true; b.step(dut, cfg); cfg.i_start = false;
                const uint64_t c0 = b.cycles;
                bool done = false;
                for (uint64_t guard = 0; guard < n * static_cast<uint64_t>(win) + 100 && !done; guard++) done = b.step(dut, cfg).o_done;
                if (!done) { st.no_done++; st.not_expr++; return false; }
                cyc += b.cycles - c0;
                st.bursts++;
                for (size_t r = 0; r < n; r++)
                {
                    const R::SpBytes row = b.peek_v1(static_cast<unsigned>(r / 4));
                    const OutVec &o = ov[base + r];
                    for (int l = 0; l < 32 && o.x0 + l < Wo; l++)
                    {
                        const int8_t h = static_cast<int8_t>(row[32 * (r % 4) + static_cast<size_t>(l)]);
                        int8_t &m = out[static_cast<size_t>(o.y) * Wo + static_cast<size_t>(o.x0 + l)];
                        if (h != m) st.miss("y=%d x=%d model=%d pipeline=%d", o.y, o.x0 + l, m, h);
                        m = h;
                    }
                }
            }
            st.elems += static_cast<uint64_t>(Ho) * Wo; st.cycles += cyc; st.model_cycles += model_cycles;
            *cycles = cyc;
            return true;
        }
        // A chunk the pipelines do not run (separable max pool).
        static void maxpool_not_expressible() { Stat &st = inst().st_[1]; st.chunks++; st.not_expr++; }

    private:
        struct Stat
        {
            uint64_t chunks{0}, not_expr{0}, no_done{0}, bursts{0}, elems{0}, differ{0}, cycles{0}, model_cycles{0};
            char first[160]{0};
            template <typename... A> void miss(const char *fmt, A... a) { if (!differ) std::snprintf(first, sizeof first, fmt, a...); differ++; }
        };
        bool on_{false};
        Stat st_[2];

        static RtlElem &inst() { static RtlElem p; return p; }
        RtlElem()
        {
            const char *v = std::getenv("HAS_ELEM_RTL");
            on_ = v && std::atoi(v) != 0;
            // HAS_GVU_TOP also hands the max pool to these pipelines (has/gvu_top_bridge.h): report then as well
            const char *t = std::getenv("HAS_GVU_TOP");
            if (on_ || (t ? std::atoi(t) : 3) > 0) std::atexit(&RtlElem::report);   // HAS_GVU_TOP default 3 (has/gvu_top_bridge.h)
        }
        static bool fit(int64_t v, int bits) { const int64_t h = static_cast<int64_t>(1) << (bits - 1); return v >= -h && v < h; }
        static rtl_ref_gvu::RoundMode map_round(int m)
        {
            namespace R = rtl_ref_gvu;
            static const R::RoundMode t[7] = {R::RND_FLOOR, R::RND_NEAR_UP, R::RND_NEAR_AWAY, R::RND_NEAR_EVEN, R::RND_CEIL, R::RND_TRUNC, R::RND_AWAY};
            return t[m];
        }
        static void report()
        {
            static const char *fn[2] = {"residual add", "max pool"};
            const RtlElem &p = inst();
            std::printf("[elem-rtl] ELEM_WISE on the cycle-level pipelines (HAS_ELEM_RTL): operation | chunks | not expressible | bursts | elements | "
                        "differ from the model | pipeline cycles | model cycles for the same chunks | first difference\n");
            for (int i = 0; i < 2; i++)
            {
                const Stat &s = p.st_[i];
                if (s.chunks == 0) continue;
                std::printf("[elem-rtl] %-12s | chunks %8llu | not expressible %6llu | bursts %8llu | elements %12llu | differ %8llu | "
                            "pipeline cycles %12llu | model cycles %12llu | %s\n", fn[i],
                            static_cast<unsigned long long>(s.chunks), static_cast<unsigned long long>(s.not_expr), static_cast<unsigned long long>(s.bursts),
                            static_cast<unsigned long long>(s.elems), static_cast<unsigned long long>(s.differ), static_cast<unsigned long long>(s.cycles),
                            static_cast<unsigned long long>(s.model_cycles), s.differ ? s.first : (s.no_done ? "a burst did not finish" : "-"));
            }
            std::fflush(stdout);
        }
    };
} // namespace has

#endif // HAS_RTL_ELEM_H
