# Programming one layer through the neo_lite register file

This document is the register-level reference for software that drives the NPU the way the `neo_lite` hardware is
driven: one layer = one image of 82 configuration words of the data flow controller (DFC), then START. The same image
runs on the model through a system TLM wrapper, on an RTL simulation of `neo_top`, and on an FPGA; only the base
addresses and two wrapper-specific writes differ (section 2).

The reference is the hardware repository `neo_lite` RTL revision 1. RTL revision 0 and 1 are the two hardware RTL revisions the model follows; `HAS_NEO_REV` selects them. The model reads this register file only when
the front end `HAS_HOST` is `neo` (the default, section 6). With `HAS_HOST=v45` the window is off and the model is
programmed with the instruction registers of `SW_INTEGRATION_GUIDE.md` only; nothing in this document applies then.

`COMPILER_GUIDE.md` says which layers can be expressed in this register file today and what a compiler must do for the
others. Labels: **MEASURED** = a run of the model or of an RTL simulation; **RTL SOURCE** = read from the RTL, not
simulated; **DERIVED** = a consequence of the two.

## 1. Address windows

The CPU talks to one block, the controller window of `df_controller_top`. The other configuration windows of `neo_top`
(core, vector unit, DMA) are written by the DFC itself while a layer runs.

| Window | FPGA / RTL simulation of `neo_top` | Model through the system wrapper |
|---|---|---|
| Controller (this document) | `cfg_base + 0x0000` (`cfg_base` = `0x80000000` in the hardware testbench; the absolute base on the FPGA is not decided yet) | model address `0x40000800` = offset `0x10800` of the wrapper's NPU window |
| Core configuration | `cfg_base + 0x1_0000` | not used (written by the model's DFC) |
| Vector unit | `cfg_base + 0x2_0000` | not used |
| DMA | `cfg_base + 0x3_0000` | not used |
| DRAM addresses inside the words | system addresses (`0x40000000` in the hardware testbench) | system addresses; the model subtracts its DRAM base (`set_dram_base()`, default `0x80000000`) |

Inside the controller window:

| Byte offset | Register | Access | Content |
|---|---|---|---|
| `0x000` | `CONTROL` | R / W | bit 0 START (write 1; self-clearing), bit 1 DONE (read 1 after completion; write 1 to clear); read bits 31:24 = `0xC0` |
| `0x008` | `IEN` | R / W | bit 0 interrupt enable |
| `0x00C` | `ISTATUS` | R / W1C | bit 0 interrupt pending; write 1 to clear |
| `0x010 + 4 x i` | word `i`, `i` = 0 to 81 | R / W | configuration image (section 3); last word at `0x154` |

In the RTL the word index is taken from address bits 8:2, so offsets from `0x200` alias down to `0x000` (RTL SOURCE).

## 2. Sequence for one layer

### 2.1 FPGA and RTL simulation

1. Write words 0 to 81 in any order.
2. Write `IEN = 1`, then `CONTROL = 1` (START).
3. Wait for the interrupt (`ISTATUS & IEN`) or poll `CONTROL` bit 1 (DONE).
4. Write `ISTATUS = 1` and `CONTROL = 2` to clear.

The DFC copies the image into a shadow at the START edge and reads words 0 to 39 once. It then sends the core
configuration (29 words + 3 interrupt writes) and the 14 vector-unit writes once per layer, and loops over the tiles on
its own. For every tile it writes only the output meta word of the core (word 68: `cin_tile_last`, `oc_tile_last`), issues
the DMA commands, starts the core and the vector unit, and clears the interrupts (RTL SOURCE). The vector-unit register 0
(`CONTROL` of the vector unit, not part of the 82 words) is written by the DFC for every tile: bit 0 starts the unit, bit 2
clears its done flag, and bit 3 forces the buffer swap on the last start of the layer. There is no instruction queue: one
layer at a time. The CPU may write the next image while a layer runs; a second START is ignored until DONE.

All 82 words and `IEN` are 0 after reset, and the image is kept between layers: software rewrites only the words that
change.

### 2.2 Model through the system wrapper

The same 84 writes, at the model addresses of section 1, framed by two writes the wrapper needs to stage the DRAM
image (it copies the program's DRAM window into the model when a command is pushed, and back after completion):

1. Once at start-up: `RICH_WINDOW_BASE` / `RICH_WINDOW_SIZE` of the wrapper bank = location and size of the program's
   DRAM image in system RAM (`SW_INTEGRATION_GUIDE.md`, section 10.1). The window must cover every tensor, table and
   parameter block of the program and lie in the RAM range the wrapper maps to the model (`0x80000000` to
   `0x8FFFFFFF` in the reference platform). For an interrupt, enable it in the wrapper's own control and interrupt
   registers.
2. For every layer:
   1. write `0` to `0x400004E4` (offset `0x104E4`): a marker that makes the wrapper stage the DRAM window; the model
      consumes it only when the value is 0 (any other value goes on to the mask-address register of FUSED_ATTN);
   2. write the 82 words at `0x40000810 + 4 x i`, then `IEN = 1` at `0x40000808` and `START = 1` at `0x40000800`;
      START only arms the command: in the model `CONTROL` bit 0 reads 1 until the doorbell is written, where the RTL clears it
      at once, so software must not poll it to learn that START was accepted;
   3. write `0x17` to `PUSH_A` (`0x40000310`, offset `0x10310`): the doorbell. The model decodes the image, checks it
      (section 5) and queues the layer in the same queue as the instructions of `SW_INTEGRATION_GUIDE.md`;
   4. wait for the wrapper's completion (its `STATUS` register or interrupt), then always read the model's `STATUS`
      (`0x40000318`, offset `0x10318`) and check that `RETIRED` (`0x4000031C`) grew: the wrapper reports completion
      also for a layer the model refused. Clear as in 2.1 step 4 and clear the wrapper's completion and interrupt
      status.

The front end exists from this release on; the model revision built into the wrapper repository before it has no
window at `0x40000800` and refuses `0x17`.

Why START does not run the layer by itself: the wrapper copies the DRAM image at the doorbell, so a layer started by
START could run before its data is in place. A doorbell without an armed START is refused.

Programs may mix the two front ends: layers that the register file can express go through this window, the others
through the instruction registers, in one queue (MEASURED on three check programs: same DRAM image and same cycle count
as the same program through the instruction registers only).

`host_trace_replay --emit-wrapper-fw <file> <trace>` prints the write list of steps 2.1 to 2.3 for firmware;
`--emit-neo-mmio` writes a program directory in which the runnable layers use the window (section 7).

## 3. The 82 words

Word `i` is at byte `0x010 + 4 x i` of the controller window. Words 0 to 39 configure the DFC, 40 to 68 the core, 69
to 81 the vector unit. Column "SAURIA" gives the index of the same value in `args[]` of
`driver/libsauria_cfg.h::sauria_encode_controller_args`, which can be reused unchanged for words 0 to 20 and 38.

Notation: `C_w`, `C_h`, `C_k` = width, height, channels of the output; `A_c` = input channels; `_til` = of one tile;
`A_w`, `A_h` = input width and height including the kernel border; `s` = stride; `B_w`, `B_h` = kernel size.

### 3.1 Words 0 to 39: data flow controller

| Word | Byte | Bits | Field | Meaning | SAURIA |
|---|---|---|---|---|---|
| 0 | `0x10` | 15:0 / 31:16 | `x_lim` / `y_lim` (words 0 to 11 cannot be written over the bus in the delivered RTL, code `A-H1`) | tiles along X − 1 / along Y − 1 | a[0] |
| 1 | `0x14` | 15:0 / 31:16 | `c_lim` / `k_lim` | input-channel tiles − 1 / output-channel tiles − 1 | a[1] |
| 2 | `0x18` | 31:0 | psum `x_step` | output tile step along X (elements) = `C_w_til` | a[2] |
| 3 | `0x1C` | 31:0 | psum `y_step` | `C_h_til x C_w` | a[3] |
| 4 | `0x20` | 31:0 | psum `k_step` | `C_k_til x C_w x C_h` | a[4] |
| 5 | `0x24` | 31:0 | ifmap `x_step` | `s x C_w_til` | a[5] |
| 6 | `0x28` | 31:0 | ifmap `y_step` | `s x C_h_til x A_w` | a[6] |
| 7 | `0x2C` | 31:0 | ifmap `c_step` | `A_c_til x A_w x A_h` | a[7] |
| 8 | `0x30` | 31:0 | weight `k_step` | `C_k_til x B_w x B_h x A_c` | a[8] |
| 9 | `0x34` | 31:0 | weight `c_step` | `C_k_til x B_w x B_h x A_c_til` | a[9] |
| 10 | `0x38` | 31:0 | DMA ifmap `y_lim` | DMA commands of an input tile along Y − 1 | a[10] |
| 11 | `0x3C` | 31:0 | DMA ifmap `c_lim` | along C − 1 | a[11] |
| 12 | `0x40` | 31:0 | DMA psum `y_step` | `C_w` | a[12] |
| 13 | `0x44` | 31:0 | DMA psum `k_step` | `C_h x C_w` | a[13] |
| 14 | `0x48` | 31:0 | DMA ifmap `y_step` | `A_w` | a[14] |
| 15 | `0x4C` | 31:0 | DMA ifmap `c_step` | `A_h x A_w` | a[15] |
| 16 | `0x50` | 31:0 | DMA weight `w_step` | `C_k_til` | a[16] |
| 17 | `0x54` | 31:0 | DMA ifmap `ett` | elements per input DMA command | a[17] |
| 18 | `0x58` | 31:0 | input base | DRAM address of tensor A (the RTL name `start_SRAMA_addr` is a DRAM address) | a[18] |
| 19 | `0x5C` | 31:0 | weight base | DRAM address of the weights | a[19] |
| 20 | `0x60` | 31:0 | output base | DRAM address of C (partial sums in / results out) | a[20] |
| 21 | `0x64` | 31:0 | vector-unit `tile_lim` | tiles of a vector-unit-only command − 1; bit 18 also selects the rule "START = not word 38 bit 22" | — |
| 22 | `0x68` | 31:0 | sbank `btt` | bytes per sbank DMA command (not used by the RTL yet) | — |
| 23 | `0x6C` | 31:0 | sbank `tile_move` | sbank DRAM step per tile | — |
| 24 | `0x70` | 31:0 | vbank0 `btt` | bytes per vbank0 DMA command | — |
| 25 | `0x74` | 31:0 | vbank0 `stride` | local address step between repetitions | — |
| 26 | `0x78` | 31:0 | vbank0 `rep_lim` | repetitions in a tile − 1 | — |
| 27 | `0x7C` | 31:0 | vbank0 `tile_move` | vbank0 DRAM step per tile | — |
| 28 | `0x80` | 31:0 | vbank1 `btt` | bytes per vbank1 command (B of RESIDUAL; the Softmax table) | — |
| 29 | `0x84` | 31:0 | vbank1 `tile_move` | vbank1 DRAM step per tile | — |
| 30 | `0x88` | 31:0 | vbank2 `btt` | bytes per vbank2 → DRAM command | — |
| 31 | `0x8C` | 31:0 | vbank2 `stride` | local step | — |
| 32 | `0x90` | 31:0 | vbank2 `rep_lim` | repetitions − 1 | — |
| 33 | `0x94` | 31:0 | vbank2 `tile_move` | vbank2 DRAM step per tile | — |
| 34 | `0x98` | 31:0 | sbank base | DRAM address of the sbank data | — |
| 35 | `0x9C` | 31:0 | vbank0 base | DRAM address of vbank0 data (input; also the Softmax output) | — |
| 36 | `0xA0` | 31:0 | vbank1 base | DRAM address of vbank1 data | — |
| 37 | `0xA4` | 31:0 | vbank2 base | DRAM address of the vector-unit output | — |
| 38 | `0xA8` | 17:16 | `loop_order` | tile loop order (`COMPILER_GUIDE.md`, section 6) | a[21] |
| 38 | | 18 | `stand_alone` | 1 = the core runs without the DMA controller | — |
| 38 | | 21:19 | keep A / B / C | keep a buffer between tiles in stand-alone mode | — |
| 38 | | 22 | — | START rule when word 21 bit 18 = 1 | — |
| 38 | | 23 / 24 / 25 | `Cw_eq` / `Ch_eq` / `Ck_eq` | the tile covers the whole W / H / K | a[21] |
| 38 | | 31 | `WXfer_op` | 1 = a whole weight tile in one DMA command | a[21] |
| 39 | `0xAC` | 3:0 | opcode | 0 GEMM_FUSED, 1 SOFTMAX, 2 FUSED_ATTN, 3 LAYERNORM, 4 RESIDUAL, 5 MAXPOOL, 6 AVGPOOL | — |

The DFC derives the remaining DMA limits and element counts from these words every cycle, as the original SAURIA
controller does.

### 3.2 Words 40 to 68: core configuration

The bit layout is the SAURIA configuration layout of `sauria_cfg_layout.h` (`SAURIA_CFG_LAYOUT`, the same order and
field widths, including the word alignments), produced by `driver/libsauria_cfg.h`. Field widths for the 32 x 32
int8 configuration: activation and weight indices 17 bits, output indices 16 bits.

| Region | Words | Bytes | Fields in order |
|---|---|---|---|
| CON | 40 to 41 | `0xB0` to `0xB4` | `incntlim`, `act_reps`, `wei_reps`, `thres[2]`, `sram_deepsleep`, `sram_powergate` (keep the last two 0) |
| ACT | 42 to 58 | `0xB8` to `0xF8` | `xlim`, `xstep`, `ylim`, `ystep`, `chlim`, `chstep`, `til_xlim`, `til_xstep`, `til_ylim`, `til_ystep`, `dil_pat[64]`, `rows_active[32]`, `loc_woffs[32 x 8]` |
| WEI | 59 to 63 | `0xFC` to `0x10C` | `wlim`, `wstep`, `auxlim`, `auxstep`, `til_klim`, `til_kstep`, `cols_active[32]`, `waligned` |
| OUT | 64 to 68 | `0x110` to `0x120` | `ncontexts`, `cxlim`, `cxstep`, `cklim`, `ckstep`, `til_cylim`, `til_cystep`, `til_cklim`, `til_ckstep`, `inactive_cols[8]`, `preload_en`, then four bits new in `neo_lite`: `transpose_mode`, `psum_stream_en`, `cin_tile_last`, `oc_tile_last` |

The four new bits are bits 25 to 28 of word 68. The DFC overwrites the last two for every tile. `psum_stream_en`
(word 68 bit 26) streams the partial sums of the last input-channel tile into the vector unit; the vector unit waits
for this stream in GEMM_FUSED (RTL SOURCE). The encoder of this package leaves the bit at 0, and the model does not
read it; whether GEMM_FUSED through the DFC needs it set is open (`KNOWN_LIMITATIONS.md`).

### 3.3 Words 69 to 81: vector unit

Word `69 + j` is sent by the DFC to vector-unit register `j + 1` (byte `0x4 + 4 x j` of the vector-unit window); 14
writes per command, the last one writes 0 to an address past the 14 registers (RTL SOURCE). The layout is the model's
`has/gvu_regs_dfc.h`, namespace `neo` (registers 1 to 13).

| Word | Byte | Vector-unit register | Fields |
|---|---|---|---|
| 69 | `0x124` | 1 `OPCODE` | overwritten by the DFC from word 39: GEMM_FUSED → 2, SOFTMAX → 3, RESIDUAL → 6, MAXPOOL → 7, others → 2. The vector unit knows more codes (4 FUSED_ATTN, 5 LAYERNORM, 8 AVGPOOL), but the DFC never sends them |
| 70 | `0x128` | 2 `LUT` | bit 0 reset the GEMM table, bits 1 / 2 Softmax tables, bit 3 LayerNorm table (pulses) |
| 71 | `0x12C` | 3 `GEMM_FUSED` | bit 0 `skip_act`, bits 15:8 output zero point (int8) |
| 72 | `0x130` | 4 `SOFTMAX_0` | bits 15:0 `token_len`, 31:16 `num_tile` |
| 73 | `0x134` | 5 `SOFTMAX_1` | `attn_scale` |
| 74 | `0x138` | 6 `SOFTMAX_2` | bits 7:0 `attn_shift`, 15:8 `attn_z` |
| 75 | `0x13C` | 7 `RESIDUAL_0` | bits 15:0 `sequence_len` (32-byte vectors), 23:16 `out_z`, 29:24 `out_shift`, 30 `sub_mode` |
| 76 | `0x140` | 8 `RESIDUAL_1` | multiplier of A |
| 77 | `0x144` | 9 `RESIDUAL_2` | multiplier of B |
| 78 | `0x148` | 10 `RESIDUAL_3` | bias bits 31:0 |
| 79 | `0x14C` | 11 `RESIDUAL_4` | bias bits 63:32 |
| 80 | `0x150` | 12 `POOLING_0` | bits 1:0 window − 2, 4:2 stride, 15:8 vectors along W, 23:16 OH, 31:24 C (this order is that of RTL revision 1; the earlier revision had C and OH swapped) |
| 81 | `0x154` | 13 `POOLING_1` | bits 9:0 step along H, 25:16 step along C |

RESIDUAL computes `out = clamp(((a x SA + b x SB + bias) >> out_shift), −256, 255) + out_z`, clamped to int8. The
model's ADD (zero points, one multiplier and shift per operand) is mapped onto it when `SA` and `SB` are at most
4,000,000 and the zero points are folded into the bias (`COMPILER_GUIDE.md`, section 4).

The proposed vector-unit registers 14 to 31 of the model (address generator, lane LayerNorm, average pool) have no
word in this image.

## 4. Completion and status

| Where | FPGA / RTL | Model through the wrapper |
|---|---|---|
| End of layer | `CONTROL` bit 1 DONE = 1, `ISTATUS` bit 0 = 1, interrupt if `IEN` | the same bits in the window, after the layer retires; the wrapper raises its own completion and interrupt from the model's queue |
| Refusal at the doorbell | not applicable | model `STATUS` (`0x40000318`) bit 2 = 1 (sticky error), bits 31:16 = 4 (`UNSUPPORTED_OP`); `RETIRED` does not grow; DRAM is not touched; one line starting with `[neo] push refused:` and the reason codes on stderr (a full queue sets error 1, `Q_OVF`, without a line) |
| Count of completed layers | — | `RETIRED` (`0x4000031C`), shared with the instruction registers |

A refused layer is never run another way. The error is sticky until `STATUS` is written (any value), as for the
instruction registers (`SW_INTEGRATION_GUIDE.md`, section 3.2).

## 5. What the model checks at the doorbell

The model decodes the image back into a layer description and checks that the hardware RTL revision 1 would run
it as written. Any reason code of class A or B refuses the layer (`HAS_HOST=neo`); `HAS_HOST=neo_patched` also accepts
the class-A codes listed in `HAS_HOST_NEO_PATCHES` (default `A-H1,A-H4`) as if those faults of the RTL were fixed, and
prints `accepted under patch <code>` once per code; it never accepts a class-B code. The codes and what a compiler does about each are in `COMPILER_GUIDE.md`, section 4.

Additional refusals of the decoder itself: a tile count that does not divide the tensor, an opcode without meaning,
a doorbell without an armed START.

MEASURED (strict mode): RESIDUAL and MAX_POOL images within the field ranges run; every GEMM_FUSED image of the
reference programs is refused (codes `A-H1`, `A-H4` and others), with `STATUS` `0x00040004`.

`neo_patched` is not a default. It assumes a layout for the per-channel requant data that the RTL does not define yet:
`scale[cout] | shift[cout] | bias[cout]` (32-bit words) at the sbank base (word 34) and the activation table at the
vbank1 base (word 36), activation only none or SiLU through the table. A GEMM_FUSED layer end to end through this mode
has not been run.

## 6. Options

| Environment variable (read once, when `HasNpuTop` is built) | Default | Effect |
|---|---|---|
| `HAS_HOST` | `neo` (when not set) | `neo`: the controller window of section 1 is served (strict). `neo_patched`: also the class-A codes of `HAS_HOST_NEO_PATCHES`. `v45` or any other value: the window does not exist, and a push of `0x17` is refused as an unknown opcode. A program that does not write the window runs identically with either value, byte for byte and cycle for cycle (MEASURED on the check programs) |
| `HAS_HOST_NEO_PATCHES` | `A-H1,A-H4` | class-A codes accepted by `neo_patched` |
| `HAS_HOST_NEO_QUIET` | 0 | 1 = no stderr line per refused layer or per layer accepted in `neo_patched` mode |
| `HAS_NEO_REV` | 0 | which RTL revision the `neo_lite` options follow: 0 = RTL revision 0, 1 = RTL revision 1 (`HW_RTL_ALIGNMENT.md`, section 14). The window always decodes the RTL revision 1 layout |

The front end changes only how a layer reaches the model, not how it runs: a layer accepted through the window runs
the same cycles as the same layer through the instruction registers (MEASURED, section 2.2). To run with the hardware
behaviour, combine it with the run profile `neo` (`COMPILER_GUIDE.md`, section 5).

## 7. Tools

| Tool | Use |
|---|---|
| `driver/libneo_cfg.h` | C++ encoder: layer description → 82 words (reuses `libsauria_cfg.h` for words 0 to 20, 38 and 40 to 68); address parameters and functions of the RTL (`neo::addr`, `dfc_word_addr`) and the field table `neo::FIELDS` (`COMPILER_GUIDE.md`, section 1.1) |
| `has/has_host_neo.h` | Decoder (82 words → layer description, independent of the encoder) and the check of section 5 (`plan()`, reason codes) |
| `tools/has/host_trace_gen.cpp` | `HAS_HOST_TRACE=<file> host_trace_gen <program dir>`: writes the register trace of a program (one `I` line per layer with its codes, its `W` lines, `X` lines for every field the register file cannot carry). No SystemC needed |
| `tools/has/host_trace_replay.cpp` | `host_trace_replay [--hw] [--emit-mmio f] [--emit-neo-mmio f] [--emit-wrapper-fw f] [--compare dir] <trace>`: `--hw` refuses a trace with any `X` line (exit code 3); `--compare` checks that the trace decodes back to the program |
| `tools/has/tb_has_neo_host.cpp` | Runs a program through `HasNpuTop` twice (instruction registers; register-file window) and compares DRAM and cycles |
| `tools/has/tb_core_rtl_neo.cpp` | The same through the drop-in model root `core_rtl/` in the way the system wrapper calls it |
| `tools/has/neo_regmap_check.py` | Compares this register map with the RTL source or with the frozen table `tools/has/neo_regmap_rev1.txt` (`COMPILER_GUIDE.md`, section 1.1) |
| `tools/has/hw_check_program.py` | Per-layer verdict and codes without building the model (`COMPILER_GUIDE.md`, section 3) |

Trace format (`neo_trace v1`): header `# neo_trace v1 neo_lite RTL revision 1 words=82 cfg_base=<hex> dram_base=<hex>`; `D in`
/ `D golden` lines name the DRAM images; per layer `I <n> <op> host_seq=<k> hw_runnable=<0|1> issues=<codes>`, then
`W <address> <value>` (hex) and `X <field> <value> <code>` lines; `H <host step>` lines for steps that run on the CPU.
Only the `W` lines of layers without `X` lines can be sent to the hardware.
