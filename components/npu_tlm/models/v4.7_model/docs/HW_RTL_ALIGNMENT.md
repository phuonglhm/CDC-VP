# Alignment with the neo_lite hardware RTL, and the transpose after the epilogue

This document covers the options that follow the hardware repository `neo_lite` at commit `9b704bb`. The repository
contains the data flow controller (DFC), the one-dimensional DMA and a new register map of the vector unit. The
document also describes the corner turn after the epilogue, a proposal of the model.

Every option here is **off by default**. With all of them off, values and cycle counts are identical to the default of
this release. Each option turns on one hardware behaviour, so that its cost can be measured. It becomes a default only
once it has been checked on whole networks.

Labels: **MEASURED** = a run of the model or of an RTL simulation; **ESTIMATE** = a parameter that stands in for a
measurement not yet made; **DERIVED** = a consequence of measured numbers, not measured on its own.

## 1. Summary

| Option (environment) | What it changes | In the RTL `9b704bb` | Measured |
|---|---|---|---|
| `HAS_GVU_REGS_NEO=1` | Uses the register map of the new RTL: 14 byte-addressed registers, opcode outside CONTROL, 14 writes per command. It runs next to the model's own register file and is compared with it | yes | Port against an RTL simulation: 0 differences over 30,000 cycles |
| `HAS_GVU_VBANK1_DEPTH=200` | Sets vbank#1 to 200 rows (25 KB) instead of 32 | yes | Port against an RTL simulation: 0 differences |
| `HAS_GVU_RESIDUAL_HW=1` | Runs the residual add through the RESIDUAL operation (opcode 6) as the hardware builds it | yes, with a shift bug | 0 differences on 98,304 elements against an RTL simulation; latency 13 + vectors |
| `HAS_DMA_DFC_1D=1` | Runs slice / concat (ELEM_WISE modes 6 / 7) as one-dimensional DMA commands of the DFC | yes (the DMA has one dimension) | Command cost from an RTL simulation: 19 / 15 / 7 cycles, data 1 beat per cycle |
| `HAS_GVU_OUT_VBANK2=1` | Sends the GEMM_FUSED result through vbank2 to DRAM by DFC commands | yes | Check programs: +0.9 % to +17.7 % cycles |
| `HAS_BUS_STRICT=1` | Charges the bus for data the DFC moves outside the DMA model: per-channel scale / shift, and the Softmax output A of FUSED_ATTN | yes (DMA steps of the DFC) | Check programs: 0 to +1,054 cycles; attention check +31,490 |
| `HAS_OUT_TRANSPOSE=1` | Corner turn after the epilogue: channels from `X_TMAJ_C0` on are written token-major | **no** (proposal of the model) | ViT-B/16 whole network PASS 244 / 244, +382 cycles |

All seven are in `bash tools/has/regress_gvu.sh` (193 items): each runs on the reference check program, and the new
units have their own tests.

## 2. Register map of the new vector unit (`HAS_GVU_REGS_NEO`)

Source: `has/rtl_ref_gvu_config_regs_neo.h`, a port of `gvu_config_regs.sv` and `gvu_pkg.sv`. Each register is 4
bytes, at byte address 4 × index.

| Index | Register | Fields |
|---|---|---|
| 0 | CONTROL | [0] enable, [1] done flag, [2] clear done (write 1) |
| 1 | OPCODE | [3:0] |
| 2 | LUT reset | [0..3] activation / exponent / reciprocal / reciprocal square root; a one-cycle pulse |
| 3 | GEMM_FUSED | [0] skip activation, [15:8] zero point |
| 4 to 6 | SOFTMAX_0..2 | token length [15:0], tiles [31:16]; scale; shift [7:0], zero point [15:8] |
| 7 | RESIDUAL_0 | length [15:0], output zero point [23:16], output shift [29:24], subtract [30] |
| 8, 9 | RESIDUAL scale A / B | |
| 10, 11 | RESIDUAL bias low / high | |
| 12 | POOLING_0 | mode [1:0], stride [4:2], W tiles [15:8], C [23:16], OH [31:24] |
| 13 | POOLING_1 | H move [9:0], C move [25:16] |

The RTL has 14 registers. A write to index 14 or 15 is dropped, and a read returns 0 in the port.

How the model uses it:

- The model's own register file stays the functional one: the map of the previous RTL drop, plus the model's
  proposals (AGU_ND, average pool, residual add). Values and the model's own cycles therefore do not change.
