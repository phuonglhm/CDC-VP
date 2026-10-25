# Slice and concat on the GVU

How the SystemC model handles the slice, split and concat operators of YOLOv8m and ViT-B/16, the options for doing them on the vector
unit (GVU) as the MAS describes, what the model implements, what was measured, and the RTL change proposed to the hardware team.

Labels used for numbers: **MEASURED** (simulated or counted), **ESTIMATED** (computed from sizes, not simulated), **READ** (taken from
the RTL or MAS text).

## 1. What the networks need

### 1.1 MAS intent (READ)

- **GVU block diagram, VLSU.** The VLSU has "an AGU at each SRAM port" with address modes for "transpose, reshape, split/slice, concat
  and padding".
- **VLSU section.**
  - Reshape, concat, slice and split "only alter the method of generating read/write addresses".
  - The AGU has a "4D stride address mode, each dimension has its own stride and repetition factors".
- **Delivered RTL.**
  - `agu_mode_t` has `AGU_FIXED`, `AGU_INCR` and `AGU_POOLING`; code 3 is unused.
  - `vlsu_stride_t` / `vlsu_rep_t` (3 dimensions × 8 bits) are declared but not used.
  - The controller drives the AGU mode and start address with constants per state; no register configures the AGU.

### 1.2 YOLOv8m (MEASURED on the model's graph: 129 operators = 98 NPU instructions)

YOLOv8m has 16 slices and 13 concats, **all on the channel axis**. Every input tensor feeds at most one concat.

| Block | Slices | Concat | Producers of the concat inputs |
|---|---|---|---|
| C2f `model.2` (160×160) | 96 → 48 + 48 | → 192 ch, 4 inputs | slice, slice, 2 residual ADDs |
| C2f `model.4` (80×80) | 192 → 96 + 96 | → 576 ch, 6 inputs | slice ×2, 4 residual ADDs |
| C2f `model.6` (40×40) | 384 → 192 + 192 | → 1,152 ch, 6 inputs | slice ×2, 4 residual ADDs |
| C2f `model.8` (20×20) | 576 → 288 + 288 | → 1,152 ch, 4 inputs | slice ×2, 2 residual ADDs |
| SPPF `model.9` (20×20) | — | → 1,152 ch, 4 inputs | conv + 3 chained MAX_POOL 5×5 s1 p2 |
| Neck C2f `model.12/15/18/21` | 2 each | 4 concats | slice ×2 + convs |
| Neck branch joins | — | 4 concats (960@40², 576@80², 576@40², 960@20²) | upsample (host) + conv; conv + conv |
| Detect | — | anchor-axis concat of the outputs | outside the NPU (DFL on the host) |

Sizes:
- Concatenated data: 23,270,400 bytes, which is 948,480 rows of 32 lanes in the Scratchpad.
- Sliced data: 7,219,200 bytes.

GVU operations that touch a slice or a concat:
- 12 residual ADDs: each writes its result into a concat cell, and 4 of them read a slice as operand A.
- 3 MAX_POOLs of SPPF: each writes into the SPPF concat.

### 1.3 ViT-B/16 (READ, per block, × 12 blocks)

- **Slices.** 36 slice views: Q, K and V of 12 heads, each 64 × 197 bytes, contiguous in a [2304][197] tensor.
- **Concat.** One concat of the 12 heads: [768][197], 151,296 bytes.
- **Axis.** All of them are on the outermost (channel) axis.
- **Channel-major access.** FUSED_ATTN reads Q / K / V and writes O channel-major through a 3-D strided descriptor that exists only in the
  model. This is an element-level transpose, not a block-of-rows operation.

## 2. Options

### 2.0 Compiler address alias (current behaviour)

- **How.** Every slice and every concat is on the outermost axis of the DRAM layout ([C][H][W], ViT [C][L]), so each piece is a
  contiguous block. The compiler gives the producer of each concat input the output address of its cell in the concatenated tensor. It
  gives the consumer of a slice the address of the slice inside its source. The data never moves.
