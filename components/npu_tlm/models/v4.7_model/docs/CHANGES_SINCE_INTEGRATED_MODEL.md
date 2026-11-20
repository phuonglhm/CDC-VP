# Changes Since the Integrated Model

This document lists what differs between this package and the model revision that is currently built into the
system wrapper repository, and what an integrator has to expect when replacing one with the other. The release
notes (`RELEASE_NOTES.md`) describe the package as a whole; this document is the delta.

It covers the delivery path: `HasNpuTop` and the model root `core_rtl/`, that is, instructions pushed with extension
registers, and the register-file front end. The native model, which runs instructions pushed without extension
registers, is no longer developed and is not compared here.

Every number carries a label as in `VERIFICATION_REPORT.md`: **MEASURED** (from a run), **DERIVED** (computed from
measured numbers), **ESTIMATE**.

## 1. The model this document compares with

| Item | Value |
|---|---|
| Location in the system wrapper repository | `components/npu_tlm/models/v4.7_model` |
| How to recognise it | sha256 of its `package_manifest.txt` begins with `56d28439da65f182`; its `RELEASE_NOTES.md` gives `tools/has/regress_gvu.sh` 242 items, 242 of 242 pass, and it does not mention `HAS_DMA_OPGEN`, `HAS_TRANSPOSE_MODE`, `HAS_CPU_DIRECT` or RTL revision 2 |
| Content | The previous delivery of this model: the options that follow `neo_lite` RTL revisions 0 and 1, the run profiles `neo` / `neo_as_rtl`, the register-file front end |
| Wrapper | The wrapper sources are not changed by this package |

A tree that matches neither this description nor this package should be compared file by file with
`package_manifest.txt` of this package (`sha256sum -c package_manifest.txt` lists every file that differs). A tree
older than the integrated model is described by the `CHANGES_SINCE_INTEGRATED_MODEL.md` of the integrated model.

## 2. What changes for an integrator

| Area | With this package | Action |
|---|---|---|
| Results and cycles in the default configuration | Unchanged: every new behaviour is behind an option that is off by default (MEASURED: 10 check programs in both builds give identical logs; DERIVED for whole networks, which were not run again) | None |
| Register interface | Unchanged by default. With `HAS_CPU_DIRECT=1` and `HAS_NEO_REV=2`: register files of the SAURIA, GVU and DMA blocks at `0x11000`, `0x12000`, `0x13000` and a model-only SYNC block at `0x14000` (`CPU_DIRECT_GUIDE.md`); with `HAS_DMA_OPGEN=1` and `HAS_NEO_REV=2`: the DMA tensor register file at `0x40000A80` (`COMPILER_GUIDE.md`, section 7.6) | None unless the options are used |
| Wrapper | No change for the default. CPU-direct DMA on the virtual platform needs a change in the wrapper to copy data between system RAM and the model's DRAM (SYNC block; a proposed change of about 40 lines, `tools/has/wrapper/npu_tlm_cpu_direct_sync.patch`, tried on a copy of the wrapper) | None by default |
| Hardware alignment | `HAS_NEO_REV=2` selects RTL revision 2: core SRAM limits, register-file widths, vector-unit latencies, lookup table and stream depth; three new options of revision 2: DMA tensor register file, vertical scan of the core, CPU-direct programming (DMA, SAURIA runs with status and soft reset, max pool, RESIDUAL, Softmax, GEMM_FUSED); RESIDUAL through the DFC with the bias, vbank2 and timing of revision 2 (`HAS_GVU_ADD_RTL`, on from revision 2) (`HW_RTL_ALIGNMENT.md`, section 15) | None |
| Planner | `tools/fe/fe_emit_insts.py --sram rev2` re-plans the layers over the revision-2 limits; `--slice-concat opgen` writes slice / concat as DMA tensor operations. Without the options the programs are unchanged (MEASURED for `--sram`: 9 of 9 files of the two YOLOv8m programs byte-identical) | None |
| Run profile on the platform | `HAS_PROFILE=neo` or `neo_as_rtl` is read by the model root `core_rtl/` as by `tools/has/tb_has_npu_top` (in the previous package the model root ignored it, so through a wrapper only single `HAS_*` variables took effect); the variables of the profile are printed on stderr. Not set: unchanged | None |
| Console output | New lines only when the matching option is set: `[sram]` (a layer refused at revision 2), the summary lines of the vertical scan and of the CPU-direct path, one error line when a tile cannot run vertical | None |
| Removed | `control/core_adapter.h` and its test: dead code, not used by any build | None |

