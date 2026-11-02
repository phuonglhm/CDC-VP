// tb_gvu_ln_lanes.cpp -- the lane LayerNorm of has/rtl_ref_gvu_ln_lanes.h (statistics per lane, D_k computed again in stage 4, result
// in place in vbank0, beta / gamma per beat in the sbank) against has::layernorm_row, row by row, on random groups: H from 1 to 768
// (the sbank depth), 1 to 32 rows, random parameters and inputs, every round mode. Also checks ln_lanes_fit (H 769, int16 output,
// gamma outside int8, per-channel M0_mul are refused) and that the result of a group does not depend on the rows of another lane.
// build: g++ -std=c++17 -O2 -Wall -I. tools/has/tb_gvu_ln_lanes.cpp -o tb_gvu_ln_lanes
// usage: tb_gvu_ln_lanes [groups, default 300] [seed, default 1] [dump dir]
//        tb_gvu_ln_lanes tiers [groups, default 100] [seed, default 1]: stops the lane machine at the end of every stage and compares
//        S, V, v, w, r, E of every lane and n_k, y_k of vbank0 with has::layernorm_row; checks where beta and gamma are placed
//   dump dir: the first four groups (H 1, 32, 768, 197; 32 rows) as text vectors for the hardware team, one file per group:
//   "CFG H rows round Pre_Shift M0_var TS_var Z_var M0_7 TS_7 E_bias Z_7 M0_div TS_div Z_div Z_mul Z_out r8_floor r8_z r9_exact r9_lsb M0_mul TS_mul",
//   "GAMMA g..", "BETA b..", then per row "X x.." and "Y y.." (golden, has::layernorm_row).
#include <cstdio>
#include <string>
#include <cstdlib>
#include <random>
#include <vector>
#include "has/rtl_ref_gvu_ln_lanes.h"

namespace
{
    has::LnParams random_params(std::mt19937_64 &g, int H)
    {
        auto u = [&](long long lo, long long hi) { return std::uniform_int_distribution<long long>(lo, hi)(g); };
        has::LnParams p;
        p.H = H; p.Pre_Shift = int(u(0, 10));
        p.M0_var = u(1ll << 30, (1ll << 31) - 1); p.TS_var = int(u(28, 48)); p.Z_var = int(u(-128, -100));
        p.M0_7 = u(1ll << 30, (1ll << 31) - 1); p.TS_7 = int(u(28, 40)); p.E_bias = u(-1000, 1000); p.Z_7 = int(u(-128, -100));
        p.M0_div = u(1ll << 30, (1ll << 31) - 1); p.TS_div = int(u(36, 60)); p.Z_div = int(u(-4, 4));
        p.Z_mul = int(u(-8, 8)); p.Z_out = int(u(-8, 8));
        p.r8_floor_z_out = u(0, 1) != 0; p.r8_z_out = int(u(1, 3));
        p.r9_exact = u(0, 3) != 0; p.r9_lsb_bits = int(u(8, 32));
        const long long m0 = u(1ll << 30, (1ll << 31) - 1);
        const int ts = int(u(28, 40));
        p.gamma.resize(size_t(H)); p.beta.resize(size_t(H)); p.M0_mul.assign(size_t(H), m0); p.TS_mul.assign(size_t(H), ts);
        for (int j = 0; j < H; j++) { p.gamma[size_t(j)] = int(u(-128, 127)); p.beta[size_t(j)] = u(-3000, 3000); }
        return p;
    }

