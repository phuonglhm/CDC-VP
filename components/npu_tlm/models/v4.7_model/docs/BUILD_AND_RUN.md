# Build and Run Guide

All commands are run from the repository root unless stated otherwise.

## 1. Environment

| Component | Version used for the published results |
|---|---|
| OS | Ubuntu 22.04 LTS, x86-64 |
| C++ compiler | g++ 11.4 (C++17 required) |
| SystemC | 2.3.3 (Accellera), system package or source build |
| Python | 3.10 |
| numpy | 2.2 |
| onnx | 1.22 |
| onnxruntime | 1.23 |
| torch | 2.12 (CPU build is sufficient) |
| ultralytics | 8.4 |
| Pillow | 9.0 (only for drawing detection boxes) |

Hardware: 4 CPU cores and 16 GB RAM are enough. Allow 5 GB of free disk for one exported network, logs and
DRAM snapshots.

```bash
export SYSTEMC_HOME=/usr                                   # directory containing include/systemc.h
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH
bash tools/unpack_data.sh                                  # once: data/ parts -> ./sauria_npu_data (checked)
export FE_WORK=$PWD/sauria_npu_data                        # the data package; generated data goes here as well
```

`FE_WORK` must always be set: the scripts read their inputs from it and write generated data only under it. The
data package is carried in `data/` as a split archive (`data/README.md`); `tools/unpack_data.sh [directory]` joins
the parts, checks the sha256 of the archive and of every extracted file, and extracts `sauria_npu_data/` into the
repository root (listed in `.gitignore`) or into the given directory. To keep the shipped data untouched, copy the
extracted directory and point `FE_WORK` at the copy before regenerating anything.

### Input data layout

```
$FE_WORK/has/insts_pe3/, insts_pe4/, vit_full/   programs of the reference runs (data package)
$FE_WORK/has/net_has/                            exported network of the HAS network testbench (data package)
$FE_WORK/reference_runs/                         logs and per-tile counters of the reference runs (data package)
$FE_WORK/weights/yolov8m.pt                      Ultralytics YOLOv8m weights (not in the package)
$FE_WORK/datasets/coco8/images/{train,val}/      calibration and test images, Ultralytics coco8 (not in the package)
$FE_WORK/step6b/net/                             exported network of the network testbench (section 3, step 6b)
```

With the data package as `$FE_WORK`, the programs of section 6 run as shipped and the YOLOv8m programs can be
regenerated without the weights (checked from a fresh copy of the repository, `RELEASE_NOTES.md`, section 4). Section 4
needs `step6b/net/`, which the frontend of section 3 produces from the weights and images.

## 2. Native model (`npu_top.h`)

Targets are defined in the `Makefile`. Some unit tests pin a 64x64 array. The Makefile rule of each target sets
the right geometry.

```bash
make tb_unified_smoke && ./tb_unified_smoke          # CSR smoke test
make test_rich_isa && ./test_rich_isa                # rich ISA, dual lane
make test_yolo && ./test_yolo                        # full-network instruction-level regression
make test_onnx_model && ./test_onnx_model
make test_vit && ./test_vit
```

The Makefile only tracks `.cpp` files, so after a header change `make` may report the target as up to date.
`tb_obp` and `tb_re` have no Makefile target. Build them with the same flags, for example:

```bash
g++ -std=c++17 -O3 -I. -I$SYSTEMC_HOME/include tb_obp.cpp -L/usr/lib/x86_64-linux-gnu -lsystemc -lm -pthread -o tb_obp
```

To force a clean regression build of `tb_obp`, `test_vit`, `test_yolo` and `test_onnx_model` without touching
the binaries in the root, run:

```bash
bash tools/has/regress_native.sh > tools/has/regress/regress.log 2>&1
```

## 3. Frontend: ONNX to a tiled int8 program

Run in order. Each step writes under `$FE_WORK/<step>/` and prints a final `RESULT` or `PASS`/`FAIL` line.

| Step | Command | Output |
|---|---|---|
| 1 Export and check | `python3 tools/fe/fe_step1_export_check.py` | `onnx/yolov8m_fp32_taps.onnx` |
| 2 Graph to IR | `python3 tools/fe/fe_step2_graph_check.py` | `step2/graph_ir.json` |
| 3 Quantization | `python3 tools/fe/fe_step3_quant.py` | `step3/<config>/params.npz` |
| 4 Integer golden | `python3 -u tools/fe/fe_step4_int8_check.py --config pc_p9999_rc --images 2` | `step4/<config>/golden_*.npz` |
| 5 Quality vs FP32 | `python3 -u tools/fe/fe_step5_quality.py --config pc_p9999_rc` | report only |
| 6a Tile plan | `python3 -u tools/fe/fe_step6a_tile_check.py --config pc_p9999_rc --images 2` | `step6/program.json` |
| 6b Export | `python3 -u tools/fe/fe_step6b_export_net.py --config pc_p9999_rc --image 0` | `step6b/net/` |

