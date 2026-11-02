# Reference flow for the RTL: the data flow controller drives the vector unit through its register file

This document gives the hardware team one block list and one set of rules for the flow the model recommends: the data flow
controller (DFC) configures and starts the vector unit (GVU, `gvu_core` / `gvu_top`) only by writing the GVU register file over
AXI-Lite, and the SAURIA core streams partial sums to the GVU through the PSM stream. The model runs this flow on the whole
YOLOv8m network (98 of 98 instructions pass, 0 elements differ). Detailed walk-throughs of each flow and the problems met on the
way are in [GVU_TOP_CORE_INTEGRATION.md](GVU_TOP_CORE_INTEGRATION.md); slice and concat options are in
[SLICE_CONCAT_ON_GVU.md](SLICE_CONCAT_ON_GVU.md). Average pool, Softmax and the attention requant also run through the register file,
as jobs on the clock of the unit (knobs `HAS_GVU_AVG`, `HAS_GVU_SOFTMAX`, `HAS_GVU_FA`, on by default); their rows below are marked
with the knob, and the details are in [GVU_OPS_ON_GVUTOP.md](GVU_OPS_ON_GVUTOP.md). LayerNorm (knob `HAS_GVU_LN`, on by default) runs
on the unit too but does not use the register file (no registers for it in the RTL).

## 1. Blocks

| Block (RTL name) | Role in the flow | Status | Model |
|---|---|---|---|
| `df_controller_top` | Fetches instructions, tiles layers, programs DMA and the core configuration registers; writes the GVU registers through its AXI-Lite master `gvu_mst`; waits on the `gvu_pipe_done` wire | in the RTL; GVU path not wired yet | `has/has_npu_top.h` (`HasNpuTop::dfc`) |
| `axi_lite_2ram` in `gvu_core` | AXI-Lite slave of the register file | needs patches: word index `reg_addr[AW+1:2]` (B17), 4-byte strobe writes (B13) | `has/gvu_reg_master.h` (one write blocks the DFC `L_w` cycles, default 4, ESTIMATED) |
| `gvu_config_regs` | 32-bit registers decoded into `gvu_top` inputs; Reg 0 bit 0 clears itself at `pipe_done` | Reg 0..8 in the RTL; Reg 9..26 proposal; `GVU_REG_AW` 4 → 5 proposal; the vbank1 buffer bit must flip on the rising edge of `lut_init_done` (B21) | `has/rtl_ref_gvu_config_regs.h` (`num_regs` 16 or 32) |
| `sauria_logic` PSM | Partial sums of the last Cin pass leave the core as packets on the PSM stream (FIFO depth 4) | needs F1′ (end-of-layer flag) and F2b (OC index on the packet) | SRAM-C write hook in `has/has_npu_top.h` + `PsmStreamTap` in `has/gvu_top_stream.h` |
| `gvu_top` / `gvu_pipe_controller` | GEMM_FUSED epilogue on the stream, max pool, Softmax; residual add and average pool (proposals) | needs B1, B9, B18; Softmax needs B20 (stage-3 shift); RESIDUAL states and the average pool data path are proposals | `has/rtl_ref_gvu_top.h` (`GvuTop`), clocked by `GvuTopStream` |
| VLSU and AGUs | One AGU per Scratchpad port | B18 (hold the address on idle cycles); AGU_ND mode 3 is a proposal | `has/rtl_ref_gvu_vlsu.h` + `has/gvu_agu_nd_vlsu.h` |
| Scratchpad | sbank (scale / shift), vbank0, vbank1 (LUT, operand B), vbank2; ping-pong buffers | B4 (DMA row decode), B12 (pooling feeder between runs) | `has/rtl_ref_gvu_scratchpad.h`, `has/rtl_ref_gvu_pool_feeder.h` |
| DMA ports in `gvu_core` | Tables, scale / shift and operands in; results out | B14 (port enables); F8 open (vbank2 to DRAM) | model DMA in `has/has_npu_top.h` |

## 2. Register map

Index = byte address / 4. Reg 0..8 are the RTL's registers; Reg 9..26 are the proposal (Reg 25 / 26 and opcode 8 come with the
average pool patch); Reg 27..31 are reserved (LayerNorm needs about 20 fields and does not fit; see the open questions at the end).

