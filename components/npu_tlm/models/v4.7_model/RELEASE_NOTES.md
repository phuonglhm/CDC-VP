# Release Notes

## 1. Release

SAURIA NPU SystemC model, delivery package: one repository tree that holds the sources, the documents and the data.

| Part | Content | Size |
|---|---|---|
| Source tree | Native model, RTL-ref core, delivery top level `HasNpuTop`, frontend and tools, and the documents of `docs/` | about 12 MB |
| `data/` | The data package `sauria_npu_data` as a split archive (parts below 100 MB): programs, golden images, quantization parameters, unit-test vectors and reference run logs, in the layout of `$FE_WORK` | about 415 MB; about 810 MB extracted |

`package_manifest.txt` in the root lists the sha256 of every file of the tree, including the parts of `data/`
(`sha256sum -c package_manifest.txt`). `bash tools/unpack_data.sh` joins the parts, checks the archive against
`data/SHA256SUMS`, extracts `sauria_npu_data/` and checks every extracted file against the manifest of the data
package. `.gitattributes` disables line-ending conversion so that the checksums hold in a git clone; `.gitignore`
lists the extracted data and the build outputs.

## 2. What this release contains

- **Delivery top level `HasNpuTop`**: the NPU driven only by the v4.5 memory-mapped protocol, with a compatibility layer,
  the data flow controller, the cycle-accurate RTL-ref core, the DMA with HAS bus parameters and the vector-unit blocks
  (`docs/ARCHITECTURE.md`, section 7; `docs/SW_INTEGRATION_GUIDE.md`).
- **Run profiles**: the default configuration reproduces the reference results; `--profile proposals` adds the two
  proposed hardware options; `--profile legacy` reproduces older results (`docs/BUILD_AND_RUN.md`, section 6).
- **Vector-unit data flow**: epilogue inline on the partial-sum path, banked scratchpad, 3-D DMA descriptors, planner
  options (input-channel split, padded edge tiles, positions per context).
- **Transformer instructions**: FUSED_ATTN and LAYERNORM through the instruction stream, bit-exact, with the attention
  products on the core.
- **RTL-accurate core in a system wrapper**: model root `core_rtl/` with a drop-in top level, a proposed wrapper
  change for the DRAM window (`tools/has/wrapper/npu_tlm_rich_window.patch`) and a reference firmware sequence
  (`docs/SW_INTEGRATION_GUIDE.md`, section 10).
- **Vector-unit additions**: AVG_POOL on the instruction path (ELEM_WISE mode 5), structure models of the pipeline
  registers and of the load/store unit, a Scratchpad sizing preset (`HAS_SP_PRESET=gvu68k`); the vector-unit files are
  named `has/gvu_*.h` and their unit testbenches `tools/has/tb_gvu_*`.
- **Frontend**: ONNX to tiled int8 program and independent golden, register-write stream generator for YOLOv8m and ViT-B/16.
- **Run roll-up**: `tools/metrics/has_rollup.py` turns the log and per-tile counters of a `tb_has_npu_top` run (or a
  reference run of the data package) into per-instruction and per-operation tables and the network totals
  (`docs/BUILD_AND_RUN.md`, section 7). The ViT-B/16 PE utilization is now reported for the GEMM instructions
  (76.05 %) and including the attention products (69.26 %); the earlier single figure mixed the two
  (`docs/PERFORMANCE_REPORT.md`, section 5.6).
- **Documentation revised for the delivery flow**: the quick start and the data layout use the data package; the
  delivery testbench is listed in `docs/CODE_MAP.md`; the AVG_POOL mode number (5), the native revision (v4.6), the
  run-C per-layer figures (`docs/PERFORMANCE_REPORT.md`, section 3.3) and the ViT-B/16 core share are corrected.
