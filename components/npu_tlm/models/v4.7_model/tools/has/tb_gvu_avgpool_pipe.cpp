// tb_gvu_avgpool_pipe.cpp -- has/rtl_ref_gvu_avgpool_pipe.h against an independent reference.
// The reference is written here from the drawing "Avg_Pool" (16-bit wrapped sum, product with the int32 scale in 64 bits, shift with a
// rounding rule, narrow to int8) with plain integer arithmetic and the rounding rules spelled out, not with the units of
// has/rtl_ref_gvu_fu.h. Cases: all 7 rounding modes, positive / negative scale, shifts 0 .. 40, window sums that overflow int16
// (large windows of large values), saturate and wrap narrowing, random and constructed (tie) values; GvuAvgTail: delay of
// kAvgTailDepth cycles, mask, address, back-to-back beats with gaps.
// build: g++ -std=c++17 -O2 -I. tools/has/tb_gvu_avgpool_pipe.cpp -o tb_gvu_avgpool_pipe
#include <cstdio>
#include <cstdlib>
#include <random>
#include "has/rtl_ref_gvu_avgpool_pipe.h"

using namespace rtl_ref_gvu;

static int ref_lane(const std::vector<int> &x, int32_t scale, int shift, int mode, bool sat)
{
    long long s = 0;
    for (int v : x) s += v;
    s &= 0xFFFF;
    if (s >= 0x8000) s -= 0x10000;                                  // 16-bit two's complement wrap
    const __int128 p = static_cast<__int128>(s) * scale;
    __int128 q = p >> shift;                                        // floor
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
        default: inc = rem > half || (rem == half && (q & 1)); break;   // RND_NEAR_EVEN
        }
    q += inc ? 1 : 0;
    if (sat) return static_cast<int>(q > 127 ? 127 : (q < -128 ? -128 : q));
    return static_cast<int8_t>(static_cast<uint8_t>(q & 0xFF));
}

int main()
{
    std::mt19937 rng(20261025);
    long long checks = 0, bad = 0;
    // ---- (1) GvuAvgPoolPipe against the reference ----
    for (int mode = 0; mode < 7; mode++)
        for (int sat = 0; sat < 2; sat++)
            for (int trial = 0; trial < 40; trial++)
            {
                const int win = (trial % 5 == 0) ? 324 : ((trial % 5 == 1) ? 258 : 1 + int(rng() % 25));
                const int rows = 1 + int(rng() % 6);
                AvgCfg c;
                c.scale = (trial % 3 == 0) ? -int32_t(rng() % 65536) : int32_t(rng() % 200000);
                c.shift = uint8_t(trial % 41);
                c.round = RoundMode(mode);
                c.narrow = sat ? NARROW_SATURATE : NARROW_WRAP;
                std::vector<std::vector<std::array<int8_t, GVU_LANES>>> w(rows, std::vector<std::array<int8_t, GVU_LANES>>(win));
                for (auto &r : w)
                    for (auto &e : r)
                        for (int l = 0; l < GVU_LANES; l++)
                            e[l] = (trial % 4 == 3) ? int8_t(127 - int(rng() % 3)) : int8_t(int(rng() % 256) - 128);
                uint64_t cyc = 0;
                const auto out = GvuAvgPoolPipe::run(w, c, &cyc);
                if (cyc != uint64_t(rows) * win + kAvgTailDepth) { bad++; std::printf("cycles %llu expected %llu\n", (unsigned long long)cyc, (unsigned long long)(uint64_t(rows) * win + kAvgTailDepth)); }
                for (int r = 0; r < rows; r++)
                    for (int l = 0; l < GVU_LANES; l++)
                    {
                        std::vector<int> x;
                        for (const auto &e : w[r]) x.push_back(e[l]);
                        checks++;
                        if (out[r][l] != ref_lane(x, c.scale, c.shift, mode, sat)) bad++;
                    }
            }
    const long long pipe_checks = checks, pipe_bad = bad;
    // ---- (2) tie values: sum * scale an exact odd multiple of 2^(shift-1), both signs ----
    long long tie_bad = 0, tie_n = 0, tie_distinct = 0;
    for (int mode = 0; mode < 7; mode++)
        for (int s16 : {1, -1, 3, -3, 5, -5, 33, -33, 40, -40})
        {
            AvgCfg c;
            c.scale = 32768; c.shift = 16; c.round = RoundMode(mode);
            std::vector<std::vector<std::array<int8_t, GVU_LANES>>> w(1, std::vector<std::array<int8_t, GVU_LANES>>(1));
            for (int l = 0; l < GVU_LANES; l++) w[0][0][l] = int8_t(s16);
            const auto out = GvuAvgPoolPipe::run(w, c);
            const int ref = ref_lane({s16}, c.scale, c.shift, mode, true);
            tie_n++;
            if (out[0][0] != ref) tie_bad++;
            if (mode == RND_NEAR_UP && ref_lane({s16}, c.scale, c.shift, RND_NEAR_AWAY, true) != ref) tie_distinct++;
        }
    // ---- (3) GvuAvgTail: delay, mask, address, gaps ----
    long long tail_bad = 0;
    {
        GvuAvgTail t;
        AvgCfg c;
        c.scale = 7282; c.shift = 16;
        std::vector<GvuAvgTail::Beat> expect(200);
        std::vector<char> pushed(200, 0);
        for (int cyc = 0; cyc < 200; cyc++)
        {
            const GvuAvgTail::Beat &o = t.out();
            const int src = cyc - kAvgTailDepth;
            if (src >= 0 && pushed[static_cast<size_t>(src)])
            {
                if (!o.valid || o.mask != expect[static_cast<size_t>(src)].mask || o.addr != expect[static_cast<size_t>(src)].addr) tail_bad++;
                else
                    for (int l = 0; l < GVU_LANES; l++)
                        if ((o.mask >> l & 1u) && o.data[static_cast<size_t>(l)] != expect[static_cast<size_t>(src)].data[static_cast<size_t>(l)]) tail_bad++;
            }
            else if (o.valid) tail_bad++;
            const bool push = (cyc % 7) != 3 && cyc < 150;
            std::array<int16_t, GVU_LANES> sum{};
            uint32_t mask = uint32_t(rng());
            for (int l = 0; l < GVU_LANES; l++) sum[static_cast<size_t>(l)] = int16_t(int(rng() % 65536) - 32768);
            if (push)
            {
                expect[static_cast<size_t>(cyc)].mask = mask;
                expect[static_cast<size_t>(cyc)].addr = uint32_t(cyc * 3);
                for (int l = 0; l < GVU_LANES; l++)
                    expect[static_cast<size_t>(cyc)].data[static_cast<size_t>(l)] = int8_t(ref_lane({sum[static_cast<size_t>(l)]}, c.scale, c.shift, RND_NEAR_UP, true));
                pushed[static_cast<size_t>(cyc)] = 1;
            }
            t.tick(push, sum, mask, uint32_t(cyc * 3), c);
        }
        if (t.busy()) tail_bad++;
    }
    std::printf("A1 pipe: %lld lane checks, %lld bad; tie cases %lld bad %lld (modes NEAR_UP and NEAR_AWAY differ on %lld of the %lld NEAR_UP cases); tail: bad %lld\n",
                pipe_checks, pipe_bad, tie_n, tie_bad, tie_distinct, (long long)(tie_n / 7), tail_bad);
    const bool ok = pipe_bad == 0 && tie_bad == 0 && tail_bad == 0 && tie_distinct > 0;
    std::printf("RESULT: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
