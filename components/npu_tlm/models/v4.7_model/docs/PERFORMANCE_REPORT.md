# Performance Report

This document reports the cycle counts measured on the delivered model and explains which numbers describe
the NPU and which describe the testbench. Labels as in `VERIFICATION_REPORT.md`:

- **MEASURED**: counted by the model during a run.
- **DERIVED**: computed from measured numbers by the stated formula.
- **ESTIMATED**: produced by a formula or assumption, not by a run.

Clock frequency for all time and throughput figures: 0.8 GHz (assumption of the HAS).

## 1. Read this first

1. **Core cycles are the reliable numbers.** They come from the cycle-accurate core, which matches the reference
   SAURIA core cycle for cycle (`VERIFICATION_REPORT.md`, V1). They depend only on the tile shapes: three full
   runs with different images and different epilogue arithmetic give exactly the same core counts.
2. **Testbench cycles of the full-network runs (sections 3, 4) use an optimistic DMA.** The DMA of those runs
   (`control/sauria_dma.h`) models a 256-bit bus (32 bytes per cycle) without DRAM latency. The HAS specifies a
   128-bit AXI bus. Measured on the same traffic, the HAS bus takes 2.00x the DMA cycles (section 5.1). The DMA
   part of those totals is therefore about 2x too small. Core cycles are not affected.
3. **Do not add the component counters.** DMA wait, OBP and ELEM_WISE cycles overlap with each other and with
   the core in parts of the flow; the testbench total is the only valid sum.
4. **Do not compare testbench totals across DMA models.** Only core cycles compare across runs with different
   DMA models.
5. **Which whole-network number to quote.** Run C (section 5.4, 62.7 M cycles, 12.76 FPS at 0.8 GHz) is the
   reference for the hardware as drawn. Run D (section 5.5, 60.4 M cycles, 13.25 FPS) assumes two proposed
   hardware options and is valid only if the hardware team adopts them. Runs A and B are kept for traceability.
   For ViT-B/16 quote section 5.6: 41.1 M cycles, 19.48 FPS with the hardware as drawn (39.9 M cycles, 20.04 FPS
   with the bias-broadcast proposal); ViT-B/16 has a
   larger estimated share than YOLOv8m.

## 2. Workload

| Quantity | Value | Label |
|---|---:|---|
| Network | YOLOv8m int8, input 640x640 | |
| Convolution layers | 83 | |
| Tiles on the core | 9,811 in the plan of sections 3 to 5.3; 7,299 in the reference run C (section 5.4) | MEASURED |
| Multiply-accumulates (theoretical) | 39,467,827,200 | MEASURED (workload counter) |
| Array | 32x32 = 1,024 processing elements | |
| Second network (section 5.6) | ViT-B/16, 197 tokens, 244 instructions, 17,714,946,048 real multiply-accumulates | DERIVED from the instruction shapes |

## 3. Core performance

Sections 3.1, 3.2 and 4 describe the tile plan of the earlier testbench runs (9,811 tiles, PE utilization
37.4 %). They are the baseline of the data-flow and planner options of section 5.4; the per-layer figures of the
reference run C are in section 3.3.

Source: per-tile counters of the full-network runs, aggregated per layer and per network. Identical in the
runs with image 1, image 2 and the HAS arithmetic.

| Quantity | Value | Label |
|---|---:|---|
| Core busy cycles | 103,044,189 | MEASURED (controller state machine) |
| Execute cycles | 75,411,857 | MEASURED |
| Stall cycles (busy - execute) | 27,632,332 | DERIVED |
| Array-busy state share of busy cycles | 80.1 % | MEASURED |
| PE utilization = MAC / (busy x 1,024) | 37.40 % | DERIVED |
| Core throughput | 612.8 GOPS | DERIVED |
| Frame rate if only core busy time counted | 7.76 FPS | DERIVED (0.8e9 / busy) |

PE utilization is the product of three factors: spatial use of the array 53.97 %, temporal use 94.70 % and
pipeline efficiency 73.18 % (DERIVED).

### 3.1 Per-layer distribution

