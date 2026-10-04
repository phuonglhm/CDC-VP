# HAS Validation

This document summarizes how the epilogue and vector-processing drawings of the HAS (OBP and RCE, called the
Generic Vector Unit, GVU, in the hardware team's specification) were modelled and validated, and lists every
point where the drawings leave a choice open. For each open point it gives the model's current assumption, the
evidence collected, and the status after comparison with the GVU specification.

Labels: **MEASURED**, **DERIVED**, **ESTIMATED** as in `VERIFICATION_REPORT.md`.

## 1. Scope

| Pipeline of the drawings | Model file | Status |
|---|---|---|
| QUANTIZE (requant, dequant) | `has/gvu_quant.h` | Implemented, validated in all four layers |
| Direct LUT (activation, exp) | `has/gvu_quant.h`, `has/gvu_obp.h` | Implemented, validated in all four layers (activation) |
| GEMM_FUSED epilogue | `has/gvu_obp.h` | Implemented, validated on the full YOLOv8m |
| ELEM_WISE ADD (two stages), MAX, AVG | `has/gvu_elemwise.h`, `has/gvu_scratchpad.h` | ADD and MAX validated on the full YOLOv8m; AVG bit-exact through the instruction path (8 instructions, `tools/has/make_avgpool_gate.py`), not used by the two networks |
| Pipeline registers, load/store unit | `has/gvu_vrf.h`, `has/gvu_lsu.h` | Structure models: all 16 stages of table 2 fit the specified registers; transfers of ELEM_WISE counted per bank and address mode |
| Indirect LUT (reciprocal, rsqrt), log-scale mode | `has/gvu_lut.h` | Implemented, bit-exact at unit level |
| Indirect LUT, piecewise-linear (PWL) mode | `has/gvu_lut.h` (`LutPwl`) | Implemented, bit-exact at unit level (segment and uniform forms) |
| SOFTMAX, LAYERNORM, FUSED_ATTN | `has/gvu_softmax.h`, `has/gvu_layernorm.h`, `has/gvu_fused_attn.h`, `has/gvu_rce_params.h` | Implemented; bit-exact at unit level, through the instruction path and on the whole ViT-B/16. Open points R2 to R9, H13, H22, H23 are knobs with documented defaults (`KNOWN_LIMITATIONS.md`, section 7.1) |

The four validation layers are defined in `VERIFICATION_REPORT.md`, section 1; per-pipeline status is in its
section 7.

## 2. Main results

| Result | Value | Label |
|---|---|---|
| Unit bit-exactness, requant and dequant | 16 files x 120,000 independent vectors, every option combination, 0 mismatches | MEASURED |
| Unit bit-exactness, indirect LUT | 2 x 200,000 vectors (reciprocal, rsqrt), 0 mismatches | MEASURED |
| Indirect LUT accuracy (8-bit mantissa index with interpolation) | relative error <= 1.74e-4 (about Q14); without interpolation 3.9e-3 | MEASURED |
| Full YOLOv8m with the HAS arithmetic through the cycle-accurate core | 129 steps, 88,915,200 bytes, 0 mismatches against the independent HAS golden | MEASURED |
| Detection result, HAS arithmetic vs v4.5 arithmetic | Same 4 boxes, same classes; IoU 0.972 to 0.998 | MEASURED |
| Quality of the HAS requant format vs FP32 (8 images, calibration set) | F1 0.920, equal to the v4.5 format with bias-folded rounding; median cosine 0.943 | MEASURED |
| Transformer instructions (softmax, LayerNorm, FUSED_ATTN) | Bit-exact against `tools/fe/fe_ref_gvu.py` at unit level; 14 of 14 instructions through the instruction path; whole ViT-B/16, 244 instructions, 0 mismatches (`VERIFICATION_REPORT.md`, V17, V18) | MEASURED |
| AVG_POOL through the instruction path | 8 of 8 instructions bit-exact (V21) | MEASURED |

The HAS requant format (int32 scale, rounding inside the hardware) keeps the quality of the previous format
without needing to fold rounding into the bias.

## 3. Evidence collected on YOLOv8m

### 3.1 Rounding (H1) and int16 narrowing (H2)

Measured on the real accumulators of all 83 convolution layers (48,326,400 outputs per image, two images).

| Quantity | Value | Label |
|---|---:|---|
| Outputs with \|r\| > 32,767 before narrowing | 0 | MEASURED |
| Largest \|r\| before narrowing | 450 (image 1), 382 (image 2); margin about 72x | MEASURED |
| Outputs that change if narrowing wraps instead of saturating | 0 | MEASURED |
| Outputs that differ between HALF_UP, HALF_AWAY and HALF_EVEN | 0 | MEASURED |
| Outputs that differ between plain FLOOR and HALF_UP | 49.6 % | MEASURED |
| int16 saturations in the full-network run | 0 of 48,326,400 | MEASURED |

Conclusion for YOLOv8m: any round-to-nearest mode gives the same result; plain floor must not be used. Narrowing
never triggers. Transformer networks may have a different range and must be measured again.

### 3.2 Average-pool accumulator (H5)

A 20x20 global average of the constant 127 sums to 50,800 and overflows the drawn int16 accumulator in 4 of 4
channels (MEASURED, `tools/has/tb_gvu_elemwise`, test E5b). Any window above 258 elements can overflow.

### 3.3 MaxPool traversal (H6)

SPPF 5x5 stride 1 pad 2 on [288, 20, 20]: direct 159,544 cycles, separable (1x5 then 5x1) 80,008 cycles,
identical output bytes (ESTIMATED cycles, MEASURED equality). The separable form needs an intermediate
scratchpad area of Hp x Wo bytes per channel.

### 3.4 Scratchpad size (H8)

ELEM_WISE ADD blocks of 4,096 elements use exactly 24,576 bytes (A 4 KB + B 4 KB + int32 sum 16 KB). The full
YOLOv8m run reached this peak (MEASURED). LayerNorm on ViT-B is expected to need about 96 KB for the int32
intermediate of 32 rows x 768 (ESTIMATED).

### 3.5 One-row over-access at full SRAM (H14)

Tiles that fill SRAM-B or SRAM-C exactly make the core issue one access at the address equal to the capacity
(80 accesses in SRAM-B, 8 in SRAM-C over the 150 tile shapes; MEASURED). The values are not used and all such
tiles pass. A planner margin of one row would avoid the access if the hardware cannot tolerate it.

### 3.6 Transformer residual precision (H13)

Integer ViT-B/16 study (fake quantization matched to the integer flow; LayerNorm and softmax still float
stand-ins). Top-1 agreement with the float model:

| Configuration | Agreement | Label |
|---|---:|---|
| Everything int8 (residual int8 per-tensor) | 4 / 16 | MEASURED |
| Residual int16, full range; LayerNorm output int8 per-tensor | 76 / 96 | MEASURED |
| Residual int16; LayerNorm output int16 | 84 / 96 | MEASURED |
| Residual int16; LayerNorm output int8 per-channel | 82 / 96 | MEASURED |
| Residual int16; LayerNorm output float | 87 / 96 | MEASURED |

Conclusion: the transformer residual path (ELEM_WISE ADD) needs at least int16. For the LayerNorm output, int16
and int8 per-channel are statistically indistinguishable; int8 per-tensor is worse.

## 4. Open hardware questions and status

Status after comparison with the GVU specification:

- **Answered**: the GVU specification settles the point; the model follows it or will be updated.
- **Partly answered**: structure given, a detail still missing.
- **Open**: not covered by the GVU specification.
- **Conflict**: the GVU specification chooses differently from what the model's evidence recommends.

### 4.1 Epilogue and element-wise (H1 to H14)

| # | Question | Model assumption | Evidence | Status |
|---|---|---|---|---|
| H1 | Rounding mode of Round-Shift | HALF_UP (`HAS_ROUND_MODE=1`) | 3 nearest modes identical on YOLOv8m; floor 49.6 % different | Open |
| H2 | int64 to int16 narrowing before clamp: saturate or wrap | Saturate (`HAS_REQ_NARROW=0`) | 0 values out of range | Open |
| H3 | Dequant: subtract zero point before the scale multiply, or add after | Subtract before (`HAS_DEQ_ZP_ORDER=0`) | | Partly answered: standalone dequant adds the zero point after round-shift (with zero-point skip); the residual path subtracts it before. Formula for standalone dequant to be confirmed |
| H4 | Softmax `x - max` in int8 can reach -255: saturate or widen | Model computes `x - max` and indexes LUT[d + 128] | | Partly answered: stage 2 computes `e^(max - x)`, a non-negative difference 0 to 255 with a 256-entry table; no overflow if the difference is unsigned 8-bit. Model must change to `max - x` |
| H5 | Average-pool int16 accumulator overflow | Wraps, counted | Overflow shown (section 3.2) | Open: accumulator still int16 |
| H6 | MaxPool 5x5 in one pass or separable | Direct (`HAS_MP_MODE=0`); separable available | Separable 2x faster, identical | Open |
| H7 | FUSED_ATTN multiplies by 1/sqrt(d) then requantizes (two roundings) | | | Answered: 1/sqrt(d) is folded into the single requant scale; asymmetric zero-point correction added; mask read from the scratchpad (masked entries = -128) |
| H8 | Scratchpad capacity | Banked, 4 / 4 / 16 KB per half in the default profile; 24,576 bytes in one block in the `legacy` profile (`HAS_SCRATCH_BYTES`) | ADD uses exactly 24 KB; LayerNorm needs about 96 KB | Open: capacity "to be defined" |
| H9 | Who moves ELEM_WISE operands from PSUM SRAM to the scratchpad | Estimated DMA transfer | | Partly answered: a Vector Load/Store Unit, still to be defined |
| H10 | LayerNorm gamma and beta location | | | Answered: scratchpad `sbank` (gamma int8, beta int32), not weight SRAM |
| H11 | Is average-pool in the ISA, which field | ELEM_WISE mode 5, `SO` / `SHO` = Avg_Scale / Avg_Shift | | Partly answered: it is an ELEM_WISE pipeline; ISA field not given |
| H12 | Residual addition inside the OBP, or only in ELEM_WISE | ELEM_WISE only | | Answered: GEMM_FUSED has no residual; residual only in ELEM_WISE |
| H13 | Precision of the transformer residual and LayerNorm output | | Needs >= int16 (section 3.6) | Conflict: residual and LayerNorm outputs are still int8 in the GVU specification |
| H14 | Over-access at full SRAM | Tolerated, counted | Section 3.5 | Open |
| MaxPool pad | Pad value and K/S/P fields | Pad -128 | | Open |

### 4.2 Indirect LUT, softmax and LayerNorm (R1 to R10)

| # | Question | Model assumption | Status |
|---|---|---|---|
| R1 | Meaning of `Base` and `W` before index assembly | Not used (log-scale mode only) | Answered: they belong to the PWL mode |
| R2 | Table format of reciprocal and rsqrt | Signed Q14 | Open |
| R3 | `>> 8` after the interpolation multiply: floor or rounded | Floor | Open |
| R4 | Reciprocal of 0 | Saturated result, flagged | Open |
| R5 | Softmax stage 3 shift before requant and precision P | | Partly answered: int16 mantissa x int8 exponent, round-shift by E_shift, then requant; precision P not given |
| R6 | Exp LUT output signed or unsigned, and its scale | | Open |
| R7 | Softmax stage 3 multiplexer | | Answered: selects E_shift from the reciprocal LUT (log-scale) or from the instruction decoder (PWL) |
| R8 | LayerNorm stage 3: comparator and subtraction around epsilon | | Open |
| R9 | LayerNorm stage 4: split of the int40 x int16 product | | Open |
| R10 | Gamma and beta location | | Answered (same as H10) |

### 4.3 Data flow controller (H15 to H21)

These concern the core, DMA and controller, which the GVU specification does not cover. All are **Open**. The
model's assumptions are listed in `SW_INTEGRATION_GUIDE.md`, section 7, and in `KNOWN_LIMITATIONS.md`,
section 7.

| # | Topic |
|---|---|
| H15 | Upsample x2: DMA mode, feeder addressing, or host |
| H16 | Instruction granularity (per layer or per tile); descriptor ring in DRAM |
| H17 | Bias preload: full read or broadcast |
| H18 | Convolution padding: DMA zero-fill, feeder, or pre-padded in DRAM |
| H19 | Packing of int8 results from 32-bit PSUM words; 3-D write descriptors |
| H20 | Completion reporting and instruction queue depth |
| H21 | How LUT, scale and shift tables are loaded |

### 4.4 Findings on the attention and LayerNorm drawings (H22 to H27)

Found with the independent integer references of FUSED_ATTN and LAYERNORM written from the drawings (9 attention
heads, three of them real ViT-B/16 heads; three real ViT-B/16 LayerNorms and random rows with outliers). With the
mask off, the real heads reach a cosine of 0.9968 to 0.9985 against floating point and the real LayerNorms 0.9966
to 0.9990, so the pipelines as drawn work; the items below are their limits. All **Open**.

| # | Finding | Measurement | Proposal | Model |
|---|---|---|---|---|
| H22 | A masked position (-128) is not suppressed: in softmax stage 2 the exponent table still returns a value of 5 to 7, not 0 | Causal head, L = 64: cosine 0.944 against floating point; 0.9997 when the exponent of masked positions is forced to 0 | Pass the mask bit to stage 2 and force the exponent to 0, or define the last table entry as 0 | Knob, off (as drawn); `KNOWN_LIMITATIONS.md`, section 7.1 |
| H23 | Phase 3 (A.V) corrects only the zero point of A; without the V term, V must be quantized symmetrically | | Confirm that a symmetric V is intended, or add the correction | Knob, off (as drawn) |
| H24 | Row and column sums of Q, K and V are kept in int16 and overflow when the head dimension or the sequence length exceeds 256 | ViT-B/16 (d = 64, L = 197): no overflow | int32 sums, or a stated limit of 256 | As drawn |
| H25 | The variance, requantized to int8 per tensor, lacks range: rows with a small variance fall on the lower bound of stage 3, and the epsilon is lost on the quantization grid | 9 of 32 random rows hit the bound; cosine per row down to 0.74 | Keep the variance in int16, or quantize it per row or logarithmically | As drawn |
| H26 | The normalized value is int8 in stage 4, clamped at about 8 standard deviations: rows with outlier channels lose accuracy | Cosine 0.86 to 0.95 on rows with outliers | With H13: normalized output of at least int16 | As drawn |
| H27 | Stage 5 clamps to int8 before beta is added | 689 to 1,303 saturations per 64 rows of ViT-B/16 with per-channel scales | Add beta before the final clamp | As drawn |

The effect on a whole network is the ViT-B/16 accuracy against floating point (`KNOWN_LIMITATIONS.md`, Q4;
`VERIFICATION_REPORT.md`, section 6.7).

### 4.5 Core, SRAM and DMA behaviour that limits performance (H28 to H36)

None of these items affects correctness; they cost cycles. H28 to H33 are MEASURED on the cycle-accurate core
(which equals the reference SAURIA model cycle for cycle); H28 and H29 were reproduced on the original SAURIA RTL
with Verilator. The tile planner already works around H28, H29 and H33 (`FRONTEND_GUIDE.md`, section 11.1); the
others need a hardware decision. Whole-network figures marked ESTIMATED were derived from the measured cycle laws
before the reference runs and are orders of magnitude, not results.

| # | Behaviour | Effect | Question or proposal |
|---|---|---|---|
| H28 | The array stalls about half of the time when a tile does not fill 32 columns or 32 rows. Same K = 1,728 (3x3): 7,193 cycles with 32 output channels, 14,054 with 16 (1.95x); 1x1, K = 576: 1.88x with 16 positions, 1.87x with 16 channels. On the RTL: 1.97x, 1.82x, 1.85x | 26.8 % of the core busy cycles in the baseline plan; avoided by planner rules 1, 2 and 4 | Intended behaviour or defect? Which feeder causes it, and can a feeder setting remove it? Otherwise the compiler must avoid tiles with fewer than 32 channels and 1x1 tiles with fewer than 32 positions |
| H29 | A 3x3 stride-2 convolution with 32 positions per context costs about 1.32 K per context; with 31 or fewer the penalty disappears. On the RTL: 2,663 cycles (32 positions) against 2,089 (31 or 24), 1.27x | Every stride-2 convolution of YOLOv8m; avoided by planner rule 3 | Is the cause the input window of 2 x 32 + 1 = 65 columns (above 64)? Then widen the window buffer to 65, or confirm 31 positions. Consequence for the interface: positions per context must be an explicit instruction field (`Y_USED`, `SW_INTEGRATION_GUIDE.md`, section 4.1) |
| H30 | The feeder counts positions in one dimension: a context cannot wrap over a row boundary, so a 3x3 convolution on a map 20, 40 or 80 wide leaves array rows unused | The largest remaining loss of spatial usage (run C: 74.58 %). Software can pack rows into blocks but is bound by tile height and multiples of 32 | A feeder that counts positions in two dimensions, so that a context wraps over the row boundary |
| H31 | A floor of about 230 cycles per context. The stall-free tile law (contexts x K + 281) holds only for K of about 230 or more; 1x1 with 64 input channels: 5,788 cycles for 24 contexts | Limits layers with a small K (`stem`, K = 27, 8.84 % PE utilization; 1x1 layers with 64 to 192 input channels); ESTIMATED 1.9 M cycles on YOLOv8m | Where does the context-switch floor come from (partial-sum drain, feeder reload), and can it be reduced? |
| H32 | A fixed cost of about 281 cycles per tile (configuration, start, drain) that cannot overlap with the previous tile, because the core must be configured before it starts | ESTIMATED 2.4 M cycles on YOLOv8m | Double-buffered (shadow) configuration registers, loaded while the current tile runs |
| H33 | SRAM-B (81 KB per buffer) cannot hold the weights of 32 output channels for the six 3x3 layers with 384 or 576 input channels, hence the input-channel split | The split costs a reload of the input window per part; the partial sums stay in SRAM-C, so no partial-sum DMA | Low priority: SRAM-B of at least 165,888 bytes per buffer removes the split. Related question: may one half of SRAM-C be kept (not swapped) between two core runs to accumulate partial sums? The model assumes yes |
| H34 | DMA bus width. Once the planner raises the PE utilization, more layers become limited by the DMA on a 128-bit AXI bus (large maps, 1x1 layers) | ESTIMATED: a 256-bit bus saves about 4 % of the total at a PE utilization of 59 % and up to about 14 % at a higher utilization | Actual AXI or NoC width for the NPU, 128 or 256 bit? How many outstanding transactions? |
| H35 | SRAM-A (79 KB per buffer) limits the tile height, which makes tiles reread the rows they share | ESTIMATED: no gain from 128 KB at a PE utilization of 59 %, up to about 12 % at a higher utilization | Can SRAM-A be 128 KB per buffer or more? |
| H36 | DRAM latency has no hardware value. With 100 cycles on a DMA-limited window: 1,806,464 to 1,894,033 cycles, +4.85 % (MEASURED, bit-exact) | ESTIMATED 1 to 5 % on the whole network (`PERFORMANCE_REPORT.md`, section 5.4) | Typical DRAM latency, and the number of read requests the DMA may keep outstanding? |

Conclusion for the hardware team: bus width and SRAM-A size pay off only after the tiling has raised the PE
utilization; at the level of the reference run the core, not the DMA, limits almost every layer (run D,
`PERFORMANCE_REPORT.md`, section 5.5). The remaining core losses are H30, H31 and H32.

## 5. Changes required by the GVU specification

| # | Change | Affects |
|---|---|---|
| G1 | Add the PWL index mode to the LUT | `has/gvu_lut.h`; SOFTMAX |
| G2 | Softmax stage 2 uses `max - x` (non-negative) | SOFTMAX implementation |
| G3 | Split the scratchpad into `sbank` (int32, two ports), `vbank#0` (v32int8), `vbank#1/#2` (v32int32), each ping-pong with the external DMA | `has/gvu_scratchpad.h`; ELEM_WISE timing |
| G4 | OBP and RCE share functional units in one GVU: a GEMM_FUSED epilogue cannot run in parallel with softmax, LayerNorm or ELEM_WISE | Scheduling in `HasNpuTop` |
| G5 | Use the per-unit pipeline depths of the GVU specification instead of estimated latencies | `has/gvu_obp.h`, `has/gvu_elemwise.h` |

None of these changes affects the validated YOLOv8m results: YOLOv8m uses neither the PWL mode nor softmax, and
the scratchpad and latency changes alter timing estimates only.
