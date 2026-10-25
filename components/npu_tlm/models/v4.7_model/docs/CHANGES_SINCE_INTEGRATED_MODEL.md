# Changes Since the Integrated Model

This document lists what differs between this package and the model revision that is currently built into the
system wrapper repository, and what an integrator has to expect when replacing one with the other. The release
notes (`RELEASE_NOTES.md`) describe the package as a whole; this document is the delta.

It covers the delivery path: `HasNpuTop` and the model root `core_rtl/`, that is, instructions pushed with extension
registers. The native model, which runs instructions pushed without extension registers, is no longer developed and
is not compared here.

Every number carries a label as in `VERIFICATION_REPORT.md`: **MEASURED** (from a run), **DERIVED** (computed from
measured numbers).

## 1. The model this document compares with

| Item | Value |
|---|---|
| Location in the system wrapper repository | `components/npu_tlm/models/v4.7_model` |
| How to recognise it | sha256 of its `package_manifest.txt` begins with `63d40df028250da2`; its `RELEASE_NOTES.md` lists "Padding documents" in section 2 and does not mention `HAS_GVU_TOP=3` |
| Content | The previous delivery of this model, unchanged |
| Wrapper used for the checks of this package | `components/npu_tlm` at revision `528007d`, built unchanged |

A tree that matches neither this description nor this package should be compared file by file with
`package_manifest.txt` of this package (`sha256sum -c package_manifest.txt` lists every file that differs). A tree
older than the integrated model is described by the `CHANGES_SINCE_INTEGRATED_MODEL.md` of the integrated model.

## 2. What changes for an integrator

| Area | With this package | Action |
|---|---|---|
| Results of programs | Unchanged | None |
| Cycle counts in the default configuration | Changed: the default now runs the vector-unit flow closest to the RTL (section 4); with the five vector-unit knobs at 0 they are unchanged (YOLOv8m 62,666,123, ViT-B/16 41,680,289, transformer check 285,205: section 3) | A script or wrapper that compares cycles with the integrated model sets `HAS_GVU_TOP=0 HAS_GVU_STALL=0 HAS_GVU_ELEM_CLK=0 HAS_GVU_REGS=0 HAS_GVU_AGU_ND=0` |
| Register interface, error codes, report file | Unchanged | None |
| Wrapper | No change | Replace the model tree, rebuild |
| Vector-unit options | New, and on by default: the vector unit on its top level `GvuTop` connected to the core on the core clock (`HAS_GVU_TOP=3`), the core stopping on the back-pressure of the vector unit (`HAS_GVU_STALL=1`), residual add and max pool on the clock (`HAS_GVU_ELEM_CLK=1`), the data flow controller driving the unit through its register file (`HAS_GVU_REGS=1`), the multi-dimensional address generator (`HAS_GVU_AGU_ND=1`); build and run parameters of `GvuTop` (section 4) | None to use them; set them to 0 to get the results and cycles of section 3 |
| Console output | New report lines `[gvu-stream] ...` at exit whenever `HAS_GVU_TOP=3` (the default) | None |

## 3. Results and cycles with the vector-unit knobs at 0

These are the results and cycles of the integrated model; this package gives them with the five vector-unit knobs at 0. The
default of this package (the flow of section 4) gives YOLOv8m 62,422,935 and ViT-B/16 41,582,468 cycles (`VERIFICATION_REPORT.md`, V38).

| Workload | Integrated model | This package | Label |
|---|---:|---:|---|
| YOLOv8m, hardware as drawn | 62,666,123, PASS 98 / 98 | 62,666,123, PASS 98 / 98 | MEASURED on the integrated model; DERIVED for this package (below) |
| ViT-B/16, hardware as drawn | 41,680,289, PASS 244 / 244 | 41,680,289, PASS 244 / 244 | MEASURED on the integrated model; DERIVED for this package (below) |
| Two-instruction YOLOv8m window (window D) | 1,658,986 | 1,658,986 | MEASURED |
| YOLOv8m instructions 86 to 88 | 280,003 | 280,003 | MEASURED |
| Transformer check program, vector-unit cycles | 285,205 | 285,205 | MEASURED |