| Layer | Tiles | Core busy cycles | PE utilization | Label |
|---|---:|---:|---:|---|
| `dark5.conv` | 720 | 10,118,160 | 7.7 % | MEASURED / DERIVED |
| `neck.d5.conv` | 480 | 6,745,440 | 7.7 % | MEASURED / DERIVED |
| `det.p4.cls1` | 240 | 6,690,480 | 15.5 % | MEASURED / DERIVED |
| `det.p5.cls1` | 96 | 4,998,624 | 7.8 % | MEASURED / DERIVED |
| `stem` | 204 | 3,629,796 | 3.6 % | MEASURED / DERIVED |
| `det.p3.cls1`, `det.p3.cls2` | 144 each | 2,528,640 each | 82.0 % | MEASURED / DERIVED |
| `dark3.c2f.*` bottleneck convolutions | 36 each | 632,160 each | 82.0 % | MEASURED / DERIVED |

The five most expensive layers take 31.2 % of the core time (DERIVED). Four of them run at under 8 %
utilization: their tiles use 16 output channels by 16 positions (a quarter of the array) or have few
multiply-accumulates per context, so context switching and partial-sum drain dominate. The first layer (`stem`)
has 27 multiply-accumulates per context. These layers were the main target of the planner options of section 5.4 (result per layer
in section 3.3).

### 3.2 Effect of the tile planner

The same core with the earlier minimum-tile-count plan needed 209.65 M busy cycles. The cost-model plan used in
all runs of this report (cycle objective, input prefetch, skip tensors kept in the scratch area) needs
103.04 M, a reduction of 50.9 % (MEASURED on both plans). The prediction made before the run was 107 to 110 M.

### 3.3 Per layer in the reference run C

| Layer | Tiles / core passes | Instruction cycles | Core busy cycles | PE utilization (real MACs) | Label |
|---|---:|---:|---:|---:|---|
| `det.p3.cls1`, `det.p3.cls2` | 144 / 144 each | 2,561,150 each | 2,528,640 each | 82.0 % | MEASURED / DERIVED |
| `dark4.conv` | 480 / 480 | 2,003,640 | 1,793,280 | 57.8 % | MEASURED / DERIVED |
| `stem` | 320 / 320 | 1,842,586 | 1,466,560 | 8.8 % | MEASURED / DERIVED |
| `det.p4.cls1` | 48 / 96 | 1,712,206 | 1,685,760 | 61.5 % | MEASURED / DERIVED |

Per layer the PE utilization ranges from 8.8 % (`stem`, 27 multiply-accumulates per context) to 89.2 %, median
61.5 %. The five layers under 40 % hold 3.9 % of the core busy cycles, and the five most expensive layers take
17.0 % of the instruction cycles (DERIVED). The time is now spread over the network instead of concentrated in a
few layers. Full table: `python3 tools/metrics/has_rollup.py $FE_WORK/reference_runs/yolov8m_run_c`
(`instructions.csv`), or `post_fullcore.sh --layers` (section 7), which also gives the array columns and rows used
and the spatial usage of every layer. The same table for the baseline comes from `reference_runs/yolov8m_baseline`.

## 4. Full-network testbench cycles

These totals include the core, the DMA of the network testbench (optimistic, see section 1), the epilogue,
host steps at zero cycles, and per-tile register programming. They run the tiles sequentially; only the input
window of the next tile is prefetched.

| Quantity | v4.5 OBP arithmetic, image 1 | v4.5 OBP arithmetic, image 2 | HAS arithmetic | Label |
|---|---:|---:|---:|---|
| Steps | 117 | 117 | 129 | |
| Core busy | 103,044,189 | 103,044,189 | 103,044,189 | MEASURED |
| Testbench total | 131,585,210 | 131,585,210 | 133,638,300 | MEASURED |
| DMA wait | 23,475,790 | not collected | 23,276,288 | MEASURED, overlaps |
| OBP | 2,501,760 | not collected | 2,583,040 | MEASURED |
| OBP configuration | 1,435,200 | not collected | 1,454,822 | ESTIMATED |
| ELEM_WISE | included in OBP | | 2,151,690 (DMA 1,301,560 + compute 850,130) | ESTIMATED (formula in `has/gvu_elemwise.h`) |
| Frame rate from testbench total | 6.08 FPS | 6.08 FPS | 5.99 FPS | DERIVED, optimistic DMA |

