# Interface and File Format Specification

All multi-byte values are little endian. Tensors are int8, laid out `[C, H, W]` (C-order, N = 1).

## 1. Network program files

An exported network directory (for example `$FE_WORK/step6b/net/`) contains:

| File | Content |
|---|---|
| `prog.bin` | Program: staging addresses, tensor table, steps and tiles |
| `dram_init.bin` | Initial DRAM image: network input and all layer parameters; tensor regions zero |
| `dram_golden.bin` | Same layout with every tensor region filled with the expected integer result |
| `manifest.json` | Addresses and shapes, configuration name, image name |

Both DRAM images have the same size (about 110 MB for YOLOv8m).

### 1.1 DRAM layout

Regions are allocated in this order, each aligned to 16 bytes:

| Region | Size | Use |
|---|---|---|
| `STAGE_A` | 79 KB | Padded input window of the current tile, `[Cin, A_H, A_W]` |
| `STAGE_SKIP` | 79 KB | Skip (residual) values of the current tile, `[k, h, w]` |
| `STAGE_PRE` | 96 KB | Partial-sum preload (bias) of the current tile, int32 `[k, h, w]`, 32 elements per 128-byte word |
| `STAGE_OUT` | 96 KB | Epilogue output of the current tile, one int32 word per element (low byte is the int8 result) |
| Tensors | per tensor | Network input, every step output and every step input |
| Conv parameters | per conv | `w` int8 `[Cout, Cin, kh, kw]`, `lut` int8[256], `scale` u32[Cout], `shift` u32[Cout], `bias` i32[Cout] |

### 1.2 `prog.bin`, format `FENP`

```
char[4]  magic = "FENP"
u32      STAGE_A, STAGE_SKIP, STAGE_PRE, STAGE_OUT     (DRAM byte addresses)
u32      n_tensors
         n_tensors x { u32 addr, u32 c, u32 h, u32 w }
u32      n_steps
         n_steps x step
```

Each step starts with a `u8 kind`:

| kind | Step | Payload after `kind` |
|---|---|---|
| 0 | conv | `u32 in, out, skip` (tensor ids; `skip = 0xFFFFFFFF` if none) · `u8 silu` · `u32 cin, cout, kh, kw, stride_h, stride_w` · `u32 w_addr, lut_addr, scale_addr, shift_addr, bias_addr` · `u32 n_tiles` · `n_tiles x tile` |
| 1 | slice_ch | `u32 in, out, start, end` |
| 2 | concat | `u32 out, n_inputs` · `n_inputs x u32 in` |
| 3 | maxpool | `u32 in, out, kernel, stride, pad` (square kernel, symmetric pad, pad value -128) |
| 4 | upsample_nearest | `u32 in, out, factor` |

A tile is eleven `i32`: `c0, c1, oy0, oy1, ox0, ox1, iy0, iy1, ix0, ix1, y_used`.
- Ranges are half-open.
- `[c0, c1) x [oy0, oy1) x [ox0, ox1)` is the output block.
- `[iy0, iy1) x [ix0, ix1)` is the input window; it may extend outside the tensor, and those positions read as 0.
- `x_used = c1 - c0`.
- `y_used` is the number of active array rows.

### 1.3 `prog.bin`, format `FEHP` (HAS variant)

The format is the same as `FENP` with three differences:
- The magic is `"FEHP"`, followed by four `u32` option values: `round_mode, req_narrow, deq_zp_order, scale_fmt`.
- A conv never has a skip. Residuals are separate steps.
- kind 3 is not used. Two kinds are added:

| kind | Step | Payload |
|---|---|---|
| 5 | elem_add | `u32 a, b, out` · `i32 zpA, zpB, zpO` · `u32 SA, sA, SB, sB, SO, sO` (S values are raw 32-bit words) |
| 6 | elem_max | `u32 in, out, k, s, p` (pad value -128) |

### 1.4 `manifest.json`

| Key | Content |
|---|---|
| `config` | Quantization configuration name |
| `image` | Input image file name |
| `dram_bytes` | Size of the DRAM images |
| `stage` | Addresses of the four staging regions |
| `tensors` | Map of tensor name to `{id, addr, shape}` |
| `cut_outputs` | Names of the six head tensors |
| `steps` | Number of steps |

## 2. Testbench outputs

### 2.1 Log lines