- Every tick of the model's register file is translated into writes of the new map and run in parallel. After each
  tick, the fields present in both maps are compared. A difference is counted, not repaired.
- Each command costs 14 register writes plus CONTROL, one write per cycle while the receiver is ready. This is
  MEASURED on the DFC (`neo_interface.sv`) without the clock-domain crossing.
- `HAS_GVU_NEO_WRITE_LAT` (default 1) sets the cycles per write.
- The DFC writes 14 registers, and its last write, at byte address 0x38, falls outside the 14-entry array (reported to
  the hardware team).

MEASURED results:

- Port against an RTL simulation of `gvu_config_regs.sv`: 0 differences over 30,000 cycles. A negative control (two
  values changed by hand) gives 2 differences.
- Check programs with the option on: all PASS, 0 fields different, 0 differences in the LUT pulses.
- Cycles: +792 (+0.29 %) on a YOLOv8m-shaped program, +268 to +402 on the padding programs, −67 to 0 on the others.
  The −67 is one poll step of the test bench.

Not done:

- The four buffer-select flops of the new RTL toggle on the enable edge. The model counts these edges but keeps its
  own buffers.
- The clock-domain crossing and the multiplexer of `neo_top` are not added. On the RTL (Verilator) they give one write
  per core cycle, plus about 4 core cycles once per write chain.

## 3. vbank#1 depth (`HAS_GVU_VBANK1_DEPTH`)

The new `gvu_pkg.sv` gives vbank#1 200 rows (25 KB) instead of 32. `HAS_GVU_VBANK1_DEPTH=200` selects the new depth;
unset or 0 keeps 32. MEASURED: the vector load / store unit and the scratchpad at depth 200 give 0 differences against
the RTL simulation, and the check programs PASS at both depths.

## 4. RESIDUAL as the hardware builds it (`HAS_GVU_RESIDUAL_HW`)

Source: `has/rtl_ref_gvu_residual_hw.h`.

- **Formula, per lane**: s = a·S_A ± b·S_B (64 bit), plus bias; shift right by the output shift; clamp to
  [−256, 255]; add the output zero point; clamp to [−128, 127]. One 32-lane vector per cycle; done after 13 + vectors
  cycles.