The HAS run is 2,053,090 cycles (1.56 %) longer. The difference is the separate ELEM_WISE steps (ESTIMATED),
partly offset by no longer loading skip tensors into the scratch area. The total was predicted before the run
at about 133.6 M; the measurement differs by less than 0.03 %.

**The testbench total is not a hardware latency.** With the HAS DMA the sequential total would be higher; with
the overlapped schedule of the delivery top level it is lower (section 5).

## 5. Delivery top level `HasNpuTop`

`HasNpuTop` executes the v4.5-protocol instruction stream with its data flow controller. It has two schedules:

- **Sequential**: DMA in, core, epilogue, DMA out, one tile after another.
- **Overlapped**: the DMA fills the host half of the core's ping-pong SRAM for tile i+1 while the core computes
  tile i; the epilogue and write-back of tile i-1 run in parallel. Weights are kept when the output-channel range
  does not change.

### 5.1 DMA model

| DMA setting | Bytes per beat | Burst | DRAM latency | CH3 priority | DMA cycles, one reference tile |
|---|---:|---:|---:|---|---|
| v4.5 (used in sections 3 and 4) | 32 | 8 beats | 0 | round robin | 1.00x |
| HAS AXI-128 | 16 | 8 beats | 0 | lowest | 2.00x (MEASURED) |
| HAS AXI-128, latency 50 / 100 | 16 | 8 beats | 50 / 100 | lowest | 2.02x / 2.05x (MEASURED) |

DRAM latency is a parameter (`has::DmaParams::dram_latency`) with no hardware value yet (ESTIMATED when used).

### 5.2 Measured windows

Two windows of the network were run in all modes (window contents in `VERIFICATION_REPORT.md`, section 6.2).
The lower bound is the core busy time of the same tiles plus the ELEM_WISE time: no schedule can be faster.

| Window | Sequential, v4.5 DMA | Sequential, HAS DMA | Overlapped, HAS DMA | Overlapped vs sequential (HAS DMA) | Lower bound | Overlapped / lower bound |
|---|---:|---:|---:|---:|---:|---:|
| A (280 tiles, 2 ADD) | 6,463,541 | 7,788,332 | 5,305,513 | -31.9 % | 4,863,460 | 1.091 |
| B (714 tiles, 3 MAX, 1 host step) | 5,339,305 | 6,285,211 | 4,320,235 | -31.3 % | 4,123,650 | 1.048 |

Labels: all cycle counts MEASURED; the lower bound is DERIVED from measured core busy plus ESTIMATED ELEM_WISE.

Observations:

- The HAS DMA makes the sequential schedule 17.7 to 20.5 % slower than with the v4.5 DMA.
- The overlapped schedule recovers all of it and is 17.9 to 19.1 % faster than the sequential schedule with the
  optimistic v4.5 DMA.
- The overlapped schedule reaches 1.05 to 1.09 times the lower bound. The remaining 0.2 to 0.4 M cycles per
  window are mostly the per-tile reset and core register programming (about 50 register writes), which cannot
  overlap because the core must be configured before it starts.
- The epilogue still runs after the core in the model, reading the partial-sum SRAM back. In the overlapped
  schedule it runs in parallel with the next tile, so its time cost is close to the hardware behaviour.

### 5.3 Full network through `HasNpuTop`

| Quantity | Value | Label |
|---|---:|---|
| Instructions / tiles on the core | 98 (+ 2 host steps) / 9,811 | MEASURED |
| Result against the golden | PASS, 0 mismatches (`VERIFICATION_REPORT.md`, V12) | MEASURED |
| Total cycles, overlapped, HAS DMA | 112,912,950 | MEASURED |
| DMA wait | 17,898,848 (overlaps with compute) | MEASURED |
| OBP | 2,583,040 | MEASURED |
| Core busy | 103,044,189 (same tiles as sections 3 and 4) | MEASURED |
| Frame rate at 0.8 GHz | 7.09 FPS | DERIVED (0.8e9 / total) |
| Ratio to the lower bound (103.04 M core busy + 2.15 M ELEM_WISE) | 1.073 | DERIVED (ELEM_WISE part ESTIMATED) |

