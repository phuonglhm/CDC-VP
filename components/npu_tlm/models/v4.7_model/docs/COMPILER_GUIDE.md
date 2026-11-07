# Compiler Guide: programs that run on the model, through the system wrapper and on the FPGA

This guide is for the team that writes the compiler. It answers four questions: which layers the hardware (`neo_lite` RTL revision 1)
runs as written, how to run a compiled program on the model so that the result says something about
the hardware, what the compiler must change in a layer that the hardware cannot run as written, and how to check the
output of a compiled program. RTL revision 0 and 1 are the two hardware RTL revisions the model follows; `HAS_NEO_REV` selects them.

Two ways to program the NPU exist side by side:

| Front end | Registers | Where it runs | Document |
|---|---|---|---|
| Instruction registers (default) | about 25 writes per layer + push of the opcode | the model, directly and through the system wrapper | `SW_INTEGRATION_GUIDE.md`, `INTERFACE_SPEC.md`, section 6 |
| neo_lite register file (`HAS_HOST`, `neo` by default) | 82 configuration words + IEN + START per layer | the model through the system wrapper (and its testbenches), an RTL simulation of `neo_top`, the FPGA | `NEO_REGISTER_FILE.md` |

The instruction registers carry more than the hardware has (padding, edge tiles, per-channel requant tables, the
transformer instructions, slice / concat descriptors). A program that passes on the default model is therefore not yet
a program for the hardware. The tools below tell, per layer, whether it is.

Labels: **MEASURED** = a run of the model or of an RTL simulation; **RTL SOURCE** = read from the RTL, not simulated;
**ESTIMATE** = a number that stands in for a measurement not yet made.

## 1. Procedure

1. Compile the network to a program directory (`mmio.txt`, `dram_init.bin`, `dram_golden.bin`, `manifest.json`;
   `INTERFACE_SPEC.md`, section 6.5) with the reference frontend or your own (`FRONTEND_GUIDE.md`).