## 3. Results and cycles

The default configuration gives the results and cycles of the integrated model: the 10 check programs give identical
logs in both builds (MEASURED); whole networks were not run again (DERIVED from the identical logs, as every change is
behind an option that is off). `tools/has/regress_gvu.sh`: 287 items, 287 of 287 pass (MEASURED, 286 in one full run and one check rerun with the corrected script; the integrated model
has 242).

Numbers of the new options against an RTL simulation of revision 2 (`HW_RTL_ALIGNMENT.md`, section 15):

| Check | RTL revision 2 | Model | Label |
|---|---:|---:|---|
| Register map (82 words, DMA register files, platform bases) | — | 602 checks, 0 differences | MEASURED |
| DMA tensor register file, 11 groups of cases | bytes and segments | 0 differences; cycles −0.1 % to +1.6 % (one group +4.0 %) | MEASURED |
| Core run driven by the CPU, START to interrupt | 9,311 cycles | 9,345 cycles; 9,216 bytes equal | MEASURED |
| Max pool driven by the CPU, enable to done | 6,168 cycles | 6,168 cycles; 192 rows equal | MEASURED |
| RESIDUAL with bias driven by the CPU, enable to done | 80 cycles | 80 cycles; 64 rows equal | MEASURED |
| Vertical scan, mode 1 against RTL traces | — | 0 differences in data, rows and masks | MEASURED |
| Vector-unit lookup table | — | 0 differences over 300,000 cycles | MEASURED |
| Softmax stage times, rows of 8 and of 1 element | done at cycle 69 and 13 | the same | MEASURED |

Vertical scan, mode 3 (proposal of the model) on token-major check programs: the DRAM image equals the horizontal scan
on 5 of 7 programs, the other 2 refuse their unsupported tiles; cycles −0.25 % to −5.8 % against mode 0 (MEASURED).

## 4. New knobs

| Knob | Default | What it changes | Results | Cycles | Label |
|---|---|---|---|---|---|
| `HAS_NEO_REV=2` (new value) | 0 | The options follow RTL revision 2: SRAM limit check, register-file widths, vector-unit latencies, lookup table, stream depth 17, Softmax delay | A layer over a revision-2 limit is refused (40 of 83 layers of YOLOv8m with the plans of this package) | Softmax +3 per stage with the lookup table and +15 in stage 3; GEMM_FUSED stream +6 | MEASURED |
| `HAS_DMA_OPGEN`, `HAS_OPGEN_FLOOR` | 0, 0 | With `HAS_NEO_REV=2`: the DMA tensor register file (copy, fill, 2-D pad, upsample) programmed by the CPU; the floor keeps a descriptor at no less than one core cycle per beat | Unchanged when off | Descriptor cost of the RTL divided by 4.17 | MEASURED |
| `HAS_TRANSPOSE_MODE` | 0 | With `HAS_NEO_REV=2`: vertical scan of the core for token-major tiles; 1 = RTL as delivered, 2 = with the shadow-register fix, 3 = proposal of the model | Mode 3: same DRAM image as mode 0 | Mode 3: −0.25 % to −5.8 % on check programs | MEASURED |
| `HAS_CPU_DIRECT` | 0 | With `HAS_NEO_REV=2`: the CPU programs the SAURIA, GVU and DMA blocks through their register files | Same bytes as the DFC path | Same cycles as the DFC path | MEASURED |
| `HAS_CPU_DIRECT_NET` | 0 | With `HAS_CPU_DIRECT=1` and `HAS_CPU_DIRECT_DUMP=<dir>`: trace of the whole program run by the data flow controller, input of `mk_cpu_net.py` | Unchanged (observation only) | Unchanged | MEASURED |
| `HAS_GVU_ADD_EXACT` | 0 | An ADD whose sum needs no rounding maps onto RESIDUAL for every round mode | Unchanged bytes | Only ADDs refused before change (they run on RESIDUAL) | MEASURED |
| `HAS_CPU_DIRECT_DUMP=<dir>`, `HAS_CPU_DIRECT_SELFCHECK` | unset, 0 | Write the first core run and the first max pool to a directory (input of the example programs); check the decode of the 29 core words against the core configuration | Unchanged | Unchanged | MEASURED |