The expectation recorded from the windows was 1.05 to 1.10 times the lower bound; the measurement is inside
that range.

Compared with the testbench-driven run of section 4 with the HAS arithmetic (133.6 M cycles), this run is
15.5 % faster although its DMA is about 2x slower (HAS AXI-128 instead of the optimistic v4.5 DMA). The gain
comes from the overlapped schedule. This is the first whole-network total in this report that uses the HAS
bus width; it is the reference for section 5.4. DRAM latency is 0 in this run (see section 6).

### 5.4 Whole network with the GVU data flow and tile-planner options

Three runs, all from the memory-mapped instruction stream through `HasNpuTop`, overlapped schedule, HAS AXI-128
DMA timing, all data-flow options of `ARCHITECTURE.md`, section 8:

- **Run A**: planner options of section 8.2 (input-channel split on 6 layers, single-row 1x1 on 22 layers,
  stem with 32 output channels) and the layer tile order.
- **Run B**: run A plus padded edge tiles (`FLAGS.PAD_TAIL`), larger tiles, and 31 positions per context for
  3x3 stride-2 convolutions (`Y_USED`).
- **Run C** (current reference): run B with the corrected weight-buffer selection of the data flow controller
  (a weight block already held by the other buffer is reused by a buffer swap, not reloaded) and padded edge tiles
  disabled on the three layers where measurement showed them slower (the stem and two detection-head outputs).

| Quantity | Section 5.3 (reference) | Run A | Run B | Run C | Label |
|---|---:|---:|---:|---:|---|
| Result against the golden | PASS, 0 mismatches | PASS, 0 mismatches | PASS, 0 mismatches | PASS, 0 mismatches | MEASURED |
| Elements compared | 58,425,600 | 58,425,600 | 58,425,600 | 58,425,600 | MEASURED |
| Tiles / core passes | 9,811 / 9,811 | 7,735 / 8,559 | 7,299 / 7,695 | 7,299 / 7,695 | MEASURED |
| Core busy cycles | 103,044,189 | 65,062,662 | about 55.7 M | 55,638,052 | MEASURED (run B: DERIVED from the PE utilization) |
| PE utilization (real MACs / (busy x 1,024)) | 37.40 % | 59.24 % | 69.22 % | 69.27 % | DERIVED |
| Core throughput on core busy | 612.8 GOPS | 971 GOPS | 1,134 GOPS | 1,135 GOPS | DERIVED |
| Throughput on total cycles | 559 GOPS | 864 GOPS | 985 GOPS | 1,007 GOPS | DERIVED |
| Total cycles | 112,912,950 | 73,128,197 | 64,109,163 | 62,681,721 | MEASURED |
| DMA wait (overlaps with compute) | 17,898,848 | 8,001,580 | 8,470,963 | 7,085,914 | MEASURED |
| Frame rate at 0.8 GHz | 7.09 FPS | 10.94 FPS | 12.48 FPS | 12.76 FPS | DERIVED |
| Change of total cycles against section 5.3 | | -35.2 % | -43.2 % | -44.5 % | DERIVED |

In runs B and C the padded channels and positions are also computed (about 44 G MACs executed against 39.47 G
real). In run C, 4,320 weight blocks were loaded by DMA and 1,338 were reused by a buffer swap (MEASURED).
PE utilization and throughput are computed on the real MACs only.

**Predictions recorded before the runs** (ESTIMATED; reported against, the model was not tuned):

| | Predicted | Measured | Deviation |
|---|---|---:|---|
| Run A core busy | 65.09 M | 65,062,662 | -0.04 % |
| Run A total | 68.9 to 71.1 M | 73,128,197 | +2.9 % above the upper bound |
| Run B total | 58.5 to 60.0 M | 64,109,163 | +6.8 % above the upper bound |
| Run C total | 62.6 to 63.1 M | 62,681,721 | within the range |

The core-cycle model is accurate. The excess is in DMA and overlap: about 45 % of the run A excess comes from
the two stride-2 layers with an input-channel split, about 24 % from 1x1 layers on large maps, about 16 % from
the first stride-2 layers with large input images.

