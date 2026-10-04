# Software Integration Guide

This guide is for teams whose driver or compiler software was written against the memory-mapped protocol of
the previous hand-over (`control/instruction_decoder.h`). It describes how that software talks to the delivery
top level, `HasNpuTop`, what stays the same, and what must change.

## 1. Status

| Component | File | Status |
|---|---|---|
| Compatibility layer: decodes the v4.5 protocol and extension registers into layer instructions | `has/has_mmio_compat.h` | Implemented and unit-tested (`tools/has/tb_has_mmio_compat`, 10 of 10 groups); connected in `HasNpuTop` |
| Tile iterator: expands one layer instruction into the tiles of the offline plan | `has/has_tile_iter.h` | Implemented and unit-tested; connected in `HasNpuTop`. Reproduces 9,811 of 9,811 YOLOv8m tiles exactly (output region, padded input window, order, active rows) |
| Replay check: whole YOLOv8m written as register writes and decoded back | `tools/has/tb_has_mmio_replay` | 98 instructions, 2,306 register writes; 9,811 of 9,811 tiles and 15 of 15 element-wise steps identical; no status errors |
| Data flow controller (instruction queue, tile loop, DMA, core, OBP, element-wise sequencing) | inside `has/has_npu_top.h` | Implemented: sequential and overlapped schedules, verified on network windows (`VERIFICATION_REPORT.md`, V8, V9); options of `ARCHITECTURE.md`, section 8 |
| DMA with the HAS bus parameters and true ping-pong loading | `has/has_dma.h` | Implemented; verified against the v4.5 DMA cycle for cycle |
| `HasNpuTop`: same ports as `NpuTop`, compatibility layer + data flow controller + RTL-ref core + HAS blocks. The default constructor selects the recommended run profile (`BUILD_AND_RUN.md`, section 6) | `has/has_npu_top.h` | Implemented; whole-network acceptance run in `VERIFICATION_REPORT.md`, V12 |
| FUSED_ATTN (one attention head per instruction) and LAYERNORM (a block of rows per instruction) | `has/has_npu_top.h`, `has/gvu_rce_params.h` | Implemented; unit and instruction-path checks (`VERIFICATION_REPORT.md`, V17) and a whole ViT-B/16 (V18). The softmax, LayerNorm and DMA timing of these instructions is ESTIMATED |
| C driver API (`npu_gemm_fused`, `npu_elem_add`, `npu_maxpool`, `npu_wait`) | not included | Developed by the integrating team; the testbench `tools/has/tb_has_npu_top.cpp` shows the register sequence |
| Firmware starting point: C register definitions, a replay engine for whole programs, a bare-metal example | `tools/has/wrapper/fw/` | Engine checked on the host through the wrapper; example compiled for RV32 (section 10.5) |
| Reference compiler backend: YOLOv8m program to register writes and a matching DRAM image (SAURIA-ordered weights, slice and concat as address mapping); ViT-B/16 program | `tools/fe/fe_emit_insts.py`, `tools/fe/fe_vit_full.py` | Done; accepted by the replay check (98 instructions, 2,306 writes, 9,811 of 9,811 tiles, 15 of 15 element-wise steps, no errors). Two upsample steps remain host operations pending H15 |
| System TLM wrapper integration: drop-in model root and proposed wrapper change | `core_rtl/`, `tools/has/wrapper/` | Whole YOLOv8m and ViT-B/16 through the wrapper equal `tools/has/tb_has_npu_top` instruction by instruction (`VERIFICATION_REPORT.md`, V20); section 10 |

The acceptance milestone of the delivery top level is a full YOLOv8m run (83 GEMM_FUSED and 15 ELEM_WISE
instructions) driven only by memory-mapped instructions, bit-exact against the HAS golden, with detection boxes
identical to the golden.

## 2. What stays the same

- **Addresses:** the gather registers `0x40000400` to `0x40000468` and the push register `0x40000310`.
- **Programming model:** write the registers of one instruction, then push its opcode.
- **Opcodes:** `0x12` GEMM_FUSED, `0x13` FUSED_ATTN, `0x14` LAYERNORM and `0x15` ELEM_WISE.
- **Granularity:** one instruction per layer. The hardware iterates over the tiles.
- **Register persistence:** the legacy registers keep their value between pushes, as before.

## 3. Register map

### 3.1 Legacy registers

