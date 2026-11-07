# Alignment with the neo_lite hardware RTL, and the transpose after the epilogue

This document covers the options that follow the hardware repository `neo_lite` RTL revision 0. RTL revision 0 and 1 are the two hardware RTL revisions the model follows; `HAS_NEO_REV` selects them. The repository
contains the data flow controller (DFC), the one-dimensional DMA and a new register map of the vector unit. The
document also describes the corner turn after the epilogue, a proposal of the model. Section 14 covers the later
RTL revision 1, selected by `HAS_NEO_REV=1`; the run profiles that set all options of one revision at once and
the per-layer hardware check are in `COMPILER_GUIDE.md`, and the register file of the DFC in `NEO_REGISTER_FILE.md`.

Every option here is **off by default**, except two that are on in this release: `HAS_DMA_ND_MERGE` (section 12) and
`HAS_OUT_TRANSPOSE` (section 8). `HAS_OUT_TRANSPOSE` changes nothing for a program that does not write `TMAJ_C0`;
`HAS_DMA_ND_MERGE` changes only the cycles of programs with slice / concat instructions. The run profile `neo` sets both
to 0. With the others off, values are identical to the earlier release. Each option turns on one hardware behaviour, so
that its cost can be measured.

Labels: **MEASURED** = a run of the model or of an RTL simulation; **ESTIMATE** = a parameter that stands in for a
measurement not yet made; **DERIVED** = a consequence of measured numbers, not measured on its own.

## 1. Summary

| Option (environment) | What it changes | In RTL revision 0 | Measured |
|---|---|---|---|
| `HAS_GVU_REGS_NEO=1` | Runs the vector unit on the register map of the new RTL: 14 byte-addressed registers, opcode outside CONTROL, 14 writes per command, the four ping-pong buffers switched on the enable edge, and the delay of the multiplexer and clock-domain crossing; the model's proposals sit at indices 14 to 31 | yes (indices 0 to 13) | Port against an RTL simulation: 0 differences over 30,000 cycles |
| `HAS_GVU_VBANK1_DEPTH=200` | Sets vbank#1 to 200 rows (25 KB) instead of 32 | yes | Port against an RTL simulation: 0 differences |
| `HAS_GVU_RESIDUAL_HW=1` | Runs the residual add through the RESIDUAL operation (opcode 6) as the hardware builds it | yes (RTL revision 0 shifts negative values logically; RTL revision 1 fixes it, and the model shifts arithmetically as RTL revision 1) | 0 differences on 98,304 elements against an RTL simulation; latency 13 + vectors |
| `HAS_DMA_DFC_1D=1` | Runs slice / concat (ELEM_WISE modes 6 / 7) as one-dimensional DMA commands of the DFC | yes (the DMA has one dimension) | Command cost from an RTL simulation: 19 / 15 / 7 cycles (single clock; the defaults are 6 / 15 / 3 core cycles, section 14.4), data 1 beat per cycle |
| `HAS_GVU_OUT_VBANK2=1` | Sends the GEMM_FUSED result through vbank2 to DRAM by DFC commands | yes | Check programs: +0.9 % to +17.7 % cycles |
| `HAS_BUS_STRICT=1` | Charges the bus for data the DFC moves outside the DMA model: per-channel scale / shift, and the Softmax output A of FUSED_ATTN | yes (DMA steps of the DFC) | Check programs: 0 to +1,054 cycles; attention check +31,490 |
| `HAS_DMA_ND_MERGE=1` (on by default) | Merges contiguous dimensions of a slice / concat descriptor so that the DMA pieces are larger (section 12) | **no** (4-D DMA is a proposal) | Check programs: up to −7,303 cycles, same DRAM image |
| `HAS_GVU_FA_OVERLAP=1` | Runs the attention requant on the vector unit during the next pass of the core (section 11) | **no** (proposal P-FA5) | 12 ViT-shaped heads: −1,383 cycles per head |
| `HAS_OUT_TRANSPOSE=1` (on by default) | Corner turn after the epilogue: channels from `X_TMAJ_C0` on are written token-major (needs a program that writes `X_TMAJ_C0`; other programs are unchanged) | **no** (proposal of the model) | ViT-B/16 whole network PASS 244 / 244, +382 cycles |
| `HAS_NEO_REV=1` | Makes the options above follow RTL revision 1 instead of RTL revision 0 (section 14) | yes (RTL revision 1) | Port against an RTL simulation: 0 differences over 30,000 cycles for both revisions |
| `HAS_DFC_CORE_DMA=1` | Charges the core operand loads as the one-dimensional commands of the DFC (section 14.3); only with `HAS_NEO_REV=1` | yes (RTL revision 1) | Check programs: +0 and +201 cycles |

