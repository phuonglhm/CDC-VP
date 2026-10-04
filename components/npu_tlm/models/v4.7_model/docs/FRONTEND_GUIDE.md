# Frontend and Compiler Guide

The frontend (`tools/fe/`) turns a trained YOLOv8m model into three files that the SystemC testbenches
execute: a program (`prog.bin`), an initial DRAM image and a golden DRAM image. It also produces an integer
reference that is independent of the SystemC model. For the delivery top level it also writes the
memory-mapped instruction stream of YOLOv8m (section 11) and of ViT-B/16 (section 12). Commands are listed in
`BUILD_AND_RUN.md`, sections 3 and 6.

## 1. Pipeline

| Step | Script | Input | Output | Check |
|---|---|---|---|---|
| 1 | `fe_step1_export_check.py` | `yolov8m.pt` | ONNX with per-layer taps | PyTorch vs onnxruntime, every tap within a relative tolerance |
| 2 | `fe_step2_graph_check.py` (`fe_graph.py`) | ONNX | `graph_ir.json` | Every node mapped to an IR op; shapes consistent |
| 3 | `fe_step3_quant.py` (`fe_quant.py`) | IR, weights, calibration images | `params.npz`, `params.json` | Overflow bounds, multiplier error |
| 4 | `fe_step4_int8_check.py` (`fe_ref_int8.py`) | IR, parameters | Integer golden | Independent recomputation (section 5) |
| 5 | `fe_step5_quality.py` (`fe_eval.py`) | Golden, FP32 | Quality comparison | Detections and similarity vs FP32 |
| 6a | `fe_step6a_tile_check.py` (`fe_tile_plan.py`) | IR, parameters | `program.json` | Tiled execution equals the golden |
| 6b | `fe_step6b_export_net.py` | Program, parameters | `prog.bin`, DRAM images, `manifest.json` | Program is core-mapped |

Shared helpers live in `fe_common.py` (paths, model loading, letterbox preprocessing).

## 2. Graph to IR (`fe_graph.py`)

- Only the part of the graph that feeds the six raw head convolutions (three box and three class outputs) is
  kept. Detection decoding (distribution focal loss, anchors, NMS) runs on the host in floating point.
- Nodes whose inputs are all compile-time constants are folded with a small numpy evaluator.
- Every remaining node must match one IR op. An unmatched node is an error; nothing is aliased silently.

| IR op | ONNX pattern |
|---|---|
| `conv` | Conv, with Sigmoid + Mul fused as SiLU when the pattern is exact |
| `slice_ch` | Slice on the channel axis, constant bounds, step 1 |
| `concat` | Concat on the channel axis |
| `add` | Add of two data tensors of equal shape |
| `maxpool` | MaxPool with symmetric padding |
| `upsample_nearest` | Resize, nearest mode, integer scale on H and W |

Tensors are NCHW with N = 1. Each conv carries a `job` name derived from the module name (for example `stem`,
`dark5.conv`, `det.p3.cls1`). The same name identifies the layer in parameters, tiles, metrics and reports.

## 3. Quantization (`fe_quant.py`)

All activations are symmetric int8 with zero point 0: `real ~= q * s`.

- **Calibration.** The float IR runs on the 8 coco8 images. For each tensor (and each SiLU pre-activation) the
  maximum and the 99.99th percentile of `|x|` are recorded.
- **Scale groups.** Slice, concat, add, maxpool and upsample move data without rescaling, so all tensors
  connected through them must share one scale. They are grouped with union-find, and the group scale is taken
  over the whole group.
- **Weights.** Per output channel: `s_w = max|w| / 127`, `q_w = round(w / s_w)`.
- **Bias.** `q_b = round(b / (s_in * s_w))`, in partial-sum units.
- **Requantization.** The real multiplier `M = s_in * s_w / s_out` of each channel is encoded as
  `M ~= scale / 2^shift`, with `scale` a 32-bit unsigned integer and `shift <= 62` chosen to keep the most
  precision.

  ```
  q_out = sat8( ((psum + q_b) * scale) >> shift )
  ```
- **Rounding compensation** (`_rc` configurations). `2^(shift-1) / scale` is added to `q_b`, so the flooring
  shift approximates round-to-nearest.
- **SiLU.** A 256-entry table maps the int8 pre-activation to the int8 output: `lut[q_pre + 128]`.
- **Checks.** Bounds of the partial sum, biased sum and 64-bit product are verified for every layer.

Configuration names encode the choices. For `pc_p9999_rc`: `pc` = per-channel weights, `p9999` = 99.99th
percentile calibration, `rc` = rounding compensation. This is the configuration used for all published results.