Every change of this package that touches the paths of every instruction (the write hook of the partial-sum manager, the
core port, `HasNpuTop`) is behind one of the options of section 4; with them at 0 the regression
(`tools/has/regress_gvu.sh`, 131 of 131) shows the windows above unchanged, and the whole YOLOv8m with the clocked unit and
no register file gives 62,420,669 cycles, equal to the earlier run cycle for cycle (`VERIFICATION_REPORT.md`, V36 to V38).

## 4. Vector-unit options (on by default since this package)

Each line can be switched off by setting the option to 0; the label column refers to the measurement named.

| Option | What it changes | Results | Cycles | Label |
|---|---|---|---|---|
| `HAS_GVU_TOP=3` | `GvuTop` runs on the core clock behind a stream tap of the partial-sum manager with the signals of the RTL PSM-to-GVU stream (FIFO of 4 packets, `valid` / `ready`, `oc_last` / `oc_tile_last`); every GEMM_FUSED epilogue beat goes through it and its int8 result is written back into SRAM-C. Residual add and max pool run on the same unit (call style). `GVU_TOP_CORE_INTEGRATION.md`, section 5 | Equal to the functional model: whole YOLOv8m 98 / 98, whole ViT-B/16 244 / 244, 0 lanes different | YOLOv8m 62,420,669; ViT-B/16 41,581,353 (continuous runs) | MEASURED |
| `HAS_GVU_STALL=1` (with `HAS_GVU_TOP=3`) | The core stops on `o_stream_stall` as `psm_top.sv` of the hardware RTL: a write of the partial-sum manager that finds the stream FIFO full is blocked and the whole core holds until there is room | Equal to `HAS_GVU_TOP=3` without it | Equal when the FIFO never fills (YOLOv8m windows: 0 stall cycles); with the ready of the unit held low, the core holds the measured number of cycles | MEASURED |
| `HAS_GVU_TOP=1` or `2` | As before (call-style connection), and max pool now runs on `GvuTop` (`POOLING_MAX`, port of the hardware RTL) instead of the model's cycle-level pipeline; a shape `GvuTop` does not have stays on that pipeline | Equal to the functional model (3 SPPF max pools of YOLOv8m, 0 different) | SPPF max pool 184,824 `GvuTop` cycles per instruction | MEASURED |
| `HAS_REQ_NARROW=1` | Already present for the functional model (wrap to 16 bits after the round-shift instead of saturating); `GvuTop` now follows it in the GEMM_FUSED epilogue | Equal to the functional model with the same option | — | MEASURED (unit test) |
| `HAS_GVU_ELEM_CLK=1` (with `HAS_GVU_TOP=3`) | Residual add and max pool run as jobs on the clock of `GvuTop` (one tick per edge) instead of the call style | Equal to the call style | Equal (check programs; YOLOv8m windows of 3 SPPF and 2 residual adds: difference 0 cycles) | MEASURED |
| (no option) GEMM_FUSED zero point of `GvuTop` | 8 bits as the GEMM_FUSED register of the hardware RTL, sign-extended; a zero point outside int8 keeps the functional-model result (counted as not expressible) | Equal on both networks (zero point 0) | — | MEASURED (unit test, check program) |
| `-DGVU_GEMM_STREAM_DEPTH=<n>` (build) | Depth of the GEMM_FUSED stream pipeline of `GvuTop`; 9 (default) follows the register cuts of the specification, 11 is the depth the hardware RTL measures | Unchanged (pure delay stages) | The epilogue tail grows with the depth | MEASURED (unit test) |
| `HAS_GVU_REGS=1` (with `HAS_GVU_TOP=3`; implies `HAS_GVU_ELEM_CLK=1`) | The data flow controller configures and starts `GvuTop` only through the register file of `gvu_core` (`has/rtl_ref_gvu_config_regs.h`, 32 registers with the proposals), one write costing `HAS_GVU_REG_LAT` cycles (default 4, ESTIMATED); a GEMM_FUSED layer ends on the end-of-tile flag of the stream; residual add through the proposed Reg 9..15 (`HAS_GVU_REGS_ADD=0` keeps it outside) | Equal: YOLOv8m 98 / 98 in windows, 0 elements different, 83 of 83 layers ended by the flag | +1,819 cycles over the run without it (sum of 91 instructions in windows); residual add +0 | MEASURED |
| `HAS_GVU_AGU_ND=1` | Residual add and max pool address the Scratchpad through identity AGU_ND programs (`docs/SLICE_CONCAT_ON_GVU.md`); 2 = shifted programs, for demonstration | Equal to 0 (check programs, 112 output files identical) | Equal; through the register file 5 to 16 more writes per program | MEASURED |