```
[STEP] <n>/<total> kind=<k> tensor=<id> [C,H,W] elements=<N> bad=<mismatches> PASS|FAIL tiles=<cumulative> via_core=<cumulative> sim_cycles=<cumulative>
[tb_fe_core_net] RESULT: PASS|FAIL (steps, tensors, elements, bad tensors, bad elements, framing errors, core deadlocks)
[tb_fe_core_net] tiles <n> (via core <n>), OBP vectors <n>, MACs (stand-in tiles) <n>, sim cycles <n> (DMA wait <n>, OBP <n>, OBP config approx <n>)
```

### 2.2 Snapshot directory (`FE_SNAPSHOT_DIR`)

| File | Content |
|---|---|
| `dram_snapshot.bin` | Full DRAM image after the last completed step. Written atomically (temporary file, then rename) |
| `last_step.txt` | 0-based index of the last completed step |

### 2.3 Metrics CSV (`FE_METRICS_CSV`, metrics build)

The first line is a comment with run metadata:

```
# meta,window=run_tile_via_npu_top_call,fifo=<act5_wei4|fifo16>,backdoor=<on|off>,launch=<first|resume>
```

After that come a header line and one row per tile executed on the core. The file is opened in append mode.

| Column | Meaning |
|---|---|
| `step`, `tile` | 0-based step and tile index in `prog.bin` |
| `cin, kh, kw, sy, nch, ht, wt, yu, n_ctx` | Tile geometry: input channels, kernel, stride, output channels, output height and width, active rows, contexts |
| `ok` | 1 if the core finished the tile |
| `ticks_all` | Controller cycles in the measurement window (reset, configuration, compute, drain) |
| `busy` | Cycles outside the idle state |
| `exec` | Cycles in which the array advanced |
| `pe_cycles` | `exec x 1024` |
| `mac_nz` | Multiply-accumulates with two non-zero operands |
| `macs_theory` | `nch x ht x wt x cin x kh x kw` |
| `a_rd_beats, a_rd_bytes, b_rd_beats, b_rd_bytes` | Core SRAM A and B reads |
| `c_rd_beats, c_rd_bytes, c_wr_beats, c_wr_bytes` | Core SRAM C reads and writes |
| `st01` ... `st24` | Cycles per controller state (index = state in section 3.3) |

## 3. RTL-ref core host interface (`rtl_ref_npu_top.h`)

### 3.1 Ports

| Port | Direction | Type | Use |
|---|---|---|---|
| `i_clk`, `i_rstn`, `i_soft_reset` | in | bool | Clock, active-low reset, soft reset |
| `i_start` | in | bool | Start pulse |
| `o_done` | out | bool | Tile finished |
| `o_deadlock` | out | bool | Combinational stall monitor (not a latched error) |
| `i_mvm_k` | in | u32 | Set to 1 |
| `i_total_contexts` | in | u32 | Number of contexts of the tile |
| `i_host_addr`, `i_host_wren`, `i_host_rden`, `i_host_wdata`, `i_host_wmask`, `o_host_rdata` | in/out | | Host bus. Data carries 4 elements per beat with a per-element mask |
| `i_select` | in | 3 bits | Ping-pong select for SRAM A (bit 0), B (bit 1), C (bit 2). 0 = host sees buffer 0 and the accelerator buffer 1; 1 = swapped |
| `i_threshold` | in | float | Array threshold input, forwarded to the processing elements (the testbenches drive a constant) |

Testbench-only helpers (build flag `FX1_A3_SRAM_BACKDOOR_LOAD`):
- `load_sram_backdoor(bank, offset, data, size)` writes both buffers of a bank without cycles;
- `read_sram_backdoor(...)` reads the buffer currently on the accelerator side.

Bank ids: 0 and 1 are B, 2 and 3 are A, 4 and 5 are C.

### 3.2 Host address map

| Region | Base | Content |
|---|---|---|
| Configuration registers | `0x000000` | Section 3.3 |
| SRAM A | `0x040000` | Ifmap. Address = row x 8 + sub-word; 4 elements per beat |
| SRAM B | `0x080000` | Weights. Same addressing |
| SRAM C | `0x0C0000` | Partial sums (int32). Same addressing |

The region is selected by `address & 0x3C0000`.

### 3.3 Configuration registers (profile `PROFILE_V1_SAURIA`)

