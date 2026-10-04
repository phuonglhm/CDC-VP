// tb_gvu_vrf.cpp -- has/gvu_vrf.h against table 2 of the vector-unit specification: every pipeline stage must fit the
// specification's pipeline registers (3 x v32int64 + 4 x v32int32 with its reconfiguration rules); the aliased 1 KB
// organisation is reported for comparison. Also checks has/gvu_lsu.h counting. No SystemC needed.
// Build and run (repository root): g++ -std=c++17 -O2 -I. tools/has/tb_gvu_vrf.cpp -o tools/has/tb_gvu_vrf && tools/has/tb_gvu_vrf
#include <cstdio>
#include <string>
#include "has/gvu_lsu.h"
#include "has/gvu_vrf.h"

int main()
{
    using namespace has;
    int fail = 0, fit_1k = 0;
    std::printf("%-22s %4s %4s %4s %4s %6s  %-58s %s\n", "stage", "i64", "i32", "i16", "i8", "bytes", "specification layout", "aliased 1 KB");
    for (const RegNeed &n : pipeline_reg_needs())
    {
        std::string how_spec, how_1k;
        const bool s = vrf_fits(n, VrfLayout::Spec, &how_spec);
        const bool a = vrf_fits(n, VrfLayout::Aliased1K, &how_1k);
        fit_1k += a ? 1 : 0;
        if (!s) fail++;
        std::printf("%-22s %4d %4d %4d %4d %6u  %-58s %s\n", n.pipeline, n.i64, n.i32, n.i16, n.i8, n.bytes(),
                    ((s ? "fits: " : "DOES NOT FIT: ") + how_spec).c_str(), ((a ? "fits (" : "does not fit (") + how_1k + ")").c_str());
    }

    // Load/store unit counting: 100 int8 elements = 4 vector accesses of 32 lanes, 3 scalar int32 = 3 accesses of 4 B.
    GvuLsu lsu;
    lsu.load(LsuBank::VBANK0, LsuMode::V32I8, 100);
    lsu.store(LsuBank::VBANK2, LsuMode::V32I32, 64);
    lsu.load(LsuBank::SBANK, LsuMode::S32, 3);
    const auto &a = lsu.at(LsuBank::VBANK0, LsuMode::V32I8), &b = lsu.at(LsuBank::VBANK2, LsuMode::V32I32),
               &c = lsu.at(LsuBank::SBANK, LsuMode::S32);
    const bool lsu_ok = a.rd == 4 && a.rd_bytes == 100 && b.wr == 2 && b.wr_bytes == 256 && c.rd == 3 && c.rd_bytes == 12 &&
                        lsu.accesses() == 9;
    if (!lsu_ok) fail++;
    std::printf("load/store unit counting: %s\n%s", lsu_ok ? "ok" : "WRONG", lsu.report().c_str());

    const int total = int(pipeline_reg_needs().size());
    std::printf("specification layout: %d/%d stages fit; aliased 1 KB: %d/%d\n", total - (fail - (lsu_ok ? 0 : 1)), total, fit_1k, total);
    std::printf("RESULT: %s\n", fail ? "FAIL" : "PASS");
    return fail ? 1 : 0;
}