- **Report tables for `tb_has_npu_top` runs**: `tools/metrics/post_fullcore.sh` and `tools/metrics/fullcore_rollup.py`
  now accept the runs of the delivery top level (YOLOv8m and ViT-B/16): network roll-up with the basis of every
  number, latency and throughput summary, PE utilization breakdown, per-layer table with the array columns and rows
  used, controller state distribution (`docs/BUILD_AND_RUN.md`, section 7; `docs/PERFORMANCE_REPORT.md`,
  section 5.7). `tools/fe/fe_vit_top5.py` reads the ViT-B/16 top-5 classes from a DRAM snapshot.
- **More input images**: survey of the integer golden against the floating-point model on 112 images (YOLOv8m) and
  5 images (ViT-B/16), and the hardest of them through `HasNpuTop` (`docs/VERIFICATION_REPORT.md`, V23).
- **Tiling rules**: the rules behind the reference tile plan, with the sign to look for in the per-layer table and
  a measured example each (`docs/FRONTEND_GUIDE.md`, section 11.1).
- **Data inside the repository**: the data package is carried in `data/`; two reference runs were added to it
  (the YOLOv8m baseline before the planner options, and ViT-B/16 with the hardware as drawn).
- **Third-party notices**: `THIRD_PARTY_NOTICES.md`.
- **Two new documents**: `docs/USER_GUIDE.md` (running the two networks step by step, what runs at each step, reading
  the results) and `docs/TILING_GUIDE.md` (tile limits with their formulas, cycle laws, a procedure per layer, worked
  examples checked against the measured runs, reference files).
- **Integration aids for the system wrapper**: the wrapper change `tools/has/wrapper/npu_tlm_rich_window.patch`
  regenerated against the wrapper revision that bundles the v4.6 native model (the earlier patch no longer applied
  there), with the commands to apply it; a firmware starting point in C under `tools/has/wrapper/fw/` (register
  definitions, program replay engine with CRC checks, bare-metal example, host test through the wrapper); the
  repository's `.gitignore` keeps the planner cost table when the tree is committed inside another repository
  (`docs/SW_INTEGRATION_GUIDE.md`, sections 9 and 10).
- **English sources**: the remaining comments and console messages written in unaccented Vietnamese (network
  testbenches, detection decoder, RTL-ref headers, unit tests) are translated; behaviour and results are unchanged.
- The native model at revision v4.6: OBP bias, scale and shift registers accept the full 32-bit range (negative
  bias values), OBP scale and shift tables are readable through the host bus, the Reduction Engine output is
  multiplexed onto the SRAM C write-back, and debug printing is off by default. The native `NpuTop` has no `o_irq`
  port. Native regression passes with a fresh build. The RTL-ref core and `HasNpuTop` do not use these files, and the
  reference results below are unchanged.

## 3. Reference results

All at 0.8 GHz with the HAS AXI-128 DMA, bit-exact against the integer golden. Details and the timing fidelity of each
block: `docs/PERFORMANCE_REPORT.md`, sections 5.4 to 5.6, and `docs/VERIFICATION_REPORT.md`, section 1.1.

| Workload | Command (data package as `$FE_WORK`) | Total cycles | Frame rate |
|---|---|---:|---:|
| YOLOv8m, hardware as drawn (run C) | `tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3` | 62,681,721 | 12.76 FPS |
| YOLOv8m, two proposed hardware options (run D) | `tools/has/tb_has_npu_top $FE_WORK/has/insts_pe4 --profile proposals` | 60,386,685 | 13.25 FPS |
| ViT-B/16, hardware as drawn | `tools/has/tb_has_npu_top $FE_WORK/has/vit_full` | 41,065,258 (1) | 19.48 FPS |
| ViT-B/16, bias-broadcast proposal | `tools/has/tb_has_npu_top $FE_WORK/has/vit_full --c-bcast` | 39,922,498 | 20.04 FPS |
| Quick check, two YOLOv8m instructions | `tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3 --first 40 --count 2` | 1,658,986 | |

Program files (data package):