| Address | Name | Handling |
|---|---|---|
| `0x40000300`, `0x40000304` | 64-bit instruction low / high | Not supported: writing the high word reports error `LEGACY64` |
| `0x40000310` | `PUSH_A` | Push opcode (bits 7:0) |
| `0x40000314` | `PUSH_B` | Not supported (no second lane): reports `LANE_B` |
| `0x40000400` ... `0x4000040C` | `IN_ADDR`, `W_ADDR`, `OUT_ADDR`, `BIAS_ADDR` | Unchanged meaning (DRAM byte addresses) |
| `0x40000410` ... `0x40000418` | `M`, `K`, `N` | Used only when no geometry extension is written: the layer is a 1x1 convolution (linear / matrix product) over `M` positions, with a default tiling |
| `0x4000041C`, `0x40000420` | `KH`, `KW` | Unchanged |
| `0x40000424`, `0x40000428` | `STRIDE`, `PAD` | Unchanged; one value for both directions, symmetric padding |
| `0x4000042C` | `ACT_TYPE` | 0 none, 1 ReLU (table built internally), 2 SiLU, 3 GELU. 2 and 3 require `LUT_ADDR` |
| `0x40000430`, `0x40000434` | `HAS_SKIP`, `SKIP_ADDR` | Split internally into GEMM_FUSED followed by an ELEM_WISE ADD with unit scales |
| `0x40000438` ... `0x40000440` | `IN_SCALE`, `W_SCALE`, `OUT_SCALE` (IEEE-754 bits) | Used only when `SCALE_ADDR` is not written: converted to one integer multiplier and shift for all channels. Approximate path |
| `0x40000444`, `0x40000448` | `A_ADDR`, `B_ADDR` | ELEM_WISE operands; FUSED_ATTN Q and K |
| `0x4000044C` | `V_ADDR` | FUSED_ATTN V |
| `0x40000450` | `LEN` | ELEM_WISE ADD element count; FUSED_ATTN sequence length L (rows of K and V); LAYERNORM row length H |
| `0x40000454` | Mode pack | Element-wise mode in bits 31:24 (legacy single-field write also accepted) |
| `0x40000458`, `0x4000045C`, `0x40000460` | Scale A, scale B, output scale (IEEE-754 bits) | ELEM_WISE ADD scales when the integer extension is not written. Approximate path. For FUSED_ATTN, `0x40000458` holds the head dimension D as an integer |
| `0x40000464`, `0x40000468` | `A_LEN`, `B_LEN` | Must equal `LEN` (no broadcast in the HAS ADD); otherwise error `BROADCAST` |

### 3.2 Status registers (new)

| Address | Name | Access | Content |
|---|---|---|---|
| `0x40000318` | `STATUS` | R; any write clears the error | bit 0 busy · bit 1 queue full · bit 2 error (sticky) · bits 15:8 queued instructions · bits 31:16 last error code |
| `0x4000031C` | `RETIRED` | R | Number of completed instructions |
| `o_irq` | port | | Level signal. High when the queue is empty and no instruction is executing, once at least one instruction has completed since reset. Low again when a new instruction is pushed or while `i_rstn` is low (there is no separate soft-reset input). There is no acknowledge or mask register (pending H20). Read `RETIRED` to know how many instructions have completed |

Error codes:

| Code | Name | Cause |
|---|---|---|
| 1 | `Q_OVF` | Push while the queue is full (default depth 16); the instruction is dropped |
| 2 | `LANE_B` | Push to the second lane |
| 3 | `LEGACY64` | 64-bit instruction interface used |
| 4 | `UNSUPPORTED_OP` | Opcode other than `0x05`, `0x12`, `0x13`, `0x14`, `0x15` |
| 5 | `UNSUPPORTED_MODE` | Element-wise mode other than ADD (0), MAX_POOL (1) and AVG_POOL (5), or activation type above 3 |
| 6 | `BROADCAST` | ADD with operands of different lengths |
| 7 | `NEED_LUT` | SiLU or GELU without `LUT_ADDR` |
| 8 | `BAD_GEOM` | Inconsistent or missing geometry |
| 9 | `SCALE_RANGE` | A floating-point scale cannot be represented as an integer multiplier and shift |
| 10 | `NO_TILING` | Tile sizes missing or zero, or the layer does not fit the default tiling |

`SET_NSPLIT` (`0x05`) is accepted and ignored; the number of such pushes is counted.

### 3.3 Extension registers (new)

Address = `0x4000046C + 4 x index`. The last register is at `0x400004E4`.

The extension registers are **one-shot**: they are cleared after every push. A later push that does not write
them therefore never picks up stale geometry.

