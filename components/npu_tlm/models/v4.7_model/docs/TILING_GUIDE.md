# Tiling Guide

How a layer is cut into tiles for the NPU: the terms, the hard limits with their formulas, what the compiler writes
and what the controller derives, the cycle laws that decide which tiling is fast, a step-by-step procedure, worked
examples checked against the measured runs, and the files to read. It complements `FRONTEND_GUIDE.md`, sections 6
and 11 (the planner as implemented) and `SW_INTEGRATION_GUIDE.md`, sections 3 to 6 (the registers).

Labels as in `VERIFICATION_REPORT.md`: **MEASURED**, **DERIVED**, **ESTIMATED**. Configuration: 32x32 array, int8,
SRAM A/B/C of 2 x 79 / 2 x 81 / 2 x 96 KB (`HARDWARE_CONFIG.md`, section 1).

## 1. Terms

| Term | Meaning |
|---|---|
| Layer | One GEMM_FUSED instruction: a convolution or a matrix product (a 1x1 convolution over positions) |
| Tile | A block of the layer's output, `[c0, c1) x [oy0, oy1) x [ox0, ox1)`, that fits the SRAMs. Size `TILE_COUT x TILE_H x TILE_W`; edge tiles are clipped |
| `K` | Multiply-accumulates per output element: `Cin x kh x kw`. With an input-channel split, `Kt = TILE_CIN x kh x kw` per pass |
| `X_used` | Array columns in use = output channels of the tile, at most 32 |
| `Y_used` | Array rows in use = output positions computed per context, at most 32 |
| Context | One load of the array: `Y_used` positions of one output row, `X_used` channels, `K` steps |
| Core pass | One run of the core on a tile. Without an input-channel split a tile is one pass; with a split it is `Cin / TILE_CIN` passes |
| Real MACs | `Cout x OH x OW x Cin x kh x kw` of the layer, independent of the tiling |

One tile of `h` rows and `w` positions per row has

```
contexts per pass  n_ctx = h x ceil(w / Y_used)
passes             P     = Cin / TILE_CIN            (1 without a split)
tiles of a layer   T     = ceil(Cout / TILE_COUT) x ceil(OH / TILE_H) x ceil(OW / TILE_W)
```

## 2. Who decides what

| Decided by | Item | Where |
|---|---|---|
| Compiler | `TILE_COUT`, `TILE_H`, `TILE_W`; optionally `TILE_CIN`, `Y_USED`, `FLAGS.PAD_TAIL`; for a 1x1 layer the shape written in the instruction (section 6, rule 2) | Extension registers, `SW_INTEGRATION_GUIDE.md`, section 3.3 |
| Compiler | Weight layout per output-channel band and per input-channel part; zero padding of the last band with `PAD_TAIL` | `SW_INTEGRATION_GUIDE.md`, sections 5 and 6.1 |
| Controller (hardware) | The walk over the tiles, the input window of each tile, `X_used`, `Y_used`, the passes of a split, the tile order (output-channel-outer or spatial-outer, whichever moves fewer DMA bytes) | `has/has_tile_iter.h`, `has/has_npu_top.h` |

The controller derives, for the tile at output region `[c0, c1) x [oy0, oy1) x [ox0, ox1)`:

```
input window   iy = [oy0 x s - pad, (oy1 - 1) x s - pad + kh)       ix likewise      (may reach into the padding: zero)
X_used         = c1 - c0
Y_used         = Y_USED                    if Y_USED is written, <= 32, and divides the tile width
               = gcd(tile width, 32)       otherwise
with PAD_TAIL  : c1 = c0 + TILE_COUT, X_used = TILE_COUT; y = Y_USED (or 32); the width is rounded up to a multiple
                 of y and Y_used = y; only real channels and positions are written back
```

## 3. Hard limits

Every tile must satisfy all of these. The right column is the place where the rule is implemented or checked.