Step 6a ranks tile candidates with the cost table `tools/eval_sw/out/prep/shape_table.csv`. If the file is
missing, the planner falls back to the minimum-tile-count objective and prints a warning. The two objectives
produce different programs. Keep the table in place to reproduce the published plan.

### HAS variant of the program

```bash
python3 tools/fe/fe_make_has_config.py                       # step3/pc_p9999_has
python3 tools/fe/fe_has_program.py                            # has/program_has_{compat,has}.json
python3 -u tools/fe/fe_has_export_net.py --mode has --image 0 # has/net_has/
```

## 4. RTL-ref network testbench

### Build

```bash
bash tools/fe/sysc/build_tb_fe_core_net.sh                    # -> fe_work/step6b/tb_fe_core_net
```

Useful build variables:

| Variable | Effect |
|---|---|
| `EXTRA_DEFS="-DFX1_A3_SRAM_BACKDOOR_LOAD"` | Allow loading core SRAM without bus cycles (needs `FE_SRAM_BACKDOOR=1` at run time as well). Removes the per-tile bus load and read-back, the largest host-side cost |
| `PERF_DEFS="" EXTRA_DEFS="-DFE_METRICS ..."` | Enable per-tile performance counters and the metrics CSV |
| `OUT=<path>` | Output binary path |

The build used for the published full-network results:

```bash
PERF_DEFS="" EXTRA_DEFS="-DFE_METRICS -DFX1_A3_SRAM_BACKDOOR_LOAD" \
  OUT=fe_work/step6b/tb_fe_core_net_full bash tools/fe/sysc/build_tb_fe_core_net.sh
```

### Run

Usage: `tb_fe_core_net <net_dir> [max_steps]`. Behaviour is selected with environment variables:

| Variable | Meaning |
|---|---|
| `FE_CORE_STEPS` | Steps executed on the cycle-accurate core: `all`, or a list such as `0,5-8`. Unset means a C++ reference convolution is used for every step |
| `FE_CORE_BACKEND` | `npu_top` (default) or `lane_a` (older core wrapper, kept for comparison) |
| `FE_SRAM_BACKDOOR` | `1` to load and read core SRAM without bus cycles (build flag required) |
| `FE_CORE_GOLD_INPUT` | `1` to feed core steps from the golden instead of the previous step, so a single layer can be checked in isolation |
| `FE_STEP_FIRST` | First step to execute. Use with `FE_RESUME_FROM` or `FE_CORE_GOLD_INPUT` |
| `FE_SNAPSHOT_DIR` | Directory for a DRAM snapshot written after every step (enables resume) |
| `FE_RESUME_FROM` | Snapshot directory to resume from |
| `FE_CORE_MAX_CYCLES` | Cycle limit per tile (default 2,000,000) |
| `FE_STALL_CYCLES` | Consecutive stalled cycles that end a tile as stuck (default 5,000) |
| `FE_CORE_SHAPE_N`, `FE_CORE_MAX_TILES` | Run only the first N tiles of each tile shape, or the first N tiles, on the core |
| `FE_METRICS_CSV` | Per-tile metrics output (metrics build only), appended across restarts |
| `FE_COVERAGE_CSV` | Tile-shape coverage table (with `FE_CORE_SHAPE_N`) |

Examples:

```bash
# one layer in isolation, all tiles on the core
FE_CORE_STEPS=39 FE_CORE_GOLD_INPUT=1 FE_STEP_FIRST=39 ./fe_work/step6b/tb_fe_core_net fe_work/step6b/net 40

# full network, chained, with snapshots and metrics
mkdir -p run/snap
FE_CORE_STEPS=all FE_SRAM_BACKDOOR=1 FE_SNAPSHOT_DIR=run/snap FE_METRICS_CSV=run/metrics_tiles.csv \
  stdbuf -oL ./fe_work/step6b/tb_fe_core_net_full fe_work/step6b/net > run/run.log 2>&1 &

# resume after an interruption at step N (the number in run/snap/last_step.txt is N-1)
FE_CORE_STEPS=all FE_SRAM_BACKDOOR=1 FE_SNAPSHOT_DIR=run/snap FE_RESUME_FROM=run/snap FE_STEP_FIRST=N \
  FE_METRICS_CSV=run/metrics_tiles.csv stdbuf -oL ./fe_work/step6b/tb_fe_core_net_full fe_work/step6b/net >> run/run.log 2>&1 &
```

