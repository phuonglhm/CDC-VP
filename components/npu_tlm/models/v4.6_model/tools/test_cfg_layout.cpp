// Standalone self-test for sauria_cfg_layout.h: proves the ENCODER and DECODER are
// bit-symmetric (encode then decode is identity) across every width config, so the
// tb decoder and the driver-side encoder cannot drift. Does NOT need SystemC.
//
// Build & run (from unified/):
//   g++ -std=c++17 -I. tools/test_cfg_layout.cpp -o /tmp/test_cfg_layout && /tmp/test_cfg_layout
#include "sauria_cfg_layout.h"
#include <cstdio>
#include <cstdint>
using namespace sauria;

static bool roundtrip(CfgWidths cw, uint64_t seed)
{
    uint64_t in[F_CFG_COUNT] = {0};
    uint64_t s = seed;
    for (int i = 0; i < SAURIA_CFG_LAYOUT_N; i++)
    {
        const CfgFieldSpec &f = SAURIA_CFG_LAYOUT[i];
        if (f.special == SP_SKIP_PER_ROW)
            continue;
        uint32_t w = cfg_resolve_width(f.wkind, cw);
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        uint64_t mask = (w >= 64) ? ~0ULL : ((1ULL << w) - 1ULL);
        in[f.field] = (s >> 11) & mask;
    }
    std::vector<uint32_t> args;
    cfg_encode(in, 22, cw, args);
    uint64_t out[F_CFG_COUNT];
    cfg_decode(args, 22, cw, out);
    for (int i = 0; i < SAURIA_CFG_LAYOUT_N; i++)
    {
        const CfgFieldSpec &f = SAURIA_CFG_LAYOUT[i];
        if (f.special == SP_SKIP_PER_ROW)
            continue;
        if (in[f.field] != out[f.field])
        {
            printf("  MISMATCH field=%d in=%llu out=%llu\n", f.field,
                   (unsigned long long)in[f.field], (unsigned long long)out[f.field]);
            return false;
        }
    }
    return true;
}

int main()
{
    struct { const char *n; CfgWidths w; } tgts[] = {
        {"int8/int16_8x16", {15, 16, 14, 16, 8}},
        {"32x32",           {17, 17, 16, 32, 32}},
        {"64x64",           {18, 18, 17, 64, 64}},
        {"fp16_8x16",       {15, 15, 15, 16, 8}},
    };
    int fails = 0;
    for (auto &t : tgts)
    {
        bool ok = true;
        for (uint64_t seed = 1; seed <= 200; seed++)
            if (!roundtrip(t.w, seed)) { ok = false; fails++; break; }
        printf("%-16s : %s\n", t.n, ok ? "ROUND-TRIP OK (200 seeds)" : "FAIL");
    }
    printf("%s\n", fails ? "SOME FAILED" : "ALL ENCODE<->DECODE ROUND-TRIPS PASS");
    return fails ? 1 : 0;
}