- **Registers.** None. The addresses are in the existing instruction / DMA address fields.
- **Who programs.** The compiler; the DFC does not see the slice or concat.
- **Cost.** 0 cycles, 0 bytes (READ: no copy instruction is emitted; no host copy step is needed in either network).
- **Limits.** It works only for contiguous blocks, that is, the outermost axis without conflicting offsets. It does not work for:
  - inner-axis slices or concats;
  - NHWC layouts;
  - the Detect anchor concat;
  - a tensor that feeds two concats;
  - data that should stay in the Scratchpad between two GVU operations.
- **Status.** This is the model's default for both networks, measured end to end in all whole-network runs.

### 2.1 (A) AGU_ND on the ports of the residual add and max pool

- **How.** A new AGU mode, `AGU_ND` (code 3 of `agu_mode_t`). One unit per Scratchpad port computes:

  ```
  addr = base + i0*s0 + i1*s1 + i2*s2 + i3*s3,   0 <= ik < nk,   i0 innermost
  ```

  - The counters step once per access of the port. The offsets are accumulated, with no multiplier.
  - An idle cycle keeps the position, unless `idle_reload` is set.
  - At the end of the program the address goes back to base, or keeps the last address with `stop`.
  - `i_agu_addr_reset` returns to base.
  - The port keeps one address register; `AGU_ND` only computes the next value it loads.
- **Use in the residual add.**
  - `RESIDUAL_ST1` reads operand A through the **source** descriptor.
  - `RESIDUAL_ST2` writes the result through the **destination** descriptor.
  - The controller picks the descriptor from the state the address register enters at the clock edge.
- **Use in the max pool.** With a destination bound to vbank2, output row k is written at `addr_at(k)`. The row counter steps at the
  feeder's `tile_last` and returns to base at `o_pooling_last_wr`.
