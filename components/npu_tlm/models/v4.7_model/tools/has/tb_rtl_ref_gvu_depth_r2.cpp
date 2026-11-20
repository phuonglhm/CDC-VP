// tb_rtl_ref_gvu_depth_r2.cpp -- depth of the GEMM_FUSED stream of GvuTop by RTL revision, and the latency tables of revision 2.
// Prints "DEPTH ..." lines and ends with "RESULT: PASS" or "RESULT: FAIL". Pure C++17.
#include <cstdio>
#include "has/rtl_ref_gvu_top.h"
#include "has/rtl_ref_gvu_fu_r2.h"
using namespace rtl_ref_gvu;
static int errors = 0;
static void expect(const char *what, int got, int want) { if (got != want) { errors++; std::printf("FAIL %s: got %d, want %d\n", what, got, want); } }
int main()
{
    const int base[] = {9, 11, 14};
    for (int b : base)
        for (unsigned rev = 0; rev <= 2; rev++)
        {
            GvuTopParams p; p.gemm_stream_depth = b; p.neo_rev = static_cast<int>(rev);
            GvuTop t(p);
            const int want = rev >= 2 ? std::min(b + 6, GEMM_STREAM_DEPTH_MAX) : b;
            std::printf("DEPTH base %d revision %u unit %d (want %d)\n", b, rev, t.gemm_stream_depth(), want);
            expect("unit depth", t.gemm_stream_depth(), want);
        }
    {   GvuTopParams p; GvuTop t(p);   // defaults: revision 0, constant depth
        expect("default depth", t.gemm_stream_depth(), GEMM_STREAM_DEPTH); }
    expect("mula 32x32 r1", mula_latency_rev(MA_32X32, 1), 4); expect("mula 32x32 r2", mula_latency_rev(MA_32X32, 2), 6);
    expect("mula 32x16 r2", mula_latency_rev(MA_32X16, 2), 4); expect("mula 32x8 r2", mula_latency_rev(MA_32X8, 2), 4); expect("mula 16x16 r2", mula_latency_rev(MA_16X16, 2), 2);
    expect("mulb 40x32 r2", mulb_latency_rev(MB_40X32, 2), 7); expect("mulb 32x32_16x8 r2", mulb_latency_rev(MB_32X32_16X8, 2), 6);
    expect("mulb 32x8 r2", mulb_latency_rev(MB_32X8, 2), 4); expect("mulb 3x16x8 r2", mulb_latency_rev(MB_3X16X8, 2), 2);
    expect("mulb 40x32 r1", mulb_latency_rev(MB_40X32, 1), 4); expect("rs r1", lat_vroundshift_rev(1), 1); expect("rs r2", lat_vroundshift_rev(2), 2);
    std::printf("RESULT: %s\n", errors ? "FAIL" : "PASS");
    return errors ? 1 : 0;
}