**Modelling defect in runs A and B, corrected in run C.** In runs A and B the data flow controller reloaded the
weights of every input-channel part, although the other weight buffer already held the needed block. The model
now selects a reused block by a buffer swap only. On `dark5.conv` alone the fix saves 19.0 % of the layer's
cycles (MEASURED on a window, bit-exact). Run C is the rerun with the corrected controller; its total was
predicted before the run and falls within the predicted range. Runs A and B are kept for traceability; run C
replaces them as the reference.

Summary of the whole-network totals in this report:

| Run | Total cycles | FPS at 0.8 GHz | DMA model |
|---|---:|---:|---|
| Testbench-driven, HAS arithmetic (section 4) | 133.6 M | 5.99 | v4.5, optimistic by about 2x |
| `HasNpuTop`, before optimizations (section 5.3) | 112.9 M | 7.09 | HAS AXI-128 |
| `HasNpuTop`, run A | 73.1 M | 10.94 | HAS AXI-128 |
| `HasNpuTop`, run B | 64.1 M | 12.48 | HAS AXI-128 |
| `HasNpuTop`, run C (current reference) | 62.7 M | 12.76 | HAS AXI-128 |
| `HasNpuTop`, run D (run C plus two hardware proposals, section 5.5) | 60.4 M | 13.25 | HAS AXI-128 |
| `HasNpuTop`, ViT-B/16, hardware as drawn (section 5.6) | 41.1 M | 19.48 | HAS AXI-128 |
| `HasNpuTop`, ViT-B/16 with the bias-broadcast proposal (section 5.6) | 39.9 M | 20.04 | HAS AXI-128 |

DRAM latency is 0 in all these runs. A latency of 100 cycles adds 4.85 % on a DMA-limited window (MEASURED);
the whole-network effect is ESTIMATED at 1 to 5 %.

### 5.5 Hardware proposals evaluated on the whole network

Run C uses the hardware as drawn. Run D adds two options that the drawings do not contain and that are
proposed to the hardware team. Both change only bus traffic and timing; the data are unchanged and run D is
bit-exact (`VERIFICATION_REPORT.md`, V16).

- **Bias preload by broadcast descriptor.** In the drawn data flow the bias of a tile is preloaded into SRAM-C
  as one 32-bit word per output element (channels x positions) over DMA channel 3. With a broadcast descriptor
  only the bias vector (4 bytes per channel) crosses the bus and the descriptor replicates it over the positions.
- **Halo reuse in SRAM-A.** Tiles are walked column by column; the input rows shared with the previous tile
  (kernel height minus stride) are copied inside SRAM-A instead of being read again from DRAM. The copy engine
  and its SRAM port are assumptions (32 bytes per cycle).

Run D also uses a planner refresh that changes the tile configuration of four small layers.

| Quantity | Run C (hardware as drawn) | Run D (with both proposals) | Label |
|---|---:|---:|---|
| Result against the golden | PASS, 0 mismatches | PASS, 0 mismatches | MEASURED |
| Tiles / core passes | 7,299 / 7,695 | 7,301 / 7,697 | MEASURED |
| Core busy cycles | 55,638,052 | 55,600,372 | MEASURED |
| PE utilization (real MACs) | 69.27 % | 69.32 % | DERIVED |
| Throughput on core busy / on total cycles | 1,135 / 1,007 GOPS | 1,136 / 1,046 GOPS | DERIVED |
| DMA wait (not hidden by the ping-pong schedule) | 7,085,914 | 4,828,643 | MEASURED |
| Total cycles | 62,681,721 | 60,386,685 | MEASURED |
| Frame rate at 0.8 GHz | 12.76 FPS | 13.25 FPS | DERIVED |
| Change of total cycles against run C | | -3.66 % | DERIVED |

**Where the gain comes from** (DERIVED from per-layer measurements and window runs with one option at a time):

| Option | Cycles saved on the whole network | Share |
|---|---:|---:|
| Bias preload by broadcast descriptor | about 2.29 M | 99.6 % |
| Halo reuse in SRAM-A (33 layers, 1,832 tiles, 16.0 MB copied inside SRAM-A) | about 2.5 k | about 0.1 % |
| Planner refresh (four layers) | about 6 k | about 0.3 % |

