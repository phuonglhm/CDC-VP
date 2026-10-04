# Known Limitations and Open Issues

This document lists what the models do not represent, the issues that are open, and the risks to keep in
mind when using or extending the package. Each item states its impact on the published results.

Impact levels:
- **None**: does not affect any published result.
- **Interpretation**: affects how a number must be read, not its correctness.
- **Latent**: correct today, but could produce wrong results if inputs or plans change.

## 1. Modelling scope

### 1.1 RTL-ref network testbench

| # | Limitation | Impact |
|---|---|---|
| M1 | Staging SRAM and core SRAM are two separate instances. The testbench copies data between them without simulated cycles | Interpretation: DMA-to-core transfer time is not modelled |
| M2 | The OBP receives the tile result from the testbench after the core has finished, not as a stream from the partial-sum manager | Interpretation: epilogue time is added after the core instead of overlapping it |
| M3 | Slice, concat, max-pool (standard program) and upsample run on the host with zero simulated cycles | Interpretation: 34 of 117 steps have no timing |
| M4 | DMA models burst scheduling at 32 bytes per cycle but no DRAM latency or bandwidth contention. The HAS specifies a 128-bit AXI bus (16 bytes per beat), so DMA time in the published runs is optimistic by about 2x. Core cycles are not affected | Interpretation |
| M8 | The full-network runs of this section are driven by a testbench, not by memory-mapped instructions. The delivery top level `HasNpuTop` runs the whole network from memory-mapped instructions (`VERIFICATION_REPORT.md`, V12 to V18) and removes M1, M2, M4 and M5 in its default profile | Interpretation: see `SW_INTEGRATION_GUIDE.md`, section 1 |
| M5 | Tiles run sequentially. Only the input window of the next tile is prefetched; weights and bias preload of the next tile are not | Interpretation: about 20 % of each tile's testbench time is non-core time that real hardware could overlap |
| M6 | The epilogue output is stored as one 32-bit word per int8 result, so the write DMA moves four times the necessary data | Interpretation: inflates DMA write cycles |
| M7 | Reset (5 cycles) and register programming (about 45 writes of 2 cycles) are repeated for every tile | Interpretation: about 110 cycles per tile |

Consequence: the total simulated cycle count of a network run (131.6 M cycles for YOLOv8m with the v4.5 OBP,
133.6 M with the HAS blocks) is a testbench time, not a hardware latency. The core busy cycles are identical in
both runs (103.0 M). Core cycles are measured per tile and reported separately in `PERFORMANCE_REPORT.md`.

### 1.2 Native model (`npu_top.h`)

| # | Limitation | Impact |
|---|---|---|
| N0 | The native model (revision v4.6) is the legacy functional model and regression baseline. It is not the computation path of the delivered model; changes to it arrive as new native revisions | Interpretation |
| N1 | In the default build, instructions are executed in double precision by the decoder (`emulate_*()`). The array, feeders, PSM, OBP and RE/RCE modules are wired and unit-tested, but not driven by the instruction stream | Interpretation: full-network tests of this model do not measure clock-level behaviour |
| N2 | `tools/onnx_compiler.py` lowers convolutions as plain matrix products on the last tensor dimension (no im2col, kernel, stride or padding), clips dimensions to 4096, and fills missing tensors with random values. Its golden is computed with the same simplified arithmetic | Interpretation: `test_yolo`, `test_onnx_model` and `test_vit` check compiler-to-model consistency, not the semantics of the real networks. Real-network correctness is established by the frontend flow (`FRONTEND_GUIDE.md`) and the RTL-ref testbenches |

## 2. Open issues

