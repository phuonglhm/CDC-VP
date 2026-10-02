# HAS_IFACE -- common arithmetic specification of the `has/` blocks (Python golden and SystemC)

This file is the specification from which the Python golden (`tools/fe/fe_ref_has.py`, `tools/fe/fe_has_vectors.py`)
and the SystemC blocks (`has/gvu_quant.h`, `has/gvu_obp.h`, `has/gvu_elemwise.h`) were written **independently of each
other**. Neither side copies code from the other; agreement between them is the unit-level validation
(`docs/VERIFICATION_REPORT.md`, section 1). The semantics come from the hardware drawings QUANTIZE, LUT, GEMM_FUSED and
ELEM_WISE. The transformer blocks are specified in `has/HAS_IFACE_RCE.md`.

## 1. Conventions

- Vector = **32 elements** (`W32`), one vector per cycle in every pipeline. Element types as drawn: int8, int16, int32, int64.
- Intermediate arithmetic in this specification is **unbounded integer**, followed by the narrowing that is written out.
  Python uses `int` / `np.int64` with overflow checks; C++ uses `__int128` for products and narrows explicitly. There is
  no implicit narrowing.
- `sat(x, lo, hi)` = clamp; `wrap16(x)` = the signed low 16 bits.

## 2. Knobs (same names in Python and C++)

Each knob stands for a point the drawings leave open (`docs/KNOWN_LIMITATIONS.md`, section 7).

| Knob | Values | HAS default | v4.5 compatibility |
|---|---|---|---|
| `ROUND_MODE` | 0 `FLOOR`, 1 `HALF_UP`, 2 `HALF_AWAY`, 3 `HALF_EVEN` | 1 `HALF_UP` | 0 `FLOOR` |
| `REQ_NARROW` | 0 `SAT16`, 1 `WRAP16`, 2 `NONE` (keep int64 until the clamp) | 0 `SAT16` | 0 `SAT16` |
| `DEQ_ZP_ORDER` | 0 `SUB_BEFORE` `(x - zp) * S`, 1 `ADD_AFTER` `x * S ... + zp` | 0 | 0 |
| `SCALE_FMT` | 0 `I32` (S in [-2^31, 2^31)), 1 `U32` (S in [0, 2^32)) | 0 `I32` | 1 `U32` |
| `MP_MODE` | 0 `DIRECT` (K x K window in one pass), 1 `SEPARABLE` (1 x K then K x 1) | 0 | 0 |
| `SCRATCH_BYTES` | integer | 24,576 | 24,576 |

## 3. Round-shift (shared)

`rshift(p, s, mode)` with integer `p` and `s` in [0, 63] (s < 0 is an error; question H1 if the hardware allows a left shift):
- `s == 0`: `p`.
- `FLOOR`: `p >> s` (arithmetic shift).
- `HALF_UP`: `(p + 2^(s-1)) >> s` with an arithmetic shift, i.e. ties round towards +infinity (-2.5 -> -2).
- `HALF_AWAY`: `sign(p) * ((|p| + 2^(s-1)) >> s)`.
- `HALF_EVEN`: round to nearest, ties to even.

## 4. QUANTIZE

**Requant** (drawing QUANTIZE, top row). Inputs: `x` int32 (int64 in ELEM_WISE stage 2 when the sum is int32), `S`
(per `SCALE_FMT`), `s` int8 in [0, 63], `zp` int16:
```
p   = x * S                                   # v32int64; |p| >= 2^63 counts OVF64 (must never happen)
r   = rshift(p, s, ROUND_MODE)
n   = SAT16: sat(r, -32768, 32767) | WRAP16: wrap16(r) | NONE: r
y   = n + zp                                  # zp == 0 skips the addition ("zero point skip" path)
out = sat(y, -128, 127)                       # int8
```
Both sides keep the counters `n_sat16` (elements saturated or wrapped at step n; always 0 with `NONE`) and `n_clamp8`
(elements clamped at step out); they are the evidence for question H2.

