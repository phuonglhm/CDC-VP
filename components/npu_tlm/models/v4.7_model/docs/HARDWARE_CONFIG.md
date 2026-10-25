# Hardware Configuration

## 1. Configuration used for all published results

| Parameter | Value |
|---|---|
| Systolic array | 32 x 32 processing elements, 1,024 MACs |
| Data types | int8 activations and weights, int32 partial sums |
| Clock assumed for derived figures | 0.8 GHz |
| Ifmap SRAM (A) | 2 x 79 KB (ping-pong), 158 KB total |
| Weight SRAM (B) | 2 x 81 KB (ping-pong), 162 KB total |
| Partial-sum SRAM (C) | 2 x 96 KB, 192 KB total (768 rows of 128 bytes per buffer; the model instantiates 1,536 rows, see below) |
| Scratch | 24 KB per lane (native model). Vector-unit scratchpad of `HasNpuTop`: section 6.3 |
| OBP | 32 lanes, per-channel scale and shift, 256-entry LUT |
| Activation FIFO | 5 positions |
| Weight FIFO | 4 positions |
| DMA, v4.5 timing (testbench-driven runs, `PERFORMANCE_REPORT.md`, sections 3 and 4; `legacy` profile) | 4 read channels sharing one port (round-robin), 1 write port, 256-byte bursts in 8 cycles (32 bytes per cycle), no DRAM latency |
| DMA, HAS AXI-128 timing (`HasNpuTop` runs, `PERFORMANCE_REPORT.md`, sections 5.3 to 5.6; `recommended` and `proposals` profiles) | 128-bit AXI (16 bytes per beat), 8-beat bursts (128 bytes), channel 3 at low priority, DRAM latency 0 unless set. Section 6 |

The tile planner limits every tile to one buffer of each SRAM: A <= 79 KB, B <= 81 KB, C <= 96 KB, skip
<= 24 KB. See `FRONTEND_GUIDE.md`, section 6.2.

Model allocation versus specification:
- SRAM C of the core is instantiated with 1,536 rows per buffer, twice the specified 768.
- The planner never uses more than 768 rows, so results are unaffected.
- The build option `FX1_SRAM_CAP_CHECK` counts every access beyond the specified capacity.

## 2. Native model (`npu_top.h`)

| Parameter | Default | Set by |
|---|---|---|
| Array | 32 x 32 | `EVAL_X`, `EVAL_Y` (Makefile, per target). Some unit tests use 64 x 64 |
| Lane split | `Y_DIM / 2` rows to Lane A | `NSPLIT` register or `SET_NSPLIT` |
| OBP LUT | 256 entries per lane | |
| RCE LUTs | exp 256 B, recip 512 B, rsqrt 2,048 B per lane | |
| Scratch | 24 KB per lane | |
| Runtime profile | `PROFILE_V1_SAURIA` (SAURIA address generation); `PROFILE_V4_LINEAR` for legacy linear feeders | `CFG_PROFILE` register |

Data type variants:

| Variant | Input / output | Accumulator | Build flags |
|---|---|---|---|
| int8 (default) | 8-bit | 32-bit integer | none |
| fp16 | 16-bit | 32-bit float | `-DNPU_FP16 -DNPU_DTYPE_IN=fp16_t -DNPU_DTYPE_OUT=fp16_t -DNPU_DTYPE_PSUM=float` |
| int16 | 16-bit | 64-bit integer | `-DNPU_INT16 -DNPU_DTYPE_IN=int16_t -DNPU_DTYPE_OUT=int64_t -DNPU_DTYPE_PSUM=int64_t` |

## 3. RTL-ref core template parameters

`sauria_rtl::NpuTop<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, SRAMA_CAP, SRAMB_CAP, SRAMC_CAP, FIFO_DEPTH, PE_LAT, EXTRA_CSREG>`

The network testbenches use:

```
NpuTop<32, 32, int8_t, int8_t, int32_t, 79*1024, 81*1024, 1536, 16, 64, 1>
```

