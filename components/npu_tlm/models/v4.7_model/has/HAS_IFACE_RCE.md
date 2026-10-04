# HAS_IFACE_RCE -- specification of the transformer blocks: indirect LUT, SOFTMAX, LAYERNORM, FUSED_ATTN

Conventions, knobs, `rshift`, `requant`, `dequant` and the direct LUT are those of `has/HAS_IFACE.md`. The semantics come
from the vector-unit specification (sections 5.4 to 5.8, figures 6 to 8 and 12). The Python golden
(`tools/fe/fe_ref_gvu.py`, `tools/fe/fe_ref_has_rce.py`) and the SystemC blocks (`has/gvu_lut.h`, `has/gvu_softmax.h`,
`has/gvu_layernorm.h`, `has/gvu_fused_attn.h`) implement this file independently and agree bit for bit
(`docs/VERIFICATION_REPORT.md`, V17). Points the drawings leave open are knobs with documented defaults
(`docs/KNOWN_LIMITATIONS.md`, section 7.1).

## 1. Status

| Block | Status |
|---|---|
| Indirect LUT, logarithmic mode (section 2) | Implemented; every bit width of the drawing agrees with one reading only |
| Indirect LUT, piecewise-linear mode (section 2.1) | Implemented (segment form as drawn, uniform form as an alternative) |
| SOFTMAX (section 3) | Implemented; open points R5 to R7 and H4 are knobs |
| LAYERNORM (section 4) | Implemented; open points R8 to R10 and H13 are knobs |
| FUSED_ATTN (section 5) | Implemented; open points H7, H22 to H24 are knobs or counters |

## 2. Indirect LUT (reciprocal, reciprocal square root)

Input `x` uint32 > 0 (`x = 0`: saturated result, flag `zero_in`).
```
E    = 31 - clz(x)                       # exponent, E_shift output (v32int5)
y    = x << (31 - E)                     # left-aligned mantissa
idx  = y[30:23]                          # reciprocal: 8 bits | rsqrt: {E[0], y[30:23]}, 9 bits
frac = y[22:15]                          # 8 bits
r    = T[idx] + (((T[idx+1] - T[idx]) * frac) >> 8)     # floor (question R3)
reciprocal: value = r * 2^-(Q + E)             E_shift out = E
rsqrt:      value = r * 2^-(Q + floor(E/2))    E_shift out = E >> 1
Tables (loaded by the host, Q = 14, question R2):
  reciprocal T[i] = round(2^Q / (1 + i/256)), i = 0..256
  rsqrt      T[p*257 + i] = round(2^Q / sqrt((1 + i/256) * 2^p)), p in {0, 1}
```
Measured (`tools/has/tb_gvu_lut`): largest table difference 64 (fits int9), largest product 16,320 (fits int17); largest
relative error 1.69e-4 (reciprocal) and 1.74e-4 (rsqrt) over 3,048,575 inputs, about the Q14 resolution. Without the
interpolation the error is about 23 times larger.

### 2.1 Piecewise-linear mode

`Base`, `W` and `E_shift` come from the instruction; the table has `n_idx + 1` entries.
```
SEG (as drawn):   E = msb(x), y = x << (31 - E); seg = E - Base; idx = (seg << W) | y[30:31-W];
                  frac = y[30-W:23-W]; E < Base -> (0, 0)
UNIFORM:          off = x - Base; idx = off >> W; frac = the 8 bits below; off < 0 -> (0, 0)
idx >= n_idx -> (n_idx - 1, 255); r = T[idx] + (((T[idx+1] - T[idx]) * frac) >> 8)
```

## 3. SOFTMAX (three stages per row; 32 lanes = 32 rows in parallel, one column per cycle)

```
stage 1:  m = max_j x_j                                  # x int8
stage 2:  d_j = m - x_j                                  # unsigned 0..255 (question H4; knob: saturate at 127)
          e_j = LUT_exp[d_j]                             # 256 entries, unsigned 8-bit values (question R6)
          sigma = sum_j e_j                              # int32
stage 3:  (r, E) = LUT_recip(sigma)                      # logarithmic mode, or PWL mode with E = E_shift (question R7)
          p_j = r * e_j
          t_j = rshift(p_j, E + Q - P, ROUND_MODE)       # P = 15 (question R5); a negative amount is a left shift
          a_j = requant(t_j, ATTN_Scale, ATTN_Shift, Z_ATTN)   # int8 (defaults S = 2^30, s = 37, Z = -128)
```
Timing (ESTIMATED): `(L + 3) + (L + 5) + (L + 18)` cycles per group of 32 rows.

## 4. LAYERNORM (five stages per row of H int8 values)

```
stage 1:  S = sum_k x_k                                  # int32
stage 2:  D_k = H * x_k - S                              # int32, kept in the scratchpad
          V = sum_k D_k^2                                # int64
          v = requant_clamp(V >> Pre_Shift, M0_var, TS_var, Z_var)          # int8
stage 3:  u = rshift((v - Z_var) * M0_7 + E_bias, TS_7);  f = sat8(u + Z_7)
          w = f - Z_7 if f > Z_7, otherwise the floor value (question R8; default the stage-3 zero point r8_z_out = 1)
stage 4:  (r, E) = LUT_rsqrt(w)
          n_k = sat8(rshift(r * D_k * M0_div, TS_div + E) + Z_div)          # exact product (question R9; knob: high part only)
stage 5:  t_k = sat(rshift((n_k - Z_div) * gamma_k * M0_mul_k, TS_mul_k) + Z_mul)
          y_k = sat(t_k - Z_mul + beta_k + Z_out)        # int8, or int16 with the 16-bit output option (question H13)
```
`requant_clamp` rounds and clamps once, without the int16 narrowing of section 4 of `HAS_IFACE.md`; shifts may exceed 63.
`gamma` (int8) and `beta` (int32) are per column (question R10). Timing (ESTIMATED): `4H + 48` cycles per group of 32 rows.

## 5. FUSED_ATTN (one head, three phases)

```
phase 1:  acc_ij = Q_i . K_j - Zk * Qrow_i - Zq * Kcol_j + d * Zq * Zk
          x_ij = requant(acc_ij, Mqk, TS_qk, Zqk)        # one requant; 1/sqrt(d) is folded into Mqk (question H7)
          x_ij = -128 where the mask is -128             # the row maximum runs while the output-channel tiles stream
phase 2:  softmax stages 2 and 3 (section 3); e forced to 0 on masked positions only with mask_e_zero (question H22)
phase 3:  acc_in = A_i . V_n - Za * Vcol_n [ - Zv * Arow_i + L * Za * Zv only with av_zv_corr (question H23) ]
          O_in = requant(acc_in, Mav, TS_av, Zav)
```
`Qrow`, `Kcol` and `Vcol` are v32int16 accumulators in the drawing; overflows are counted (question H24), not modelled.
The two matrix products run on the systolic array; in the model they run on the cycle-accurate core
(`HasNpuTop::rce_core`) or in software with identical results.

## 6. Parameters and vectors

The parameter blocks of the FUSED_ATTN and LAYERNORM instructions are in `docs/SW_INTEGRATION_GUIDE.md`, section 6.2
(`has/gvu_rce_params.h`). Unit-test vectors: `$FE_WORK/has/vectors/gvu/` (`attn_<name>.txt` with sections Q K V M X S A O;
`ln_<case>.txt` with rows X R N Y), read by `tools/has/tb_gvu_fused_attn` and `tools/has/tb_gvu_layernorm`.
