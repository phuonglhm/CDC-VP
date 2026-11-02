# Average pool, Softmax, attention requant and LayerNorm on GvuTop

This document describes how the instruction path runs four vector-unit operations on `GvuTop` (the port of the top level of the
vector unit, `has/rtl_ref_gvu_top.h`) instead of the functional model, what the model proposes to the hardware team where the
delivered RTL cannot run the operation, and the checks. Each operation is behind a knob that is on by default; with the four knobs
set to 0 the model gives the same values, cycles and log as the functional model of the vector unit.

Labels: MEASURED (run in the model or in an RTL simulation), READ (read in the code), ESTIMATE, PROPOSAL (a change the model asks
the hardware team for; not in the delivered RTL).

## 1. Knobs

| Knob | Default | Operation | Needs | Path in the model |
|---|---|---|---|---|
| `HAS_GVU_AVG` | 1 | ELEM_WISE AVG_POOL (mode 5) | `HAS_GVU_TOP=3` | `HasElemwise::avgpool` -> `GvuTopStream::start_avgpool` (POOLING_AVG of the RTL patch) |
| `HAS_GVU_SOFTMAX` | 1 | Softmax rows of FUSED_ATTN | `HAS_GVU_TOP=3` | `fused_attn` hook -> `GvuTopStream::start_softmax` (GVU_OP_SOFTMAX) |
| `HAS_GVU_FA` | 1 | Requant of the attention products (phase 1 Q.K^T, phase 3 A.V) | `HAS_GVU_SOFTMAX` | `fused_attn` hook -> `GvuTopStream::start_requant` (GEMM_FUSED stream beats) |
| `HAS_GVU_LN` | 1 | LAYERNORM | `HAS_GVU_TOP=3` | `exec_ln` -> `GvuTopStream::start_layernorm` (lane data path, `has/rtl_ref_gvu_ln_lanes.h`) |
| `HAS_GVU_AVG_AS_RTL=1` | 0 | negative control | `HAS_GVU_AVG` | rounding and narrowing of the delivered RTL (round half away, wrap) |
| `HAS_GVU_SOFTMAX_AS_RTL=1` | 0 | negative control | `HAS_GVU_SOFTMAX` | stage-3 shift of the delivered RTL (`E` instead of `E + q - P`) |
| `HAS_GVU_LN_DUMP=<dir>` | unset | test vectors | `HAS_GVU_LN` | every LayerNorm group written as a text file (format of `tools/has/tb_gvu_ln_lanes.cpp`) for the hardware team |

In every case the model computes its own result first, the operation then runs on `GvuTop`, the two are compared lane by lane, and
the value of `GvuTop` is the one written to memory. An operand that `GvuTop` cannot hold keeps the model result and is counted as
"not expressible". The counts are printed at the end of a run (`[gvu-stream] avg pool`, `softmax`, `attention requant`, `layernorm`).
The cycles of `GvuTop` replace the functional estimate of an instruction only when every group of that instruction ran on `GvuTop`;
one group left on the model keeps the estimate for the whole instruction.

With the register file on (`HAS_GVU_REGS=1`, the model default), average pool, Softmax and the attention requant are configured and
started by register writes of the data flow controller; the unit takes its configuration from the register file, and each write costs
`HAS_GVU_REG_LAT` edges (default 4, ESTIMATE). A value that the register fields do not hold keeps the model result (counted). LayerNorm
does not use the register file or the controller of the unit: the delivered RTL has no registers for it (see P-LN6).

All four operations run as jobs on the clock of the unit, like residual add and max pool: the data flow controller starts the job and
waits for its end, and the job takes one step per edge (one tick of the unit, or one edge of a register write). With
`HAS_GVU_REGS=0 HAS_GVU_ELEM_CLK=0` they run in the call style instead: the data flow controller waits the cycles of the unit without
simulated steps; the values are the same.

## 2. Average pool