| Index | Address | Name | Used by | Meaning |
|---|---|---|---|---|
| 0 | `0x4000046C` | `IN_C` | GEMM, MAX_POOL | Input channels |
| 1 | `0x40000470` | `IN_H` | GEMM, MAX_POOL | Input height |
| 2 | `0x40000474` | `IN_W` | GEMM, MAX_POOL | Input width |
| 3 | `0x40000478` | `OUT_C` | GEMM | Output channels |
| 4 | `0x4000047C` | `OUT_H` | GEMM | Output height |
| 5 | `0x40000480` | `OUT_W` | GEMM | Output width |
| 6 | `0x40000484` | `TILE_COUT` | GEMM | Tile output channels (from the offline tile plan) |
| 7 | `0x40000488` | `TILE_H` | GEMM | Tile output height |
| 8 | `0x4000048C` | `TILE_W` | GEMM | Tile output width |
| 9 | `0x40000490` | `SCALE_ADDR` | GEMM | DRAM address of the per-channel signed 32-bit multipliers |
| 10 | `0x40000494` | `SHIFT_ADDR` | GEMM | DRAM address of the per-channel shifts |
| 11 | `0x40000498` | `LUT_ADDR` | GEMM | DRAM address of the 256-entry activation table |
| 12 | `0x4000049C` | `ZP_OUT` | GEMM | Output zero point (0 for symmetric quantization) |
| 13 | `0x400004A0` | `FLAGS` | GEMM | Bit 0 `PAD_TAIL`: edge tiles are computed at the full `TILE_COUT` channels and at a width rounded up to a multiple of the positions per context (`Y_USED`, or 32 when not written); only the real channels and positions are written back. Requires the compiler to zero-pad the weights of the last output-channel band to `TILE_COUT` (section 6.1). Bit 1 `CHANNEL_MAJOR` (FUSED_ATTN, LAYERNORM): operands and outputs are stored channel-major, element (token i, channel j) at `address + j x rows + i`, the layout a GEMM_FUSED over tokens writes. Other bits reserved, write 0 |
| 14 | `0x400004A4` | `ZP_A` | ADD | Zero point of A |
| 15 | `0x400004A8` | `ZP_B` | ADD | Zero point of B |
| 16 | `0x400004AC` | `ZP_O` | ADD | Zero point of the output |
| 17 | `0x400004B0` | `SA` | ADD | Multiplier of A (raw 32-bit) |
| 18 | `0x400004B4` | `SHA` | ADD | Shift of A |
| 19 | `0x400004B8` | `SB` | ADD | Multiplier of B |
| 20 | `0x400004BC` | `SHB` | ADD | Shift of B |
| 21 | `0x400004C0` | `SO` | ADD, AVG_POOL | Output multiplier; AVG_POOL: Avg_Scale (raw 32-bit) |
| 22 | `0x400004C4` | `SHO` | ADD, AVG_POOL | Output shift; AVG_POOL: Avg_Shift (0 to 63) |
| 23 | `0x400004C8` | `POOL_K` | MAX_POOL, AVG_POOL | Kernel size |
| 24 | `0x400004CC` | `POOL_P` | MAX_POOL, AVG_POOL | Padding (MAX_POOL pad value -128); AVG_POOL: must be 0 |
| 25 | `0x400004D0` | `POOL_MODE` | MAX_POOL | 0 direct window, 1 separable (same result) |
| 26 | `0x400004D4` | `TILE_CIN` | GEMM | Input channels per core pass. Not written: all input channels in one pass. Must divide `IN_C` (otherwise error `BAD_GEOM`). Values below `IN_C` split each tile into `IN_C / TILE_CIN` passes that accumulate in SRAM-C (`ARCHITECTURE.md`, section 8.1) |
| 27 | `0x400004D8` | `Y_USED` | GEMM | Output positions per context (array rows used), 1 to 32. Applied to a tile when it divides the tile width; otherwise, and when not written, the greatest common divisor of the tile width and 32 is used. Larger than 32: error `BAD_GEOM`. Typical use: 31 for 3x3 stride-2 convolutions, which avoids a slower mode of the array at 32 positions |
| 28 | `0x400004DC` | `ROWS` | FUSED_ATTN, LAYERNORM | FUSED_ATTN: query rows NQ (not written: equal to `LEN`). LAYERNORM: number of rows (required) |
| 29 | `0x400004E0` | `PARAM_ADDR` | FUSED_ATTN, LAYERNORM | DRAM address of the instruction's parameter block (section 6.2). Required; missing: error `BAD_GEOM` |
| 30 | `0x400004E4` | `MASK_ADDR` | FUSED_ATTN | DRAM address of an int8 mask [NQ][L], value -128 = masked position. Not written: no mask |

The ranges `0x40000318` to `0x4000031C` and `0x4000046C` to `0x400004E4` are not used by any other part of the
model.