All of them are in `bash tools/has/regress_gvu.sh` (242 items): each runs on a check program, and the new
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

- With the option on, the register file of the vector unit is the map above: indices 0 to 13 exactly as the RTL, the
  model's proposals at 14 to 31 (average pool 14 / 15, AGU_ND 16 to 24, the parts of the residual add that the RTL
  RESIDUAL does not have 25 to 29). The residual add uses RESIDUAL_0 to RESIDUAL_2 of the RTL; with
  `HAS_GVU_RESIDUAL_HW=1` the bias registers 10 / 11 hold the value the hardware needs. The map needs `GVU_NUM_REGS` 32
  and `IF_AW` 7 in the RTL (proposal).
- Each command costs 14 register writes plus CONTROL, one write per cycle while the receiver is ready. This is MEASURED
  on the DFC (`neo_interface.sv`). `HAS_GVU_NEO_WRITE_LAT` (default 1) sets the cycles per write.
- Ping-pong: the four buffer-select flops toggle on the rising edge of the enable, as in the RTL. The model loads the
  tables, operands and results of GEMM_FUSED, max pool, average pool and the residual add into the buffer the flops
  select. Softmax, the attention requant and LayerNorm keep fixed buffers.
- Multiplexer and clock-domain crossing of `neo_top`: `HAS_GVU_NEO_CDC_LAT` cycles (default 4, MEASURED 3.8 to 4.1 core
  cycles on the RTL with Verilator) once per write chain; 0 removes them.
- With the option off, the model's previous register file runs and nothing changes.

MEASURED results:

- Port against an RTL simulation of `gvu_config_regs.sv`: 0 differences over 30,000 cycles. A negative control (two
  values changed by hand) gives 2 differences.
- Option off: identical to the default on 10 check programs in both build variants.
- Option on: 10 of 10 check programs PASS, 0 elements different; the configuration that the vector unit reads from the
  registers equals the model's configuration; 0 buffer-select differences.
- Cycles on the YOLOv8m-shaped program (98 commands = 98 buffer switches): −262 with the map alone, the same with the
  ping-pong, +536 with the clock-domain crossing. Other programs: −67 to +268 (steps of 67 are the poll of the bench).

Not done:

- Softmax, the attention requant and LayerNorm use fixed buffers; the paths without a clock (call style) as well.
- The DFC writes 14 registers and its last write, at byte address 0x38 (index 14), falls outside the RTL array.
  In the delivered RTL it overwrites the proposed average-pool register 14.
- The RTL ends every command with one more CONTROL write that clears start (`start = 0`). The model does not issue
  it: about 3 to 7 cycles per command on the RTL, no effect on values.

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
- **Shift bug**: the delivered `gvu_vector_shift.sv` shifts negative values logically (a finding for the hardware team). The model shifts arithmetically, the intended behaviour. `HAS_GVU_RESIDUAL_HW_SHIFT_LOGICAL=1`
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

The cost table above is the single-clock measurement; the defaults of the three variables are the core-cycle values of section 14.4.

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
  - Attention check: +31,490 at the poll step of 67 cycles of the bench; +31,488 counted by the model and with a bench
    that polls every 4 cycles, as predicted.
- `HAS_BUS_STRICT_LUT=1` (test only) loads the activation table as a 256-byte DMA transfer instead of its estimate:
  +1 cycle per table.
