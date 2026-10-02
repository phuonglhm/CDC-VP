// gvu_softmax.h -- GVU SOFTMAX (vector-unit specification section 5.4, figure 6), 3 stages per row:
//   stage 1  m = max_j x_j                                         (Comparator v32int8, row_max -> Scratchpad vbank#1)
//   stage 2  d_j = m - x_j (unsigned 0..255, H4) ; e_j = LUT_exp[d_j] (256 x 1 B, unsigned, R6) ; sigma = sum e_j (int32)
//   stage 3  (r, E) = LUT_recip(sigma)   log-scale: has::LutIndirect | PWL: has::LutPwl, E = E_shift of the instruction
//            p_j = r * e_j ; t_j = RoundShift(p_j, E + Q - P) (P = 15, R5) ; a_j = Requant(t_j, ATTN_Scale, ATTN_Shift, Z_ATTN)
// Bit-exact with tools/fe/fe_ref_gvu.py softmax_row at the default knobs (checked by tools/has/tb_gvu_softmax.cpp).
// Timing: cycles() is an ESTIMATE from the drawing's dashed pipeline cuts (32 rows in parallel lanes, one element per cycle).
#ifndef HAS_SOFTMAX_H
#define HAS_SOFTMAX_H

#include <algorithm>
#include <cstdint>
#include <vector>
#include "has/gvu_lut.h"
#include "has/gvu_quant.h"

namespace has
{
    struct SoftmaxCfg
    {
        const int32_t *exp{nullptr};           // LUT_exp, 256 entries (u8 values)
        const LutIndirect *log_recip{nullptr}; // log-scale mode when set
        const LutPwl *pwl_recip{nullptr};      // PWL mode otherwise
        int64_t S{1 << 30};                    // ATTN_Scale
        int s{37};                             // ATTN_Shift
        int zp{-128};                          // Z_ATTN
        int P{15};                             // R5
        int q{14};                             // table format Q (R2)
        bool sub_sat{false};                   // H4: false = unsigned 0..255, true = saturate to 127
    };

    struct SoftmaxRow
    {
        int m{0};
        int64_t sigma{0};
        int r{0}, E{0};
        std::vector<int8_t> a;
    };

    // zero[j] != 0 forces e_j = 0 (FUSED_ATTN masked position, knob mask_e = zero; H22).
    inline SoftmaxRow softmax_row(const std::vector<int> &x, const SoftmaxCfg &c, const Knobs &k,
                                  const std::vector<uint8_t> *zero = nullptr, QuantCounters *qc = nullptr)
    {
        SoftmaxRow o;
        o.m = *std::max_element(x.begin(), x.end());
        std::vector<int32_t> e(x.size());
        for (size_t j = 0; j < x.size(); j++)
        {
            int d = o.m - x[j];
            if (c.sub_sat) d = std::min(d, 127);
            e[j] = (zero && (*zero)[j]) ? 0 : c.exp[d];
            o.sigma += e[j];
        }
        const LutResult lr = c.log_recip ? c.log_recip->recip(uint32_t(o.sigma)) : c.pwl_recip->recip(uint32_t(o.sigma));
        o.r = lr.r;
        o.E = lr.e_shift;
        const int sh = o.E + c.q - c.P;
        o.a.resize(x.size());
        for (size_t j = 0; j < x.size(); j++)
        {
            const i128 p = static_cast<i128>(o.r) * e[j];
            const i128 t = sh <= 0 ? (p << -sh) : rshift(p, sh, k.round_mode);
            o.a[j] = requant(static_cast<int64_t>(t), c.S, c.s, c.zp, k, qc);
        }
        return o;
    }

    // ESTIMATE: stage 1 = L + 3, stage 2 = L + 5, stage 3 = L + 18 (LUT_recip 4 cuts + Mul + Round-Shift + Requant 9 + read/write)
    inline uint64_t softmax_cycles(uint64_t L) { return (L + 3) + (L + 5) + (L + 18); }
} // namespace has

#endif // HAS_SOFTMAX_H