Halo reuse saves 5.3 % on a layer that is limited by input DMA (`dark4.conv`, MEASURED on a window, also with a
free copy engine: the copy is hidden behind the core), but in this configuration almost every layer is limited
by the core, so the network total barely moves. It becomes relevant only with smaller tiles or a narrower bus.
The bias broadcast is the proposal with a measurable benefit.

**Predictions recorded before the run** (ESTIMATED): 59.3 to 60.7 M cycles, refined to 60.1 to 60.5 M after the
window runs and before the run finished. Measured 60,386,685: within both ranges. Per layer, 5 of 83
convolutions deviate from the cycle model by more than 3 % (at most +14.4 % on a small detection-head layer that
became limited by the core); the model was not tuned.

### 5.6 Vision transformer ViT-B/16 through `HasNpuTop`

The whole ViT-B/16 network (patch embedding, 12 encoder blocks, final LayerNorm and the 1,000-class head) runs
from the memory-mapped instruction stream only: 244 instructions, no host step. Matrix products are GEMM_FUSED
instructions on the core (tokens as pixels of a 1x1 layer); attention heads are FUSED_ATTN instructions and the
normalizations are LAYERNORM instructions (`SW_INTEGRATION_GUIDE.md`, sections 4.4 and 4.5). Same timing options as
run D of section 5.5 except halo reuse (layer tile order, bias preload by broadcast descriptor), plus the attention
products on the core.

| Quantity | Value | Label |
|---|---:|---|
| Result against the golden | PASS, 244 of 244 instructions, 26,068,616 elements, 0 mismatches | MEASURED |
| Total cycles | 39,922,498 | MEASURED |
| Patch embedding (GEMM + position add) | 208,685 | MEASURED |
| 12 encoder blocks | 39,436,637 (3,286,386 per block on average) | MEASURED |
| Final LayerNorm + head | 277,179 | MEASURED |
| Core busy cycles, GEMM_FUSED instructions | 21,830,592 (54.7 % of the total) | MEASURED |
| Attention products Q.K^T and A.V on the core, inside the FUSED_ATTN instructions (not part of the row above) | 3,147,984 (7.9 %) | MEASURED |
| DMA wait (not hidden by the ping-pong schedule) | 11,773,826 (29.5 %) | MEASURED on the DMA model |
| Softmax, LayerNorm and their DMA in the vector unit | 5,284,480 (13.2 %) | ESTIMATED |
| PE utilization, GEMM_FUSED (real MACs 1.7000e10 / (core busy x 1,024)) | 76.05 % | DERIVED |
| PE utilization including the attention products (real MACs 1.7715e10 for 197 tokens) | 69.26 % | DERIVED |
| Throughput on GEMM core busy / on total cycles | 1,246 / 710 GOPS | DERIVED |
| Frame rate at 0.8 GHz | 20.04 FPS | DERIVED |

One encoder block (MEASURED, cycles per instruction): LayerNorm 41,920; QKV projection 517,782; each of the 12
attention heads 29,420; output projection 178,758; residual add 29,848; LayerNorm 41,920; first MLP layer with GELU
691,390; second MLP layer (input-channel split in 4 passes) 1,401,872; residual add 29,848.

**Prediction recorded before the run** (ESTIMATED): 39.85 to 40.10 M cycles. Measured 39,922,498: -0.02 % from
the centre of the range.

**Hardware as drawn (without the bias-broadcast proposal).** The table above uses the bias-broadcast proposal. With
the default profile only:

| Quantity | As drawn | With bias broadcast | Label |
|---|---:|---:|---|
| Total cycles | 41,065,258 | 39,922,498 | MEASURED |
| Frame rate at 0.8 GHz | 19.48 FPS | 20.04 FPS | DERIVED |
| DMA wait (not hidden by the ping-pong schedule) | 12,916,626 | 11,773,826 | MEASURED |
| Core busy cycles, PE utilization | unchanged | 21,830,592, 76.05 % | MEASURED |