## 4. Programming sequences

### 4.1 Convolution (GEMM_FUSED)

1. Write `IN_ADDR`, `W_ADDR`, `OUT_ADDR`, `BIAS_ADDR`, `KH`, `KW`, `STRIDE`, `PAD`, `ACT_TYPE`, and `HAS_SKIP = 0`.
2. Write the extensions: `IN_C/H/W`, `OUT_C/H/W`, `TILE_COUT/H/W`, `SCALE_ADDR`, `SHIFT_ADDR`, `LUT_ADDR`
   (if an activation is used), `ZP_OUT`.
3. Write `0x12` to `PUSH_A`.

The hardware walks the tiles in this order: output channel blocks, then output rows, then output columns.
Edge tiles are clipped to the tensor. For each tile it derives:
- the input window `iy = oy * stride - pad`, `ix = ox * stride - pad`, of size `(h - 1) * stride + kh`;
- the active columns = the tile's output channels;
- the active rows = `Y_USED` when it is written and divides the tile width, otherwise `gcd(tile width, 32)`.

### 4.2 Residual add (ELEM_WISE ADD)

1. Write `A_ADDR`, `B_ADDR`, `OUT_ADDR`, `LEN`, `A_LEN = B_LEN = LEN`, and mode 0 in the mode pack.
2. Write the extensions `ZP_A/B/O`, `SA`, `SHA`, `SB`, `SHB`, `SO`, `SHO`.
3. Write `0x15` to `PUSH_A`.

### 4.3 Max-pool and average pool (ELEM_WISE MAX_POOL, AVG_POOL)

1. Write `A_ADDR` (input), `OUT_ADDR`, `STRIDE`, and mode 1 (MAX_POOL) or 5 (AVG_POOL) in the mode pack.
2. Write the extensions `IN_C/H/W`, `POOL_K`, `POOL_P`, and `POOL_MODE` (MAX_POOL) or `SO` = Avg_Scale and
   `SHO` = Avg_Shift (AVG_POOL, `POOL_P` = 0).
3. Write `0x15` to `PUSH_A`.

AVG_POOL follows the vector-unit drawing: the k x k window is summed in a 16-bit accumulator, multiplied by
Avg_Scale and round-shifted by Avg_Shift; the model saturates the result to int8 (`KNOWN_LIMITATIONS.md`, H5).
Typical scale: Avg_Scale = round(2^Avg_Shift / k^2).

### 4.4 Attention head (FUSED_ATTN)

One instruction computes one head: X = Requant(Q.K^T with zero-point corrections), masked positions set to -128,
row softmax, O = Requant(A.V).

1. Write `A_ADDR` = Q [NQ][D], `B_ADDR` = K [L][D], `V_ADDR` = V [L][D] (int8), `OUT_ADDR` = O [NQ][D] (int8).
2. Write `LEN` = L and register `0x40000458` = D (integer).
3. Write `ROWS` = NQ (optional), `PARAM_ADDR` (block `ATN1`, section 6.2), `MASK_ADDR` (optional).
4. For operands produced by a GEMM_FUSED over tokens, write `FLAGS` bit 1 (`CHANNEL_MAJOR`); head h then starts at
   base + h x D x L.
5. Push opcode `0x13`.

### 4.5 LayerNorm (LAYERNORM)

1. Write `IN_ADDR` = X [ROWS][H] (int8), `OUT_ADDR` = Y [ROWS][H] (int8, or int16 little-endian when the block
   selects a 16-bit output).
2. Write `LEN` = H (must equal H in the parameter block), `ROWS`, `PARAM_ADDR` (block `LNP1`), and `FLAGS` bit 1 if
   the tensors are channel-major.
3. Push opcode `0x14`.

### 4.6 Completion

Poll `STATUS` until bit 0 clears, or wait for `o_irq`. `RETIRED` counts completed instructions. Check bit 2
and the error code after each batch.

Software that waits a fixed time still works, but wastes time.

For the whole YOLOv8m network the sequence is 98 instructions (83 GEMM_FUSED, 12 ADD, 3 MAX_POOL) and 2,318
register writes in the reference program (`insts_pe3`; 2,306 without the planner options of `ARCHITECTURE.md`,
section 8.2). For comparison, one instruction per tile would need about 186,000 writes. ViT-B/16 is 244
instructions (50 GEMM_FUSED, 144 FUSED_ATTN, 25 LAYERNORM, 25 ADD) and 3,264 register writes.

## 5. Required software changes

The compatibility layer cannot hide these changes:

