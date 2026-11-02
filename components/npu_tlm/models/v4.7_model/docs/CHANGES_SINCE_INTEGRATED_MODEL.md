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
| How to recognise it | sha256 of its `package_manifest.txt` begins with `bbbc3776faab7b5b`; its `RELEASE_NOTES.md` gives `tools/has/regress_gvu.sh` 131 of 131 and does not mention `HAS_GVU_AVG` or `DMA_TENSOR` |
| Content | The previous delivery of this model (default = the vector-unit flow closest to the RTL) |
| Wrapper used for the checks of this package | `components/npu_tlm` at revision `edb33a1` (the same wrapper sources as `528007d`), built unchanged |

A tree that matches neither this description nor this package should be compared file by file with
`package_manifest.txt` of this package (`sha256sum -c package_manifest.txt` lists every file that differs). A tree
older than the integrated model is described by the `CHANGES_SINCE_INTEGRATED_MODEL.md` of the integrated model.

## 2. What changes for an integrator

| Area | With this package | Action |
|---|---|---|
| Results of programs | Unchanged: the values of the four operations on `GvuTop` equal the functional model on every lane (MEASURED, section 3) | None |
| Cycle counts in the default configuration | Changed where a program uses average pool, Softmax, the requant of FUSED_ATTN or LayerNorm: these now run on `GvuTop` by default and its cycles replace the functional estimate (ViT-B/16 41,582,468 to 41,244,030, MEASURED). YOLOv8m uses none of them: 62,422,935, unchanged (MEASURED). With the four operation knobs set to 0 the logs of the check programs are identical to the integrated model byte for byte, cycles included (MEASURED) | Update expected cycle counts, or set `HAS_GVU_AVG=0 HAS_GVU_SOFTMAX=0 HAS_GVU_FA=0 HAS_GVU_LN=0` to reproduce the integrated model |
| Register interface | One new opcode, `0x16` `DMA_TENSOR` (op COPY), and ELEM_WISE modes 6 (slice) / 7 (concat), accepted by default; with `HAS_DMA_TENSOR=0` / `HAS_DMA_ND=0` the push is refused with `UNSUPPORTED_OP` / `UNSUPPORTED_MODE` as before. No new register address (`DMA_TENSOR_MODE.md`, `DMA_SLICE_CONCAT.md`) | None: programs that do not use them run as before |
| Optional register | New extension register `TMAJ_C0` (`0x40000500`) and FUSED_ATTN `FLAGS` bit 2, used only with `HAS_OUT_TRANSPOSE=1` (off by default); the model now treats writes up to `0x40000503` as extended (`HW_RTL_ALIGNMENT.md`, section 8) | None unless the option is used; then the wrapper must forward `0x40000500` (`INTEGRATOR_QUICK_START.md`, section 2.1) |
| Error codes, report file | Unchanged | None |
| Wrapper | No change. The model root forwards a push of opcode `0x16` to `HasNpuTop` unless `HAS_DMA_TENSOR=0` is set in the environment of the wrapper process | Replace the model tree, rebuild |
| Vector-unit operations | New and on by default: average pool, Softmax rows, the requant of FUSED_ATTN and LayerNorm run on `GvuTop` as jobs on its clock (section 4, `GVU_OPS_ON_GVUTOP.md`); each knob set to 0 returns that operation to the functional model | None |
| Frontend | `tools/fe/fe_emit_insts.py --slice-concat dma_nd` writes the YOLOv8m program with every concat as ELEM_WISE mode 7 instructions (`insts_pe3_nd/`), `--slice-concat dma` as `DMA_TENSOR` COPY instructions (`insts_pe3_dma/`); the default (`alias`) is unchanged byte for byte (MEASURED) | None |
| Console output | New `[gvu-stream]` lines for average pool, Softmax, attention requant, LayerNorm and the jobs on the clock, printed when the matching knob is on (the default) | None |

## 3. Results and cycles