The difference (+2.86 %) is the full bias preload read of the GEMM_FUSED instructions. Measured on coco128 image
000000000502 with an instruction stream identical to the shipped program; timing does not depend on data values, and
the first 23 instructions of the shipped program give identical cycles. Prediction recorded before the run: 40.4 to
41.2 M cycles (within the range). The log of this run is `reference_runs/vit_b16_as_drawn/` of the data package; a
third image gives the same cycles (`VERIFICATION_REPORT.md`, V23).

**Reading the ViT numbers.** Unlike YOLOv8m, where the core accounts for about 89 % of the total, only 54.7 % of
the ViT total comes from the cycle-accurate core for the GEMM_FUSED instructions, and 7.9 % for the attention
products. DMA wait and the vector-unit stages are 43 % together, so the
ViT frame rate depends much more on the DMA model and on the estimated softmax and LayerNorm latencies. The
largest DMA costs are the Q, K and V operands of the attention heads and the second MLP layer (3,072 input
channels). Accuracy of the integer network against the floating-point model is limited by the per-tensor
LayerNorm quantization (`KNOWN_LIMITATIONS.md`, Q4); it does not affect bit-exactness.

### 5.7 Summary metrics of the reference runs

The table collects the metrics of the two reference runs with the hardware as drawn (YOLOv8m run C, section 5.4;
ViT-B/16 as drawn, section 5.6) and of the YOLOv8m baseline before the planner and data-flow options (section 5.3).
All at 0.8 GHz. `bash tools/metrics/post_fullcore.sh` prints the same tables from the logs of the data package
(section 7).

End to end (whole testbench: core, DMA not hidden by the schedule, epilogue, vector unit, register writes):

| Metric | YOLOv8m baseline | YOLOv8m run C | ViT-B/16 as drawn | Label |
|---|---:|---:|---:|---|
| Total cycles | 112,912,950 | 62,681,721 | 41,065,258 | MEASURED |
| Latency per image | 141.1 ms | 78.4 ms | 51.3 ms | DERIVED: total cycles / f |
| Frame rate | 7.09 FPS | 12.76 FPS | 19.48 FPS | DERIVED: f / total cycles |
| Effective throughput | 0.559 TOPS | 1.007 TOPS | 0.690 TOPS | DERIVED: 2 x real MACs x frame rate |
| Ideal cycles | 38,542,800 | 38,542,800 | 17,299,752 | DERIVED: real MACs / 1,024 |
| Frame-rate ceiling of the array | 20.76 FPS | 20.76 FPS | 46.24 FPS | DERIVED: f / ideal cycles |
| Efficiency against the ceiling | 34.13 % | 61.49 % | 42.13 % | DERIVED: ideal cycles / total cycles |
| Peak throughput of the array | 1.638 TOPS | 1.638 TOPS | 1.638 TOPS | DERIVED: 2 x 1,024 x f |

Core only (while the core is busy; the cycle-accurate part):

| Metric | YOLOv8m baseline | YOLOv8m run C | ViT-B/16 as drawn | Label |
|---|---:|---:|---:|---|
| Core busy cycles | 103,044,189 | 55,638,052 | 21,830,592 (GEMM_FUSED) | MEASURED |
| Core throughput on busy cycles | 612.83 GOPS | 1,134.99 GOPS | 1,245.93 GOPS (GEMM_FUSED) | DERIVED: 2 x real MACs x f / busy |
| PE utilization | 37.40 % | 69.27 % | 76.05 % (GEMM_FUSED); 69.26 % with the attention products | DERIVED: real MACs / (busy x 1,024) |
| Spatial usage | 53.97 % | 74.58 % | 87.93 % | DERIVED: real MACs / (contexts x K x 1,024), K = cin x kh x kw |
| Temporal efficiency in execute | 94.70 % | 95.25 % | 87.03 % | DERIVED: contexts x K / execute cycles |
| Execute share of busy | 73.18 % | 97.52 % | 99.37 % | MEASURED: execute / busy |
| Latency if only core busy time counted | 128.8 ms | 69.5 ms | 27.3 ms | DERIVED: lower bound, all other work hidden |
| Frame rate if only core busy time counted | 7.76 FPS | 14.38 FPS | 36.65 FPS | DERIVED: upper bound, not a result |

