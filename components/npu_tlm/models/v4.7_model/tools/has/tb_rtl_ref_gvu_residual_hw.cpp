// tb_rtl_ref_gvu_residual_hw.cpp -- unit checks of has/rtl_ref_gvu_residual_hw.h and the RTL replay driver.
//   tb_rtl_ref_gvu_residual_hw                          self-check: the mapping of the model's ADD parameters (ResidualHw::map) against has::dequant / add_sat32 / requant
//   tb_rtl_ref_gvu_residual_hw --run <in> <out> <n> <sa> <sb> <bias hex> <sh> <z> <sub> <logical>   one vector run: <in> has one "a b" pair per line, <out> gets one result per line then "DONE <cycles>"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include "has/gvu_quant.h"
#include "has/rtl_ref_gvu_residual_hw.h"

using namespace has;
namespace R = rtl_ref_gvu;

int main(int argc, char **argv)
{
    if (argc > 1 && !std::strcmp(argv[1], "--run"))
    {
        if (argc < 12) { std::fprintf(stderr, "usage: --run in out n sa sb bias_hex sh z sub logical\n"); return 2; }
        const int n = std::atoi(argv[4]);
        R::ResidualHwParams p;
        p.sa = static_cast<int32_t>(std::strtoll(argv[5], nullptr, 10)); p.sb = static_cast<int32_t>(std::strtoll(argv[6], nullptr, 10));
        p.bias = static_cast<int64_t>(std::strtoull(argv[7], nullptr, 16)); p.sh = std::atoi(argv[8]); p.z = std::atoi(argv[9]);
        p.sub = std::atoi(argv[10]) != 0; p.logical_shift = std::atoi(argv[11]) != 0;
        std::FILE *fi = std::fopen(argv[2], "r"), *fo = std::fopen(argv[3], "w");
        if (!fi || !fo) return 2;
        for (int i = 0; i < n; i++) { int a = 0, b = 0; if (std::fscanf(fi, "%d %d", &a, &b) != 2) return 2; std::fprintf(fo, "%d\n", R::residual_hw_lane(a, b, p)); }
        std::fprintf(fo, "DONE %llu\n", static_cast<unsigned long long>(R::residual_hw_cycles(n)));
        std::fclose(fi); std::fclose(fo);
        return 0;
    }
    std::mt19937_64 g(20261101);
    auto ri = [&](int64_t lo, int64_t hi) { return static_cast<int64_t>(lo + static_cast<int64_t>(g() % static_cast<uint64_t>(hi - lo + 1))); };
    long n = 0, bad = 0, notfit = 0, fit = 0;
    for (int mode = 0; mode < 2; mode++)
    {
        Knobs k; k.round_mode = mode == 0 ? FLOOR : HALF_UP; k.scale_fmt = I32;
        for (int it = 0; it < 1000000; it++)
        {
            const int sA = static_cast<int>(ri(0, 15)), sB = static_cast<int>(ri(0, 15));
            const int64_t SA = ri(0, 70000) << (ri(0, 1) ? sA : 0), SB = ri(0, 70000) << (ri(0, 1) ? sB : 0), SO = ri(1, 3) << ri(0, 20);
            const int zpA = static_cast<int>(ri(-128, 127)), zpB = static_cast<int>(ri(-128, 127)), zpO = static_cast<int>(ri(-128, 127)), sO = static_cast<int>(ri(0, 46));
            R::ResidualHwParams p;
            if (!R::ResidualHw::map(zpA, zpB, zpO, SA, SB, SO, sA, sB, sO, k.round_mode, true, &p)) { notfit++; continue; }
            fit++;
            const int a = static_cast<int>(ri(-128, 127)), b = static_cast<int>(ri(-128, 127));
            const int32_t da = dequant(a, zpA, SA, sA, k), db = dequant(b, zpB, SB, sB, k);
            const int m = requant(add_sat32(da, db), SO, sO, zpO, k);
            n++; bad += R::residual_hw_lane(a, b, p) != m;
        }
    }
    std::printf("map() vs has::dequant/add_sat32/requant (FLOOR and HALF_UP): %ld expressible samples, %ld not expressible, differ %ld\n", fit, notfit, bad);
    // not expressible must be refused
    R::ResidualHwParams p;
    const bool r1 = R::ResidualHw::map(0, 0, 0, 101, 100, 1, 1, 0, 4, 1, true, &p);          // per-operand shift that does not divide
    const bool r2 = R::ResidualHw::map(0, 0, 0, 100, 100, 1, 0, 0, 4, 2, true, &p);          // HALF_AWAY
    const bool r3 = R::ResidualHw::map(0, 0, 0, 5000000, 100, 1, 0, 0, 4, 1, true, &p);     // scale too large
    std::printf("refusals (per-operand shift, HALF_AWAY, scale > 4e6): %d %d %d (all must be 0)\n", r1, r2, r3);
    const bool ok = bad == 0 && fit > 0 && !r1 && !r2 && !r3;
    std::printf("RESULT: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
