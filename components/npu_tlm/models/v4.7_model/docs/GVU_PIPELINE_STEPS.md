# Vector unit: steps, units, registers and cuts of every pipeline

This document lists, for every pipeline of the vector unit, the steps the pipeline drawings of the micro-architecture
specification (MAS V1.0) show: which functional unit works at each step, in which mode, which stream register follows
it, and where the drawing cuts the pipeline. It then derives the depth of every pipeline stage in three ways and says
which ported block (`has/rtl_ref_gvu_*.h`) exists for each element of the drawings. It is the starting point for
composing a pipeline from the ported blocks.

It is a table of what the drawings and the RTL say. The flows of this table run on `GvuTop` in the instruction path by default (`GVU_TOP_CORE_INTEGRATION.md`); the table itself is taken from the drawings and the RTL.

## 0. Sources, labels, conventions

**Sources**

- MAS V1.0: Figures 5-1 to 5-18, Figure 6-1, Table 5-1 (functional units), Table 5-2 (registers accessed in parallel),
  Table 5-4 (Scratchpad ports), the text of section 5.
- The RTL of the hardware team (`neo_core/gvu`).
- Ported blocks: `has/rtl_ref_gvu_fu.h`, `rtl_ref_gvu_lut.h`, `rtl_ref_gvu_vrf.h`, `rtl_ref_gvu_scratchpad.h`,
  `rtl_ref_gvu_vlsu.h`, `rtl_ref_gvu_ctrl.h`.
- Functional model: `has/gvu_quant.h`, `gvu_softmax.h`, `gvu_layernorm.h`, `gvu_pipeline_ctrl.h`, `gvu_elemwise.h`,
  `has_npu_top.h`, `gvu_vrf.h`, `gvu_sp_map.h`.

**Labels**

- **FIGURE**: read from a drawing of the MAS. Figures 5-5 and 5-6 have a low resolution; they were read region by region
  at three times magnification. **(faint)** marks an index or a word that could be read but not with certainty.
- **TABLE**: read from a table or the text of the MAS.
- **DERIVED**: computed from values that were read; the rule is in section 0.3.
- **MEASURED**: measured on the RTL simulation or on a ported block.

### 0.1 How the MAS numbers the resources (DERIVED from the drawings; consistent with every drawing read)

| Resource | Index | Basis |
|---|---|---|
| Multiplier `#0` | the 48 x 40 unit (modes 40x32, 32x32 + 16x8, 32x8, 3 x 16x8); `#0-0`, `#0-1` are its 16x8 sub-multipliers | Figure 5-5 "Mul v32 int16 x int8 #0-0 / #0-1"; Figure 5-6 stage 4 "Mul v32 int40 x int32 #0" |
| Multiplier `#1` | the v32int32 unit (32x32, 32x16, 32x8, 2 x 16x16) | Figure 5-2 "Mul v32 int32 x int32 #1"; Figure 5-6 stage 3 "Mul v32 int32 x int16 #1" |
| Adder `#0` | the v32int64 unit; `#0-0`, `#0-1` are its two 32-bit halves | Figure 5-6 stage 2 "Add v32int64 #0"; Figure 5-5 "Add v32int32 #0-0 / #0-1" |
| Adders `#1`, `#2` | the two v32int32 units; `#1-0`, `#1-1`, `#2-0` are 16-bit halves (also used for 8-bit elements) | Figure 5-7 "Sub v32int16 #1-0 / #1-1", "Add v32int32 #2"; Figure 5-2 "Add v32int16 #2-0" |
| Round-shift `#0` | v32int72 | Figure 5-4 stage 3, Figure 5-6 stages 2 and 4 |
| Round-shift `#1` | v32int64 | Figure 5-2, Figure 5-6 stage 3, Figure 5-7 average pool |
| Clamp `#0`, comparator `#0` | one unit each | Table 5-1 |
| Stream registers | Figure 5-15: int64 `#0` splits into int16 `#0..#3`; int32 `#0` into int16 `#4, #5` or int8 `#0..#3`; int64 `#2` into int32 `#4, #5`; int64 `#1` and int32 `#1..#3` do not split | Figure 5-15; equal to `stream_vrf.sv` and `rtl_ref_gvu::StreamVrf` |

### 0.2 Depth inside each unit