- **Use**: with the option on, the ADD of the element-wise block runs through this unit when its parameters can be
  expressed exactly (bias = 2^(sh−1) − zp_A·S_A − zp_B·S_B gives the half-up rounding of the model's ADD). Otherwise
  the ADD stays on the functional path: 20 of the 22 ADD chunks of the check program fit.
- **Shift bug**: the delivered `gvu_vector_shift.sv` shifts negative values logically (reported to the hardware team,
  with a patch). The model shifts arithmetically, the intended behaviour. `HAS_GVU_RESIDUAL_HW_SHIFT_LOGICAL=1`
  reproduces the delivered RTL.

MEASURED:

- 98,304 elements equal to the RTL simulation, in both shift variants.
- Latency 13 + R, 5 of 5 cases.
- The translation from the model's ADD parameters: 0 differences on 615,684 samples.
- ADD cycles drop, because the vector-unit path takes V + 13 cycles instead of 2V + 11.

## 5. Slice and concat by one-dimensional DFC commands (`HAS_DMA_DFC_1D`)

Source: `has/dma_dfc_1d.h`.

**How the hardware does it**: the DMA of the RTL copies one contiguous block. For slice and concat (ELEM_WISE modes 6
and 7, `DMA_SLICE_CONCAT.md`), the DFC therefore issues one command per contiguous run. The data goes DRAM →
scratchpad bank → DRAM, in pieces of at most one vbank0 buffer, one command after the other.

**Cost**: MEASURED on `neo_dma_controller.sv` with the pulp `axi` / `common_cells` libraries, on one clock, without
the multiplexer or clock-domain crossing.

| Part | Cycles | Environment |
|---|---|---|
| First command of an operation, before its first request (14 configuration writes + 5) | 19 | `HAS_DMA_DFC1D_FIRST` |
| One command to the next (last beat to first request) | 15 | `HAS_DMA_DFC1D_CHAIN` |
| End of the operation after the last beat (interrupt, then clear) | 7 | `HAS_DMA_DFC1D_TAIL` |
| Data | ceil(bytes / 16) + 1 (128-bit bus) | |
| Largest piece | 25,600 bytes (one vbank0 buffer) | `HAS_DMA_DFC1D_STAGE` |

**Cycles with the option on** (MEASURED on the check programs, all PASS):

- YOLOv8m-shaped program: +1.0 %.
- Depthwise-separable program: +57 %.
- Large strided slice / concat program: ×14.

A strided slice has contiguous runs of one byte, so each byte becomes one command of about 17 cycles. That is the
consequence of "one command per contiguous run". The number is therefore an **upper** bound for a DMA that could
gather, and a **lower** bound with respect to the multiplexer and the clock-domain crossing.

## 6. GEMM_FUSED result through vbank2 (`HAS_GVU_OUT_VBANK2`)

In the RTL, the GEMM_FUSED result goes from the vector unit into vbank2, then to DRAM by one-dimensional DFC commands
(`DMA_SEND_VBANK2`). The model's default writes the result back through the core SRAM C, as before.

With the option on, the data moves as before, but the write-back is charged with the measured cost of section 5.
MEASURED on the check programs (all PASS): +134 cycles on the reference program, +0.9 % on the YOLOv8m-shaped
program, +1.2 % on a padding program, +17.7 % on the depthwise-separable program.

## 7. Bus cost of the data the DFC moves (`HAS_BUS_STRICT`)

Two transfers that the hardware makes through the DMA had no cost in the model:

1. **Per-channel scale and shift of a tile** (8 bytes per channel). The hardware brings them into the vector-unit
   sbank with a DMA step of the DFC. With the option on, they cross the DMA read port together with the prefetch of the
   tile's bias.
2. **Softmax output A of FUSED_ATTN**. The vector unit has no direct path to the core SRAM. With the option on, A is
   written out of the vector unit and read back into SRAM A for A·V.

MEASURED:

- With the option off: identical to the default, 22 of 22 checks.
- With the option on, values PASS on 22 of 22:
  - YOLOv8m-shaped program: +1,054 cycles.
  - Padding / short-context program: +268.
  - Depthwise-separable, DMA and reference programs: 0.
  - Attention check: +31,490. The prediction was +31,488; the 2-cycle difference is not explained.
- `HAS_BUS_STRICT_LUT=1` (test only) loads the activation table as a 256-byte DMA transfer instead of its estimate:
  +1 cycle per table.
- Port contention, counted only: 55 % to 85 % of the vector unit's writes into SRAM C share a clock step with a PSM
  write. The model does not stall either side (`KNOWN_LIMITATIONS.md`, O35).

## 8. Corner turn after the epilogue (`HAS_OUT_TRANSPOSE`, proposal)

**Need**: the PSM gives the results of one context as one beat per output channel, with the positions on the lanes,
so the output is written channel-major ([C][H][W]). Attention needs V token-major (one row of channels per token).

**Why a separate unit**: the vertical scan of the new RTL (`transpose_mode`) is not used. On the RTL it is correct for
contexts 1 and 2 only. From context 3 on it adds the result of context k−2 (MEASURED on an RTL simulation, reported
with a patch; the case with preload is still open). The model does not imitate it, and instead proposes a unit after
the epilogue.

**Unit** (`has/gvu_out_transpose.h`): two banks of W × W int8, used as a ping-pong.

- One bank takes the beats of a context, one per channel.
- The other bank gives back one word per position, with the channels on the lanes.
- A context of n channels and p positions takes n cycles in and p cycles out.
- The last beat of a context turns the banks over only when the drain bank is empty. Otherwise it waits
  (back-pressure on the epilogue).

**Register `X_TMAJ_C0`** (GEMM_FUSED extension, byte address `0x40000500`; the extension range now ends at
`0x40000504`):

- The value must be a multiple of `TILE_COUT`.
- When written, channels `TMAJ_C0` .. `OUT_C−1` are written token-major: position p of channel c goes to
  `OUT_ADDR + TMAJ_C0·OH·OW + p·(OUT_C − TMAJ_C0) + (c − TMAJ_C0)`.
- Channels below `TMAJ_C0` stay [C][H][W].
- With `HAS_OUT_TRANSPOSE=0` the register is ignored, and every tile is written [C][H][W] as before.
- It cannot be combined with `HAS_SKIP`.

**FUSED_ATTN FLAGS bit 2**: V is token-major. Row (token) i, channel j is at `V_ADDR + i·IN_C + j`.

**Frontend**: `python3 tools/fe/fe_vit_full.py --v-token-major [--out DIR]` writes the ViT-B/16 program with V
token-major. It sets `X_TMAJ_C0 = 1536` on each QKV GEMM_FUSED (12 writes) and FLAGS bit 2 on each FUSED_ATTN.
`tools/fe/fe_vit_block.py` has the same option for one block. `tools/has/ref_program.py` follows both.

MEASURED, with `--c-bcast` unset as in the default:

| Run | Result |
|---|---|
| Corner-turn unit (`tools/has/tb_gvu_out_transpose.cpp`) | 90 / 90 |
| GEMM_FUSED token-major on the reference check program, against `ref_program.py` | equal |
| One ViT-B/16 block, V token-major, option on | PASS 20 / 20, 3,301,548 cycles (+26 against the channel-major program), 72 token-major tiles |
| The same program with the option off (negative control) | FAIL, 19 tensors different (as expected) |
| ViT-B/16 whole network, V token-major, option on | PASS 244 / 244, 26,068,616 elements, 0 different, **41,245,003** cycles (+382 against the default 41,244,621), 864 token-major tiles; the corner-turn tails (27,648 cycles, 32 per tile) are almost entirely hidden behind the next tile |

## 9. The core against the new RTL

The core of the model (systolic array, feeders, PSM, controller, horizontal scan) was compared with the core of
`neo_lite@9b704bb` in a ModelSim simulation.

- **Values**: 8 of 8 cases write SRAM C identically (number of writes, rows, masks, data).
- **Cycles**: model minus RTL is between −2 and +1 per write, and does not accumulate.
- **Not modelled**: the vertical scan (section 8), the four new output bits (transpose, stream, `cin_last`,
  `oc_tile_last`) with the start-by-stream port, and non-square configurations.

## 10. Configuration with only what the hardware has

`docs/PERFORMANCE_REPORT.md` and section 3 of `RELEASE_NOTES.md` use the default of this release. That default turns
on proposals of the model that the RTL does not have: 4-D DMA and COPY, the attention requant on the vector unit, lane
LayerNorm and average pool. The closest runs to the RTL are:

| Network | Configuration | Result |
|---|---|---|
| YOLOv8m | program `insts_pe3` (concat by address, no DMA instruction), default build | PASS 98 / 98, **62,422,935** cycles. A lower bound: the options of sections 2 and 5 to 7 are off |
| ViT-B/16 | `HAS_GVU_FA=0 HAS_GVU_LN=0` (Softmax on the vector unit as in the RTL; attention requant and LayerNorm on the functional model) | PASS 244 / 244, **41,608,570** cycles |

The ViT-B/16 number is not a "hardware only" number: the RTL has no LayerNorm data path for H = 768, so LayerNorm still
runs on the model.

Against the default, step by step (MEASURED):

- LayerNorm on the functional model: +627,547 cycles (25 instructions).
- Attention requant on the functional model: −263,440 cycles (144 heads).

## 11. Reproducing

| Item | Command |
|---|---|
| All options on the reference check program, unit tests | `bash tools/has/regress_gvu.sh` (193 items) |
| One option | `HAS_GVU_REGS_NEO=1 tools/has/tb_has_npu_top <program dir>` (likewise the other variables of section 1) |
| Register map self-check | `g++ -std=c++17 -O2 -I. tools/has/tb_rtl_ref_gvu_config_regs_neo.cpp && ./a.out`; against the RTL: `tools/has/gvu_oracle/tb_oracle_cfgregs_neo.sv` with the RTL tree (not part of this package) |
| RESIDUAL unit | `g++ -std=c++17 -O2 -I. tools/has/tb_rtl_ref_gvu_residual_hw.cpp && ./a.out` |
| Corner turn | `g++ -std=c++17 -O2 -I. tools/has/tb_gvu_out_transpose.cpp && ./a.out`; programs: `python3 tools/has/make_tmaj_gate.py <source program dir> <out dir>` (adds `X_TMAJ_C0` to a program), then `HAS_OUT_TRANSPOSE=1 tools/has/tb_has_npu_top <out dir>`; reference tool against numpy: `python3 tools/has/test_ref_tmaj.py <program dir> ...` |
| ViT-B/16 with V token-major | `python3 tools/fe/fe_vit_full.py --v-token-major --out <dir>` (needs the trained weights), then `HAS_OUT_TRANSPOSE=1 tools/has/tb_has_npu_top <dir>` |
| ViT-B/16 closest to the RTL | `HAS_GVU_FA=0 HAS_GVU_LN=0 tools/has/tb_has_npu_top $FE_WORK/has/vit_full` |