`HAS_GVU_STREAM_HOLD` and `HAS_GVU_STALL=2` are test-only options of the regression (they hold the ready of the unit low,
or compute the stall without applying it).

## 5. Tools and documents added or changed

| Item | Purpose |
|---|---|
| `has/gvu_top_stream.h` | The clocked connection (`PsmStreamTap`, `GvuTopStream`) |
| `has/rtl_ref_gvu_pool_feeder.h` | Pooling feeder of the vector unit, ported from the hardware RTL (frozen header) |
| `tools/has/tb_gvu_top_stream.cpp`, `tb_gvu_top_params.cpp`, `tb_gvu_top_maxpool.cpp` | Unit tests of the clocked connection (mode `elem`: residual add and max pool on the clock), of the two parameters and the 8-bit zero point (mode `zp`), and of max pool on `GvuTop` |
| `tools/has/gvu_oracle/tb_oracle_maxpool.sv`, `run_verilator_maxpool.sh`, `fifo_v3_standin.sv` | Cycle-by-cycle check of max pool against the hardware RTL with Verilator |
| `tools/has/tb_gvu_top_maxpool_runs.cpp`, `tools/has/dump_maxpool.cpp`, `tools/has/gvu_oracle/tb_oracle_maxpool_runs.sv`, `chk_maxpool_runs.py`, `gen_sppf_sets.py` | Max pool of a whole layer against the hardware RTL, runs back to back, and the SPPF vector sets of YOLOv8m (`VERIFICATION_REPORT.md`, V39) |
| `tools/has/regress_gvu.sh` | 131 items (was 58): adds the load/store-unit self-checks and the RTL trace digest, the zero-point test, the clocked residual add / max pool, and the register-file group (register file against the RTL, write sequences, AGU_ND, check programs of 6 and 9 instructions through the register file); sets the five vector-unit knobs to 0 for its reference values |
| `has/rtl_ref_gvu_config_regs.h`, `has/gvu_reg_master.h`, `has/gvu_regs_dfc.h` | Register file of `gvu_core`, the controller's AXI-Lite write port, the write sequences |
| `has/gvu_agu_nd.h`, `has/gvu_agu_nd_vlsu.h`, `has/gvu_agu_nd_move.h` | AGU_ND, the load / store unit with AGU_ND around the frozen unit, the MOVE operation |
| `tools/has/tb_rtl_ref_gvu_config_regs.cpp`, `tb_gvu_regs_dfc.cpp`, `tb_gvu_agu_nd.cpp`, `tb_gvu_agu_nd_move.cpp`, `make_ref_gate_mp.py`; `tools/has/gvu_oracle/tb_oracle_cfgregs.sv` and its scripts | Their tests, the 9-instruction check program, the register-file oracle against the RTL |
| `docs/GVU_DFC_REFERENCE_FLOW.md` (with `docs/figures/gvu_dfc_reference_flow.svg`), `docs/SLICE_CONCAT_ON_GVU.md` | New: the recommended flow for the RTL; slice and concat on the vector unit |
| `docs/GVU_TOP_CORE_INTEGRATION.md` | Rewritten for the hardware team: every flow of the unit in the core including the register file, the model against the RTL with file and line on both sides, the pitfalls met and how they were solved, results |
| `docs/GVU_TOP_IMPLEMENTATION_AND_TEST.md` | Updated: interface and states of `GvuTop` against the RTL, stream mode, max pool |

