// tb_rtl_ref_gvu_latency.cpp -- the latency (clock edges from the accepted beat to the output valid) of the ported multipliers, Round-Shift and LUT unit
// of one RTL revision, printed in the format of the LATENCY lines of the RTL oracle (tools/has/gvu_oracle/tb_oracle_fu.sv, tb_oracle_b.sv), with the
// default module parameters. tools/has/gvu_oracle/check_latency.py compares them with the lines of an oracle summary.
// build: g++ -std=c++17 -O2 -I. tools/has/tb_rtl_ref_gvu_latency.cpp -o tb_rtl_ref_gvu_latency ; run: <binary> <revision 0 | 1 | 2>
#include <cstdio>
#include <cstdlib>
#include "has/rtl_ref_gvu_fu_r2.h"
#include "has/rtl_ref_gvu_lut_r2.h"
using namespace rtl_ref_gvu;
int main(int argc, char **argv)
{
    const unsigned rev = argc > 1 ? static_cast<unsigned>(std::atoi(argv[1])) : 0u;
    const int a[4] = {mula_latency_rev(MA_32X32, rev), mula_latency_rev(MA_32X16, rev), mula_latency_rev(MA_32X8, rev), mula_latency_rev(MA_16X16, rev)};
    const int b[4] = {mulb_latency_rev(MB_40X32, rev), mulb_latency_rev(MB_32X32_16X8, rev), mulb_latency_rev(MB_32X8, rev), mulb_latency_rev(MB_3X16X8, rev)};
    std::printf("LATENCY mul32x32 32x32 %d..%d 32x16 %d..%d 32x8 %d..%d 16x16 %d..%d\n", a[0], a[0], a[1], a[1], a[2], a[2], a[3], a[3]);
    std::printf("LATENCY mul48x40 40x32 %d..%d 32x32_16x8 %d..%d 32x8 %d..%d 3x16x8 %d..%d\n", b[0], b[0], b[1], b[1], b[2], b[2], b[3], b[3]);
    std::printf("LATENCY mul48x40 40x32 %d..%d 32x8 %d..%d\n", b[0], b[0], b[2], b[2]);
    const int rs = lat_vroundshift_rev(rev);
    std::printf("LATENCY roundshift64 %d..%d\n", rs, rs);
    std::printf("LATENCY roundshift72 %d..%d\n", rs, rs);
    const LutParams lp;
    auto L = [&](LutType t, LutIdxMode m) { return rev >= 2 ? lat_lut_r2(t, m, lp) : lat_lut(t, m, lp); };
    std::printf("LATENCY lut direct %d..%d index_mode_0 %d..%d index_mode_1 %d..%d\n", L(LUT_ACT, LUT_PWL), L(LUT_ACT, LUT_PWL), L(LUT_RECIP, LUT_LOG_SCALE), L(LUT_RECIP, LUT_LOG_SCALE), L(LUT_RECIP, LUT_PWL), L(LUT_RECIP, LUT_PWL));
    return 0;
}