**Dequant** (bottom row). Inputs: `x` int8, `zp` int16, `S`, `s`:
```
SUB_BEFORE:  out = sat(rshift((x - zp) * S, s, ROUND_MODE), -2^31, 2^31 - 1)
ADD_AFTER:   out = sat(rshift(x * S, s, ROUND_MODE) + zp, -2^31, 2^31 - 1)
```

**v4.5 compatibility**: with `FLOOR`, `U32`, `SAT16` and zp = 0, `out = sat8((psum_plus_bias * scale) >> shift)`, identical
to `fe_ref_int8.requant()` (clamping to int16 and then to int8 equals clamping directly to int8).

## 5. LUT

- **Direct** (activation, exponent): `out = LUT[x + 128]`, `LUT` int8[256]. One table for all 32 lanes.
- **Indirect** (reciprocal, reciprocal square root): `has/HAS_IFACE_RCE.md`, section 2.

## 6. Blocks and interfaces

### 6.1 `gvu_obp` -- GEMM_FUSED epilogue (array -> PSM -> **Requant -> LUT_act -> write PSUM SRAM**)
- **SystemC ports as the v4.5 `Obp`** so that a testbench can swap it in: `i_data` (`psum_vector_t<32,int32_t>`), `i_addr`
  (vector id = `ctx * nch + x`), `i_wmask`, `i_valid` -> `o_sramc_wdata` (`psum_vector_t<32,int32_t>`, the int8 value
  sign-extended), `o_sramc_addr`, `o_sramc_wren`, `o_sramc_wmask`, `o_valid`; `i_lut_en`. There is **no** bias or residual
  input in use: the bias is the PSUM preload and the residual is an ELEM_WISE step. The `i_residual` port is kept for
  pin compatibility and ignored; `i_residual_en = 1` is reported as an error.
- **Channel** of a vector: `x = i_addr % NCH` (register `NCH`, written by the host). No implicit channel counter, so no idle
  cycles are needed between contexts.
- **Host registers** (v4.5 addresses kept): `LUT_BASE 0x140000` (int8[256]), `SCALE_BASE 0x180000 + 4 * ch` (S per channel,
  per `SCALE_FMT`), `SHIFT_BASE 0x190000 + 4 * ch` (s), plus `ZP_OUT 0x1A0000`, `NCH 0x1A0004`. In the drawing S and s sit
  in the scratchpad, so every read of S / s is counted as a scratchpad read.
- **Timing**: one vector per cycle, fixed latency `L_OBP` (read S, multiply, round-shift, add zp / clamp, LUT, write) from
  the pipeline cuts of the drawings; ESTIMATED until the micro-architecture specification gives it (`has::Knobs::lat_obp`).
- **Inline mode**: the same arithmetic applied on the PSM -> SRAM-C write path, one output channel at a time
  (`HasObp::apply_inline`).

### 6.2 `gvu_elemwise` -- ELEM_WISE ADD / MAX / AVG
- **Data through the scratchpad in chunks** (in the drawing every operand is read from and written to the scratchpad;
  who moves it is question H9, the model uses the DMA):
  - ADD: chunks of **4,096 elements** = A (4 KB int8) + B (4 KB int8) + Sum (16 KB int32) = 24 KB = `SCRATCH_BYTES`; the int8
    result overwrites the A area and is written back to DRAM. With the banked scratchpad the chunks are double-buffered.
  - MAX: per channel group, as many channels as fit in the scratchpad with their padding.
- **ADD** (drawing ELEM_WISE rows 1 and 2): stage 1 `a' = Dequant(A, zpA, SA, sA)`, `b' = Dequant(B, zpB, SB, sB)`,
  `Sum = sat(a' + b', int32)` written to the scratchpad; stage 2 `out = Requant(Sum, SO, sO, zpO)`.
  **Unit scales** (SA = SB = SO = 1, sA = sB = sO = 0, zp = 0) give `out = sat8(A + B)`, the residual add of v4.5.
- **MAX** (5 x 5, stride 1, pad 2 for SPPF): pad value **-128**; `DIRECT` reads 25 vectors per output vector, `SEPARABLE`
  5 + 5. Both modes must give the same result (evidence for question H6).