1. **Weight layout.** Weights in DRAM must be in the SAURIA order produced by `sauria_weight_order()`
   (`driver/libsauria_mem.h`), stored per output-channel block so that the block of channels `c0..c1` starts at
   `W_ADDR + c0 * Cin * kh * kw`. A row-major `K x N` matrix is no longer accepted.
2. **Activation layout.** Activations are flat `[C][H][W]` int8. Transformer activations are channel-major
   `[dim][token]` (a GEMM_FUSED over tokens is a 1x1 layer with the tokens as pixels); FUSED_ATTN and LAYERNORM
   read this layout with `FLAGS.CHANNEL_MAJOR`. Software that stores `[token][dim]` needs a transpose.
3. **Integer per-channel quantization.** Per-channel signed 32-bit multipliers, shifts and zero point, plus the
   256-entry SiLU table, are stored in DRAM and referenced by `SCALE_ADDR`, `SHIFT_ADDR`, `LUT_ADDR`. The
   floating-point scale registers remain only as an approximate path.
4. **Extension registers and completion.** Convolutions need the geometry and tiling extensions (section 3.3).
   Completion is read from `STATUS` / `RETIRED` or the interrupt.
5. **Single lane and reduced element-wise set.** There is no second lane and `SET_NSPLIT` has no effect.
   ELEM_WISE supports ADD, MAX_POOL and AVG_POOL (mode 5, without padding). MUL, SUB and DIV are not
   supported.

The numerical results of the previous decoder cannot be reproduced. That decoder evaluated a simplified
functional model rather than the hardware arithmetic.

## 6. Data preparation expected from the compiler

| Item | Prepared by | Notes |
|---|---|---|
| Tile sizes per layer (`TILE_COUT/H/W`) | Offline planner (`tools/fe/fe_tile_plan.py`) | Must respect the SRAM limits in `HARDWARE_CONFIG.md` |
| Weight blocks in SAURIA order | Compiler | See section 5. With `TILE_CIN` < `IN_C`, see section 6.1 |
| Bias as int32 per channel | Compiler | Loaded as the partial-sum preload |
| Slice and concat | Compiler, by address mapping | Concat outputs are written directly into sub-regions of the concatenated tensor; a channel slice is an address offset |
| Upsample | Host, until hardware support is decided | See section 7 |

### 6.1 Weight layout with an input-channel split

Without a split, the weights of the output-channel band starting at channel `c0` are one block at
`W_ADDR + c0 * K`, with `K = KH * KW * IN_C`.

With `TILE_CIN` < `IN_C`, the block of each output-channel band is divided into `IN_C / TILE_CIN` parts, stored
one after the other. Part `j` starts at

```
W_ADDR + c0 * K + j * nch * Kt
```

where `nch = min(TILE_COUT, OUT_C - c0)` is the real number of output channels of the band (the last band
is smaller than `TILE_COUT` when `OUT_C` is not a multiple of it, and its parts are packed with that smaller
`nch`; with `FLAGS.PAD_TAIL` set, the last band is zero-padded by the compiler and `nch = TILE_COUT` for every
band), and
`Kt = KH * KW * TILE_CIN`. Each part is in SAURIA order for its `TILE_CIN` input channels, exactly as a whole
block would be for a layer with `TILE_CIN` input channels. The total size is unchanged.

### 6.2 Parameter blocks of FUSED_ATTN and LAYERNORM

Little-endian, 4-byte words, at `PARAM_ADDR`. The model parses them in `has/gvu_rce_params.h`; a reference
writer is `tools/has/make_rce_gate.py`.

| Block | Layout |
|---|---|
| `ATN1` (1,080 bytes) | word 0 magic `0x314E5441`; words 1 to 5 Zq, Zk, Zv, Zqk, Zav (int32); words 6-7 Mqk (int64); word 8 TSqk; words 9-10 Mav (int64); word 11 TSav; word 12 flags (bit 0 asymmetric zero points, bit 1 zero exponent on masked positions, bit 2 V zero-point correction); word 13 reserved; words 14 to 269 the 256-entry exponent table (int32 holding unsigned 8-bit values) |
| `LNP1` | word 0 magic `0x31504E4C`; word 1 H; then 19 int64 scalars: H, pre-shift, variance multiplier and shift and zero point, stage-3 multiplier, shift, bias and zero point, divide multiplier, shift and zero point, multiply zero point, output zero point, stage-3 floor option and value, stage-4 split option and bit count, 16-bit output flag; then gamma int32[H], beta int64[H], per-column multiplier int64[H], per-column shift int32[H] |

The reciprocal and reciprocal-square-root tables are the built-in logarithmic tables (Q14) of the vector unit.
The 1/sqrt(d) factor of attention is folded into Mqk. Parameters that stand in for open hardware questions are
listed in `KNOWN_LIMITATIONS.md`, section 7.1.

