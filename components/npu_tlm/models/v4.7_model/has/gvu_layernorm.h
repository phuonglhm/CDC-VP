// gvu_layernorm.h -- GVU LAYERNORM (vector-unit specification section 5.6, figure 8), one row of H int8 values:
//   stage 1  S = sum x_k (int32, row_sum -> Scratchpad vbank#1)
//   stage 2  D_k = H*x_k - S (int32, stored in vbank#2) ; V = sum D_k^2 (int64) ; v = Requant(V >> Pre_Shift, M0_var, TS_var, Z_var)
//   stage 3  u = RoundShift((v - Z_var)*M0_7 + E_bias, TS_7) ; f = clamp8(u + Z_7) ;
//            w = f - Z_7 if f > Z_7 else floor value (R8: Z_out3 = r8_z_out | 1)
//   stage 4  (r, E) = LUT_rsqrt(w) ; n_k = clamp8(RoundShift(r*D_k*M0_div, TS_div + E) + Z_div)   (R9: exact | MSB part only)
//   stage 5  t_k = clamp(RoundShift((n_k - Z_div)*gamma_k*M0_mul_k, TS_mul_k) + Z_mul) ; y_k = clamp(t_k - Z_mul + beta_k + Z_out)
//            (gamma int8 / beta int32 from Scratchpad sbank; clamps int8, or int16 with out_int16 -- H13 knob)
// Bit-exact with tools/fe/fe_ref_gvu.py layernorm_row (tools/has/tb_gvu_layernorm.cpp). Note: stages 2/5 requant WITHOUT the
// int16 narrowing of has::requant (the drawing's LN requant clamps once), and shifts may exceed 63 (i128 path).
#ifndef HAS_LAYERNORM_H
#define HAS_LAYERNORM_H

#include <algorithm>
#include <cstdint>
#include <vector>
#include "has/gvu_lut.h"
#include "has/gvu_quant.h"

namespace has
{
    struct LnParams
    {
        int H{0}, Pre_Shift{0};
        int64_t M0_var{1}, M0_7{1}, M0_div{1}, E_bias{0};
        int TS_var{0}, TS_7{0}, TS_div{0};
        int Z_var{-128}, Z_7{-128}, Z_div{0}, Z_mul{0}, Z_out{0};
        std::vector<int> gamma, TS_mul;
        std::vector<int64_t> beta, M0_mul;
        bool r8_floor_z_out{true};
        int r8_z_out{1};
        bool r9_exact{true};
        int r9_lsb_bits{32};
        bool out_int16{false};
    };

    struct LnRow
    {
        int64_t S{0}, V{0};
        int v{0}, f{0}, w{0}, r{0}, E{0};
        std::vector<int> n, y;
    };

    // Round-shift of an i128 with any shift in [0, 126] (rshift() is limited to 63).
    inline i128 rshift_wide(i128 p, int s, int mode)
    {
        if (s <= 63) return rshift(p, s, mode);
        if (mode == FLOOR) return p >> s;
        const i128 half = static_cast<i128>(1) << (s - 1);
        if (mode == HALF_UP) return (p + half) >> s;
        if (mode == HALF_AWAY) { const i128 a = p < 0 ? -p : p; const i128 q = (a + half) >> s; return p < 0 ? -q : q; }
        i128 q = p >> s;
        const i128 rem = p - (q << s);
        if (rem > half || (rem == half && (q & 1))) q += 1;
        return q;
    }

    inline int requant_clamp(i128 x, int64_t S, int s, int zp, int lo, int hi, int mode)
    {
        const i128 r = rshift_wide(x * S, s, mode) + zp;
        return static_cast<int>(clamp128(r, lo, hi));
    }

    inline LnRow layernorm_row(const std::vector<int> &x, const LnParams &p, const LutIndirect &lut, const Knobs &k)
    {
        LnRow o;
        const int H = p.H;
        for (int v : x) o.S += v;
        std::vector<int64_t> D(x.size());
        for (size_t j = 0; j < x.size(); j++) { D[j] = int64_t(H) * x[j] - o.S; o.V += D[j] * D[j]; }
        o.v = requant_clamp(static_cast<i128>(o.V >> p.Pre_Shift), p.M0_var, p.TS_var, p.Z_var, -128, 127, k.round_mode);
        const i128 u = rshift_wide(static_cast<i128>(o.v - p.Z_var) * p.M0_7 + p.E_bias, p.TS_7, k.round_mode);
        o.f = static_cast<int>(clamp128(u + p.Z_7, -128, 127));
        o.w = o.f > p.Z_7 ? o.f - p.Z_7 : (p.r8_floor_z_out ? p.r8_z_out : 1);
        const LutResult lr = lut.rsqrt(uint32_t(o.w));
        o.r = lr.r;
        o.E = lr.e_shift;
        const int lo = p.out_int16 ? -32768 : -128, hi = p.out_int16 ? 32767 : 127;
        o.n.resize(x.size());
        o.y.resize(x.size());
        for (size_t j = 0; j < x.size(); j++)
        {
            const i128 prod = static_cast<i128>(o.r) * D[j];
            const i128 acc = p.r9_exact ? prod * p.M0_div : ((prod >> p.r9_lsb_bits) * p.M0_div) << p.r9_lsb_bits;
            o.n[j] = static_cast<int>(clamp128(rshift_wide(acc, p.TS_div + o.E, k.round_mode) + p.Z_div, -128, 127));
        }
        for (size_t j = 0; j < x.size(); j++)
        {
            const int t = requant_clamp(static_cast<i128>(o.n[j] - p.Z_div) * p.gamma[j], p.M0_mul[j], p.TS_mul[j], p.Z_mul, lo, hi,
                                        k.round_mode);
            o.y[j] = static_cast<int>(clamp128(static_cast<i128>(t) - p.Z_mul + p.beta[j] + p.Z_out, lo, hi));
        }
        return o;
    }
} // namespace has

#endif // HAS_LAYERNORM_H