## 4. Integer golden (`fe_ref_int8.py`)

`run_int8` executes the IR with the integer parameters:
- Convolution partial sums are computed with float64 matrix products on im2col columns. They are exact
  because every partial sum is an integer below 2^53.
- Requant and LUT follow the formula above. An optional `round_mode = 1` adds `2^(shift-1)` before the shift
  (used by the HAS configuration).
- Max-pool pads with -128.

The result is the expected byte value of every tensor. The golden DRAM image is built from it.

## 5. Independent checks (step 4)

- **Partial sums:** every convolution is compared with onnxruntime `ConvInteger` on the same int8 data (input
  pre-padded, so padding conventions cannot differ). The match is exact.
- **Requant and LUT:** recomputed with arbitrary-precision Python integers on 2,000 random elements per layer.
  The match is exact.

## 6. Tile planning (`fe_tile_plan.py`)

### 6.1 Residual fusion

An `add` whose one input is a SiLU conv output consumed only by that add is fused into the conv as the OBP
residual stage (`q_out = sat8(lut[q_pre + 128] + skip)`). The standalone add disappears from the program.

### 6.2 Tile constraints

A tile is a block of outputs `[c0:c1, oy0:oy1, ox0:ox1]` with all input channels.

| Resource | Constraint |
|---|---|
| Input window `Cin * A_H * A_W` | <= 79 KB (one ifmap buffer) |
| Weights `kh * kw * Cin * Cout_t` | <= 81 KB (one weight buffer) |
| Partial sums `H_t * W_t * Cout_t` | <= 24,576 (96 KB of int32) |
| Output channels `Cout_t` | <= 32 (OBP lanes) |
| Skip values (fused residual) | <= 24 KB (scratch) |

Geometry follows the core mapping:
- `Cout_t` and `W_t` are chosen among divisors of 32 (or multiples of 32 for wide rows).
- `X_used = Cout_t` and `Y_used = W_t` for full tiles. Remainder tiles use the largest divisor of 32 that fits.

The input window of each tile includes the padding, which may reach outside the tensor:

```
iy0 = oy0 * stride - pad_top      iy1 = (oy1 - 1) * stride - pad_top + kh
```

### 6.3 Objective

- **With a cost model** (`tools/eval_sw/tile_model.py` and `tools/eval_sw/out/prep/shape_table.csv`):
  candidates are ranked by predicted cycles over the whole layer.
- **Without it:** candidates are ranked by minimum tile count, then minimum context count.

A per-layer override (`FE_TILE_HT_OVERRIDE`) limits the tile height of specific layers. Two head layers use it
by default.

The published plan has 117 steps (83 convolutions, 16 slice, 13 concat, 3 max-pool, 2 upsample) and 9,811
tiles.

### 6.4 Plan check (step 6a)

`run_program` executes the tiled program in numpy, tile by tile, and compares every tensor with the golden.
It also checks every tile against the bank limits.

## 7. Export (step 6b)

The exporter:
1. Recomputes the golden for the chosen image.
2. Lays out DRAM: four staging areas (input window, skip, preload, output), all tensors, then per-conv
   parameters (weights `[Cout, Cin, kh, kw]`, LUT, scale, shift, bias).
3. Writes the initial image (input and parameters only), the golden image (every tensor filled), the program
   (`"FENP"` format) and a manifest of addresses.

The exact formats are in `INTERFACE_SPEC.md`.

## 8. HAS variant

| Script | Purpose |
|---|---|
| `fe_make_has_config.py` | Derives HAS requant parameters from `pc_p9999`: signed 32-bit scale `S' = ceil(S/2)`, `shift' = shift - 1`, HALF_UP rounding, zero point 0 |
| `fe_has_program.py` | Lowers the program: a conv with a fused residual becomes conv + `elem_add`; host max-pool becomes `elem_max`. Conv tiles are unchanged (their checksum is verified) |
| `fe_ref_has.py` | Independent Python golden of the HAS arithmetic |
| `fe_has_export_net.py` | Writes the `"FEHP"` program (option header plus element-wise steps) and DRAM images |
| `fe_has_vectors.py`, `fe_ref_has_rce.py` | Vector files for the HAS unit testbenches |

In compatibility mode, the HAS golden is verified byte-equal to the standard golden before anything is written.

## 9. Detection decoding (`fe_eval.py`, `fe_sysc_detect.py`)

`fe_sysc_detect.py` reads the six head tensors from a DRAM image (golden or a testbench snapshot) at the
addresses in the manifest, dequantizes them with the configuration scales, and calls:
- `fe_eval.decode`: distribution focal loss with 16 bins, anchors, strides 8, 16, 32;
- `fe_eval.detections`: confidence 0.25, IoU 0.7, at most 300 boxes.