Data layout as max pool: the input of a channel group in vbank0 buffer 0 as rows of 32 bytes (row `(c * H + y) * W_TILE + t`), no
padding; the result in vbank2 at the plain addresses of the VLSU. Runs of at most `cmax` channels, each from a reset of the unit
(bug B12 of max pool). Registers: POOLING_0 / POOLING_1 (Reg 7 / 8) for the window, **Reg 25 `AVG_SCALE`** (Avg_Scale, int32) and
**Reg 26 `AVG_CFG`** (`[5:0]` Avg_Shift, `[10:8]` round mode), start with opcode AVGPOOL (8) in Reg 0 `[6:3]`. Reg 25 / 26 and opcode 8
are part of the RTL patch for average pooling (PROPOSAL; the delivered RTL has no average pool data path). The patch delays the write
by D = 5 edges after the last element of a window (MEASURED in an RTL simulation of the patch). Saturation to int8.

POOLING_0 encodes the window as `k - 2` in two bits (k = 2..5) and the stride in three bits, but the pooling data path of the unit
handles only these (k, s): 2x2 s1 / s2, 3x3 s1, 4x4 s1 / s2 / s4, 5x5 s1 (READ, the window table of `GvuTopStream::pool_geom`). An
average pool with another window, or with more than 255 output rows or W tiles, stays on the model (counted). **Proposal P-AVG2**:
extend the pooling data path to the common average pools (3x3 s2, k x k with stride k) and widen the window field of POOLING_0 for
the global average pool, whose window is the whole plane.

## 3. Softmax (FUSED_ATTN phase 2)

One group = at most 32 rows of the score matrix, one row per lane; position `p` of the row is word `p` of vbank0 buffer 0 (the result
is written in place). Tables: reciprocal image in vbank1 buffer 1 words 0..3 (16 bit), exponential table in words 4..5 (8 bit), one DMA
of 768 bytes per instruction. Registers: SOFTMAX_0 (Reg 4, token length `[8:0]`, tiles `[17:9]`, written as 1), SOFTMAX_1 (Reg 5,
ATTN_Scale, int32), SOFTMAX_2 (Reg 6, ATTN_Shift `[7:0]`, Z_ATTN `[15:8]`), start with opcode SOFTMAX (3).

| Code | What | Evidence |
|---|---|---|
| B20 | Stage 3 shifts the product by `E + q - P` (q = 14, P = 15 in the functional model), not by `E`: the delivered RTL gives half the value; a smaller ATTN_Shift compensates only up to one LSB at the rounding ties | MEASURED: on three ViT-B heads the delivered shift differs at 326 / 3,443 / 2,033 of 6,304 outputs, the patch (shift `E - 1`) at 0 / 0 / 0, in the port and in an RTL simulation; on the transformer check program the negative control differs at 94,961 of 242,070 lanes |
| B20b | Stage 3 has no left shift: with an exponent E = 0 (sum of the exponentials equal to 1) the shift `E + q - P` is -1 and cannot be expressed. The model uses the exponent of the current table lookup and shifts left | MEASURED in an RTL simulation on a constructed case with E = 0: the delivered RTL and the B20 patch both differ at 8 of 64 outputs; the model differs at 0. No RTL patch yet. Not reached on ViT-B/16 (smallest E is 7, because the exponential table gives 255 at 0) |
| B21 | The vbank1 buffer bit flips once, on the rising edge of the table-init done output, not on its level | MEASURED in an RTL simulation through the register file: the delivered RTL flips the bit 617 times during one Softmax and on every idle edge after it, the row maximum of stage 2 is read wrong 99 / 197 times and every output differs; with the patch the register path equals the direct drive value for value |
| P-SMX3 | Token length is 9 bits: L <= 511 through the register file | ViT-B (L = 197) fits; longer sequences need a wider field. Without the register file the limit is the depth of vbank0 (800) |