| File | sha256 |
|---|---|
| `has/insts_pe3/mmio.txt` | `2297f5d12405ded5a7e454eba3c3a0029dd519187116dfc93278ea75a7ff0704` |
| `has/insts_pe3/dram_init.bin` | `12114574e544d725c57faeeffe6798a8e99d2ef5ae3f82ebd08ffd1700a48ae1` |
| `has/insts_pe3/dram_golden.bin` | `854e37553f0a05a902a675eaf0a81fad355977c8b8539a5cd4869c1f08554657` |
| `has/insts_pe4/mmio.txt` | `97aeaabaeeebc62bb74ae0a6ab85afc20dafcec6b91aa2f867338985bfef9c58` |
| `has/insts_pe4/dram_init.bin` | `cd958e854fa98853a52e3a92ea5072af7012d9ed8646522f35e9dc9da7be6be8` |
| `has/insts_pe4/dram_golden.bin` | `e17d1551ba40beab20cb8dd3680e490b7b343031e7f8adc68f4935aeb8df092e` |
| `has/vit_full/mmio.txt` | `48ebc2b2686c3d297807a688bef9d519f0f0b625c69fa85704e34a2b4c56017e` |
| `has/vit_full/dram_init.bin` | `22dee0aa5c118a163267cbebf08175069bc6fda2cdd9ee9e5f409bce5b166e66` |
| `has/vit_full/dram_golden.bin` | `a60236f1375d5852ed54a4d564a44a603fe01e6135dac9d0cd208ab19885b8e6` |
| `step3/pc_p9999_has/params.npz` (quantization parameters) | `aab3a947bec00446a192b481ae7cea0658b907a041ecd58d9ce33b88cb518cb7` |

The measured logs of these four whole-network runs, and of the YOLOv8m baseline before the planner and data-flow
options (112,912,950 cycles, 7.09 FPS; `docs/PERFORMANCE_REPORT.md`, section 5.3), are in `reference_runs/` of the
data package.

(1) Measured on coco128 image 000000000502 with an instruction stream identical to `vit_full` (timing does not depend
on data values; the first 23 instructions run on the shipped program give identical cycles, and a third image gives
the same total). The log is `reference_runs/vit_b16_as_drawn/` (`docs/VERIFICATION_REPORT.md`, V22 and V23).

## 4. How this release was checked

On Ubuntu 22.04 with g++ 11 and SystemC 2.3.3, from a fresh copy of the repository tree only (data extracted with
`tools/unpack_data.sh`):

| Check | Result |
|---|---|
| Repository tree: every file against `package_manifest.txt`; data package joined, checked and extracted by `tools/unpack_data.sh`; a git clone made with line-ending conversion requested still matches the manifest, and git tracks every shipped file | PASS |
| Build of the delivery testbench and 11 unit testbenches | PASS |
| Data package self-sufficiency: vector-unit tables exported from the shipped vectors, transformer check regenerated byte-identical | PASS |
| Unit tests: run profiles, register decoder, quantization, element-wise, DMA, attention, LayerNorm, softmax tables | PASS |
| Transformer instruction path (14 instructions) | PASS |
| Two-instruction YOLOv8m window, default profile | PASS, 1,658,986 cycles (equal to the reference run) |
| Native model regression (fresh build) | PASS |
| Python byte-compilation of `tools/` | PASS |
| YOLOv8m program regenerated by the packaged frontend from the data package | byte-identical to the shipped program |
| YOLOv8m baseline program regenerated (`fe_emit_insts.py --plan has`) | byte-identical to the program of the baseline reference run |
| Report tables regenerated by `tools/metrics/post_fullcore.sh` from the reference runs (YOLOv8m run C and baseline, ViT-B/16 as drawn), `has_rollup.py`, `fe_vit_top5.py` on the shipped ViT-B/16 program | PASS: figures equal to `docs/PERFORMANCE_REPORT.md`, section 5.7 |
| System TLM wrapper with the model root `core_rtl/` (wrapper built unchanged; with the DRAM-window change: whole YOLOv8m and ViT-B/16 programs) | PASS: 0 mismatches, per-instruction cycles equal to the reference runs |
| Wrapper revision that bundles the v4.6 native model: its unit test with this tree as native root and as `core_rtl/` root, unpatched and patched; the patch applies exactly; transformer check and first two YOLOv8m instructions through the patched wrapper | PASS: unit test output identical to the bundled model; cycles equal to the reference |
| Firmware engine `tools/has/wrapper/fw/` (`docs/VERIFICATION_REPORT.md`, V24): transformer check through the wrapper on the host; host steps and output CRCs of whole YOLOv8m and ViT-B/16 on the golden images; negative control; example compiled for RV32 | PASS. Not run on a CPU model inside a full platform |