| Resource | Rule | Value | Source |
|---|---|---|---|
| SRAM-A, input window | `Cin_t x A_H x A_W <= 80,896` with `A_H = (h - 1) x s + kh`, `A_W = (w - 1) x s + kw`, `Cin_t = TILE_CIN` or `Cin`; `w` is the computed width (after `PAD_TAIL` rounding) | 79 KB per buffer = 2,528 rows of 32 bytes | `tools/fe/fe_emit_insts.py` (`fits`), `tools/fe/fe_tile_plan.py` |
| SRAM-B, weights | `X_used x Kt <= 82,944` | 81 KB per buffer = 2,592 rows of 32 bytes | same |
| SRAM-C, partial sums | `X_used x h x w <= 24,576` | 96 KB per buffer = 768 rows of 128 bytes = 24,576 int32 | same |
| Output channels per tile | `TILE_COUT <= 32` | 32 array columns and 32 epilogue lanes | `fe_tile_plan.py` (`OBP_LANES`) |
| Positions per context | `1 <= Y_USED <= 32`; larger: error `BAD_GEOM` | 32 array rows | `has/has_mmio_compat.h` |
| Input-channel split | `TILE_CIN` divides `Cin`; otherwise error `BAD_GEOM` | | `has/has_mmio_compat.h` |
| Tile sizes | `TILE_COUT`, `TILE_H`, `TILE_W` all positive; otherwise error `NO_TILING` | | `has/has_mmio_compat.h` |
| Skip tensor of a fused residual (network testbench with the native OBP only) | `X_used x h x w <= 24,576 bytes` | 24 KB scratch | `fe_tile_plan.py` (`BANK_SKIP_BYTES`); not used by `HasNpuTop`, where a residual is a separate ELEM_WISE ADD |

Consequences worth remembering (DERIVED):

| Question | Formula | Examples |
|---|---|---|
| Largest `K` that still allows 32 output channels in one pass | `K <= 82,944 / 32 = 2,592` | 3x3: `Cin <= 288`; 1x1: `Cin <= 2,592` |
| Input-channel split needed | when `K > 2,592`: choose the largest `TILE_CIN` that divides `Cin` with `32 x TILE_CIN x kh x kw <= 82,944` | 3x3, `Cin` 384: `TILE_CIN` 192, 2 passes. 1x1, `Cin` 3,072 (ViT-B/16 second MLP layer): 768, 4 passes |
| Widest tile of a 1x1 layer laid out as one row | `w <= min(768, floor(80,896 / Cin))`, rounded down to a multiple of 32 | `Cin` 64: 768. `Cin` 192: 416. `Cin` 1,152: 64 |
| Why 768 | SRAM-C with `X_used` 32 and one row: `24,576 / 32` | |
| Tallest tile of a KxK layer for a chosen `w` | `h <= min( ((80,896 / (Cin_t x A_W)) - kh) / s + 1 , 24,576 / (X_used x w) )` | 3x3 stride 2, `Cin_t` 192, `w` 31: `A_W` 63, `h <= 2` |

The model does not enforce the SRAM capacities at run time: an oversize tile wraps addresses silently unless the
build option `FX1_SRAM_CAP_CHECK` is on (`KNOWN_LIMITATIONS.md`, O5 and O6). The compiler must guarantee the three
SRAM rules. A tile that fills SRAM-B or SRAM-C exactly is legal and passes, with one unused access at the address
equal to the capacity (question H14).

## 4. Cycle laws of the core

MEASURED on the cycle-accurate core and reproduced on the original SAURIA RTL (`HAS_VALIDATION.md`, section 4.5):

```
cycles of one core pass  ~=  n_ctx x max(Kt, 230) + 281
```

| Case | Effect on the pass | Model used by the planner |
|---|---|---|
| `X_used < 32` | 1.82 to 1.97 times slower (the feeders stall about half of the time) | x 2 |
| 1x1 layer with `Y_used < 32` | 1.82 to 1.88 times slower | x 2 |
| 3x3 stride 2 with `Y_used = 32` | each context costs 1.27 to 1.33 K | `1.32 x Kt` per context |
| 3x3 stride 2 with `Y_used <= 31`; KxK stride 1 at any `Y_used` with `X_used` 32 | no penalty | |
| `Kt < 230` | every context still costs about 230 cycles | `max(Kt, 230)` |
| Every tile | about 281 cycles of start and drain, plus a reset and about 45 to 50 register writes outside the core | `+ 281` |

