# Known Limitations and Open Issues

This document lists what the models do not represent, the issues that are open, and the risks to keep in
mind when using or extending the package. Each item states its impact on the published results.

**Default configuration.** The vector unit is now connected as described in `GVU_TOP_CORE_INTEGRATION.md`: with no variable set,
`HAS_GVU_TOP=3 HAS_GVU_STALL=1 HAS_GVU_ELEM_CLK=1 HAS_GVU_REGS=1 HAS_GVU_AGU_ND=1`. Items below that say "off by default" for these
knobs were written for the earlier default; O25 to O30 are brought up to date. The reference numbers of the earlier default
(`PERFORMANCE_REPORT.md`, section 5.8) belong to the configuration with the five knobs set to 0.

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
| O17 | Through a system wrapper, an extended instruction that `HasNpuTop` refuses at the push is still reported as done by the wrapper, and the internal error counters of a run are not visible in any register | Read the model's status register (bit 2) after every push; the model root prints a warning line on stderr in both cases (`SW_EVALUATION_GUIDE.md`, section 4) | Wrong results without an error |
| O18 | Through a system wrapper, pushing an extended instruction while another one runs, or writing into the DRAM window during a run, corrupts data without any error (measured: `SW_EVALUATION_GUIDE.md`, section 2.1). The behaviour is that of the wrapper, which reloads and copies back the whole window | One instruction at a time; wait for the DONE bit of the wrapper | Results that depend on timing |
| O19 | `tools/has/tb_has_npu_top` does not run or check an instruction that the decoder refuses at the push: it prints a `REFUSED` line with the error code, goes on with the next instruction and ends with FAIL. This includes a recording made with `SAURIA_CORE_RTL_RECORD` of a run with a refused push | Read the error code (`docs/SW_EVALUATION_GUIDE.md`, section 4) | Interpretation |
| O20 | `HAS_ELEM_RTL=1` (ELEM_WISE ADD and MAX_POOL from the cycle-level pipelines) and `HAS_OBP_RTL=1` (GEMM_FUSED epilogue through the cycle-level pipeline) were measured on the whole YOLOv8m (bit-exact, no value different from the model) and, for ViT-B/16, only on part of its instructions. `HAS_OBP_RTL=1` compares values and sets the tail of a core pass, it is not a clocked module on the write path | Off by default; the reference results do not use it | None for the default configuration |
| O21 | Padding by the IFMAP feeder (`HAS_FEEDER_PAD=1`, `HARDWARE_CONFIG.md`, section 6.5; design for an RTL implementation in `IFMAP_FEEDER_PADDING.md`) is a design of this model; no hardware description exists for it and the timing of its comparators has not been evaluated. Tiles of layers with a 1x1 kernel and stride 1, and tiles whose input window lies entirely in the padding, keep the staged flow (the testbench line "padding:" counts them). On some layer shapes the feeder's FIFO-full and stall flags change at other cycles than with staged padding; results were equal on everything measured, and core busy cycles too except one tile of a dilated layer whose window is much wider than its real part (+8 of 8,950 cycles; `VERIFICATION_REPORT.md`, V31, V33) | Off by default; the reference results do not use it | None for the default configuration |
| O22 | One `STRIDE` serves both directions for every convolution. Dilation is given by `DILATION` with `(KW - 1) x DILATION` at most 63; halo reuse (`--profile proposals`) is not applied to dilated layers. The report file (`SAURIA_CORE_RTL_REPORT`) gives the padding per side, the padding value and the dilation in its last six columns; its `pad` column is the `PAD` register only | Read the last six columns | Layers that need different strides, or a wider dilated kernel, must be lowered by the compiler |
| O23 | Before the correction below, a GEMM_FUSED tile could be computed wrongly without any error, in two ways. (a) The last context of the tile got no weights, so its output positions held the bias only. In the model of the core, the "done" pulse of the weight counter reached the repetition counter one cycle late and gated by the enable of the previous cycle; when the weight FIFO (4 positions) filled right after the counter wrap that ends the last but one repetition, the weights of the last context were flagged as out of bounds. With 32 output channels per tile and no pause of the activation side this happens for K = 2 with 5 contexts, K = 3 from 5 contexts on, K = 4 with 3, K = 8 with 2 and K = 9 from 3 contexts on (K = `KH x KW x` input channels of the core pass; all MEASURED, as predicted from the mechanism). A tile with a larger K is exposed only when the activation side pauses at that one step (seen in the attention products of the transformer check at other repetitions, never at the last but one). (b) K = 1 (configuration value `INCNTLIM` = 0) ran with two multiply-accumulates per context, at any number of output channels. The hardware description computes all these tiles correctly (RTL simulation of 12 shapes, output compared byte by byte with the golden) | Resolved: build options `FX1_A3_WEI_DONE_Q_GATE` and `FX1_A3_INCNTLIM_ZERO_RTL`, on by default (`HARDWARE_CONFIG.md`, section 4). With them, results and cycles are unchanged on every program measured with K of 2 or more outside these shapes (YOLOv8m window of instructions 40 and 41, transformer check, padding and random-layer programs). Regression item "short-context layers" (19 layers) in `tools/has/regress_gvu.sh`; program `tools/has/make_pad_gate.py --set smallk`. Earlier releases have the defect: there, avoid K = 1, and K up to 9 with 32 output channels per tile | Resolved. Neither network of this package has an affected tile (smallest K is 27, YOLOv8m stem) |
| O24 | One row sweep of the IFMAP feeder is limited to 256 positions by the 8-bit position counters of its lanes, a property of the SAURIA design: `xlim = 1 + (Y_used - 1) x stride + W + 1 - (W mod 2) + 32`, `W = (KW - 1) x dilation + 1`. MEASURED on the core and on the SAURIA RTL with the same shapes: 256 right, 257 wrong without any error, larger values wrong or hanging (3x3 stride 8 with 32 positions per context hangs both). The same limit holds with the padding by the feeder, whose mask uses the same counter width. Cause proven: with the lane counters widened to 12 bits, every failing shape is right on the core and on the SAURIA RTL (`VERIFICATION_REPORT.md`, V34). Only large strides or dilations reach it; the two reference networks stay far below | The instruction decoder refuses such an instruction at the push with error `SWEEP` (11) | Choose `Y_USED` (positions per context) so that every tile stays within the limit (`TILING_GUIDE.md`, section 3) |
| O25 | `HAS_GVU_TOP=1` or `2` (GEMM epilogue, residual add and max pool on the vector-unit top level `GvuTop`) runs each epilogue beat through `GvuTop` to completion inside the write of the partial-sum manager; the clocked connection is `HAS_GVU_TOP=3` (O26). Max pool on `GvuTop` covers the window and stride pairs of the hardware RTL (2x2 stride 1 and 2, 3x3 stride 1, 4x4 stride 1, 2 and 4, 5x5 stride 1); other pairs stay on the cycle-level pipeline and are counted. In the tile mode of GEMM_FUSED the per-channel scale and shift read from the sbank overlap and are not usable as written; the stream mode used by the instruction path carries them with the beat. Measured on YOLOv8m instructions 86 to 88, window D, the residual adds of both networks and the three SPPF max pools (`VERIFICATION_REPORT.md`, V35, V36) | Not the default any more (the default is level 3, O26); the reference numbers of the earlier default use `HAS_GVU_TOP=0`. Open points: `GVU_TOP_CORE_INTEGRATION.md`, section 7 | None |
| O26 | The default configuration (`HAS_GVU_TOP=3 HAS_GVU_STALL=1 HAS_GVU_ELEM_CLK=1 HAS_GVU_REGS=1 HAS_GVU_AGU_ND=1`, `GVU_TOP_CORE_INTEGRATION.md`) differs from the hardware RTL where the RTL is open, wrong or not there yet: the int8 result is written back into SRAM-C instead of `vbank#2` (question F8); Out_Scale / Out_Shift travel with the packet instead of being read from the sbank (question F2; the model counts the beats where the RTL's index would be wrong); the layer ends on the packet that holds the last element of the tile, where the RTL as delivered leaves 9 beats after the first `oc_tile_last & oc_last` (patches B9 and F1′ of the feedback package); the stream pipeline has the 9 stages of the specification (the RTL measures 11; `GVU_GEMM_STREAM_DEPTH=11` reproduces it); the narrowing after the round-shift saturates to 16 bits by default (the RTL wraps; `HAS_REQ_NARROW=1`); the configuration of `GvuTop` goes through the GVU registers, except the rounding mode and narrowing of GEMM_FUSED, the per-channel scale and shift and the operands, for which the RTL has no register (O27, O28). No reference program fills the FIFO of the stream, so the core stop on back-pressure is exercised only by a test that holds the ready of the unit low | Setting the five knobs to 0 returns to the model's own epilogue, add and max pool (the configuration of the numbers in `PERFORMANCE_REPORT.md`, section 5.8). Full list with reasons: `GVU_TOP_CORE_INTEGRATION.md`, sections 3, 4 and 7 | None for the numbers of the earlier configuration; the vector-unit timing is that of the specification plus the estimated register writes (O27), not that of the RTL |
| O27 | The latency of one register write as the data flow controller sees it (`HAS_GVU_REG_LAT`, 4 clock edges, minimum 2) is an estimate: the AXI-Lite path into `gvu_core` is not modelled and the DFC thread waits while a sequence is written. Through YOLOv8m the model makes 15,461 writes (387 GEMM_FUSED, 252 max pool, 14,822 residual add), 61,844 edges at the write port at 4 edges per write (in the continuous default run, with the descriptors of the address generator: 20,036 writes; ViT-B/16 11,952); the cycle count grows by 0.003 % because most writes overlap the DMA | The totals scale linearly with the latency (0.05 % at 2 edges, 0.17 % at 7, ESTIMATED); set `HAS_GVU_REG_LAT` to try another value. The hardware team gives the real number | Interpretation |
| O28 | Registers 9 to 15 (residual add: scales, shifts, rounding mode, zero points, row addresses, last-row lane mask), the opcode-6 path of the controller and registers 16 to 24 (address generator for slice and concat, `SLICE_CONCAT_ON_GVU.md`) are proposals of the model team. The RTL as delivered has neither: its controller has no state for the residual add and its register file has 16 registers. The proposals exist as patches in the hardware feedback package (residual add: 50 cases, 3,082 rows equal to the model in ModelSim) but `gvu_core.sv` with the patches has not been compiled by anyone, because the AXI library is missing | The model runs the proposals as the reference; with `HAS_GVU_REGS_ADD=0` the residual add stays outside the register file. Requests to the hardware team: `GVU_TOP_CORE_INTEGRATION.md`, section 7 | Interpretation: the cycles and values of the residual add through registers are those of the proposal |
| O29 | Softmax, LayerNorm and FUSED_ATTN (ViT-B/16) do not run on `GvuTop` on the instruction path: they stay on the functional models, so their cycles are the model's. The unit has their pipelines and test benches, and the register file has the Softmax registers (Reg 4 to 6), but the instruction path does not call them. LayerNorm needs about 17 fields, several 64 bits wide, which do not fit the registers still free (25 to 31) | Open question for the hardware team (register block of its own, 64 registers, or descriptors in the instruction). ViT-B/16 numbers include an estimated vector-unit time for these instructions (`PERFORMANCE_REPORT.md`, section 5.8) | Interpretation |
| O30 | The cycle count of each instruction of a window run (`tb_has_npu_top --first F --count N`) is read at the poll of the test bench, every 67 cycles, and the window starts cold from the golden DRAM image. A difference below about 134 cycles per instruction between two window runs, or between a window run and a continuous run, is the step of the bench, not of the model: the same window with the register file on and off differs by -67, 0 or +67 cycles per instruction (sum +201 over 17 instructions). The sum of the cycles of the windows is not the cycle count of a continuous run | Compare windows by the sum of their instructions; measure a difference below a few hundred cycles with a bench that polls every cycle, both sides built from the same copy; take whole-network cycle counts from continuous runs | Interpretation |
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
| H8 scratchpad capacity | Partly answered: 128 KB double-buffered in total; the size of each bank is not specified | `HAS_SPEC`, `HAS_SP_PRESET`, `HAS_SP_V0/V1/V2`, `HAS_SP_S`, `HAS_SP_LIMIT_ROWS`, `HAS_SCRATCH_BYTES` | banked, 10,784 / 5,600 / 43,008 bytes and a 6,144-byte scalar bank per half, rows per group limited by the banks (`recommended`); 24,576 bytes in one block (`legacy`) | any size; `HAS_SPEC=v47ref` = 4 / 4 / 16 KB per half and 32 rows per group; `mas128k` = the typical column of the specification's capacity table |
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
| Softmax and LayerNorm pipeline latency | Estimate | `has/gvu_softmax.h` `softmax_cycles`, `has/has_npu_top.h` `ln_cycles` | 3L + 26 and 4H + 48 per group of rows (32 rows, or fewer when the scratchpad limits the group: 14 rows for 768 channels) | code change |

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
| H18 convolution padding | Open | Staging region in `has/has_npu_top.h`; `HAS_FEEDER_PAD` | 3-D DMA that fills the padding (modelled through a staging area) | 1 = the IFMAP feeder generates the padding (`HARDWARE_CONFIG.md`, section 6.5) |
| H19 int8 output packing | Open | `has/has_npu_top.h` write-back | DMA packs, 3-D descriptor | |
| H20 completion reporting, queue depth | Open | `has::HasNpuTop` constructor `q_depth`; status register | completed-instruction counter and end-of-stream interrupt; depth 16; full queue = error | any depth |
| H21 table loading | Open | Data flow controller in `has/has_npu_top.h` | activation table and output zero point loaded once per GEMM_FUSED instruction, scale and shift per tile | |
| Controller issue timing | No hardware value | `has/has_npu_top.h` | 1 cycle per register write, 4 cycles decode | |
| Epilogue position | Model simplification | `has/has_npu_top.h` `set_obp_inline()` | on the partial-sum path (`recommended` and `proposals` profiles) | after the core, reading the partial-sum SRAM (`legacy` profile and the testbench runs) |