| # | Issue | Status and workaround | Impact |
|---|---|---|---|
| O1 | The first tile of layer `det.p3.box_out` fails when planned with a tile height of 24 (the tile fills 100 % of one SRAM-C buffer). 149 of 150 tile shapes pass | The planner limits `det.p3.box_out` and `det.p4.box_out` to a tile height of 4 by default. With this limit, the full network passes. Root cause not identified | Latent: another layer planned to fill SRAM-C exactly could hit the same failure |
| O2 | Tiles that fill SRAM-B or SRAM-C exactly issue one extra access at the address equal to the capacity. The value is not used and results are correct | Reported to hardware as question H14 (`HAS_VALIDATION.md`). A planner margin of one row would avoid it | Latent: a real SRAM of exact size would receive an out-of-range address |
| O3 | Weight tiles larger than one weight buffer (splitting weights across both buffers and switching `i_select` mid-tile) fail on `dark5.conv` | Disabled: every layer uses a single weight buffer (`WIDE_WEIGHT_JOBS` is empty in `fe_tile_plan.py`) | None today; limits future planner options |
| O4 | In the network testbench of section 1.1 the second weight buffer is never filled ahead of time, so weight double-buffering is not exercised there | `HasNpuTop` loads the next weights into the host half of the ping-pong SRAM and reuses a block already held by the other buffer (`PERFORMANCE_REPORT.md`, section 5.4) | Interpretation: testbench runs only |
| O5 | The core SRAM-C is instantiated with 1,536 rows per buffer instead of the specified 768 (total 192 KB) | Planner never uses more than 768 rows; `FX1_SRAM_CAP_CHECK` counts any access beyond the specification | Latent |
| O6 | Outside the capacity-check build, SRAM addresses wrap silently and cycle-free SRAM loads drop out-of-range bytes silently | Build with `FX1_SRAM_CAP_CHECK` when changing the planner or the SRAM sizes | Latent |
| O7 | The v4.5 OBP derives the channel of each vector from an internal counter that resets only when its pipeline is empty. The testbench inserts 6 idle cycles between contexts to guarantee this. The HAS OBP does not have this dependency | Keep `IDLE_CYCLES >= 4` when using the v4.5 OBP | Latent |
| O8 | The core path uses the vertical stride for both directions. The C++ reference convolution uses both | All YOLOv8m convolutions have equal strides. Unequal strides are not rejected | Latent |
| O9 | Staging-region sizes in DRAM are not checked by the testbench; they rely on the planner limits | Planner enforces the limits | Latent |
| O10 | A tile that exceeds `FE_CORE_MAX_CYCLES` and a tile that stalls are reported by the same counter (`core deadlocks`) | Distinguish by checking whether the tile consumed the full cycle budget | Interpretation |
| O11 | A failing step does not stop the run. Later steps consume the wrong data and fail as well | Read the first `FAIL` line of the log | Interpretation |
| O12 | The tile plan depends on the cost table `tools/eval_sw/out/prep/shape_table.csv`. Without it, the planner silently changes objective | Keep the table with the package (it is shipped); the planner prints a warning when it is missing | Latent: reproducibility |
| O13 | Host operations do not check tensor shapes (slice and concat assume equal height and width) | Shapes are guaranteed by the frontend | Latent |
| O14 | `FE_STEP_FIRST` without `FE_RESUME_FROM` or `FE_CORE_GOLD_INPUT` runs later steps on uninitialized tensors | Documented in `BUILD_AND_RUN.md` | None |
| O15 | Runs A and B of `PERFORMANCE_REPORT.md`, section 5.4, were produced before a fix in the data flow controller: weights of each input-channel part were reloaded although the other buffer already held them | Resolved: run C uses the corrected controller and is the reference total. Runs A and B are kept for traceability only. Results are bit-exact in all three | Resolved |
| O16 | `tools/has/tb_has_npu_top` executes only the host step `H upsample2x`. `tools/fe/fe_emit_insts.py` emits `H copy` for a concatenation it cannot map by address; such a line is reported as an unknown host line and counted as an MMIO error | The shipped programs contain only `upsample2x` steps. Implement `H copy` in the testbench before using a network that needs it | Latent |

## 3. Performance characteristics

| # | Observation | Impact |
|---|---|---|
| P1 | Reference run C: network PE utilization 69.27 % on real MACs; per layer 8.8 % (first layer) to 89.2 %, median 61.5 %. The five layers under 40 % hold 3.9 % of the core busy cycles (`PERFORMANCE_REPORT.md`, section 3.3). The plan of the earlier testbench runs reached 37.4 % | Interpretation |
| P2 | The first layer has 27 multiply-accumulates per context (3 input channels), so context switching and partial-sum drain dominate whatever the tiling. The other low layers of the earlier plan (16 output channels by 16 positions) were raised by the planner options of `ARCHITECTURE.md`, section 8.2 | Interpretation |
| P3 | The planner objective matters: the cost-model objective halves the core busy cycles of the minimum-tile-count objective (`PERFORMANCE_REPORT.md`, section 3.2). Keep the cost table in place (O12) | Interpretation |

