# SAURIA NPU SystemC Model

A header-only C++17 / SystemC model of the SAURIA neural processing unit, plus a Python frontend that
compiles real networks (YOLOv8m, ViT-B/16) into memory-mapped instruction programs with an independent integer
golden reference, and the testbenches and scripts that run the whole network through the model and check it
bit-exactly.

## What is in this package

The package contains two lines of code for the same NPU.

| | RTL-ref / HAS path (validated, delivery path) | Native model (legacy, regression baseline) |
|---|---|---|
| Role | The path validated on real networks and the basis of the delivered NPU model | The functional model of the previous hand-overs, at revision v4.6; regression baseline and target of existing integration wrappers |
| Top level | `HasNpuTop` (`has/has_npu_top.h`), around the RTL-ref core `rtl_ref_npu_top.h`; `core_rtl/` makes it the model of a system TLM wrapper | `npu_top.h` |
| Compute | Clock-level core that follows the SAURIA RTL (controller, feeders, 32x32 systolic array, partial-sum manager, SRAM) with the HAS epilogue and element-wise blocks (`has/`) | Instruction-level: `instruction_decoder.h` evaluates each instruction in double-precision C++ (`emulate_*()`), bypassing the array, OBP and RE/RCE modules |
| Validation | Whole YOLOv8m and ViT-B/16 run by `HasNpuTop` from memory-mapped instructions only, bit-exact against independent integer goldens, also through a system TLM wrapper; earlier whole-network runs chained through the core on two images; vector-unit blocks bit-exact against independent Python implementations | `test_yolo` and `test_onnx_model` show that the old compiler and this emulator agree with each other; they do not validate real network semantics |
| Main testbenches | `tools/has/tb_has_npu_top.cpp` (delivery top level, reference results); `tools/fe/sysc/tb_fe_core_net.cpp`, `tools/fe/sysc/tb_has_net.cpp` (network testbenches of the earlier flow) | `tools/test_*.cpp`, `tb_obp.cpp`, `tb_re.cpp`, `tb_unified_smoke.cpp` |

**Integration direction.** Software written for the previous hand-over programs the NPU through the v4.5
memory-mapped protocol (gather registers at `0x40000400`, push an opcode to `0x40000310`). The delivery top
level, `HasNpuTop`, keeps that protocol through a compatibility layer, adds a small set of extension and status
registers, and executes each instruction on the RTL-ref / HAS path. The compatibility layer and the tile
iterator are implemented and verified. `HasNpuTop` is implemented and has passed its acceptance milestone: the full YOLOv8m
(98 instructions) driven only by memory-mapped instructions, bit-exact. With its default run profile it reaches
12.76 FPS on YOLOv8m and 19.48 FPS on ViT-B/16 (0.8 GHz, HAS AXI-128 DMA), with the hardware as drawn; 20.04 FPS on
ViT-B/16 with the proposed bias-broadcast option. Required software changes are listed in
`docs/SW_INTEGRATION_GUIDE.md`.

## Key results

| Result | Evidence |
|---|---|
| YOLOv8m int8, all 117 steps (83 convolutions, 34 host data-movement operations), chained layer to layer through the cycle-accurate core | 0 mismatching bytes against the integer golden; 9,811 of 9,811 tiles executed on the core; 0 core stalls |
| Two different input images | Both pass bit-exactly; detection boxes identical to the golden (4 and 9 boxes) |
| Cycle-level equivalence of the ported core with the reference core | 40 of 40 sampled tiles with identical busy and execute cycle counts |
| YOLOv8m with the epilogue and element-wise arithmetic of the HAS drawings, chained through the cycle-accurate core (129 steps, including 12 ELEM_WISE ADD and 3 ELEM_WISE MAX) | 88,915,200 output bytes, 0 mismatches against the independent HAS golden; 9,811 of 9,811 tiles on the core; 4 of 4 detection boxes identical to the golden (IoU 1.0000); no int16 saturation in the requant stage |
| HAS epilogue blocks (requant, dequant, LUT, ELEM_WISE add/max, indirect LUT) | Bit-exact against independent Python implementations on 120,000 to 400,000 vectors per block |
| Whole YOLOv8m expressed in the v4.5 memory-mapped protocol | 98 instructions, 2,306 register writes; decoded back to 9,811 of 9,811 tiles and 15 of 15 element-wise steps exactly |
| Whole YOLOv8m run by the delivery top level `HasNpuTop` from the memory-mapped instruction stream only (overlapped schedule, HAS AXI-128 DMA timing) | 58,425,600 elements, 0 mismatches; 9,811 of 9,811 tiles on the core; 112.9 M cycles = 7.09 FPS at 0.8 GHz |
| Whole YOLOv8m with the data flow of the hardware vector-unit specification and tile-planner optimizations, through `HasNpuTop` | 0 mismatches in all three configurations; reference run: 62.7 M cycles = 12.76 FPS (PE utilization 69.3 %) at 0.8 GHz. Earlier configurations: 73.1 M cycles = 10.94 FPS and 64.1 M cycles = 12.48 FPS. With two proposed hardware options (bias preload by broadcast descriptor, halo reuse): 60.4 M cycles = 13.25 FPS, also bit-exact |
| Whole ViT-B/16 (patch embedding, 12 encoder blocks, head) run by `HasNpuTop` from the memory-mapped instruction stream only, with the attention and LayerNorm instructions of the vector-unit specification | 244 instructions, 26,068,616 elements, 0 mismatches; 41.1 M cycles = 19.48 FPS at 0.8 GHz as drawn, 39.9 M cycles = 20.04 FPS with the proposed bias broadcast (softmax and LayerNorm timing estimated) |
| Both networks on a second image that was not used for calibration (coco128 000000000502), hardware as drawn | Both bit-exact; YOLOv8m cycles identical to the reference run, detection box identical to the golden; ViT-B/16 top-1 equal to the floating-point model |
| More images, chosen as the hardest of a survey of 112 images (YOLOv8m) and 5 images (ViT-B/16) of the integer golden against the floating-point model | Four YOLOv8m images and two ViT-B/16 images through `HasNpuTop`, hardware as drawn: all bit-exact, cycles identical on every instruction, boxes and top-1 identical to the golden. Integer against floating point: recall 0.90 and precision 0.94 of the boxes (YOLOv8m), top-1 equal on 3 of 5 images (ViT-B/16) |
| The same YOLOv8m and ViT-B/16 programs through a system TLM integration wrapper, with the model root `core_rtl/` | Every instruction passes; per-instruction cycles equal to the runs above (98 of 98 and 244 of 244) |
| AVG_POOL (ELEM_WISE mode 5) through the instruction path | 8 of 8 instructions bit-exact (not used by the two networks) |
| Native regression unaffected by the RTL-ref work | `test_yolo`, `test_onnx_model`, `tb_obp`, `test_vit` pass unchanged |