## 7. Open hardware questions that affect software

| # | Question | Assumption used by the model |
|---|---|---|
| H15 | How is the nearest-neighbour x2 upsample performed (DMA replication, feeder addressing, or host)? | Host |
| H16 | Instruction granularity: one instruction per tile or per layer; can instructions be read from a descriptor ring in DRAM? | One per layer, hardware tile loop |
| H17 | Bias preload: full `OH x OW x OC` int32 read, or a broadcast mode reading `OC` values? | Full read (broadcast evaluated as a proposal, `PERFORMANCE_REPORT.md`, section 5.5) |
| H18 | Convolution padding: DMA zero fill, feeder generation, or pre-padded data in DRAM? | DMA 3-D descriptor with zero fill |
| H19 | Where are int8 results packed four per 32-bit word; can the write DMA scatter into a sub-region of the destination tensor? | DMA packs, 3-D descriptor |
| H20 | Completion reporting (per instruction, per stream, status only); queue depth and full-queue behaviour | Status and retired counter, interrupt at end of stream, depth 16, error on overflow |
| H21 | Which command loads the LUT, scales and shifts? | Loaded by the controller: the LUT once per instruction, scales and shifts per tile |

The answers may change the extension registers or the data layout. Changes will be versioned in this document.

## 8. Current limitations relevant to integration

- The default profile of `HasNpuTop` uses the HAS AXI-128 DMA timing with DRAM latency 0; a latency can be set
  (`--dram-lat N`). The testbench-driven runs (`ARCHITECTURE.md`, section 4) use a DMA of 32 bytes per cycle,
  optimistic by about 2x.
- Softmax, LayerNorm and their operand DMA timing are ESTIMATED (`PERFORMANCE_REPORT.md`, section 6).
- No C driver API is included (developed by the integrating team); the register sequence is shown by
  `tools/has/tb_has_npu_top.cpp`, and `tools/has/wrapper/fw/` gives the register definitions and a replay engine in C
  (section 10.5).

## 9. Integration into an existing TLM wrapper

A system-level wrapper written for the previous native model (class `sauria::NpuTop` in `npu_top.h`, host bus
`i_host_*` / `o_host_rdata`, `i_start`, `o_done`, `o_deadlock`, `attach_perf()`, `set_dram()`, `decoder_inst`)
builds against this tree without changes:

1. Place this repository tree next to the previous model tree (where the wrapper keeps its bundled models). When
   the tree is committed inside another git repository, check that every file is tracked: the tree's own
   `.gitignore` and `.gitattributes` re-include the planner cost table under `tools/eval_sw/out/` (a common ignore
   rule for `out/` would drop it) and keep every file byte for byte; `sha256sum -c package_manifest.txt` in a fresh
   clone confirms it.
2. Point the wrapper's model root at it (for example the build variable `SAURIA_NPU_ROOT`). The files a wrapper
   usually requires are all at the same paths: `npu_top.h`, `npu_profile.h`, `sauria_targets.h`, `config_regs.h`,
   `control/instruction_decoder.h`, `control/sauria_dma.h`, `driver/libsauria_cfg.h`, `driver/sauria_run.h`,
   `instrumentation/perf_counters.h`.
3. Rebuild the wrapper and run its unit test; the expected output is identical to the previous native revision
   (`VERIFICATION_REPORT.md`, V19).

What the native top level provides through such a wrapper:

| Path | Behaviour |
|---|---|
| Native registers, SRAM windows, `i_start` | Native core (controller, feeders, array, PSM) with the native OBP; functional results are exact |
| Instruction registers (`0x40000300..0x40000468`) | Functional decoder (`emulate_*()`), exact integer results, no RTL-accurate timing |
| Performance counters | Native core counters; not comparable with the reference results of `PERFORMANCE_REPORT.md` |

The RTL-accurate core and the delivery top level `HasNpuTop` are not reached through the native top level; they run
through `tools/has/tb_has_npu_top` (`BUILD_AND_RUN.md`, section 6), or through the same wrapper with the model root
`core_rtl/` (section 10). The native `NpuTop` has no `o_irq` port: a
wrapper derives its interrupt from `o_done` and `o_deadlock`.

## 10. Running the RTL-accurate core in a system wrapper

The same wrapper can run programs on the RTL-accurate core (`HasNpuTop`) instead of the functional decoder. Native
behaviour is unchanged in both configurations.