## 5. Tools and documents added or changed

| Item | Purpose |
|---|---|
| `has/sram_limits.h`, `has/rtl_ref_gvu_fu_r2.h`, `has/rtl_ref_gvu_lut_r2.h`, `has/rtl_ref_gvu_lut_sel.h` | SRAM limits per revision; vector unit of RTL revision 2 |
| `has/has_opgen.h`, `has/has_vert_cfg.h`, `has/has_cpu_direct.h` | DMA tensor register file; counter fields of a vertical-scan tile; CPU-direct register files and SYNC |
| `has/has_npu_top.h`, `has/has_mmio_compat.h`, `has/hw_capability.h`, `has/has_host_neo.h`, `has/has_host_neo_port.h`, `has/neo_rev.h`, `has/gvu_regs_dfc.h`, `has/gvu_top_stream.h`, `has/gvu_top_bridge.h`, `has/gvu_obp.h`, `has/rtl_ref_gvu_top.h`, `driver/libneo_cfg.h`, `core_rtl/npu_top.h` | Hooks of the new options; revision read at run time; register-file encoder per revision |
| `systolic_array/rtl_ref_sa_array.h`, `psm/rtl_ref_psm_top.h`, `psm/rtl_ref_psm_shift_fsm.h`, `rtl_ref_npu_top.h` | Vertical scan of the core (modes 1 to 3) |
| `tools/has/opgen_sim/`, `tools/has/cpu_direct/`, `tools/has/tb_vert_scan_bit.cpp`, `tb_vert_scan_sa.cpp`, `make_vgate.py`, `tb_has_sram_check.cpp`, `tb_rtl_ref_gvu_{depth_r2,depth_bridge_r2,latency,bdump_r2}.cpp`, `tb_gvu_top_softmax_trace.cpp`, `gvu_oracle/` (revision 2) | Benches and generators of the new options (`CODE_MAP.md`) |
| `tools/has/neo_regmap_check.py` (`--rev 2`), `tools/has/neo_regmap_rev2.txt`, `tools/has/hw_check_program.py` (`--rev 2`), `tools/fe/fe_emit_insts.py` (`--sram rev2`, `--slice-concat opgen`) | Register map and per-layer check of revision 2; planner options |
| `tools/has/regress_gvu.sh` | 287 items (was 282): the upsample by the DMA, the ADD on RESIDUAL, the CPU-direct chain and flows |
| `tools/has/cpu_direct/mk_cpu_net.py`, `run_cpu_direct.sh`, `evidence_cpu_direct.py`, `rtl_cpu_runs.txt`, `tools/has/verify_flows.py` (flows `CPU_*`) | Whole-network CPU program from the DFC trace; the CPU-direct checks alone with evidence sheets and the stored RTL results |
| `tools/fe/fe_emit_insts.py` (`--upsample dma`, `--r2-tile`) | Upsample as `DMA_TENSOR` mode 2; one layer re-planned with a given tile |
| `docs/CPU_DIRECT_GUIDE.md` (section 10), `FLOW_VERIFICATION.md` (section 7), `COMPILER_GUIDE.md` (sections 2.1, 7.2, 11), `KNOWN_LIMITATIONS.md` (O16, O44, O46, Q6, section 8.3), `BUILD_AND_RUN.md`, `CODE_MAP.md`, `VERIFICATION_REPORT.md` | The items above |
| `docs/CPU_DIRECT_GUIDE.md` | New |
| `docs/HW_RTL_ALIGNMENT.md` (section 15), `KNOWN_LIMITATIONS.md` (section 8), `COMPILER_GUIDE.md` (sections 2.1, 7.5, 7.6, 11), `PERFORMANCE_REPORT.md` (section 5.12), `BUILD_AND_RUN.md`, `CODE_MAP.md` | Revision 2 |

