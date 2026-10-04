# Verification Report

This document states how the correctness of the delivered model was established and what each check proved.
Every number carries a label:

- **MEASURED**: produced by running the model; the source file is given.
- **DERIVED**: computed from measured numbers by a stated formula.
- **ESTIMATED**: produced by a formula or assumption, not by a run.

Performance numbers are in `PERFORMANCE_REPORT.md`; the arithmetic questions still open with the hardware
team are in `HAS_VALIDATION.md`; modelling simplifications are in `KNOWN_LIMITATIONS.md`.

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
| V11 | Native regression | `tb_obp`, `test_vit`, `test_yolo`, `test_onnx_model`, `tb_unified_smoke`, rebuilt from current sources | All pass; network results identical to the previous binaries; `tb_obp` 11 / 11 including negative bias and table read-back | MEASURED |
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
| `tb_obp` | 11 / 11 | MEASURED |
| `test_vit` | 4 / 4 | MEASURED |
| `test_yolo` | 261 / 261 | MEASURED |
| `test_onnx_model` | 497 / 497 | MEASURED |
| `tb_unified_smoke` | all checks pass | MEASURED |

The network results are identical to the previous binaries. `tb_obp` adds two cases to the previous nine: negative
bias values written and read back, and the scale and shift tables read back through the host bus;
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
| V13, V14 | Earlier planner options, superseded by V15 (same flow and checks, options of section 6.5); rerun V15 instead |
| V1 | Requires the reference SAURIA core model, which is not part of this package |
