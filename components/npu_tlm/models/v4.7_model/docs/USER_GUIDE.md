# User Guide

The shortest path from a fresh copy of the repository to the reference results, with a picture of what runs at
each step. Details are in the documents named in each section.

Requirements: Ubuntu 22.04, g++ 11, SystemC 2.3.3, Python 3.10 with numpy (`BUILD_AND_RUN.md`, section 1). All
commands are run from the repository root.

## 1. Running the two networks

### 1.1 Prepare

```bash
sha256sum --quiet -c package_manifest.txt                 # optional: every file of the repository is intact
bash tools/unpack_data.sh                                 # once: joins data/ parts, checks them, extracts ./sauria_npu_data
export SYSTEMC_HOME=/usr
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH
export FE_WORK=$PWD/sauria_npu_data                       # the data package: programs, golden images, reference logs
```

### 1.2 Build

One binary serves every network and every configuration:

```bash
bash tools/has/build_tb_has_npu_top.sh                    # -> tools/has/tb_has_npu_top
```

### 1.3 Quick check

Two YOLOv8m instructions, about 20 minutes on one core:

```bash
tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3 --first 40 --count 2     # PASS, 1,658,986 cycles
```

### 1.4 Whole networks

| Network and configuration | Command | Time on one core | Expected result |
|---|---|---|---|
| YOLOv8m, hardware as drawn (reference, run C) | `tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3` | about 10 hours | PASS, 98 instructions + 2 host steps, 0 mismatches, 62,681,721 cycles (12.76 FPS at 0.8 GHz) |
| ViT-B/16, hardware as drawn | `tools/has/tb_has_npu_top $FE_WORK/has/vit_full` | about 5 hours | PASS, 244 instructions, 0 mismatches, 41,065,258 cycles (19.48 FPS) |
| ViT-B/16 with the bias-broadcast proposal | `tools/has/tb_has_npu_top $FE_WORK/has/vit_full --c-bcast` | about 5 hours | PASS, 39,922,498 cycles (20.04 FPS) |
| YOLOv8m with the two hardware proposals (run D) | `tools/has/tb_has_npu_top $FE_WORK/has/insts_pe4 --profile proposals` | about 10 hours | PASS, 60,386,685 cycles (13.25 FPS) |

Every instruction prints one `[STEP]` line as soon as it finishes; the run ends with `RESULT: PASS` or `FAIL` and
the total `sim cycles`. To keep a log, the per-tile counters and the final memory image of a long run:

```bash
mkdir -p run_c
FE_METRICS_CSV=run_c/metrics_tiles.csv FE_SNAPSHOT_DIR=run_c/snap \
  stdbuf -oL tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3 > run_c/run.log 2>&1 &
```

The measured logs of all these runs are already in `$FE_WORK/reference_runs/`, so the tables of section 3 can be
produced without rerunning anything.

### 1.5 Shorter checks

| Check | Command | Expected |
|---|---|---|
| Transformer instructions (attention, LayerNorm), 14 instructions | `tools/has/tb_has_npu_top $FE_WORK/has/rce_gate` | PASS, 14 steps |
| Native model regression | `bash tools/has/regress_native.sh` | every target passes |
| Vector-unit blocks | `bash tools/has/build_tb_has.sh tb_gvu_quant && ./tools/has/tb_gvu_quant` (also `tb_gvu_elemwise`, `tb_has_dma`) | `RESULT: PASS` |
| Average pool through the instruction path | `python3 tools/has/make_avgpool_gate.py /tmp/avgpool_gate && tools/has/tb_has_npu_top /tmp/avgpool_gate` | PASS, 8 steps |

## 2. What runs

### 2.1 Python frontend (already run; its outputs are in the data package)

```
trained model (ONNX / weights)
  -> IR                         tools/fe/fe_graph.py
  -> int8 quantization          tools/fe/fe_quant.py
  -> independent integer golden tools/fe/fe_ref_int8.py, fe_ref_has.py
  -> tile plan                  tools/fe/fe_tile_plan.py, then the per-layer rules of fe_emit_insts.py
  -> program                    tools/fe/fe_emit_insts.py (YOLOv8m), tools/fe/fe_vit_full.py (ViT-B/16)
```

A program is three files in one directory:

| File | Content |
|---|---|
| `mmio.txt` | The register writes a CPU performs, in order; one instruction per layer |
| `dram_init.bin` | Initial memory image: the input image, weights and parameters |
| `dram_golden.bin` | The same layout with every tensor filled with its expected value |