| Workload | Integrated model | This package, default | Label |
|---|---:|---:|---|
| YOLOv8m, default | 62,422,935, PASS 98 / 98 | 64,135,381, PASS 135 / 135 with `insts_pe3_nd` (concat by 4-D DMA descriptors, epilogue depth 11); `insts_pe3` at the epilogue depth 9: 62,422,935, PASS 98 / 98 (no average pool, Softmax or LayerNorm in YOLOv8m) | MEASURED |
| ViT-B/16, default | 41,582,468, PASS 244 / 244 | 41,244,621, PASS 244 / 244, 0 elements different (41,244,030 at the epilogue depth 9) | MEASURED |
| YOLOv8m, four operation knobs at 0 | 62,422,935 | 62,422,935 (predicted; the changed code is behind the four knobs) | MEASURED on the integrated model |
| ViT-B/16, four operation knobs at 0 | 41,582,468 | 41,582,468 (predicted) | MEASURED on the integrated model |
| YOLOv8m, the nine vector-unit knobs at 0 | 62,666,123 | 62,666,123 (predicted) | MEASURED on the integrated model |
| ViT-B/16, the nine vector-unit knobs at 0 | 41,680,289 | 41,680,289 (predicted) | MEASURED on the integrated model |
| Transformer check program, default knobs | 285,256 | 263,883 (263,816 with the epilogue depth 9) | MEASURED |
| Transformer check program, four operation knobs at 0 | 285,256 | 285,256 | MEASURED on this package; the log is identical byte for byte to the integrated model's |
| AVG_POOL check program, default knobs | 3,486 | 3,687 | MEASURED |
| AVG_POOL check program, four operation knobs at 0 | 3,486 | 3,486 | as the row above |

ViT-B/16 in the default configuration (MEASURED): 457 jobs on the clock of `GvuTop`, 0 failed, every lane equal to the
functional model; core busy cycles unchanged (24,852,624). The difference against the integrated model, -338,438 cycles
(DERIVED), is in the vector-unit operations: the cycles of `GvuTop` replace the estimate of the functional model. The
requant of FUSED_ATTN runs after the multiplication of the core and does not overlap with it. With the register file and
the clocked add off (`HAS_GVU_REGS=0 HAS_GVU_ELEM_CLK=0`) the three transformer operations run in the call style: transformer
check 242,711 cycles (MEASURED).

YOLOv8m with the COPY program: predicted 63,902,467 cycles (DERIVED: 62,422,935 plus 1,479,532 cycles of the 37 COPY
instructions, each MEASURED equal to the cycle formula of `DMA_TENSOR_MODE.md`); MEASURED: PASS 135 / 135, 0 elements
different, 63,902,431 cycles (1,479,496 cycles of the COPY instructions against 1,479,532 by the formula; the 36 cycles are
the poll step of the bench), core busy cycles equal to the default (55,653,442).

Slice and concat by 4-D DMA descriptors (`HAS_DMA_ND`, on by default): with the knob off the programs give the
same results and cycles (transformer check 263,883 both ways); with it on, the programs without modes 6 / 7 give the same cycles and
DRAM image as with it off (MEASURED). `tools/has/regress_gvu.sh`: 182 of 182 (MEASURED), its last item the verification by
instruction flow (`FLOW_VERIFICATION.md`): 67 of 67 steps PASS.

## 4. New knobs