| Unit | MAS (Table 5-1 and text) | RTL with its default parameters (MEASURED on the simulation) | Constant of the port |
|---|---|---|---|
| Multiplier 32x32 | 4 to 5 stages | 4 | `mula_latency(MA_32X32)` |
| Multiplier 32x16 | 2 to 3 | 2 | `mula_latency(MA_32X16)` |
| Multiplier 32x8 | 1 to 2 | 1 | `mula_latency(MA_32X8)` |
| Multiplier 2 x 16x16 | 2 | 2 | `mula_latency(MA_16X16)` |
| Multiplier 40x32 (unit `#0`) | 4 to 5 | 4 | `mulb_latency(MB_40X32)` |
| Multiplier 32x32 + 16x8 (unit `#0`) | 32x32: 4 to 5; 16x8: 1 | 4 for both products | `mulb_latency(MB_32X32_16X8)` |
| Multiplier 3 x 16x8 (unit `#0`) | 1 | 1 | `mulb_latency(MB_3X16X8)` |
| Round-shift | 2 phases | 1 (`S2_PIPE = 0`); 2 with `S2_PIPE = 1` | `lat_vroundshift()` |
| LUT direct / log-scale / piecewise-linear | 1 / 3 / 6 | 0 / 2 / 4 (`LERP_SHIFT_PIPE = 0`); piecewise-linear 5 with `LERP_SHIFT_PIPE = 1`, the value `gvu_top.sv` sets | `lat_lut()` |
| Add / subtract, clamp, shift | not given | 0 | `kLatAdder32`, `kLatClamp`, `kLatShift` |
| Comparator | not given (Figure 5-11 draws an output register) | 0 (`OUT_PIPE = 0`); 1 with `OUT_PIPE = 1` | `lat_vcmp32()` |
| Scratchpad read | not given | 2 | `ScratchpadParams::read_latency()` |

Remark (DERIVED): the default RTL latency plus ONE stream register gives the upper end of the MAS range for every unit
except the piecewise-linear LUT: multipliers 4 + 1 = 5, 2 + 1 = 3, 1 + 1 = 2; round-shift 1 + 1 = 2; LUT direct
0 + 1 = 1, log-scale 2 + 1 = 3; piecewise-linear 4 + 1 = 5 against 6 in the MAS (6 with `LERP_SHIFT_PIPE = 1`). See
open point O2.

### 0.3 Three ways to count the depth of a pipeline stage

A "step" is one functional unit with the stream register behind it (the MAS defines exactly one stream register per
step). The "read pipeline register" behind the Scratchpad counts as a step. A last step that writes straight to the
Scratchpad has no stream register.

- **(a) MAS**, as a range. Lower end: every step costs max(1, smallest depth of the unit), the stream register being the
  last stage of the unit. Upper end: every step costs (largest depth of the unit) + 1 register.
- **(b) cuts of the MAS with the latency of the ported blocks**: every step costs (default RTL latency of the unit) + 1
  stream register.
- **(c) the figure the functional model uses.**

(a) and (b) are DERIVED; only the unit latencies of section 0.2 are MEASURED. (b) counts one cycle for a Scratchpad read;
the RTL needs two.

The requant chain appears in many stages and is counted once: multiply 32x32, round-shift, add zero point (the clamp has
no register of its own): (a) 7 to 10, (b) 4 + 1 + 1 + 1 + 0 + 1 = 8.

## 1. The pipelines step by step

Column "Cut": "yes" = the drawing has a dashed cut right after the step.

### 1.1 Requant (Figure 5-2, upper half)

| Step | Input | Unit, index, mode | Stream register | Cut | Goes to |
|---|---|---|---|---|---|
| 0a | Scratchpad: Out_Scale (sbank int32), or Out_Scale of the instruction (broadcast) | none | read pipeline register int32 | yes | step 1 |
| 0b | Scratchpad: Out_Shift (sbank int8), or Out_Shift of the instruction | none | read pipeline register int8 | yes | step 2 |
| 1 | input data v32int32 x scale | Mul v32 int32 x int32 `#1` | v32int64 reg `#1` | yes | step 2 |
| 2 | product, shift | Round-shift v32int64 `#1` | v32int16 reg `#4` | yes | step 3 |
| 3 | + zero point (instruction), sign extended | Add v32int16 `#2-0` | v32int16 reg `#5` | yes | step 4 |
| 4 | multiplexer "zero point skip" (takes the value right after the round-shift) | Clamp v32int32 `#0`, -128 to 127 | none | | output v32int8 |

Table 5-2: 1 v32int64 + 2 v32int16, as drawn. Table 5-4: Out_Scale in sbank int32, Out_Shift in sbank int8, as drawn.

### 1.2 Dequant (Figure 5-2, lower half)

| Step | Input | Unit, index, mode | Stream register | Cut | Goes to |
|---|---|---|---|---|---|
| 0a / 0b | In_Scale (sbank int32) / In_Shift (sbank int8), or from the instruction | none | read pipeline register int32 / int8 | yes | step 1 / 2 |
| 1 | input data v32int8 x scale | Mul v32 int32 x int8 `#0/1` | v32int64 reg `#0/1` (sign extended) | yes | step 2 |
| 2 | product, shift | Round-shift v32int72/64 `#0/1` | v32int32 reg `#3/1` | yes | step 3 |
| 3 | + zero point (instruction) | Add v32int32 `#0-1/0-2` | none | | multiplexer "zero point skip", output v32int32 |

"`#0/1`" are the two copies that run in parallel in the residual add (Figure 5-7 uses exactly these pairs). Table 5-2:
1 v32int64 + 1 v32int32, as drawn. The index "`#0-2`" does not exist under section 0.1 (open point O3).

### 1.3 GEMM_FUSED epilogue (Figure 5-3)

