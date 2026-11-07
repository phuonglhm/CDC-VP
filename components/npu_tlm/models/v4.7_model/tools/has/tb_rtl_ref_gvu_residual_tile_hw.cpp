// tb_rtl_ref_gvu_residual_tile_hw.cpp -- the RESIDUAL test vectors of the hardware team (neo_lite target/test/residual/tile, 409 600 elements) through the
// model's RESIDUAL unit (has/rtl_ref_gvu_residual_hw.h, residual_hw_lane), with the register values of the hardware bench (tb/tb_neo_top.sv "Test config":
// Sa 1073741824, Sb 1055666676, bias 266712933412, sh 30, out_z -125, add).
//   usage: tb_rtl_ref_gvu_residual_tile_hw [<vector dir>]   (default tools/has/gvu_top_vectors/residual_tile_rev1: input_a.txt, input_b.txt, golden_output.txt,
//          one unsigned byte per line)
// PASS: the arithmetic shift (default of the model, the fixed RTL) gives 0 differences, and the logical shift of the earlier RTL drop
// (HAS_GVU_RESIDUAL_HW_SHIFT_LOGICAL=1) gives 154 (control: exactly the elements whose sum plus bias is negative).
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include "has/rtl_ref_gvu_residual_hw.h"

namespace R = rtl_ref_gvu;

static std::vector<int> read_bytes(const std::string &f)
{
    std::ifstream s(f); std::vector<int> v; int x;
    while (s >> x) v.push_back(x > 127 ? x - 256 : x);
    return v;
}

int main(int argc, char **argv)
{
    const std::string dir = argc > 1 ? argv[1] : "tools/has/gvu_top_vectors/residual_tile_rev1";
    const std::vector<int> a = read_bytes(dir + "/input_a.txt"), b = read_bytes(dir + "/input_b.txt"), g = read_bytes(dir + "/golden_output.txt");
    const size_t kElems = 409600, kLogicalDiff = 154;
    if (a.size() != kElems || b.size() != kElems || g.size() != kElems)
    {
        std::printf("vector sizes %zu %zu %zu (expected %zu)\nRESULT: FAIL\n", a.size(), b.size(), g.size(), kElems);
        return 1;
    }
    R::ResidualHwParams p;
    p.sa = 1073741824; p.sb = 1055666676; p.bias = 266712933412LL; p.sh = 30; p.z = -125; p.sub = false;
    size_t diff[2] = {0, 0}, negative = 0;
    for (int lg = 0; lg < 2; lg++)
    {
        p.logical_shift = lg != 0;
        for (size_t i = 0; i < kElems; i++) diff[lg] += R::residual_hw_lane(a[i], b[i], p) != g[i];
    }
    for (size_t i = 0; i < kElems; i++)
        negative += static_cast<int64_t>(a[i]) * p.sa + static_cast<int64_t>(b[i]) * p.sb + p.bias < 0;
    std::printf("arithmetic shift (default): %zu / %zu differ from golden\n", diff[0], kElems);
    std::printf("logical shift (HAS_GVU_RESIDUAL_HW_SHIFT_LOGICAL=1, earlier RTL drop): %zu / %zu differ (elements with a negative sum: %zu)\n", diff[1], kElems, negative);
    std::printf("selected by the environment (%s): %zu differ\n", R::ResidualHw::logical() ? "logical" : "arithmetic", diff[R::ResidualHw::logical() ? 1 : 0]);
    std::printf("model cycles per tile of 12 800 elements: %llu\n", static_cast<unsigned long long>(R::residual_hw_cycles(12800)));
    const bool ok = diff[0] == 0 && diff[1] == kLogicalDiff && negative == kLogicalDiff;
    std::printf("RESULT: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