The whole-network results of section 3 come from the measured runs whose logs are shipped; a full rerun takes about
10 hours (YOLOv8m) or 5 hours (ViT-B/16) on one core.

## 5. Not included

- Floating-point trained weights, ONNX exports and image datasets (third-party material): needed only to rerun the
  frontend from the start or to regenerate the ViT-B/16 program. See `THIRD_PARTY_NOTICES.md` and
  `docs/FRONTEND_GUIDE.md`. The data package does contain weights derived from these models: the quantized int8
  weights and biases of YOLOv8m and ViT-B/16 inside the program DRAM images (`dram_init.bin`) and the quantization
  parameters (`params.npz`).
- The case-driven demo pack of earlier native revisions (`npu_demo_clean`, `make demo`, `make check`): its testbench
  flow does not run on the current native model. The native regression (`tools/has/regress_native.sh`) and the unit
  testbenches of section 4 cover the native model instead.
- A C driver library for `HasNpuTop`: developed by the integrating team; the testbench
  `tools/has/tb_has_npu_top.cpp` shows the register sequence
  (`docs/SW_INTEGRATION_GUIDE.md`, section 4).

## 6. Known limitations and open points

- Timing of the DMA, the epilogue, the element-wise blocks and the vector-unit stages of the transformer instructions is
  modelled with parameters that stand in for open hardware questions (`docs/KNOWN_LIMITATIONS.md`, section 7;
  `docs/HAS_VALIDATION.md`). Only the core is cycle-accurate (`docs/VERIFICATION_REPORT.md`, section 1.1).
- The accuracy of the integer ViT-B/16 against the floating-point model is limited by the per-tensor LayerNorm
  quantization (`docs/KNOWN_LIMITATIONS.md`, Q4).
- Debug-only accessors and diagnostic macro blocks (all off by default) remain in the RTL-ref sources.

## 7. Integration notes

- The native top level (`npu_top.h`, class `NpuTop`) keeps the port list and register map used by existing
  integration wrappers: a wrapper built for the previous native model builds and runs unchanged against this tree.
  Point the wrapper's model root (for example a `SAURIA_NPU_ROOT` build variable) at the extracted source tree.
- The reference results of section 3 come from the delivery top level `HasNpuTop` (`tools/has/tb_has_npu_top`).
  Through a wrapper with the model root at the repository root, cycle counts and performance counters come from the
  native core, whose timing is not RTL-accurate, and are not comparable with section 3. With the model root
  `core_rtl/`, extended instructions run on `HasNpuTop` and their counters equal `tb_has_npu_top`
  (`docs/SW_INTEGRATION_GUIDE.md`, section 10).
- In the native top, instructions pushed through the instruction registers are executed by the functional decoder
  (`emulate_*()`): a whole network run through the native path gives the functional result, not RTL-accurate timing
  (`docs/KNOWN_LIMITATIONS.md`, N1).
- The Reduction Engine output multiplexer on the SRAM C write-back of the native top is covered by the build and the
  native regression; no test drives data through the Reduction Engine yet.
- Distribution: this package is provided for internal integration and evaluation. Do not redistribute it, or binaries
  built from it, without the approval of the rights owner of the SystemC implementation. The third-party terms that
apply to the ported SAURIA design and to the network weights inside the data package are in `THIRD_PARTY_NOTICES.md`,
section 4.