A row group stays on the model when the parameter block of the instruction asks for exponentials forced to zero at masked positions
(`mask_e_zero`) and one of its rows has a masked position. Without that flag a masked position only sets the score to -128 and the
group runs on the unit.

## 4. Attention requant (FUSED_ATTN phases 1 and 3)

The int32 sums of the two products (on the core) go through the GEMM_FUSED epilogue of `GvuTop` in stream mode, without activation
(Reg 3 `[0]` skip activation, zero point in `[15:8]`), one beat of 32 lanes per edge. The zero-point corrections are inside the sums
before the epilogue. Each phase is a job that starts after the multiplication of the core and takes one edge per beat plus the edges
of its register writes; it does not overlap with the core.

| Code | What |
|---|---|
| P-FA1 | Phase 1 and phase 3 requant on the GEMM_FUSED stream of `GvuTop` (no new unit): parameters M (int32), shift 0..63, zero point int8 |
| P-FA2 | Phase 3 correction `-Za * Vcol_n` (and the `Zv` terms when enabled) by the preload of the partial sums of the core (the core has the preload path) |
| P-FA3 | Softmax stage 1 (row maximum) consumes the beats of phase 1 as they leave the epilogue (`softmax_inp_sel = 0` driven by the FUSED_ATTN opcode): its cycles would leave the softmax time. Not modelled: on the clock, stage 1 runs inside the Softmax job after phase 1, and nothing is subtracted |
| P-FA4 | The mask sets a score to -128 after the requant; the epilogue has no mask input today (the model applies it after the run and counts the scores) |

## 5. LayerNorm with lane statistics (proposal)

The delivered RTL keeps the row sum S, the variance V and the rsqrt operand w as one value for all 32 lanes and writes the result to
vbank1 (128 int8 rows): it cannot normalise 32 tokens at once and does not hold H = 768. The model proposes the data path of
`has/rtl_ref_gvu_ln_lanes.h` (MAS figure 8, one token per lane, one channel per beat):

| Code | What |
|---|---|
| P-LN1 | S, V, v, w, r, E per lane (32 sets of stage registers) |
| P-LN2 | Stage 4 computes `D_k = H * x_k - S` again from `x_k` instead of keeping `D_k` in vbank2 (256 words limit H to 256) |
| P-LN3 | Stage 4 writes `n_k` and stage 5 writes `y_k` in place in vbank0 (800 words): H up to 768 with the sbank below |
| P-LN4 | Per channel: beta_k (int32) in sbank buffer 0 word k, gamma_k (int8, sign-extended in the word) in sbank buffer 1 word k (sbank 768 words) |
| P-LN5 | `M0_mul` / `TS_mul` as one value for the row (MAS form; ViT-B uses one value for its 768 channels); per-channel values stay on the model |
| P-LN6 | The scalars of the row (Pre_Shift, M0_var, TS_var, Z_var, M0_7, TS_7, E_bias, Z_7, M0_div, TS_div, Z_div, M0_mul, TS_mul, Z_mul, Z_out, R8 / R9 flags) need about 20 register words; Reg 27..31 are not enough, so the register file would grow to 64 words (`GVU_REG_AW` 6). Not modelled: the model's register file keeps 32 words and LayerNorm takes its parameters from the parameter block of the instruction |

A group stays on the model when H > 768, the output is int16, a gamma is outside int8 or `M0_mul` / `TS_mul` differ between channels.
The values are the integer equations of `has::layernorm_row` lane by lane, with the operands read from the banks. The cycles of a
group are an ESTIMATE: beats plus stage depths from the latencies of the ported units (bank read 2, Round-Shift 1, LUT PWL, one
register per multiplier), about 4H + 29 per group (DERIVED); there is no RTL to measure. The lane data path is a state machine
ticked once per edge (beat k at edge k + 2). Because `D_k` is not stored, a group holds 32 tokens; the functional model's
estimate limits the rows of a group by the Scratchpad space for `D_k`, which is why LayerNorm takes fewer cycles with the knob on.

