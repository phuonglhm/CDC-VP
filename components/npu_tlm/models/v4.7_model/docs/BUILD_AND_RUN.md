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

# full network, chained, with snapshots and metrics (about 14 hours on 4 cores)
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
python3 tools/fe/fe_vit_full.py                            # ViT-B/16, 12 blocks          -> $FE_WORK/has/vit_full
```

The `pe` plans need measured per-layer tables, and the ViT scripts need the timm weights and `vit_scales.json`
(`FRONTEND_GUIDE.md`, sections 11 and 12).

### Run profiles

The testbench and the `HasNpuTop` constructor select a **profile**; the default is the configuration of the
reference results, so no option is needed to reproduce them.

| Profile | Selects | Use |
|---|---|---|
| `recommended` (default) | overlapped ping-pong schedule, HAS AXI-128 DMA, epilogue inline on the PSM -> SRAM-C path, banked scratchpad, vector-unit latencies, 3-D DMA descriptors, per-layer tile order, attention products on the core | Hardware as drawn (run C) |
| `proposals` | `recommended` + bias preload by broadcast descriptor + halo reuse in SRAM-A | The two proposed hardware options (run D) |
| `legacy` | sequential schedule, v4.5 DMA timing, epilogue after the core | Regression of older results only |

Single options are added on top of a profile (list: header of `tools/has/tb_has_npu_top.cpp`), for example
`--c-bcast` alone or `--dram-lat 100`. In C++: `has::HasNpuTop npu("npu");` gives the recommended profile,
`has::HasNpuTop npu("npu", has::Profile::Proposals);` another one.

```bash
tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3                          # YOLOv8m, run C (about 10 hours, one core)
tools/has/tb_has_npu_top $FE_WORK/has/insts_pe4 --profile proposals       # YOLOv8m, run D
tools/has/tb_has_npu_top $FE_WORK/has/vit_full                            # ViT-B/16, hardware as drawn (about 5 hours)
tools/has/tb_has_npu_top $FE_WORK/has/vit_full --c-bcast                  # ViT-B/16 with the bias-broadcast proposal
tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3 --first 40 --count 2      # a window of two instructions
tools/has/tb_has_npu_top $FE_WORK/has/insts_has --profile legacy --overlap --dma has   # YOLOv8m baseline (regenerate insts_has first)
```

Environment: `FE_METRICS_CSV=<file>` writes per-tile core counters, `FE_SNAPSHOT_DIR=<dir>` writes the DRAM image
after the run, `HAS_*` variables set the arithmetic knobs (`KNOWN_LIMITATIONS.md`, section 7).

### Expected results

| Program and options | Instructions | Result | Total cycles |
|---|---:|---|---:|
| `insts_pe3`, default | 98 + 2 host steps | PASS, 58,425,600 elements, 0 mismatches | 62,681,721 |
| `insts_pe4`, `--profile proposals` | 98 + 2 host steps | PASS, 0 mismatches | 60,386,685 |
| `vit_full`, default | 244 | PASS, 26,068,616 elements, 0 mismatches | 41,065,258 |
| `vit_full`, `--c-bcast` | 244 | PASS, 26,068,616 elements, 0 mismatches | 39,922,498 |
| `insts_pe3`, `--first 40 --count 2` | 2 | PASS | 1,658,986 |
| `insts_has`, `--profile legacy --overlap --dma has` (baseline) | 98 + 2 host steps | PASS, 58,425,600 elements, 0 mismatches | 112,912,950 |

The run prints one `[STEP]` line per instruction and ends with `[tb_fe_core_net] RESULT: PASS|FAIL (...)` and the
total `sim cycles`; exit code 0 on PASS. Instruction-path check of the transformer instructions:
`tools/has/tb_has_npu_top $FE_WORK/has/rce_gate` (14 instructions, PASS; the program is shipped in the data package
and `python3 tools/has/make_rce_gate.py <vector dir> <table dir> <out dir>` regenerates it).

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