Always use `stdbuf -oL` when redirecting to a file. Without it, output is block-buffered and the log looks empty
for a long time.

### Output

- `[STEP] n/117 kind=... bad=0 PASS tiles=... via_core=... sim_cycles=...`: one line per step, printed as soon as
  the step completes.
- `[tb_fe_core_net] RESULT: PASS|FAIL (...)` at the end, followed by tile, OBP and DMA counters.
- Exit code 0 on PASS.

## 5. HAS network testbench

```bash
bash tools/fe/sysc/build_tb_has_net.sh                         # -> fe_work/has/tb_has_net
FE_CORE_STEPS=all FE_SRAM_BACKDOOR=1 stdbuf -oL ./fe_work/has/tb_has_net fe_work/has/net_has > has_run.log 2>&1
```

The same run-time variables as section 4 apply. The HAS program has 129 steps (element-wise add and max run in
the ELEM_WISE block instead of the host).

Unit testbenches of the HAS blocks:

```bash
bash tools/has/build_tb_has.sh tb_gvu_quant    && ./tools/has/tb_gvu_quant [vector files]
bash tools/has/build_tb_has.sh tb_gvu_elemwise && ./tools/has/tb_gvu_elemwise
bash tools/has/build_tb_has.sh tb_gvu_lut      && ./tools/has/tb_gvu_lut [vector files]
g++ -std=c++17 -O2 -I. tools/has/tb_gvu_vrf.cpp -o tools/has/tb_gvu_vrf && ./tools/has/tb_gvu_vrf
python3 tools/has/make_avgpool_gate.py /tmp/avgpool_gate && ./tools/has/tb_has_npu_top /tmp/avgpool_gate   # AVG_POOL
```

Independent Python vector files are produced by `tools/fe/fe_has_vectors.py` and `tools/fe/fe_ref_has_rce.py`.

## 6. Delivery top level `HasNpuTop`

This is the flow of the reference results (`PERFORMANCE_REPORT.md`, sections 5.4 to 5.6): the network runs from
the memory-mapped instruction stream only, the testbench acts as the CPU.

### Build

```bash
bash tools/has/build_tb_has_npu_top.sh                     # -> tools/has/tb_has_npu_top (one binary for every profile)
```

### Programs

```bash
python3 tools/fe/fe_emit_insts.py --plan pe3               # YOLOv8m, reference tiling  -> $FE_WORK/has/insts_pe3
python3 tools/fe/fe_emit_insts.py --plan has               # YOLOv8m, baseline tiling   -> $FE_WORK/has/insts_has (not shipped)
python3 tools/fe/fe_emit_insts.py --plan pe4 --halo --c-bcast   # tiling for the proposals profile -> $FE_WORK/has/insts_pe4
python3 tools/fe/fe_emit_insts.py --plan pe3 --slice-concat dma # concat by DMA_TENSOR COPY   -> $FE_WORK/has/insts_pe3_dma (not shipped)
python3 tools/fe/fe_emit_insts.py --plan pe3 --slice-concat dma_nd # concat by 4-D descriptors -> $FE_WORK/has/insts_pe3_nd (not shipped)
python3 tools/fe/fe_vit_full.py                            # ViT-B/16, 12 blocks          -> $FE_WORK/has/vit_full
```

The `pe` plans need measured per-layer tables, and the ViT scripts need the timm weights and `vit_scales.json`
(`FRONTEND_GUIDE.md`, sections 11 and 12).

### Run profiles

The testbench and the `HasNpuTop` constructor select a **profile**. The reference results of this package were measured with the five vector-unit knobs at 0 (the prefix `K0`, below); the default configuration gives the numbers of the default rows in the table of the expected results.

| Profile | Selects | Use |
|---|---|---|
| `recommended` (default) | overlapped ping-pong schedule, HAS AXI-128 DMA, epilogue inline on the PSM -> SRAM-C path, banked scratchpad, vector-unit latencies, 3-D DMA descriptors, per-layer tile order, attention products on the core | Hardware as drawn (run C) |
| `proposals` | `recommended` + bias preload by broadcast descriptor + halo reuse in SRAM-A | The two proposed hardware options (run D) |
| `legacy` | sequential schedule, v4.5 DMA timing, epilogue after the core | Regression of older results only |

Single options are added on top of a profile (list: header of `tools/has/tb_has_npu_top.cpp`), for example
`--c-bcast` alone or `--dram-lat 100`. In C++: `has::HasNpuTop npu("npu");` gives the recommended profile,
`has::HasNpuTop npu("npu", has::Profile::Proposals);` another one.