## 4. Quantization and quality

| # | Limitation | Impact |
|---|---|---|
| Q1 | Calibration uses the 8 coco8 images, the same images used for the quality comparison | Quality numbers compare configurations; they are not an accuracy measurement |
| Q2 | No mAP evaluation has been run. On 112 coco128 images the integer YOLOv8m finds 90 % of the boxes of the floating-point model, with 94 % of its boxes matching (`VERIFICATION_REPORT.md`, section 6.7); four images were run end to end through the model | Interpretation |
| Q3 | All activations are symmetric int8 with zero point 0; scale groups force equal scales across concatenations | Design choice |
| Q4 | ViT-B/16: LayerNorm is quantized per tensor. Outlier tokens (the class token) are distorted, and the attention concentrates on them; the cosine similarity of the logits against the floating-point model is 0.36 to 0.67 over the tested images, and the top-1 class is the same on 3 of 5 images (`VERIFICATION_REPORT.md`, section 6.7). The int8 logits also saturate: on one image the two best classes both reach 127, so the integer network gives them the same score (0.45 each) where the floating-point model gives 0.82 to the first. Per-row parameters or a wider normalized range need a hardware decision | Accuracy, not bit-exactness |
| Q5 | Channels whose weights are almost zero get very small per-channel scales; the quantized bias can then exceed the int32 range of the partial sums. The frontend enforces a scale floor so that bias and accumulation stay within int32 | Required rule for any compiler targeting this NPU |

## 5. HAS blocks

| # | Limitation | Impact |
|---|---|---|
| H1 | Several details of the hardware drawings are not yet confirmed (rounding, narrowing, scratchpad size and others). The model exposes them as options with documented defaults | See section 7 and `HAS_VALIDATION.md`, section 4 |
| H2 | SOFTMAX, LAYERNORM and FUSED_ATTN are implemented bit-exactly and run from the instruction stream (a whole ViT-B/16 passes). Their pipeline and DMA timing is estimated, and the operand DMA is not overlapped with the core | ViT totals carry a larger estimated share (13 %) than YOLOv8m |
| H3 | Pipeline latencies of the HAS blocks are estimates taken from the drawings | Interpretation |
| H4 | How upsampling, bias preload, padding, output packing, completion reporting and table loading are done in hardware is not yet specified (questions H15 to H21 in `SW_INTEGRATION_GUIDE.md`, section 7). The model uses documented assumptions | The extension registers or data layout may change |
| H5 | The previous decoder's ELEM_WISE MUL and SUB modes, the second lane and the 64-bit instruction interface are not part of the HAS and are rejected with an error code | Software change required |

## 6. Environment

| # | Limitation |
|---|---|
| E1 | Builds and runs are supported on Linux only (tested on Ubuntu 22.04 with g++ 11 and SystemC 2.3.3) |
| E2 | A whole-network run of `tools/has/tb_has_npu_top` takes about 10 hours (YOLOv8m) or 5 hours (ViT-B/16) on one core; a chained run of the network testbench of section 1.1 about 14 to 15 hours on 4 cores. Use windows (`--first F --count N`) or layer-isolated runs (`FE_CORE_GOLD_INPUT=1`) for quicker checks |
| E3 | Each exported network needs about 110 MB per DRAM image, and each snapshot another 110 MB |
| E4 | The Makefile tracks only `.cpp` files; header changes require a forced rebuild (`tools/has/regress_native.sh`) |

## 7. Model parameters that stand in for open hardware questions

Where the HAS drawings or the GVU specification leave a choice open, the model implements a parameter (knob)
or a fixed assumption. Every result in this package was produced with the default in the third column. When
hardware answers a question, change the knob (or the code in the last column) and rerun the unit vectors and
the affected network check. Question texts and evidence are in `HAS_VALIDATION.md`, section 4.

