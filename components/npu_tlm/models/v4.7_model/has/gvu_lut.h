// gvu_lut.h -- LUT unit of the HW drawing "LUT": LUT_direct (act / exp, in gvu_quant.h) and LUT_indirect (recip / rsqrt).
// Spec: has/HAS_IFACE_RCE.md §2. Interpretation of the drawing (the bit widths all agree with it; open points R1-R4 there):
//   E    = 31 - clz(x)                       exponent = MSB position (v32int5 "E_shift" output)
//   y    = x << (31 - E)                     mantissa left-aligned (bits below the MSB; zero-filled when E is small)
//   idx  = y[30:23]  (8 bits)                recip index;  rsqrt index = {E[0], y[30:23]} (9 bits)
//   frac = y[22:15]  (8 bits)                interpolation fraction
//   r    = T[idx] + (((T[idx+1] - T[idx]) * frac) >> 8)      delta fits int9, product int17, ">> 8" as drawn (floor)
//   recip(x) ~= r * 2^-(Q + E)               rsqrt(x) ~= r * 2^-(Q + floor(E/2))     with table format Q14 (default)
// Tables (loaded by the host, HAS §9.1 "LUT_recip, LUT_rsqrt"): recip T[i] = round(2^Q / (1 + i/256)), i = 0..256;
// rsqrt T[p*257 + i] = round(2^Q / sqrt((1 + i/256) * 2^p)), p = E[0] in {0,1}, i = 0..256.
#ifndef HAS_LUT_H
#define HAS_LUT_H

#include <cmath>
#include <cstdint>
#include <vector>

namespace has
{
    struct LutResult
    {
        int16_t r;     // mantissa, table format Q
        int e_shift;   // E (recip) or floor(E/2) (rsqrt)
        bool zero_in;  // x == 0 (undefined input; r saturated)
    };

    class LutIndirect
    {
    public:
        static constexpr int IDX_BITS = 8, FRAC_BITS = 8, N = (1 << IDX_BITS) + 1; // 257 entries per segment

        explicit LutIndirect(int q = 14) : q_(q), recip_(N), rsqrt_(2 * N)
        {
            for (int i = 0; i < N; i++)
            {
                const double m = 1.0 + static_cast<double>(i) / (1 << IDX_BITS);
                recip_[i] = static_cast<int16_t>(std::lround(std::ldexp(1.0 / m, q_)));
                for (int p = 0; p < 2; p++)
                    rsqrt_[p * N + i] = static_cast<int16_t>(std::lround(std::ldexp(1.0 / std::sqrt(m * (1 << p)), q_)));
            }
        }

        int q() const { return q_; }
        const std::vector<int16_t> &recip_table() const { return recip_; }
        const std::vector<int16_t> &rsqrt_table() const { return rsqrt_; }

        LutResult recip(uint32_t x) const { return lookup(x, false); }
        LutResult rsqrt(uint32_t x) const { return lookup(x, true); }

        // Real value represented by a result (for accuracy evaluation only)
        double value(const LutResult &res) const { return std::ldexp(static_cast<double>(res.r), -(q_ + res.e_shift)); }

    private:
        int q_;
        std::vector<int16_t> recip_, rsqrt_;

        static int msb(uint32_t x) { return 31 - __builtin_clz(x); }

        LutResult lookup(uint32_t x, bool is_rsqrt) const
        {
            if (x == 0)
                return {INT16_MAX, 0, true};
            const int E = msb(x);
            const uint32_t y = x << (31 - E);
            const int idx = static_cast<int>((y >> (31 - IDX_BITS)) & ((1u << IDX_BITS) - 1));
            const int frac = static_cast<int>((y >> (31 - IDX_BITS - FRAC_BITS)) & ((1u << FRAC_BITS) - 1));
            const int16_t *T = is_rsqrt ? &rsqrt_[(E & 1) * N] : recip_.data();
            const int delta = T[idx + 1] - T[idx];                  // int9 per the drawing
            const int r = T[idx] + ((delta * frac) >> FRAC_BITS);   // int17 product, arithmetic >> 8
            return {static_cast<int16_t>(r), is_rsqrt ? (E >> 1) : E, false};
        }
    };
    // PWL indexing mode of the LUT (vector-unit specification section 5.8, figure 12 lower path). Base, W and E_shift come from the
    // instruction; the host loads a 513-entry table (n_idx + 1). Golden: tools/fe/fe_ref_gvu.py pwl_index / lut_pwl.
    //   form SEG (default, as drawn): E = msb(x), y = x << (31-E); seg = E - Base; idx = seg << W | y[30:31-W];
    //                                  frac = y[30-W:23-W];  E < Base -> (0, 0)
    //   form UNIFORM (alternative):   off = x - Base; idx = off >> W; frac = 8 bits below; off < 0 -> (0, 0)
    //   idx >= n_idx -> (n_idx-1, 255);  r = T[idx] + ((T[idx+1] - T[idx]) * frac >> 8)  (same int9 interpolation, floor)
    class LutPwl
    {
    public:
        enum Form { SEG = 0, UNIFORM = 1 };
        LutPwl(const std::vector<int32_t> &table, int base, int w, int e_shift, Form form = SEG)
            : T_(table), base_(base), w_(w), e_shift_(e_shift), form_(form) {}

        LutResult recip(uint32_t x) const
        {
            int idx, frac;
            index(x, idx, frac);
            const int delta = T_[size_t(idx) + 1] - T_[size_t(idx)];
            return {static_cast<int16_t>(T_[size_t(idx)] + ((delta * frac) >> 8)), e_shift_, x == 0};
        }
        void index(uint32_t x, int &idx, int &frac) const
        {
            const int n_idx = static_cast<int>(T_.size()) - 1;
            idx = 0; frac = 0;
            if (form_ == SEG)
            {
                if (x == 0) return;
                const int E = 31 - __builtin_clz(x);
                const uint32_t y = x << (31 - E);
                const int seg = E - base_;
                if (seg < 0) return;
                idx = (seg << w_) | (w_ ? static_cast<int>((y >> (31 - w_)) & ((1u << w_) - 1)) : 0);
                frac = static_cast<int>((y >> (23 - w_)) & 0xFF);
            }
            else
            {
                const int64_t off = int64_t(x) - base_;
                if (off < 0) return;
                idx = static_cast<int>(off >> w_);
                frac = static_cast<int>((w_ >= 8 ? (off >> (w_ - 8)) : (off << (8 - w_))) & 0xFF);
            }
            if (idx >= n_idx) { idx = n_idx - 1; frac = 255; }
        }

    private:
        std::vector<int32_t> T_;
        int base_, w_, e_shift_;
        Form form_;
    };
} // namespace has

#endif // HAS_LUT_H