`CFG_PROFILE` at `0x004` selects the profile and must be written first. Offsets of the register groups:
CON `0x200`, ACT `0x400`, WEI `0x600`, OUT `0x800`, LAYER `0xA00`.

| Address | Field | Address | Field |
|---|---|---|---|
| `0x200` | `INCNTLIM` | `0x600+0x04` | `WEI_INCNTLIM` |
| `0x204` | `ACT_REPS` | `0x600+0x08` | `WEI_INCNTSTEP` |
| `0x208` | `WEI_REPS` | `0x610` / `0x614` | `WEI_WLIM` / `WEI_WSTEP` |
| `0x400` | `ROWS_ACTIVE` (bit mask) | `0x618` / `0x61C` | `WEI_KLIM` / `WEI_KSTEP` |
| `0x404` / `0x408` | `ACT_INCNTLIM` / `ACT_INCNTSTEP` | `0x620` / `0x624` | `WEI_TIL_KLIM` / `WEI_TIL_KSTEP` |
| `0x40C` / `0x410` | `ACT_OUTCNTLIM` / `ACT_OUTCNTSTEP` | `0x628` | `WEI_COLS_ACTIVE` (64-bit, two beats) |
| `0x414` / `0x418` | `ACT_XLIM` / `ACT_XSTEP` | `0x62C` | `WEI_WALIGNED` |
| `0x41C` / `0x420` | `ACT_YLIM` / `ACT_YSTEP` | `0x800` | `NCONTEXTS` |
| `0x424` / `0x42C` | `ACT_CHLIM` / `ACT_CHSTEP` | `0x804` ... `0x810` | `CXLIM`, `CXSTEP`, `CKLIM`, `CKSTEP` |
| `0x428` / `0x440` | `DIL_PAT` low / high 32 bits | `0x814` ... `0x820` | `TIL_CYLIM`, `TIL_CYSTEP`, `TIL_CKLIM`, `TIL_CKSTEP` |
| `0x444` | `ACT_STRIDE` | `0x824` | `INACTIVE_COLS` |
| `0x430` ... `0x43C` | `ACT_TIL_XLIM`, `_XSTEP`, `_YLIM`, `_YSTEP` | `0x828` | `PRELOAD_EN` |
| `0x480` / `0x680` / `0x880` | ACT / WEI / OUT base address | `0xA00` ... `0xA44` | Layer description (informational) |

Register values are computed from a layer description by `sauria_compute_core_fields()` in
`driver/libsauria_cfg.h`. `npu_apply_cfg()` in `tools/fe/sysc/tb_fe_core_net.cpp` is the reference sequence.

### 3.4 Controller states (metrics columns `st01`..`st24`)

| # | State | # | State | # | State |
|---|---|---|---|---|---|
| 1 | START_FLAGS | 9 | WAIT_CSWITCH_STALL | 17 | OBUF_BUSY_SHIFT |
| 2 | ARRAY_PREP | 10 | WAIT_OBUF | 18 | FORCE_STALL |
| 3 | ARRAY_FILL | 11 | WAIT_OBUF_STALL | 19 | OBUF_BUSY |
| 4 | FIRST_SHIFT | 12 | SCND_SHIFT | 20 | ARRAY_CSWITCH |
| 5 | START_COMP | 13 | SCND_SHIFT_STALL | 21 | ARRAY_CSWITCH_STALL |
| 6 | DRAIN_FEED | 14 | ALL_BUSY_SHIFT | 22 | LAST_SHIFT |
| 7 | ARRAY_FLUSH | 15 | ALL_BUSY | 23 | LAST_WAIT |
| 8 | WAIT_CSWITCH | 16 | ARRAY_BUSY | 24 | DONE |

State 0 is IDLE and is not counted in `busy`.

## 4. Epilogue host interface

### 4.1 Native OBP (`psm/obp_top.h`)

The LUT and bias bases are template parameters; the scale and shift regions follow the LUT base at `+0x40000` and
`+0x50000`. The native top level maps the two lanes as follows (host addresses):