## 6. Checks

Transformer check program (9 FUSED_ATTN, 5 LAYERNORM: ViT-B blocks 0 / 6 / 11 and random cases) and the AVG_POOL check program,
with the five stream knobs at their defaults (`HAS_GVU_TOP=3`, register file on) and the operation knobs as in the first column;
MEASURED:

| Run | Result | Unit counts | Cycles of the program |
|---|---|---|---|
| four operation knobs at 0 | 14 / 14 | — | 285,256 |
| `HAS_GVU_SOFTMAX` only | 14 / 14 | 47 groups, 242,070 lanes, 0 differ, 188 register writes | not checked |
| `HAS_GVU_SOFTMAX` and `HAS_GVU_FA` | 14 / 14 | 18 phases, 327,958 lanes, 0 differ, 11,246 beats | not checked |
| `HAS_GVU_LN` only | 14 / 14 | 8 groups, 174,080 lanes, 0 differ | not checked |
| default (all on, jobs on the clock) | 14 / 14 | as above; 32 jobs, 0 failed | 263,883 (263,816 with the epilogue depth 9) |
| all on, call style (`HAS_GVU_REGS=0 HAS_GVU_ELEM_CLK=0`) | 14 / 14 | as above | 242,711 |
| `HAS_GVU_SOFTMAX_AS_RTL=1` (negative control) | FAIL | 94,961 lanes differ | — |
| AVG_POOL, four operation knobs at 0 | 8 / 8 | — | 3,486 |
| AVG_POOL, default | 8 / 8 | 4 instructions on GvuTop, 1,104 lanes, 0 differ, 24 register writes; 4 with a window the RTL does not pool | 3,687 |
| `HAS_GVU_AVG_AS_RTL=1` (negative control) | FAIL | 175 lanes differ | — |

`tools/has/regress_gvu.sh` (182 of 182, MEASURED) has an item for each run above except the rows with the four operation knobs at
0, which are the reference of this table (the regression runs the same check programs with every vector-unit knob at 0). It checks
the result and the unit counts, and the cycles of the default run (263,883) and of the call style (242,711). The lane LayerNorm has its own unit test,
`tools/has/tb_gvu_ln_lanes.cpp` (against `has::layernorm_row` on 300 random groups, H 1..768, 1..32 rows, every round mode:
2,013,991 lanes, 0 differ; four parameter sets that do not fit are refused).

Whole networks in the default configuration (YOLOv8m does not use these operations):

| Network | Result | Unit counts | Label |
|---|---|---|---|
| ViT-B/16 | PASS 244 / 244, 0 elements different, 41,244,030 cycles at the epilogue depth 9, 41,244,621 at the default depth 11 (four operation knobs at 0, depth 9: 41,582,468; the difference, -338,438 cycles, is in the vector-unit operations, and the LayerNorm cycles are an ESTIMATE), core busy cycles unchanged (24,852,624) | 457 jobs on the clock (144 Softmax instructions in 1,008 groups, 288 requant phases, 25 LayerNorm instructions in 175 groups), 0 failed, 0 lanes different | MEASURED |
| YOLOv8m | PASS 98 / 98, 0 elements different, 62,422,935 cycles (`insts_pe3`, epilogue depth 9), equal to the four operation knobs at 0; core busy cycles 55,653,442 (unchanged) | none of the four operations | MEASURED |

## 7. Not done

- RTL patches and simulation oracles for P-LN1..P-LN6 and P-FA3 / P-FA4 (the patches for B20, B21 and the average pool exist).
- The attention requant overlapped with the multiplication of the core (it runs after it, section 4), and P-FA3.
- A DMA (COPY) program for ViT-B: none is needed. The heads read and write 64-channel blocks of channel-major [C][L] tensors, so
  every slice and every concat of the network is an address on the outermost axis.