## 6. Replacing the integrated model

1. Replace `components/npu_tlm/models/v4.7_model` with the source tree of this package (data not needed for the
   wrapper build). Keep the wrapper sources as they are.
2. Rebuild the wrapper with the model root `core_rtl/` as before (forwarding headers were added under `core_rtl/has/`).
3. Check with the wrapper's own test and with `tools/has/wrapper/test_npu_tlm_core_rtl.cpp` on the transformer check
   program of the data package: 14 of 14 instructions, 285,205 cycles with the five vector-unit knobs at 0 (section 2).

## 7. Points that did not change

These points of the integrated model still hold: a refused push completes in the wrapper's
`STATUS` register (the refusal shows in `RICH_STATUS`, bit 2); pushing while an instruction runs corrupts data
(`KNOWN_LIMITATIONS.md`, O18); only the core is cycle-accurate.

## 8. Files that differ

Comparison of this package with the integrated model tree, line endings ignored, `data/` and `package_manifest.txt`
left out.

| Area | Changed | Added |
|---|---|---|
| RTL-ref core | `rtl_ref_npu_top.h`; `control/rtl_ref_context_fsm.h`, `rtl_ref_feeders_fsm.h`, `rtl_ref_main_controller.h`; `psm/rtl_ref_psm_idxcnt.h`, `rtl_ref_psm_shift_fsm.h`, `rtl_ref_psm_top.h`; `sram/rtl_ref_sram_top.h` | |
| `HasNpuTop` and the vector unit (`has/`) | `has_npu_top.h`, `gvu_elemwise.h`, `gvu_obp.h`, `gvu_top_bridge.h`, `rtl_ref_gvu_top.h`, `rtl_ref_gvu_fu.h`, `rtl_ref_gvu_vlsu.h` | `gvu_top_stream.h`, `rtl_ref_gvu_pool_feeder.h`, `rtl_ref_gvu_config_regs.h`, `gvu_reg_master.h`, `gvu_regs_dfc.h`, `gvu_agu_nd.h`, `gvu_agu_nd_vlsu.h`, `gvu_agu_nd_move.h` |
| Model root `core_rtl/` | | forwarding headers of the new `has/` files |
| Tools and tests | `tools/has/regress_gvu.sh`, `tb_rtl_ref_gvu_replay.cpp`, `gvu_oracle/frozen_headers.sha256` | `tools/has/tb_gvu_top_stream.cpp`, `tb_gvu_top_params.cpp`, `tb_gvu_top_maxpool.cpp`; `tools/has/gvu_oracle/tb_oracle_maxpool.sv`, `run_verilator_maxpool.sh`, `fifo_v3_standin.sv`; `tools/has/tb_gvu_top_maxpool_runs.cpp`, `dump_maxpool.cpp`; `tools/has/gvu_oracle/tb_oracle_maxpool_runs.sv`, `chk_maxpool_runs.py`, `gen_sppf_sets.py` |
| Documents | `RELEASE_NOTES.md`; `docs/CHANGES_SINCE_INTEGRATED_MODEL.md`, `CODE_MAP.md`, `HARDWARE_CONFIG.md`, `GVU_TOP_CORE_INTEGRATION.md`, `GVU_TOP_IMPLEMENTATION_AND_TEST.md`, `KNOWN_LIMITATIONS.md`, `VERIFICATION_REPORT.md` and the run guides | `docs/GVU_DFC_REFERENCE_FLOW.md`, `docs/SLICE_CONCAT_ON_GVU.md`, `docs/figures/gvu_dfc_reference_flow.svg` |