Full numbers, methodology and limitations are in `docs/VERIFICATION_REPORT.md` and `docs/PERFORMANCE_REPORT.md`.

## Documentation map

| Document | Read it to |
|---|---|
| `docs/USER_GUIDE.md` | Start here: run the two networks step by step, see what runs at each step, read the results |
| `docs/ARCHITECTURE.md` | Understand both models, the block diagrams and which parts are cycle-accurate |
| `docs/SW_INTEGRATION_GUIDE.md` | Port existing driver software to the delivery top level: register map, required changes, open points |
| `docs/FRONTEND_GUIDE.md` | Understand how an ONNX model becomes a tiled int8 program and a golden reference |
| `docs/TILING_GUIDE.md` | Cut a layer into tiles: limits and their formulas, cycle laws, procedure, worked examples, reference files |
| `docs/INTERFACE_SPEC.md` | Look up file formats (`prog.bin`, DRAM image, manifest, metrics CSV) and registers |
| `docs/HARDWARE_CONFIG.md` | Look up array, SRAM and OBP sizes and the build-time model options |
| `docs/BUILD_AND_RUN.md` | Set up the environment, build, and run every flow |
| `docs/VERIFICATION_REPORT.md` | See how correctness is established and the results |
| `docs/PERFORMANCE_REPORT.md` | See measured core metrics and how to interpret them |
| `docs/HAS_VALIDATION.md` | See the validation of the HAS drawings and the open questions for hardware |
| `docs/KNOWN_LIMITATIONS.md` | See modelling simplifications, open issues and risks |
| `docs/CODE_MAP.md` | Find where things live in the source tree |
| `RELEASE_NOTES.md` | See the contents and checksums of this release |
| `THIRD_PARTY_NOTICES.md` | See third-party software, models and data used, with their licences |
| `data/README.md` | Rebuild and extract the data package carried in this repository |

## Quick start

On Ubuntu 22.04 with g++ 11 and SystemC 2.3.3 installed (details in `docs/BUILD_AND_RUN.md`), from the root of
this repository:

```bash
export SYSTEMC_HOME=/usr
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH
bash tools/unpack_data.sh                                              # once: data/ parts -> ./sauria_npu_data, checked
export FE_WORK=$PWD/sauria_npu_data                                    # the extracted data package
make tb_unified_smoke && ./tb_unified_smoke                            # native model smoke test
bash tools/has/build_tb_has_npu_top.sh
tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3 --first 40 --count 2   # two YOLOv8m instructions: PASS, 1,658,986 cycles, about 20 minutes
python3 tools/metrics/has_rollup.py $FE_WORK/reference_runs/yolov8m_run_c --out rollup_c   # tables of the reference run
```

The repository is self-contained: the data package (programs, golden images, reference run logs) is carried in
`data/` as a split archive, each part below 100 MB (`data/README.md`). `.gitattributes` keeps every file byte for
byte so that the checksums of `package_manifest.txt` hold after a clone.

The delivery top level runs with the configuration of the reference results by default. The whole-network runs,
the profiles and the expected numbers are in `docs/BUILD_AND_RUN.md`, section 6. The network testbench of the
earlier flow (`tb_fe_core_net`, `docs/BUILD_AND_RUN.md`, section 4) needs an exported network that the data
package does not contain; the frontend produces it from the trained weights (section 3 of the same document).
