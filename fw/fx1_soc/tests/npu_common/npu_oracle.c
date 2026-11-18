/*
 * Independent C oracle (npu_oracle.h). Arithmetic of tools/has/ref_program.py:
 *   round_shift(p, s) = (p + 2^(s-1)) >> s (half up, arithmetic shift), s 0..63
 *   requant(x, S, s, zp) = clamp8(clamp16(round_shift(x * S, s)) + zp)
 *   dequant(x, zp, S, s) = clamp32(round_shift((x - zp) * S, s))
 *   GEMM_FUSED  psum = bias[c] + sum(in * w); q = requant(psum, S[c], s[c], zp_out);
 *               ReLU: max(q, 0); HAS_SKIP: clamp8(q + skip); padding value 0
 *   ADD         out = requant(clamp32(dequant(A) + dequant(B)), SO, sO, zpO)
 *   MAX_POOL    k x k window, stride, padding -128
 *   AVG_POOL    window sum wrapped to int16, out = clamp8(round_shift(sum * SO, sO)), no padding
 */
#include "npu_oracle.h"

static int64_t round_shift(int64_t p, int s)
{
    if (s < 0) s = 0;
    if (s > 63) s = 63;
    const int64_t half = s > 0 ? ((int64_t)1 << (s - 1)) : 0;
    return (p + half) >> s; /* arithmetic shift of a signed value (GCC) */
}
static int64_t clamp(int64_t v, int64_t lo, int64_t hi) { return v < lo ? lo : (v > hi ? hi : v); }
static int8_t requant(int64_t x, int64_t S, int s, int32_t zp)
{
    const int64_t r = clamp(round_shift(x * S, s), -32768, 32767) + zp;
    return (int8_t)clamp(r, -128, 127);
}
static int64_t dequant(int64_t x, int32_t zp, int64_t S, int s)
{
    return clamp(round_shift((x - zp) * S, s), -2147483648LL, 2147483647LL);
}

void npu_oracle_gemm_bank(const int8_t* a, const int8_t* w, uint32_t k, int32_t* c)
{
    for (uint32_t r = 0; r < FX1_NPU_GEMM_ROWS; ++r)
        for (uint32_t col = 0; col < FX1_NPU_GEMM_COLS; ++col) {
            int32_t acc = 0;
            for (uint32_t i = 0; i < k; ++i) acc += (int32_t)a[r * k + i] * (int32_t)w[i * FX1_NPU_GEMM_COLS + col];
            c[r * FX1_NPU_GEMM_COLS + col] = acc;
        }
}

void npu_oracle_conv(const fx1_npu_conv_t* c, const int8_t* in, const int8_t* w, const int32_t* bias, const int32_t* scale,
                     const int32_t* shift, const int8_t* skip, int8_t* out)
{
    const uint32_t oh = fx1_npu_conv_oh(c), ow = fx1_npu_conv_ow(c), taps = c->kh * c->kw;
    for (uint32_t co = 0; co < c->cout; ++co)
        for (uint32_t oy = 0; oy < oh; ++oy)
            for (uint32_t ox = 0; ox < ow; ++ox) {
                int64_t acc = bias[co];
                for (uint32_t ci = 0; ci < c->cin; ++ci)
                    for (uint32_t i = 0; i < c->kh; ++i)
                        for (uint32_t j = 0; j < c->kw; ++j) {
                            const int32_t iy = (int32_t)(oy * c->stride + i) - (int32_t)c->pad;
                            const int32_t ix = (int32_t)(ox * c->stride + j) - (int32_t)c->pad;
                            if (iy < 0 || ix < 0 || iy >= (int32_t)c->ih || ix >= (int32_t)c->iw) continue; /* pad 0 */
                            const int8_t x = in[(ci * c->ih + (uint32_t)iy) * c->iw + (uint32_t)ix];
                            /* weights [cin][kh][kw][cout] */
                            const int8_t wt = w[(ci * taps + i * c->kw + j) * c->cout + co];
                            acc += (int64_t)x * wt;
                        }
                int s = shift[co];
                s = s < 0 ? 0 : (s > 63 ? 63 : s);
                int8_t q = requant(acc, scale[co], s, c->zp_out);
                if (c->act == FX1_NPU_ACT_RELU && q < 0) q = 0;
                const uint32_t idx = (co * oh + oy) * ow + ox;
                if (c->skip) q = (int8_t)clamp((int64_t)q + skip[idx], -128, 127);
                out[idx] = q;
            }
}

void npu_oracle_pool(const fx1_npu_pool_t* p, const int8_t* in, int8_t* out)
{
    const uint32_t oh = fx1_npu_pool_oh(p), ow = fx1_npu_pool_ow(p);
    for (uint32_t ch = 0; ch < p->c; ++ch)
        for (uint32_t y = 0; y < oh; ++y)
            for (uint32_t x = 0; x < ow; ++x) {
                int64_t m = -128, sum = 0;
                for (uint32_t dy = 0; dy < p->k; ++dy)
                    for (uint32_t dx = 0; dx < p->k; ++dx) {
                        const int32_t iy = (int32_t)(y * p->stride + dy) - (int32_t)p->pad;
                        const int32_t ix = (int32_t)(x * p->stride + dx) - (int32_t)p->pad;
                        int64_t v = -128; /* MAX_POOL padding */
                        if (iy >= 0 && ix >= 0 && iy < (int32_t)p->h && ix < (int32_t)p->w)
                            v = in[(ch * p->h + (uint32_t)iy) * p->w + (uint32_t)ix];
                        if (v > m) m = v;
                        sum += v;
                    }
                int64_t r;
                if (p->avg) {
                    const int64_t wrapped = ((sum + 32768) & 0xFFFF) - 32768;
                    r = clamp(round_shift(wrapped * p->so, p->sho), -128, 127);
                } else {
                    r = m;
                }
                out[(ch * oh + y) * ow + x] = (int8_t)r;
            }
}

void npu_oracle_add(const fx1_npu_add_t* a, const int8_t* x, const int8_t* y, int8_t* out)
{
    for (uint32_t i = 0; i < a->n; ++i) {
        const int64_t s = clamp(dequant(x[i], a->zp_a, a->sa, a->sha) + dequant(y[i], a->zp_b, a->sb, a->shb),
                                -2147483648LL, 2147483647LL);
        out[i] = requant(s, a->so, a->sho, a->zp_o);
    }
}
