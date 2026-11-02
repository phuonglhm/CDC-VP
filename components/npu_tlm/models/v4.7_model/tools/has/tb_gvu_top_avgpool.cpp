// tb_gvu_top_avgpool.cpp -- GvuTop POOLING_AVG (has/rtl_ref_gvu_top.h, GvuTopParams::avg_enable) on vector sets and generated cases.
//
// Cycle-level C++17 path (GvuTop, no SystemC). Every case: the tensor [C][H][W] int8 goes to vbank0 buffer 0 as rows of 32 bytes
// (row c * H * W_TILE + h * W_TILE + w_tile), the window geometry to the i_pooling_* inputs (c_move = W_TILE * H, h_move =
// W_TILE * stride), Avg_Scale / Avg_Shift / round mode to i_avg_*; the run ends with o_avgpool_pipe_done; the output rows of vbank2
// (port 1, v32int8, row c * OH * W_TILE + oh * W_TILE + w_tile, lane ow % O_LANE_TILE, as tb/gvu/tb_gvu_top.sv) are compared with a
// reference written here from the drawing "Avg_Pool" with plain integer arithmetic (16-bit wrapped window sum, 64-bit product, the
// seven rounding rules spelled out, saturate to int8).
//
//   tb_gvu_top_avgpool vectors <dir with avgpool/{tile,vec}>     the two sets of the vector-unit owner (stride 1, K 5 / 3, Avg_Shift 16)
//   tb_gvu_top_avgpool cases                                     generated cases: tie values, saturation, stride 2 / 4, K = stride,
//                                                                several W tiles, the largest channel count that fits, narrow wrap
//   tb_gvu_top_avgpool runs                                      two runs back to back on one unit, with and without reset in between
//   tb_gvu_top_avgpool states                                    the generated cases: controller states of every run and the done cycle
//                                                                against a max pool of the same window (5 cycles of tail)
//   environment HAS_GVU_AVG_AS_RTL=1                             the hardware RTL as delivered: NEAR_AWAY and wrap instead of the
//                                                                requested rounding and saturation (negative control: tie / saturation
//                                                                cases must then differ). Read here once.
// Build: g++ -std=c++17 -O2 -I. tools/has/tb_gvu_top_avgpool.cpp -o tb_gvu_top_avgpool
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <string>
#include <vector>
#include "has/rtl_ref_gvu_top.h"

using namespace rtl_ref_gvu;

namespace
{
    constexpr unsigned L = 32;

    struct Case
    {
        std::string name;
        unsigned C, H, W, K, S;
        int32_t scale;
        unsigned shift;
        RoundMode round;
        bool wrap;                 // request wrap narrowing (default saturate)
        std::vector<int8_t> x;     // [C][H][W]
    };

    int ref_lane(const std::vector<int> &x, int32_t scale, int shift, int mode, bool sat)
    {
        long long s = 0;
        for (int v : x) s += v;
        s &= 0xFFFF;
        if (s >= 0x8000) s -= 0x10000;
        const __int128 p = static_cast<__int128>(s) * scale;
        __int128 q = p >> shift;
        const __int128 rem = p - (q << shift);
        const __int128 half = shift ? (static_cast<__int128>(1) << (shift - 1)) : 0;
        const bool neg = p < 0, nz = rem != 0;
        bool inc = false;
        if (shift)
            switch (mode)
            {
            case RND_FLOOR: inc = false; break;
            case RND_CEIL: inc = nz; break;
            case RND_TRUNC: inc = neg && nz; break;
            case RND_AWAY: inc = !neg && nz; break;
            case RND_NEAR_UP: inc = rem >= half; break;
            case RND_NEAR_AWAY: inc = rem > half || (rem == half && !neg); break;
            default: inc = rem > half || (rem == half && (q & 1)); break;
            }
        q += inc ? 1 : 0;
        if (sat) return static_cast<int>(q > 127 ? 127 : (q < -128 ? -128 : q));
        return static_cast<int8_t>(static_cast<uint8_t>(q & 0xFF));
    }

    bool as_rtl()
    {
        static const bool v = std::getenv("HAS_GVU_AVG_AS_RTL") && std::atoi(std::getenv("HAS_GVU_AVG_AS_RTL")) != 0;
        return v;
    }

    GvuTop make_unit()
    {
        GvuTopParams p;
        p.avg_enable = true;
        return GvuTop(p);
    }