## 6. Replacing the integrated model

1. Replace `components/npu_tlm/models/v4.7_model` with the source tree of this package (data not needed for the
   wrapper build). Keep the wrapper sources as they are.
2. Rebuild the wrapper with the model root `core_rtl/` as before (forwarding headers were added under `core_rtl/has/`).
3. Check with the wrapper's own test and with `tools/has/wrapper/test_npu_tlm_core_rtl.cpp` on the transformer check
   program of the data package: 14 of 14 instructions; the integrated model gives 263,808 cycles in the default
   configuration with the firmware engine (`tools/has/wrapper/fw/test_fw_replay.cpp`), and this package is expected to
   give the same count (DERIVED: default unchanged).

## 7. Points that did not change

These points of the integrated model still hold: a refused push completes in the wrapper's `STATUS` register (the
refusal shows in `RICH_STATUS`, bit 2, and `RETIRED` does not grow); this holds for the register-file front end as
well; pushing while an instruction runs corrupts data (`KNOWN_LIMITATIONS.md`, O18); only the core is cycle-accurate.

## 8. Files that differ

Comparison of the source tree of this package with the integrated model tree, line endings ignored, `data/`, `docs/`
and the top-level documents left out; the exact list is the difference of the two `package_manifest.txt`.

| Area | Changed | Added | Removed |
|---|---|---|---|
| `HasNpuTop` and the vector unit (`has/`) | `gvu_obp.h`, `gvu_regs_dfc.h`, `gvu_top_bridge.h`, `gvu_top_stream.h`, `has_host_neo.h`, `has_host_neo_port.h`, `has_mmio_compat.h`, `has_npu_top.h`, `hw_capability.h`, `neo_rev.h`, `rtl_ref_gvu_top.h` | `has_cpu_direct.h`, `has_opgen.h`, `has_vert_cfg.h`, `rtl_ref_gvu_fu_r2.h`, `rtl_ref_gvu_lut_r2.h`, `rtl_ref_gvu_lut_sel.h`, `sram_limits.h` | — |
| Core ports | `systolic_array/rtl_ref_sa_array.h`, `psm/rtl_ref_psm_shift_fsm.h`, `psm/rtl_ref_psm_top.h`, `rtl_ref_npu_top.h` | — | `control/core_adapter.h` |
| Model root and driver | `core_rtl/npu_top.h`, `driver/libneo_cfg.h` | forwarding headers `core_rtl/has/` of the seven new `has/` headers | — |
| Build | `Makefile` | — | — |
| Tools | `tools/fe/fe_emit_insts.py`, `tools/has/hw_check_program.py`, `tools/has/neo_regmap_check.py`, `tools/has/regress_gvu.sh`, `tools/has/tb_gvu_regs_dfc.cpp`, `tools/has/tb_gvu_top_stream.cpp`, `tools/has/tb_has_npu_top.cpp`, `tools/trace_gvu_top.cpp`, ten harness files of `tools/has/gvu_oracle/` | `tools/has/opgen_sim/` (8 files), `tools/has/cpu_direct/` (9), `tools/has/gvu_oracle/` (4), `tools/has/neo_regmap_rev2.txt`, `make_vgate.py` and 8 benches of `tools/has/` | `tools/test_phase2_adapter_native_check.cpp` |