| Reg | Name | Fields (bit ranges) | Written |
|---|---|---|---|
| 0 | CONTROL | [0] pipeline enable (self-clears at `pipe_done`), [1] done flag, [2] clear done (write 1), [6:3] opcode (2 GEMM_FUSED, 3 SOFTMAX, 6 RESIDUAL, 7 MAXPOOL, 8 AVGPOOL) | last write of every operation |
| 1 | LUT | [0] / [1] / [2] / [3] reset of the act / exp / recip / rsqrt table flag (level) | 1 then 0 when a layer loads a new table |
| 2 | SCRATCHPAD | [0] sbank, [1] vbank0, [2] vbank1 (toggles at `lut_init_done`: once, on the rising edge, B21), [3] vbank2 buffer select | when a buffer changes |
| 3 | GEMM_FUSED | [0] skip activation, [15:8] output zero point (int8) | per layer; per phase of the attention requant (skip activation 1) |
| 4 | SOFTMAX_0 | [8:0] token length (RESIDUAL: rows of the run), [17:9] number of tiles (RESIDUAL and Softmax: 1) | per run |
| 5, 6 | SOFTMAX_1, SOFTMAX_2 | attention scale (int32); attention shift [7:0] and zero point [15:8] | per Softmax group |
| 7 | POOLING_0 | [1:0] window (k − 2), [4:2] stride, [15:8] W tiles, [23:16] C, [31:24] OH | per max pool / average pool run |
| 8 | POOLING_1 | [9:0] H move, [19:10] C move | per max pool / average pool run |
| 9, 10, 11 | RES_SCALE_A / _B / _OUT | int32 scales | once per ADD |
| 12 | RES_SHIFT | [7:0] / [15:8] / [23:16] shifts, [26:24] rounding mode, [27] int16 output | once per ADD |
| 13 | RES_ZP | [7:0] / [15:8] / [23:16] zero points (int8) | once per ADD |
| 14 | RES_ROW | [9:0] first row of A (vbank0), [19:10] of B (vbank1), [29:20] of the int32 sum (vbank2) | first run, then on change |
| 15 | RES_LASTMASK | lane mask of the last row of the last run | first and last run |
| 16 | AGU_ND_CTRL | [2:0] source port, [3] source on, [6:4] destination port, [7] destination on, [8] idle reload, [9] stop | per ADD / max pool when bound |
| 17..20 | AGU_SRC_0..3 | base [11:0], repetitions n0..n3 − 1 (10 bits each), strides s0..s3 (16 bits, two's complement) | per ADD when bound |
| 21..24 | AGU_DST_0..3 | same layout, destination | per ADD / max pool when bound |
| 25 | AVG_SCALE | Avg_Scale (int32) | per average pool run |
| 26 | AVG_CFG | [5:0] Avg_Shift, [10:8] rounding mode | per average pool run |

## 3. Sequences the DFC follows

Every configuration write completes before the write that sets Reg 0 bit 0. The DFC learns the end of an operation from the
`o_pipe_done` wire, then writes Reg 0 bit 2 to clear the done flag (optional: any later full write of Reg 0 also clears it).

| Operation | Writes | Ends |
|---|---|---|
| GEMM_FUSED, once per layer (all core calls of the layer) | Reg 3; Reg 1 = 1, then 0 if the layer loads another activation table; Reg 0 = `(2 << 3) \| 1` | the packet that holds the last element of the last context of the last call carries `oc_last & oc_tile_last`; the DFC sets the core's `oc_tile_last` configuration bit only on that last call |
| MAX_POOL, once per run (≤ the channels that fit) | [Reg 16, Reg 21..24 if AGU_ND binds the destination]; Reg 7; Reg 8; Reg 0 = `(7 << 3) \| 1` | the unit writes its last result row |
| RESIDUAL (ADD), runs of ≤ 128 rows (B sits in vbank1) | once: Reg 9..13; per run: [Reg 16..24 if bound], Reg 4, Reg 14, Reg 15 when they change; Reg 0 = `(6 << 3) \| 1` | `o_residual_pipe_done` (proposed into the OR of `o_pipe_done`) |
| AVG_POOL (knob `HAS_GVU_AVG`), once per run, unit reset before the run | Reg 7; Reg 8; Reg 25; Reg 26; Reg 0 = `(8 << 3) \| 1` | the average pool done output, 5 edges after the max pool timing (patch) |
| SOFTMAX (knob `HAS_GVU_SOFTMAX`), once per group of ≤ 32 rows; tables in vbank1 buffer 1 by DMA | Reg 5; Reg 6; Reg 4 (token length, tiles 1); Reg 0 = `(3 << 3) \| 1` | `o_softmax_pipe_done`; the vbank1 buffer bit flips once on the rising edge of `lut_init_done` (B21) |
| Attention requant (knob `HAS_GVU_FA`), phase 1 and phase 3 of FUSED_ATTN | Reg 3 (zero point, skip activation 1); Reg 0 = `(2 << 3) \| 1`; then the int32 sums as GEMM_FUSED stream beats | the last beat of the phase leaves the epilogue |

Cost on the model, `L_w` = 4 (ESTIMATED latency): about 15,461 register writes for the whole YOLOv8m network (sum of the counts
MEASURED in the window runs); the DFC waits on the write port for about 0.10 % of the run (derived from the counts), and the
MEASURED run gets 1,819 cycles longer (0.003 %), because most writes overlap DMA.

## 4. Rules that came out of the work

- **End of a GEMM_FUSED layer**: put the flag on the packet that holds the last element of the tile. Do not count `oc_last`
  packets: with a dense layout the last channel row of one context can sit on two SRAM-C words, so one context emits two `oc_last`
  packets and a count lands in the middle of the tile (the core hangs). Measured in the model on YOLOv8m and on the RTL core.
- **Keep `pipeline_en` high for the whole layer**: re-arming it between contexts restarts the AGUs and overwrites vbank2.
- **AGUs hold their address on idle cycles** inside a stream (B18), otherwise a bubble in the psum stream writes row 0.
- **Reset the pooling feeder between max pool runs** (B12) without resetting the whole unit (that would clear the register file).
- **Register read-back does not prove the address map** (B17): writes and reads hit the same wrong index; check the decoded fields.

## 5. Patches and proposals for the RTL

The RTL feedback package holds 13 fixes and the two proposals as `git apply` diffs on the RTL drop, in this order: B13, B4, B14,
B17, B12, B16, B1, B9, B18, F1′ (version 3), F2b (version 2), B19, then the residual-add proposal, then the AGU_ND proposal. The
model implements every row of this document behind the default knobs `HAS_GVU_TOP=3 HAS_GVU_STALL=1 HAS_GVU_ELEM_CLK=1
HAS_GVU_REGS=1 HAS_GVU_AGU_ND=1` (see [CODE_MAP.md](CODE_MAP.md)); the rows marked with a knob are also on by default (`HAS_GVU_AVG`,
`HAS_GVU_SOFTMAX`, `HAS_GVU_FA`).
Three more patches are separate diffs, not part of that package: the average pool data path (Reg 25 / 26, opcode 8, write delay
D = 5), B20 (Softmax stage-3 shift) and B21 (vbank1 buffer bit on the rising edge); each comes with an RTL simulation (see
[GVU_OPS_ON_GVUTOP.md](GVU_OPS_ON_GVUTOP.md)). Stage 3 of Softmax has no left shift, so an exponent E = 0 cannot be expressed
either by the delivered RTL or with the B20 patch (B20b, MEASURED in an RTL simulation on a constructed case; no patch yet); the
model shifts left with the exponent of the current table lookup.

## 6. Open questions for the hardware team

1. Residual add: accept Reg 9..15 and the RESIDUAL states, or another split? LayerNorm needs about 20 fields: a register block of
   its own, 64 registers (`GVU_REG_AW` 6, proposal P-LN6), or a descriptor in the instruction?
2. `i_lut_act_reset` acts on the level: should it clear itself like the done flag, so that one write is enough?
3. Word or byte addressing of the GVU registers (B17)?
4. Path from vbank2 to DRAM and the owner of the vbank2 buffer flip (F8).
5. Average pool: accept Reg 25 / 26 and opcode 8, or another place for Avg_Scale / Avg_Shift?
6. B20b: how should stage 3 handle an exponent E = 0 (the B20 patch shifts by `E - 1`, which is -1 there)? B21: the table-init done output is selected by
   opcode, so a change of operation can make an extra rising edge; is that intended?
7. Softmax token length is 9 bits (L ≤ 511, P-SMX3): widen it for longer sequences?