```bash
tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3                          # YOLOv8m, default vector-unit flow
tools/has/tb_has_npu_top $FE_WORK/has/vit_full                            # ViT-B/16, default vector-unit flow
# the reference results of the table below: the five vector-unit knobs set to 0
K0="HAS_GVU_TOP=0 HAS_GVU_STALL=0 HAS_GVU_ELEM_CLK=0 HAS_GVU_REGS=0 HAS_GVU_AGU_ND=0"
env $K0 tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3                          # YOLOv8m, hardware as drawn (62,666,123 cycles)
env $K0 tools/has/tb_has_npu_top $FE_WORK/has/vit_full                            # ViT-B/16, hardware as drawn (41,680,289 cycles)
env $K0 HAS_SPEC=v47ref tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3          # YOLOv8m, run C
env $K0 HAS_SPEC=v47ref tools/has/tb_has_npu_top $FE_WORK/has/insts_pe4 --profile proposals   # YOLOv8m, run D
env $K0 HAS_SPEC=v47ref tools/has/tb_has_npu_top $FE_WORK/has/vit_full --c-bcast # ViT-B/16 with the bias-broadcast proposal
env $K0 tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3 --first 40 --count 2      # a window of two instructions (1,658,986 cycles)
env $K0 tools/has/tb_has_npu_top $FE_WORK/has/insts_has --profile legacy --overlap --dma has   # YOLOv8m baseline (regenerate insts_has first)
```

Environment: `FE_METRICS_CSV=<file>` writes per-tile core counters, `FE_SNAPSHOT_DIR=<dir>` writes the DRAM image
after the run, `HAS_*` variables set the arithmetic knobs (`KNOWN_LIMITATIONS.md`, section 7). `HAS_SPEC` selects
the vector-unit configuration as a group (`HARDWARE_CONFIG.md`, section 6.3): unset = the default (scratchpad of the
micro-architecture specification), `v47ref` = the configuration of the results measured before that specification,
`mas10` = the typical bank sizes of the specification's capacity table, `rtl` = the behaviours of the functional-unit
RTL over the `v47ref` scratchpad.

### Vector-unit flow

By default the vector unit runs on its top level `GvuTop`, next to the core on the core clock, with the
full flow that follows the RTL most closely: `HAS_GVU_TOP=3` (stream of the partial-sum path into `GvuTop`), `HAS_GVU_STALL=1`
(the core stops on a full stream FIFO), `HAS_GVU_ELEM_CLK=1` (residual add and max pool on the clock), `HAS_GVU_REGS=1` (the
data flow controller configures and starts the unit through its register file) and `HAS_GVU_AGU_ND=1` (the residual add and the
max pool address the Scratchpad through the multi-dimensional address generator, `docs/SLICE_CONCAT_ON_GVU.md`). Average pool,
Softmax, the requant of FUSED_ATTN and LayerNorm also run on `GvuTop` as jobs on its clock (`HAS_GVU_AVG`, `HAS_GVU_SOFTMAX`,
`HAS_GVU_FA`, `HAS_GVU_LN`, all 1; `docs/GVU_OPS_ON_GVUTOP.md`). Setting a variable to 0 switches that part off
(`docs/GVU_TOP_CORE_INTEGRATION.md`, section 1.3). The reference results of this package were measured with the first five set to 0;
the commands that reproduce them carry the prefix
`HAS_GVU_TOP=0 HAS_GVU_STALL=0 HAS_GVU_ELEM_CLK=0 HAS_GVU_REGS=0 HAS_GVU_AGU_ND=0`. Whole-network totals of the default: YOLOv8m 63,902,513
cycles with `insts_pe3_nd`, ViT-B/16 41,244,621 cycles. The `GvuTop` epilogue has the depth 11 of the hardware RTL;
`EXTRA_CXXFLAGS=-DGVU_GEMM_STREAM_DEPTH=9 bash tools/has/build_tb_has_npu_top.sh` builds the depth 9 of the earlier releases,
with which YOLOv8m `insts_pe3` takes 62,422,935 cycles (the same with the four operation knobs at 0) and ViT-B/16 41,244,030
(41,582,468 with the four operation knobs at 0).

| Variable | Default | 0 gives |
|---|---|---|
| `HAS_GVU_TOP` | 3 | the model's own epilogue, residual add and max pool (1 and 2: `GvuTop` called as a function) |
| `HAS_GVU_STALL` | 1 | the core never stops on the stream; a full FIFO is counted as `STALL NEEDED` |
| `HAS_GVU_ELEM_CLK` | 1 | residual add and max pool called as functions (still forced on by `HAS_GVU_REGS=1`) |
| `HAS_GVU_REGS` | 1 | `GvuTop` configured by assignments instead of the register file |
| `HAS_GVU_AGU_ND` | 1 | no address program bound (2: shifted programs, for demonstration) |
| `HAS_GVU_AVG` | 1 | ELEM_WISE AVG_POOL on the functional model instead of `GvuTop` with the proposed average pool data path |
| `HAS_GVU_SOFTMAX` | 1 | the Softmax rows of FUSED_ATTN on the functional model (also switches `HAS_GVU_FA` off) |
| `HAS_GVU_FA` | 1 | the requant of the two attention products on the functional model |
| `HAS_GVU_LN` | 1 | LAYERNORM on the functional model instead of the lane data path proposed by the model (`has/rtl_ref_gvu_ln_lanes.h`) |