- Port contention, counted only: 55 % to 85 % of the vector unit's writes into SRAM C share a clock step with a PSM write.
  This describes a shortcut of the model, not the RTL: in the RTL the vector unit does not write SRAM C (the PSM is the
  only writer of the core side; the vector unit's results go to vbank2 and out through the DMA, `HAS_GVU_OUT_VBANK2`).
  Nothing is stalled and nothing needs to be.

## 8. Corner turn after the epilogue (`HAS_OUT_TRANSPOSE`, proposal)

On by default in this release as the temporary transpose on the vector unit; it acts only on a program that writes
`TMAJ_C0`, and `HAS_OUT_TRANSPOSE=0` (or the run profile `neo`) turns it off.

**Need**: the PSM gives the results of one context as one beat per output channel, with the positions on the lanes,
so the output is written channel-major ([C][H][W]). Attention needs V token-major (one row of channels per token).
In A·V the key token is the reduction index, so a weight word needs the 32 channels of one token. The default program
reads V channel-major and the model reorders it at no cycle cost; on the hardware that reorder would be a byte gather
(estimate: about +1.7 million cycles on ViT-B/16 at one byte per cycle). The +382 cycles of the corner turn are measured
against that optimistic default.

**Why a separate unit**: the vertical scan of the new RTL (`transpose_mode`) is not used. On the RTL it is correct for
contexts 1 and 2 only. From context 3 on it adds the result of context k−2 (MEASURED on an RTL simulation). The model does not imitate it, and instead proposes a unit after
the epilogue. With the vertical scan corrected, an RTL simulation without preload gives 0 differences on
4 contexts; with preload on, the preload term is still lost, which needs a change of the PSM sequence.

**Unit** (`has/gvu_out_transpose.h`): two banks of W × W int8, used as a ping-pong.

- One bank takes the beats of a context, one per channel.
- The other bank gives back one word per position, with the channels on the lanes.
- A context of n channels and p positions takes n cycles in and p cycles out.
- The last beat of a context turns the banks over only when the drain bank is empty. Otherwise it waits
  (back-pressure on the epilogue).

Cycles: the write-back of a token-major tile waits for the corner-turn tail (the words of its last context, at most 32)
and then moves the same bytes as the channel-major write-back.

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
neo_lite RTL revision 0 in a ModelSim simulation.

- **Values**: 8 of 8 cases write SRAM C identically (number of writes, rows, masks, data).
- **Cycles**: model minus RTL is between −2 and +1 per write, and does not accumulate.
- **Not modelled**: the vertical scan (section 8), the four new output bits (transpose, stream, `cin_last`,
  `oc_tile_last`) with the start-by-stream port, and non-square configurations.

## 10. Configuration with only what the hardware has

`docs/PERFORMANCE_REPORT.md` and section 3 of `RELEASE_NOTES.md` use the default of this release. That default turns
on proposals of the model that the RTL does not have: 4-D DMA with merged pieces, COPY, upsample and reshape, lane LayerNorm, average pool and the transpose after the epilogue. The attention
requant is in the RTL as GEMM_FUSED without activation, limited to 614 scale words (section 11). The closest runs to the RTL are:

| Network | Configuration | Result |
|---|---|---|
| YOLOv8m | program `insts_pe3` (concat by address, no DMA instruction), epilogue depth 9 (the whole network at depth 11 has not been measured) | PASS 98 / 98, **62,422,935** cycles. A lower bound: the options of sections 2 and 5 to 7 are off |
| ViT-B/16 | `HAS_GVU_FA=0 HAS_GVU_LN=0` (Softmax on the vector unit as in the RTL; attention requant and LayerNorm on the functional model) | PASS 244 / 244, **41,608,570** cycles |

The ViT-B/16 number is not a "hardware only" number: the RTL has no LayerNorm data path for H = 768, so LayerNorm still
runs on the model.

Against the default, step by step (MEASURED):

- LayerNorm on the functional model: +627,547 cycles (25 instructions).
- Attention requant on the functional model: −263,440 cycles (144 heads).

## 11. Vector-unit operations against the RTL

**Softmax configuration cost (MEASURED on 12 ViT-shaped heads).** Softmax on the vector unit costs 181 cycles per head
more than the functional path: per group of rows 5 register writes (20 cycles at `HAS_GVU_REG_LAT` 4, ESTIMATE) and
one cycle less in the unit, and per instruction a DMA of the 768-byte table (48 cycles). The data path is not slower.
Over the 144 heads of ViT-B/16 this is about 26,000 cycles, which explains the difference between the run closest to
the RTL and the default (section 10).

**Exponent 0 in Softmax stage 3.** The model shifts left when the exponent of the reciprocal is 0, as a corrected stage 3 would. A check program with exponent 0 on most rows passes; with the delivered shift it fails,
as expected.

**Attention requant.** The RTL can do it: GEMM_FUSED with the activation skipped multiplies the core's sum by a scale,
round-shifts, adds a 16-bit zero point and clamps to int8. MEASURED in an RTL simulation on 19 vector files (43,846
lanes): equal to the model's port up to 56 beats per phase. A phase of 980 beats (140 rows of a 197-token head) differs
on 10,134 of 27,580 lanes, because the sbank holds only 614 scale words. With one scale and one
shift for all beats (one bit in Reg 3) there are 0 differences on all 19 files, the long phase included; this is a proposal
of the model team. The RTL rounds half away from zero and wraps, the model rounds half up and saturates: the two
differ only on 5 directed files, not on the 13 files taken from real attention heads.

**Requant overlapped with the core (`HAS_GVU_FA_OVERLAP`, proposal P-FA5, off by default).** The requant of one core
pass runs on the vector unit during the next pass; the zero-point corrections go into the preload of the partial sums.
MEASURED: values unchanged; 12 ViT-shaped heads −1,383 cycles per head; one ViT-B/16 block −16,586 cycles (only the 12
FUSED_ATTN instructions change). Whole network: `PERFORMANCE_REPORT.md`, section 5.10.

**LayerNorm by lanes.** A behavioural RTL module of the model's lane LayerNorm (`gvu_ln_lanes.sv`, proposal) equals the
model on 438,529 lanes in a ModelSim simulation (a negative control differs on 107,712). It is not connected to
`gvu_top` yet.

## 12. Slice and concat pieces over merged dimensions (`HAS_DMA_ND_MERGE`, proposal)

On by default in this release; `HAS_DMA_ND_MERGE=0` (or the run profile `neo`) turns it off.

Before splitting a slice / concat descriptor into DMA pieces, the model drops dimensions of size 1 and merges
neighbouring dimensions that are contiguous in memory, so that each piece is as large as the chunk allows. MEASURED:
option off identical; option on PASS on 12 check programs with the same DRAM image; the large slice / concat program
127,636 → 120,333 cycles (DMA operations 1,952 → 1,840); `tools/has/ref_program.py` matches on all slice / concat
programs. Whole YOLOv8m: `PERFORMANCE_REPORT.md`, section 5.10.

## 13. Reproducing

| Item | Command |
|---|---|
| All options on the reference check program, unit tests | `bash tools/has/regress_gvu.sh` (242 items) |
| One option | `HAS_GVU_REGS_NEO=1 tools/has/tb_has_npu_top <program dir>` (likewise the other variables of section 1) |
| Register map self-check | `g++ -std=c++17 -O2 -I. tools/has/tb_rtl_ref_gvu_config_regs_neo.cpp && ./a.out`; against the RTL: `tools/has/gvu_oracle/tb_oracle_cfgregs_neo.sv` with the RTL tree (not part of this package) |
| RESIDUAL unit | `g++ -std=c++17 -O2 -I. tools/has/tb_rtl_ref_gvu_residual_hw.cpp && ./a.out` |
| Corner turn | `g++ -std=c++17 -O2 -I. tools/has/tb_gvu_out_transpose.cpp && ./a.out`; programs: `python3 tools/has/make_tmaj_gate.py <source program dir> <out dir>` (adds `X_TMAJ_C0` to a program), then `HAS_OUT_TRANSPOSE=1 tools/has/tb_has_npu_top <out dir>`; reference tool against numpy: `python3 tools/has/test_ref_tmaj.py <program dir> ...` |
| ViT-B/16 with V token-major | `python3 tools/fe/fe_vit_full.py --v-token-major --out <dir>` (needs the trained weights), then `HAS_OUT_TRANSPOSE=1 tools/has/tb_has_npu_top <dir>` |
| ViT-B/16 closest to the RTL | `HAS_GVU_FA=0 HAS_GVU_LN=0 tools/has/tb_has_npu_top $FE_WORK/has/vit_full` |
| Slice / concat with merged dimensions | `HAS_DMA_ND=1 HAS_DMA_ND_MERGE=1 tools/has/tb_has_npu_top <slice / concat program>`, then `python3 tools/has/ref_program.py <dir> --image <snapshot>` |
| Attention requant overlapped with the core | `python3 tools/has/make_vit_heads_gate.py <dir>`, then `tools/has/tb_has_npu_top <dir>` with and without `HAS_GVU_FA_OVERLAP=1` |
| Softmax with exponent 0 | `python3 tools/has/make_vit_heads_gate.py <dir> --exp-e0 --heads 2 --tokens 64`, then `tools/has/tb_has_npu_top <dir>` (and `HAS_GVU_SOFTMAX_AS_RTL=1` as the negative control) |

## 14. RTL revision 1 (`HAS_NEO_REV`)

`HAS_NEO_REV` selects which revision of `neo_lite` the options of this document follow: 0 = RTL revision 0 (default), 1 =
RTL revision 1. It acts only together with the option of the part concerned (for example `HAS_GVU_REGS_NEO` for the
register file of the vector unit). With `HAS_NEO_REV=0`, values and cycles are identical to a tree without it
(MEASURED on the check programs, both builds). The run profiles `neo` and `neo_as_rtl` set `HAS_NEO_REV=1` and the
options of section 1 in one step (`COMPILER_GUIDE.md`, section 5).

The core is unchanged between the two revisions (RTL SOURCE), so section 9 holds for both.

### 14.1 Vector unit

| Change in RTL revision 1 | Model with `HAS_NEO_REV=1` |
|---|---|
| Register 12 (`POOLING_0`): output height and channel fields swapped | follows RTL revision 1 |
| CONTROL bit 3 forces the buffer swap on the last write of a command, and every command ends with a CONTROL write `start = 0` | both followed (the last write costs 3 to 7 cycles per command; values unchanged) |
| Dense max pool with stride 2 or 4: layout of the output in vbank2 | follows RTL revision 1 |
| The pool feeder is not reset between passes | follows RTL revision 1 |

MEASURED: the register-file port against an RTL simulation, 0 differences over 30,000 cycles for both revisions;
`HAS_NEO_REV=1` with the options of section 1: 20 of 20 check runs pass.

### 14.2 Output tiles that are not contiguous in DRAM

In RTL revision 1 the DRAM side of a two-dimensional DMA pointer is one contiguous region (address of repetition r = tile
offset + r x `btt`, RTL SOURCE). A tile whose DRAM runs are not adjacent therefore cannot be one command: with
`HAS_NEO_REV=1` the model re-opens a command for every run after the first (end of the command, configuration of a
new one, plus `HAS_DMA_DFC1D_RESTART` cycles, default 0, not measured) instead of the chained 15 cycles. This is a
lower bound of the price of the constraint, not a path the hardware has. `HAS_DMA_DFC1D_REPORT=1` prints the counters
(commands, tiles not contiguous, tiles that would need a third dimension, re-opened commands) at the end of a run.

| Item | Result |
|---|---|
| Address rule confirmed in an RTL simulation of `neo_top` | one output tile split into 1, 8, 200 and 800 commands with step = `btt`: output equal to the golden, 0 differences, in all four (MEASURED) |
| Output tiles not contiguous (MEASURED count) | YOLOv8m 7,292 of 7,299 (3,500 would be one command with a DRAM stride register, 3,792 need three dimensions); ViT-B/16 7,944 of 7,944 (all with one stride register) |
| Command cost, whole network | +13.76 million cycles for YOLOv8m, +2.71 million for ViT-B/16 if none of it overlaps the core (DERIVED from the counts; an upper bound of the exposed part) |
| Exposed part on check programs | 0.19 of the command cost on compute-bound programs, about 1.0 on DMA-bound programs (MEASURED) |
| Check programs | large slice / concat program +1,039,103 cycles, depthwise-separable check +112,663, YOLOv8m-shaped and reference checks +0 (MEASURED) |

Proposal to the hardware team: a DRAM stride register for vbank0 and vbank2 (0 = `btt`).

### 14.3 Core operand loads as one-dimensional commands (`HAS_DFC_CORE_DMA`)

The DFC of RTL revision 1 loads the operands of a core tile with one-dimensional commands, one after the other. With
`HAS_DFC_CORE_DMA=1` (and `HAS_NEO_REV=1`) every read transfer of a tile starts `commands + (commands − 1) x 15` cycles
late: the input window takes one command per contiguous DRAM run, the weights of a channel block one command, the bias
preload one command (ESTIMATE: how the hardware fills the partial sums is not known). MEASURED: reference check +0,
YOLOv8m-shaped check +201 cycles. Whole networks: average 1,074 commands per input window for YOLOv8m (at most 2,496)
and 768 for ViT-B/16 (MEASURED counts), about +132 million and +129 million cycles (ESTIMATE): see `COMPILER_GUIDE.md`,
section 7.3. Not part of the run profiles.

### 14.4 Command cost in core cycles

The DFC and the DMA run in the system clock, the core and the vector unit in the core clock, with a clock-domain
crossing on the configuration and memory ports (ratio 3 to 4.17 between the clocks, RTL SOURCE). The model counts core
cycles. MEASURED in an RTL simulation of `neo_top`: a chained command takes 37 to 62 system cycles, 9 to 21 core
cycles, about 11.5 to 16 core cycles per command on average, which matches the 15 of `HAS_DMA_DFC1D_CHAIN`. Converted
to core cycles, the first-command and end costs would be 6 and 3 instead of 19 and 7 (`HAS_DMA_DFC1D_FIRST`,
`HAS_DMA_DFC1D_TAIL`); MEASURED with these values on 7 check programs: at most 0.5 % fewer cycles (whole network below
0.05 %, ESTIMATE). The defaults of this release are 6 / 15 / 3 core cycles (`HAS_DMA_DFC1D_FIRST`, `HAS_DMA_DFC1D_CHAIN`, `HAS_DMA_DFC1D_TAIL`); 19 / 15 / 7 are the single-clock numbers of section 5, set by the three variables to reproduce the earlier releases.

### 14.5 Tile loop of the DFC

`has/rtl_ref_dfc_tile_loop_neo.h` is a port of the tile pointer generator of the DFC. MEASURED with
`tools/has/tb_dfc_tile_loop_neo`: the same tiles as the model on every layer of `insts_pe3`, `insts_pe3_nd` and
`vit_full`, in the three tile orders (the column order by swapping the roles of X and Y); weight loads equal in orders
0 and 2, different in order 1 on 12 of 83 YOLOv8m layers; the order of the input-channel passes differs on every layer
that splits the input channels (18 of 18).

### 14.6 The model against the whole `neo_top`

An RTL simulation of the whole `neo_top` (RTL revision 1; DFC, DMA, core, vector unit; Verilator) against the model, MEASURED:

| Program (from the hardware testbench) | Values | Core cycles per tile, start to done | Total core cycles of the RTL |
|---|---|---|---|
| RESIDUAL, 16 x 160 x 160 | 0 differences | RTL 419, model 413 (+6: clock crossing and interrupt) | 79,001 |
| MAXPOOL 3 x 3, stride 1 | 0 differences | RTL 14,991, model 14,986 | 305,526 |

The same register writes run through the model (`tools/has/tb_neo_top_program_hw.cpp`): 0 of 409,600 (RESIDUAL) and 0
of 243,360 (MAXPOOL) bytes differ from the golden of the hardware testbench; two negative controls are caught. A
GEMM_FUSED layer through the DFC of `neo_top` does not complete: the DFC stops while it configures the vector unit,
because it does not accept the write responses of those writes (MEASURED with probes).

### 14.7 Reproducing

| Item | Command |
|---|---|
| Command cost rules | `g++ -std=c++17 -O2 -I. tools/has/tb_dma_dfc_1d_neo.cpp -lsystemc -o t && ./t` |
| Output tiles not contiguous | `tools/has/tb_dfc_noncontig_neo <mmio.txt> [--csv <file>]` |
| Core operand commands | `tools/has/tb_dfc_core_dma_neo <mmio.txt>` |
| Tile loop | `tools/has/tb_dfc_tile_loop_neo <mmio.txt>` |
| One program with RTL revision 1 | `HAS_NEO_REV=1 HAS_DMA_DFC_1D=1 tools/has/tb_has_npu_top <program dir>` (and the other options of section 1), or `HAS_PROFILE=neo` |
| Hardware testbench programs through the model | `g++ -std=c++17 -O2 -Wall -I. tools/has/tb_neo_top_program_hw.cpp -o t && ./t all --neg all` |
| RTL simulation of `neo_top` | not part of this package |