These are the Ultralytics prediction defaults. The decode is validated against the model's own final output in
step 5.

## 10. Reproducibility notes

- All generated data goes under `$FE_WORK`; no script writes anywhere else. When `$FE_WORK` is the extracted data
  package inside the repository (`sauria_npu_data/`), it is listed in `.gitignore`.
- Rounding of weights, biases, scales and LUT entries is fixed (round half away from zero), so parameters are
  reproducible bit for bit.
- The tile plan depends on the cost table (section 6.3). Keep it with the package to reproduce the published
  plan.
- Calibration uses the same 8 images that the quality step evaluates. The quality numbers compare
  configurations; they are not an accuracy measurement.

## 11. Instruction stream backend (`fe_emit_insts.py`)

`fe_emit_insts.py` is the reference compiler backend for `HasNpuTop`. It turns the HAS program of YOLOv8m into
the register-write stream of the v4.5 protocol (one instruction per layer) and a new DRAM layout. File formats:
`INTERFACE_SPEC.md`, section 6.5; registers: `SW_INTEGRATION_GUIDE.md`, section 3.

- **DRAM layout.** Tensors are `[C][H][W]` int8. Slice and concat become address aliases; a concat whose inputs
  cannot all be placed stays a host copy (`H copy ...`) and is listed in the manifest (none for YOLOv8m). Weights
  are stored per output-channel band in SAURIA order (`SW_INTEGRATION_GUIDE.md`, sections 5 and 6.1). LUT, scale,
  shift and bias keep their values.
- **Host steps.** The two x2 upsamples stay host steps (`H upsample2x ...`, question H15).
- **Self-check.** Every tensor is compared by name with the HAS golden before the files are written.

`--plan` selects the tiling written into the instructions. The tensor values never change, only the tiles.

| Plan | Output directory | Tiling |
|---|---|---|
| `has` | `$FE_WORK/has/insts_has` | The tile plan of section 6 (9,811 tiles) |
| `pe` | `$FE_WORK/has/insts_pe` | Tiling for PE utilization from a measured per-layer table: input-channel split (`TILE_CIN`) on selected layers, 1x1 layers flattened to a single row, stem with 32 output channels |
| `pe2` | `$FE_WORK/has/insts_pe2` | `pe` plus padded edge tiles (`FLAGS.PAD_TAIL`), larger tiles, and 31 positions per context (`Y_USED`) for 3x3 stride-2 convolutions |
| `pe3` (default) | `$FE_WORK/has/insts_pe3` | `pe2` without the three layers where measurement showed the padded tiles slower (`stem`, `det.p3.cls_out`, `det.p4.cls_out`). Reference plan (run C) |
| `pe4` | `$FE_WORK/has/insts_pe4` | `pe3` re-planned per layer on the total core and DMA cost; `--halo` and `--c-bcast` cost the DMA as the proposals profile does. Also writes `pe4_decisions.csv`. Plan of run D |

The runs of each plan are in `PERFORMANCE_REPORT.md`, sections 5.3 to 5.5 (`pe` = run A, `pe2` = run B).

Inputs besides the frontend outputs: the `pe` plans read the measured per-layer table
`$FE_WORK/has/insts_pe/measured_layers.csv` (option `--pe-csv`); `pe4` also reads the measured per-layer tables
of the `pe` and `pe2` runs under `$FE_WORK/has/pe4_measured/`. These tables come from whole-network runs; keep
them to reproduce the published plans.

With a `pe` plan the script also writes `$FE_WORK/has/net_<plan>/prog.bin` (the `FEHP` program with the new
tiles), the reference for the replay check `tools/has/tb_has_mmio_replay`.

### 11.1 Tiling rules for PE utilization

The `pe` plans apply the rules below. They were found by experiment on YOLOv8m (32x32 array, int8, SRAM A/B/C of
158/162/192 KB, scratchpad 48 KB) and raise the network PE utilization from 37.40 % (plan `has`) to 69.27 % (plan
`pe3`). Use them when mapping another network: read the per-layer table of a run
(`bash tools/metrics/post_fullcore.sh --layers`, `BUILD_AND_RUN.md`, section 7), find the sign in the second column,
apply the rule. "Columns x rows" is the `X x Y used` column of that table: output channels by positions per context.
Examples are measured, plan `has` to plan `pe3` (`PERFORMANCE_REPORT.md`, sections 5.3 and 5.4).