The last seven only act with `HAS_GVU_TOP=3`; with `HAS_GVU_REGS=0 HAS_GVU_ELEM_CLK=0` the four operations run in the call style
instead of as jobs on the clock (same values). The report printed at exit is described in `GVU_TOP_CORE_INTEGRATION.md`, section 6.3.

Options (the default is in the second column; the third column says what 1 gives):

| Variable | Default | 1 gives |
|---|---|---|
| `HAS_GVU_AVG_AS_RTL`, `HAS_GVU_SOFTMAX_AS_RTL` | 0 | negative controls: the rounding of the delivered RTL (average pool), the stage-3 shift of the delivered RTL (Softmax); the run fails |
| `HAS_GVU_LN_DUMP=<dir>` | unset | every LayerNorm group written as a text vector file |
| `HAS_DMA_TENSOR` | 1 | accepts the `DMA_TENSOR` COPY instruction (opcode `0x16`), used by `insts_pe3_dma` (`docs/DMA_TENSOR_MODE.md`); 0 refuses it at the push |
| `HAS_DMA_TENSOR_CHUNK=<bytes>` | 1024 | chunk of one COPY step (16 to 98,304); also the chunk of the slice / concat of `HAS_DMA_ND` |
| `HAS_TRACE_DMA_TENSOR` | 0 | one line per COPY: bytes, chunk, cycles waiting for the idle DMA, cycles of the copy (read by `tools/has/verify_flows.py`) |
| `HAS_DMA_ND` | 1 | slice / concat by 4-D DMA descriptors: ELEM_WISE modes 6 and 7 accepted, tile window and write-back through descriptors, used by `insts_pe3_nd` (`docs/DMA_SLICE_CONCAT.md`); 0 refuses modes 6 / 7 at the push |
| `HAS_GVU_REGS_NEO` | 0 | the vector unit runs on the register map of the `neo_lite` RTL (indices 0 to 13 as the RTL, proposals at 14 to 31), 14 writes per command, ping-pong buffers switched on the enable edge; values unchanged (`docs/HW_RTL_ALIGNMENT.md`, section 2). `HAS_GVU_NEO_WRITE_LAT` (1): cycles per write; `HAS_GVU_NEO_CDC_LAT` (4): cycles of the multiplexer and clock-domain crossing per write chain |
| `HAS_GVU_VBANK1_DEPTH=<rows>` | 32 | rows of vbank#1; 200 = the `neo_lite` RTL |
| `HAS_GVU_RESIDUAL_HW` | 0 | residual add through the RESIDUAL operation as the hardware builds it, when its parameters are expressible; `HAS_GVU_RESIDUAL_HW_SHIFT_LOGICAL=1` reproduces the logical shift of the delivered RTL (section 4 of `docs/HW_RTL_ALIGNMENT.md`) |
| `HAS_DMA_DFC_1D` | 0 | slice / concat (modes 6 / 7) as one-dimensional DMA commands of the DFC, with the measured command cost; `HAS_DMA_DFC1D_FIRST` (6), `HAS_DMA_DFC1D_CHAIN` (15), `HAS_DMA_DFC1D_TAIL` (3), core cycles, `HAS_DMA_DFC1D_STAGE` (25600) |
| `HAS_GVU_OUT_VBANK2` | 0 | GEMM_FUSED write-back charged as vbank2 -> DRAM by one-dimensional DFC commands; data path unchanged |
| `HAS_BUS_STRICT` | 0 | bus cost of the per-channel scale / shift of a tile and of the Softmax output A of FUSED_ATTN (data the DFC moves through the DMA); `HAS_BUS_STRICT_LUT=1` (test only) loads the activation table as a DMA transfer |
| `HAS_OUT_TRANSPOSE` | 1 | corner turn after the epilogue (proposal of the model, not in the RTL): GEMM_FUSED channels from `TMAJ_C0` (`0x40000500`) on are written token-major; programs: `tools/fe/fe_vit_full.py --v-token-major` (`docs/HW_RTL_ALIGNMENT.md`, section 8) |
| `HAS_DMA_ND_MERGE` | 1 | slice / concat by 4-D descriptors with contiguous dimensions merged, so that the DMA pieces are larger; same DRAM image (`docs/HW_RTL_ALIGNMENT.md`, section 12) |
| `HAS_GVU_FA_OVERLAP` | 0 | the attention requant runs on `GvuTop` during the next pass of the core (proposal P-FA5); values unchanged (`docs/HW_RTL_ALIGNMENT.md`, section 11) |
| `HAS_DMA_RESHAPE` | 1 | upsample and reshape as DMA operations (`DMA_TENSOR` modes 2 / 3); 0 refuses them with `UNSUPPORTED_MODE` (`docs/COMPILER_GUIDE.md`, section 7.2) |
| `HAS_DMA_RESHAPE_T` | 0 | 1: the transpose flag of a reshape (`X_FLAGS` bit 0) is accepted (an experiment); 0 refuses it |
| `HAS_NEO_REV` | 0 | which revision of the `neo_lite` RTL the options above follow: 0 = RTL revision 0, 1 = RTL revision 1 (`docs/HW_RTL_ALIGNMENT.md`, section 14) |
| `HAS_DMA_DFC1D_RESTART`, `HAS_DMA_DFC1D_REPORT` | 0, 0 | with `HAS_NEO_REV=1`: extra cycles per re-opened DFC command (not measured); 1 = counters of the one-dimensional commands on stderr at exit |
| `HAS_DFC_CORE_DMA` | 0 | with `HAS_NEO_REV=1`: core operand loads charged as one-dimensional DFC commands (`docs/HW_RTL_ALIGNMENT.md`, section 14.3) |
| `HAS_PROFILE` (or `--profile neo` / `--profile neo_as_rtl`) | unset | run profiles with only what the hardware RTL revision 1 has; a variable set by the user wins over the profile (`docs/COMPILER_GUIDE.md`, section 5) |
| `HAS_HW_CHECK` | 0 | one line per instruction: would the hardware run it as written, with reason codes (`docs/COMPILER_GUIDE.md`, section 3) |
| `HAS_HOST`, `HAS_HOST_NEO_PATCHES`, `HAS_HOST_NEO_QUIET` | `neo` when unset (`v45` or any other value: window off), `A-H1,A-H4`, 0 | front end for the 82-word register file of the `neo_lite` DFC (`docs/NEO_REGISTER_FILE.md`, section 6) |
| `HAS_HOST_TRACE=<file>` | unset | `tools/has/host_trace_gen`: file of the register trace of a program |

