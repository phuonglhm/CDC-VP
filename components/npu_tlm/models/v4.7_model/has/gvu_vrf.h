// gvu_vrf.h -- vector-unit pipeline registers (vector-unit specification section 3 "Programmable Stream Switch", table 2
// "Regfile access ports of compute pipeline"). Structural model: it does not hold data (the pipelines are functional);
// it answers whether the registers each pipeline stage accesses in parallel fit a register-file organisation.
//
// Layouts:
//   Spec: 3 x v32int64 + 4 x v32int32 (1.25 KB). Reconfiguration: v32int64 reg#0 -> 4 x v32int16, v32int64 reg#2 ->
//         2 x v32int32, v32int32 reg#0 -> 4 x v32int8 or 2 x v32int16. Every other register keeps its width.
//   Aliased1K: 1 KB of storage shared by any mix of v32int8 (32 B), v32int16 (64 B), v32int32 (128 B), v32int64
//         (256 B) registers (a free aliasing organisation, as proposed for the vector-unit register file).
// fits() returns the chosen configuration or why no configuration works. tools/has/tb_gvu_vrf.cpp checks table 2.
#ifndef HAS_GVU_VRF_H
#define HAS_GVU_VRF_H

#include <cstdint>
#include <string>
#include <vector>

namespace has
{
    struct RegNeed
    {
        const char *pipeline;
        int i64, i32, i16, i8;
        uint32_t bytes() const { return uint32_t(i64) * 256 + uint32_t(i32) * 128 + uint32_t(i16) * 64 + uint32_t(i8) * 32; }
    };

    // Table 2 of the vector-unit specification: registers accessed in parallel per compute pipeline stage.
    inline const std::vector<RegNeed> &pipeline_reg_needs()
    {
        static const std::vector<RegNeed> t = {
            {"Requant", 1, 0, 2, 0},
            {"Dequant", 1, 1, 0, 0},
            {"GEMM_FUSED", 1, 0, 2, 1},
            {"Softmax stage 1", 0, 0, 0, 1},
            {"Softmax stage 2", 0, 1, 0, 1},
            {"Softmax stage 3", 1, 2, 3, 0},
            {"FUSED_ATTN stage 1", 2, 1, 2, 1},
            {"LayerNorm stage 1", 0, 1, 0, 0},
            {"LayerNorm stage 2", 3, 3, 2, 0},
            {"LayerNorm stage 3", 2, 0, 3, 1},
            {"LayerNorm stage 4", 1, 3, 1, 2},
            {"LayerNorm stage 5", 1, 3, 4, 1},
            {"Residual Add stage 1", 2, 4, 2, 0},
            {"Residual Add stage 2", 1, 0, 2, 0},
            {"Max pool", 0, 0, 0, 1},
            {"Average pool", 1, 0, 1, 0},
        };
        return t;
    }

    enum class VrfLayout { Spec, Aliased1K };

    inline const char *vrf_layout_name(VrfLayout l) { return l == VrfLayout::Spec ? "spec 3xi64+4xi32 (1.25 KB)" : "aliased 1 KB"; }

    // Does `n` fit layout `l`? `how` receives the configuration (or the reason it does not fit).
    inline bool vrf_fits(const RegNeed &n, VrfLayout l, std::string *how = nullptr)
    {
        if (l == VrfLayout::Aliased1K)
        {
            const bool ok = n.bytes() <= 1024;
            if (how) *how = std::to_string(n.bytes()) + " B of 1024";
            return ok;
        }
        // Spec: enumerate the reconfiguration of i64 reg#0 (i64 | 4 x i16), i64 reg#2 (i64 | 2 x i32), i32 reg#0 (i32 | 4 x i8 | 2 x i16).
        for (int r0 = 0; r0 < 2; r0++)
            for (int r2 = 0; r2 < 2; r2++)
                for (int q0 = 0; q0 < 3; q0++)
                {
                    const int a64 = 1 + (r0 == 0) + (r2 == 0);
                    const int a32 = 3 + (q0 == 0) + (r2 == 1 ? 2 : 0);
                    const int a16 = (r0 == 1 ? 4 : 0) + (q0 == 2 ? 2 : 0);
                    const int a8 = (q0 == 1 ? 4 : 0);
                    if (n.i64 <= a64 && n.i32 <= a32 && n.i16 <= a16 && n.i8 <= a8)
                    {
                        if (how)
                            *how = std::string("i64 reg#0 ") + (r0 ? "as 4 x i16" : "i64") + ", i64 reg#2 " + (r2 ? "as 2 x i32" : "i64") +
                                   ", i32 reg#0 " + (q0 == 0 ? "i32" : q0 == 1 ? "as 4 x i8" : "as 2 x i16");
                        return true;
                    }
                }
        if (how) *how = "no reconfiguration provides the registers";
        return false;
    }
} // namespace has

#endif // HAS_GVU_VRF_H