| Model root (`SAURIA_NPU_ROOT` or equivalent) | Native registers, SRAM, `i_start`, instructions without extension registers | Instructions pushed after writing extension registers |
|---|---|---|
| repository root | native core / functional decoder | functional decoder |
| `core_rtl/` | native core / functional decoder (identical) | `HasNpuTop`: RTL-accurate core, HAS data flow, DMA, epilogue, vector unit |

### 10.1 One-time changes

1. Point the model root at `core_rtl/`. It contains the drop-in `npu_top.h` and one-line forwarding headers for every
   other file, so the wrapper's include path and required-file list stay the same (`core_rtl/README.md`).
2. Apply `tools/has/wrapper/npu_tlm_rich_window.patch` to the wrapper (two source files, 28 added lines), from the
   root of the wrapper's repository:

   ```bash
   git apply --check --directory=components/npu_tlm <model root>/tools/has/wrapper/npu_tlm_rich_window.patch
   git apply --directory=components/npu_tlm <model root>/tools/has/wrapper/npu_tlm_rich_window.patch
   ```

   (`--directory` is the wrapper's directory holding `include/npu_tlm_regmap.h` and `src/npu_tlm.cpp`; without git:
   `patch -p1` from that directory.) The patch was generated against the wrapper revision that bundles the v4.6
   native model; it applies with LF and with CRLF working files. If a later wrapper revision changes the context
   lines, the same 28 lines are added at the same places: two register constants, two fields of the register bank
   with their read and write cases, a flag set by any write to `0x4000046C..0x400004E7`, and the window staging at
   the start of the job preparation. It adds two
   registers to the wrapper's parameter bank and stages a DRAM window for extended instructions:

| Offset in the wrapper bank | Register | Meaning |
|---|---|---|
| `0x1130` | `RICH_WINDOW_BASE` | Physical address of the program's DRAM image |
| `0x1134` | `RICH_WINDOW_SIZE` | Size of the image in bytes |

   An instruction is extended when any of `0x4000046C..0x400004E7` was written since the previous push. For it, the
   wrapper copies the whole window into the model before the push and back to RAM after completion; other
   instructions are staged exactly as before. Programs of `HasNpuTop` do not use `M`, `K`, `N`, so the size-based
   staging of the unpatched wrapper cannot serve them.

### 10.2 Firmware sequence

The reference is `tools/has/wrapper/test_npu_tlm_core_rtl.cpp` (a SystemC test, built with the wrapper sources):

1. Load `dram_init.bin` into RAM at a base address; write that base and the image size to `RICH_WINDOW_BASE` and
   `RICH_WINDOW_SIZE`.
2. Replay `mmio.txt` in order. Registers that hold DRAM addresses get the RAM base added: `0x40000400`, `0x404`,
   `0x408`, `0x40C`, `0x434`, `0x444`, `0x448`, `0x44C`, and the extension registers `0x40000490`, `0x494`, `0x498`,
   `0x4E0`, `0x4E4`. The model converts the extension addresses back to image offsets (base `0x80000000` by default,
   `set_dram_base()`).
3. After each push to `0x40000310`, wait for `STATUS.DONE` of the wrapper, read the performance counters, clear
   `STATUS`.
4. Host steps of the program (`H` lines; YOLOv8m has two `upsample2x`, nearest neighbour x2 on `[c][h][w]` int8) run on
   the CPU between the instructions, as in `tools/has/tb_has_npu_top.cpp`.

### 10.3 Performance counters

While an instruction runs on `HasNpuTop`, each counter below grows by that instruction's value; the native counters
keep their meaning (`total_cycles` counts every clock).

| Counter | Value added per instruction |
|---|---|
| `PERF_EXEC_CYCLES`, `PERF_PROCESSING_CYCLES` | Instruction cycles: RTL-accurate core, modelled DMA and vector unit |
| `PERF_DMA_ENGINE_CYCLES` | Cycles waiting for the DMA |
| `PERF_OBP_CYCLES` | Epilogue cycles |
| `PERF_REDUCTION_ENGINE_CYCLES` | FUSED_ATTN and LAYERNORM cycles |
| `PERF_POOLING_ENGINE_CYCLES` | ELEM_WISE cycles |
| `PERF_DDR_READ_BYTES`, `PERF_DDR_WRITE_BYTES` | DMA bytes |

Cycle counts do not depend on the wrapper's clock period and equal those of `tools/has/tb_has_npu_top` for the same
instruction (`VERIFICATION_REPORT.md`, V20).

### 10.4 Options and limits

