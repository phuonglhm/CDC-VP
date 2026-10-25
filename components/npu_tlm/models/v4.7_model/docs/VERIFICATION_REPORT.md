# Verification Report

This document states how the correctness of the delivered model was established and what each check proved.
Every number carries a label:

- **MEASURED**: produced by running the model; the source file is given.
- **DERIVED**: computed from measured numbers by a stated formula.
- **ESTIMATED**: produced by a formula or assumption, not by a run.

Performance numbers are in `PERFORMANCE_REPORT.md`; the arithmetic questions still open with the hardware
team are in `HAS_VALIDATION.md`; modelling simplifications are in `KNOWN_LIMITATIONS.md`.

**Default configuration.** The vector unit is now connected in the way described in `GVU_TOP_CORE_INTEGRATION.md`: with no variable
set, `HAS_GVU_TOP=3 HAS_GVU_STALL=1 HAS_GVU_ELEM_CLK=1 HAS_GVU_REGS=1 HAS_GVU_AGU_ND=1` (the clocked stream with the real FIFO and
back-pressure, the DFC driving `GvuTop` through the register file of `gvu_core`, residual add and max pool on the clock). The checks
V1 to V36 were made before that change; their numbers (for example 62,666,123 and 41,680,289 cycles) belong to the configuration
with the five knobs set to 0 (the model's own epilogue, add and max pool), which is still available by setting them to 0. Sections
6.14 and 6.15 (V37, V38) give the checks of the new default.

## 1. Verification strategy

Correctness is established in four layers. A block or flow is considered validated only when it passes all
layers that apply to it.

| Layer | What is compared | Pass criterion |
|---|---|---|
| 1. Independent reference | A Python implementation written from the specification, without reading the SystemC code | Exists and is reviewed against the specification |
| 2. Unit, bit-exact | The SystemC block against vector files produced by layer 1, including boundary cases | 0 mismatching outputs |
| 3. Core equivalence | The ported core (`rtl_ref_*`) against the reference SAURIA core model, per tile | Identical output bytes and identical cycle counts |
| 4. Network integration | A whole network chained through the cycle-accurate core (layer N consumes the real output of layer N-1) against an independent integer golden | 0 mismatching bytes in every tensor; detection boxes identical to the golden |

The golden references are produced by the Python frontend (`tools/fe`) in pure integer arithmetic. They share
no code with the SystemC model. Two goldens exist: the v4.5 OBP arithmetic (`fe_ref_int8`) and the arithmetic
of the HAS drawings (`fe_ref_has`).

### 1.1 Timing fidelity by block

Functional correctness and timing accuracy are separate properties. Every block of the NPU below is bit-exact
against the golden; only the SAURIA core is also cycle-accurate. The table states, for each block, which of the
two properties is established and by what evidence.

| Block | Functional result | Timing | Evidence |
|---|---|---|---|
| SAURIA core (feeders, 32x32 PE array, PSM, ping-pong SRAM, context controller) | Bit-exact | **Cycle-accurate** | Per-tile cycle equivalence with the reference SAURIA core model (V1); every tile shape of the network covered (V2); the stall patterns that limit PE utilization (fewer than 32 output channels, 1x1 layers with fewer than 32 positions per context, 3x3 stride-2 with 32 positions) reproduced by a Verilator simulation of the original SAURIA RTL (MEASURED) |
| Output epilogue (OBP: requant, LUT activation), inline on the PSM to SRAM-C path | Bit-exact (V10, V12 to V15) | Modelled: pipeline latency of 10 cycles derived from the stage counts of the vector-unit drawing | ESTIMATED until the micro-architecture specification gives the value (`HAS_VALIDATION.md`) |
| ELEM_WISE (ADD, MAX_POOL; AVG_POOL through the instruction path) through the scratchpad | Bit-exact (V10, V12 to V15, V21) | Modelled: pipeline latency per stage and DMA time per transfer; ping-pong scratchpad banks per the drawing | ESTIMATED |
| RCE: Softmax, FUSED_ATTN, LAYERNORM, logarithmic and piecewise-linear LUTs | Bit-exact at unit level, through the instruction path (V17) and on the whole ViT-B/16 (V18) | Mixed: the two matrix products of attention run on the cycle-accurate core; softmax, LayerNorm stages and their DMA are modelled | Products MEASURED; the rest ESTIMATED |
| DMA (4 channels, AXI-128, 8-beat bursts, CH3 low priority) | Data verified through every output tensor | Modelled cycle by cycle with parameters (bytes per beat, burst length, DRAM latency) | No hardware reference exists; the model is self-consistent with measured per-layer totals within 0.6 % on average, which is not a comparison with silicon |
| Data flow controller (tile walk, ping-pong schedule, input-channel split, tile order, padded tiles) | Correct: every instruction of the full network passes | Sequenced cycle by cycle in SystemC, but it is a proposed design | No hardware drawing exists yet for the controller; its timing is a proposal, not a validated value |
| Proposed options: halo reuse in SRAM-A, bias preload by broadcast descriptor | Data unchanged (V16) | Modelled | Proposals to the hardware team, ESTIMATED; whole-network effect in `PERFORMANCE_REPORT.md`, section 5.5 |

**How to read the performance numbers.** In the reference whole-network run of YOLOv8m (`PERFORMANCE_REPORT.md`,
section 5.4, run C) the core is busy for 55,638,052 of 62,681,721 total cycles (DERIVED: about 89 %). That share
comes from the cycle-accurate core. The remaining cycles (DMA waits not hidden by the ping-pong schedule, epilogue
drain, configuration) depend on the modelled blocks above. Therefore:

- core busy cycles, PE utilization and throughput on core busy are reliable as measured;
- total cycles and frame rate are reliable to the extent of the DMA and vector-unit assumptions, and will move
  when the hardware team fixes bus width, DRAM latency and pipeline latencies (`KNOWN_LIMITATIONS.md`, section 7).
- for ViT-B/16 the core share is only 54.7 % for the GEMM instructions plus 7.9 % for the attention products (the
  rest is DMA wait and estimated vector-unit stages), so its
  total is much more sensitive to those assumptions (`PERFORMANCE_REPORT.md`, section 5.6).

Functional validation is against the integer golden of the frontend. It is independent of how close the
integer network is to the floating-point model; that accuracy question is outside this report.

## 2. Summary of results

| # | Check | Scope | Result | Label |
|---|---|---|---|---|
| V1 | Core equivalence, cycle level | 40 sampled tiles over 9 layers | 40/40 tiles: sum of absolute busy and execute cycle differences = 0 | MEASURED |
| V2 | Tile-shape coverage | 150 distinct tile shapes of YOLOv8m (6,924 tiles) | 149/150 shapes pass; 6,914/6,914 tiles of the passing shapes (99.86 % of all tiles) | MEASURED |
| V3 | Tile shapes at the specified SRAM capacity | Same 150 shapes, SRAM A/B/C at 79/81/96 KB per buffer, capacity checker enabled | 150/150 jobs identical to V2 (verdict, mismatch count, element count, cycles) | MEASURED |
| V4 | Full network, v4.5 OBP arithmetic, image 1 | YOLOv8m, 117 steps, image `000000000009.jpg` (coco8) | PASS, 82,540,800 bytes, 0 mismatches, 9,811/9,811 tiles on the core | MEASURED |
| V5 | Full network, v4.5 OBP arithmetic, image 2 | YOLOv8m, 117 steps, image `000000000443.jpg` (coco128) | PASS, 0 mismatches; cycle counts identical to V4 | MEASURED |
| V6 | Full network, HAS arithmetic | YOLOv8m, 129 steps (HAS OBP and ELEM_WISE), image of V4 | PASS, 88,915,200 bytes, 0 mismatches, 9,811/9,811 tiles | MEASURED |
| V7 | Instruction stream decoding | YOLOv8m as 98 v4.5-protocol instructions | 9,811/9,811 tiles and 15/15 element-wise steps decoded exactly | MEASURED |
| V8 | Instruction-driven top level, sequential | `HasNpuTop`, two windows of the network | 13/13 steps PASS, 0 mismatches | MEASURED |
| V9 | Instruction-driven top level, overlapped, HAS DMA | `HasNpuTop` with ping-pong SRAM and AXI-128 DMA, same windows | PASS, 0 mismatches | MEASURED |
| V10 | HAS blocks, unit bit-exact | Requant, dequant, direct and indirect LUT, ELEM_WISE | 0 mismatches on 120,000 to 400,000 independent vectors per file | MEASURED |
| V11 | Native regression | `tb_obp`, `test_vit`, `test_yolo`, `test_onnx_model`, `tb_unified_smoke`, rebuilt from current sources | All pass; network results identical to the previous binaries; `tb_obp` 17 / 17 including negative bias, table read-back, symmetric rounding and the output zero point | MEASURED |
| V12 | Full network through `HasNpuTop` (delivery acceptance) | YOLOv8m as 98 instructions, overlapped, HAS DMA | PASS, 58,425,600 elements, 0 mismatches, 9,811/9,811 tiles, 0 MMIO errors | MEASURED |
| V13 | Full network, GVU data flow + planner options (run A) | YOLOv8m, 98 instructions, 7,735 tiles / 8,559 core passes | PASS, 58,425,600 elements, 0 mismatches, 0 MMIO errors | MEASURED |
| V14 | Same, plus padded edge tiles, larger tiles, 31-position contexts (run B) | YOLOv8m, 98 instructions, 7,299 tiles / 7,695 core passes | PASS, 58,425,600 elements, 0 mismatches, 0 MMIO errors | MEASURED |
| V15 | Same as V14 with the corrected weight-buffer selection, padded edge tiles off on three layers (run C, current reference) | YOLOv8m, 98 instructions, 7,299 tiles / 7,695 core passes | PASS, 58,425,600 elements, 0 mismatches, 0 MMIO errors | MEASURED |
| V16 | Same as V15 with the two proposed hardware options (bias preload by broadcast descriptor, halo reuse in SRAM-A) and a planner refresh of four layers (run D) | YOLOv8m, 98 instructions, 7,301 tiles / 7,697 core passes | PASS, 58,425,600 elements, 0 mismatches, 0 MMIO errors | MEASURED |
| V17 | Transformer instructions through the instruction path | 9 FUSED_ATTN heads (sequence lengths 32 to 197, causal, asymmetric zero points, padded, three heads of a real ViT) and 5 LAYERNORM cases (H = 64 and 768), in both operand layouts (token-major and channel-major) | PASS, 14 of 14 instructions per layout, 259,968 elements, 0 mismatches | MEASURED |
| V18 | Full ViT-B/16 through `HasNpuTop` | Patch embedding, 12 encoder blocks, final LayerNorm and head: 244 instructions, attention products on the core | PASS, 26,068,616 elements, 0 mismatches, 0 MMIO errors | MEASURED |
| V22 | Second input image, hardware as drawn | coco128 image 000000000502 (not a calibration image), programs regenerated by the frontend with instruction streams identical to `insts_pe3` and `vit_full`; YOLOv8m default profile, ViT-B/16 without the bias broadcast | Both PASS, 0 mismatches. YOLOv8m 62,681,721 cycles (equal to V15), 1 detection box (bear, 0.935) identical to the golden; ViT-B/16 41,065,258 cycles (19.48 FPS), top-1 equal to the floating-point model | MEASURED |
| V23 | More input images (data dependence) | Step 1, golden against the floating-point model: YOLOv8m on 112 coco128 images, ViT-B/16 on 5 images. Step 2, through `HasNpuTop`, hardware as drawn: YOLOv8m on two more images chosen as the hardest of step 1 (most int8 clamps; most objects), ViT-B/16 on an image whose integer top-1 differs from the floating-point model | Step 2: all PASS, 0 mismatches; YOLOv8m 62,681,721 cycles on both images, equal to V15 on every instruction, boxes identical to the golden (11 of 11 and 29 of 29); ViT-B/16 41,065,258 cycles, equal to V22, top-1 equal to the golden. Step 1: no int16 saturation and no int32 risk on any image (section 6.7) | MEASURED |
| V24 | Wrapper revision that bundles the v4.6 native model, and the firmware engine | The wrapper's extended unit test (negative OBP bias, scale and shift read-back) against this tree as native root and as `core_rtl/` root, before and after the wrapper patch; program replay through the patched wrapper; the C replay engine of `tools/has/wrapper/fw/` on the host through the wrapper | Unit test output identical to the bundled v4.6 model in all four builds; transformer check 14 of 14 (250,885 cycles) and the first two YOLOv8m instructions (1,842,586 and 1,565,552 cycles, equal to the reference run) pass through the patched wrapper; the C engine passes the transformer check through the wrapper with the same cycles, reproduces both YOLOv8m host steps and all output CRCs of YOLOv8m (100) and ViT-B/16 (244) on the golden images, and detects a single corrupted byte | MEASURED |
| V25 | Default vector-unit configuration (scratchpad of the micro-architecture specification) | Both networks rerun through `HasNpuTop` with the default configuration, hardware as drawn; predictions recorded before the runs from per-instruction measurements (201 single-instruction runs in three configurations, all PASS) | YOLOv8m: PASS, 98 of 98 instructions, 0 mismatches, 62,666,123 cycles (predicted 62,666,177). ViT-B/16: PASS, 244 of 244, 0 mismatches, 41,680,289 cycles (predicted 41,679,983). `HAS_SPEC=v47ref` reproduces the earlier transformer-check cycles exactly (250,885; default 285,205) | MEASURED |
| V26 | Behaviours taken from the RTL of the functional units | Both networks with `HAS_SPEC=rtl`: LUT tables of 256 / 512 entries with the last entry held, LUT input 0 returns 0, wrapping zero-point adder, int32 scalar words, over the earlier scratchpad split. ViT-B/16 also with `HAS_SPEC=mas10` | Both PASS with 0 mismatches and totals equal to the earlier reference runs (62,681,721 and 41,065,258 cycles): no bit and no cycle changes. `mas10`: PASS, 42,192,302 cycles | MEASURED |
| V27 | Vector-unit items added from the micro-architecture specification | `tools/has/tb_gvu_mas` against the independent Python reference `tools/has/mas_ref.py`: rounding modes ceil, toward zero, away from zero; LUT with the 256 / 512 tables; log-scale LUT index and softmax with the log-scale reciprocal; scratchpad capacity table and port conflicts of the 16 pipeline stages; load/store unit ports and address generation; controller state traces; the default configuration. The frontend golden against the same reference (`tools/fe/fe_ref_gvu.py --only mas`); every existing vector file regenerated byte-identical | PASS | MEASURED |
| V28 | Native model at the current v4.6 revision | `tb_obp`, `tb_re`, `tb_pipeline_re`, `tb_unified_smoke` and the native regression rebuilt; the instruction-path regression repeated (this path includes none of the changed files); unit checks of the RTL-ref core that drive the native `Obp`, rebuilt with `tools/obp_port_compat.h` | 17 / 17, 32 / 32, 12 / 12, 32 / 32; `test_vit` 4 / 4, `test_yolo` 261 / 261, `test_onnx_model` 497 / 497; instruction path unchanged (transformer check and the two-instruction YOLOv8m window give the same cycles) | MEASURED |
| V30 | PE utilization through the wrapper, diagnostics, and this release as a whole | (a) Core busy counter of `HasNpuTop` against the instrumented build on a two-instruction YOLOv8m window (432 core passes). (b) `test_npu_tlm_core_rtl` built on the current wrapper sources: transformer check, PE utilization read from `PERF_ACTIVE_PE_CYCLES / PERF_TOTAL_PE_CYCLES`. (c) Warning lines: a refused instruction and an overwritten parameter block. (d) `SAURIA_CORE_RTL_RECORD`: the transformer check, the same program with a CPU step, and the first three instructions of a ViT-B/16 block, each replayed with `tb_has_npu_top`. (e) Both networks rerun through `HasNpuTop` in the default configuration with the binary built from this tree. (f) Misuse cases of `SW_EVALUATION_GUIDE.md`, section 2.1 | (a) 2 cycles per core pass above the instrumented count (0.06 %); cycles unchanged. (b) 14 of 14, 285,205 cycles, 23.18 %, equal to the testbench. (c) One line each; none on a correct run. (d) Every replay bit-exact; cycles per instruction equal (66,880 / 536,910 / 29,420 on the ViT-B/16 block); two segments for the program with a CPU step, 285,205 cycles in total. (e) YOLOv8m PASS, 62,666,123 cycles, PE utilization 69.26 %; ViT-B/16 PASS, 41,680,289 cycles, 69.61 %: both equal to the reference runs cycle for cycle. (f) As listed there | MEASURED |
| V31 | Convolution padding by the IFMAP feeder (`HAS_FEEDER_PAD=1`, `HARDWARE_CONFIG.md`, section 6.5) and the padding fields of the instruction (`PAD_VALUE`, `PAD_T`, `PAD_L`, `PAD_B`, `PAD_R`) | Check program of 13 convolutions (per-side padding, padding values -128 to 127, kernels 1x1 to 7x7, strides 1 and 2, several tiles, input-channel split) against explicit loops and `tools/has/ref_program.py`, both ways of padding, four run profiles; random layers; unit test of the padding generator; one-line faults; sanitizer and SRAM capacity builds; the system wrapper; whole YOLOv8m with both ways of padding against a prediction written before the runs (section 6.8) | All PASS, 0 mismatches. YOLOv8m: 62,666,123 cycles with the padding in SRAM-A, 62,698,248 with the feeder (predicted 62,698,171 +- 6,566), 4,006 core passes padded by the feeder, core busy 55,653,442 in both. With the option off, cycles equal to the reference runs | MEASURED |
| V32 | Correction of short contexts in the RTL-ref core (`KNOWN_LIMITATIONS.md`, O23) | The SAURIA RTL (Verilator) on 12 short-context shapes with a byte compare of the whole output; the short-context program `make_pad_gate.py --set smallk` (19 layers) with and without the correction; the seed-1 random padding set (43 layers) both ways of padding; the reference programs; the whole networks | RTL: 12 / 12 right. Without the correction 12 of 19 short-context layers wrong, with it 19 / 19. Random set 43 / 43 with both ways of padding (before: 41 / 43), core busy 5,319,656 both. Window D 1,658,986, instructions 86 to 88 280,003, transformer check 14 / 14, padding program unchanged; regression 42 / 42. Whole YOLOv8m and ViT-B/16 PASS with unchanged cycles (section 6.9) | MEASURED |
| V33 | Dilation (`DILATION`, `SW_INTEGRATION_GUIDE.md`, row 36) | Step 1: the SAURIA RTL and the RTL-ref core on 10 one-tile shapes with dilation 1 to 9. Steps 2 to 4: check program `make_pad_gate.py --set dilation` (13 layers) against explicit loops and `ref_program.py`, both ways of padding, also through the system wrapper; decoder unit test (C12); padding generator unit test with dilation 2 to 9; old check programs regenerated byte-identical (section 6.10) | All PASS, 0 mismatches. Core busy 247,370 with the padding staged, 247,378 by the feeder (8 cycles on one tile). Programs that do not write `DILATION` keep their cycles | MEASURED |
| V34 | Row sweep limit of the IFMAP feeder (`KNOWN_LIMITATIONS.md`, O24) and the push-time check `SWEEP` | Boundary programs on the RTL-ref core and on the SAURIA RTL (Verilator): `xlim` 252 to 264, dilation 12 / 14 / 16 at stride 7, stride 5 to 8 with 32 positions per context; decoder unit test (C13); `ref_program.py`; the same programs through the system wrapper; cause test with the lane position counters widened from 8 to 12 bits on the core and on the RTL (section 6.11) | Limit at `xlim` = 256 on both sides; every instruction past it refused with error 11 and none of the reference programs; with 12-bit counters every failing shape right on the core and on the RTL and the passing shapes keep their cycles | MEASURED |
| V35 | Vector-unit top level `GvuTop` on the instruction path (`HAS_GVU_TOP`, `GVU_TOP_CORE_INTEGRATION.md`, sections 1.3 and 5) | `tools/has/tb_gvu_top_vs_model.cpp`: 51 random runs (tile mode, stream mode with gaps and per-beat parameters, residual add with a partial last row) against the model's cycle-level units, and two planted faults; YOLOv8m instructions 86 to 88 and window D with `HAS_OBP_RTL=1`, `HAS_GVU_TOP=1` and `HAS_GVU_TOP=2`; the recorded epilogue beats replayed on `GvuTop`; the ELEM_WISE instructions of YOLOv8m and ViT-B/16 replayed with the residual adds on `GvuTop` (section 6.12) | 51 of 51 runs equal; each planted fault makes 12 runs fail; in the core 108,544 and 596,736 lanes with 0 different from the model, 280,003 and 1,658,651 cycles as with `HAS_OBP_RTL=1`, 271,829 and 1,650,477 with `HAS_GVU_TOP=2`; 33,200 recorded beats replayed with 0 different, depth 9; 12 and 25 residual adds with 0 different; the six runs of the `GvuTop` test benches unchanged. Whole networks not run | MEASURED |
| V36 | Vector-unit top level on the core clock, max pool on it, core stop on its back-pressure (`HAS_GVU_TOP=3`, `HAS_GVU_STALL=1`; section 6.13) | Unit tests of the stream, the parameters and max pool; max pool against the hardware RTL (Verilator); check programs; YOLOv8m windows; whole YOLOv8m (28 windows and one continuous run) and whole ViT-B/16 (one continuous run) at level 3; the core stop with the ready of the unit held low | Whole YOLOv8m PASS 98 / 98, 62,420,669 cycles; whole ViT-B/16 PASS 244 / 244, 41,581,353 cycles; 0 lanes different from the functional model; core busy cycles equal to the default runs; max pool 10 of 10 sets equal to the RTL cycle by cycle; core stop: FIFO never above 4, every packet once, results unchanged; regression 99 / 99 | MEASURED |
| V37 | Register file of the vector unit and the DFC flow (`HAS_GVU_REGS=1`, `GVU_TOP_CORE_INTEGRATION.md`, sections 2.8 and 5; section 6.14) | Register-file and sequence unit tests at three write latencies; the register file against the RTL (200,000 cycles, two simulators); check programs of 6 and 9 instructions through the registers and with the residual add outside them; whole YOLOv8m in seven windows plus the 12 residual adds one by one; the same window with the register file on and off; the end-of-layer flag on a layer whose rows straddle two SRAM-C words; the regression of the vector unit | Every check PASS; 98 of 98 instructions, 83 of 83 layers left by the flags, 0 elements different; +1,819 cycles (0.003 %) over the reference; regression 131 of 131 | MEASURED |
| V38 | New default configuration (section 6.15) | Whole YOLOv8m with the knobs of V36 on one continuous run; regression with the new defaults; continuous runs of both networks with no variable set | YOLOv8m 98 of 98, 62,420,669 cycles (equal to the earlier continuous run to the cycle); with no variable set: YOLOv8m 62,422,935, ViT-B/16 41,582,468, both PASS; regression 131 of 131 | MEASURED |
| V39 | Max pool over a whole layer against the hardware RTL: runs back to back without a reset, the SPPF max pools of YOLOv8m as vector sets (section 6.16) | `tools/has/gvu_oracle/tb_oracle_maxpool_runs.sv` on the RTL with the fixes, `tools/has/tb_gvu_top_maxpool_runs.cpp` on `GvuTop`, traces compared line by line and with the golden outputs; ModelSim and Verilator | 32 of 32 ModelSim cases and 26 of 26 Verilator traces equal to the model; every case with the fix of B12 equal to the golden | MEASURED |
| V29 | System wrapper revision that already contains the DRAM-window registers, with the current v4.6 native model | The wrapper built unchanged against the model root `core_rtl/`: its own unit test; the C replay engine through the wrapper on the transformer check, in the default configuration and with `HAS_SPEC=v47ref`; host steps and output CRCs of whole YOLOv8m and ViT-B/16 on the golden images; negative controls: one corrupted output byte, and one instruction pushed with an unsupported opcode | Unit test passes; transformer check 14 of 14 with 285,205 cycles (250,885 with `HAS_SPEC=v47ref`); 100 and 244 output CRCs match; the corrupted byte is detected; the refused instruction fails its step with error code 4 read from the model's status register, the other 13 instructions pass | MEASURED |
| V21 | Vector-unit additions | AVG_POOL through the instruction path (8 instructions: 2x2 to global windows, int16 wrap, saturation, negative scale); pipeline registers of table 2; load/store unit counting | AVG_POOL 8/8 bit-exact; 16/16 stages fit the specified registers; window D unchanged (1,658,986) | MEASURED |
| V20 | RTL-accurate core through the TLM wrapper | Wrapper with the DRAM-window change, model root `core_rtl/`, 1.25 ns clock: the wrapper's own unit test, and the whole YOLOv8m (98 instructions, 2 host steps) and ViT-B/16 (244 instructions) programs replayed as firmware would | Unit test PASS (0 failed checks); every instruction and host step PASS, 0 mismatches; per-instruction cycles equal to the reference runs (YOLOv8m 98/98, sum 62,681,370; ViT-B/16 244/244, sum 39,922,316) | MEASURED |
| V19 | Existing TLM integration wrapper | Wrapper and its unit test built against this tree: 32x32 GEMM bank (K = 17), native registers and SRAM, one element-wise instruction, error cases | PASS, 0 failed checks, output identical to the previous native revision | MEASURED |

## 3. Core equivalence and tile-shape coverage (V1 to V3)

### 3.1 Cycle-level equivalence (V1)

Forty tiles from nine layers were run on both the ported core and the reference core with the same FIFO
configuration (activation depth 5, weight depth 4). Output bytes and the busy and execute cycle counters are
identical for all 40 tiles. The ported core is therefore used as the cycle reference for all later runs.

### 3.2 Coverage over all tile shapes (V2)

The YOLOv8m tile plan contains 150 distinct tile shapes. Each shape was run in isolation through the core
with seeded data and compared with the C++ reference convolution.

| | Value | Label |
|---|---:|---|
| Shapes passing | 149 / 150 | MEASURED |
| Tiles covered by passing shapes | 6,914 / 6,924 (99.86 %) | MEASURED |
| Failing shape | `det.p3.box_out`, first tile, tile height 24: 1,536 of 24,576 elements wrong | MEASURED |

The failing shape fills one SRAM-C buffer exactly. The planner avoids it by using a tile height of 4 for the
two `box_out` layers; with that plan every tile of the network passes (V4 to V6). The root cause is not
identified (`KNOWN_LIMITATIONS.md`, O1).

A stop criterion of the tile testbench was also corrected during this work: the controller's `o_deadlock`
output is a combinational back-pressure indicator, not an error. A tile now fails only when it exhausts its
cycle budget, or when back-pressure persists for 5,000 cycles without progress. Tiles with 64,788 back-pressure
pulses pass with 0 mismatches.

### 3.3 Specified SRAM capacity (V3)

The core SRAM arrays of the model are larger than the specification (see `KNOWN_LIMITATIONS.md`, O5). To prove
that no result depends on the extra space, the 150 shapes were rerun with per-buffer capacities of
A 79 KB, B 81 KB and C 96 KB and with the capacity checker (`FX1_SRAM_CAP_CHECK`) enabled. The checker counts
every real access beyond the specified capacity and every address that wraps in the model array. Its detection
power was confirmed with a negative control (SRAM-C reduced to 8 KB: 67,608 violations reported).

| SRAM | Highest row used | Specified rows per buffer | Accesses beyond capacity | Label |
|---|---:|---:|---|---|
| A | 2,520 | 2,528 | 0 | MEASURED |
| B | 2,591 | 2,592 | 80 accesses at row 2,592 (tiles that fill SRAM-B exactly) | MEASURED |
| C | 768 | 768 | 8 reads at row 768 (the 3 tiles that fill SRAM-C exactly) | MEASURED |

All 150 jobs produce the same verdict, mismatch count, element count and cycle count as in V2. The accesses at
row = capacity occur only in tiles that fill a buffer exactly, including tiles that pass. The values read there
are not used. This is reported to hardware as question H14 (`HAS_VALIDATION.md`).

## 4. Full-network runs with the v4.5 OBP arithmetic (V4, V5)

**Native OBP rounding.** V4 and V5 were measured when the native OBP shifted without rounding, and their golden
(`tools/fe/fe_ref_int8.py`) follows that arithmetic. The native OBP of the current v4.6 revision rounds half away from
zero (V28). With it, the network testbenches of this section need a golden generated with the same rounding:
`python3 tools/fe/fe_step6b_export_net.py --obp-round half_away --out-dir <net dir>` (the program and the initial DRAM
image are unchanged; only the golden differs). Checked on the whole YOLOv8m program with the functional core stand-in
(`tb_fe_network`, 117 steps, 82,540,800 elements): 0 mismatches against the half-away golden; against the earlier
golden 47,925,339 elements differ, and all of them match again when the rounding term alone is removed from the OBP
(MEASURED). The effect of the rounding change on detection quality has not been measured. The delivery path
(`HasNpuTop`, sections 6.3 to 6.7) uses the HAS epilogue and is not affected.

Both runs are chained: every layer reads the real output of the previous layer from the simulated DRAM. Host
steps (slice, concat, max-pool, upsample) run in C++ with zero simulated cycles.

| Quantity | Image 1 (`000000000009`, coco8) | Image 2 (`000000000443`, coco128) | Label |
|---|---:|---:|---|
| Steps / tensors checked | 117 / 117 | 117 / 117 | MEASURED |
| Output bytes compared | 82,540,800 | 82,540,800 | MEASURED |
| Mismatching bytes | 0 | 0 | MEASURED |
| Tiles executed on the core | 9,811 / 9,811 | 9,811 / 9,811 | MEASURED |
| Core stalls (budget exhausted) | 0 | 0 | MEASURED |
| Core busy cycles | 103,044,189 | 103,044,189 | MEASURED |
| Testbench cycles | 131,585,210 | 131,585,210 | MEASURED |
| Detection boxes (model / golden) | 4 / 4, IoU 1.0000, confidence difference 0 | 9 / 9, IoU 1.0, confidence difference 0 | MEASURED |
| Ground-truth objects found (IoU >= 0.5, same class) | 4 of 8 (4 small oranges, 0.9 to 2.2 % of the image, missed) | 6 of 6 | MEASURED |

The cycle counts of the two images are identical to the cycle. Core timing therefore depends only on the tile
shapes, not on the data. The ground-truth comparison reflects the quality of the int8 network, not the
correctness of the model; the model reproduces the golden exactly in both cases.

## 5. Full-network run with the HAS arithmetic (V6)

The epilogue uses the HAS OBP (`has/gvu_obp.h`): `Out_Scale` int32 multiply, round-shift (half up), int16
saturation, zero point, int8 clamp, then the activation LUT. Residual additions run as ELEM_WISE ADD in two
stages through the scratchpad (`has/gvu_elemwise.h`), and the SPPF max-pools run as ELEM_WISE MAX. The golden is
the independent HAS golden (`fe_ref_has`).

Model options of this run: `round=1` (HALF_UP), `narrow=0` (SAT16), `deq_zp=0` (subtract before), `scale_fmt=0`
(int32), `mp=0` (direct), `scratch=24576`.

| Quantity | Value | Label |
|---|---:|---|
| Steps | 129 = 83 convolutions + 12 ELEM_WISE ADD + 3 ELEM_WISE MAX + 31 host | MEASURED |
| Output bytes compared / mismatching | 88,915,200 / 0 | MEASURED |
| Tiles on the core | 9,811 / 9,811 | MEASURED |
| ELEM_WISE steps passing | 15 / 15 (6,374,400 ADD and 345,600 MAX elements) | MEASURED |
| OBP vectors in / out | 2,014,080 / 2,014,080 | MEASURED |
| Requant operations | 48,326,400 | MEASURED |
| int16 saturations in requant | 0 | MEASURED |
| int8 clamps (OBP + ELEM_WISE) | 1,909 + 59 = 1,968, equal to the golden counter | MEASURED |
| 64-bit overflows, configuration errors, LUT lane mismatches | 0, 0, 0 | MEASURED |
| Detection boxes | 4 / 4 identical to the HAS golden (IoU 1.0000, confidence difference 0) | MEASURED |
| Core busy cycles | 103,044,189 (identical to V4) | MEASURED |

The HAS golden and the v4.5 golden differ in 24.7 % of elements across the network (different rounding),
yet produce the same four boxes with the same classes (IoU 0.972 to 0.998 between the two goldens).

## 6. Instruction-driven top level (V7 to V9)

### 6.1 Instruction stream (V7)

The frontend emits YOLOv8m as a stream of v4.5-protocol register writes (`tools/fe/fe_emit_insts.py`): 98
instructions (83 GEMM_FUSED, 12 ELEM_WISE ADD, 3 ELEM_WISE MAX) and 2 host upsample steps, in 2,306 register
writes. A replay testbench decodes the stream through the compatibility layer and the tile iterator.

| Check | Result | Label |
|---|---|---|
| Tiles decoded identical to the reference plan | 9,811 / 9,811 | MEASURED |
| ELEM_WISE steps decoded identically | 15 / 15 | MEASURED |
| Decoder errors / writes outside the register block | 0 / 0 | MEASURED |
| Tensors in the new DRAM layout equal by name to the golden | 130 / 130 | MEASURED |

### 6.2 `HasNpuTop` sequential (V8)

`HasNpuTop` executes the instruction stream with the data flow controller; the testbench acts only as the CPU
(register writes and status polling). Two windows were chosen to cover the new mechanisms.

| Window | Content | Result | Label |
|---|---|---|---|
| A | Instructions 3 to 8: four 3x3 GEMM layers and two ELEM_WISE ADD | 6/6 PASS, 280 tiles, 7,372,800 elements, 0 mismatches | MEASURED |
| B | Instructions 49 to 54: SPPF (three MAX_POOL), a convolution on an address-aliased concat, host upsample | 6/6 + 1 host PASS, 714 tiles, 0 mismatches | MEASURED |

No error codes, no stalls, no OBP configuration errors, no int16 saturations. The cycle count of one layer
differs from the testbench-driven run by 256 cycles, which is the register-write time: the instruction path does
not change core timing.

The new DMA model (`has/has_dma.h`) configured with the v4.5 parameters matches the v4.5 DMA on 400 random
transfers with 0 differences in completion cycle and 0 differing bytes (MEASURED).

### 6.3 `HasNpuTop` overlapped with the HAS DMA (V9)

In overlapped mode the DMA writes directly into the host half of the core's ping-pong SRAM while the core
computes on the other half; the epilogue and write-back of tile i-1 run in parallel with tile i. The DMA is
set to the HAS bus (128-bit AXI, 16 bytes per beat).

| Window | Sequential, HAS DMA | Overlapped, HAS DMA | Result | Label |
|---|---:|---:|---|---|
| A | 7,788,332 cycles | 5,305,513 cycles | PASS, 0 mismatches | MEASURED |
| B | 6,285,211 cycles | 4,320,235 cycles | PASS, 0 mismatches | MEASURED |

The host-half SRAM access functions were checked separately: writing the host half never changes the NPU half,
and after a swap the core read port returns the written pattern (PASS in three build variants). Timing results
are discussed in `PERFORMANCE_REPORT.md`, section 5.

### 6.4 Whole network through `HasNpuTop` (V12, delivery acceptance)

The whole YOLOv8m runs from the v4.5-style memory-mapped instruction stream only: 98 instructions (83
GEMM_FUSED, 15 ELEM_WISE) and 2 host upsample steps, overlapped schedule, HAS AXI-128 DMA timing. The testbench
acts only as the CPU.

| Quantity | Value | Label |
|---|---:|---|
| Result | PASS: 98 instructions + 2 host steps, 100 tensors | MEASURED |
| Elements compared / mismatching | 58,425,600 / 0 | MEASURED |
| Tiles on the core | 9,811 / 9,811 | MEASURED |
| Core stalls / MMIO errors | 0 / 0 | MEASURED |
| Requant operations | 48,326,400 | MEASURED |
| int8 clamps (OBP + ELEM_WISE) | 1,909 + 59 = 1,968, equal to the golden counter | MEASURED |
| Detection boxes | Every tensor is byte-identical to the HAS golden, so the boxes are those of the golden (4 of 4, as in V6) | DERIVED |

The element count is lower than in V6 because slices and concatenations are address aliases in the
instruction-stream DRAM layout and are not separate tensors. Timing is in `PERFORMANCE_REPORT.md`, section 5.3.

### 6.5 Whole network with the GVU data flow and planner options (V13 to V16)

Both runs use the options of `ARCHITECTURE.md`, section 8: epilogue on the partial-sum path, banked
scratchpad, GVU latencies, 3-D DMA descriptors, layer tile order, and input-channel split with partial sums
accumulated in SRAM-C. V14 adds padded edge tiles (`FLAGS.PAD_TAIL`) and `Y_USED` = 31 for stride-2
convolutions. These change the order of computation and the tile shapes, not the arithmetic, so the golden is
unchanged.

| Quantity | V13 (run A) | V14 (run B) | V15 (run C) | V16 (run D) | Label |
|---|---:|---:|---:|---:|---|
| Instructions passing | 98 / 98 (+ 2 host) | 98 / 98 (+ 2 host) | 98 / 98 (+ 2 host) | 98 / 98 (+ 2 host) | MEASURED |
| Elements compared / mismatching | 58,425,600 / 0 | 58,425,600 / 0 | 58,425,600 / 0 | 58,425,600 / 0 | MEASURED |
| Tiles / core passes | 7,735 / 8,559 | 7,299 / 7,695 | 7,299 / 7,695 | 7,301 / 7,697 | MEASURED |
| Core stalls / MMIO errors | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 | MEASURED |
| Requant operations / int8 clamps | 48,326,400 / 1,909 (equal to V12) | 53,783,808 / 2,129 | 52,017,408 / 2,129 | 52,206,848 / 2,129 | MEASURED |

In V14 to V16 the requant and clamp counters are higher because the padded channels are also computed; they are not
written back, and every written tensor is byte-identical to the golden. Timing: `PERFORMANCE_REPORT.md`,
section 5.4.

### 6.6 Transformer instructions and ViT-B/16 (V17, V18)

FUSED_ATTN and LAYERNORM are executed by the data flow controller from their memory-mapped registers and a
parameter block in DRAM (`SW_INTEGRATION_GUIDE.md`, sections 4.4, 4.5 and 6.2). The arithmetic is the one of the
vector-unit drawings (softmax with `max - x` and a logarithmic reciprocal table, one requant of Q.K^T with the
zero-point corrections, five-stage LayerNorm with the reciprocal square root table), validated at unit level
against the independent Python reference before integration.

| Check | Content | Result | Label |
|---|---|---|---|
| Unit level | Same heads and cases, every intermediate (phase-1 scores, row maximum, sum, reciprocal, attention, output; LayerNorm sums, variance, normalized and final values) | 0 mismatches | MEASURED |
| V17, instruction path | 14 instructions, each checked byte by byte: token-major with the matrix products in software and on the core, channel-major on the core | 0 mismatches in every run | MEASURED |
| V18, whole network | 244 instructions; golden chained block to block (block N consumes the integer output of block N-1); every psum checked to stay within int32 | 26,068,616 elements, 0 mismatches | MEASURED |

The integer network agrees with the floating-point model on the top-1 class of the tested image; the cosine
similarity of the logits is 0.61, limited by the per-tensor LayerNorm quantization (`KNOWN_LIMITATIONS.md`, Q4).
This concerns model accuracy, not the bit-exactness of the hardware model. More images: section 6.7.

### 6.7 More input images (V22, V23)

Timing does not depend on data values, and the arithmetic risks that do depend on the data (int32 partial sums,
int16 saturation and int8 clamps in the requant, integer LayerNorm and softmax) were measured on many images before
choosing the images to run through the model.

Step 1, integer golden against the floating-point model (no simulation):

| Network | Images | Arithmetic | Integer against floating point | Label |
|---|---|---|---|---|
| YOLOv8m | 112 coco128 images, none used for calibration | int16 saturations: 0 on every image; largest partial sum plus bias 0.026 % of the int32 range; int8 clamps per image: median 1,771, maximum 8,010 | 747 boxes (floating point), 715 (integer), 674 matching: recall 0.902, precision 0.943, mean IoU of the matches 0.965; 61 of 112 images identical | MEASURED |
| ViT-B/16 | 5 coco128 images | LayerNorm: only the stage-5 output saturation counter is non-zero (292 to 555 per image); partial sums stay at the bias bound of 2^30 set by the frontend (`KNOWN_LIMITATIONS.md`, Q5) | Top-1 equal on 3 of 5 images; on the other two the integer top-1 is the second class of the floating-point model or a neighbouring class; cosine of the logits 0.36 to 0.67 | MEASURED |

Step 2, whole networks through `HasNpuTop` with the default profile (hardware as drawn), programs regenerated by
the frontend for each image, instruction streams identical to `insts_pe3` and `vit_full`:

| Network | Image (coco128) | Why chosen | Result | Total cycles | Output | Label |
|---|---|---|---|---:|---|---|
| YOLOv8m | `000000000009` | Reference image | PASS, 0 mismatches | 62,681,721 | 4 of 4 boxes identical to the golden | MEASURED |
| YOLOv8m | `000000000502` | Not a calibration image | PASS, 0 mismatches | 62,681,721 | 1 of 1 box identical (bear, 0.935) | MEASURED |
| YOLOv8m | `000000000544` | Most int8 clamps of step 1 (8,010) | PASS, 0 mismatches | 62,681,721 | 11 of 11 boxes identical | MEASURED |
| YOLOv8m | `000000000196` | Most objects of step 1 (34), largest integer to floating-point difference | PASS, 0 mismatches | 62,681,721 | 29 of 29 boxes identical | MEASURED |
| ViT-B/16 | `000000000502` | Not a calibration image | PASS, 0 mismatches | 41,065,258 | Top-1 equal to the golden and to the floating-point model | MEASURED |
| ViT-B/16 | `000000000109` | Integer top-1 differs from the floating-point model | PASS, 0 mismatches | 41,065,258 | Top-1 equal to the golden (the second class of the floating-point model) | MEASURED |

Every `[STEP]` line of the three additional YOLOv8m runs has the same cycle count as the reference run, and the
detection tensors read from the DRAM snapshot differ from the golden in 0 bytes. The model therefore reproduces the
integer golden on every image tried, including the ones selected as hardest; the differences against the
floating-point model are those of the int8 quantization (`KNOWN_LIMITATIONS.md`, Q2 and Q4), not of the model.
The totals were predicted before each run (same instruction stream, hence the same cycles) and met exactly.

### 6.8 Convolution padding by the IFMAP feeder (V31)

With `HAS_FEEDER_PAD=1` SRAM-A holds only the part of a tile's input window inside the image and the IFMAP feeder
replaces every other element by the padding value (`HARDWARE_CONFIG.md`, section 6.5). No hardware description of
this logic exists; "correct" means equal to the flow that stages the padded window, and equal to two independent
references.

| Check | Content | Result | Label |
|---|---|---|---|
| Check program (`tools/has/make_pad_gate.py`) | 13 GEMM_FUSED instructions: the old `PAD` form and per-side padding, padding values -128, -121, -3, 0, 5, 7, 127, kernels 1x1, 3x3, 5x3, 5x5, 7x7, strides 1 and 2, 4 x 2 tiles, a tile width rounded up (`PAD_TAIL`), 64 input channels in two passes. Golden from explicit loops in the generator and from `tools/has/ref_program.py`. Both ways of padding, profiles default, `legacy`, `proposals` and overlapped without 3-D descriptors | 13 / 13 in every run, 0 mismatches. Core busy 234,839 cycles with both ways of padding; total 246,223 staged, 246,491 by the feeder (30 core passes padded by the feeder, 8 cycles each, on the 67-cycle status-polling grid of the testbench) | MEASURED |
| Random layers | `make_pad_gate.py --set random`, three seeds (43, 15 and 15 instructions) | Seeds 2 and 3: 15 / 15. Seed 1: 41 / 43 with both ways of padding; the 2 failing instructions fail identically with the padding staged and come from the short-K error of `KNOWN_LIMITATIONS.md`, O23 (corrected since), not from the padding. These runs were made before that correction | MEASURED |
| Padding generator (`tools/has/tb_has_pad_gen.cpp`) | 4,000 random layers (image size, kernel, stride, padding per side, tile size, `PAD_TAIL`): every tile staged as the data flow controller does, every element the feeder gathers addressed with the generator's registers and compared with the image or the padding value; register limits | 30,751 padded tiles, 22,392,897 elements, 0 failed checks | MEASURED |
| One-line faults | 8 single-line changes in the feeder, its index counter, the padding generator and the core top level, each built and run on the check program | 8 / 8 detected (the generator faults also by its unit test); with the option off every faulty build still passes, as expected for faults on the padding path only | MEASURED |
| Sanitizer and SRAM capacity builds | AddressSanitizer and UndefinedBehaviorSanitizer; SRAM row checker (`-DFX1_SRAM_CAP_CHECK`) | Clean | MEASURED |
| System wrapper (revision 528007d, model root `core_rtl/`) | Check program through the wrapper, both ways of padding; registers `0x4E8` to `0x4F8` of the extension window; 5 reset sequences | 13 / 13 with both; `PERF_EXEC_CYCLES` 246,124 staged and 246,364 by the feeder (+240 = 8 x 30); resets 5 / 5 | MEASURED |
| Option off | YOLOv8m instructions 86 to 88 and the two-instruction window | 280,003 and 1,658,986 cycles, equal to the reference runs | MEASURED |
| Whole YOLOv8m, padding in SRAM-A | 98 instructions, default profile | PASS, 58,425,600 elements, 0 mismatches, 62,666,123 cycles (predicted: equal to the reference run) | MEASURED |
| Whole YOLOv8m, padding by the feeder | Same program with `HAS_FEEDER_PAD=1`. Prediction written before the run: 4,006 core passes padded by the feeder (3,610 tiles in 59 layers), 62,666,123 + 8 x 4,006 = 62,698,171 cycles within +- 6,566 (98 instructions x 67-cycle polling grid), core busy equal to the run above | PASS, 0 mismatches, 4,006 passes padded by the feeder, 62,698,248 cycles (+77 against the prediction), core busy 55,653,442 in both runs, PE utilization 69.26 % | MEASURED |
| Whole ViT-B/16, option off | 244 instructions | PASS, 41,680,289 cycles. With the option on no tile qualifies (padded tiles of ViT-B/16 are tail tiles of 1x1 layers); not run | MEASURED / DERIVED |

The feeder outputs (values and the flags `done`, `fifo_full`, `stall` of every cycle, recorded with
`-DFX1_A3_FEED_SIG`) are identical with both ways of padding on YOLOv8m instructions 86 to 88 and on the
two-instruction window. On the check program the flags `fifo_full` and `stall` change cycle on some core passes,
while the results and the core busy cycles stay equal; the cause is not established.

### 6.9 Short contexts (V32)

A GEMM_FUSED tile whose K (kernel positions x input channels of the core pass) is very small could get no weights in
its last context: the "done" pulse of the weight counter reached the feeder controller one cycle late, so the
repetition counter of the weights advanced one repetition early when the weight FIFO filled right after the wrap
that ends the last but one repetition. Separately, K = 1 (configuration value `INCNTLIM` = 0) was run as two MACs per
context. The SAURIA RTL computes all of these shapes right. Both points are corrected and on by default
(`HARDWARE_CONFIG.md`, section 4).

| Check | Result | Label |
|---|---|---|
| SAURIA RTL, Verilator, 12 shapes (K 1 to 10, 2 to 24 contexts, 16 and 32 output channels), byte compare of the whole output region; a changed golden byte is detected | 12 / 12 right | MEASURED |
| `make_pad_gate.py --set smallk`, 19 layers, golden from explicit loops and `ref_program.py` | Without the correction 12 of 19 wrong; with it 19 / 19 | MEASURED |
| Random padding set, seed 1, 43 layers | 43 / 43 staged (5,448,992 cycles) and by the feeder (5,452,836); core busy 5,319,656 in both; before the correction 41 / 43 | MEASURED |
| Reference programs | Window D 1,658,986 cycles, core busy 1,500,768; instructions 86 to 88 280,003; transformer check 14 / 14 (285,205); padding program unchanged; AVG_POOL 8 / 8 | MEASURED |
| Whole ViT-B/16, default configuration | PASS, 244 / 244, 26,068,616 elements, 0 mismatches; 41,680,289 cycles, core busy 24,852,624, PE utilization 69.61 %: equal to the prediction and to the run before the correction | MEASURED |
| Whole YOLOv8m, default configuration | PASS, 98 / 98; 62,666,123 cycles, core busy 55,653,442: equal to the prediction and to the run before the correction, with the same cycle mark at every instruction | MEASURED |

### 6.10 Dilation (V33)

The RTL-ref core has the dilation of the SAURIA design (a 64-position dilation pattern in the IFMAP feeder); the
instruction decoder, the tile walk and the padding generator now use the effective kernel `(K - 1) x DILATION + 1`.

| Check | Content | Result | Label |
|---|---|---|---|
| Core against the SAURIA RTL | 10 one-tile shapes: dilation 1, 2, 3, 4, 9; strides 1 and 2; kernels 3x3, 5x5, 1x3, 8x1 (dilation 9: effective width 64); 16 and 32 output channels; one and two contexts per output row. RTL: Verilator, tester and byte compare of the output; core: `tools/fe/sysc/tb_fe_core_tile` against explicit loops | RTL 10 / 10, core 10 / 10, 0 mismatches. Each side used its own random data and golden. With dilation 1 and 2 on the same shape both sides keep their cycle count | MEASURED |
| Check program `make_pad_gate.py --set dilation` | 13 layers: dilation 2, 3, 4, 9; with and without padding, the old `PAD` form, stride 2, kernels 3x3, 5x5, 1x3, 3x1, 1x1, 1x8 at dilation 9, 4 x 2 tiles, a tile width rounded up, 64 input channels in two passes, two contexts per output row | `ref_program.py` 13 / 13 match; model 13 / 13 staged and 13 / 13 by the feeder (28 core passes padded by the feeder); core busy 247,370 staged, 247,378 by the feeder | MEASURED |
| Through the system wrapper (528007d, model root `core_rtl/`) | Same program, both ways of padding | 13 / 13 both; `PERF_EXEC_CYCLES` 260,943 and 261,175 (+232 = 8 x 28 + 8) | MEASURED |
| Decoder unit test, C12 | Effective kernel and tile windows at dilation 9; one-shot; 0 = 1; a 1x1 kernel keeps 1; `(KW - 1) x DILATION` = 63 accepted, 64 refused; negative refused | PASS | MEASURED |
| Padding generator unit test | 2,392 random layers with dilation 2 to 9: every element the feeder reads, through `TIL_YSTEP` and `YSTEP` | 23,218 padded tiles, 15,135,468 elements, 0 failed checks; the dilation-1 part unchanged (30,751 tiles) | MEASURED |
| Random dilation layers | `make_pad_gate.py --set dilrandom`, seeds 1 to 3, 30 layers each (dilation 2 to 9, kernels 1 to 5, padding per side, strides 1 and 2, random tiles, `PAD_TAIL`, `Y_USED`, `TILE_CIN`) | `ref_program.py` 90 / 90 match; model 90 / 90 staged and 90 / 90 by the feeder (424, 856 and 407 core passes padded by the feeder). Core busy staged / by the feeder: 2,164,536 / 2,164,533, 2,403,163 / 2,403,163, 1,681,344 / 1,681,344 | MEASURED |
| Report file through the wrapper | `SAURIA_CORE_RTL_REPORT` on the dilation and padding programs and the transformer check | The 32 earlier columns equal to the run before the change; the padding per side, padding value and dilation columns equal to the registers of every instruction (a 1x1 kernel reports dilation 1); transformer check 14 lines, 285,205 cycles, 23.18 % | MEASURED |
| Unchanged behaviour | `make_pad_gate.py` sets fixed, random (12 and 40), smallk, probe, dilation regenerated with the old and the new generator; regression | Byte-identical; regression 45 / 45; instructions 86 to 88 280,003 / 280,204 as before | MEASURED |

The 8 cycles come from one tile, the 1x8 layer with dilation 9: its window is 103 columns, of which the real part is
40, so the rows of SRAM-A fall differently on the 32-element words with the two ways of padding (8,950 against 8,958
core busy cycles). Every other tile has the same core busy cycles. A cycle trace of the feeder outputs on that tile
(`-DFX1_A3_FEED_SIG`) shows where the 8 cycles are: the 32 lane values are identical in every cycle with both ways of
padding up to the end of the tile, and the tile-done pulse of the feeder comes in the same cycle; the context-done
pulses move by 1 or 2 cycles, the feeder runs ahead further with the padding by the feeder (FIFO full in 8,215 cycles
against 8,092, stalled in 1,039 against 555), and the drain after the last context takes 400 cycles against 392. On
the random dilation layers the difference is within 3 cycles in either direction.

### 6.11 Row sweep limit (V34)

One row sweep of the IFMAP feeder covers `xlim = 1 + (Y_used - 1) x stride + W + 1 - (W mod 2) + 32` positions, with
`W = (KW - 1) x DILATION + 1`. The position counters of the feeder lanes are 8 bits wide (`PARAMS_W = 8` in the
SAURIA design and in the RTL-ref port), so a sweep longer than 256 positions wraps.

| Check | Content | Result | Label |
|---|---|---|---|
| Boundary, core and SAURIA RTL | 1xN kernels at strides 6 and 7 chosen so that `xlim` is 252, 253, 254, 255, 256, 257, 258, 260 and 264; RTL: Verilator, tester and byte compare of the output | Up to 256 right on both sides; 257 and above wrong on both sides without any error (257: 28 of 256 elements) | MEASURED |
| Large dilation and stride | 3x3 at stride 7 with dilation 12, 14, 16 (with and without padding); 3x3 at strides 5 to 8 with 32 positions per context | Every shape past the limit wrong on both sides; stride 8 hangs the core and the RTL; shapes within the limit right | MEASURED |
| Push-time check | Decoder unit test C13: `xlim` 256 queued, 257 refused with `SWEEP`, stride 8 with 32 positions refused, the same layer with 16 positions per context queued; boundary programs: 4, 5 and 2 instructions refused, the others right; `ref_program.py` refuses the same ones; YOLOv8m (100 instructions) and ViT-B/16 (75) refused none; through the system wrapper the refused pushes are reported with error 11 | As predicted; regression 48 / 48 | MEASURED |
| Cause, core | Lane position counters and the padding mask counter widened to 12 bits, check lifted: the boundary, dilation and stride programs | 23 / 23 right with both ways of padding, no hang; window D unchanged (1,658,986 cycles) | MEASURED |
| Cause, SAURIA RTL | Rebuilt with Verilator 5.052 twice: unchanged (control) and with `PARAMS_W + 4` on the lanes of `ifmap_feeder.sv` only | Control as the earlier build: 256 right, 257 wrong. Widened: 256, 257, 258, 260, 264, dilation 12 / 14 / 16 and strides 7 and 8 all right, no timeout; on 256 the tester statistics are equal in both builds | MEASURED |

The 8-bit lane counters are therefore the cause, in the RTL as in the model. The model keeps the 8-bit counters of
the design and refuses instructions past the limit; the widening was a test only.

### 6.12 Vector-unit top level on the instruction path (V35)

With `HAS_GVU_TOP=1` the GEMM epilogue and the residual add of the instruction path run on the vector-unit top level
`GvuTop` (`has/rtl_ref_gvu_top.h`) through the socket `has/gvu_top_bridge.h`; each partial-sum beat goes through the
nine-stage stream mode of GEMM_FUSED to completion inside the write of the partial-sum manager. `HAS_GVU_TOP=2` also
charges the activation table of a layer as `GvuTop`'s own path (DMA of 256 bytes plus 5 cycles) instead of 4,096
host-port cycles. Max pool now runs on `GvuTop` as well (V36).

| Check | Content | Result | Label |
|---|---|---|---|
| Unit level | `tools/has/tb_gvu_top_vs_model.cpp 12 1`: 24 tile-mode runs, 12 stream runs of 400 beats in bursts of 1 to 64 with gaps up to 100 cycles, per-beat scale, shift and lane mask, 12 residual adds of 1 to 4,096 elements, 3 residual adds between stream runs on the shared unit | 51 of 51 equal to `GvuGemmEpilogue` / `GvuResAddPipe`; every beat out after 9 cycles with its address and mask; nothing out in a gap; masked lanes untouched | MEASURED |
| Sensitivity | The same test with the last-row lane mask ignored, and with one multiplier register removed | 12 runs fail in each case | MEASURED |
| Instructions 86 to 88 of YOLOv8m (two layers with an activation) | Knobs off; `HAS_OBP_RTL=1`; `HAS_GVU_TOP=1`; `HAS_GVU_TOP=2` | PASS 3 of 3 in all four; 280,003 cycles in the first three, 271,829 with `=2` (2 tables x (4,096 - 9)); 3,392 beats, 108,544 lanes, 0 different | MEASURED |
| Window D (two convolutions) | `HAS_OBP_RTL=1`; `HAS_GVU_TOP=1`; `HAS_GVU_TOP=2` | PASS; 1,658,651, 1,658,651 and 1,650,477 cycles; 29,808 beats, 596,736 lanes, 0 different | MEASURED |
| Replay outside the core | The beats recorded with `HAS_OBP_RTL=1 HAS_OBP_DUMP=<file>` on both programs, run on `GvuTop` back to back and one at a time (`tools/has/tb_gvu_top_replay.cpp`) | 3,392 and 29,808 beats: 0 lanes different, latency 9 on every beat, 0 address or mask errors, 0 beats out of nothing | MEASURED |
| Residual adds | Every ELEM_WISE instruction of YOLOv8m and ViT-B/16 replayed from the program tensors with the residual adds on `GvuTop` | 12 adds (6,374,400 elements) and 25 adds (3,782,400 elements): 0 different from the reference image; `GvuTop` takes 16 cycles less than `GvuResAddPipe` per run of up to 128 rows | MEASURED |
| Test benches of `GvuTop` | The six runs of its own test benches and LayerNorm, SystemC and plain C++ | Unchanged (432 / 223, 144 / 77, 274 / 140 cycles; LayerNorm 10,480 ns) | MEASURED |

Whole networks and the clocked connection: section 6.13 (V36).

### 6.13 Clocked connection, max pool and core stop on the vector unit (V36)

With `HAS_GVU_TOP=3` the vector-unit top level `GvuTop` runs on the core clock beside the RTL-ref core
(`has/gvu_top_stream.h`, `GVU_TOP_CORE_INTEGRATION.md`, sections 2.1 and 2.5): a stream tap of the partial-sum manager keeps the FIFO
of 4 packets of `psm_top.sv` and hands each GEMM_FUSED epilogue beat to `GvuTop` with `valid` / `ready`; `GvuTop` latches it
one edge later and its int8 result is written back into SRAM-C after the depth of the stream pipeline. Residual add and
max pool run on the same unit. With `HAS_GVU_STALL=1` the core stops on `o_stream_stall` as the RTL does. Every number is
MEASURED with the default build of `tools/has/tb_has_npu_top` (the whole-network runs with the metrics build).

| Check | Content | Result | Label |
|---|---|---|---|
| Unit level | `tools/has/tb_gvu_top_stream.cpp` (both process orders, held ready), `tb_gvu_top_params.cpp` (depth 9 and 11, saturate and wrap against `has::requant`), `tb_gvu_top_maxpool.cpp` | Every packet latched one edge after the push and out after the depth (9; 11 with `-DGVU_GEMM_STREAM_DEPTH=11`), same trace in both orders; 25 of 25 parameter runs; 17 of 17 max pool sets, 8 of 8 socket cases | MEASURED |
| Max pool against the hardware RTL | `tools/has/gvu_oracle/run_verilator_maxpool.sh` on the nine golden sets of the hardware vectors and a two-run case | The port and `gvu_top.sv` write the same `vbank#2` rows in the same cycles: 0 lines different on 10 of 10. A second run without a reset is wrong on the RTL as well (4,221 of 12,960 values): the unit is reset before each run | MEASURED |
| Check programs | `make_ref_gate.py` (6 instructions), `make_pad_gate.py` (13 instructions), knobs off / level 1 / level 3 | Level 3: PASS, every epilogue beat equal to level 1 byte for byte, 0 lanes different; DRAM image equal to the knobs-off run; 39,361 and 245,955 cycles (level 1: 47,602 and 246,223) | MEASURED |
| YOLOv8m instructions 86 to 88 and window D | Level 3 against level 1 | PASS; 271,695 and 1,647,060 cycles; beats equal to level 1 byte for byte; FIFO depth 1; 0 extra cycles behind any core pass | MEASURED |
| Whole YOLOv8m, level 3, in 28 windows of instructions | Each window from `dram_golden.bin` | 98 of 98 instructions PASS, 0 bad elements, 1,996,784 packets, 0 lanes different | MEASURED |
| **Whole YOLOv8m, level 3, one continuous run** | `HAS_GVU_TOP=3 tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3` | **PASS 98 / 98**, 58,425,600 elements, 0 bad; **62,420,669 cycles**; core busy 55,653,442 (equal to the default run); 1,996,784 packets, 52,017,408 lanes, 0 different; FIFO depth 1, 0 packets taken with the FIFO full; 12 residual adds (6,374,400 lanes) and 3 max pools (345,600 lanes) on `GvuTop`, 0 different | MEASURED |
| **Whole ViT-B/16, level 3, one continuous run** | `HAS_GVU_TOP=3 tools/has/tb_has_npu_top $FE_WORK/has/vit_full` | **PASS 244 / 244**, 26,068,616 elements, 0 bad; **41,581,353 cycles**; core busy 24,852,624 (equal to the default run); 50 GEMM_FUSED layers, 593,152 packets, 18,980,864 lanes, 0 different; 25 residual adds (3,782,400 lanes) on `GvuTop`, 0 different. Softmax, LayerNorm and FUSED_ATTN stay on the functional model | MEASURED |
| Core stop on the stream back-pressure | `HAS_GVU_STALL=1` on the check program, with the ready of `GvuTop` held low for 300 edges (`HAS_GVU_STREAM_HOLD`, test only) | FIFO never above 4, 0 packets taken with the FIFO full; the core held 277 cycles (core busy 32,841 + 277); 2,672 packets = 2,672 writes of the last input-channel pass; every beat equal to level 1, DRAM image equal to the knobs-off run; 39,629 cycles. Without the stop, the same hold gives 28 packets taken with the FIFO full; the stop computed without applying it (`HAS_GVU_STALL=2`) falls on the same 28 edges | MEASURED |
| Core stop, no hold | `HAS_GVU_STALL=1` on the check program, instructions 86 to 88 and window D | 0 stall cycles; 39,361, 271,695 and 1,647,060 cycles, as without the option | MEASURED |
| Zero point of the hardware register | `tb_gvu_top_params zp`; check program with an output zero point of 200 | int8 zero points taken, 0 lanes different; outside int8 every beat counted as not expressible and the result kept from the functional model; the check program passes 6 / 6 in the same cycles | MEASURED |
| Residual add and max pool on the clock | `tb_gvu_top_stream elem` (12 cases); check program with `HAS_GVU_ELEM_CLK=1`; YOLOv8m windows `--first 50`, `51`, `52` (SPPF) and `5`, `44` (residual add) with the option off and on | values, unit cycles and runs equal to the call style; window cycles 187,664 / 187,684 / 187,732 and 239,529 / 24,504 equal with the option on and off | MEASURED |
| Whole YOLOv8m, all options of this release (without the register file) | `HAS_GVU_TOP=3 HAS_GVU_STALL=1 HAS_GVU_ELEM_CLK=1`, one continuous run | **PASS 98 / 98**, 58,425,600 elements, 0 bad, 0 framing errors, 0 core deadlocks; **62,420,669 cycles**, inside the band [62,389,459, 62,451,879] written before the run and equal to the continuous run of level 3 to the cycle; core busy 55,653,442; 1,996,784 packets, 0 different; FIFO depth 1, `STALL NEEDED` 0, core stall cycles 0; 1,142 residual add chunks (6,374,400 lanes, 412,056 `GvuTop` cycles) and 63 max pool runs (345,600 lanes, 554,472 cycles), 0 different; 1,205 jobs on the clock, 0 failed, 0 extra ticks, 0 late starts | MEASURED |
| Regression | `bash tools/has/regress_gvu.sh` | 105 passed, 0 failed at the time of this check (with the load/store-unit self-checks and the RTL trace digest of the latest RTL drop); 131 items in V37; the older paths give the same results and cycles as before | MEASURED |

Not run: the whole ViT-B/16 with `HAS_GVU_STALL=1` (0 stalls are expected, since the FIFO never held more than one packet in
the level-3 runs; the whole YOLOv8m with it is the continuous run above, 0 stall cycles); Softmax, LayerNorm and FUSED_ATTN on
`GvuTop` (`KNOWN_LIMITATIONS.md`, O29).

### 6.14 Register file of the vector unit and the DFC flow (V37)

The data flow controller configures and starts `GvuTop` only by writing the register file of `gvu_core` (`HAS_GVU_REGS=1`;
register map and sequences in `GVU_DFC_REFERENCE_FLOW.md`, the walk-through in `GVU_TOP_CORE_INTEGRATION.md`, section 2.8). The
end of a GEMM_FUSED layer is the packet that holds the last element of the tile. The register write latency is a parameter,
`HAS_GVU_REG_LAT`, default 4 clock edges per write (ESTIMATED: the real latency is the hardware team's). Residual add through the
registers 9 to 15 is a proposal of the model team (`KNOWN_LIMITATIONS.md`, O28).

| Check | Content | Result | Label |
|---|---|---|---|
| Register file against the RTL | `tools/has/tb_rtl_ref_gvu_config_regs.cpp` and the oracle in `tools/has/gvu_oracle/` (ModelSim): 200,000 random cycles | The RTL as delivered with the model without patches, and the RTL with the patches B13 and B17 with the model with patches: 0 mismatches in both; the crossed pairs mismatch in 199,492 cycles. The RTL as delivered mismatches the model without patches in 195,759 of 200,000 cycles in Verilator and in none in ModelSim (the simulators read a part-select outside the register differently) | MEASURED |
| Frozen blocks against the RTL | `bash tools/has/gvu_oracle/run_all.sh` | 22 lines, 0 mismatches; seven frozen headers unchanged (hash list) | MEASURED |
| Register sequences and write times | `tools/has/tb_gvu_regs_dfc.cpp 2`, `4` and `7` edges per write | PASS: the sequences decoded as intended; write k lands at edge `k L + 2`, a sequence of n writes is over at edge `n L`; seven GEMM_FUSED layers by registers equal the function-call style beat by beat (one with a table, one without, a table after a table, two calls in one layer, a beat that carries the flags, a layer whose last row straddles two SRAM-C words); every layer leaves by itself on the flagged packet, 0 fallbacks; max pool by registers equal to the function-call style in values, cycles and runs and in time `1 + runs (3 L + 2) + cycles`; 14 residual adds of 2,304 to 12,000 elements in 1 to 3 runs with `vbank#1` buffer 0 and 1 equal in values and cycles, in time `1 + sum(n_w L + 2) + cycles`, and with the configuration read back from the registers equal to the call style's in all 24 runs | MEASURED |
| End of layer when a row straddles two words | A layer of 11 packets with 3 `oc_last` and 2 contexts; instruction 66 of YOLOv8m (3x3 stride 2, 192 to 192 channels, 240 tiles, two contexts of 31 positions) with `--first 66 --count 1` | The layer leaves exactly on the last packet. Instruction 66 with the rule that counted `oc_last` packets: FAIL, 288 elements different, 1 core deadlock (every call had three `oc_last` packets for two contexts); with the flag on the packet that holds the last element of the tile: PASS, 0 different, 1,003,464 cycles (equal to the run without the register file). Range 66 to 82: 17 of 17 PASS | MEASURED |
| Check programs, 6 and 9 instructions | `make_ref_gate.py` and `make_ref_gate_mp.py` (the same plus three max pools), knobs off / level 3 / the register file with the residual add outside / the full flow | All instructions PASS in every run, 0 lanes different, every epilogue beat equal to level 3, DRAM image equal to the knobs-off run, `STALL NEEDED` 0, 4 of 4 layers left by the flags, the flag on exactly one packet per layer. Cycles on the standard bench (6 instructions / 9): knobs off 47,602 / 62,965; level 3 39,361 / 68,124; register file with the add outside 39,428 / 68,258 (14 / 30 register writes); full flow 39,562 / 68,392 (44 / 60 writes). With a bench that polls every cycle the register cost is +40 and +116 cycles at 4 edges per write | MEASURED |
| Whole YOLOv8m through the registers | Seven windows (0-11, 12-28, 29-47, 48-65, 66-73, 74-82, 83-97), `HAS_GVU_TOP=3 HAS_GVU_STALL=1 HAS_GVU_ELEM_CLK=1 HAS_GVU_REGS=1`, 4 edges per write | **98 of 98 instructions PASS**, 0 elements different, 0 core deadlocks; **83 of 83** GEMM_FUSED layers left by the flags (0 fallbacks, 0 timeouts); 63 max pool runs by registers, 554,472 `GvuTop` cycles; 639 register writes in the windows; epilogue 0 lanes different; `STALL NEEDED` 0 | MEASURED |
| Residual adds through the registers | One window per residual-add instruction (12), with the register file and with `HAS_GVU_REGS_ADD=0` | 24 of 24 PASS, 0 different; cycles equal between the two on every instruction; 1,142 chunks, 2,276 runs, 14,822 register writes; the configuration read back differs from the call style in 0 runs | MEASURED |
| Cost of the registers on the whole network | Sum of 91 instructions (the first of each window dropped) against the continuous run of the same program with the stream and the clocked add and max pool (62,420,669 cycles) | **+1,819 cycles (0.003 %)**, inside the range +1,443 to +2,771 written down before the run; max pool +268 / +335 / +268 per SPPF instruction (range +248 to +382), residual add +0. Register writes of the whole network: 15,461 (387 GEMM_FUSED, 252 max pool, 14,822 residual add), 61,844 edges of the DFC at the write port at 4 edges per write (0.10 % of the run; the rest overlaps the DMA). Window sums: 1,032, 90, 143, 134, 93, 10 and 317 cycles; the window 12 to 28 is 68 cycles below the lower end of its range, about one polling period of the bench | MEASURED (derived sums) |
| Register file on and off, same bench | Window 12 to 28 (17 instructions) with `HAS_GVU_REGS` unset and set | 17 of 17 PASS in both; the difference per instruction is -67, 0 or +67 cycles, sum +201, residual add 0. The test bench asks for retired instructions every 67 cycles (`tools/has/tb_has_npu_top.cpp`), so a per-instruction difference below 134 cycles is the bench's step, not the registers. The per-instruction differences against the reference (-202 to +234; 61 of 91 within 67) come from the window bench, not from the register file: the continuous run of the same configuration without registers equals the reference to the cycle on the 27 instructions compared | MEASURED |
| Regression | `bash tools/has/regress_gvu.sh` with the 26 added items (unit tests of the register file and of the address generator for slice and concat, check programs of 6 and 9 instructions through the registers and with the add outside) | 131 passed, 0 failed (one item had a wrong condition on its first run, 130 of 131, and was corrected) | MEASURED |
| Patches checked on the RTL (hardware feedback package) | F1′ version 3: 11 valid cases, two ways of driving, Verilator; F2b version 2: 7 cases, two ways; residual add path (R3): 50 cases in ModelSim | F1′ right in 22 of 22 (one packet carries the flag and it is the last; the first version, which counted packets, wrong in the 4 cases with straddling rows); F2b 14 of 14 runs read the right scale and shift for every packet; R3 0 of 3,082 rows different from the model including the int32 sum, the RTL 13 cycles longer per tile | MEASURED |

Not run: the register file with the RTL of the whole `gvu_core` (it needs the AXI library, which is not available); output width
16 bits of the residual add; Softmax, LayerNorm and FUSED_ATTN through the register file (`KNOWN_LIMITATIONS.md`, O29).

### 6.15 New default configuration (V38)

| Check | Content | Result | Label |
|---|---|---|---|
| Whole YOLOv8m with the stream, the stop and the clocked add and max pool, no register file | `HAS_GVU_TOP=3 HAS_GVU_STALL=1 HAS_GVU_ELEM_CLK=1`, one continuous run | PASS 98 / 98, 62,420,669 cycles, equal to the earlier continuous run of level 3 to the cycle (see V36) | MEASURED |
| Regression | `bash tools/has/regress_gvu.sh` on the tree with the address generator hook for slice and concat, before the change of defaults: 131 of 131. With the new defaults (the script sets the five knobs to 0 for the items written for the earlier configuration): 131 of 131 | 131 of 131 before the change, 131 of 131 after it | MEASURED |
| Whole YOLOv8m, no variable set | `tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3`, one continuous run | PASS 98 of 98, 62,422,935 cycles (+2,266), 0 elements different, 83 layers left by the flags, 0 fallbacks, 0 core stops needed, 20,036 register writes, residual-add configuration read back different in 0 of 2,276 runs. Prediction written before the run: 98 of 98, 83 layers left by the flags, 62,420,669 cycles plus 1,443 to 2,771, hard bounds 62,415,546 to 62,430,606 | MEASURED (prediction ESTIMATED) |
| Whole ViT-B/16, no variable set | `tools/has/tb_has_npu_top $FE_WORK/has/vit_full`, one continuous run | PASS 244 of 244, 41,582,468 cycles (+1,115, inside the hard bounds; 15 cycles above the narrow range, which left out the address-generator writes of the 700 residual adds and is below one 67-cycle polling step), 0 elements different, 50 layers left by the flags, 0 fallbacks, 11,952 register writes. Prediction written before the run: 244 of 244, 41,581,353 cycles plus 50 layers x 6 to 22 cycles, hard bounds 41,565,305 to 41,599,401 | MEASURED (prediction ESTIMATED) |

### 6.16 Max pool over a whole layer against the hardware RTL (V39)

The SPPF block of YOLOv8m has three chained 5x5 stride-1 max pools with padding 2 over 288 channels of 20 x 20. The data flow
controller runs each of them on `GvuTop` in 21 runs of up to 14 channels, one after the other. The hardware vector sets
(`target/test/maxpool`) have one run and no padding, so the layer was written out in their format:
`tools/has/dump_maxpool.cpp` takes the three instructions from the reference memory image of the program and
`tools/has/gvu_oracle/gen_sppf_sets.py` writes four sets (the three layers with real activations, and one random set with
windows of negative values only), the input padded with -128 as the controller places it in `vbank#0` (the RTL has no
padding). The script checks that each reference equals a plain max pool and that each layer takes the previous output.

| Check | Content | Result | Label |
|---|---|---|---|
| RTL with the 13 fixes of the RTL feedback package, and with the fixes plus the residual-add proposal and the address generator (identity destination program) | `tb_oracle_maxpool_runs.sv`: all runs of a set back to back, no reset in between; the model trace from `tb_gvu_top_maxpool_runs.cpp` with a reset before each run (what the controller does) | every trace equal line by line: SPPF sets 184,362 lines, 21 runs, 115,200 values, 0 wrong, 0 not written; the 9 hardware sets equal and with the cycles of the RTL as delivered; two runs of 40 channels back to back equal | MEASURED |
| 33 channels per run (the largest that fits the Scratchpad) | the first SPPF layer in 9 runs | equal, 0 wrong | MEASURED |
| Address generator on and off | the same layer with and without the identity destination program | identical traces | MEASURED |
| RTL as delivered (negative control) | the first SPPF layer, 21 runs back to back | runs 2 to 21 wrong: 386 values wrong, 109,200 of 115,200 not written; the RTL trace equals the model run without a reset | MEASURED |
| Two simulators | ModelSim ASE 18.1 (32 cases) and Verilator 5.052 (13 cases on each RTL variant) | the Verilator traces have the sha256 of the model traces (26 of 26) | MEASURED |
| Cycles of one SPPF layer | sum over the 21 runs | 184,866 cycles in the bench = 184,824 cycles of the unit counted on the whole network plus 2 cycles per run of the bench; the register writes of the controller (3 writes and one clear per run) are not included | MEASURED |

With the fix of B12 the RTL needs no reset between runs: the runs back to back give the same trace as the model's reset
before each run. The RTL patches, the sets and the scripts went to the hardware team as a separate test package.

## 7. HAS blocks, unit level (V10)

All vector files were generated by independent Python implementations written from the interface
specification. Each requant/dequant file has 120,000 lines, including boundary cases (inputs at +-2^31, scale at
its limits, shifts 0, 1, 31, 62, 63, exact half-way cases).

| Block | Vectors | Mismatches | Label |
|---|---|---:|---|
| Requant, 8 option combinations | 8 files x 120,000 | 0 | MEASURED |
| Dequant, 8 option combinations | 8 files x 120,000 | 0 | MEASURED |
| Direct LUT inside the HAS OBP | 20,000 vectors | 0 | MEASURED |
| Indirect LUT, reciprocal | 200,000 | 0 | MEASURED |
| Indirect LUT, reciprocal square root | 200,000 | 0 | MEASURED |
| ELEM_WISE ADD (compatibility mode) | 1.23 M elements against `sat8(a + b)` | 0 | MEASURED |
| ELEM_WISE MAX | SPPF shape + 39 random shapes; direct and separable traversal identical | 0 | MEASURED |
| ELEM_WISE AVG | 2x2 stride 2 against an integer reference; int16 accumulator overflow detected where expected | 0 | MEASURED |

Validation status per pipeline (four layers of section 1):

| Pipeline | Independent reference | SystemC | Unit bit-exact | Network integration |
|---|---|---|---|---|
| QUANTIZE requant | yes | yes | yes | yes (V6) |
| QUANTIZE dequant | yes | yes | yes | yes (V6, ELEM_WISE ADD) |
| Direct LUT (activation) | yes | yes | yes | yes (V6) |
| GEMM_FUSED epilogue | via golden | yes | yes | yes (V6) |
| ELEM_WISE ADD | via golden | yes | yes | yes (V6) |
| ELEM_WISE MAX | yes | yes | yes | yes (V6) |
| ELEM_WISE AVG | yes (drawing formula) | yes | yes | instruction path (V21); not used by the two networks |
| Indirect LUT (reciprocal, rsqrt) | yes | yes | yes | yes (V17, V18, V20) |
| SOFTMAX, LAYERNORM, FUSED_ATTN | yes (`tools/fe/fe_ref_gvu.py`) | yes | yes | yes (V17, V18, V20) |

## 8. Native regression (V11, V19)

The RTL-ref work must not change the behaviour of the native model. The native targets were rebuilt from the
current sources with the exact Makefile flags into a separate directory (`tools/has/regress_native.sh`) and
compared with the previous binaries.

| Target | Result | Label |
|---|---|---|
| `tb_obp` | 17 / 17 | MEASURED |
| `test_vit` | 4 / 4 | MEASURED |
| `test_yolo` | 261 / 261 | MEASURED |
| `test_onnx_model` | 497 / 497 | MEASURED |
| `tb_unified_smoke` | all checks pass | MEASURED |

The network results are identical to the previous binaries. `tb_obp` covers negative bias values written and read
back, the scale and shift tables read back through the host bus, rounding half away from zero on negative ties, the
shift guard and the output zero point (scalar and per channel);
`tb_unified_smoke` checks the same read-back through the native top level for both lanes. The Makefile does not track header files, so a plain
`make <target>` can reuse a stale binary; always use the script or `make -B` for regression.

What these targets prove is limited: they check that the old compiler and the native emulator agree with each
other, not the semantics of real networks (`KNOWN_LIMITATIONS.md`, N2).

V19: an existing TLM integration wrapper of the native top level, with its own unit test, was built against this
tree without any change to the wrapper. The test runs the wrapper's 32x32 GEMM bank with K = 17 against a
reference product, writes and reads native registers and SRAM, runs one element-wise instruction and checks the
error paths: 0 failed checks, and the output is identical line by line to the same test on the previous native
revision (`SW_INTEGRATION_GUIDE.md`, section 9).

V20: with the model root `core_rtl/` and the DRAM-window change of the wrapper (`SW_INTEGRATION_GUIDE.md`,
section 10), the wrapper's unit test passes unchanged (instructions without extension registers still run on the
functional decoder), and `tools/has/wrapper/test_npu_tlm_core_rtl.cpp` replays whole programs through the wrapper
with a 1.25 ns clock, checking every output tensor against the golden image and reading `PERF_EXEC_CYCLES` after
every instruction:

| Program | Steps | Outputs | Per-instruction cycles vs reference run | Sum of instruction cycles |
|---|---|---|---|---|
| YOLOv8m `insts_pe3` (run C) | 98 instructions (GEMM_FUSED, ELEM_WISE ADD and MAX_POOL), 2 host `upsample2x` | all PASS, 0 mismatches | 98 / 98 equal | 62,681,370 (run C: 62,681,370) |
| ViT-B/16 `vit_full`, `SAURIA_CORE_RTL_C_BCAST=1` | 244 instructions (GEMM_FUSED, ELEM_WISE, LAYERNORM, FUSED_ATTN) | all PASS, 0 mismatches | 244 / 244 equal | 39,922,316 (reference: 39,922,316) |

The total cycles of the reference runs (62,681,721 and 39,922,498, section 3 of the release notes) additionally
count the host register writes before the first instruction. `tb_has_npu_top` itself gives the same cycles with a
10 ns and a 1.25 ns clock (`--clk-ns`).

## 9. Pending verification

| Item | Content | Status |
|---|---|---|
| Delivery acceptance run | All 98 instructions of YOLOv8m through `HasNpuTop`, overlapped, HAS DMA, compared with the golden | Done: PASS (V12, section 6.4) |
| Transformer pipelines | SOFTMAX, LAYERNORM and FUSED_ATTN bit-exact, then ViT-B/16 through the network flow | Done: V17, V18. Open points of the drawings stay knobs until hardware answers (`KNOWN_LIMITATIONS.md`, section 7.1) |
| Accuracy | mAP evaluation on a dataset | Not planned in this package. Integer against floating point on 112 images (YOLOv8m) and 5 images (ViT-B/16), four and two images through the model (section 6.7) |

## 10. Reproducing the checks

| Check | Command or script |
|---|---|
| V2, V3 | `tools/has/a1_rerun.sh` (tile jobs of `fe_work/core/jobs.tsv`, capacity checker enabled) |
| V4 to V6 | Network testbenches, see `BUILD_AND_RUN.md`; post-processing `bash tools/metrics/post_fullcore.sh --run <run dir> --out <dir> --net <net dir> --draw` |
| V7 | `tools/has/tb_has_mmio_replay <net dir> <insts dir>/mmio.txt` |
| V8, V9 | `bash tools/has/build_tb_has_npu_top.sh`, then `tools/has/tb_has_npu_top <insts dir> --profile legacy [--first F --count N] [--overlap] [--dma has]` |
| V12 | `python3 tools/fe/fe_emit_insts.py --plan has` (writes `$FE_WORK/has/insts_has`), then `tools/has/tb_has_npu_top $FE_WORK/has/insts_has --profile legacy --overlap --dma has`; measured log: `reference_runs/yolov8m_baseline/` of the data package |
| V15, V16 | `tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3` (default profile); `tools/has/tb_has_npu_top $FE_WORK/has/insts_pe4 --profile proposals` (`BUILD_AND_RUN.md`, section 6) |
| V17, V18 | `tools/has/tb_has_npu_top $FE_WORK/has/rce_gate` (program shipped in the data package; `python3 tools/has/make_rce_gate.py <vector dir> <table dir> <out dir>` regenerates it); `tools/has/tb_has_npu_top $FE_WORK/has/vit_full --c-bcast` |
| V10 | `tools/has/tb_gvu_quant <vector files>`, `tools/has/tb_gvu_lut`, `tools/has/tb_gvu_elemwise`, `tools/has/tb_has_dma` |
| V11 | `bash tools/has/regress_native.sh` |
| V19 | The integrator's own TLM wrapper and its unit test (not part of this package), rebuilt with the model root set to this tree (`SW_INTEGRATION_GUIDE.md`, section 9); expected: 0 failed checks, output identical to the previous native revision |
| V20 | Wrapper with `tools/has/wrapper/npu_tlm_rich_window.patch` applied and the model root set to `core_rtl/`; build `tools/has/wrapper/test_npu_tlm_core_rtl.cpp` with the wrapper sources (build line in the file header, `SW_INTEGRATION_GUIDE.md`, section 10); run `test_npu_tlm_core_rtl $FE_WORK/has/insts_pe3 all` and `SAURIA_CORE_RTL_C_BCAST=1 test_npu_tlm_core_rtl $FE_WORK/has/vit_full all`; compare the per-instruction cycles with `reference_runs/yolov8m_run_c/run.log` and `reference_runs/vit_b16/run.log` of the data package. A whole network takes hours; `test_npu_tlm_core_rtl <insts dir>` alone runs the first two instructions |
| V21 | `python3 tools/has/make_avgpool_gate.py <out dir>` then `tools/has/tb_has_npu_top <out dir>`; `g++ -std=c++17 -O2 -I. tools/has/tb_gvu_vrf.cpp -o tb_gvu_vrf && ./tb_gvu_vrf`; window D: `tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3 --first 40 --count 2` (1,658,986 cycles) |
| V22 | Programs for another image: `python3 tools/fe/fe_has_export_net.py --mode has --image-path <jpg>` then `python3 tools/fe/fe_emit_insts.py --plan pe3`; `python3 tools/fe/fe_vit_full.py --image <coco128 index>` (these need the trained weights, not in the package); then `FE_SNAPSHOT_DIR=<dir> tools/has/tb_has_npu_top <insts dir>` and `tools/fe/fe_sysc_detect.py --dram <snapshot> --net-dir <insts dir> --compare-golden` (boxes) or `tools/fe/fe_vit_top5.py --net-dir <vit dir> --dram <snapshot>` (top-5 classes). Measured log of the ViT-B/16 run: `reference_runs/vit_b16_as_drawn/` of the data package |
| V23 | Step 2 as V22, one program per image. Step 1 needs the trained weights and the coco128 images and was run with scripts that are not part of the package: it calls the golden of the frontend (`tools/fe/fe_ref_has.py` for YOLOv8m, the golden chain of `tools/fe/fe_vit_full.py` for ViT-B/16) and the floating-point references on each image and compares the decoded results |
| V24 | Wrapper unit test as V19, with and without `tools/has/wrapper/npu_tlm_rich_window.patch`; `test_npu_tlm_core_rtl $FE_WORK/has/rce_gate all` and `test_npu_tlm_core_rtl $FE_WORK/has/insts_pe3`; firmware engine: `python3 tools/has/wrapper/fw/make_fw_program.py <insts dir> <out>`, build `tools/has/wrapper/fw/test_fw_replay.cpp` with `npu_has_fw.c` and the wrapper sources (header of the file), `test_fw_replay <out>.nhp <insts dir>/dram_init.bin`, or `test_fw_replay <out>.nhp <insts dir>/dram_golden.bin host-only` for the host steps and CRCs alone (`SW_INTEGRATION_GUIDE.md`, section 10.5) |
| V25 | `tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3` and `... $FE_WORK/has/vit_full` (default configuration); logs in `reference_runs/yolov8m_default/` and `reference_runs/vit_b16_default/`. Per-instruction measurements: `bash tools/has/measure_mas_instr.sh <binary> <program dir> <csv>` |
| V26 | `HAS_SPEC=rtl tools/has/tb_has_npu_top <program dir>`; `HAS_SPEC=mas10 ...` |
| V27 | `python3 tools/has/mas_ref.py <dir>`, then `tools/has/tb_gvu_mas <dir>` (built with `g++ -std=c++17 -O2 -I. tools/has/tb_gvu_mas.cpp`); `python3 tools/fe/fe_ref_gvu.py --only mas`; all of it with the instruction-path checks: `bash tools/has/regress_gvu.sh` |
| V28 | `make tb_obp tb_re tb_pipeline_re tb_unified_smoke` (use `make -B`), `bash tools/has/regress_native.sh`, `bash tools/has/regress_gvu.sh` |
| V30 | (a) `tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3 --first 40 --count 2` in the default build and in the build with `-DFE_METRICS` and `FE_METRICS_CSV=<file>`: compare the line "core busy" with the sum of the column `busy`. (b), (c) as V29 with `tools/has/wrapper/test_npu_tlm_core_rtl.cpp`; stderr shows the warning lines. (d) `SAURIA_CORE_RTL_RECORD=<dir> test_npu_tlm_core_rtl <program dir> <count>`, then `tools/has/tb_has_npu_top <dir>/segNNN --trace`. (e) `tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3` and `... $FE_WORK/has/vit_full`. (f) needs a test program that pushes without waiting; it is not part of the package |
| V29 | As V24 without the patch step: build `components/npu_tlm/src/npu_tlm.cpp`, the bus router and memory models and `tools/has/wrapper/fw/test_fw_replay.cpp` with `-Icore_rtl -Icore_rtl/driver -Icore_rtl/instrumentation`; `test_fw_replay <program.nhp> <dram image>` |
| V31 | `python3 tools/has/make_pad_gate.py <dir>` (`--set random --seed S --count N` for random layers), then `python3 tools/has/ref_program.py <dir>`, `tools/has/tb_has_npu_top <dir>` and `HAS_FEEDER_PAD=1 tools/has/tb_has_npu_top <dir>`; compare the lines "RESULT" and "core busy". Whole network: `HAS_FEEDER_PAD=1 tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3`. Unit test: `g++ -std=c++17 -O2 -I. tools/has/tb_has_pad_gen.cpp`. All of it is part of `bash tools/has/regress_gvu.sh` except the whole network |
| V32 | `python3 tools/has/make_pad_gate.py <dir> --set smallk`, then `ref_program.py` and `tb_has_npu_top` on it; the corrections are switched off with `-DFX1_A3_WEI_DONE_Q_GATE_OFF -DFX1_A3_INCNTLIM_ZERO_RTL_OFF` for the comparison. The RTL runs need the SAURIA oracle flow, which is not part of this package |
| V33 | `python3 tools/has/make_pad_gate.py <dir> --set dilation`, then as V31; unit tests as V31 (`tb_has_mmio_compat`, `tb_has_pad_gen`); all of it is in `regress_gvu.sh`. The comparison with the RTL needs the SAURIA oracle flow, not part of this package |
| V34 | `tools/has/tb_has_mmio_compat` (C13) in `regress_gvu.sh`; the boundary programs are written by short scripts around `make_pad_gate.py` conventions and run with `tb_has_npu_top`; the RTL runs and the 12-bit builds need the SAURIA oracle flow, which is not part of this package |
| V35 | `bash tools/has/regress_gvu.sh` (items `tb_gvu_top_vs_model`, instructions 86-88 with `HAS_OBP_RTL=1` and the replay of its beats, `HAS_GVU_TOP=1`, `HAS_GVU_TOP=2`, ELEM_WISE of YOLOv8m on `GvuTop`); window D: `HAS_GVU_TOP=1 tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3 --first 40 --count 2`; ViT-B/16 ELEM_WISE: `tools/has/tb_rtl_ref_gvu_replay $FE_WORK/has/vit_full 0 --gvu-top` |
| V36 | `bash tools/has/regress_gvu.sh` (level-B, parameter, max pool and core-stop items); whole networks: `HAS_GVU_TOP=3 tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3` and `$FE_WORK/has/vit_full`; max pool against the RTL: `bash tools/has/gvu_oracle/run_verilator_maxpool.sh <rtl gvu dir> <work dir> <vector dir> <tb_gvu_top_maxpool binary>` |
| V37 | `g++ -std=c++17 -O2 -I. tools/has/tb_gvu_regs_dfc.cpp -lsystemc -o t && ./t 2 && ./t 4 && ./t 7`; `bash tools/has/regress_gvu.sh` (register file, address generator and check-program items); one instruction: `HAS_GVU_TOP=3 HAS_GVU_STALL=1 HAS_GVU_ELEM_CLK=1 HAS_GVU_REGS=1 tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3 --first 66 --count 1`; register file against the RTL: `tools/has/gvu_oracle/run_modelsim_cfgregs.sh` and `run_verilator_cfgregs.sh` |
| V38 | `tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3` and `... $FE_WORK/has/vit_full` with no variable set; the earlier configuration: `HAS_GVU_TOP=0 HAS_GVU_STALL=0 HAS_GVU_ELEM_CLK=0 HAS_GVU_REGS=0 HAS_GVU_AGU_ND=0` |
| V39 | Sets: `g++ -std=c++17 -O2 -I. tools/has/dump_maxpool.cpp -o dump && ./dump $FE_WORK/has/insts_pe3 <dir>`, then `python3 tools/has/gvu_oracle/gen_sppf_sets.py <dir> <sets>`. Model trace: `tools/has/tb_gvu_top_maxpool_runs.cpp` (`g++ -std=c++17 -O2 -I.`; arguments input, trace, H, W, k, s, C, channels per run). RTL trace: `tools/has/gvu_oracle/tb_oracle_maxpool_runs.sv` with the RTL tree and the stand-ins of `tools/has/gvu_oracle/` (plusargs `+in +out +h +w +k +s +c +group`). Compare the traces with `diff` and each run with `tools/has/gvu_oracle/chk_maxpool_runs.py` |
| V13, V14 | Earlier planner options, superseded by V15 (same flow and checks, options of section 6.5); rerun V15 instead |
| V1 | Requires the reference SAURIA core model, which is not part of this package |