| Region | Lane A | Lane B | Addressing | Value |
|---|---|---|---|---|
| LUT | `0x140000` | `0x160000` | `BASE + lane x 256 + entry`, 4 entries per beat | u8 |
| Bias | `0x150000` | `0x170000` | `BASE + 4 x channel`, one word per write | int32, two's complement: the full 32-bit range is accepted, including negative values |
| Scale | `0x180000` | `0x1A0000` | `BASE + 4 x channel`, one word per write | u32 |
| Shift | `0x190000` | `0x1B0000` | `BASE + 4 x channel`, one word per write | u32 |

All four regions of both lanes are readable through the host bus of the native top level (same address, word 0 of
the read data). The RTL-ref network testbench uses the Lane A addresses.

Control ports: `i_bias_en`, `i_requant_en`, `i_lut_en`, `i_residual_en`, `i_vec_channel_mode`, default scale and shift.
In vector-channel mode, the channel of each input vector comes from an internal counter that resets when the
pipeline is empty.

### 4.2 HAS OBP (`has/gvu_obp.h`)

Same ports as the v4.5 OBP. Bias and residual are not supported (bias is the partial-sum preload; residuals
are ELEM_WISE steps).

| Register | Address | Content |
|---|---|---|
| LUT | `0x140000` | int8[256], one table for all lanes |
| Scale | `0x180000 + 4 x ch` | S per channel (signed or unsigned 32-bit, per `SCALE_FMT`) |
| Shift | `0x190000 + 4 x ch` | s per channel |
| `ZP_OUT` | `0x1A0000` | Output zero point |
| `NCH` | `0x1A0004` | Channels of the current tile; channel of a vector = `address mod NCH` |

## 5. Native model host interface (legacy)

This is the protocol of the previous hand-over, implemented by the legacy functional model. The delivery top
level keeps the gather-and-push part of it and adds extension and status registers. The register map for
software is in `SW_INTEGRATION_GUIDE.md`. Main registers of the legacy model (full list in `config_map.h`,
behaviour in `config_regs.h`):

| Register | Address | Access | Use |
|---|---|---|---|
| `INST_LO` | `0x40000300` | WO | Instruction bits 31:0 |
| `INST_HI` | `0x40000304` | WO | Instruction bits 63:32; triggers decode |
| `QUEUE_A_PUSH`, `QUEUE_B_PUSH` | `0x40000310`, `0x40000314` | WO | Push an instruction to a lane queue |
| `NSPLIT` | `0x40000214` | RW | Lane split (rows of Lane A) |
| `OUT_OBP_CFG_A`, `_B` | `0x40000130`, `0x40000134` | RW | Bit 0 bias, bit 1 requant, bits 6:4 activation, bit 7 residual |
| `RICH_HEADS_DIM_MODE` | `0x40000454` | RW | `mode[31:24] dim[23:16] num_heads[15:0]` |
| OBP LUT A, B | `0x40140000`, `0x40160000` | RW | Activation LUTs |
| RCE LUTs A, B | `0x40200000` ... `0x40250000` | RW | exp, recip, rsqrt tables |

Instruction encoding and opcodes are in `ARCHITECTURE.md`, section 2.2. Scale registers that hold real numbers
take the raw IEEE-754 bit pattern as an integer.

## 6. Delivery top level interface (`has/has_npu_top.h`)

The register-level interface for software (addresses, status, error codes, programming sequences, parameter
blocks) is specified in `SW_INTEGRATION_GUIDE.md`, sections 3, 4 and 6.2. This section summarizes it and
specifies the files that drive the testbench.

### 6.1 Ports and memory

`HasNpuTop` has the host ports of the core (`i_clk`, `i_rstn`, `i_host_addr`, `i_host_wren`, `i_host_rden`,
`i_host_wdata`, `i_host_wmask`, `o_host_rdata`) plus `o_irq`, `o_done` and `o_deadlock`. Memory is one DRAM
image passed with `set_dram()`; the data flow controller appends a private staging area after it
(`HARDWARE_CONFIG.md`, section 6.1).

### 6.2 Opcodes

Written to `PUSH_A` (`0x40000310`), bits 7:0.

| Opcode | Instruction | Parameters |
|---|---|---|
| `0x05` | `SET_NSPLIT` | Accepted and ignored (counted) |
| `0x12` | GEMM_FUSED | One convolution or matrix product layer; the hardware iterates the tiles |
| `0x13` | FUSED_ATTN | One attention head; parameter block `ATN1` |
| `0x14` | LAYERNORM | A block of rows; parameter block `LNP1` |
| `0x15` | ELEM_WISE | ADD (mode 0), MAX_POOL (mode 1) or AVG_POOL (mode 5; Avg_Scale / Avg_Shift in `SO` / `SHO`) |

