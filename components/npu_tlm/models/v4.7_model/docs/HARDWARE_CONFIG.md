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
| Context handling | `FX1_A3_NO_CTX_REARM`, `FX1_A3_TILDONE_Q_GATE`, `FX1_A3_INCNTLIM_FIX`, `FX1_A3_CNTCLEAR_RTL` | Context re-arm, tile-done gating and counter-limit behaviour as in the RTL |
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
| Private staging area | 3 x 128 KB appended after the program's DRAM image, 4 KB aligned (models the 3-D DMA descriptors, questions H17 to H19) |

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
| `sp_banked` | 0: one 24 KB block, DMA and compute in series | 1 | Scratchpad as banks, each double-buffered against the DMA (vector-unit specification, section 4) |
| `sp_v0`, `sp_v1`, `sp_v2` | 4,096 / 4,096 / 16,384 bytes | same | Bytes per half of bank 0 (int8 vectors) and banks 1 and 2 (int32 vectors). Two halves: 48 KB in total. Size still open (question H8). Environment `HAS_SP_V0`, `HAS_SP_V1`, `HAS_SP_V2`; `HAS_SP_PRESET=gvu68k` sets 16,384 / 16,384 / 32,768 (MEASURED effect: ELEM_WISE ADD -2.5 % cycles, MAX_POOL +0.5 %) |
| `lat_obp` | 6 | 10 | Epilogue pipeline latency (cycles) |
| `lat_add1`, `lat_add2`, `lat_max` | 6, 5, 3 | 8, 10, 3 | ELEM_WISE stage latencies (cycles) |
| `elem_dma_lat` | 20 | 20 | Fixed cycles per ELEM_WISE transfer before the data (ESTIMATED; environment `HAS_ELEM_DMA_LAT`) |

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