- **AVG** (mode 5, drawing "Avg_Pool"): k x k window summed in an int16 accumulator (wraps, counted), times Avg_Scale
  (`SO`), round-shift by Avg_Shift (`SHO`), saturated to int8 (counted); no padding.
- **Timing**: each stage one vector per cycle plus pipeline latency, plus the DMA cycles into and out of the scratchpad.
  Counters `scratch_rd_bytes`, `scratch_wr_bytes`.

## 7. Program and parameters (format owned by the frontend)

- `program.json`, HAS variant: a convolution with `skip` is split into the convolution (output `T@pre`) and an **`elem_add`**
  step {`a`: `T@pre`, `b`: skip, `out`: T, `zpA`, `zpB`, `zpO`, `SA`, `sA`, `SB`, `sB`, `SO`, `sO`}; a host max-pool becomes
  an **`elem_max`** step {`in`, `out`, `k`, `s`, `p`}. The convolution tiles are unchanged.
- `prog.bin`: magic `"FEHP"` + `<4I>` knobs (round, narrow, deq_zp, scale_fmt), then the format of `docs/INTERFACE_SPEC.md`
  with two more step kinds:
  - `KIND 5 elem_add`: `<B3I3i6I>` = kind, `a`, `b`, `out` (tensor ids), `zpA`, `zpB`, `zpO` (int), `SA`, `sA`, `SB`, `sB`,
    `SO`, `sO` (uint32; S as raw 32 bits).
  - `KIND 6 elem_max`: `<B5I>` = kind, `in`, `out`, `k`, `s`, `p`.
  YOLOv8m: 129 steps = 83 convolutions + 12 `elem_add` + 3 `elem_max` + 31 host steps. Testbench `tools/fe/sysc/tb_has_net.cpp`.
- HAS requant parameters: `params.npz` adds `<job>/scale_i32` (int32), `<job>/shift_i8` (int8), `<job>/zp_out` (int16, 0).
  The HAS parameter set of YOLOv8m (`tools/fe/fe_make_has_config.py`) is derived from the v4.5 one with `S' = ceil(S / 2)`
  in [2^30, 2^31) and `s' = s - 1`, and also writes `<job>/scale`, `<job>/shift` and `<job>/round_mode = 1` so that
  `fe_ref_int8.run_int8` runs on it (without `round_mode` it rounds by floor, as before).

## 8. Unit-test vectors (written by Python, read by SystemC)

Directory `$FE_WORK/has/vectors/`. Text files, one operation per line, integers separated by spaces. First two lines:
`# knobs round=<0..3> narrow=<0..2> deq_zp=<0..1> scale_fmt=<0..1>` and `# <column names>`. `S` is written as the **raw 32
bits the host writes** (unsigned 0 .. 2^32 - 1); the reader decodes it per `scale_fmt`. Flags `sat16` / `clamp8` are 1 when
that operation saturated or wrapped at the corresponding step (`sat16` is 1 when `r` is outside [-32768, 32767] with both
`SAT16` and `WRAP16`, and always 0 with `NONE`). Reader: `tools/has/tb_gvu_quant <files>`.
- `requant_<mode>.txt`: `x S s zp out sat16 clamp8` (at least 100,000 lines: random plus boundaries -- `x` at +-2^31, `S` at
  its limits, `s` in {0, 1, 31, 62, 63}, exact half-way cases).
- `dequant_<mode>.txt`, `lut_direct.txt`, `elem_add_<mode>.txt` (4,096-element chunks), `elem_max_<mode>.txt` (small tensors
  with padding).
- `<mode>` in `compat`, `has`, and one variant per knob of questions H1 to H3.

## 9. Notes for network testbenches

With `gvu_obp` a testbench writes `NCH` (0x1A0004) for every tile (number of channels of the tile), waits until
`HasObp::vectors_out` has reached the expected count instead of a fixed idle time, and keeps `residual_en` at 0 (the
residual runs as an `elem_add` step).