With the default build options:
- `SRAMA_CAP` and `SRAMB_CAP` are bytes per buffer.
- `SRAMC_CAP` is rows per buffer.
- `FIFO_DEPTH` is overridden by the separate activation and weight depths (5 and 4).

## 4. Build-time options of the RTL-ref core

Defined in `control/rtl_ref_defaults.h`. Each option is on by default unless stated otherwise. Each
reproduces a specific behaviour of the RTL. They exist as options so that each fix could be measured on its
own; turning one off breaks equivalence with the RTL.

| Group | Options | Effect |
|---|---|---|
| Controller structure | `FX1_A3_CONTEXT_FSM`, `FX1_A3_FEEDERS_FSM`, `FX1_A3_EXTRA_CSREG` | Controller built from the RTL context and feeder state machines |
| Context handling | `FX1_A3_NO_CTX_REARM`, `FX1_A3_TILDONE_Q_GATE`, `FX1_A3_WEI_DONE_Q_GATE`, `FX1_A3_INCNTLIM_FIX`, `FX1_A3_INCNTLIM_ZERO_RTL`, `FX1_A3_CNTCLEAR_RTL` | Context re-arm, tile-done gating and counter-limit behaviour as in the RTL. `FX1_A3_WEI_DONE_Q_GATE` gives the repetition counter the "done" and "tile done" pulses of the weight counter in the cycle of the RTL; without it the last context of some tiles is computed with zero weights. `FX1_A3_INCNTLIM_ZERO_RTL` keeps `INCNTLIM` = 0 (one multiply-accumulate per context) when `HasNpuTop` marks it with `i_mvm_k` = 0; without it such a tile is wrong. Both are corrections of this model, not defaults of the reference core model (`KNOWN_LIMITATIONS.md`, O23). Opt-out for comparisons: `-DFX1_A3_WEI_DONE_Q_GATE_OFF`, `-DFX1_A3_INCNTLIM_ZERO_RTL_OFF` |
| Same-cycle signalling | `FX1_A3_EMPTY_DIRECT`, `FX1_A3_SEAM_ORDER`, `FX1_A3_PIPE_EN_DIRECT`, `FX1_A3_VALID_DIRECT`, `FX1_A3_START_DIRECT`, `FX1_A3_FDFSM_DIRECT`, `FX1_A3_STALL_SAME_CYCLE`, `FX1_A3_FIFOFLAG_SAME_CYCLE`, `FX1_A3_PSM_START_DIRECT` | Combinational RTL paths are evaluated in the same cycle instead of through a one-cycle signal delay |
| Feeders | `FX1_A3_IFMAP_FEEDER_RTL`, `FX1_A3_WEI_FEEDER_RTL`, `FX1_A3_FIFO_DEPTH_SPLIT` | RTL feeder implementations; separate FIFO depths (5 and 4) |
| SRAM | `FX1_A3_SRAM_CAP_BYTES`, `FX1_A3_SRAM_CAP_C_ROWS`, `FX1_A3_SRAMA_RDEN_PHASE`, `FX1_A3_SRAMB_RDEN_PHASE` | Capacity units and read-enable timing |
| Partial-sum manager | `FX1_A3_PSM_SHIFT_FSM`, `FX1_A3_PSM_WDATA_MGR`, `FX1_A3_PSM_REAL_PRELOAD`, `FX1_A3_PSM_INACTIVE_COLS`, `FX1_A3_PSM_SEAM_ORDER`, `FX1_A3_PSM_CTX_OWNCNT`, `FX1_A3_WRADDR_Q4` | RTL shift state machine, write-data manager, preload and inactive-column handling |
| Scan chain | `FX1_A3_CSCAN_*`, `FX1_A3_CARR_*`, `FX1_A3_CSDELAY_MODE` (3) | Array scan-chain capture and context-switch delay |
| Index widths | `SAURIA_ACT_IDX_W`, `SAURIA_WEI_IDX_W` (18) | Address counter widths |

Options that are not on by default:

| Option | Effect |
|---|---|
| `FX1_NO_PERF` | Removes all performance hooks (set by the default build scripts) |
| `FE_METRICS` | Per-tile metrics CSV in the network testbench (requires building without `FX1_NO_PERF`) |
| `FX1_A3_SRAM_BACKDOOR_LOAD` | Cycle-free SRAM load and read helpers for testbenches |
| `FX1_SRAM_CAP_CHECK` | Counts SRAM accesses beyond the specified capacity |
| `SAURIA_DEBUG` | Debug printing; 0 by default (the build scripts also pass 0). Set 1 for per-cycle prints, about 30 % slower, same results |

## 5. HAS block options

See `ARCHITECTURE.md`, section 5. The HAS program stores its option values in the `FEHP` header, so the
program and the testbench always agree.

| Configuration | Round | Narrow | Scale format | Use |
|---|---|---|---|---|
| Compatibility | FLOOR | SAT16 | U32 | Reproduces the standard results byte for byte |
| HAS | HALF_UP | SAT16 | I32 | Arithmetic as specified in the drawings |

The knob values of the `has::Knobs` structure (`has/gvu_quant.h`) are set from the environment by
`has::Knobs::from_env()`: `HAS_MODE=compat|has` selects one of the two rows above, and `HAS_ROUND_MODE`,
`HAS_REQ_NARROW`, `HAS_DEQ_ZP_ORDER`, `HAS_SCALE_FMT`, `HAS_MP_MODE`, `HAS_SCRATCH_BYTES` override single knobs
(value encoding in `has/HAS_IFACE.md`, section 2).

## 6. Delivery top level `HasNpuTop`

Source: `has/has_npu_top.h`, `has/has_dma.h`, `has/gvu_quant.h`. Usage: `BUILD_AND_RUN.md`, section 6.

### 6.1 Fixed configuration

| Parameter | Value |
|---|---|
| Core | `sauria_rtl::NpuTop<32, 32, int8_t, int8_t, int32_t, 79*1024, 81*1024, 1536, 16, 64, 1>`, as section 3 |
| Instruction queue | 16 entries (constructor argument `q_depth`) |
| OBP | `has::HasObp<32>`, host registers as `INTERFACE_SPEC.md`, section 4.2 |
| Core cycle limit per pass | 2,000,000 cycles (`core_max_cycles`) |
| Private staging area | 128 KB (input window of a tile) and 2 x 192 KB (preload and read-back of the partial sums, the size of the modelled SRAM-C), appended after the program's DRAM image, 4 KB aligned (models the 3-D DMA descriptors, questions H17 to H19) |

### 6.2 DMA (`has::DmaParams`)

| Field | v4.5 timing (`DmaParams::v45()`) | HAS AXI-128 (`DmaParams::has_axi128(latency)`) |
|---|---|---|
| `bytes_per_beat` | 32 | 16 |
| `burst_beats` | 8 | 8 |
| `dram_latency` (cycles before the first burst of a transfer) | 0 | argument, default 0 (`--dram-lat N`) |
| `ch3_low_priority` (LUT, bias, scale on channel 3) | false (round-robin) | true |

Channels: CH0 weights, CH1 input window, CH2 write-back, CH3 bias preload and tables.

### 6.3 Vector-unit scratchpad and pipeline latencies (`has::Knobs`)

