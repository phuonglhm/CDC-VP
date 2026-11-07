# Changes Since the Integrated Model

This document lists what differs between this package and the model revision that is currently built into the
system wrapper repository, and what an integrator has to expect when replacing one with the other. The release
notes (`RELEASE_NOTES.md`) describe the package as a whole; this document is the delta.

It covers the delivery path: `HasNpuTop` and the model root `core_rtl/`, that is, instructions pushed with extension
registers, and the new register-file front end. The native model, which runs instructions pushed without extension
registers, is no longer developed and is not compared here.

Every number carries a label as in `VERIFICATION_REPORT.md`: **MEASURED** (from a run), **DERIVED** (computed from
measured numbers), **ESTIMATE**.

## 1. The model this document compares with

| Item | Value |
|---|---|
| Location in the system wrapper repository | `components/npu_tlm/models/v4.7_model` |
| How to recognise it | sha256 of its `package_manifest.txt` begins with `d4a9d3e0833ed173`; its `RELEASE_NOTES.md` gives `tools/has/regress_gvu.sh` 193 items and a package check of 100 of 100, and does not mention `HAS_DMA_ND_MERGE`, `HAS_GVU_FA_OVERLAP` or `HAS_NEO_REV` |
| Content | The previous delivery of this model: default = the vector-unit flow closest to the RTL with 4-D DMA and COPY on; the options that follow neo_lite RTL revision 0 and the transpose after the epilogue present and off |
| Wrapper used for the checks of this package | `components/npu_tlm` at the wrapper revision of the previous release (the wrapper sources are unchanged in the current wrapper revision) |

A tree that matches neither this description nor this package should be compared file by file with
`package_manifest.txt` of this package (`sha256sum -c package_manifest.txt` lists every file that differs). A tree
older than the integrated model is described by the `CHANGES_SINCE_INTEGRATED_MODEL.md` of the integrated model.

## 2. What changes for an integrator

| Area | With this package | Action |
|---|---|---|
| Results and cycles in the default configuration | Changed only where a program uses slice / concat (`HAS_DMA_ND_MERGE` is on: fewer DMA pieces, same DRAM image) or writes `TMAJ_C0` (`HAS_OUT_TRANSPOSE` is on). The other options added by this package are off, except `HAS_DMA_RESHAPE` and `HAS_HOST`, which only add modes and a window that no existing program uses. With `HAS_DMA_ND_MERGE=0` and `HAS_OUT_TRANSPOSE=0` the logs of the check programs are identical to the integrated model byte for byte, cycles included (MEASURED, both builds, after each step of the work) | None |
| DFC command cost | The defaults of `HAS_DMA_DFC1D_FIRST` / `_CHAIN` / `_TAIL` are now 6 / 15 / 3 core cycles (before: 19 / 15 / 7, a single-clock measurement); they act only with `HAS_DMA_DFC_1D=1` or the profile `neo`, so no default run changes | Set the three variables to 19 / 15 / 7 to reproduce the integrated model with `HAS_DMA_DFC_1D=1` |
| Register interface | Unchanged for the instruction registers. New, on by default (`HAS_HOST=neo`): the register file of the `neo_lite` data flow controller at `0x40000800` to `0x400009FF` and opcode `0x17` as its doorbell (`NEO_REGISTER_FILE.md`). With `HAS_HOST=v45`, `0x17` is refused with `UNSUPPORTED_OP` as before | None unless the front end is used |
| Error codes, report file | Unchanged. A layer refused by the new front end reports the existing error `UNSUPPORTED_OP` in `STATUS` | None |
| Wrapper | No change. The model root forwards the new window to `HasNpuTop` unless `HAS_HOST=v45` is set in the environment of the wrapper process | Replace the model tree, rebuild |
| Hardware alignment | New options: RTL revision 1 of the `neo_lite` RTL (`HAS_NEO_REV`), core operand loads as DFC commands (`HAS_DFC_CORE_DMA`), the RTL register map of the vector unit as the functional one with its ping-pong buffers and clock-domain crossing, merged slice / concat pieces (`HAS_DMA_ND_MERGE`, on by default), attention requant overlapped with the core (`HAS_GVU_FA_OVERLAP`); all off except the merged pieces (section 4, `HW_RTL_ALIGNMENT.md`) | None |
| Run profiles and hardware check | `HAS_PROFILE=neo` / `neo_as_rtl` set every option to what RTL revision 1 has; `HAS_HW_CHECK=1` and `tools/has/hw_check_program.py` print per instruction whether the hardware runs it as written (`COMPILER_GUIDE.md`) | Use them to judge compiled programs for the hardware |
| Console output | New lines only when the matching option is set: `[profile neo]`, `[hw-check]`, `neo push refused`, `accepted under patch`, the counters of `HAS_DMA_DFC1D_REPORT` | None |