Details: `FRONTEND_GUIDE.md`; file formats: `INTERFACE_SPEC.md`, section 6.5; how the tiles are chosen:
`TILING_GUIDE.md`.

### 2.2 SystemC simulation (`tb_has_npu_top` plays the CPU)

```
1. load dram_init.bin into the DRAM
2. replay mmio.txt; every push is one instruction (one layer):
     host register writes (v4.5 protocol + extension registers)
       -> compatibility layer: registers -> instruction queue
       -> data flow controller: walks the tiles of the layer
            -> DMA (AXI-128, 3-D descriptors) loads weights and input window into SRAM A / B, ping-pong
            -> cycle-accurate core: controller, feeders, 32x32 array, partial-sum manager
            -> epilogue on the partial-sum path into SRAM-C (requant, activation table)
            -> DMA writes the int8 results to the DRAM
       -> vector unit: ELEM_WISE (ADD, MAX_POOL, AVG_POOL), softmax, LAYERNORM;
          FUSED_ATTN: the two matrix products run on the core, the softmax in the vector unit
     host steps (`H` lines; YOLOv8m has two upsample x2) are done by the CPU between instructions
3. read STATUS / RETIRED; compare the DRAM with dram_golden.bin byte by byte; print the cycles of every
   instruction and the total
```

Details: `ARCHITECTURE.md`, sections 7 and 8. Which numbers are measured and which are estimated:
`VERIFICATION_REPORT.md`, section 1.1.

## 3. Reading the results

| Wanted | Command |
|---|---|
| Network totals, latency, frame rate, TOPS, PE utilization and its three factors, per-layer table, controller states | `bash tools/metrics/post_fullcore.sh --run $FE_WORK/reference_runs/yolov8m_run_c --net $FE_WORK/has/insts_pe3 --out post_c --layers --fsm --no-detect` |
| The same for ViT-B/16 | `bash tools/metrics/post_fullcore.sh --run $FE_WORK/reference_runs/vit_b16_as_drawn --net $FE_WORK/has/vit_full --out post_vit --layers --fsm` |
| Per-instruction CSV, comparison of two runs | `python3 tools/metrics/has_rollup.py <run dir> [--cmp <run dir>]` |
| Detection boxes of an own YOLOv8m run, compared with the golden | `python3 tools/fe/fe_sysc_detect.py --dram run_c/snap/dram_snapshot.bin --net-dir $FE_WORK/has/insts_pe3 --compare-golden` |
| Top-5 classes of an own ViT-B/16 run | `python3 tools/fe/fe_vit_top5.py --net-dir $FE_WORK/has/vit_full --dram <run>/snap/dram_snapshot.bin --no-float` |

For an own run, replace the reference directory by the run directory (`run_c` above). The numbers to expect are in
`PERFORMANCE_REPORT.md`, section 5.7.

## 4. Using the model in a system platform

| Goal | Do | Read |
|---|---|---|
| Drop-in replacement of the previous native model | Point the wrapper's model root at the repository root; nothing else changes | `SW_INTEGRATION_GUIDE.md`, section 9 |
| Run programs on the RTL-accurate core through the wrapper | Model root `core_rtl/`, apply `tools/has/wrapper/npu_tlm_rich_window.patch` | `SW_INTEGRATION_GUIDE.md`, sections 10.1 to 10.4 |
| First firmware run | `tools/has/wrapper/fw/` with the 14-instruction transformer check | `SW_INTEGRATION_GUIDE.md`, section 10.5 |
| Write a driver | Register map and programming sequences | `SW_INTEGRATION_GUIDE.md`, sections 3 and 4 |
| Write or extend a compiler | Data layout, weight order, tiling | `SW_INTEGRATION_GUIDE.md`, sections 5 and 6; `FRONTEND_GUIDE.md`; `TILING_GUIDE.md` |

## 5. If something goes wrong

| Symptom | Cause |
|---|---|
| `FE_WORK is not set` or a missing program file | `export FE_WORK=...` in the current shell; `tools/unpack_data.sh` not yet run |
| `unpack_data.sh` reports a checksum error | The parts were altered, typically by a line-ending conversion: keep `.gitattributes` and clone again |
| The log file stays empty | Output is block-buffered: use `stdbuf -oL` |
| `make` does not rebuild after a header change | The Makefile tracks only `.cpp` files: use `tools/has/regress_native.sh` or delete the binary |
| Other cases | `BUILD_AND_RUN.md`, section 8; `KNOWN_LIMITATIONS.md` |