Status: **Open**, **Partly answered**, **Answered** (model must follow the answer), **Conflict** (hardware chose
differently from the model's evidence).

### 7.1 Epilogue, element-wise and LUT

| Question | Status | Model knob or location | Default used | Alternatives in the model |
|---|---|---|---|---|
| H1 rounding of Round-Shift | Open | `HAS_ROUND_MODE` (`has::Knobs::round_mode`) | 1 = HALF_UP | 0 FLOOR, 2 HALF_AWAY, 3 HALF_EVEN |
| H2 int64 to int16 narrowing | Open | `HAS_REQ_NARROW` | 0 = saturate | 1 wrap, 2 no narrowing |
| H3 dequant zero-point order | Partly answered | `HAS_DEQ_ZP_ORDER` | 0 = subtract before scale | 1 = add after round-shift |
| Scale word format | Model choice | `HAS_SCALE_FMT` | 0 = int32 | 1 = uint32 (v4.5 compatibility) |
| H4 softmax difference sign | Partly answered | `has/gvu_softmax.h` `SoftmaxCfg::sub_sat` | `max - x`, unsigned 0..255 | saturate at 127 |
| H5 average-pool accumulator width | Open | `has/gvu_elemwise.h` `avgpool()` | int16, wraps, counted in `acc16_ovf`; int8 saturation after shift | none (code change) |
| H6 MaxPool traversal | Open | `HAS_MP_MODE` | 0 = direct | 1 = separable |
| MaxPool pad value | Open | `has/gvu_elemwise.h` | -128 | none |
| H7 FUSED_ATTN scaling | Answered | `has/gvu_fused_attn.h` | one requant (Mqk, 1/sqrt(d) folded in), zero-point corrections | matches the answer |
| H8 scratchpad capacity | Open | `HAS_SCRATCH_BYTES`, `HAS_SP_V0/V1/V2`, `HAS_SP_PRESET` | banked, 4 / 4 / 16 KB per half (`recommended`); 24,576 bytes in one block (`legacy`) | any size; `gvu68k` = 16 / 16 / 32 KB per half |
| AVG_POOL padding | Open | ELEM_WISE mode 5 | no padding (`POOL_P` must be 0), as drawn | none |
| Pipeline register organisation | Specified | `has/gvu_vrf.h` | 3 x v32int64 + 4 x v32int32 with reconfiguration: all table-2 stages fit | an aliased 1 KB register file would not hold LayerNorm stage 2 (1,280 B) nor Residual Add stage 1 (1,152 B) |
| H9 PSUM SRAM to scratchpad transfer | Partly answered | `HAS_ELEM_DMA_LAT` (`has::Knobs::elem_dma_lat`), bus width | bytes / bus width + 20 cycles per transfer (ESTIMATED) | any latency; +0.5 % on the YOLOv8m total at 100 cycles (ESTIMATED from measured windows) |
| H10 / R10 LayerNorm gamma and beta | Answered | `LNP1` parameter block (`has/gvu_rce_params.h`) | gamma int8, beta int32 per column | residency in scratchpad `sbank` not modelled |
| H11 average-pool ISA field | Partly answered | `has/has_mmio_compat.h` | ELEM_WISE mode 5, `SO` / `SHO` = Avg_Scale / Avg_Shift | another mode number (code change) |
| H12 residual in OBP | Answered | `has/gvu_obp.h` | no residual in the epilogue | matches the answer |
| H13 transformer residual and LayerNorm output width | Conflict | `LNP1` 16-bit output flag (`has::LnParams::out_int16`) | int8 output | int16 output; the residual path stays int8 (evidence requires >= int16) |
| R1 PWL mode (`Base`, `W`) | Answered | `has/gvu_lut.h` `LutPwl` | log-scale mode in the network runs | PWL mode implemented and unit-tested (segment and uniform forms) |
| R2 reciprocal / rsqrt table format | Open | `has/gvu_lut.h` | signed Q14 | Q is a parameter of the table builder |
| R3 rounding after interpolation `>> 8` | Open | `has/gvu_lut.h` | floor | none |
| R4 reciprocal of 0 | Open | `has/gvu_lut.h` | saturated result, `zero_in` flag set | none |
| R5 softmax stage-3 precision | Partly answered | `SoftmaxCfg::P` | P = 15 | any |
| R6 exp LUT signedness and scale | Open | `ATN1` exponent table | unsigned 0..255 values | |
| R7 softmax stage-3 multiplexer | Answered | `has/gvu_softmax.h` | E_shift from the logarithmic reciprocal LUT | E_shift from the instruction (PWL mode) |
| R8 LayerNorm stage 3 comparator | Open | `has::LnParams::r8_floor_z_out`, `r8_z_out` | f <= Z_7 writes the stage-3 zero point `r8_z_out` = 1 | `max(f - Z_7, 1)` |
| R9 LayerNorm stage 4 product split | Open | `has::LnParams::r9_exact`, `r9_lsb_bits` | exact product (high and low partial products kept) | high partial product only (low bits dropped) |
| Epilogue latency | Estimate | `has::Knobs::lat_obp` (`--gvu-lat` selects the drawing stage counts) | 6 cycles; 10 in the runs with the vector-unit data flow | any value >= 1 |
| ELEM_WISE latencies | Estimate | `has::Knobs::lat_add1`, `lat_add2`, `lat_max` | 6, 5, 3 cycles; 8, 10, 3 in the runs with the vector-unit data flow | any |
| H22 masked positions in softmax stage 2 | Open | `ATN1` flag bit 1 | exponent not forced to 0 (as drawn) | forced to 0 |
| H23 V zero-point correction in A.V | Open | `ATN1` flag bit 2 | off (as drawn) | on |
| Softmax and LayerNorm pipeline latency | Estimate | `has/gvu_softmax.h` `softmax_cycles`, `has/has_npu_top.h` `ln_cycles` | 3L + 26 per 32 rows; 4H + 48 per 32 rows | code change |

### 7.2 Core SRAM, DMA and data flow controller

| Question | Status | Model knob or location | Default used | Alternatives in the model |
|---|---|---|---|---|
| H14 access at address = capacity | Open | Planner (`tools/fe/fe_tile_plan.py`); `FX1_SRAM_CAP_CHECK` counts it | no margin; tiles may fill a buffer exactly | margin of one row (not implemented) |
| SRAM capacities per buffer | Specified | `FX1_SRAM_REAL_{A,B,C}_BYTES` (checker only) | A 79 KB, B 81 KB, C 96 KB | |
| DMA bus width | Specified (HAS AXI-128) | `has::DmaParams::bytes_per_beat`; `--dma v45\|has` | 16 in `HasNpuTop --dma has`; 32 in the full-network testbench runs | 32 (v4.5 compatibility) |
| DMA burst length | Specified | `has::DmaParams::burst_beats` | 8 | |
| DRAM latency | No hardware value | `has::DmaParams::dram_latency`; `--dram-lat N` | 0 | any |
| CH3 priority (LUT, bias, scale) | Specified | `has::DmaParams::ch3_low_priority` | true with `--dma has` | false = round robin |
| H15 upsample x2 | Open | Host step | host, 0 cycles | |
| H16 instruction granularity | Open | `has/has_mmio_compat.h`, `has/has_tile_iter.h` | one instruction = one layer; the controller iterates tiles | |
| H17 bias preload | Open | DMA timing in `has/has_npu_top.h` | full `OH x OW x OC` int32 read | broadcast descriptor, 4 bytes per channel (`c_bias_bcast`, `proposals` profile, run D) |
| H18 convolution padding | Open | Staging region in `has/has_npu_top.h` | 3-D DMA with zero fill (modelled through a staging area) | |
| H19 int8 output packing | Open | `has/has_npu_top.h` write-back | DMA packs, 3-D descriptor | |
| H20 completion reporting, queue depth | Open | `has::HasNpuTop` constructor `q_depth`; status register | completed-instruction counter and end-of-stream interrupt; depth 16; full queue = error | any depth |
| H21 table loading | Open | Data flow controller in `has/has_npu_top.h` | activation table and output zero point loaded once per GEMM_FUSED instruction, scale and shift per tile | |
| Controller issue timing | No hardware value | `has/has_npu_top.h` | 1 cycle per register write, 4 cycles decode | |
| Epilogue position | Model simplification | `has/has_npu_top.h` `set_obp_inline()` | on the partial-sum path (`recommended` and `proposals` profiles) | after the core, reading the partial-sum SRAM (`legacy` profile and the testbench runs) |