## 3. Results and cycles

| Workload | Integrated model | This package, default | Label |
|---|---:|---:|---|
| YOLOv8m `insts_pe3_nd`, default | 64,135,381, PASS 135 / 135 | 63,902,513 | MEASURED in both columns; with `HAS_DMA_ND_MERGE=0` this package gives 64,135,381 |
| ViT-B/16, default | 41,244,621, PASS 244 / 244 | 41,244,621 | MEASURED; unchanged (no slice / concat in ViT-B/16) |
| YOLOv8m `insts_pe3`, epilogue depth 9 | 62,422,935, PASS 98 / 98 | 62,422,935 | as the row above |
| Transformer check program, default | 263,883 | 263,883 | MEASURED on the integrated model; unchanged on the check programs (MEASURED) |

Whole networks with the new options on (MEASURED, PASS, 0 elements different; `PERFORMANCE_REPORT.md`, sections 5.10
and 5.11):

| Run | Cycles | Difference against the integrated model's default (or the reference named in the row) |
|---|---:|---:|
| YOLOv8m `insts_pe3_nd`, `HAS_DMA_ND_MERGE=1` (the default of this package) | 63,902,513 | −232,868 |
| ViT-B/16, `HAS_GVU_FA_OVERLAP=1` | 41,045,481 | −199,140 |
| YOLOv8m `insts_pe3_nd`, the six options of `HW_RTL_ALIGNMENT.md` sections 2 to 7 (RTL revision 0) | 69,843,642 | +5,708,261 |
| ViT-B/16, the same six options | 42,006,610 | +761,989 |
| YOLOv8m `insts_pe3`, profile `neo` (PASS 98 / 98; the cycles are a sum of seven windows, a continuous run is about 18,000 lower) | 65,371,733 | +2,948,798 (+4.7 %) against `insts_pe3` at the epilogue depth 9, 62,422,935 (the depth 11 of the default adds a few hundred cycles on ViT-B/16) |

`tools/has/regress_gvu.sh`: 242 items, 242 of 242 pass (MEASURED).

## 4. New knobs