    // Stage by stage: the lane machine is stopped at the end of every stage and its lane statistics (S after stage 1; V and v after
    // stage 2; w, r, E after stage 3) and the words of vbank0 (n_k after stage 4, y_k after stage 5) are compared with the values of
    // has::layernorm_row for the same row. Also checks where ln_lanes_load_params puts beta (sbank buffer 0, int32) and gamma
    // (sbank buffer 1) of channel k: in word k.
    int tiers(int groups, unsigned long long seed)
    {
        using namespace rtl_ref_gvu;
        std::mt19937_64 g(seed);
        has::Knobs k;
        const has::LutIndirect lut(14, true, true);
        long tokens = 0, elems = 0, words = 0, place = 0, sb = 0, Vb = 0, vb = 0, wb = 0, rb = 0, Eb = 0, nb = 0, yb = 0, skipped = 0;
        for (int n = 0; n < groups; n++)
        {
            const int H = n < 4 ? (n == 0 ? 1 : n == 1 ? 32 : n == 2 ? 768 : 197) : int(std::uniform_int_distribution<int>(1, 768)(g));
            const int rows = n < 4 ? 32 : int(std::uniform_int_distribution<int>(1, 32)(g));
            const int rm = n % 7;
            const has::LnParams p = random_params(g, H);
            if (!ln_lanes_fit(p)) { skipped++; continue; }
            std::vector<std::vector<int>> X(static_cast<size_t>(rows), std::vector<int>(static_cast<size_t>(H)));
            for (auto &r : X)
                for (int &v : r) v = std::uniform_int_distribution<int>(-128, 127)(g);
            GvuScratchpad sp;
            for (int c = 0; c < H; c++)
            {
                SpBytes w(32, 0);
                for (int l = 0; l < rows; l++) w[size_t(l)] = uint8_t(int8_t(X[size_t(l)][size_t(c)]));
                sp.sram(0, 0).poke(unsigned(c), w);
            }
            ln_lanes_load_params(sp, p);
            for (int c = 0; c < H; c++)
            {
                const SpBytes bw = sp.sram(3, 0).word(unsigned(c)), gw = sp.sram(3, 1).word(unsigned(c));
                const int32_t beta = int32_t(uint32_t(bw[0]) | uint32_t(bw[1]) << 8 | uint32_t(bw[2]) << 16 | uint32_t(bw[3]) << 24);
                words++;
                place += beta != p.beta[size_t(c)] || int8_t(gw[0]) != p.gamma[size_t(c)];
            }
            has::Knobs kr = k;
            kr.round_mode = rm;
            std::vector<has::LnRow> ref;
            for (int l = 0; l < rows; l++) ref.push_back(has::layernorm_row(X[size_t(l)], p, lut, kr));
            tokens += rows;
            elems += long(rows) * H;
            GvuLnLanes u;
            u.begin(sp, rows, p, lut, rm);
            int last = u.stage();
            for (bool done = false; !done;)
            {
                done = u.tick();
                const int s = u.stage();
                if (s == last) continue;
                for (int l = 0; l < rows; l++)
                {
                    const has::LnRow &o = ref[size_t(l)];
                    if (last == 0) sb += u.lane_S(l) != o.S;
                    if (last == 1) { Vb += u.lane_V(l) != o.V; vb += u.lane_v(l) != o.v; }
                    if (last == 2) { wb += u.lane_w(l) != o.w; rb += u.lane_r(l) != o.r; Eb += u.lane_E(l) != o.E; }
                    for (int c = 0; c < H && last >= 3; c++)
                    {
                        const int got = int8_t(sp.sram(0, 0).word(unsigned(c))[size_t(l)]);
                        if (last == 3) nb += got != o.n[size_t(c)];
                        if (last == 4) yb += got != o.y[size_t(c)];
                    }
                }
                last = s;
            }
        }
        std::printf("tb_gvu_ln_lanes tiers: %d groups (%ld refused by ln_lanes_fit), reference has::layernorm_row of each token\n", groups, skipped);
        std::printf("parameters: beta words %ld, gamma words %ld, placement errors %ld\n", words, words, place);
        std::printf("tier 1-2: rows %ld, S differ %ld, V differ %ld, v differ %ld\n", tokens, sb, Vb, vb);
        std::printf("tier 3: rows %ld, w differ %ld, r differ %ld, E differ %ld\n", tokens, wb, rb, Eb);
        std::printf("tier 4: elements %ld, n differ %ld\n", elems, nb);
        std::printf("tier 5: elements %ld, y differ %ld\n", elems, yb);
        // negative control: one group of 32 tokens, H 197, against a reference made with Pre_Shift + 1 (a wrong variance scale)
        long neg = 0;
        {
            std::mt19937_64 g3(seed + 99);
            const has::LnParams p = random_params(g3, 197);
            has::LnParams q = p;
            q.Pre_Shift += 1;
            GvuScratchpad sp;
            std::vector<std::vector<int>> X(32, std::vector<int>(197));
            for (auto &r : X)
                for (int &v : r) v = std::uniform_int_distribution<int>(-128, 127)(g3);
            for (int c = 0; c < 197; c++)
            {
                SpBytes w(32, 0);
                for (int l = 0; l < 32; l++) w[size_t(l)] = uint8_t(int8_t(X[size_t(l)][size_t(c)]));
                sp.sram(0, 0).poke(unsigned(c), w);
            }
            ln_lanes_load_params(sp, p);
            GvuLnLanes u;
            u.begin(sp, 32, p, lut, 0);
            while (u.stage() < 2) u.tick();
            has::Knobs kq = k;
            kq.round_mode = 0;
            for (int l = 0; l < 32; l++) neg += u.lane_v(l) != has::layernorm_row(X[size_t(l)], q, lut, kq).v;
        }
        std::printf("negative: reference with Pre_Shift + 1, v differ %ld of 32 (must not be 0)\n", neg);
        const bool ok = skipped == 0 && place + sb + Vb + vb + wb + rb + Eb + nb + yb == 0 && tokens > 0 && neg > 0;
        std::printf("RESULT: %s\n", ok ? "PASS" : "FAIL");
        return ok ? 0 : 1;
    }
}

