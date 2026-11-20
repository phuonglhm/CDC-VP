// sram_limits.h -- capacity of ONE SRAM buffer (A input window, B weights, C partial sums) per neo_lite RTL revision, and the check of a layer's
// tile against it. Pure C++17, no SystemC.
//
//   revisions 0 / 1 (HAS numbers the programs were planned with): A 80 896 B, B 82 944 B, C 24 576 psums (768 rows). Nothing is rejected
//   revision 2 (HAS_NEO_REV=2): A 98 304 B (3072 x 32 B), B 65 536 B (2048 x 32 B), C 10 240 psums (320 rows x 32)
//
// The A limit that takes effect is min(A of the revision, the physical SRAM-A array of the model): the model keeps its SRAM-A at 79 KiB
// (HasNpuTop::kSramaBytes), so a tile above 80 896 B would not fit the array of the MODEL even though the RTL could hold it. This is a
// property of the model, not of the RTL; a plan that stays at or below 80 896 B loses nothing (measured for YOLOv8m: the largest window
// of the revision-2 plan is 78 336 B). The SRAM-C array of the model (1536 rows per half) and the staging areas are NOT changed; the
// check sits outside the arrays.
//
// One layer = one uniform tile grid, so the first tile (channel 0, row 0, column 0) is the largest in every dimension (edge tiles are
// clipped; with PAD_TAIL they are rounded up to at most the first tile's size): the verdict of the layer is the verdict of tile 0.
//   A = cin_t x window rows x window columns (padding included, as the planner's fits() counts it)
//   B = min(cout_t, cout) x cin_t x kh x kw
//   C = x_used x rows x columns of the COMPUTED tile (PAD_TAIL: whole channel tile, width rounded up to Y)
// The same rules, on the register values of a program, are in tools/fe/fe_emit_insts.py (fits, fits_b) and tools/has/hw_check_program.py.
#ifndef HAS_SRAM_LIMITS_H
#define HAS_SRAM_LIMITS_H

#include <algorithm>
#include <cstdint>
#include <string>

#include "has/has_mmio_compat.h"
#include "has/has_tile_iter.h"
#include "has/neo_rev.h"

namespace has
{
    struct SramLimits
    {
        int a_bytes{0}, b_bytes{0}, c_psums{0}, c_rows{0};   // per buffer; c_rows = c_psums / 32 positions of one 32-channel pass
    };

    constexpr int kSramModelABytes = 79 * 1024;              // = HasNpuTop::kSramaBytes (physical array of the model)
    constexpr int kSramRtlABytes = 98304;                    // RTL revision 2: 3072 rows x 32 B

    // Limits of a revision; `model_a_bytes` = the physical SRAM-A array of the model (see above).
    inline SramLimits sram_limits_rev(unsigned rev, int model_a_bytes = kSramModelABytes)
    {
        if (rev < NEO_REV_2) return {80896, 82944, 24576, 768};
        return {std::min(kSramRtlABytes, model_a_bytes), 65536, 10240, 320};
    }
    inline SramLimits sram_limits() { return sram_limits_rev(neo_rev()); }

    struct SramUse { int64_t a{0}, b{0}, c{0}; };

    // Use of one tile (see the header comment for the three terms).
    inline SramUse tile_sram_use(const LayerGeom &g, int cin_t, const TileGeom &t)
    {
        SramUse u;
        u.a = int64_t(cin_t) * int64_t(t.iy1 - t.iy0) * int64_t(t.ix1 - t.ix0);
        u.b = int64_t(std::min(g.cout_t, g.cout)) * int64_t(cin_t) * int64_t(g.kh) * int64_t(g.kw);
        u.c = int64_t(t.x_used) * int64_t(t.oy1 - t.oy0) * int64_t(t.ox1 - t.ox0);
        return u;
    }

    struct SramVerdict
    {
        unsigned mask{0};   // bit 0 A, bit 1 B, bit 2 C over the limit
        SramUse use;
        SramLimits lim;
        bool over() const { return mask != 0; }
        std::string over_str() const
        {
            std::string s;
            if (mask & 1u) s += "A";
            if (mask & 2u) s += (s.empty() ? "B" : ",B");
            if (mask & 4u) s += (s.empty() ? "C" : ",C");
            return s;
        }
    };

    // GEMM_FUSED layer against the limits of revision `rev`. rev < 2: always fine (nothing is checked).
    inline SramVerdict layer_sram_verdict(const LayerInstr &li, unsigned rev, int model_a_bytes = kSramModelABytes)
    {
        SramVerdict v;
        v.lim = sram_limits_rev(rev, model_a_bytes);
        if (rev < NEO_REV_2) return v;
        const LayerGeom &g = li.geom;
        const int cin_t = (li.cin_t > 0 && li.cin_t < li.in_c) ? li.cin_t : li.in_c;
        try
        {
            const TileIter it(g);
            v.use = tile_sram_use(g, cin_t, it.at(0));
        }
        catch (const std::exception &) { return v; }   // not a layer the tile iterator takes: the push checks refuse it elsewhere
        if (v.use.a > v.lim.a_bytes) v.mask |= 1u;
        if (v.use.b > v.lim.b_bytes) v.mask |= 2u;
        if (v.use.c > v.lim.c_psums) v.mask |= 4u;
        return v;
    }

    // Positions per pass of core_dot (FUSED_ATTN products on the core): the activation block cin x w and the 32-channel psum block w x 32
    // must fit; 0 = the product does not fit at all. rev < 2: the arrays of the model (79 KiB, 1536 rows), as before.
    inline uint32_t core_dot_wmax(uint32_t cin, unsigned rev, int model_a_bytes = kSramModelABytes, int model_c_rows = 1536)
    {
        if (cin == 0) return 0;
        const SramLimits lim = sram_limits_rev(rev, model_a_bytes);
        const uint32_t a_cap = rev >= NEO_REV_2 ? uint32_t(lim.a_bytes) : uint32_t(model_a_bytes);
        const uint32_t c_cap = rev >= NEO_REV_2 ? uint32_t(lim.c_rows) : uint32_t(model_c_rows);
        return std::min<uint32_t>(a_cap / cin, c_cap) / 32u * 32u;
    }

    // FUSED_ATTN: does core_dot take both products? Q.K^T has cin = head_dim, A.V has cin = seq_len; weights of one pass are cin x 32 bytes.
    inline bool attn_sram_ok(const LayerInstr &li, unsigned rev, int model_a_bytes = kSramModelABytes)
    {
        if (rev < NEO_REV_2) return true;
        const int b_cap = sram_limits_rev(rev, model_a_bytes).b_bytes;
        for (const uint32_t cin : {uint32_t(std::max(li.head_dim, 0)), uint32_t(std::max(li.seq_len, 0))})
            if (core_dot_wmax(cin, rev, model_a_bytes) == 0 || uint64_t(cin) * 32u > uint64_t(b_cap)) return false;
        return true;
    }
} // namespace has

#endif // HAS_SRAM_LIMITS_H
