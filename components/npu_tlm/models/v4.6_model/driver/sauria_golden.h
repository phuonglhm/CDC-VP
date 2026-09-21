// sauria_golden — reference convolution/GeMM in pure C (no Python, no torch), so a
// test can be generated AND checked entirely on the target: the expected output of
// a layer is computed here instead of by SAURIA's torch reference.
//
// Semantics match SAURIA get_ideal_results (cross-correlation, torch.nn.Conv2d):
//   out[k][oy][ox] = (preload ? Cpre[k][oy][ox] : 0)
//                  + sum_{c,kh,kw} A[c][oy*s+kh*d][ox*s+kw*d] * B[k][c][kh][kw]
// INT (op_type=0): exact integer accumulate (int64) -> bit-exact golden.
// FP  (op_type=1): accumulate in float32 (as torch does), round to fp16 ONCE ->
//                  golden within a small ULP of the reference (FP add is not
//                  associative; consistent with the model's ULP-tolerant verify).
//
// Output is returned in NATURAL [C_out][C_h][C_w] row-major order (the logical
// output tensor). For 1x1 / GeMM this is exactly the SAURIA gold_dram order.

#ifndef SAURIA_GOLDEN_H
#define SAURIA_GOLDEN_H

#include <cstdint>
#include <vector>
#include "fp16.h"
#include "sauria_targets.h"

namespace sauria
{
    struct SauriaConvShape
    {
        int B_w, B_h, d, s;
        int C_in, C_w, C_h, C_out; // full output dims; C_in = input channels
        int preload_en;
        int A_h, A_w;              // input spatial (derived; 0 => compute from output)
    };

    inline void sauria_derive_input_dims(SauriaConvShape &sh)
    {
        int Bw_eff = 1 + (sh.B_w - 1) * sh.d, Bh_eff = 1 + (sh.B_h - 1) * sh.d;
        sh.A_w = (1 + sh.s * (sh.C_w - 1)) + Bw_eff - 1;
        sh.A_h = (1 + sh.s * (sh.C_h - 1)) + Bh_eff - 1;
    }

    // A: [C_in][A_h][A_w], B: [C_out][C_in][B_h][B_w], Cpre: [C_out][C_h][C_w] — all
    // row-major doubles (decoded element values). Returns [C_out][C_h][C_w].
    inline std::vector<double> sauria_reference_conv(
        const double *A, const double *B, const double *Cpre,
        const SauriaConvShape &sh, const SauriaTarget &t)
    {
        const bool fp = (t.op_type == 1);
        const int Cin = sh.C_in, Cout = sh.C_out, Ch = sh.C_h, Cw = sh.C_w;
        const int Bh = sh.B_h, Bw = sh.B_w, s = sh.s, d = sh.d, Aw = sh.A_w;
        std::vector<double> out((size_t)Cout * Ch * Cw, 0.0);

        for (int k = 0; k < Cout; k++)
            for (int oy = 0; oy < Ch; oy++)
                for (int ox = 0; ox < Cw; ox++)
                {
                    size_t oidx = ((size_t)k * Ch + oy) * Cw + ox;
                    double pre = sh.preload_en ? Cpre[oidx] : 0.0;
                    if (fp)
                    {
                        float acc = (float)pre; // float32 accumulate, like torch
                        for (int c = 0; c < Cin; c++)
                            for (int kh = 0; kh < Bh; kh++)
                                for (int kw = 0; kw < Bw; kw++)
                                {
                                    int iy = oy * s + kh * d, ix = ox * s + kw * d;
                                    double a = A[((size_t)c * sh.A_h + iy) * Aw + ix];
                                    double w = B[(((size_t)k * Cin + c) * Bh + kh) * Bw + kw];
                                    acc += (float)a * (float)w;
                                }
                        // round to fp16 once (value already a half after this)
                        out[oidx] = (double)fp16_t::half_to_float(fp16_t::float_to_half(acc));
                    }
                    else
                    {
                        int64_t acc = (int64_t)pre;
                        for (int c = 0; c < Cin; c++)
                            for (int kh = 0; kh < Bh; kh++)
                                for (int kw = 0; kw < Bw; kw++)
                                {
                                    int iy = oy * s + kh * d, ix = ox * s + kw * d;
                                    int64_t a = (int64_t)A[((size_t)c * sh.A_h + iy) * Aw + ix];
                                    int64_t w = (int64_t)B[(((size_t)k * Cin + c) * Bh + kh) * Bw + kw];
                                    acc += a * w;
                                }
                        out[oidx] = (double)acc;
                    }
                }
        return out;
    }

} // namespace sauria

#endif // SAURIA_GOLDEN_H
