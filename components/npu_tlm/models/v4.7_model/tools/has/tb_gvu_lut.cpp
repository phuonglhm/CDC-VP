// tb_gvu_lut.cpp -- accuracy + bit-width check of the indirect LUT (recip / rsqrt) interpretation in has/gvu_lut.h.
// HW feedback #4 asked whether log-scale indexing is accurate enough; this measures it on every 32-bit input class:
//   L1. structural: every table delta fits int9, every interpolation product fits int17 (the drawing's widths)
//   L2. relative error vs exact 1/x and 1/sqrt(x): exhaustive x < 2^20, then 2 000 000 log-uniform samples up to 2^31
//   L3. same without interpolation (frac ignored) -- what the interpolation stage buys
#include <systemc.h>
#include <cmath>
#include <cstdio>
#include <random>
#include "has/gvu_lut.h"

using namespace has;

struct Err
{
    double max_rel{0}, sum_rel{0};
    uint64_t n{0};
    uint32_t worst_x{0};
    void add(double got, double want, uint32_t x)
    {
        const double e = std::fabs(got - want) / want;
        sum_rel += e;
        n++;
        if (e > max_rel) { max_rel = e; worst_x = x; }
    }
};

// L4. Independent Python vectors (tools/fe/fe_ref_has_rce.py): lut_indirect_{recip,rsqrt}.txt, rows "x r E".
static int check_vectors(const char *path)
{
    std::FILE *f = std::fopen(path, "r");
    if (!f) { std::printf("[L4] cannot open %s\n", path); return 1; }
    const bool rs = std::string(path).find("rsqrt") != std::string::npos;
    LutIndirect L(14);
    char line[256];
    unsigned long long rows = 0, bad = 0;
    while (std::fgets(line, sizeof line, f))
    {
        if (line[0] == '#' || line[0] == '\n') continue;
        unsigned long long x; long long r, e;
        if (std::sscanf(line, "%llu %lld %lld", &x, &r, &e) != 3) { bad++; continue; }
        const LutResult got = rs ? L.rsqrt(static_cast<uint32_t>(x)) : L.recip(static_cast<uint32_t>(x));
        if (got.r != r || got.e_shift != e)
        {
            if (bad < 5) std::printf("[L4] %s x=%llu got r=%d E=%d want r=%lld E=%lld\n", path, x, got.r, got.e_shift, r, e);
            bad++;
        }
        rows++;
    }
    std::fclose(f);
    std::printf("[L4] %s: %llu rows, %llu mismatches: %s\n", path, rows, bad, bad || !rows ? "FAIL" : "PASS");
    return bad || !rows;
}

int sc_main(int argc, char **argv)
{
    int fail = 0;
    for (int i = 1; i < argc; i++) fail += check_vectors(argv[i]);
    for (int q : {14})
    {
        LutIndirect L(q);
        // L1 -----------------------------------------------------------------------------------------------
        int max_delta = 0;
        for (size_t i = 0; i + 1 < L.recip_table().size(); i++)
            max_delta = std::max(max_delta, std::abs(L.recip_table()[i + 1] - L.recip_table()[i]));
        for (int p = 0; p < 2; p++)
            for (int i = 0; i + 1 < LutIndirect::N; i++)
                max_delta = std::max(max_delta, std::abs(L.rsqrt_table()[p * LutIndirect::N + i + 1] -
                                                         L.rsqrt_table()[p * LutIndirect::N + i]));
        const bool fits = max_delta <= 255 && max_delta * 255 < (1 << 16);
        fail += !fits;
        std::printf("[L1] Q%d: max |T[i+1]-T[i]| = %d (int9 needs <= 255), max |delta*frac| = %d (int17 needs < 65536): %s\n",
                    q, max_delta, max_delta * 255, fits ? "PASS" : "FAIL");
        // L2 / L3 --------------------------------------------------------------------------------------------
        Err er, es, er_ni, es_ni;
        auto eval = [&](uint32_t x) {
            const LutResult a = L.recip(x), b = L.rsqrt(x);
            er.add(L.value(a), 1.0 / x, x);
            es.add(L.value(b), 1.0 / std::sqrt(static_cast<double>(x)), x);
            // no interpolation: truncate x to its 8 mantissa bits below the MSB
            const int E = 31 - __builtin_clz(x);
            const uint32_t keep = E > 8 ? (x >> (E - 8)) << (E - 8) : x;
            er_ni.add(L.value(L.recip(keep)), 1.0 / x, x);
            es_ni.add(L.value(L.rsqrt(keep)), 1.0 / std::sqrt(static_cast<double>(x)), x);
        };
        for (uint32_t x = 1; x < (1u << 20); x++) eval(x);
        std::mt19937_64 rng(11);
        for (int i = 0; i < 2000000; i++)
        {
            const double l = std::ldexp(1.0, 20) * std::pow(2.0, std::uniform_real_distribution<double>(0, 11)(rng));
            eval(static_cast<uint32_t>(std::min(l, 2147483647.0)));
        }
        std::printf("[L2] recip  Q%d interpolated: max rel err %.3e (x=%u), mean %.3e over %llu inputs\n", q, er.max_rel,
                    er.worst_x, er.sum_rel / er.n, (unsigned long long)er.n);
        std::printf("[L2] rsqrt  Q%d interpolated: max rel err %.3e (x=%u), mean %.3e\n", q, es.max_rel, es.worst_x,
                    es.sum_rel / es.n);
        std::printf("[L3] recip  without interpolation: max rel err %.3e, mean %.3e\n", er_ni.max_rel, er_ni.sum_rel / er_ni.n);
        std::printf("[L3] rsqrt  without interpolation: max rel err %.3e, mean %.3e\n", es_ni.max_rel, es_ni.sum_rel / es_ni.n);
        // A Q14 mantissa has resolution 2^-14 of a value in (0.5, 1] -> the representation floor is ~1.2e-4 relative.
        const bool ok = er.max_rel < 3e-4 && es.max_rel < 3e-4;
        fail += !ok;
        std::printf("[L2] interpolated error within 3e-4 (≈ 2.5 LSB of Q14 at 0.5): %s\n", ok ? "PASS" : "FAIL");
    }
    std::printf("RESULT: %s\n", fail ? "FAIL" : "PASS");
    return fail;
}