| Environment variable (read at construction) | Effect |
|---|---|
| `SAURIA_CORE_RTL_PROFILE` | `recommended` (default, configuration of the reference results), `proposals`, `legacy` |
| `SAURIA_CORE_RTL_C_BCAST=1` | Bias preload by broadcast descriptor (a proposed hardware option): changes the DMA timing only, not the data. Set it to reproduce the ViT-B/16 reference cycles (`PERFORMANCE_REPORT.md`, section 5.6); without it the program runs as drawn |
| `SAURIA_CORE_RTL_TRACE=1` | One line per `HasNpuTop` instruction with its cycles |

- Lane A only, INT8 only (as `HasNpuTop`).
- Simulation speed is that of the RTL-accurate core: a whole network takes hours (`RELEASE_NOTES.md`, section 4).
- The model reserves a private staging area above `0x10000000` in its DRAM vector (about 256 MB of host memory);
  `set_staging_base()` moves it.
- The DRAM window is copied in and out for every extended instruction; the CPU must not change the window while an
  extended instruction is queued.
- `test_npu_tlm_core_rtl.cpp` checks the outputs of GEMM_FUSED, ELEM_WISE, FUSED_ATTN and LAYERNORM instructions and of
  host steps. Whole YOLOv8m and ViT-B/16 runs through the wrapper match the golden images and the reference cycles
  instruction by instruction (`VERIFICATION_REPORT.md`, V20).
- Extended instructions support ELEM_WISE ADD (mode 0), MAX_POOL (1) and AVG_POOL (5) only; multiply, subtract and
  divide (modes 2 to 4 of the functional decoder) are rejected (section 5). A wrapper document that lists the modes
  of the functional decoder needs this note, and the two window registers of section 10.1, for the `core_rtl/` root.

### 10.5 Firmware example

`tools/has/wrapper/fw/` holds what firmware needs to run a program on the NPU of a platform, in plain C99:

| File | Content |
|---|---|
| `npu_has_fw.h` | Register addresses of this guide as C definitions (instruction and extension registers, opcodes, modes, flags, the wrapper's status, window and counter registers) and the interface of the replay engine |
| `npu_has_fw.c` | Replay engine: declares the DRAM window, writes the registers of each instruction (image offsets + RAM base in the address registers), pushes, waits for the wrapper's `STATUS`, clears it, runs the host steps on RAM, and checks every output by CRC-32 |
| `make_fw_program.py` | Converts a program directory (`mmio.txt`, DRAM images) into the stream the engine executes; the expected CRC of every output is taken from `dram_golden.bin`, so firmware does not need the golden image. `--asm` writes an assembly file that links the stream and `dram_init.bin` into the firmware ELF |
| `fw_main_example.c`, `linker_example.ld`, `Makefile` | Bare-metal example: hardware abstraction (five functions: 32-bit register access, RAM copy, idle, report), UART output, memory layout with the image at a fixed RAM address |
| `test_fw_replay.cpp` | Host test: the same C engine driven through the wrapper with TLM transactions |

Steps for a first run on a platform (a 14-instruction transformer check with a 1 MB image, 250,885 NPU cycles,
a few minutes of simulation):

```bash
cd tools/has/wrapper/fw
make PROGRAM=$FE_WORK/has/rce_gate FW_INC=<platform firmware include dir> STARTUP=<platform startup.S>
<platform executable> --fw npu_has_example.elf        # expected on the console: NPU HAS start ... NPU HAS PASS
```

The platform must be built with the model root `core_rtl/` and the patched wrapper (section 10.1). Then
`PROGRAM=$FE_WORK/has/insts_pe3 COUNT=2` runs the first two YOLOv8m instructions, and without `COUNT` the whole
network (hours; expected cycles in `PERFORMANCE_REPORT.md`, sections 5.4 and 5.6).

**Loading the DRAM image.** The image of a program must be in system RAM before the first instruction: 1 MB for
`rce_gate`, 86 MB for YOLOv8m, 114 MB for ViT-B/16, all below the usual 256 MB of RAM. The example links it into
the ELF at `0x81000000` (`linker_example.ld`), which works when the platform's ELF loader copies every loadable
segment into RAM. If the loader cannot take a segment of that size, load `dram_init.bin` with a platform mechanism
(a memory preload option, a flash image, a debugger) and pass its address to `npu_has_replay()`. The NPU reads and
writes the image in place, so the region must not overlap the firmware, its stack or its heap.

**What was checked.** The engine runs unchanged on the host through the wrapper (`test_fw_replay`): the whole
transformer check passes, and on whole programs every host step and every output CRC agree with the golden images
(`RELEASE_NOTES.md`, section 4). The example firmware is compiled for RV32 against a platform's firmware headers.
It has not been run on a CPU model inside a full platform; that run is the integrator's first step.