| Knob | Default (`legacy`) | `recommended`, `proposals` | Meaning |
|---|---|---|---|
| `scratch_bytes` | 24,576 | 24,576 | Scratchpad of the ELEM_WISE blocks |
| `sp_banked` | 0: one 24 KB block, DMA and compute in series | 1 | Scratchpad as banks, each double-buffered against the DMA (vector-unit specification, section 4; micro-architecture specification: 128 KB, 64 KB per half) |
| `sp_v0`, `sp_v1`, `sp_v2`, `sp_s` | 4,096 / 4,096 / 16,384 bytes, scalar bank not limited | 10,784 / 5,600 / 43,008 bytes and 6,144 bytes (65,536 bytes per half) | Bytes per half of vector bank 0 (int8), banks 1 and 2 (int8 or int32 by stage) and the scalar bank. The micro-architecture specification gives the total (128 KB, double-buffered) and a capacity table of typical contents, not the size of each bank; this split is a model choice that holds 14 LayerNorm rows of 768 channels and ELEM_WISE blocks of 5,600 elements (question H8). Environment `HAS_SP_V0`, `HAS_SP_V1`, `HAS_SP_V2`, `HAS_SP_S`; presets `HAS_SP_PRESET=mas128k_bal8` (this split), `mas128k_bal` (10,784 / 7,904 / 43,008 / 3,840), `mas128k` (the typical column of the capacity table: 24,576 / 1,024 / 32,768 / 2,560), `gvu68k` (16,384 / 16,384 / 32,768) |
| `sp_limit_rows` | 0: 32 rows per group | 1 | Softmax and LayerNorm rows per group limited by the bank sizes (`has/gvu_sp_map.h`): 32 rows of 197 scores, 14 rows of 768 channels with the split above. Environment `HAS_SP_LIMIT_ROWS` |
| `lut_mas_depth` | 0: 257 entries per segment | 1 | Reciprocal table 256 entries, inverse-square-root table 512 entries; the address of the second interpolation point is held at the last entry (functional-unit RTL). Bit-identical results on both networks. Environment `HAS_LUT_MAS_DEPTH` |
| `lut_zero0` | 0: largest value | 1 | LUT input 0 returns 0 (functional-unit RTL). Environment `HAS_LUT_ZERO0` |
| `lut_stages` | 4 | 4 | Pipeline stages of the reciprocal LUT in the softmax cycle estimate: 4 in the RTL with its default parameters, 6 in the micro-architecture specification. Environment `HAS_LUT_STAGES` |
| `sp_s_word32` | 0: 5 bytes per LayerNorm channel | 1 | Every scalar parameter takes one int32 word of the scalar bank (8 bytes per LayerNorm channel), as the scalar-bank port of the RTL. Environment `HAS_SP_S_WORD32` |
| `zp_add_wrap16` | 0 | 0 | 1 = the zero-point adder of the requantization wraps at 16 bits as in the RTL, instead of saturating. Off by default: reported to the hardware team as a possible defect. No effect on either network. Environment `HAS_ZP_ADD_WRAP16` |
| `round_mode` | half up | half up | Floor, half up, half away, half even, ceil, toward zero, away from zero (`HAS_ROUND_MODE` 0 to 6); the last three were added from the micro-architecture specification |
| `lat_obp` | 6 | 10 | Epilogue pipeline latency (cycles) |
| `lat_add1`, `lat_add2`, `lat_max` | 6, 5, 3 | 8, 10, 3 | ELEM_WISE stage latencies (cycles) |
| `elem_dma_lat` | 20 | 20 | Fixed cycles per ELEM_WISE transfer before the data (ESTIMATED; environment `HAS_ELEM_DMA_LAT`) |

`HAS_SPEC` selects these knobs as a group for the `recommended` and `proposals` profiles: unset or `mas` = the
column above (`Knobs::spec_default()`); `v47ref` = the values of the `legacy` column for every knob from `sp_v0` to
`sp_s_word32` (the configuration of the results measured before the micro-architecture specification: 4,096 / 4,096 /
16,384 bytes, 32 rows per group, 257-entry tables); `mas10` = `v47ref` plus the preset `mas128k`, rows limited, 6 LUT
stages and the 256 / 512 tables; `rtl` = `v47ref` plus the 256 / 512 tables, LUT input 0, the wrapping zero-point
adder and the int32 scalar words. A single `HAS_*` variable overrides the group. A default-constructed `has::Knobs`
(unit tests, the `legacy` profile) keeps the `legacy` column.

The `recommended` values of the latencies are derived from the stage counts of the vector-unit drawing
(`Knobs::gvu_latencies()`); the `legacy` values are the pipeline cuts of the HAS drawings. Both are ESTIMATED
(`PERFORMANCE_REPORT.md`, section 6).

### 6.4 Run profiles (`has::Profile`)