The same law is `core_cost` in `tools/fe/fe_emit_insts.py` and `core_pass_est` in `has/has_npu_top.h`. The older
planner (`fe_tile_plan.py`) uses the equivalent per-shape model of `tools/eval_sw/tile_model.py`, calibrated on 149
measured tile shapes (`tools/eval_sw/out/prep/shape_table.csv`): execute `= n_ctx x max(K, 102) + 267`, busy
`= execute + n_ctx x stall + 10`, with the stall per context equal to `max(K, 170)` in the two slow cases above and
`max(0, 225 - K)` at 32 x 32.

### 4.1 PE utilization of a tiling

```
PE utilization  = real MACs / (core busy cycles x 1,024)
                = spatial usage x temporal efficiency x execute share        (PERFORMANCE_REPORT.md, section 5.7)
```

For one layer the law above gives (DERIVED):

```
spatial usage   = (X_used / 32) x (Y_used / 32) x (real channels / computed channels) x (real positions / computed positions)
PE utilization ~= spatial usage x  n_ctx x Kt / ( penalty x n_ctx x max(Kt, 230) + 281 )
```

| Ceiling | Value | Reason |
|---|---|---|
| `X_used = Y_used = 32`, no padding, large `K` | `n_ctx x K / (n_ctx x K + 281)`, for example 98.4 % at `K` 1,728 and 10 contexts | Only the per-tile cost remains |
| `Y_USED = 31` | at most 31 / 32 = 96.9 % | One array row unused |
| Map width not a multiple of the positions per context | width 80: 80 / 96 = 83.3 %. Width 40 (tiles of 32 and 8 positions) or width 20 computed as 32 with `PAD_TAIL`: 62.5 %. Width 160 with `w` 93 and `Y_USED` 31: 160 / 186 x 31 / 32 = 83.3 % | A context cannot wrap over a row boundary (question H30) |
| Small `K` | `K / 230`: 11.7 % for `K` 27 | Per-context floor (question H31) |

## 5. Cost outside the core

The total of an instruction is not the core alone: DMA that the ping-pong schedule cannot hide, and the per-tile
programming, add to it. `fe_emit_insts.py` uses a simple model for plan `pe4` (ESTIMATED, fitted on measured
layers):

```
DMA bytes of a layer   = min( ifmap x G + weights + output ,  ifmap + weights x T + output )
                         ifmap   = sum over spatial tiles of Cin x (input window clipped to the image)
                         weights = Cout x Cin x kh x kw        output = Cout x OH x OW
                         G = output-channel bands, T = spatial tiles   (first term: channel-outer order, second: spatial-outer)
total cycles          ~= max( 1.022 x core , 1.22 x bytes / 16 )      and x 1.26 when 0.6 < DMA / core < 1.0
```

The bias preload (one int32 per output element on DMA channel 3, question H17) is not in this byte count; it is why
the bias-broadcast proposal saves 2.29 M cycles on YOLOv8m (`PERFORMANCE_REPORT.md`, section 5.5).

Two practical consequences (MEASURED): padded edge tiles made three layers slower in total although their core time
fell, because those layers are limited by the DMA (plan `pe3` leaves them unpadded); and beyond about 70 % PE
utilization further core gains show up only partly in the frame rate. Judge a tiling on total cycles.

## 6. Rules

The six rules, with the sign to look for and a measured example each, are in `FRONTEND_GUIDE.md`, section 11.1.
In terms of the formulas above:

| # | Rule | Formula behind it |
|---|---|---|
| 1 | `TILE_COUT = 32`; split the input channels when `K > 2,592` | Avoids the x 2 of `X_used < 32` (section 4) at the cost of `Cin / TILE_CIN` passes (section 3) |
| 2 | 1x1, stride 1, no padding: write the layer as one row, `IN_H = OUT_H = 1`, `IN_W = OUT_W = H x W`, `TILE_H = 1`, `TILE_W` a multiple of 32 up to `min(768, floor(80,896 / Cin))` | Makes `Y_used = 32` on every full tile; `[C][H][W]` is contiguous, so no data moves |
| 3 | 3x3 stride 2: `Y_USED = 31`, `TILE_W` a multiple of 31 | Removes the `1.32 x K` context |
| 4 | `FLAGS.PAD_TAIL` when the edge tile would have `X_used < 32` or a short `Y_used` | Trades padded MACs for the x 2; compare total cycles, not core cycles |
| 5 | The largest `TILE_H` (and `TILE_W`) that satisfy section 3 | Fewer tiles: fewer `+ 281` and fewer per-tile register sequences |
| 6 | Stop at 32 x 32 with an execute share near 99 %; accept `K / 230` for small `K` | Section 4.1 ceilings |

## 7. Procedure for a layer

1. Compute `K = Cin x kh x kw` and the output size.
2. Output channels: `TILE_COUT = 32`. If `Cout` is not a multiple of 32, either accept one band with fewer channels
   (it runs at half speed) or set `PAD_TAIL` and zero-pad the weights of the last band to 32 channels.
3. Weights: if `32 x K > 82,944`, set `TILE_CIN` (section 3) and store the weight parts as in
   `SW_INTEGRATION_GUIDE.md`, section 6.1. `Kt = TILE_CIN x kh x kw` replaces `K` below.
4. Width:
   - 1x1, stride 1, no padding: rule 2.
   - 3x3 stride 2: `Y_USED = 31`, `TILE_W` = 31, 62, 93, ... not above the SRAM limits; usually with `PAD_TAIL`.
   - otherwise: `TILE_W` = 32 or a multiple of 32 up to the map width; for a map narrower than 32, the map width
     (then `Y_used = gcd(width, 32)`), or `PAD_TAIL` to round it up to 32.
5. Height: the largest `TILE_H` that satisfies the SRAM-A and SRAM-C rules for the computed width.
6. Check the three SRAM rules on the largest tile (the padded one when `PAD_TAIL` is set).
7. Predict: tiles `T`, passes `P`, contexts, core cycles by the law of section 4, PE utilization by section 4.1.
   Compare alternatives on total cycles (section 5) when the layer moves much data per MAC (first layers, 1x1
   layers on large maps, output layers).
8. Run the layer and read its row in the per-layer table (`BUILD_AND_RUN.md`, section 7): `X x Y used` must be
   32x32 (or 32x31), execute / busy near 99 %, and the busy cycles close to the prediction.

For the reference frontend, steps 2 to 5 are written per layer in a table, `$FE_WORK/has/insts_pe/measured_layers.csv`,
whose column `tiler_recommendation` the script parses (other columns are informative). Three forms exist:

```
layer,...,tiler_recommendation
sppf.cv2,...,"flatten HxW -> 1x400 row, w_t 64 h_t 1, cout_t 32"                 rule 2 (the row length must equal H x W)
dark5.conv,...,"cout_t 32, Cin split 2 (cin_t 192), h_t 2 w_t 16 unchanged"      rule 1 (h_t, w_t = those of plan `has`)
stem,...,"cout_t 32(+16), h_t 4, w_t 160 (SRAM-C 20480)"                          first layer, fixed in the script
```

A layer without a row keeps the tiling of plan `has`. Plans `pe2` to `pe4` then search `TILE_H`, `TILE_W`,
`Y_USED` (0 or 31 for 3x3 stride 2) and `PAD_TAIL` for the 3x3 layers and adopt a change only when it is at least
1 % better (`opt_plan`).

## 8. Worked examples

All from YOLOv8m; "measured" is the per-layer table of the reference run C and of the baseline run
(`reference_runs/yolov8m_run_c`, `reference_runs/yolov8m_baseline` of the data package).