### Expected results

| Program and options | Instructions | Result | Total cycles |
|---|---:|---|---:|
| `insts_pe3_nd`, default (every knob default, epilogue depth 11) | 135 + 2 host steps | PASS, 0 mismatches | 63,902,513 |
| `insts_pe3_nd`, `HAS_DMA_ND_MERGE=0` (the default of the previous release), epilogue depth 11 | 135 + 2 host steps | PASS, 0 mismatches | 64,135,381 |
| `vit_full`, default (every knob default, epilogue depth 11) | 244 | PASS, 0 mismatches | 41,244,621 (LayerNorm cycles ESTIMATED) |
| `insts_pe3`, epilogue depth 9 (`EXTRA_CXXFLAGS=-DGVU_GEMM_STREAM_DEPTH=9`, as every row with `GvuTop` below) | 98 + 2 host steps | PASS, 0 mismatches | 62,422,935 |
| `insts_pe3`, depth 9, four operation knobs at 0 (the default of an earlier release) | 98 + 2 host steps | PASS, 0 mismatches | 62,422,935 |
| `vit_full`, depth 9 | 244 | PASS, 0 mismatches | 41,244,030 (LayerNorm cycles ESTIMATED) |
| `vit_full`, depth 9, four operation knobs at 0 (the default of an earlier release) | 244 | PASS, 0 mismatches | 41,582,468 |
| `insts_pe3`, knobs off | 98 + 2 host steps | PASS, 58,425,600 elements, 0 mismatches | 62,666,123 |
| `vit_full`, knobs off | 244 | PASS, 26,068,616 elements, 0 mismatches | 41,680,289 |
| `insts_pe3`, knobs off, `HAS_SPEC=v47ref` (run C) | 98 + 2 host steps | PASS, 58,425,600 elements, 0 mismatches | 62,681,721 |
| `insts_pe4`, `--profile proposals`, `HAS_SPEC=v47ref` (run D) | 98 + 2 host steps | PASS, 0 mismatches | 60,386,685 |
| `vit_full`, knobs off, `HAS_SPEC=v47ref` | 244 | PASS, 26,068,616 elements, 0 mismatches | 41,065,258 |
| `vit_full`, `--c-bcast`, `HAS_SPEC=v47ref` | 244 | PASS, 26,068,616 elements, 0 mismatches | 39,922,498 |
| `insts_pe3`, `--first 40 --count 2`, knobs off (either `HAS_SPEC`: two convolutions) | 2 | PASS | 1,658,986 |
| `insts_pe3`, knobs off, `HAS_FEEDER_PAD=1` (padding by the IFMAP feeder) | 98 + 2 host steps | PASS, 0 mismatches | 62,698,248 |
| `insts_pe3`, `--first 40 --count 2`, `HAS_GVU_TOP=1` / `HAS_GVU_TOP=2` with `HAS_GVU_STALL=0 HAS_GVU_ELEM_CLK=0 HAS_GVU_REGS=0 HAS_GVU_AGU_ND=0` (vector unit on `GvuTop` as a function) | 2 | PASS | 1,658,651 / 1,650,477 |
| `rce_gate` (transformer instruction check), knobs off / knobs off + `HAS_SPEC=v47ref` | 14 | PASS | 285,205 / 250,885 vector-unit cycles |
| `rce_gate`, default / four operation knobs at 0 / call style (`HAS_GVU_REGS=0 HAS_GVU_ELEM_CLK=0`) | 14 | PASS | 263,883 / 285,256 / 242,711 (default with `-DGVU_GEMM_STREAM_DEPTH=9`: 263,816) |
| AVG_POOL check (`python3 tools/has/make_avgpool_gate.py <dir>`), default / four operation knobs at 0 | 8 | PASS | 3,687 / 3,486 |
| `DMA_TENSOR` check (`docs/DMA_TENSOR_MODE.md`, section 8), `HAS_DMA_TENSOR=1` | 25 | PASS | — |
| `insts_pe3_dma`, depth 9 | 135 + 2 host steps | PASS, 0 mismatches | 63,902,431 |
| slice / concat check (`python3 tools/has/make_ref_gate_dma_slice_concat.py <dir>`), `HAS_DMA_ND=1` | 27 | PASS, 149,504 elements, 0 mismatches | 179,787 |
| slice / concat of 256 KB tensors (`python3 tools/has/make_dma_nd_big_gate.py <dir>`, needs `numpy`), `HAS_DMA_ND=1` | 10 | PASS, 2,048,000 elements, 0 mismatches | 127,636 |
| `insts_has`, knobs off, `--profile legacy --overlap --dma has` (baseline) | 98 + 2 host steps | PASS, 58,425,600 elements, 0 mismatches | 112,912,950 |