- **Registers.** Reg 16..24, with `GVU_REG_AW` 5 (32 registers). Word index = byte address / 4.

  | Reg | Byte | Name | Bits |
  |---|---|---|---|
  | 16 | `0x40` | `AGU_ND_CTRL` | [2:0] source port, [3] source on, [6:4] destination port, [7] destination on, [8] idle_reload, [9] stop; ports: 0 sbank P0, 1 sbank P1, 2 vbank0, 3 vbank1, 4 vbank2 |
  | 17 / 21 | `0x44` / `0x54` | `AGU_SRC_0` / `AGU_DST_0` | [11:0] base, [21:12] n0 − 1, [31:22] n1 − 1 |
  | 18 / 22 | `0x48` / `0x58` | `AGU_SRC_1` / `AGU_DST_1` | [9:0] n2 − 1, [19:10] n3 − 1 |
  | 19 / 23 | `0x4C` / `0x5C` | `AGU_SRC_2` / `AGU_DST_2` | [15:0] s0, [31:16] s1 (two's complement) |
  | 20 / 24 | `0x50` / `0x60` | `AGU_SRC_3` / `AGU_DST_3` | [15:0] s2, [31:16] s3 (two's complement) |

  Reset value 0 = nothing bound = the delivered behaviour.
- **Who programs.**
  - The DFC writes the words that changed before the Reg 0 start.
  - The compiler supplies the programs: base = cell offset, n / s = layout of the block.
- **Cost.**
  - Values and unit cycles are the same as with the alias (same number of accesses).
  - Register writes: measured in the model on the 6-instruction and MAX_POOL test programs. 5 to 16 extra writes per program; the DFC
    wait grows by exactly writes × write latency.
  - For YOLOv8m (12 ADD, 3 MAX_POOL) at 4 cycles per write: at most about 600 cycles, about 0.001 % (ESTIMATED).
  - No extra memory.
- **Risks and limits.**
  - The residual add with AGU_ND needs `num_tile` = 1.
  - Programs must stay inside the real depth of the bank; vbank0 has 800 of its 1,024 rows (MEASURED in the model).
  - Rows of 32 lanes only.
  - The 32-register file is shared with the residual-add proposal (Reg 9..15).
- **Status.** Model and RTL patch done and checked (sections 4 and 5).

### 2.2 (B) SPPF chain kept in the Scratchpad

- **How.** The three chained MAX_POOLs of SPPF would read each other's output from the Scratchpad instead of going through DRAM.
- **Why it is not address-only.** The H padding of the next pool is an address offset of 2 rows. The W padding needs a shift of 2 lanes:
  20 elements in lanes 2..21 of a padded row. That is lane rotation, which the MAS lists as a VLSU padding block (TBD).
- **Registers.** AGU_ND destination plus a lane-rotation / padding block; no register layout is proposed yet.
- **Who programs.** The DFC, with a fused SPPF sequence from the compiler.
- **Cost.** It would save about 3 × 115,200 bytes of DMA out and in, about 2–4 × 10⁴ cycles per network (ESTIMATED).
- **Risks.** It needs new hardware (rotation) and a fused-sequence format.
- **Status.** Not modelled. Deferred.

### 2.3 (C) MOVE operation and "concat through the GVU"

- **How.** A new ELEM_WISE operation, MOVE (proposed opcode 4'd10). Each cycle it reads one row of a source port through the AGU_ND
  source descriptor and writes it, two cycles later, to a destination port through the destination descriptor. It does slice, concat,
  split and permutations of blocks of rows on any axis above the 32-lane row: inner-axis concat, [C][H][Wt] ↔ [H][C][Wt], gathering
  attention heads, and tiles kept in the Scratchpad.
- **"Concat through the GVU".** A compiler mode that would move every concat through the GVU instead of aliasing addresses, to show the
  path works end to end.
- **Registers.** Reg 16..24 as in (A), plus the opcode.
- **Who programs.** The compiler emits MOVE; the DFC writes Reg 16..24 and starts it.
- **Cost.**
  - MOVE of N rows: N + 3 cycles (MEASURED in the model).
  - Replacing every YOLOv8m concat alias with MOVE: copy 23.3 MB each way through DMA plus 948,480 GVU rows. That is
    +1.45 M … +3.86 M cycles, +2.3 % … +6.2 % of the network (ESTIMATED).
  - No extra DRAM if results are written directly to their cells.
- **Risks.** Slower than the alias wherever the alias works. Useful only where the alias cannot be used.
- **Status.**
  - MOVE is modelled and checked on the frozen Scratchpad and VLSU ports (section 4).
  - The compiler mode and the RTL state are not done.

### 2.4 Comparison and recommendation

| | Alias (now) | (A) AGU_ND on ADD / max pool | (B) SPPF in Scratchpad | (C) MOVE |
|---|---|---|---|---|
| Cycles, YOLOv8m | 0 | ≤ ~600 register-write cycles (ESTIMATED) | −2…−4 × 10⁴ (ESTIMATED) | +2.3 … +6.2 % if used for every concat (ESTIMATED) |
| Memory | 0 | 0 | less DMA traffic | 0 (direct to cells) |
| Covers | outermost axis, contiguous | as alias + strided blocks in the Scratchpad for ADD / max pool | SPPF chain | any block-of-rows permutation |
| New hardware | none | AGU mode + 9 registers + controller selection | AGU_ND + lane rotation | AGU_ND + MOVE state |
| Evidence | whole networks | model + RTL patch, simulated (sections 4, 5) | none | model only |

**Recommendations for the RTL:**
1. **Adopt (A).** It is the address-only mechanism the MAS describes, and it costs one AGU unit per port plus nine registers. It keeps
   the delivered behaviour when nothing is bound. The patch in section 5 implements it and has been simulated against the model.
2. Keep the compiler alias for outermost-axis slices and concats; it is free.
3. Add MOVE (C) when a network needs inner-axis or NHWC concats, or tensors kept in the Scratchpad.
4. Consider (B) together with the VLSU padding block.

Two rules come from the simulation and must be kept in any implementation:
- A change of descriptor takes effect at the clock edge of `i_agu_addr_reset`. An access in that cycle still uses the old program.
- Keep one address register per port. A second register with a mux gives wrong addresses when a port changes between bound and unbound.

## 3. Questions for the hardware team

1. Who does the element-level transpose for attention (the MAS assigns it to the PSUM path, TBD)?
2. Is the VLSU padding / lane-rotation block planned?
3. Is the VLSU AXI master of the MAS planned? It decides whether a concatenation is assembled in DRAM by DMA addresses or by the GVU.
4. Are Reg 16..24 and `GVU_REG_AW` = 5 acceptable, together with Reg 9..15 of the residual-add proposal?

## 4. The model

| File | Content |
|---|---|
| `has/gvu_agu_nd.h` | `AguNdDesc` (with `addr_at(k)`), `AguNdCtrl`, `AguNdRegs` (Reg 16..24 pack / unpack), `GvuAguNd` (one unit, `eval` / `tick` in the style of the RTL ports) |
| `has/gvu_agu_nd_vlsu.h` | `GvuVlsuNd`: the frozen VLSU port plus five AGU_ND units, without editing the frozen header. A bound port is handed to the frozen unit as `AGU_FIXED` with the start address = the next AGU_ND address, so the frozen address register is the AGU_ND register. `sel` gives the per-state pick. `GvuVlsuNdPort` has the interface of the frozen `GvuVlsu` and calls it directly while nothing is bound |
| `has/gvu_agu_nd_move.h` | `GvuMove`: the MOVE operation on the frozen Scratchpad and VLSU ports |
| `has/rtl_ref_gvu_top.h` | `In::i_agu_nd_ctrl`, `i_agu_src`, `i_agu_dst`; the VLSU member is `GvuVlsuNdPort`; residual add source / destination pick per state; max-pool destination `addr_at` |
| `has/gvu_top_bridge.h`, `has/gvu_top_stream.h` | knob `HAS_GVU_AGU_ND`; the residual add and the max pool load and read back through the programs; with `HAS_GVU_REGS=1` the programs come from Reg 16..24 |
| `has/gvu_regs_dfc.h` | `agu_nd(...)`: the Reg 16..24 words that changed, written ahead of the operation's sequence |
| `tools/has/tb_gvu_agu_nd.cpp` | T1 random programs vs nested loops; T2 INCR / FIXED vs the frozen VLSU; T3 pooling program vs `AGU_POOLING`; T4 register packing; T1b `addr_at` vs the unit |
| `tools/has/tb_gvu_agu_nd_move.cpp` | T5 `GvuVlsuNd` vs the unit; T6 MOVE cases (concat C / H, slice of W tiles, permutation, split, C2f / SPPF tiles, int32 ports, random programs); T7 idle cycles kept vs reloaded; T8 pass-through and per-state pick |

**Knob `HAS_GVU_AGU_ND`:**

| Level | Meaning |
|---|---|
| 0 | Nothing bound; the unit behaves as the RTL drop |
| **1 (default in this delivery)** | The residual add and the max pool run through AGU_ND identity programs that give the same rows as level 0; the reference behaviour for the hardware |
| 2 | Shifted programs, for demonstration: the residual add reads A at vbank0 rows 37 + 2r and writes the result at rows 400 + r; the max pool writes its output rows permuted [C][OH][Wt] → [OH][C][Wt]. Loads and read-backs follow the programs |

With `HAS_GVU_REGS=1`, the DFC writes Reg 16..24 and the unit reads them from the register file.

**Results (MEASURED, Windows build of the model):**

| Check | Result |
|---|---|
| `tb_gvu_agu_nd` | PASS (0 of 5): 7,554,274 random accesses, 221,888 INCR / FIXED cycles, 189 pooling runs, 10,000 register sets, 7,119,266 `addr_at` accesses, 0 differences |
| `tb_gvu_agu_nd_move` | PASS (0 of 4): 600,000 cycles; 25 named MOVEs (3,098 rows) and 300 random MOVEs (10,106 rows), 0 wrong rows, N + 3 cycles each; idle cycles kept 0 wrong (reloaded as `AGU_INCR` does: 2,930 wrong rows, the expected fault); per-state pick 1,163,171 accesses, 0 differences; 20 extra seeds PASS |
| Level 0 vs the model without the change | 6-instruction and MAX_POOL test programs at every GVU level, stream / parameter / element-wise / quantisation benches: 112 / 112 output files identical byte for byte, cycles included |
| Levels 1 and 2 vs level 0 | 112 / 112 output files identical byte for byte (values and cycles) |
| Level 2 with the read-back not following the program (negative control) | residual-add and max-pool steps FAIL, GEMM steps PASS: the programs really drive the addresses |
| Register path (levels 1 / 2 with `HAS_GVU_REGS=1`) | DRAM images identical; every step PASS; extra register writes 5–16 per program, DFC wait + writes × latency exactly; with a bench that polls every cycle, total cycles within ±3 of writes × latency |

The register-sequence bench `tb_gvu_regs_dfc` checks the absolute write counts of the sequences without Reg 16..24. Run it with
`HAS_GVU_AGU_ND=0`, or extend its expected counts.

## 5. RTL patch

**Files**
- `agu_nd_full.diff`. Applies on the RTL drop of the feedback package (archive SHA-256 `90694d8f06956c623fbdc29bb8db0266ec2339dbb1b7121c3e93aef95c19d67d`)
  plus, in this order: B13, B4, B14, B17, B12, B16, B1, B9, B18, F1′ (v3), F2b (v2), B19 and the residual-add patch. 7 files, +251 / −3.
  - `gvu_pkg.sv`: `AGU_ND`, `agu_nd_desc_t`, `agu_nd_ctrl_t`, Reg 16..24 names.
  - `vlsu/gvu_agu_nd.sv`: new.
  - `gvu_vlsu.sv`: 5 port units, pooling destination unit, 4 new inputs.
  - `gvu_config_regs.sv`: Reg 16..24 decode.
  - `gvu_core.sv`: wiring.
  - `gvu_top.sv`: inputs, pick from the controller to the VLSU.
  - `gvu_pipe_controller.sv`: AGU_ND selection in `RESIDUAL_ST1` / `ST2`.
- `agu_nd_vlsu.diff`. The VLSU part alone, on the RTL drop plus all patches of the feedback package. For review only.

**How to apply.** `git apply agu_nd_full.diff` from the root of the RTL tree, after the patches above. The CRLF line ends of the sources
are kept in the diff.

**Interaction with F2b.** The direct sbank addresses of F2b keep priority over the sbank address registers, and so over AGU_ND on the
sbank ports. F2b is enabled only in GEMM_FUSED, and AGU_ND is selected only in the residual add and for the max-pool destination, so the
two do not meet.

**Simulation (MEASURED, ModelSim; expected values from the model):**

| Check | Result |
|---|---|
| Compile `gvu/` (except `gvu_core.sv`, which needs the AXI library missing from the delivered tree) and benches | 0 errors, 0 warnings |
| VLSU, nothing bound, vs the unpatched VLSU, random inputs | 200,000 cycles, every output, 0 differences |
| VLSU AGU_ND programs vs the model | 600,000 cycles × 5 port addresses, 0 differences |
| VLSU max-pool destination vs `addr_at(k)` | 189 runs, 88,371 cycles, 0 differences |
| Residual add on `gvu_top`, nothing bound, vs the base | 50 cases: results, sums, cycle counts and a clock-by-clock trace (7,722 lines) identical; cycle counts equal to those of the residual-add patch alone |
| Residual add with A at rows 37 + 2r (source s0 = 2) and the result at rows 400 + r | 27 cases, 1,473 rows: equal to the unshifted run and to the model, A not overwritten, cycle counts equal |
| Reg 16..24 decode vs the model's register layout | 2,000 random register sets, 0 differences |
| Max pool on `gvu_top` with destination programs (offset, permutation, stride 2, reversed) | unbound runs = model in rows and cycles; 1,848 result rows at `addr_at(k)` correct, other rows untouched; trace identical except the vbank2 address; cycle counts equal |

**Not checked yet.**
- `gvu_core.sv` wiring was checked by reading only, and the AXI-Lite register writes were not simulated.
- The average pool with a destination was not simulated.
- MOVE is not in the RTL.