### 8.1 `dark5.conv`: 3x3 stride 2, `Cin` 384, `Cout` 576, output 20x20

`K = 384 x 9 = 3,456 > 2,592`, so 32 output channels do not fit SRAM-B in one pass.

| | Baseline (plan `has`) | Reference (plan `pe3`) |
|---|---|---|
| Choice | `TILE_COUT` 16 (largest power of two with `16 x 3,456 <= 82,944`), `TILE_W` 16, `TILE_H` 2 | `TILE_COUT` 32, `TILE_CIN` 192 (`Kt` 1,728, `32 x 1,728 = 55,296`), `Y_USED` 31, `TILE_W` 31 with `PAD_TAIL` (20 positions computed as 31), `TILE_H` 2 |
| SRAM-A | `384 x 5 x 33 = 63,360` | `192 x 5 x 63 = 60,480` (`TILE_H` 3 would need `192 x 7 x 63 = 84,672`: too large) |
| SRAM-C | `16 x 2 x 16 = 512` | `32 x 2 x 31 = 1,984` |
| Tiles / passes | `36 x 10 x 2 = 720` / 720 | `18 x 10 x 1 = 180` / 360 |
| Contexts per pass | 2 | `2 x 31 / 31 = 2` |
| Predicted core cycles | `720 x 2 x (2 x 3,456 + 281) = 10,357,920` (x 2 for 16 columns) | `360 x (2 x 1,728 + 281) = 1,345,320` |
| Measured core busy | 10,118,160 | 1,344,960 |
| PE utilization (real MACs 796,262,400) | 7.69 % | 57.82 % = spatial 62.5 % (20 / 31 x 31 / 32) x 92.5 % |

### 8.2 `sppf.cv2`: 1x1, `Cin` 1,152, `Cout` 576, output 20x20

Written as one row of 400 positions. `TILE_W = min(768, floor(80,896 / 1,152) = 70)` rounded down to 64;
`32 x 1,152 = 36,864` fits SRAM-B; SRAM-C `32 x 64 = 2,048`. `PAD_TAIL` computes the last tile (16 positions) as 32.

| | Value |
|---|---|
| Tiles | `18 x ceil(400 / 64) = 126` |
| Predicted core cycles | per band: six tiles of 2 contexts and one of 1: `6 x (2 x 1,152 + 281) + (1,152 + 281) = 16,943`; `x 18 = 304,974` |
| Measured core busy | 304,848 (PE utilization 85.03 %; baseline 15.20 % with 1,705,140 cycles) |

### 8.3 `dark2.c2f.b0.1`: 3x3 stride 1, `Cin` 48, `Cout` 48, output 160x160

`K = 432`. `Cout` 48 is not a multiple of 32: with `PAD_TAIL` both bands are computed with 32 channels (the second
one has 16 real channels and 16 zero-weight channels). `TILE_W` 32, `TILE_H` 23: SRAM-C `32 x 23 x 32 = 23,552`
(24 rows, 24,576, also fit and cost the same; the search keeps the first), SRAM-A `48 x 25 x 34 = 40,800`.

| | Value |
|---|---|
| Tiles | `2 x 7 x 5 = 70` (six rows of height 23 and one of height 22) |
| Predicted core cycles | `10 x (6 x (23 x 432 + 281) + (22 x 432 + 281)) = 710,870` |
| Measured core busy | 710,800 (PE utilization 72.93 % = 48 / 64 real channels x 97 %; baseline 49.16 %) |

### 8.4 `stem`: 3x3 stride 2, `Cin` 3, `Cout` 48, output 320x320

`K = 27`. With `TILE_COUT` 32, `TILE_H` 4, `TILE_W` 160 the array is full on most tiles (spatial usage 75 %: the
second band has 16 of 32 channels), but each context costs about 230 cycles for 27 useful steps:
`75 % x 27 / 230 = 8.8 %`; measured 8.84 %. No tiling raises it further (question H31). Padding the short band was
measured slower in total (the layer is limited by its input DMA) and is not used.

## 9. Plans of the reference frontend