| Knob | Default | What it changes | Results | Cycles | Label |
|---|---|---|---|---|---|
| `HAS_GVU_REGS_NEO` (changed) | 0 | The register map of the `neo_lite` RTL is now the functional register file of the vector unit (before: run next to the model's), with the ping-pong buffers switched on the enable edge (`HAS_GVU_NEO_PP`, 1 with the option) and the multiplexer and clock-domain crossing (`HAS_GVU_NEO_CDC_LAT`, 4; `HAS_GVU_NEO_WRITE_LAT`, 1) | Unchanged (10 of 10 check programs, 0 configuration and 0 buffer-select differences) | YOLOv8m-shaped check −262 / −262 / +536 in the three steps | MEASURED |
| `HAS_DMA_ND_MERGE` | 1 | Contiguous dimensions of a slice / concat descriptor merged before it is cut into DMA pieces (proposal) | Same DRAM image | Large slice / concat program 127,636 → 120,333; whole YOLOv8m −232,868 | MEASURED |
| `HAS_GVU_FA_OVERLAP` | 0 | The attention requant of one core pass runs on the vector unit during the next pass (proposal) | Unchanged | −1,385 per ViT-shaped head; whole ViT-B/16 −199,140 | MEASURED |
| `HAS_DMA_RESHAPE` | 1 | Upsample (nearest neighbour) and reshape of a DRAM tensor by the DMA: `DMA_TENSOR` modes 2 / 3 and the ELEM_WISE aliases modes 8 / 9; the transpose flag of a reshape is refused unless `HAS_DMA_RESHAPE_T=1` (default 0); no new register address; 0 refuses them with `UNSUPPORTED_MODE` (`COMPILER_GUIDE.md`, section 7.2) | Check program of 10 instructions PASS 10 of 10 (8,664 elements), reference tool MATCH 10 of 10; unit bench 69 of 69 | 881 cycles for the check program; programs that do not use the modes are unchanged | MEASURED (the cycles are model cycles; the hardware has no such operation) |
| `HAS_NEO_REV` | 0 | The `neo_lite` options follow RTL revision 1 instead of RTL revision 0 (`HW_RTL_ALIGNMENT.md`, section 14) | Unchanged | Re-opened DFC commands for output tiles not contiguous in DRAM, up to +13.3 % on the DMA-bound padding check program | MEASURED |
| `HAS_DMA_DFC1D_RESTART`, `HAS_DMA_DFC1D_REPORT` | 0, 0 | With `HAS_NEO_REV=1`: extra cycles per re-opened command (not measured); counters on stderr | Unchanged | Unchanged by the report | — |
| `HAS_DFC_CORE_DMA` | 0 | With `HAS_NEO_REV=1`: core operand loads charged as one-dimensional DFC commands | Unchanged | +0 and +201 on check programs | MEASURED |
| `HAS_PROFILE=neo`, `neo_as_rtl` (or `--profile`) | unset | Run profiles with only what RTL revision 1 has (`COMPILER_GUIDE.md`, section 5) | `neo`: unchanged where the program uses only hardware instructions, refused at slice / concat / COPY; `neo_as_rtl`: the arithmetic of the RTL, differs from the golden | Check programs: `PERFORMANCE_REPORT.md`, section 5.11 | MEASURED |
| `HAS_HW_CHECK` | 0 | One line per instruction: would RTL revision 1 run it as written, with reason codes | Unchanged | Unchanged | MEASURED |
| `HAS_HOST` (`neo`, `neo_patched`, `v45`); `HAS_HOST_NEO_PATCHES`, `HAS_HOST_NEO_QUIET` | `neo`; `A-H1,A-H4`, 0 | Second front end: the 82-word register file of the `neo_lite` DFC at `0x40000800`, doorbell `0x17` (`NEO_REGISTER_FILE.md`) | Same DRAM image as through the instruction registers | Same cycles | MEASURED |
| `HAS_HOST_TRACE=<file>` | unset | `tools/has/host_trace_gen`: writes the register trace of a program | — | — | — |

## 5. Tools and documents added or changed

| Item | Purpose |
|---|---|
| `has/gvu_regs_neo_shadow.h`, `has/rtl_ref_gvu_config_regs_neo.h`, `has/gvu_reg_master.h`, `has/gvu_regs_dfc.h`, `has/gvu_top_stream.h`, `has/gvu_top_bridge.h`, `has/rtl_ref_gvu_top.h`, `has/rtl_ref_gvu_residual_hw.h` | Register map of the RTL as the functional one (ping-pong, clock-domain crossing), overlapped attention requant, RTL revision 1 of the vector unit |
| `has/neo_rev.h`, `has/rtl_ref_gvu_pool_feeder_neo.h`, `has/rtl_ref_gvu_vlsu_pool_dense.h` | `HAS_NEO_REV`; pool feeder and dense max pool of RTL revision 1 |
| `has/dma_dfc_1d.h`, `has/dfc_core_dma.h`, `has/rtl_ref_dfc_tile_loop_neo.h`, `has/has_dma.h` | DFC command costs per revision, core operand loads as commands, port of the DFC tile pointer generator; merged descriptor pieces |
| `has/has_profile_neo.h`, `has/hw_capability.h`, `has/has_npu_top.h` | Run profiles, per-instruction hardware check, their hooks; cost of the vbank2 write-back without 4-D DMA |
| `driver/libneo_cfg.h`, `has/has_host_neo.h`, `has/has_host_neo_port.h`, `has/has_host_trace.h`, `has/has_mmio_compat.h`, `core_rtl/npu_top.h` | 82-word register image: encoder, decoder with reason codes, the front end `HAS_HOST=neo` and its routing in the model root, register traces |
| `tools/has/neo_regmap_check.py`, `tools/has/neo_regmap_rev1.txt`, `tools/has/tb_dma_upsample_reshape.cpp`, `tools/has/make_neo_window_gate.py` | Comparison of the register map with the RTL (`COMPILER_GUIDE.md`, section 1.1); frozen table of that map; unit bench of upsample and reshape; check programs for the register-file window |
| `tools/has/host_trace_gen.cpp`, `host_trace_replay.cpp`, `tb_has_host_neo.cpp`, `tb_has_neo_host.cpp`, `tb_core_rtl_neo.cpp`, `hw_check_program.py` | Traces, round trip, front-end benches, per-layer check without SystemC |
| `tools/has/tb_dma_dfc_1d_neo.cpp`, `tb_dfc_noncontig_neo.cpp`, `tb_dfc_core_dma_neo.cpp`, `tb_dfc_tile_loop_neo.cpp`, `tb_neo_top_program_hw.cpp`, `tools/has/neo_top_sim/vectors/`, `run_full_neo.sh` | Unit tests and counts of the DFC rules; hardware testbench programs through the model, with their max pool vectors |
| `tools/has/tb_has_profile.cpp`, `tb_has_npu_top.cpp`, `tb_rtl_ref_gvu_config_regs_neo.cpp`, `tb_gvu_top_maxpool.cpp`, `tb_gvu_top_maxpool_runs.cpp`, `gvu_oracle/chk_maxpool_runs.py`, `make_vit_heads_gate.py`, `tb_fa_requant_port.cpp`, `tb_rtl_ref_gvu_residual_tile_hw.cpp` | Profile modes, `--profile`, both revisions of the register map, max pool of RTL revision 1, ViT-shaped head checks, attention requant and residual tile against RTL vectors |
| `tools/has/regress_gvu.sh` | 242 items (was 193): the items of the options of section 4 |
| `docs/COMPILER_GUIDE.md`, `docs/NEO_REGISTER_FILE.md` | New |
| `docs/HW_RTL_ALIGNMENT.md` (section 14), `SW_INTEGRATION_GUIDE.md` (section 11), `INTERFACE_SPEC.md` (section 7), `TILING_GUIDE.md`, `FRONTEND_GUIDE.md` (section 13), `KNOWN_LIMITATIONS.md`, `PERFORMANCE_REPORT.md`, `VERIFICATION_REPORT.md`, `BUILD_AND_RUN.md`, `HARDWARE_CONFIG.md`, `CODE_MAP.md`, `README.md` | Updated for the above |

## 6. Replacing the integrated model

1. Replace `components/npu_tlm/models/v4.7_model` with the source tree of this package (data not needed for the
   wrapper build). Keep the wrapper sources as they are.
2. Rebuild the wrapper with the model root `core_rtl/` as before (forwarding headers were added under `core_rtl/has/`
   and `core_rtl/driver/`).
3. Check with the wrapper's own test and with `tools/has/wrapper/test_npu_tlm_core_rtl.cpp` on the transformer check
   program of the data package: 14 of 14 instructions; 263,808 cycles measured in the default configuration
   through the wrapper with the firmware engine (`tools/has/wrapper/fw/test_fw_replay.cpp`); the integrated model gives
   263,808.

## 7. Points that did not change

These points of the integrated model still hold: a refused push completes in the wrapper's `STATUS` register (the
refusal shows in `RICH_STATUS`, bit 2, and `RETIRED` does not grow); this holds for the new front end as well; pushing
while an instruction runs corrupts data (`KNOWN_LIMITATIONS.md`, O18); only the core is cycle-accurate.