| Option (member of `HasNpuTop`, testbench flag) | `legacy` | `recommended` (default) | `proposals` |
|---|:-:|:-:|:-:|
| Overlapped ping-pong schedule (`set_mode(true, ...)`, `--overlap`) | | x | x |
| HAS AXI-128 DMA timing (`--dma has`) | | x | x |
| Epilogue on the PSM -> SRAM-C write path (`set_obp_inline(true)`, `--obp-inline`) | | x | x |
| Banked scratchpad and vector-unit latencies (`--sp-banked`, `--gvu-lat`) | | x | x |
| 3-D DMA descriptors (`desc3d`, `--desc3d`) | | x | x |
| Per-layer tile order (`tile_order_auto`, `--s4`) | | x | x |
| FUSED_ATTN products on the core (`rce_core`, `--rce-core`) | | x | x |
| Proposal: bias preload by broadcast descriptor (`c_bias_bcast`, `--c-bcast`) | | | x |
| Proposal: halo reuse in SRAM-A (`halo_reuse`, `--halo`; copy rate `halo_copy_bpc` = 32 bytes per cycle, ESTIMATED) | | | x |

`has::HasNpuTop npu("npu")` selects `recommended`; `has::HasNpuTop npu("npu", has::Profile::Proposals)` another
profile. The constructor that takes a `has::Knobs` value leaves every option off, for tools that set options one
by one. The options change timing only, with one exception: a program with an input-channel split (`TILE_CIN`,
for example `insts_pe3`) needs the epilogue on the partial-sum path and fails under `legacy`.

### 6.5 Convolution padding by the IFMAP feeder (`feeder_pad`)

| Option | Default | Meaning |
|---|---|---|
| `feeder_pad` (member of `HasNpuTop`; environment `HAS_FEEDER_PAD`) | 0 | 0: the data flow controller loads the input window of a tile with its padding into SRAM-A and the core runs a convolution without padding. 1: SRAM-A receives only the part of the window inside the image and the IFMAP feeder inserts the padding value while it gathers |
| `HAS_PAD_VALUE` (environment) | not set | Forces one padding value for every convolution instead of `PAD_VALUE` of the instruction. For experiments on existing programs |

With `feeder_pad` the feeder of the RTL-ref core has logic that the SAURIA RTL does not have (new design of this
model, no hardware description exists for it yet):