2. Check it on the default model: `tools/has/tb_has_npu_top <program dir>` must end with `RESULT: PASS` (every
   instruction's output compared with `dram_golden.bin`). Optionally `python3 tools/has/ref_program.py <program dir>`
   (an independent integer reference, section 9).
3. Get the per-layer hardware verdict: `python3 tools/has/hw_check_program.py <program dir>` (section 3). Fix in the
   compiler every layer whose verdict is `TRANSFORM` (section 4.1). Layers with `PATCH` or `NO` wait for the hardware or
   must be mapped differently (section 4.2).
4. Run on the model with the hardware behaviour: `HAS_PROFILE=neo tools/has/tb_has_npu_top <program dir>` (section 5):
   PASS and a cycle count closer to the hardware.
5. For a comparison with an RTL simulation or the FPGA: write the register trace
   (`HAS_HOST_TRACE=<file> host_trace_gen <program dir>`), and the DRAM image the RTL should produce
   (`HAS_PROFILE=neo_as_rtl`, section 9). Only layers without `X` lines in the trace can be sent to the hardware.
6. Through the system wrapper: either front end, with the model root `core_rtl/`
   (`SW_INTEGRATION_GUIDE.md`, section 10); with `HAS_HOST=neo`, the layers that the register file can carry go
   through it (`NEO_REGISTER_FILE.md`, section 2.2).

### 1.1 Addressing rule

A compiler that targets the hardware register file produces **offsets**, never absolute addresses.

- **Offsets** are relative to the configuration base of `neo_top`. The data flow controller (DFC) block is at offset
  `0x0_0000`: `CONTROL` at byte `0x000` (bit 0 START), `IEN` at `0x008`, the interrupt status to clear at `0x00C`, and the
  82 configuration words at byte `0x010 + 4 x i`, `i` = 0 to 81 (`NEO_REGISTER_FILE.md`, sections 1 and 3).
- **The configuration base and the DRAM base are parameters of the firmware or driver**, not of the compiler (the absolute base address of `neo_top` on the FPGA is not decided yet). The encoder
  `driver/libneo_cfg.h` takes both as arguments. Its namespace `neo::addr` holds the 26 parameters of the RTL address package
  under the RTL names (the block offsets `0x0`, `0x1_0000`, `0x2_0000`, `0x3_0000` and the mask `0xF_0000`; the core register
  regions `0x200`, `0x400`, `0x600`, `0x800`; the SRAM, vbank and sbank offsets; the bus addresses `BUS_*`), and these
  functions: `cfg_addr`, `block_of`, `dfc_word_addr(i, cfg_base)` (= block base + `0x10` + 4 x i), `core_word_addr`,
  `core_reg_addr`, `gvu_reg_addr`, `gvu_word_addr`, `bus_addr(mem, off)` and `dram_addr(dram_base, off)`. The table
  `neo::FIELDS` (115 rows) gives the position and width of every field of the 40 DFC words, the 13 vector-unit words and
  the controller window; `neo::addr::CORE_EXTRA` lists the six core bits that the RTL has and the model leaves at 0
  (`sram_deepsleep`, `sram_powergate`, `transpose_mode`, `psum_stream_en`, `cin_tile_last`, `oc_tile_last`). The DRAM
  addresses inside the 82 words are system addresses (the DRAM base plus the offset of the tensor in the program image).

What differs between the model through the system wrapper and the FPGA (or an RTL simulation of `neo_top`):

| Item | FPGA / RTL simulation | Model through the wrapper |
|---|---|---|
| Base of the DFC block | configuration base of `neo_top` in the SoC (`0x80000000` in the hardware testbench; the absolute base on the FPGA is not decided yet) + `0x0` | wrapper window offset `0x10800`, that is model address `0x40000800` |
| DRAM base | system address of the DRAM (`0x40000000` in the hardware testbench) | the model subtracts its DRAM base (`0x80000000` by default) from the system addresses in the words |
| Start of a layer | write `CONTROL` START | write the marker `0x400004E4 = 0`, the words, `IEN`, `CONTROL` START (arms), then the doorbell `0x17` to `PUSH_A` |
| End of a layer | `DONE` and the interrupt; `CONTROL` bit 0 clears by itself | the same bits after the layer retires; `CONTROL` bit 0 stays 1 until the doorbell; read the model `STATUS` and `RETIRED` as well (`NEO_REGISTER_FILE.md`, section 2.2) |
| Core, vector unit and DMA windows (`+0x1_0000`, `+0x2_0000`, `+0x3_0000`) | the CPU may write them directly | not served: the model writes them itself while it runs a layer |

**Warning.** Operations that exist only in the model (DMA copy, slice, concat, upsample, reshape, and the transpose after
the epilogue) are programmed through the model's own instruction registers (`SW_INTEGRATION_GUIDE.md`, section 3); the RTL
has no address for them. A compiler that targets
the FPGA therefore uses only the DFC window of `NEO_REGISTER_FILE.md` and checks every program with
`HAS_PROFILE=neo HAS_HW_CHECK=1` (section 3).

**Checking the register map against the RTL.** `tools/has/neo_regmap_check.py` compares the register map used by the encoder
and decoder with the RTL source:

```bash
python3 tools/has/neo_regmap_check.py --rtl <neo_lite source tree>     # reads the RTL, writes nothing there
python3 tools/has/neo_regmap_check.py --table tools/has/neo_regmap_rev1.txt   # against the frozen table, no RTL tree needed
```

Exit code 0 means no difference. It compares the address constants (both directions), the opcode enumerations, every field
of the DFC, vector-unit and controller words, and the 42 core fields, in position and width (not bit order inside a
field, and not meaning). Measured on this tree against RTL revision 1: 432 checks, 0 differences, 82 of 82 words covered, in
both modes; eight deliberate faults in a copy were each caught with the RTL file and line. Run it whenever the RTL has a new
commit, before regenerating register traces; when it reports a difference, do not use the old map. It knows one
configuration (32 x 32 int8).

## 2. What the hardware RTL revision 1 has

| Item | Hardware | Source |
|---|---|---|
| Vector-unit operations started by the DFC | GEMM_FUSED, SOFTMAX, RESIDUAL, MAXPOOL. FUSED_ATTN, LAYERNORM and AVGPOOL have an opcode number but no DFC path: the vector unit receives GEMM_FUSED | RTL SOURCE |
| LayerNorm | no data path: the opcode leaves the vector-unit state machine idle and the command never completes | RTL SOURCE |
| FUSED_ATTN | not a vector-unit operation: GEMM_FUSED (Q x K^T) + SOFTMAX + GEMM_FUSED (A x V) | RTL SOURCE |
| DMA | one-dimensional commands, 128 bits per beat; no slice / concat descriptor, no copy instruction | RTL SOURCE |
| Tile loop | uniform grid: `cout mod cout_t = 0`, `oh mod h_t = 0`, `ow mod w_t = 0`; one core configuration for all tiles of a layer | RTL SOURCE; tile loop compared with the model, section 6 |
| Padding | none: the padding block is not connected to `neo_top`, offsets are unsigned; tensors must be padded in DRAM | RTL SOURCE |
| Per-channel scale / shift / bias | must be in the sbank; the DFC has no command to load the sbank yet | RTL SOURCE |
| Scratchpad | sbank 768 words (614 for scales); vbank0 800 rows of 32 bytes (25,600 bytes), vbank1 200 rows and vbank2 256 rows of 128 bytes (25,600 and 32,768 bytes) | RTL SOURCE |
| Softmax | row length at most 800 elements | RTL SOURCE |
| Max pool | window 2 to 5, stride at most 7; stride 2 or 4 with a window different from the stride miscounts | RTL SOURCE; miscount MEASURED in an RTL simulation |
| Residual add | one multiplier per operand, one output shift, a 64-bit bias, no separate zero points (`NEO_REGISTER_FILE.md`, section 3.3) | RTL SOURCE |
| Rounding and narrowing | round half away from zero, 16-bit wrap (not saturating) | RTL SOURCE |
| Transpose after the epilogue | no (the vertical scan of the core is wrong from the third context on) | MEASURED in an RTL simulation |
| Core SRAM depths | A / B 2,048 rows, C 1,024 rows (32 x 32 int8) | RTL SOURCE |

Open defects of this revision that a compiler cannot work around (the model does not
imitate them unless a run profile says so): words 0 to 11 of the register file cannot be written over the bus (code
`A-H1`; MEASURED in an RTL simulation: 12 of 82 words wrong as delivered, 0 of 82 when the address compare uses the word index); the
LUT initialisation reads the wrong vector-unit word (`P-H2`); FUSED_ATTN, LAYERNORM and AVGPOOL have no DFC path
(`A-H3`); the sbank is never loaded (`A-H4`); dense max pool miscount (`P-POOLDENSE`).

## 3. Per-layer verdict

```bash
python3 tools/has/hw_check_program.py <program dir | mmio.txt> [--quiet] [--csv <file>]   # no SystemC, no numpy
HAS_HW_CHECK=1 tools/has/tb_has_npu_top <program dir>                                     # the same lines during a model run
```

One line per instruction: `[hw-check] i=<n> op=<name> HW revision 1: OK | TRANSFORM | PATCH | NO <codes>`. The verdict is
the worst code of the layer:

| Verdict | Meaning |
|---|---|
| `OK` | the hardware runs the layer as written |
| `TRANSFORM` | only codes that the compiler can remove (section 4.1) |
| `PATCH` | the layer would run if the fault were fixed: a class-A code (an RTL fault or gap that a small RTL change removes), nothing worse |
| `NO` | a code that only the hardware or the model can remove (section 4.2) |

`HAS_HW_CHECK=1` does not change values or cycles (MEASURED). The C++ check and the Python tool give the same codes on
the reference programs and check programs (MEASURED, 13 of 13 programs).

Reference programs (MEASURED):

| Program | Instructions | OK | TRANSFORM | PATCH | NO |
|---|---|---|---|---|---|
| YOLOv8m `insts_pe3` (concat by address) | 98 | 12 | 3 | 0 | 83 |
| YOLOv8m `insts_pe3_nd` (slice / concat instructions) | 135 | 12 | 3 | 0 | 120 |
| ViT-B/16 `vit_full` | 244 | 25 | 0 | 0 | 219 |

The 12 / 25 `OK` layers are the residual adds; the 3 `TRANSFORM` layers are the SPPF max pools. **No GEMM_FUSED layer
of either network runs on RTL revision 1 as written**: every one carries `A-H1`, `A-H4` and `B-REQ3`, and SiLU / GELU layers
also `B-ACTLUT`.

## 4. Support matrix

The hardware RTL revision 1 against the model, per operation and option. "Model accepts" is the default model;
"HW accepts" is what the hardware runs as written; the last column gives the reason code when it cannot
(codes below).

| Operation or option | Model accepts | HW revision 1 accepts | Code when the hardware cannot |
|---|---|---|---|
| GEMM_FUSED, tile grid uniform, input pre-padded, per-channel scale in the sbank | yes | no (the register words 0 to 11 cannot be written; the sbank is not loaded; bias, shift and activation table have no word) | `A-H1`, `A-H4`, `B-REQ3`, `B-ACTLUT` |
| GEMM_FUSED with padding, edge tiles, `PAD_TAIL`, input size different from the grid | yes | no | `B-PAD`, `B-EDGE`, `B-PADTAIL`, `B-INDIM` |
| GEMM_FUSED with real-valued scales, unequal strides | yes | no | `B-FSCALE`, `B-STRIDE` |
| GEMM_FUSED with an input-channel split | yes | order differs | `B-CINORD` |
| Activation: none | yes | yes | — |
| Activation: SiLU, GELU (table) | yes | no word for the table | `B-ACTLUT` |
| ADD within the RESIDUAL parameter ranges | yes | yes | — |
| ADD outside them, or length not a multiple of 32 | yes | no | `B-ADDMAP`, `B-ADDTILE` |
| MAX_POOL, window 2 to 5, no padding | yes | yes (stride 2 or 4 with a different window miscounts) | `P-POOLDENSE` |
| MAX_POOL with padding, window outside 2 to 5, stale fields | yes | no | `B-POOLPAD`, `B-POOLK`, `B-POOLFIT` |
| AVG_POOL | yes | no DFC path | `A-H3`, `B-AVG` |
| FUSED_ATTN | yes | no | `A-H3`, `B-FA` |
| LAYERNORM | yes | no data path | `A-H3`, `B-LN` |
| Slice, concat, COPY instructions | yes | no opcode | `B-DMA` |
| Upsample, reshape on the DMA | yes (`HAS_DMA_RESHAPE`, default 1) | no | `B-DMA` |
| Transpose after the epilogue | `HAS_OUT_TRANSPOSE=1` | no | `B-TMAJ` |
| `HAS_SKIP` (GEMM_FUSED followed by ADD) | yes | two instructions | `P-SPLIT` |

### Reason codes

The same codes are used by `hw_check_program.py`, `HAS_HW_CHECK`, the register trace (`X` lines) and the refusal of
`HAS_HOST=neo` (`NEO_REGISTER_FILE.md`, section 5). Counts are layers of the reference programs (MEASURED).

### 4.1 Codes the compiler removes (verdict `TRANSFORM`)

| Code | Meaning | What the compiler does | YOLOv8m `insts_pe3` | ViT-B/16 |
|---|---|---|---|---|
| `B-PAD` | the input window has padding | allocate the input tensor already padded in DRAM | 59 of 83 GEMM | 0 of 50 |
| `B-INDIM` | input size differs from the size the tile grid implies (a consequence of padding or of edge tiles) | disappears with pre-padded input and a uniform grid | 77 of 83 | 50 of 50 |
| `B-EDGE` | edge tiles (`cout mod cout_t`, `oh mod h_t` or `ow mod w_t` not 0) | choose tiles that divide, or allocate the output up to a multiple of the tile | 64 of 83 | 50 of 50 |
| `B-PADTAIL` | `FLAGS` bit 0 (`PAD_TAIL`, full-size edge tiles) | allocate the output up to a multiple of the tile | 30 of 83 | 50 of 50 |
| `B-FSCALE` | real-valued scale, no per-channel arrays | write per-channel multipliers and shifts (`SCALE_ADDR`, `SHIFT_ADDR`) | 0 | 0 |
| `B-STRIDE` | stride along X differs from Y | use a square stride | 0 | 0 |
| `B-ADDMAP` | the ADD parameters do not map onto one RESIDUAL | one multiplier per operand without separate shifts; `SA`, `SB` at most 4,000,000 | 0 of 12 ADD | 0 of 25 |
| `B-ADDTILE` | ADD length not a multiple of 32 | pad the length to a multiple of 32 | 0 | 0 |
| `B-POOLPAD` | max-pool window with padding (SPPF 5 x 5, pad 2) | pre-pad in DRAM | 3 of 3 max pools | — |
| `B-POOLFIT` | a max-pool field out of range (stride, mode, or a stale `B_ADDR` from an earlier ADD) | write `B_ADDR = 0` before the pool; keep the fields in range | 3 of 3 | — |
| `B-FIELD` | another field cannot be decoded back (for example the dilation of a one-element column) | write it another way | 0 | 0 |

### 4.2 Codes that wait for the hardware or the model (verdict `PATCH` or `NO`)

| Code | Meaning | Removed by | YOLOv8m `insts_pe3` | ViT-B/16 |
|---|---|---|---|---|
| `A-H1` | register words 0 to 11 cannot be written over the bus | hardware | 83 of 83 GEMM | 50 of 50 |
| `A-H4` | the sbank is not loaded (per-channel scale) | hardware | 83 of 83 | 50 of 50 |
| `A-H3` | FUSED_ATTN, LAYERNORM, AVGPOOL have an opcode but no DFC path | hardware | 0 | 169 (144 FUSED_ATTN + 25 LAYERNORM) |
| `B-REQ3` | no word for bias and shift as separate blocks (scale, shift, bias as three sbank blocks) | hardware / model | 83 of 83 | 50 of 50 |
| `B-ACTLUT` | activation table: no word for its address and mode | hardware / model | 77 of 83 (SiLU) | 12 of 50 (GELU) |
| `B-CINORD` | input-channel split: the model's order differs from `loop_order` | hardware / model | 6 | 12 |
| `B-HALO` | halo tile order | kept for completeness: the column order is expressible by swapping X and Y (section 6) | 0 | 0 |
| `B-TMAJ` | transpose after the epilogue (proposal of the model) | hardware | 0 | 0 |
| `B-POOLK` | max-pool window outside 2 to 5 | hardware | 0 | — |
| `B-AVG` | average-pool scale and shift have no word | hardware | 0 | — |
| `B-FA` | FUSED_ATTN is not a vector-unit operation | hardware (or a split into GEMM_FUSED + SOFTMAX + GEMM_FUSED, not provided) | — | 144 |
| `B-LN` | LayerNorm has no data path | hardware (a behavioural RTL module of the model's lane LayerNorm exists, `HW_RTL_ALIGNMENT.md`, section 11) | — | 25 |
| `B-DMA` | slice / concat / COPY / upsample / reshape have no opcode (the DMA is one-dimensional) | hardware; or drop the instruction and use addresses (`insts_pe3`) | 0 (`insts_pe3_nd`: 37) | — |

Notes that do not change the verdict: `P-SPLIT` (`HAS_SKIP` runs as GEMM_FUSED followed by ADD), `P-H2` (the LUT
initialisation reads the wrong word: activation and Softmax tables), `P-POOLDENSE` (max pool with stride 2 or 4 and a window
different from the stride), `P-ADDQ` (an ADD whose parameters map onto RESIDUAL: the zero points are folded into the bias).

Even with every `TRANSFORM` code removed, each GEMM_FUSED layer still waits for `A-H1`, `A-H4` and a decision on
`B-REQ3` and `B-ACTLUT`. The register-file front end accepts `A-H1` and `A-H4` in `HAS_HOST=neo_patched` mode only
(`NEO_REGISTER_FILE.md`, section 5).

## 5. Run profiles and knobs a compiler user sets

| Setting | Values | Effect |
|---|---|---|
| `HAS_PROFILE` (or `--profile` of `tb_has_npu_top`) | not set (default) | the default model: proposals of the model on (4-D DMA with merged pieces, COPY, upsample and reshape, the transpose after the epilogue, attention requant and lane LayerNorm on the vector unit, average pool) |
| | `neo` | only what RTL revision 1 has, with its measured costs (table below). Programs with slice / concat / COPY instructions are refused at those instructions (code `B-DMA`) and the run fails, by design |
| | `neo_as_rtl` | `neo` plus the arithmetic of the RTL (rounding half away from zero, 16-bit wrap, Softmax stage-3 shift as in the RTL): the output is the image the RTL should produce, not the program's golden |
| `HAS_HW_CHECK` | 0 / 1 | per-layer verdict lines (section 3) |
| `HAS_HOST` | `neo` (default) / `neo_patched` / `v45` | front end of `NEO_REGISTER_FILE.md`; `v45` switches the window off |
| `HAS_NEO_REV` | 0 / 1 | RTL revision the `neo_lite` options follow (0 = RTL revision 0, 1 = RTL revision 1); the profiles set 1 |
| `SAURIA_CORE_RTL_PROFILE` | `recommended` (default), `proposals`, `legacy` | data-flow options of the delivery top level (`SW_INTEGRATION_GUIDE.md`, section 10.4); independent of `HAS_PROFILE` |

Precedence: a `HAS_*` variable set by the user wins over the profile table, which wins over the default of the code;
`--profile` wins over `HAS_PROFILE`. The testbench prints one line per knob, `[profile neo] HAS_X=v (profile|env)`.
Without `HAS_PROFILE` and `--profile`, the model is identical byte for byte to a build without profiles (MEASURED).
Through the system wrapper, `HAS_PROFILE` must be in the environment before the model is built; a part of the model
that read a knob before the profile was applied prints `WARNING ... read before the profile was applied`.

Profile `neo` (each knob with its reason; the full table is in `has/has_profile_neo.h`):

| Knob | `neo` | Reason |
|---|---|---|
| `HAS_NEO_REV` | 1 | follow RTL revision 1 (vector-unit register 12 order, forced buffer swap, dense max pool, pool feeder) |
| `HAS_GVU_REGS_NEO`, `HAS_GVU_NEO_PP` | 1, 1 | register map of 14 registers, 14 writes per command, ping-pong buffers switched on the enable edge |
| `HAS_GVU_NEO_CDC_LAT`, `HAS_GVU_NEO_WRITE_LAT` | 4, 1 | multiplexer and clock-domain crossing (MEASURED 3.8 to 4.1 core cycles), one register write per cycle |
| `HAS_GVU_VBANK1_DEPTH` | 200 | vbank1 depth of the RTL |
| `HAS_GVU_RESIDUAL_HW` | 1 | ADD through RESIDUAL |
| `HAS_DMA_DFC_1D` | 1 | one-dimensional DFC commands with their measured cost (6 / 15 / 3 core cycles for the first, chained and last command) |
| `HAS_GVU_OUT_VBANK2` | 1 | GEMM_FUSED result through vbank2 to DRAM |
| `HAS_BUS_STRICT` | 1 | per-channel scale / shift and the Softmax output cross the DMA |
| `HAS_GVU_AGU_ND`, `HAS_DMA_ND`, `HAS_DMA_TENSOR`, `HAS_DMA_ND_MERGE`, `HAS_DMA_RESHAPE` | 0 (the default model has the last four on) | proposals: no address generator registers, no 4-D DMA, no copy opcode, no upsample or reshape |
| `HAS_GVU_FA`, `HAS_GVU_FA_OVERLAP`, `HAS_GVU_LN`, `HAS_GVU_AVG` | 0 | proposals; attention requant and LayerNorm run on the functional model |
| `HAS_OUT_TRANSPOSE`, `HAS_FEEDER_PAD` | 0 | proposals; `HAS_OUT_TRANSPOSE` is 1 in the default model and acts only on a program that writes `TMAJ_C0` |
| `neo_as_rtl` adds `HAS_GVU_SOFTMAX_AS_RTL`, `HAS_ROUND_MODE=2`, `HAS_REQ_NARROW`, `HAS_ZP_ADD_WRAP16`, `HAS_ARITH_RTL` | 1, 2, 1, 1, 1 | arithmetic of the RTL |

MEASURED with profile `neo` on the check programs: 6 of 10 pass; the other 4 contain slice / concat / COPY and are
refused at that instruction (with `HAS_DMA_ND=1 HAS_DMA_TENSOR=1` set by the user all 10 pass). Cycles of three check
programs: reference check 39,562; with max pools 68,459; average pool 3,486. Whole YOLOv8m `insts_pe3` with profile `neo`:
PASS 98 / 98, 65,371,733 cycles (+4.7 % against `insts_pe3` at the epilogue depth 9; `PERFORMANCE_REPORT.md`, section 5.11). ViT-B/16 with profile `neo`
has not been run.

Limits of the profiles: `neo` does not imitate the open RTL defects (section 2), so a layer that uses one of those
paths will differ on the hardware. The cycle counts of `neo` do not yet include the one-dimensional core DMA of
section 7.3. In ViT-B/16 under `neo`, LayerNorm and the attention requant run on the functional model because the
hardware has neither.

## 6. Tiling for the hardware register file

`TILING_GUIDE.md` gives the limits of the core and the cost laws; this section adds what the register file imposes.

- **Uniform grid, one core configuration.** Every tile of a layer has the same size and the same core configuration
  (`Y_USED`, active rows and columns). Edge tiles and `PAD_TAIL` are not expressible (`B-EDGE`, `B-PADTAIL`).
- **No padding.** The input tensor must be stored padded (`B-PAD`, `B-POOLPAD`).
- **Tile order** (`loop_order`, word 38 bits 17:16): 0 = X, then Y, then input channel, then output channel; 1 = input
  channel, output channel, then space; 2 and 3 = output channel, input channel, then space. They correspond to the
  model's tile orders 0 and 1; the model's column order 2 is expressible by swapping the roles of X and Y in the tile
  pointers (MEASURED: the RTL tile loop, ported, gives the same tiles as the model on every layer of `insts_pe3`,
  `insts_pe3_nd` and `vit_full`, in all three orders). The weight loads of orders 0 and 2 are equal (535 and 2,648
  loads); order 1 differs on 12 of 83 YOLOv8m layers (those with exactly two output-channel blocks).
- **Input-channel split** (`TILE_CIN` below `IN_C`): the order of the passes differs from the RTL on every layer that
  splits (6 YOLOv8m, 12 ViT-B/16 layers; `B-CINORD`).
- **Output tiles in DRAM.** With RTL revision 1 an output tile that is not contiguous in DRAM costs extra DMA
  commands: 7,292 of 7,299 output tiles of YOLOv8m and all 7,944 of ViT-B/16 are not contiguous (MEASURED count);
  3,500 of the YOLOv8m tiles and all ViT-B/16 tiles would be contiguous with one DRAM stride register, the other 3,792
  need a 3-D command. A tile-major output layout avoids the cost (section 7.3).
- **Scratchpad sizes**: section 2.
- **Core SRAM.** The RTL parameter set for 32 x 32 int8 declares 2,048 rows for SRAM A and B and 1,024 rows for SRAM C;
  the capacities the model checks (`TILING_GUIDE.md`, section 3) follow the micro-architecture specification and are
  larger. No tool of this package checks a tile against the RTL depths; a compiler that targets this RTL
  configuration must check them itself.

## 7. DMA operations

### 7.1 Slice, concat, copy

| Operation | Model | Hardware RTL revision 1 | Document |
|---|---|---|---|
| Concat along channels by address (producer writes into the destination) | yes, no instruction | yes (addresses only) | `FRONTEND_GUIDE.md`, section 11 |
| Slice / concat instruction (ELEM_WISE modes 6 / 7, 4-D descriptors) | `HAS_DMA_ND=1` (default) | no opcode (`B-DMA`) | `DMA_SLICE_CONCAT.md` |
| The same as one-dimensional DFC commands | `HAS_DMA_DFC_1D=1` | the DMA has the one-dimensional commands; no opcode starts them alone | `HW_RTL_ALIGNMENT.md`, section 5 |
| Tensor copy `DMA_TENSOR` (`0x16`) | `HAS_DMA_TENSOR=1` | no opcode | `DMA_TENSOR_MODE.md` |
| Slice / concat on the vector unit | address generator (`HAS_GVU_AGU_ND`) | no | `SLICE_CONCAT_ON_GVU.md` |
| Upsample, reshape (`DMA_TENSOR` modes 2 / 3) | `HAS_DMA_RESHAPE` (default 1) | no (DMA is one-dimensional) | section 7.2 |

For the hardware today, a compiler maps concat and slice to addresses (program `insts_pe3`), or runs them on the CPU.

### 7.2 Upsample, reshape

The model has both as DMA operations of DRAM-to-DRAM tensors. The hardware RTL revision 1 has neither: its DMA is
one-dimensional, so a layer program that uses them does not run on the hardware (the same as slice / concat, code
`B-DMA`). They are on in the model (`HAS_DMA_RESHAPE`, default 1); with `HAS_DMA_RESHAPE=0` the push is refused with
`UNSUPPORTED_MODE` and a line on stderr. The front end of this package does not emit them: the nearest-neighbour x2
upsample of YOLOv8m stays a host step (`H upsample2x` lines of `mmio.txt`, run by the CPU between instructions;
`SW_INTEGRATION_GUIDE.md`, section 10.2), and the question of how the hardware does it stays open
(`SW_INTEGRATION_GUIDE.md`, section 7, H15).

Entry: `DMA_TENSOR` (push `0x16`) with `MODE_PACK` 2 = upsample, 3 = reshape. Use this entry. The model also accepts the same
operations as ELEM_WISE (push `0x15`) modes 8 and 9; both entries carry `B-DMA` for the hardware RTL revision 1. No new register address; the instruction registers of `SW_INTEGRATION_GUIDE.md` are used:

| Register | Address | Use |
|---|---|---|
| `A_ADDR` | `0x40000444` | source |
| `OUT_ADDR` | `0x40000408` | destination |
| `MODE_PACK` | `0x40000454` | operation |
| `LEN` | `0x40000450` | reshape without `IN_C`: size in bytes |
| `STRIDE` | `0x40000424` | upsample factor, last resort |
| `IN_C`, `IN_H`, `IN_W` | `0x4000046C`, `0x40000470`, `0x40000474` | source shape |
| `OUT_C`, `OUT_H`, `OUT_W` | `0x40000478`, `0x4000047C`, `0x40000480` | destination shape |
| `POOL_K`, `POOL_P` | `0x400004C8`, `0x400004CC` | upsample factors (second choice) |
| `FLAGS` | `0x400004A0` | bits must be 0 (see below) |

**Upsample.** Nearest neighbour, `[C][H][W]` int8 to `[C][H x sh][W x sw]`. The factors are taken in this order: (1) from
`OUT_H` and `OUT_W` when written: `sh = OUT_H / IN_H`, `sw = OUT_W / IN_W`, both exact, and `OUT_C`, when written, equal
to `IN_C`; (2) `sh = POOL_K` and `sw = POOL_P` when `POOL_P` is above 0, else `sw = sh`; (3) `sh = sw = STRIDE`. Use (1).
Source and destination must not overlap, each range stays below 2^32, and the output stays inside the program image
(beyond it, in the staging areas of the data flow controller, the model prints a warning and copies nothing).

**Reshape.** `[IN_C][IN_H][IN_W]` (or `LEN` bytes when `IN_C` is not written) to `[OUT_C][OUT_H][OUT_W]`. `OUT_C` is
required, `OUT_H` and `OUT_W` default to 1, at most one of the three may be -1 (`0xFFFFFFFF`, computed from the element
count). The element count is preserved. With the same source and destination address no byte moves; with different
addresses the data is copied in C order.

**No transpose.** `FLAGS` bit 0 (transpose `[C][HW]` to `[HW][C]`) and bits 1 to 31 are refused (`HAS_DMA_RESHAPE_T=1`, default 0, accepts bit 0 as an experiment). A transpose is done by the
corner turn after the epilogue (section 7.4).

The registers of the DMA window of the hardware (`neo_top` DMA at `cfg_base + 0x30000`) must not be used for these
operations: the DMA register decoder takes only address bits 5:0 (RTL SOURCE), so an extension register placed at offset
`0x40` of that window aliases onto `CTRL` (offset `0x00`), and writing it can start the reader.

### 7.3 One-dimensional core DMA (cost risk)

The DMA of RTL revision 1 loads core tiles with one-dimensional commands per row and channel; a command after the first
costs about 15 to 16 core cycles (MEASURED in an RTL simulation of `neo_top`, converted to core cycles). With the
channel-major tensors of today's programs, loading the input windows of the core needs on average 1,074 commands per
tile for YOLOv8m (at most 2,496) and 768 for ViT-B/16 (MEASURED counts). That is about +132 million cycles for YOLOv8m
and +129 million for ViT-B/16 (ESTIMATE, a lower bound), more than the whole program today: the network would be
limited by the DMA. Ways out: a tile-major layout chosen by the compiler (each tile contiguous in DRAM), or a 2-D / 3-D
load path in the hardware (a question to the hardware team). The model charges this cost only with
`HAS_DFC_CORE_DMA=1` (off by default, and not part of profile `neo`); MEASURED on check programs: +201 cycles on the
YOLOv8m-shaped check, +0 on the reference check.

### 7.4 Transpose

The transpose of an output after the epilogue belongs to this model: `HAS_OUT_TRANSPOSE=1` with `TMAJ_C0`
(`SW_INTEGRATION_GUIDE.md`, section 3.3; `HW_RTL_ALIGNMENT.md`, section 8). It is a proposal: the hardware has no
working transpose (code `B-TMAJ`). Other matrix operations on the DMA are in section 7.2.

## 8. Worked example: the first YOLOv8m layer and a residual add

Program `insts_pe3`, instruction 0: `stem`, 3 x 3 convolution, stride 2, padding 1, 3 input channels, 48 output
channels, input 640 x 640, output 320 x 320, SiLU; tiles of 32 channels x 4 rows x 160 columns (320 tiles).

**Instruction registers** (from `mmio.txt`; addresses are offsets in the DRAM image):

| Register | Value | | Register | Value |
|---|---|---|---|---|
| `IN_ADDR` `0x40000400` | `0x0` | | `IN_C` / `IN_H` / `IN_W` | 3 / 640 / 640 |
| `W_ADDR` `0x40000404` | `0x38E4100` | | `OUT_C` / `OUT_H` / `OUT_W` | 48 / 320 / 320 |
| `OUT_ADDR` `0x40000408` | `0x12C000` | | `TILE_COUT` / `TILE_H` / `TILE_W` | 32 / 4 / 160 |
| `BIAS_ADDR` `0x4000040C` | `0x38E48C0` | | `SCALE_ADDR` / `SHIFT_ADDR` | `0x38E4740` / `0x38E4800` |
| `KH`, `KW`, `STRIDE`, `PAD` | 3, 3, 2, 1 | | `LUT_ADDR` | `0x38E4640` |
| `ACT_TYPE`, `HAS_SKIP` | 2 (SiLU), 0 | | `ZP_OUT`, `FLAGS` | 0, 0 |
| `PUSH_A` `0x40000310` | `0x12` | | | |

**The same layer as 82 words** (`host_trace_gen`, FPGA form: controller window at `0x80000000`, DRAM at
`0x40000000`; through the wrapper the window is at `0x40000800` and the DRAM addresses are the RAM addresses of the
image). Non-zero words of the DFC part, with the formula of `NEO_REGISTER_FILE.md`, section 3.1:

| Word | Value | Check |
|---|---|---|
| 0 | `0x004F0001` | `y_lim` 79 (80 tile rows), `x_lim` 1 (2 tile columns) |
| 1 | `0x00010000` | `k_lim` 1 (2 output-channel tiles: 48 channels in tiles of 32, an edge tile), `c_lim` 0 |
| 2, 3, 4 | `0xA0`, `0x500`, `0x320000` | 160; 4 x 320; 32 x 320 x 320 |
| 5, 6, 7 | `0x140`, `0x1408`, `0x12CF03` | 2 x 160; 2 x 4 x 641; 3 x 641 x 641 (`A_w` = `A_h` = 641) |
| 8, 9 | `0x360`, `0x360` | 32 x 3 x 3 x 3 |
| 10, 11 | 8, 2 | 9 input rows per tile; 3 channels |
| 12 to 17 | `0x140`, `0x19000`, `0x281`, `0x64501`, `0x20`, `0x141` | 320; 320 x 320; 641; 641 x 641; 32; 321 elements per command |
| 18, 19, 20 | `0x40000000`, `0x438E4100`, `0x4012C000` | input, weights, output |
| 34 | `0x438E4740` | sbank base = the per-channel scales |
| 35, 36, 37 | `0x40000000` | not used by this layer |
| 38 | `0x82000000` | `WXfer_op` = 1, bit 25 = 1 |
| 39 | 0 | GEMM_FUSED |
| 69 | 2 | vector-unit opcode GEMM_FUSED |

Words 40 to 68 are the core configuration of `libsauria_cfg.h` (for example word 68 = `0x00005000`). Then `IEN = 1`,
`START = 1`.

Verdict: `NO`, codes `A-H1 A-H4 B-ACTLUT B-EDGE B-INDIM B-PAD B-REQ3`. The trace lists what the 82 words cannot carry:
`X bias_addr ... B-REQ3`, `X shift_addr ... B-REQ3`, `X lut_addr ... B-ACTLUT`, `X in_h 640 B-INDIM`, `X in_w 640
B-INDIM` (the grid implies a 641 x 641 pre-padded input), `X geom.cout 48 B-EDGE`, `X geom.pad_t 1 B-PAD` (and the
other three sides). The compiler removes the `TRANSFORM` codes by storing the input padded to 641 x 641 and by
choosing `TILE_COUT` 16 (a divisor of 48) or padding weights and output to 64 channels; the layer then still carries
`A-H1`, `A-H4` (`PATCH`) and `B-REQ3`, `B-ACTLUT` (`NO`), which wait for the hardware.

**A layer that runs as written**: instruction 5, ELEM_WISE ADD after `dark2.c2f.b0.2`, 1,228,800 elements, both
multipliers 16,384, output shift 14, zero points 0. Instruction registers: `A_ADDR` `0xE10000`, `B_ADDR` `0x960000`,
`OUT_ADDR` `0xA8C000`, `LEN` = `A_LEN` = `B_LEN` = `0x12C000`, mode 0, `SA` = `SB` = `0x4000`, `SHA` = `SHB` = 0,
`SO` = 1, `SHO` = 14, push `0x15`. Non-zero words:

| Word | Value | Meaning |
|---|---|---|
| 18, 19, 20 | `0x40000000` | not used |
| 21 | `0x2F` | 48 tiles |
| 24, 27, 28, 29, 30, 33 | `0x6400` | 25,600 bytes per tile for vbank0, vbank1 and vbank2 commands and DRAM steps |
| 34 | `0x40000000` | not used |
| 35, 36, 37 | `0x40E10000`, `0x40960000`, `0x40A8C000` | A, B, output |
| 39 | 4 | RESIDUAL |
| 69 | 6 | vector-unit opcode RESIDUAL |
| 75 | `0x0E000320` | 800 vectors per tile, `out_z` 0, `out_shift` 14 |
| 76, 77 | `0x4000`, `0x4000` | multipliers |
| 78 | `0x2000` | bias = 2^13, the rounding half of the shift by 14 (zero points 0) |

Verdict `OK`. Through the wrapper with `HAS_HOST=neo` this layer gives the same DRAM image and cycles as through the
instruction registers (MEASURED on the check programs that contain such layers).

## 9. Checking the output of a compiled program

| Check | Command | Reference |
|---|---|---|
| Every instruction against the golden | `tools/has/tb_has_npu_top <program dir>` | `dram_golden.bin`; `RESULT: PASS`; a window of instructions: `--first <n> --count <m>` (starts from the golden image) |
| Independent of the model | `python3 tools/has/ref_program.py <program dir>` | plain integer arithmetic, no shared code; names the first wrong instruction and element. FUSED_ATTN and LAYERNORM are not computed |
| Through the system wrapper | firmware replay engine (`tools/has/wrapper/fw/`) | CRC-32 of every output, taken from `dram_golden.bin` (`SW_INTEGRATION_GUIDE.md`, section 10.5) |
| Through the register-file front end | `tools/has/tb_has_neo_host <program dir> <writes file> --dram-out <f>` | DRAM image of the same program through the instruction registers (`cmp`) |
| Register trace decodes back to the program | `host_trace_replay --compare <program dir> <trace>` | the program's instructions |
| What the RTL / FPGA should produce | `FE_SNAPSHOT_DIR=<dir> HAS_PROFILE=neo_as_rtl tools/has/tb_has_npu_top <program dir>` | writes `<dir>/dram_snapshot.bin`; compare with the RTL or FPGA output, not with `dram_golden.bin` |

`neo_as_rtl` reports differences against the program's golden by design: MEASURED 162 (average pool check), 527
(reference check) and 596 (max-pool check) elements, all from rounding half away from zero and wrapping.

Detection boxes and classes of the two networks: `BUILD_AND_RUN.md`, section 7.

## 10. What is not covered yet

- A GEMM_FUSED layer end to end through the register file (needs the hardware changes of section 4.2 and a requant data
  layout; the `neo_patched` layout is a proposal).
- A run of the reference programs on an RTL simulation of the whole `neo_top` with GEMM_FUSED: the DFC stops while it
  configures the vector unit, because it does not accept the write responses of those writes (MEASURED in an RTL
  simulation). RESIDUAL and max pool through the full `neo_top` match the model with 0 differences.
- The FPGA script of the hardware team.
- Proposed, not in the RTL yet: the DFC register file could grow from 82 to 112 words, with words 96 to 111 for DMA
  operations (108 and 109 for the transpose). The 82 words described here keep their positions.
- Upsample and reshape emitted by the frontend: the reference frontend still writes the upsample as a host step (section 7.2).