    void load(GvuTop &dut, const Case &c)
    {
        const unsigned wt = (c.W + L - 1) / L;
        for (unsigned ch = 0; ch < c.C; ch++)
            for (unsigned h = 0; h < c.H; h++)
                for (unsigned t = 0; t < wt; t++)
                {
                    std::vector<uint8_t> row(L, 0);
                    for (unsigned l = 0; l < L && t * L + l < c.W; l++) row[l] = uint8_t(c.x[(size_t(ch) * c.H + h) * c.W + t * L + l]);
                    dut.scratchpad().sram(0, 0).poke((ch * c.H + h) * wt + t, row);
                }
    }

    struct Result { bool done{false}; uint64_t done_cycle{0}; long long n{0}, bad{0}; unsigned state_after{0}; };

    // Run one case on dut (already loaded; reset phase optional). Returns the comparison. With trace, the controller states the
    // run passes through are appended (one entry per change); with op = GVU_OP_MAXPOOL the same window runs as max pool and only
    // the done cycle is meaningful.
    Result run(GvuTop &dut, const Case &c, bool reset_first, std::vector<unsigned> *trace = nullptr, GvuOpcode op = GVU_OP_AVGPOOL)
    {
        GvuTop::In in;
        if (reset_first)
        {
            for (int i = 0; i < 5; i++) dut.tick(in, false);
            dut.tick(in, true);
        }
        const unsigned wt = (c.W + L - 1) / L, oh = (c.H - c.K) / c.S + 1, ow = (c.W - c.K) / c.S + 1;
        const unsigned olt = c.S == 1 ? L : (c.K == c.S ? (L - c.K) / c.S + 1 : (L - c.K) / c.S + 2);
        in.i_pooling_mode = int(c.K) - 2;
        in.i_pooling_stride = c.S;
        in.i_pooling_num_w_tile = wt;
        in.i_pooling_c = c.C;
        in.i_pooling_oh = oh;
        in.i_pooling_c_move = wt * c.H;
        in.i_pooling_h_move = wt * c.S;
        in.i_avg_scale = c.scale;
        in.i_avg_shift = uint8_t(c.shift);
        in.i_avg_round = as_rtl() ? RND_NEAR_AWAY : c.round;
        in.i_avg_narrow = (as_rtl() || c.wrap) ? NARROW_WRAP : NARROW_SATURATE;
        in.i_gvu_op = op;
        in.i_pipeline_en = true;
        Result r;
        unsigned post = 0;
        if (trace) trace->push_back(unsigned(dut.state()));
        for (uint64_t cyc = 0; cyc < 400000; cyc++)
        {
            const GvuTop::Out o = dut.tick(in, true);
            if (trace && unsigned(dut.state()) != trace->back()) trace->push_back(unsigned(dut.state()));
            const bool done = op == GVU_OP_AVGPOOL ? o.o_avgpool_pipe_done : o.o_maxpool_pipe_done;
            if (done && !r.done) { r.done = true; r.done_cycle = cyc; in.i_pipeline_en = false; }
            if (r.done && ++post >= 15) break;
        }
        r.state_after = unsigned(dut.state());
        if (op != GVU_OP_AVGPOOL) return r;
        for (unsigned ch = 0; ch < c.C; ch++)
            for (unsigned y = 0; y < oh; y++)
                for (unsigned x = 0; x < ow; x++)
                {
                    std::vector<int> win;
                    for (unsigned i = 0; i < c.K; i++)
                        for (unsigned j = 0; j < c.K; j++) win.push_back(c.x[(size_t(ch) * c.H + y * c.S + i) * c.W + x * c.S + j]);
                    const int want = ref_lane(win, c.scale, int(c.shift), int(c.round), !c.wrap);
                    const unsigned addr = (ch * oh + y) * wt + x / olt;
                    const auto word = dut.scratchpad().sram(2, 0).word(addr >> 2);
                    const int got = int8_t(word[(addr & 3u) * L + x % olt]);
                    r.n++;
                    if (got != want) r.bad++;
                }
        return r;
    }

    Case from_vectors(const std::string &dir, bool tile)
    {
        Case c;
        c.name = tile ? "vector tile" : "vector vec";
        c.H = c.W = tile ? 80 : 20;
        c.K = tile ? 5 : 3;
        c.S = 1;
        c.scale = tile ? 2621 : 7282;
        c.shift = 16;
        c.round = RND_NEAR_UP;
        c.wrap = false;
        const unsigned wt = (c.H + L - 1) / L;
        c.C = (25 * 1024) / (c.H * wt * L);
        std::ifstream f(dir + "/avgpool/" + (tile ? "tile" : "vec") + "/input.txt");
        int v;
        while (f >> v) c.x.push_back(int8_t(v));
        return c;
    }