- SRAM-A holds `[Cin][hr][wr]`, the rows and columns of the window inside the image, behind a lead of
  `pt x wr + pl` unused elements (`pt` / `pl` = padding rows above / columns left inside this tile's window). With
  the lead, the address counters need no offset; their row and channel steps use `wr` and `hr`. With dilation the
  kernel-row step is `wr x DILATION`; the comparators work on element positions and need no change.
- Four configuration registers of the core, written by the data flow controller before the start pulse of a core
  pass and cleared by the core reset (not written by software, not part of the instruction interface):

  | Core register (offset in the configuration region) | Content |
  |---|---|
  | `ACT_PAD_CTRL` (`0x448`) | bit 0: enable; bits 15:8: padding value (int8) |
  | `ACT_PAD_COL` (`0x44C`) | bits 15:0: first valid window column; bits 31:16: one past the last valid column |
  | `ACT_PAD_ROW_LO` (`0x450`) | first valid window row, multiplied by the row step `wr` |
  | `ACT_PAD_ROW_HI` (`0x454`) | one past the last valid window row, multiplied by `wr` |

- The 32 lanes of the feeder gather from one stream of SRAM-A words. Every element of that stream whose window
  position lies outside the valid rows or columns is replaced by the padding value before the lanes; the lanes,
  their FIFOs and the rest of the core are unchanged. No pipeline stage is added.
- The values of one tile come from `has/has_pad_gen.h` (comparators and five multiplications; combinational, like
  the other per-tile configuration values of the model it costs no cycle).

Cost: four register writes, 8 cycles, per core pass with padding. Results are the same as with `feeder_pad` = 0
(`VERIFICATION_REPORT.md`, V31). Tiles that keep the staged flow also with `feeder_pad` = 1: layers with a 1x1
kernel and stride 1 (their address mode reads one SRAM word per group of positions and needs rows that are a
multiple of 32 elements), tiles whose window lies entirely in the padding, and tiles whose values do not fit the
registers (more than 65,535 columns). Whether the comparators fit a clock period has not been evaluated.
`IFMAP_FEEDER_PADDING.md` describes the design at the level of an RTL implementation.

### 6.6 Vector unit on its top level (`HAS_GVU_TOP`)

| Option | Default | Meaning |
|---|---|---|
| `HAS_GVU_TOP` (environment) | 3 | 0: the functional vector unit of the previous releases. 1: the GEMM epilogue (stream mode of GEMM_FUSED, nine stages) and the residual add of the instruction path run on the vector-unit top level `GvuTop` (`has/rtl_ref_gvu_top.h`, socket `has/gvu_top_bridge.h`); max pool also runs on `GvuTop` (a shape `GvuTop` does not have goes per channel to the cycle-level max pool pipeline); values are compared with the functional model and a `[gvu-top]` table is printed at exit. 2: as 1, and the activation table of a layer costs a DMA of 256 bytes plus 5 cycles (`GvuTop`'s own path) instead of 4,096 host-port cycles. 3: level B, `GvuTop` on the core clock connected by the PSM-to-GVU stream (`GVU_TOP_CORE_INTEGRATION.md`, sections 4.2 and 5) |
| `HAS_GVU_STALL` (environment, with `HAS_GVU_TOP=3`) | 1 | 1: the core stops on `o_stream_stall` as `psm_top.sv` does (`GVU_TOP_CORE_INTEGRATION.md`, section 2.5); 0: the stall is computed and counted only |
| `HAS_GVU_ELEM_CLK` (environment, with `HAS_GVU_TOP=3`) | 1 | 1: residual add and max pool run as jobs on the clock of `GvuTop` instead of the call style; same values and cycles (`GVU_TOP_CORE_INTEGRATION.md`, sections 2.6, 2.7) |
| `HAS_GVU_REGS` (environment, with `HAS_GVU_TOP=3`) | 1 | 1: the data flow controller configures and starts `GvuTop` only through the register file (`GVU_DFC_REFERENCE_FLOW.md`); implies `HAS_GVU_ELEM_CLK=1` |
| `HAS_GVU_REGS_ADD` (environment) | 1 | 0: the residual add stays outside the register file |
| `HAS_GVU_REG_LAT` (environment) | 4 | Cycles of one register write seen by the data flow controller, minimum 2 (ESTIMATED) |
| `HAS_GVU_AGU_ND` (environment) | 1 | 1: identity AGU_ND programs for the residual add and the max pool; 2: shifted programs for demonstration; 0: off (`SLICE_CONCAT_ON_GVU.md`) |
| `-DGVU_GEMM_STREAM_DEPTH=<n>` (build) | 9 | Stages of the `GvuTop` GEMM epilogue, 9 to 16; 9 as the specification, 11 as the hardware RTL measures (pure delay stages) |
| `HAS_GVU_TOP_DUMP=<file>` (environment, with `HAS_GVU_TOP` 1 or 3) | not set | One line per epilogue beat; changes no value and no cycle (`GVU_TOP_CORE_INTEGRATION.md`, section 1.3) |
| `HAS_OBP_DUMP=<file>` (environment, with `HAS_OBP_RTL=1`) | not set | Writes every activation table and epilogue beat to the file for `tools/has/tb_gvu_top_replay.cpp`; changes no value and no cycle |

With `HAS_GVU_TOP=0 HAS_GVU_STALL=0 HAS_GVU_ELEM_CLK=0 HAS_GVU_REGS=0 HAS_GVU_AGU_ND=0` the model computes and counts what the previous releases did. The other vector-unit options (`HAS_ELEM_RTL`,
`HAS_OBP_RTL`, `HAS_ARITH_RTL`, `HAS_RANGE_PROBE`, `HAS_PSM_PROBE`) are described in `GVU_RTL_CONFORMANCE.md`,
section 4.10. Results: `VERIFICATION_REPORT.md`, V35; limits: `KNOWN_LIMITATIONS.md`, O25.