| Step | Input | Unit | Stream register | Cut | Goes to |
|---|---|---|---|---|---|
| 1 | systolic array, partial-sum manager, partial-sum stream v32int32 (at the boundary of an output-channel tile) | requant pipeline (1.1) | v32int8 reg `#0` | yes (before and after the requant) | step 2 |
| 2 | reg `#0` | LUT_act v32int8 (direct) | none | yes (before the LUT) | multiplexer `skip_act`, write to the partial-sum SRAM |

Table 5-2: 1 v32int64 + 2 v32int16 + 1 v32int8, as drawn by count. The indices conflict: v32int16 reg `#4`, `#5` and
v32int8 reg `#0` all come from int32 reg `#0` (open point O1).

### 1.4 Softmax (Figure 5-4)

**Stage 1, row maximum**

| Step | Input | Unit | Stream register | Cut | Goes to |
|---|---|---|---|---|---|
| 0 | Scratchpad: x (vbank#0 v32int8); or the requant pipeline when FUSED_ATTN, through a multiplexer | none | read pipeline register v32int8 | yes | step 1 |
| 1 | sign extended | Comparator v32int8 `#0`, accumulate | v32int8 reg `#1` (loops back to the comparator) | yes ("end of row") | write row_max (vbank#1 v32int8) |

**Stage 2, row sum of exponentials**

| Step | Input | Unit | Stream register | Cut | Goes to |
|---|---|---|---|---|---|
| 0 | x (vbank#0), row_max (vbank#1) | none | two read pipeline registers v32int8 | yes | step 1 |
| 1 | row_max - x | Sub v32int8 `#1-0` | v32int8 reg `#0` | yes | step 2 |
| 2 | reg `#0` | LUT_exp v32int8 (direct) | none | yes (before the LUT) | write e (vbank#2 v32int8) and step 3 |
| 3 | e | Add v32int32 `#0-0`, accumulate | v32int32 reg `#2` | yes ("end of row") | write row_sigma (vbank#1 v32int32) |

**Stage 3, attention score**

| Step | Input | Unit | Stream register | Cut | Goes to |
|---|---|---|---|---|---|
| 0 | row_sigma (vbank#1 v32int32) | none | read pipeline register v32int32 | yes | step 1 |
| 1 | row_sigma | LUT_recip pipeline v32int16 | v32int16 reg `#2`; E_shift to v32int5 buf `#0` | yes | step 2 |
| 2 | x e (vbank#2 v32int8 through a read pipeline register) | Mul v32 int16 x int8 `#0` | v32int32 reg `#1`; E_shift to v32int5 buf `#1` | yes | step 3 |
| 3 | sign extended; shift amount through the multiplexer `LUT_mode`: buf `#1` or the E_shift of the instruction | Round-shift v32int72 `#0` | v32int32 reg `#3` | yes | step 4 |
| 4 | Z_ATTN, ATTN_Scale, ATTN_Shift (instruction) | requant pipeline | (the three registers of the requant) | yes | v32int8, write attention score to the Scratchpad, or to the input-feature-map SRAM (FUSED_ATTN) |

Table 5-2: stage 1 1 v32int8; stage 2 1 v32int32 + 1 v32int8; stage 3 1 v32int64 + 2 v32int32 + 3 v32int16, as drawn
(the two v32int5 buffers are not part of the register file).

### 1.5 FUSED_ATTN (Figure 5-5)

Time line (FIGURE): phase 1 "Q.K^T with running row maximum" = systolic array + epilogue + softmax stage 1; phase 2
"attention score" = softmax stages 2 and 3; phase 3 "A.V" = systolic array + epilogue.

**Phase 1**

| Step | Input | Unit | Stream register | Cut | Goes to |
|---|---|---|---|---|---|
| S | input-feature-map feeder (Q), weight feeder (K^T) | systolic array, partial-sum manager | | "end of Q row" | step 3a |
| 1a | Q (from the feeder) | Add v32int16 `#1-0`, accumulate | v32int16 reg `#0` ("Q row sum") | yes ("end of Q row") | step 2a |
| 1b | K (from the feeder) | Add v32int16 `#1-1`, accumulate | v32int16 reg `#1` ("K col sum") | yes | step 2b |
| 2a | Q row sum x constant of the instruction, labelled "-Zq" (faint) | Mul v32 int16 x int8 `#0-0` | v32int32 reg `#1` | yes | step 3a |
| 2b | K col sum x constant of the instruction, labelled "-Zk" (faint) | Mul v32 int16 x int8 `#0-1` | v32int32 reg `#4` | yes | step 3b |
| 3a | partial sum + reg `#1` | Add v32int32 `#0-0` | v32int32 reg `#2` | yes | step 4 |
| 3b | reg `#4` + "D.Zq.Zk" of the instruction | Add v32int32 `#0-1` | v32int32 reg `#3` | yes | step 4 |
| 4 | reg `#2` + reg `#3` | Add v32int32 `#1` | v32int32 reg `#5` | yes | step 5 |
| 5 | Mqk, TS_qk, Zqk (instruction) | requant pipeline | v32int8 reg `#0` | yes | step 6 |
| 6 | mask (Scratchpad v32int8 through a read pipeline register); multiplexer "skip mask": a mask of -128 replaces the value by -128 | multiplexer | v32int8 reg `#2` | yes | softmax stage 1 and write x to the Scratchpad |

**Phase 3**

| Step | Input | Unit | Stream register | Cut | Goes to |
|---|---|---|---|---|---|
| S | input-feature-map feeder (A), weight feeder (V) | systolic array, partial-sum manager | | "end of A row" | step 3 |
| 1 | V (from the feeder) | Add v32int16 `#1-0`, accumulate | v32int16 reg `#0` ("V col sum") | yes | step 2 |
| 2 | x constant of the instruction, labelled "-Zp" (faint) | Mul v32 int16 x int8 `#0-0` | v32int32 reg `#1` | yes | step 3 |
| 3 | partial sum + reg `#1` | Add v32int32 `#1` | v32int32 reg `#2` | yes | step 4 |
| 4 | Mav, TS_av, Zav (instruction) | requant pipeline | | | "write x to the Scratchpad" as labelled |

Against Table 5-2 ("FUSED_ATTN stage 1": 2 v32int64, 1 v32int32, 2 v32int16, 1 v32int8): the drawing of phase 1 uses
1 v32int64 (of the requant), **5 v32int32** (`#1..#5`), 4 v32int16 and 2 v32int8. Even with int32 `#4`, `#5` counted
as one split int64 there are 3 v32int32 against 1 in the table: not consistent (open point O1).

### 1.6 LayerNorm (Figure 5-6)

**Stage 1, row sum**

| Step | Input | Unit | Stream register | Cut | Goes to |
|---|---|---|---|---|---|
| 0 | x (vbank#0 v32int8) | none | read pipeline register v32int8 | yes | step 1 |
| 1 | sign extended | Add v32int32 `#1`, accumulate | v32int32 reg `#2` | yes ("end of row") | write row_sum (vbank#1 v32int32) |

**Stage 2, difference and variance**

| Step | Input | Unit | Stream register | Cut | Goes to |
|---|---|---|---|---|---|
| 0 | x (vbank#0), row_sum (vbank#1 v32int32) | none | read pipeline registers v32int8, v32int32 | yes | steps 1, 2 |
| 1 | x times H (instruction), sign extended | Mul v32 int16 x int8 `#0` (faint) | v32int32 reg `#1` | yes | step 2 |
| 2 | reg `#1` - row_sum | Sub v32int32 `#1` | v32int32 reg `#2` | yes | write D (vbank#2 v32int32) and step 3 |
| 3 | D x D | Mul v32 int32 x int32 `#0` (faint) | v32int64 reg `#0` | yes | step 4 |
| 4 | accumulate | Add v32int64 `#0` | v32int64 reg `#2` | yes ("end of row") | step 5 |
| 5 | Pre_Shift (instruction) | Round-shift v32int72 `#0` | v32int32 reg `#3` | yes | step 6 |
| 6 | M0_var, TS_var, Z_var (instruction) | requant pipeline | | | v32int8, write variance (vbank#0 v32int8) |

Steps 1 and 3 both carry the index `#0`: this is the mode "32x32 + 16x8" of multiplier `#0` (two products at once for
two consumers).

**Stage 3, epsilon add**

| Step | Input | Unit | Stream register | Cut | Goes to |
|---|---|---|---|---|---|
| 0 | variance (vbank#0 v32int8) | none | read pipeline register v32int8 | yes | step 1 |
| 1 | - Z_var (instruction) | Sub v32int16 `#1-0` | v32int16 reg `#0` | yes | step 2 |
| 2 | x M0_7 (instruction) | Mul v32 int32 x int16 `#1` | v32int64 reg `#1` | yes | step 3 |
| 3 | + E_bias (instruction) | Add v32int64 `#0` | v32int64 reg `#2` | yes | step 4 |
| 4 | TS_7 (instruction) | Round-shift v32int64 `#1` | v32int16 reg `#2` | yes | step 5 |
| 5 | + Z_7 (instruction) | Add v32int16 `#1-1` | v32int16 reg `#1` | yes | step 6 |
| 6 | -128 to 127 | Clamp v32int32 `#0` | v32int8 reg `#0` | yes | step 7 |
| 7 | - Z_7 and the test "x7 > Z_7"; a multiplexer selects Z_out (instruction) when not greater | Sub v32int16 `#2-0`, Comparator v32int8 `#0` | none | | v32int16, write (variance + epsilon) or Z_out (vbank#1 v32int16) |

**Stage 4, normalise and requant**

| Step | Input | Unit | Stream register | Cut | Goes to |
|---|---|---|---|---|---|
| 0 | variance + epsilon (vbank#1 v32int16) | none | read pipeline register v32int16 | yes | step 1 |
| 1 | | LUT_rsqrt pipeline v32int16 | v32int16 reg `#0`; E_shift to v32int5 buf `#0` | yes | step 2 |
| 2 | x D (vbank#2 v32int32 through a read pipeline register) | Mul v32 int32 x int16 `#1` | high part to v32int8 reg `#0`, low part to v32int32 reg `#1`; E_shift to v32int5 buf `#1-3` | yes | step 3 |
| 2' | E_shift + TS_div (instruction) | Add v32int16 `#2-0` | v32int9 buf `#0`, then buf `#1-4`, then buf `#5` | yes | step 4 |
| 3 | x M0_div (instruction) | Mul v32 int40 x int32 `#0` | high part to v32int8 reg `#1`, low part to v32int64 reg `#0` | yes | step 4 |
| 4 | shift amount from v32int9 buf `#5` | Round-shift v32int72 `#0` | v32int32 reg `#3` | yes | step 5 |
| 5 | + Z_div (instruction) | Add v32int32 `#1` | v32int32 reg `#2` | yes | step 6 |
| 6 | -128 to 127 | Clamp v32int32 `#0` | none | | v32int8, write normalised value (vbank#0 v32int8) |

**Stage 5, gamma multiply and beta add**

| Step | Input | Unit | Stream register | Cut | Goes to |
|---|---|---|---|---|---|
| 0 | normalised value (vbank#0 v32int8); gamma (sbank int8, read pipeline register int8, broadcast) | none | read pipeline register v32int8 | yes | steps 1, 2 |
| 1 | - Z_div (instruction) | Sub v32int16 `#1-0` | v32int16 reg `#0` | yes | step 2 |
| 2 | x gamma | Mul v32 int16 x int8 `#0` | v32int32 reg `#1` | yes | step 3 |
| 3 | M0_mul, TS_mul, Z_mul (instruction) | requant pipeline | v32int8 reg `#0` | yes | step 4 |
| 4 | - Z_mul (instruction) | Sub v32int16 `#1-1` | v32int16 reg `#1` | yes | step 5 |
| 5 | + beta (sbank int32, read pipeline register int32, broadcast) | Add v32int32 `#0-0` | v32int32 reg `#2` | yes | step 6 |
| 6 | + Z_out (instruction) | Add v32int32 `#0-1` | v32int32 reg `#3` | yes | step 7 |
| 7 | -128 to 127 | Clamp v32int32 `#0` | none | | v32int8, write result (vbank#1 v32int8) |

Against Table 5-2 by count: stage 1 (1 int32), stage 2 (3 int64, 3 int32, 2 int16), stage 3 (2 int64, 3 int16,
1 int8), stage 4 (1 int64, 3 int32, 1 int16, 2 int8) and stage 5 (1 int64, 3 int32, 4 int16, 1 int8) all agree with
the drawing.

### 1.7 ELEM_WISE (Figure 5-7)

**Residual add stage 1, dequant and add**

| Step | Input | Unit | Stream register | Cut | Goes to |
|---|---|---|---|---|---|
| 0 | tensor A (vbank#0 v32int8), tensor B (vbank#1 v32int8) | none | two read pipeline registers v32int8 | yes | step 1 |
| 1 | - zero point of A / B (instruction) | Sub v32int16 `#1-0` / `#1-1` | v32int16 reg `#0` / reg `#1` | yes | step 2 |
| 2 | A_Scale, A_Shift / B_Scale, B_Shift (instruction) | dequant pipeline: A uses v32int64 reg `#0`, v32int32 reg `#3`; B uses v32int64 reg `#1`, v32int32 reg `#1` | v32int32 reg `#2` / reg `#0` | yes | step 3 |
| 3 | A + B | Add v32int32 `#2` | none | | write sum (vbank#2 v32int32) |

**Residual add stage 2, requant**

| Step | Input | Unit | Stream register | Cut | Goes to |
|---|---|---|---|---|---|
| 0 | sum (vbank#2 v32int32) | none | read pipeline register v32int32 | yes | step 1 |
| 1 | Out_Scale, Out_Shift (instruction; the box of the drawing reads "Out_Scale + Out_Scale") | requant pipeline | | yes | write to the Scratchpad (vbank#0 v32int8) |

**Max pool**

| Step | Input | Unit | Stream register | Cut | Goes to |
|---|---|---|---|---|---|
| 0 | tensor (vbank#0 v32int8) | none | read pipeline register v32int8 | yes | step 1 |
| 1 | accumulate | Comparator v32int8 `#0` | v32int8 reg `#0` | yes ("end of window") | write to the Scratchpad (vbank#1 v32int8) |

**Average pool**

| Step | Input | Unit | Stream register | Cut | Goes to |
|---|---|---|---|---|---|
| 0 | tensor (vbank#0 v32int8) | none | read pipeline register v32int8 | yes | step 1 |
| 1 | accumulate | Add v32int16 `#1-0` | v32int16 reg `#0` | yes ("end of window") | step 2 |
| 2 | x Avg_Scale (instruction) | Mul v32 int32 x int16 `#1` | v32int64 reg `#1` | yes | step 3 |
| 3 | Avg_Shift (instruction) | Round-shift v32int64 `#1` | none | yes | write to the Scratchpad (vbank#1 v32int8) |

Table 5-2 agrees by count for the four pipelines. The average pool of the drawing has no clamp and adds no zero point.

## 2. Depth of every pipeline stage

Unit: cycles from the data entering step 0 to the data leaving at the destination, on the longest path. A stage with an
accumulation ("end of row / window") is split into the repeated part and the tail.

| Stage | (a) MAS, range | (b) cuts + ported blocks | (c) functional model | (b) against (c) |
|---|---|---|---|---|
| Requant (parameter reads not counted) | 7 to 10 | 8 | inside `lat_obp` / `lat_add2` | |
| Dequant, int8 operand (final zero-point add not counted) | 3 to 6 | 4 | inside `lat_add1` | |
| GEMM_FUSED epilogue | 9 to 12 | 8 + 1 + 0 = 9 | `lat_obp` = 10 in the run profiles | 9 against 10 |
| Softmax stage 1 | 2 | 2 | 3 | 2 against 3 |
| Softmax stage 2 | 4 | 1 + 1 + 0 + 1 = 3 | 5 | 3 against 5 |
| Softmax stage 3, piecewise-linear LUT | 17 to 23 | 1 + 5 + 2 + 2 + 8 = 18 | 14 + `lut_stages` = 18 | equal |
| Softmax stage 3, log-scale LUT | 14 to 20 | 16 | no figure of its own (17 with 3 LUT stages) | 16 against 17 |
| FUSED_ATTN phase 1, tail after "end of Q row" | 12 to 16 | 2 + 1 + 1 + 8 + 1 + 1 = 14 | no figure of its own | |
| FUSED_ATTN phase 3, tail after "end of A row" | 9 to 13 | 2 + 1 + 8 = 11 | no figure of its own | |
| LayerNorm stage 1 | 2 | 2 | 3 | 2 against 3 |
| LayerNorm stage 2, repeated part | 8 to 11 | 1 + 5 + 1 + 5 + 1 = 13 (both products of the mode 32x32 + 16x8 take 4 cycles, MEASURED) | 5 for the whole stage | 13 plus a tail of 10 against 5 |
| LayerNorm stage 2, tail at the end of a row (round-shift + requant) | 9 to 13 | 2 + 8 = 10 | included above | |
| LayerNorm stage 3 | 9 to 12 | 1 + 1 + 3 + 1 + 2 + 1 + 1 = 10 | 10 | equal |
| LayerNorm stage 4 | 16 to 22 | 1 + 5 + 3 + 5 + 2 + 1 = 17 | 10 | 17 against 10 |
| LayerNorm stage 5 | 14 to 18 | 1 + 1 + 2 + 8 + 1 + 1 + 1 + 1 = 16 | 10 | 16 against 10 |
| Residual add stage 1, branch B (multiplier `#1`, 32x16) | 7 to 10 | 1 + 1 + 3 + 2 + 1 = 8 | `lat_add1` = 8 | equal |
| Residual add stage 1, branch A (multiplier `#0` has no 32x16 mode, so 40x32) | 9 to 12 | 1 + 1 + 5 + 2 + 1 = 10 | as above | 10 against 8 |
| Residual add stage 2 | 8 to 11 | 1 + 8 = 9 | `lat_add2` = 10 | 9 against 10 |
| Max pool | 2 | 2 | `lat_max` = 3 | 2 against 3 |
| Average pool: repeated part 2; tail multiply + round-shift | 2 + (4 to 6) | 2 + 3 + 1 = 6 | uses `lat_add2` (10) | 6 against 10 |

The largest differences between (b) and (c) are LayerNorm stages 2, 4 and 5. These depths are added once per row group,
not per element. ESTIMATE for ViT-B/16 with the default configuration: 25 LayerNorm instructions, 15 groups each on the functional model (the default runs them on `GvuTop`: 175 groups, 7 per instruction, `GVU_OPS_ON_GVUTOP.md`, section 6),
31 cycles more per group, about 11,600 cycles of 41.68 million.

## 3. State machines of the MAS (Figures 5-16 to 5-18) against `has/gvu_pipeline_ctrl.h`

| Figure | States on the drawing | In the model | Difference |
|---|---|---|---|
| 5-16 softmax stage 1 | IDLE, SETUP (stream switch, stream registers, unit configuration, address generator), "FUSED_ATTN ?" then WAIT_SA or READ_DATA x, SOFTMAX_STAGE_1, at the end of a row WRITE_DATA row_max, at the end of the columns SOFTMAX_STAGE_2 | `softmax()`: IDLE, SETUP, WAIT_SA or READ_DATA, STAGE_1 per element, WRITE_DATA | none; the model folds the two end tests into a loop over groups of 32 rows |
| 5-16 stage 2 | SETUP, READ_DATA x and row_max, SOFTMAX_STAGE_2 (LUT valid writes e), WRITE_DATA row_sigma, SOFTMAX_STAGE_3 | SETUP, READ_DATA, STAGE_2, WRITE_DATA | none |
| 5-16 stage 3 | SETUP, READ_DATA row_sigma, SOFTMAX_STAGE_3 (the last LUT stage reads e; requant valid writes the score), IDLE | SETUP with a pipeline flush, per group READ_DATA + STAGE_3, IDLE | the model reads row_sigma per group; the drawing has one read before the loop |
| 5-17 LayerNorm stage 1 | IDLE, SETUP, READ_DATA x, LN_STAGE_1, WRITE_DATA row_sum, LN_STAGE_2 | `layernorm()` | none |
| 5-17 stage 2 | SETUP, READ_DATA x, READ_DATA row_sum, LN_STAGE_2_PASS_1 (writes D), LN_STAGE_2_PASS_2 (requant valid writes the variance), LN_STAGE_3 | SETUP, two reads, stage 2 per element then once | none; the model has one state name for both passes |
| 5-17 stage 3 | SETUP, READ_DATA variance, LN_STAGE_3, LN_STAGE_4 | SETUP, READ_DATA, STAGE_3 per group | none |
| 5-17 stage 4 | SETUP, READ_DATA variance + epsilon, READ_DATA D, LN_STAGE_4 (clamp valid writes the result), LN_STAGE_5 | SETUP, READ_DATA, per group READ_DATA + STAGE_4 | the model reads D per group; the drawing once before the loop |
| 5-17 stage 5 | SETUP, READ_DATA normalised value, READ_DATA gamma, LN_STAGE_5 (requant valid reads beta; clamp valid writes the result), END | SETUP, two reads, STAGE_5 per group, END | none |
| 5-18 residual add | two columns: SETUP, READ_DATA sum, RESIDUAL_STAGE_2, IDLE; and IDLE, SETUP, READ_DATA A and B, RESIDUAL_STAGE_1 (add valid writes the sum), RESIDUAL_STAGE_2 | `residual()` | none in content; the two captions under the drawing are swapped |

No state of the drawings is missing in the model and the model has no state the drawings lack. The drawings have no
state machine for max pool, average pool, requant / GEMM_FUSED and phase 3 of FUSED_ATTN. The cycle cost of each state
in the model is an assumption; the MAS gives no numbers.

The RTL controller (`gvu_pipe_controller.sv`) has other states: IDLE, one table-load state per LUT table, three softmax
states and five LayerNorm state names. Only the table load and softmax stage 1 run in the RTL delivered (see
`GVU_RTL_CONFORMANCE.md`, section 6).

## 4. Ported block for each element of the drawings

| Element of the drawings | RTL unit | Ported block | State against the RTL simulation |
|---|---|---|---|
| Mul `#1`: 32x32, 32x16, 32x8, 2 x 16x16 | `gvu_vmul32x32.sv` | `vmul32x32_lane()` | MEASURED: 0 mismatches of 300,000 |
| Mul `#0`: 40x32, 32x32 + 16x8, 32x8, 3 x 16x8 | `gvu_vmul48x40.sv` | `vmul48x40_lane()` (values per beat) | MEASURED: 0 mismatches of 300,000 over the four modes |
| Round-shift `#1` (int64), `#0` (int72) | `gvu_vroundshift64.sv`, `gvu_vroundshift72.sv` | `vroundshift64_lane()`, `vroundshift72_lane()` | MEASURED: 0 mismatches of 300,000 and 400,000 |
| Add / subtract on `#1`, `#2` (32 / 16 / 8 bit) | `gvu_adder32_lane.sv` | `adder32_lane()` | MEASURED: 0 mismatches of 400,000 |
| Add on `#0` (64 bit; two 32-bit halves) | `gvu_adder64_lane.sv` | `adder64_lane()`, `Adder64Lane` | MEASURED: 0 mismatches; a subtraction on this unit has no effect without its pipeline register, and the drawings only add on `#0` |
| Clamp `#0` | `gvu_vector_clamp.sv` | `clamp_lane()`, `vector_clamp()` | MEASURED: 0 mismatches of 400,000 |
| Comparator `#0` (accumulate, and the test of LayerNorm stage 3) | `gvu_vcmp32.sv` | `lane_minmax()`, `Vcmp32` | MEASURED: 0 mismatches, both modes, with and without the output register. The RTL has no comparison flag output (open point O4) |
| LUT_act, LUT_exp (direct); LUT_recip, LUT_rsqrt (log-scale, piecewise-linear) | `gvu_lut_unit.sv`, `gvu_lut_lookup_lane.sv`, `gvu_lut_mem.sv` | `LutMem`, `LutLookupLane`, `LutUnit`, `lut_value()` | MEASURED: 0 mismatches, one lane and 32 lanes, every output |
| LUT table load | `gvu_pipe_controller.sv` (load states) in `gvu_top.sv` | `LutInitSequence` | MEASURED: 0 mismatches, 9 cycles to the first softmax state |
| Stream registers v32int64 / 32 / 16 / 8 | `stream_vrf.sv`, `vstream_reg_*.sv`, `pipeline_reg.sv` | `StreamVrf`, `VstreamReg`, `PipelineReg` | MEASURED: 0 mismatches of 6,000 cycles on the 19 ports |
| Scratchpad (storage, accelerator ports) | `gvu_scratchpad.sv` and the memory wrappers | `GvuScratchpad` | MEASURED: 0 mismatches of 40,000 cycles. The DMA side is not connected in the RTL |
| Scratchpad reads and writes with their addresses | `gvu_vlsu.sv` | `GvuVlsu` | MEASURED: 0 mismatches of 40,000 cycles. One address counter per port; no multi-dimensional stride |
| Multiplexers "zero point skip", `skip_act`, "skip mask", `LUT_mode`, the Z_out selection, sign extension, broadcast (stream-switch routing) | written inside `gvu_top.sv` for the stages it connects; no unit of their own | none | to be written with each pipeline |
| Buffers v32int5 `#0..#3`, v32int9 `#0..#5` (delay line of E_shift) | not in `stream_vrf.sv` | none | open point O8 |
| Pipeline controller | `gvu_pipe_controller.sv`: softmax and table load | table load only | the softmax data path of `gvu_top.sv` is not connected |
| Shift v32int32 (Table 5-1) | `gvu_vector_shift.sv` | `shift_lane()` | MEASURED: 0 mismatches of 400,000; no pipeline drawing uses this unit |
| `vrf.sv` (aliased 1 KB register file) | `vrf.sv` | `Vrf` | MEASURED: 0 mismatches of 20,000 cycles; Figure 5-15 describes the organisation of `stream_vrf.sv`, which is the one `gvu_top.sv` instantiates |

## 5. Open points: the MAS against itself or against the RTL

- **O1** The stream-register indices on the pipeline drawings conflict with the splitting of Figure 5-15: one stage uses
  splittings that exclude each other. GEMM_FUSED, LayerNorm stage 5 and FUSED_ATTN phase 1 use v32int16 reg `#4`, `#5`
  (int32 reg `#0` split in two) together with v32int8 reg `#0` or `#2` (the same register split in four); LayerNorm
  stage 4 and residual add stage 1 use v32int16 reg `#0` together with the whole v32int64 reg `#0`; FUSED_ATTN phase 1
  uses five v32int32 registers where Table 5-2 lists one. By count every stage fits the register file when the indices
  are reassigned (`has::vrf_fits`: 16 of 16 stages), so the question is which index map is right.
- **O2** Does the stage count of a unit in Table 5-1 include the stream register of its step? Default RTL latency plus
  one register equals the upper end of the MAS range for every unit except the piecewise-linear LUT. Figures 5-10 and
  5-11 draw output registers of the round-shift and the comparator (`S2_PIPE = 1`, `OUT_PIPE = 1`) while the RTL defaults
  are 0. What are the final parameter values?
- **O3** Figure 5-2 (dequant) adds the zero point AFTER the round-shift, Figure 5-7 subtracts it BEFORE the dequant and
  the text of the MAS gives x = S (q - Z). The two orders differ when Z is not 0. The index "`#0-2`" does not exist. The
  dequant of Figure 5-2 shows a v32int8 input and an int32 x int8 multiplier, but in the residual add the operand after
  the subtraction is int16.
- **O4** LayerNorm stage 3 needs the comparison flag "x7 > Z_7" to drive the multiplexer that selects Z_out;
  `gvu_vcmp32.sv` outputs the minimum or maximum only. Which unit makes the selection?
- **O5** Residual add stage 1: the two dequant branches run on two different multipliers. Unit `#0` has no 32x16 mode,
  so its branch runs 40x32 (4 to 5 stages) while the other runs 32x16 (2 to 3 stages): the branches arrive two cycles
  apart at adder `#2`. How are they balanced?
- **O6** The mode "32x32 + 16x8" of multiplier `#0` delivers both products after 4 cycles (MEASURED); Table 5-1 gives
  one stage for a 16x8 product. LayerNorm stage 2 uses both products.
- **O7** FUSED_ATTN (Figure 5-5, faint): the row sum of Q is multiplied by a constant labelled "-Zq" and the column sum
  of K by "-Zk"; expanding the sum of (q - Zq)(k - Zk) the sum of Q is multiplied by -Zk and the sum of K by -Zq.
  Phase 3 shows a constant "-Zp" and the destination "write x to the Scratchpad" for the result of A.V.
- **O8** The buffers v32int5 and v32int9 of E_shift (softmax stage 3: two; LayerNorm stage 4: v32int5 `#0..#3`, v32int9
  `#0..#5`) are neither in Table 5-2 nor in `stream_vrf.sv`. Their number follows the multiplier depths.
- **O9** The requant inside LayerNorm (stages 2 and 5) is the common requant pipeline on the drawing, so it narrows to
  int16 after the round-shift; the functional model computes the LayerNorm requant without that narrowing
  (`has/gvu_layernorm.h`, `requant_clamp`). The two differ when the rounded value leaves int16.
- **O10** One shift amount for all 32 lanes on both round-shift units against one E_shift per lane from the LUT
  (softmax stage 3, LayerNorm stage 4): 32 rows with different exponents cannot pass the round-shift in one beat.
- Noted: Figure 5-9 draws an adder module that is in `FUs/deprecated/` of the RTL; Figures 5-8 and 5-10 use module and
  port names that differ from the RTL; Table 5-1 lists the comparator as "v32 int8" while Figure 5-11 and the RTL have
  32-bit lanes split into 8 / 16 / 32.