| Plan | Rules applied | Total cycles, YOLOv8m | Reproducible from the package |
|---|---|---:|---|
| `has` | SRAM limits, objective = predicted cycles of `tile_model.py` (minimum tile count without the cost table) | 112,912,950 | Yes: regenerated byte-identical |
| `pe` | 1, 2, first layer with 32 channels | 73,128,197 (run A, measured before the weight-block reuse of the controller) | Needs `measured_layers.csv`; regeneration not rechecked for this release |
| `pe2` | `pe` + 3, 4, 5 on every 3x3 layer | 64,109,163 (run B, same remark) | Same as `pe` |
| `pe3` (default) | `pe2`, with rule 4 off on three layers measured slower in total | 62,681,721 | Yes: regenerated byte-identical |
| `pe4` | Per layer on total cycles (core + DMA) | 60,386,685 with the two hardware proposals | The shipped program was chosen with a calibrated DMA model that is not part of the package; the script falls back to the simple model of section 5 and to the measured tables of `$FE_WORK/has/pe4_measured/`, and may decide differently on some layers |

## 10. Reference files

| File | Content |
|---|---|
| `tools/fe/fe_tile_plan.py` | First planner (plan `has`): SRAM limits, candidate search, cost-model objective, numpy execution of a tiled program |
| `tools/eval_sw/tile_model.py`, `tools/eval_sw/out/prep/shape_table.csv` | Per-shape cycle model and its 149 measured shapes |
| `tools/fe/fe_emit_insts.py` | Plans `pe` to `pe4`: `pe_plan` (table parser, rules 1 and 2), `opt_plan` (rules 3 to 5), `tiles_of` (the controller's tile walk in Python), `core_cost`, `dma_bytes`, `total_cost`, `fits`, weight layout with a split |
| `has/has_tile_iter.h` | The controller's tile walk: window, `X_used`, `Y_used`, `PAD_TAIL`, tile orders |
| `has/has_mmio_compat.h` | Decoding of the tiling registers and their error codes; default tiling of a matrix product given only as M, K, N |
| `has/has_npu_top.h` | Passes of an input-channel split, weight-block reuse, choice of the tile order, `core_pass_est` |
| `tools/has/tb_has_tile_iter.cpp`, `tools/has/tb_has_mmio_replay.cpp` | Checks of the tile walk against an exported program |
| `tools/metrics/post_fullcore.sh`, `tools/metrics/has_rollup.py` | Per-layer tables of a run |
| `$FE_WORK/has/insts_pe/measured_layers.csv`, `$FE_WORK/has/insts_pe4/pe4_decisions.csv` | Per-layer tiling table; per-layer decisions of plan `pe4` with core and DMA cycles |
| `docs/FRONTEND_GUIDE.md`, sections 6 and 11; `docs/ARCHITECTURE.md`, sections 3.2 and 8; `docs/SW_INTEGRATION_GUIDE.md`, sections 3.3, 4.1 and 6.1; `docs/PERFORMANCE_REPORT.md`, sections 3, 5.4 and 5.7; `docs/HAS_VALIDATION.md`, section 4.5 | Planner, mapping, registers and weight layout, measured results, hardware questions |

## 11. Open points that affect tiling

| Point | Effect on the compiler |
|---|---|
| Stall with fewer than 32 columns or rows (H28) and the stride-2 context (H29) | Rules 1 to 4 exist because of them; a hardware fix would relax them |
| One-dimensional position counter (H30), per-context floor (H31), per-tile cost (H32) | Set the ceilings of section 4.1 |
| SRAM-B size (H33) | Decides which layers need `TILE_CIN` |
| Access at address = capacity on a full buffer (H14) | A margin of one row would avoid it; not applied today |
| Weight tiles larger than one buffer (`KNOWN_LIMITATIONS.md`, O3) | Disabled: every tile uses one weight buffer |
| The first planner limits two 1x1 output layers to `TILE_H` 4 (`KNOWN_LIMITATIONS.md`, O1) | Applies to plan `has`; the `pe` plans lay these layers out as one row |