Reading the table:

- **Quote the end-to-end rows as the result.** The "core busy only" rows are bounds: they would be reached only if
  every DMA transfer, the epilogue and the vector unit were hidden behind the core.
- **PE utilization is the product of the three factors below it.** Spatial usage is lost when a core pass uses
  fewer than 32 output channels or fewer than 32 positions per context, or computes padding. Temporal efficiency is
  lost in the fill and drain of each context. The execute share is lost in stalls and context switches. From the
  baseline to run C the planner raised the spatial usage (53.97 to 74.58 %) and removed almost all stalls (execute
  share 73.18 to 97.52 %); the rules it follows are in `FRONTEND_GUIDE.md`, section 11.1. What remains is set by
  the hardware: the core behaviours and the SRAM and bus sizes listed in `HAS_VALIDATION.md`, section 4.5.
- **The YOLOv8m totals compare as** 112.9 M to 62.7 M cycles (-44.5 %), 7.09 to 12.76 FPS (+80.1 %), 612.83 to
  1,134.99 GOPS on core busy (+85.2 %).
- **ViT-B/16.** The core rows cover the GEMM_FUSED instructions; the attention products add 3,147,984 core cycles
  and 715,327,488 MACs (included in the end-to-end rows). The gap between 19.48 FPS and the core-only bound is the
  DMA wait and the estimated vector-unit time (section 5.6).

## 6. Estimated components

These parts of the model use estimated timing until the hardware micro-architecture specification provides
values.

| Component | Model value | Source |
|---|---|---|
| HAS OBP pipeline latency | 6 cycles (drawing pipeline cuts; section 5.3) or 10 cycles (derived from the stage counts of the vector-unit drawing; sections 5.4 to 5.6) | `has/gvu_obp.h`, `has/gvu_quant.h` |
| ELEM_WISE pipeline latencies | ADD stage 1: 6, ADD stage 2: 5, MAX: 3 cycles (section 5.3); 8, 10, 3 cycles (sections 5.4 to 5.6) | `has/gvu_elemwise.h`, `has/gvu_quant.h` |
| ELEM_WISE data movement | bytes / bus width + 20 cycles per transfer | `has/gvu_elemwise.h` |
| Data flow controller | 1 cycle per register write, 4 cycles decode | `has/has_npu_top.h` |
| Softmax and LayerNorm pipelines | 3L + 26 and 4H + 48 cycles per 32 rows | `has/gvu_softmax.h`, `has/has_npu_top.h` |
| Halo copy inside SRAM-A (run D) | 32 bytes per cycle | `has/has_npu_top.h` (`halo_copy_bpc`) |
| DRAM latency | 0 unless set | `has/has_dma.h` |
| MaxPool 5x5 traversal | direct 159,544 cycles, separable 80,008 cycles (SPPF shape) | `tools/has/tb_gvu_elemwise` |

## 7. Where the numbers come from

| Data | Location |
|---|---|
| Per-tile core counters | `metrics_tiles.csv` of each run |
| Per-layer and per-network rollup | `rollup/layers.csv`, `rollup/network.csv`, `rollup/report.md` |
| Regenerating the tables of sections 5.4 to 5.6 (`tb_has_npu_top` runs) | `python3 tools/metrics/has_rollup.py <run dir> [--cmp <run dir>]`; the reference runs are in `reference_runs/` of the data package |
| Regenerating the tables of section 5.7, with the per-layer table (array columns x rows used, tile shape, busy, execute, stall, PE utilization, spatial usage) and the controller state distribution | `bash tools/metrics/post_fullcore.sh --run $FE_WORK/reference_runs/<run> --net $FE_WORK/has/<program> --out <dir> --layers --fsm --no-detect` (`BUILD_AND_RUN.md`, section 7) |
| Regenerating the tables of section 4 (network testbench) | `bash tools/metrics/post_fullcore.sh --run <run dir> --out <dir> --net <net dir> [--layers]` |
| `HasNpuTop` window runs | `tools/has/tb_has_npu_top <insts dir> --first F --count N [--overlap] [--dma v45\|has] [--dram-lat N]` |