## 8. Files that differ

Comparison of this package with the integrated model tree, line endings ignored, `data/` and `package_manifest.txt`
left out; the exact list is the difference of the two `package_manifest.txt`.

| Area | Changed | Added |
|---|---|---|
| `HasNpuTop` and the vector unit (`has/`) | `has_npu_top.h`, `has_mmio_compat.h`, `has_dma.h`, `dma_dfc_1d.h`, `gvu_reg_master.h`, `gvu_regs_dfc.h`, `gvu_regs_neo_shadow.h`, `gvu_top_bridge.h`, `gvu_top_stream.h`, `rtl_ref_gvu_config_regs_neo.h`, `rtl_ref_gvu_residual_hw.h`, `rtl_ref_gvu_top.h` | `neo_rev.h`, `has_profile_neo.h`, `hw_capability.h`, `dfc_core_dma.h`, `rtl_ref_dfc_tile_loop_neo.h`, `rtl_ref_gvu_pool_feeder_neo.h`, `rtl_ref_gvu_vlsu_pool_dense.h`, `has_host_neo.h`, `has_host_neo_port.h`, `has_host_trace.h` |
| Driver | — | `driver/libneo_cfg.h` |
| Model root `core_rtl/` | `npu_top.h` | forwarding headers of the new `has/` and `driver/` files |
| Tools and tests | `tools/has/regress_gvu.sh`, `tb_has_npu_top.cpp`, `tb_has_profile.cpp`, `tb_rtl_ref_gvu_config_regs_neo.cpp`, `tb_gvu_top_maxpool.cpp`, `tb_gvu_top_maxpool_runs.cpp`, `gvu_oracle/chk_maxpool_runs.py` | the tools of section 5; RTL vectors `tools/has/gvu_top_vectors/residual_tile_rev1/`, `tools/has/neo_top_sim/vectors/` |
| Repository files | `.gitattributes`, `.gitignore` | — |
| Documents | `RELEASE_NOTES.md`; the documents of section 5 | `docs/COMPILER_GUIDE.md`, `docs/NEO_REGISTER_FILE.md` |