int main(int argc, char **argv)
{
    using namespace rtl_ref_gvu;
    if (argc > 1 && std::string(argv[1]) == "tiers")
        return tiers(argc > 2 ? std::atoi(argv[2]) : 100, argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 1);
    const int groups = argc > 1 ? std::atoi(argv[1]) : 300;
    std::mt19937_64 g(argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 1);
    has::Knobs k;
    const has::LutIndirect lut(14, true, true);
    long lanes = 0, bad = 0, fit_bad = 0;
    uint64_t cyc = 0;
    for (int n = 0; n < groups; n++)
    {
        const int H = n < 4 ? (n == 0 ? 1 : n == 1 ? 32 : n == 2 ? 768 : 197) : int(std::uniform_int_distribution<int>(1, 768)(g));
        const int rows = n < 4 ? 32 : int(std::uniform_int_distribution<int>(1, 32)(g));
        const int rm = n % 7;
        const has::LnParams p = random_params(g, H);
        if (!ln_lanes_fit(p)) { fit_bad++; continue; }
        std::vector<std::vector<int>> X(static_cast<size_t>(rows), std::vector<int>(static_cast<size_t>(H)));
        for (auto &r : X)
        {
            const int lo = std::uniform_int_distribution<int>(-128, 0)(g), hi = std::uniform_int_distribution<int>(lo, 127)(g);
            for (int &v : r) v = std::uniform_int_distribution<int>(lo, hi)(g);
        }
        GvuScratchpad sp;
        for (int c = 0; c < H; c++)
        {
            SpBytes w(32, 0);
            for (int l = 0; l < 32; l++) w[size_t(l)] = l < rows ? uint8_t(int8_t(X[size_t(l)][size_t(c)])) : uint8_t(std::uniform_int_distribution<int>(0, 255)(g));
            sp.sram(0, 0).poke(unsigned(c), w);
        }
        ln_lanes_load_params(sp, p);
        has::Knobs kr = k;
        kr.round_mode = rm;
        if (argc > 3 && n < 4)
        {
            const std::string fn = std::string(argv[3]) + "/ln_lanes_g" + std::to_string(n) + "_h" + std::to_string(H) + ".txt";
            if (std::FILE *f = std::fopen(fn.c_str(), "w"))
            {
                std::fprintf(f, "CFG %d %d %d %d %lld %d %d %lld %d %lld %d %lld %d %d %d %d %d %d %d %d %lld %d\n", H, rows, rm, p.Pre_Shift,
                             (long long)p.M0_var, p.TS_var, p.Z_var, (long long)p.M0_7, p.TS_7, (long long)p.E_bias, p.Z_7, (long long)p.M0_div, p.TS_div,
                             p.Z_div, p.Z_mul, p.Z_out, int(p.r8_floor_z_out), p.r8_z_out, int(p.r9_exact), p.r9_lsb_bits, (long long)p.M0_mul[0], p.TS_mul[0]);
                std::fprintf(f, "GAMMA"); for (int v : p.gamma) std::fprintf(f, " %d", v); std::fprintf(f, "\n");
                std::fprintf(f, "BETA"); for (long long v : p.beta) std::fprintf(f, " %lld", v); std::fprintf(f, "\n");
                has::Knobs kd = k; kd.round_mode = rm;
                for (int l = 0; l < rows; l++)
                {
                    const has::LnRow o = has::layernorm_row(X[size_t(l)], p, lut, kd);
                    std::fprintf(f, "X"); for (int v : X[size_t(l)]) std::fprintf(f, " %d", v); std::fprintf(f, "\n");
                    std::fprintf(f, "Y"); for (int v : o.y) std::fprintf(f, " %d", v); std::fprintf(f, "\n");
                }
                std::fclose(f);
            }
        }
        const LnLaneRun run = ln_lanes_run(sp, rows, p, lut, rm);
        if (!run.ok) { bad++; continue; }
        cyc += run.cycles();
        for (int l = 0; l < rows; l++)
        {
            const has::LnRow o = has::layernorm_row(X[size_t(l)], p, lut, kr);
            for (int c = 0; c < H; c++)
            {
                const int got = int8_t(sp.sram(0, 0).word(unsigned(c))[size_t(l)]);
                lanes++;
                if (got != o.y[size_t(c)])
                {
                    if (bad < 5) std::printf("  group %d H %d rows %d round %d lane %d channel %d: lanes %d golden %d\n", n, H, rows, rm, l, c, got, o.y[size_t(c)]);
                    bad++;
                }
            }
        }
    }
    // refused cases
    std::mt19937_64 g2(7);
    has::LnParams q = random_params(g2, 64);
    int refused = 0;
    { has::LnParams a = random_params(g2, 769); refused += !ln_lanes_fit(a); }
    { has::LnParams a = q; a.out_int16 = true; refused += !ln_lanes_fit(a); }
    { has::LnParams a = q; a.gamma[5] = 200; refused += !ln_lanes_fit(a); }
    { has::LnParams a = q; a.M0_mul[3] += 1; refused += !ln_lanes_fit(a); }
    std::printf("tb_gvu_ln_lanes: %d groups, lanes %ld, differ %ld, parameter sets refused %ld (expected 0), cases refused %d / 4, cycles %llu\n",
                groups, lanes, bad, fit_bad, refused, (unsigned long long)cyc);
    const bool ok = bad == 0 && fit_bad == 0 && refused == 4 && lanes > 0;
    std::printf("RESULT: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