"knobs off" is the prefix `HAS_GVU_TOP=0 HAS_GVU_STALL=0 HAS_GVU_ELEM_CLK=0 HAS_GVU_REGS=0 HAS_GVU_AGU_ND=0` (variable `K0` above); every row
except the rows that name the default or the four operation knobs was measured that way. "Four operation knobs at 0" is the prefix
`HAS_GVU_AVG=0 HAS_GVU_SOFTMAX=0 HAS_GVU_FA=0 HAS_GVU_LN=0`. The run prints one `[STEP]` line per instruction and ends with
`[tb_fe_core_net] RESULT: PASS|FAIL (...)` and the total `sim cycles`; exit code 0 on PASS. Instruction-path check of the transformer instructions:
`tools/has/tb_has_npu_top $FE_WORK/has/rce_gate` (14 instructions, PASS; the program is shipped in the data package
and `python3 tools/has/make_rce_gate.py <vector dir> <table dir> <out dir>` regenerates it).

Regression of the vector-unit blocks and of the instruction path (transformer check, window D, AVG_POOL, padding,
short contexts, dilation, `GvuTop`, `DMA_TENSOR`; the script sets the nine vector-unit knobs to 0 itself and each item sets the knobs it
checks): `bash tools/has/regress_gvu.sh <output dir>` from the repository root, 242 items
(including the four operations on `GvuTop`, their negative controls, the `DMA_TENSOR` group, the `HAS_DMA_ND` group, the
verification by instruction flow, the options of `docs/HW_RTL_ALIGNMENT.md` each on the reference check program with their unit tests,
the RTL revision 1 group, the run profiles and the hardware check, and the register-file front end); a second argument `--unit-only` runs the unit tests only. The last line is
`RESULT: <n> passed, <m> failed`. The last item runs `python3 tools/has/verify_flows.py <output dir>`, which writes
`<output dir>/flows/<FLOW>.log` and `<output dir>/flows/SUMMARY.md`: every step of the ten flows of the vector unit and the
DMA with its checks, references and measured results (`docs/FLOW_VERIFICATION.md`); it can be run again on the logs alone. The script was written for the development tree and reads the programs and vectors
under `fe_work/` at the repository root (`fe_work/has/...`); in this repository that is the extracted data package
(for example `ln -s sauria_npu_data fe_work`). The package check runs its own subset of these items, not this script.