    Case gen(std::mt19937 &rng, const std::string &name, unsigned C, unsigned H, unsigned W, unsigned K, unsigned S, int32_t scale, unsigned shift,
             RoundMode rm, bool wrap, int lo, int hi)
    {
        Case c{name, C, H, W, K, S, scale, shift, rm, wrap, {}};
        c.x.resize(size_t(C) * H * W);
        for (auto &v : c.x) v = int8_t(lo + int(rng() % unsigned(hi - lo + 1)));
        return c;
    }
} // namespace

int main(int argc, char **argv)
{
    const std::string mode = argc > 1 ? argv[1] : "cases";
    std::mt19937 rng(20261025);
    int fails = 0;
    auto report = [&](const Case &c, const Result &r) {
        const bool ok = r.done && r.bad == 0 && r.state_after == unsigned(IDLE);
        std::printf("  %-34s K %u S %u [%u,%u,%u] scale %d shift %u round %d%s: outputs %lld, differ %lld, done at cycle %llu, state %s%s\n", c.name.c_str(), c.K, c.S,
                    c.C, c.H, c.W, c.scale, c.shift, int(c.round), c.wrap ? " wrap" : "", r.n, r.bad, (unsigned long long)r.done_cycle,
                    r.state_after == unsigned(IDLE) ? "IDLE" : "NOT IDLE", ok ? "" : "   <-- FAIL");
        if (!ok) fails++;
    };
    std::printf("tb_gvu_top_avgpool %s%s\n", mode.c_str(), as_rtl() ? " (HAS_GVU_AVG_AS_RTL=1)" : "");
    if (mode == "vectors")
    {
        const std::string dir = argc > 2 ? argv[2] : "target/test";
        for (bool tile : {true, false})
        {
            Case c = from_vectors(dir, tile);
            GvuTop dut = make_unit();
            load(dut, c);
            report(c, run(dut, c, true));
        }
    }
    else if (mode == "cases" || mode == "states")
    {
        std::vector<Case> cs;
        // ties: window sums that make sum * scale an odd multiple of 2^(shift - 1) (scale 32768, shift 16: sum / 2), both signs
        for (RoundMode rm : {RND_FLOOR, RND_CEIL, RND_TRUNC, RND_AWAY, RND_NEAR_UP, RND_NEAR_AWAY, RND_NEAR_EVEN})
            cs.push_back(gen(rng, "ties r" + std::to_string(int(rm)), 4, 20, 20, 2, 2, 32768, 16, rm, false, -3, 3));
        cs.push_back(gen(rng, "saturate int8 (scale 1/2)", 5, 12, 12, 2, 2, 32768, 16, RND_NEAR_UP, false, -128, 127));
        cs.push_back(gen(rng, "saturate, negative scale", 5, 12, 12, 2, 2, -49152, 16, RND_NEAR_UP, false, -128, 127));
        cs.push_back(gen(rng, "wrap narrowing (request)", 5, 12, 12, 2, 2, 32768, 16, RND_NEAR_UP, true, -128, 127));
        cs.push_back(gen(rng, "stride 2, 4x4", 6, 20, 20, 4, 2, 4096, 16, RND_NEAR_UP, false, -128, 127));
        cs.push_back(gen(rng, "stride 4, K = S = 4", 6, 20, 20, 4, 4, 4096, 16, RND_NEAR_EVEN, false, -128, 127));
        cs.push_back(gen(rng, "stride 2, 2x2 (K = S)", 8, 20, 20, 2, 2, 16384, 16, RND_NEAR_UP, false, -128, 127));
        cs.push_back(gen(rng, "3x3 s1 several W tiles", 2, 40, 70, 3, 1, 7282, 16, RND_NEAR_AWAY, false, -128, 127));
        cs.push_back(gen(rng, "5x5 s1 W 100 (4 tiles)", 2, 20, 100, 5, 1, 2621, 16, RND_NEAR_UP, false, -128, 127));
        cs.push_back(gen(rng, "2x2 s1 shift 8", 3, 20, 20, 2, 1, 64, 8, RND_CEIL, false, -128, 127));
        cs.push_back(gen(rng, "largest channel count, 20x20 3x3", 40, 20, 20, 3, 1, 7282, 16, RND_NEAR_UP, false, -128, 127));
        cs.push_back(gen(rng, "int16 sum wrap, 5x5 of 127", 1, 20, 20, 5, 1, 2621, 16, RND_NEAR_UP, false, 127, 127));
        // states: the controller states of every case (IDLE -> POOLING_AVG -> IDLE) and its done cycle against a max pool of the same
        // window and data on a fresh unit (the tail of the average pool is 5 cycles longer).
        auto name = [](unsigned s) {
            return s == unsigned(IDLE) ? std::string("IDLE") : s == unsigned(POOLING_AVG) ? std::string("POOLING_AVG")
                   : s == unsigned(POOLING_MAX) ? std::string("POOLING_MAX") : "state " + std::to_string(s);
        };
        unsigned seq_other = 0, done_other = 0;
        long long first_delta = -1;
        for (const Case &c : cs)
        {
            GvuTop dut = make_unit();
            load(dut, c);
            if (mode == "cases") { report(c, run(dut, c, true)); continue; }
            std::vector<unsigned> tr;
            const Result ra = run(dut, c, true, &tr);
            GvuTop mp = make_unit();
            load(mp, c);
            const Result rm = run(mp, c, true, nullptr, GVU_OP_MAXPOOL);
            std::string seq;
            for (unsigned s : tr) seq += (seq.empty() ? "" : " -> ") + name(s);
            const long long delta = (long long)ra.done_cycle - (long long)rm.done_cycle;
            if (first_delta < 0) first_delta = delta;
            const bool seq_ok = seq == "IDLE -> POOLING_AVG -> IDLE", d_ok = ra.done && rm.done && delta == 5;
            seq_other += !seq_ok;
            done_other += !d_ok;
            std::printf("  %-34s states %s; done at cycle %llu, max pool of the same window at %llu (+%lld)%s\n", c.name.c_str(), seq.c_str(),
                        (unsigned long long)ra.done_cycle, (unsigned long long)rm.done_cycle, delta, seq_ok && d_ok ? "" : "   <-- FAIL");
        }
        if (mode == "states")
        {
            std::printf("states: IDLE -> POOLING_AVG -> IDLE | cases %zu, other sequences %u\n", cs.size(), seq_other);
            std::printf("done after max pool: %lld cycles in every case | cases %zu, other %u\n", first_delta, cs.size(), done_other);
            fails += int(seq_other + done_other);
        }
    }
    else if (mode == "runs")
    {
        Case a = gen(rng, "run A 3x3 s1", 4, 20, 20, 3, 1, 7282, 16, RND_NEAR_UP, false, -128, 127);
        Case b = gen(rng, "run B 3x3 s1 (other data)", 4, 20, 20, 3, 1, 7282, 16, RND_NEAR_UP, false, -128, 127);
        for (int with_reset = 0; with_reset < 2; with_reset++)
        {
            GvuTop dut = make_unit();
            load(dut, a);
            Result ra = run(dut, a, true);
            std::printf(" second run %s reset\n", with_reset ? "after a" : "without");
            report(a, ra);
            load(dut, b);
            const Result rb = run(dut, b, with_reset != 0);
            if (with_reset)
                report(b, rb);
            else
            {
                // B12 of the RTL review: the pooling feeder keeps first_load_cnt between runs, so a second run without a reset reads out of step and ends
                // early; the port reproduces the RTL as delivered, as for max pool (the data flow controller resets the unit before every run). The
                // run must therefore differ here: the check is the negative control, not a failure.
                const bool differs = rb.bad > 0;
                std::printf("  %-34s without reset (B12, as the delivered RTL): outputs %lld, differ %lld, done at cycle %llu  -> %s\n", b.name.c_str(), rb.n, rb.bad,
                            (unsigned long long)rb.done_cycle, differs ? "differs as expected" : "UNEXPECTED: no difference   <-- FAIL");
                if (!differs) fails++;
            }
        }
    }
    else
    {
        std::fprintf(stderr, "usage: %s vectors <dir> | cases | runs\n", argv[0]);
        return 2;
    }
    std::printf("RESULT: %s (%d failing)\n", fails == 0 ? "PASS" : "FAIL", fails);
    return fails == 0 ? 0 : 1;
}