| Knob | Default | What it changes | Results | Cycles | Label |
|---|---|---|---|---|---|
| `HAS_GVU_AVG` (with `HAS_GVU_TOP=3`) | 1 | ELEM_WISE AVG_POOL runs on `GvuTop` with the average pool data path of the RTL patch, through Reg 7 / 8 / 25 / 26; windows the pooling data path does not handle stay on the model | Equal to the functional model (AVG_POOL check 8 / 8, 1,104 lanes, 0 different) | `GvuTop` cycles replace the estimate | MEASURED |
| `HAS_GVU_SOFTMAX` | 1 | Softmax rows of FUSED_ATTN on `GvuTop` in groups of 32 rows, through Reg 4..6, with the stage-3 shift `E + q - P` (B20), where E is the exponent of the current table lookup, a shift of 0 included, and the vbank1 buffer bit flipped once on the edge (B21) | Equal (242,070 lanes, 0 different) | As above | MEASURED |
| `HAS_GVU_FA` (needs `HAS_GVU_SOFTMAX`) | 1 | Requant of the two attention products as GEMM_FUSED stream beats of `GvuTop`, one beat per edge after the multiplication of the core | Equal (327,958 lanes, 0 different) | Beats of the unit; not overlapped with the core | MEASURED |
| `HAS_GVU_LN` | 1 | LayerNorm with statistics per lane (proposal of the model team, `has/rtl_ref_gvu_ln_lanes.h`, one tick per edge) | Equal (174,080 lanes, 0 different) | ESTIMATE of the lane data path | MEASURED values, ESTIMATED cycles |
| `HAS_GVU_AVG_AS_RTL=1`, `HAS_GVU_SOFTMAX_AS_RTL=1` | 0 | Negative controls: rounding and narrowing, or stage-3 shift, of the delivered RTL | FAIL as expected (175 and 94,961 lanes different) | — | MEASURED |
| `HAS_GVU_LN_DUMP=<dir>` | unset | Writes every LayerNorm group as a text vector file for the hardware team | Unchanged | Unchanged | — |
| `HAS_DMA_TENSOR` | 1 | Accepts `DMA_TENSOR` COPY (opcode `0x16`); the data flow controller copies the block through the DMA read and write ports | COPY check program 25 / 25, reference tool MATCH 25 / 25 | Cycle formula of `DMA_TENSOR_MODE.md`, MEASURED equal on every COPY | MEASURED |
| `HAS_DMA_TENSOR_CHUNK=<bytes>` | 1,024 | Chunk of one COPY step (16 to 98,304) | Unchanged | Changes the overlap of read and write | MEASURED (128, 4,096) |
| `HAS_TRACE_DMA_TENSOR` | 0 | One line per COPY: bytes, chunk, cycles waiting for the idle DMA, cycles of the copy | Unchanged | Unchanged | MEASURED |
| `HAS_DMA_ND` | 1 | Slice / concat by 4-D DMA descriptors: ELEM_WISE modes 6 and 7 run by the DMA from DRAM to DRAM; tile window and write-back through descriptors (`DMA_SLICE_CONCAT.md`) | Check programs 27 / 27 and 10 / 10, reference tool MATCH | Bytes moved, no cost per row change (optimistic); programs without modes 6 / 7 unchanged | MEASURED |
| `HAS_GVU_REGS_NEO` | 0 | Register map of the vector unit of the `neo_lite` RTL run next to the model's register file; 14 writes per command | Unchanged | Check programs -67 to +792 | MEASURED |
| `HAS_GVU_VBANK1_DEPTH=<rows>` | 32 | Rows of vbank#1 (200 in the `neo_lite` RTL) | Unchanged on the check programs | Unchanged | MEASURED |
| `HAS_GVU_RESIDUAL_HW` | 0 | Residual add through the RESIDUAL operation of the hardware when expressible | Equal (0 differences against the RTL on 98,304 elements) | ADD faster (V + 13 instead of 2V + 11) | MEASURED |
| `HAS_DMA_DFC_1D` | 0 | Slice / concat as one-dimensional DFC commands with the measured cost | Equal | +1.0 % to x14 on check programs (O36) | MEASURED |
| `HAS_GVU_OUT_VBANK2` | 0 | GEMM_FUSED write-back charged as vbank2 -> DRAM | Equal | +0.9 % to +17.7 % on check programs | MEASURED |
| `HAS_BUS_STRICT` | 0 | Bus cost of the per-channel scale / shift and of the Softmax output A | Equal | 0 to +1,054 on check programs, attention check +31,490 | MEASURED |
| `HAS_OUT_TRANSPOSE` | 0 | Corner turn after the epilogue, channels from `TMAJ_C0` on token-major (proposal) | ViT-B/16 with V token-major 244 / 244 | +382 on the whole ViT-B/16 | MEASURED |

The lane counts are those of the check programs with one operation knob on at a time (`GVU_OPS_ON_GVUTOP.md`, section 6).

## 5. Tools and documents added or changed