## 7. Post-processing

### Runs of `tb_has_npu_top` (section 6)

Run with `FE_METRICS_CSV` set so that the per-tile core counters are written next to the log, then roll up:

```bash
mkdir -p run_c
FE_METRICS_CSV=run_c/metrics_tiles.csv tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3 --trace > run_c/run.log 2>&1
python3 tools/metrics/has_rollup.py run_c                                  # -> run_c/rollup/
python3 tools/metrics/has_rollup.py run_d --cmp $FE_WORK/reference_runs/yolov8m_run_c   # compare per instruction
python3 tools/metrics/has_rollup.py $FE_WORK/reference_runs/vit_b16 --out vit_rollup    # a reference run
```

Outputs: `instructions.csv` (cycles, tiles, core passes and counters, real and executed MACs, PE utilization and
SRAM bytes per instruction), `operations.csv` (cycles per operation), `network.json` and `report.md` (totals,
PE utilization, GOPS, frame rate). It works on a running job. `--trace` gives the exact cycles of each instruction;
without it they are derived from the `[STEP]` lines and include the register writes between instructions. Definitions are in the header of the script. On the
three reference runs it reproduces the totals of `PERFORMANCE_REPORT.md`, sections 5.4 to 5.6.

The report tables (network roll-up with the basis of every number, latency and throughput summary, PE utilization
breakdown, per-layer table, controller state distribution) come from `post_fullcore.sh`, which recognizes a
`tb_has_npu_top` run from its log:

```bash
# reference runs of the data package (no DRAM snapshot there, hence --no-detect)
bash tools/metrics/post_fullcore.sh --run $FE_WORK/reference_runs/yolov8m_run_c --net $FE_WORK/has/insts_pe3 \
  --out post_c --layers --fsm --no-detect                                  # YOLOv8m reference
bash tools/metrics/post_fullcore.sh --run $FE_WORK/reference_runs/yolov8m_baseline --net $FE_WORK/has/insts_pe3 \
  --out post_base --layers --fsm --no-detect                               # YOLOv8m baseline
bash tools/metrics/post_fullcore.sh --run $FE_WORK/reference_runs/vit_b16_as_drawn --net $FE_WORK/has/vit_full \
  --out post_vit --layers --fsm                                            # ViT-B/16 as drawn

# an own run with a snapshot: detection boxes compared with the golden, and the ViT-B/16 top-5 classes
FE_SNAPSHOT_DIR=run_c/snap FE_METRICS_CSV=run_c/metrics_tiles.csv tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3 > run_c/run.log 2>&1
python3 tools/fe/fe_sysc_detect.py --dram run_c/snap/dram_snapshot.bin --net-dir $FE_WORK/has/insts_pe3 --compare-golden
python3 tools/fe/fe_vit_top5.py --net-dir $FE_WORK/has/vit_full --dram run_vit/snap/dram_snapshot.bin --no-float
```

The numbers printed for the three reference runs are those of `PERFORMANCE_REPORT.md`, section 5.7. Drawing the
boxes (`--draw`) and the floating-point comparison of `fe_vit_top5.py` need the source image and the trained
weights, which are not in the package; the per-layer table reads only the log and the per-tile counters.

### Runs of the network testbench (section 4)

```bash
# roll-up of a (possibly still running) full-network run
python3 tools/metrics/fullcore_rollup.py --program fe_work/step6/program.json \
  --csv run/metrics_tiles.csv --log run/run.log --out-dir run/rollup

# detection boxes from a DRAM snapshot, compared with the golden
python3 tools/fe/fe_sysc_detect.py --dram run/snap/dram_snapshot.bin --compare-golden --json run/det.json --draw run/det.png

# everything at once (roll-up, per-layer and FSM tables, detections)
bash tools/metrics/post_fullcore.sh --run run --out run/post --layers --fsm --draw
```

## 8. Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `run.log` stays empty | Output is block-buffered. Run the binary with `stdbuf -oL` |
| Run is much slower than expected | Binary built without `-DFX1_A3_SRAM_BACKDOOR_LOAD`; `FE_SRAM_BACKDOOR=1` is then ignored |
| Every step after the first fails when using `FE_STEP_FIRST` | Earlier tensors were never produced. Add `FE_RESUME_FROM` or `FE_CORE_GOLD_INPUT=1` |
| `make` target not rebuilt after a header change | Use `tools/has/regress_native.sh` or delete the binary first |
| A tile reported as `deadlock/timeout` | The tile hit `FE_CORE_MAX_CYCLES` or stalled for `FE_STALL_CYCLES`; see `docs/KNOWN_LIMITATIONS.md` |