| # | Rule | Sign in the per-layer table | What to do | Cost | Measured example |
|---|---|---|---|---|---|
| 1 | Always use all 32 columns | 16 or 8 columns used, spatial usage at or below 31 %, execute / busy about 50 % | When the weights of 32 output channels do not fit SRAM-B (K = cin x kh x kw above 2,592), split the input channels into parts (`TILE_CIN`); the partial sums accumulate in SRAM-C. When the tile was merely chosen small, raise the output channels per tile to 32 | More core passes than tiles | `dark5.conv`: 16x16 to 32x31, PE utilization 7.69 to 57.82 % |
| 2 | 1x1 convolution: lay the map out as one row | A 1x1 layer with fewer than 32 rows used because the output map is small (20x20) | Treat H x W as 1 x (H x W); only the shape written in the instruction changes | None | `sppf.cv2` (tile 64 x 1): 32x16 and 32x4 to 32x32, PE utilization 15.20 to 85.03 % |
| 3 | 3x3 stride 2: 31 rows, not 32 | A stride-2 layer at 32x32 whose execute / busy is only about 76 % | `Y_USED` = 31 | One row of 32 unused | `dark2.conv`: execute / busy 75.95 to 99.78 %, PE utilization 72.06 to 79.06 % |
| 4 | Edge tiles: compute the full size | Two shapes in `X x Y used`, for example 32x32 and 16x32 (a short edge tile) | Pad the last tile with zeros to the full width (`FLAGS.PAD_TAIL`), so that rule 1 holds on the edge as well | The padded MACs are executed and lost; on three layers the padded tiles were slower and are left unpadded in `pe3` | `dark2.c2f.b0.1`: execute / busy 67.16 to 99.88 %, PE utilization 49.16 to 72.93 % |
| 5 | Fewer tiles: the largest tile that fits the SRAMs | Instruction cycles far above core busy cycles (many small tiles) | Choose the largest tile that still fits SRAM A, B and C | A very large tile is harder to overlap with the DMA | `det.p3.box_out`: 120 to 18 tiles, instruction cycles -41.6 % |
| 6 | Know when to stop | 32x32, execute / busy about 99 %, PE utilization about 80 % or more; or a very small K | Leave the layer: every further change has a cost. With a very small K the core spends a fixed minimum of about 230 cycles per context, so the ceiling is about K / 230 and only a hardware change raises it | None | `det.p3.cls1` stays at 82.00 %; `stem` (K = 27) uses 32 columns and 32 rows on most tiles but reaches 8.84 % |

Plan `pe4` applies the same rules but decides per layer on the total of core and DMA cycles instead of the PE
utilization alone. The limits, the cycle laws behind the rules, a step-by-step procedure and worked examples are in
`TILING_GUIDE.md`.

## 12. ViT-B/16

| Script | Purpose |
|---|---|
| `fe_vit_float.py` | Float reference in numpy from the timm weights (`$FE_WORK/weights/`), checked against an independent PyTorch forward and on labelled images |
| `fe_vit_quant.py` | Int8 quantization laid out on the vector-unit pipelines (per-channel weights, per-tensor activations), calibrated on coco128; writes `$FE_WORK/has/vit/vit_scales.json` |
| `fe_ref_gvu.py` | Independent integer golden of the vector unit: indirect LUT, SOFTMAX, FUSED_ATTN, LAYERNORM (`has/HAS_IFACE_RCE.md`) |
| `fe_vit_block.py` | Integer golden of one encoder block in the form the data flow controller runs it, and the instruction emitter |
| `fe_vit_full.py` | Whole network: patch embedding, 12 encoder blocks, final LayerNorm and head, 244 instructions, no host step. Output `$FE_WORK/has/vit_full/` |

- **Layout.** Every activation is channel-major `[C][tokens]` with 197 tokens; FUSED_ATTN and LAYERNORM set
  `FLAGS.CHANNEL_MAJOR`.
- **Patch embedding.** The 16x16 stride-16 convolution is a GEMM_FUSED over the patch matrix (a layout of the
  input image); column 0 is the class-token slot, and an ELEM_WISE ADD adds the position embedding.
- **Encoder block** (20 instructions): LAYERNORM, QKV GEMM_FUSED, 12 FUSED_ATTN (one per head), output
  projection, residual ADD, LAYERNORM, first MLP layer with the GELU table (activation type 3), second MLP layer,
  residual ADD.
- **Head.** LAYERNORM over all tokens and a GEMM_FUSED to 1,000 classes (output channels padded to 1,024 with
  `PAD_TAIL`); the logits are token 0.
- **Golden.** Block N consumes the integer output of block N-1. The quality against the float model is reported
  per block (cosine) and on the logits (top-1); see `KNOWN_LIMITATIONS.md`, Q4.