| Item | Purpose |
|---|---|
| `has/gvu_top_stream.h`, `has/has_npu_top.h`, `has/gvu_regs_dfc.h`, `has/gvu_fused_attn.h`, `has/gvu_elemwise.h` | The four operations on `GvuTop` as jobs on the clock (`start_avgpool`, `start_softmax`, `start_requant`, `start_layernorm`; the data flow controller waits for the end of the job), their knobs, the hooks in FUSED_ATTN and ELEM_WISE, the average pool register sequence; `exec_dma_tensor` |
| `has/rtl_ref_gvu_top.h` | Average pool data path of the RTL patch (`GvuTopParams::avg_enable`) and the stage-3 shift input (`In::i_softmax_qp`), set by `HAS_GVU_AVG` and `HAS_GVU_SOFTMAX`; with `i_softmax_qp` the shift uses the exponent of the current table lookup |
| `has/rtl_ref_gvu_avgpool_pipe.h`, `has/rtl_ref_gvu_ln_lanes.h` | Average pool tail; LayerNorm with statistics per lane as a state machine ticked once per edge |
| `has/has_mmio_compat.h`, `core_rtl/npu_top.h` | Opcode `0x16` and its checks at the push; forwarding of the push in the model root when the option is on |
| `tools/has/tb_gvu_avgpool_pipe.cpp`, `tb_gvu_top_avgpool.cpp`, `ref_avgpool.py`, `target/test/avgpool/{tile,vec}/` | Average pool unit tests, the datasets of the vector-unit owner, an independent reference |
| `tools/has/tb_gvu_top_softmax_vit.cpp`, `tb_gvu_ln_lanes.cpp` | Softmax rows of ViT-B/16 on `GvuTop`; lane LayerNorm against the functional model |
| `tools/has/tb_gvu_top_maxpool_vectors.cpp`, `target/test/maxpool/{tile,vec}/` | Max pool on `GvuTop` against two fixed datasets of the vector-unit owner |
| `tools/has/make_dma_tensor_gate.py`, `tb_has_dma_tensor_isa.cpp`, `tools/has/ref_program.py` | COPY check program, instruction unit test, COPY in the independent reference |
| `tools/fe/fe_emit_insts.py` | `--slice-concat dma` and `dma_nd`, `--dram-limit` |
| `has/has_dma.h`, `has/has_mmio_compat.h`, `has/has_npu_top.h`, `tools/has/tb_has_npu_top.cpp` | 4-D DMA descriptors (`has::DmaNdDesc`, gather / scatter, chunked DRAM-to-DRAM copy), ELEM_WISE modes 6 / 7 and their push-time checks, tile window and write-back by descriptors, all behind `HAS_DMA_ND` (on by default); the COPY trace line behind `HAS_TRACE_DMA_TENSOR` |
| `tools/has/tb_has_dma.cpp`, `make_ref_gate_dma_slice_concat.py`, `make_dma_nd_big_gate.py`, `make_yolov8m_full_gate.py`, `tools/has/ref_program.py` | Descriptor unit test, slice / concat check programs, the YOLOv8m graph lowered with modes 6 / 7, modes 6 / 7 in the independent reference |
| `tools/has/verify_flows.py` | Verification by instruction flow: reads a regression run and writes one log per flow with every step, its checks, references and results (`FLOW_VERIFICATION.md`) |
| `has/rtl_ref_gvu_config_regs_neo.h`, `has/gvu_regs_neo_shadow.h`, `has/rtl_ref_gvu_residual_hw.h`, `has/dma_dfc_1d.h`, `has/gvu_out_transpose.h`; changed `has/rtl_ref_gvu_scratchpad.h` (vbank#1 depth), `has/gvu_reg_master.h`, `has/gvu_top_stream.h`, `has/rtl_ref_gvu_config_regs.h`, `has/gvu_elemwise.h`, `has/has_mmio_compat.h`, `has/has_npu_top.h`, `core_rtl/npu_top.h` | The options of `HW_RTL_ALIGNMENT.md` (all off by default) |
| `tools/has/tb_rtl_ref_gvu_config_regs_neo.cpp`, `tb_rtl_ref_gvu_residual_hw.cpp`, `tb_gvu_out_transpose.cpp`, `make_tmaj_gate.py`, `test_ref_tmaj.py`, `gvu_oracle/tb_oracle_cfgregs_neo.sv`; `tools/fe/fe_vit_block.py`, `fe_vit_full.py` (`--v-token-major`, `--out`) | Unit tests, check program and reference of the options; ViT-B/16 program with V token-major |
| `tools/has/tb_gvu_top_avgpool.cpp`, `tb_gvu_ln_lanes.cpp`, `tb_has_dma_tensor_isa.cpp`, `has/rtl_ref_gvu_ln_lanes.h` | Summary lines read by `verify_flows.py`: state sequence and done delay of average pool (mode `states`), LayerNorm tier by tier (mode `tiers`, read-only accessors of the lane machine), one line per push-time case of `DMA_TENSOR` |
| `tools/has/regress_gvu.sh` | 193 items (was 131; 182 before the options of `HW_RTL_ALIGNMENT.md`): the `HAS_DMA_ND` group, the Softmax stages on three heads, the benches of the vector-unit owner, `verify_flows.py`; the two max pool datasets, the average pool, Softmax and lane LayerNorm unit tests, the COPY group, the instruction-path runs of the four operations with their negative controls, the three transformer operations as jobs on the clock and in the call style. The script sets the four operation knobs to 0 for the earlier items, whose reference values are those of the integrated model |
| `docs/GVU_OPS_ON_GVUTOP.md`, `docs/DMA_TENSOR_MODE.md`, `docs/DMA_SLICE_CONCAT.md`, `docs/FLOW_VERIFICATION.md`, `docs/HW_RTL_ALIGNMENT.md` | New |
| `docs/GVU_DFC_REFERENCE_FLOW.md`, `GVU_TOP_CORE_INTEGRATION.md`, `GVU_TOP_IMPLEMENTATION_AND_TEST.md`, `SLICE_CONCAT_ON_GVU.md`, `KNOWN_LIMITATIONS.md`, `VERIFICATION_REPORT.md`, `CODE_MAP.md`, `BUILD_AND_RUN.md`, `HARDWARE_CONFIG.md`, `INTERFACE_SPEC.md`, `FRONTEND_GUIDE.md`, `USER_GUIDE.md`, `README.md` | Updated for the above |

## 6. Replacing the integrated model

1. Replace `components/npu_tlm/models/v4.7_model` with the source tree of this package (data not needed for the
   wrapper build). Keep the wrapper sources as they are.
2. Rebuild the wrapper with the model root `core_rtl/` as before (forwarding headers were added under `core_rtl/has/`).
3. Check with the wrapper's own test and with `tools/has/wrapper/test_npu_tlm_core_rtl.cpp` on the transformer check
   program of the data package: 14 of 14 instructions; cycles in the default configuration: 263,808, measured through the wrapper with the
   firmware engine (`tools/has/wrapper/fw/test_fw_replay.cpp`); 285,205 on the integrated model, which this package
   reproduces with the four operation knobs at 0.

## 7. Points that did not change

These points of the integrated model still hold: a refused push completes in the wrapper's
`STATUS` register (the refusal shows in `RICH_STATUS`, bit 2); pushing while an instruction runs corrupts data
(`KNOWN_LIMITATIONS.md`, O18); only the core is cycle-accurate.

## 8. Files that differ

Comparison of this package with the integrated model tree, line endings ignored, `data/` and `package_manifest.txt`
left out.

| Area | Changed | Added |
|---|---|---|
| `HasNpuTop` and the vector unit (`has/`) | `has_npu_top.h`, `has_mmio_compat.h`, `has_dma.h`, `gvu_top_stream.h`, `gvu_regs_dfc.h`, `gvu_fused_attn.h`, `gvu_elemwise.h`, `rtl_ref_gvu_top.h`, `gvu_reg_master.h`, `rtl_ref_gvu_config_regs.h`, `rtl_ref_gvu_scratchpad.h` | `rtl_ref_gvu_avgpool_pipe.h`, `rtl_ref_gvu_ln_lanes.h`, `rtl_ref_gvu_config_regs_neo.h`, `gvu_regs_neo_shadow.h`, `rtl_ref_gvu_residual_hw.h`, `dma_dfc_1d.h`, `gvu_out_transpose.h` |
| Model root `core_rtl/` | `npu_top.h` | forwarding headers of the new `has/` files |
| Tools and tests | `tools/has/regress_gvu.sh`, `tools/has/ref_program.py`, `tools/has/tb_has_dma.cpp`, `tools/has/tb_has_npu_top.cpp`, `tools/has/tb_has_mmio_compat.cpp`, `tools/has/wrapper/fw/npu_has_fw.h`, `tools/has/wrapper/fw/make_fw_program.py`, `tools/has/wrapper/test_npu_tlm_core_rtl.cpp` (extension range to `0x40000503`), `tools/has/gvu_oracle/frozen_headers.sha256`, `tools/fe/fe_emit_insts.py`, `tools/fe/fe_vit_block.py`, `tools/fe/fe_vit_full.py` | `tools/has/tb_gvu_avgpool_pipe.cpp`, `tb_gvu_top_avgpool.cpp`, `tb_gvu_top_softmax_vit.cpp`, `tb_gvu_ln_lanes.cpp`, `tb_gvu_top_maxpool_vectors.cpp`, `tb_has_dma_tensor_isa.cpp`, `ref_avgpool.py`, `make_dma_tensor_gate.py`, `make_ref_gate_dma_slice_concat.py`, `make_dma_nd_big_gate.py`, `make_yolov8m_full_gate.py`, `verify_flows.py`, `tb_rtl_ref_gvu_config_regs_neo.cpp`, `tb_rtl_ref_gvu_residual_hw.cpp`, `tb_gvu_out_transpose.cpp`, `make_tmaj_gate.py`, `make_fa_vtmaj_gate.py`, `test_ref_tmaj.py`, `gvu_oracle/tb_oracle_cfgregs_neo.sv`; `target/test/maxpool/`, `target/test/avgpool/` |
| Documents | `RELEASE_NOTES.md`; `docs/CHANGES_SINCE_INTEGRATED_MODEL.md` and the documents of section 5 | `docs/GVU_OPS_ON_GVUTOP.md`, `docs/DMA_TENSOR_MODE.md`, `docs/DMA_SLICE_CONCAT.md`, `docs/FLOW_VERIFICATION.md`, `docs/HW_RTL_ALIGNMENT.md` |