Any other opcode sets error `UNSUPPORTED_OP`.

### 6.3 Extension registers and `FLAGS`

Extension register `i` is at `0x4000046C + 4 x i`, `i` = 0 to 30 (last address `0x400004E4`). All are cleared
after every push. Indices 0 to 25 carry the layer geometry, tiling, quantization and pooling parameters;
indices 26 to 30 were added for the planner options and the transformer instructions:

| Index | Address | Name | Instructions |
|---|---|---|---|
| 26 | `0x400004D4` | `TILE_CIN` | GEMM_FUSED: input channels per core pass |
| 27 | `0x400004D8` | `Y_USED` | GEMM_FUSED: output positions per context, 1 to 32 |
| 28 | `0x400004DC` | `ROWS` | FUSED_ATTN: query rows; LAYERNORM: rows |
| 29 | `0x400004E0` | `PARAM_ADDR` | FUSED_ATTN, LAYERNORM: address of the parameter block (required) |
| 30 | `0x400004E4` | `MASK_ADDR` | FUSED_ATTN: int8 mask `[rows][L]`, -128 = masked (optional) |

`FLAGS` (index 13, `0x400004A0`):

| Bit | Name | Instructions | Meaning |
|---|---|---|---|
| 0 | `PAD_TAIL` | GEMM_FUSED | Edge tiles computed at full `TILE_COUT` and a width rounded up to a multiple of the positions per context; only real channels and positions are written back. The last output-channel band of the weights must be zero-padded |
| 1 | `CHANNEL_MAJOR` | FUSED_ATTN, LAYERNORM | Element (token i, channel j) at `address + j x rows + i`, the layout a GEMM_FUSED over tokens writes |
| 31:2 | reserved | | Write 0 |

Full definitions: `SW_INTEGRATION_GUIDE.md`, sections 3.3 and 6.1.

### 6.4 Parameter blocks `ATN1` and `LNP1`

Little-endian 4-byte words at `PARAM_ADDR`, magic `0x314E5441` (`ATN1`, 1,080 bytes) and `0x31504E4C`
(`LNP1`, size depends on H). Word layout: `SW_INTEGRATION_GUIDE.md`, section 6.2. Parser:
`has/gvu_rce_params.h`; reference writer: `tools/has/make_rce_gate.py`.

### 6.5 Instruction directory (testbench input)

Written by `tools/fe/fe_emit_insts.py` (YOLOv8m) and `tools/fe/fe_vit_full.py` (ViT-B/16); read by
`tools/has/tb_has_npu_top`.

| File | Content |
|---|---|
| `mmio.txt` | Register-write stream, in the order a CPU writes it |
| `dram_init.bin` | Initial DRAM image: network input, SAURIA-ordered weights and all parameters |
| `dram_golden.bin` | Same layout with every tensor region filled with the expected result |
| `manifest.json` | YOLOv8m: configuration, tensor addresses and shapes, parameter addresses, plan name, tile count, per-layer tiling of the plan |
| `summary.json` | ViT-B/16: per-block golden and quality summary |

`mmio.txt` has one item per line:

```
W <address hex> <value hex>        register write (a write to 0x40000310 pushes an instruction)
H <host operation> <arguments>     host step between instructions, for example
                                   H upsample2x in=0x... out=0x... c=<c> h=<h> w=<w>
# [<n>] <text>                     label of instruction n (printed in the [STEP] line)
# <text>                           comment
```

Tensors are `[C][H][W]` int8 (YOLOv8m) or channel-major `[C][tokens]` int8 (ViT-B/16). Slice and concat are
address aliases: a concat output is written directly into sub-regions of the concatenated tensor. Weights of
each output-channel band are stored in SAURIA order at `W_ADDR + c0 x K` (`SW_INTEGRATION_GUIDE.md`,
section 5 and 6.1).

The testbench compares the output tensor of every retired instruction with `dram_golden.bin` and prints the
log lines of section 2.1 (`[STEP]` per instruction, then `RESULT`). `FE_METRICS_CSV` and `FE_SNAPSHOT_DIR` work
as in sections 2.2 and 2.3.
