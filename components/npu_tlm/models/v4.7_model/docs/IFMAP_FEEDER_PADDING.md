# Convolution padding in the IFMAP feeder: design notes for the hardware team

This document describes the convolution padding that the RTL-ref core of this model generates inside the IFMAP feeder
(option `HAS_FEEDER_PAD=1`), at the level needed to implement it in RTL: what SRAM-A holds, which registers are added,
which signals are added in the feeder and its index counter, what the Data Flow Controller computes per tile, what it
costs, and how an RTL implementation can be checked against the model. It is a design of this model; the SAURIA RTL
has no such logic, and the "Input Padding Unit" of the micro-architecture specification is still open.

Related documents: `HARDWARE_CONFIG.md`, section 6.5 (option and registers), `VERIFICATION_REPORT.md`, V31 and V33
(results), `KNOWN_LIMITATIONS.md`, O21 and O24, `SW_INTEGRATION_GUIDE.md`, section 3.3 (instruction fields `PAD_VALUE`,
`PAD_T`, `PAD_L`, `PAD_B`, `PAD_R`, `DILATION`).

## 1. Two places to pad

A convolution reads, for every output tile, an input window that may reach outside the image. The positions outside
take the padding value (the input zero point for an asymmetric int8 network). The window can be completed at two
places:

| | Way 1: on the DMA path (default of the model) | Way 2: in the IFMAP feeder (`HAS_FEEDER_PAD=1`) |
|---|---|---|
| SRAM-A holds | The whole window `[Cin][ah][aw]`, padding included | Only the part inside the image, `[Cin][hr][wr]`, behind `lead` unused elements |
| Who writes the padding | The DMA (3-D descriptors of the default run profile) or a padding block on the DMA-to-SRAM-A path | Nobody: the feeder substitutes the padding value while it gathers |
| Bus traffic | Real elements only (the padding is generated next to SRAM-A) | Real elements only |
| Core | Unchanged; runs a convolution without padding | One multiplexer on the SRAM-A word stream, four configuration registers |
| Activation counters | Steps of the padded window (`aw`, `ah`) | Steps of the real part (`wr`, `hr`) |
| Results | Reference | Equal bit for bit on every program run (section 8) |

A padding block on the DMA path that writes the padded window into SRAM-A row by row (row stride rounded up to 32
elements) was compared with way 1 of the model with Verilator: on 88 tile windows of YOLOv8m (corner, edge, stride-2,
right-heavy `PAD_TAIL` and input-channel-split tiles, padding values 0 and -128) the SRAM-A content was identical byte
for byte, apart from the row stride, which the activation counters then have to use (the Verilator harness of that
comparison is not part of this package). Both ways are therefore available
to the hardware: way 1 keeps the core unchanged; way 2, described below, keeps SRAM-A smaller (only the real part) and
needs no padding logic outside the core.

## 2. Way 2 at a glance

```text
 DRAM image ──DMA (real rows x real columns only)──▶ SRAM-A:  [lead][Cin][hr][wr]
                                                                  │  words of 32 elements
 Data Flow Controller                                             ▼
   padding generator (per tile) ──▶ ACT_PAD_CTRL/COL/ROW_LO/ROW_HI     IFMAP feeder
   six activation-counter fields                     ┌──────────────────────────────────────────┐
   (row step wr, channel step wr x hr)               │ index counter ─▶ address, woffs          │
                                                     │   + pad tags (window column, row)        │
                                                     │ sram_data_q ─▶ [pad mux x32] ─▶ 32 lanes │
                                                     │   pad_ok[i] from tags and ACT_PAD_*      │
                                                     └──────────────────────────────────────────┘
```

The lanes, their FIFOs, the skew registers, the dilation pattern and the rest of the core are unchanged. No pipeline
stage is added.

## 3. SRAM-A layout and the lead

For a tile whose input window is rows `[iy0, iy1)` and columns `[ix0, ix1)` of the image (`ah = iy1 - iy0`,
`aw = ix1 - ix0`, sized with the effective kernel `(K - 1) x DILATION + 1`):

```text
pt = max(0, -iy0)                 pl = max(0, -ix0)            padding rows above / columns left inside the window
hr = min(iy1, IN_H) - max(iy0, 0)  wr = min(ix1, IN_W) - max(ix0, 0)   real part of the window
lead = pt x wr + pl
SRAM-A element (c, y, x) of the real part, at lead + (c x hr + y) x wr + x,  0 <= y < hr, 0 <= x < wr
```

The feeder addresses window element (channel `c`, window row `wy`, window column `wx`) at `c x wr x hr + wy x wr + wx`,
exactly as the unchanged address counters do with the row step `wr` and the channel step `wr x hr`. With the lead in
front, the real element `(wy - pt, wx - pl)` sits at that address, so the counters need no offset and never go below
zero. Addresses of window positions outside the image point into the lead, into other real elements, or past the real
part (old SRAM-A content); their data is never used, because the mask replaces it (the model fills the lead with
`0x5A` so that a masking fault shows). Since `wr <= aw` and `hr <= ah`, every address of way 2 is at most the address
of the same position in way 1 (row sweeps included), so a window that fits SRAM-A in way 1 also keeps every address of
way 2 inside SRAM-A.

Worked example (3x3 kernel, stride 1, padding 1, image 6 x 6, one output tile 4 x 6, window rows `[-1, 5)`, columns
`[-1, 7)`):

| Value | Result |
|---|---|
| `pt`, `pl` | 1, 1 |
| `hr`, `wr` | 5 rows (image rows 0 to 4), 6 columns (image columns 0 to 5) |
| `lead` | 1 x 6 + 1 = 7 |
| `ACT_PAD_COL` | valid window columns `[1, 7)`: `0x0007_0001` |
| `ACT_PAD_ROW_LO`, `ACT_PAD_ROW_HI` | `1 x 6 = 6`, `(1 + 5) x 6 = 36` (window rows 1 to 5, in units of the row step) |

## 4. Registers

Four configuration registers of the core, in the activation-feeder region (offsets of the core configuration map).
The Data Flow Controller writes them before the start pulse of a core pass; the core reset clears them, so a pass
without feeder padding writes nothing. Software does not write them; software writes the instruction fields of
`SW_INTEGRATION_GUIDE.md`, section 3.3.

| Offset | Name | Fields | Reset |
|---|---|---|---|
| `0x448` | `ACT_PAD_CTRL` | bit 0: enable; bits 15:8: padding value (int8, two's complement) | 0 |
| `0x44C` | `ACT_PAD_COL` | bits 15:0: first valid window column (`pl`); bits 31:16: one past the last valid column (`pl + wr`) | 0 |
| `0x450` | `ACT_PAD_ROW_LO` | first valid window row times the row step (`pt x wr`) | 0 |
| `0x454` | `ACT_PAD_ROW_HI` | one past the last valid window row times the row step (`(pt + hr) x wr`) | 0 |

Rows are given in the unit of the address counters (window row x row step) so that the comparator works on the counter
value directly, without a multiplication. The row fields need the width of the activation index counters (18 bits in
the configuration of this model); the column fields are 16 bits.

Six existing activation-counter fields change with the real part (same formulas as for a staged window, with `wr`
and `hr` in place of `aw` and `ah`; `kh` kernel height, `d` dilation, `cin` input channels of the pass, `h_til` output
rows of the tile):

| Field | Value with feeder padding |
|---|---|
| `ACT_YLIM` | `wr x ((kh - 1) x d + 1)` |
| `ACT_YSTEP` | `wr x d` |
| `ACT_CHSTEP` | `wr x hr` |
| `ACT_CHLIM` | `wr x hr x cin` |
| `ACT_TIL_YSTEP` | `wr x stride` |
| `ACT_TIL_YLIM` | `h_til x wr x stride` |

`ACT_XLIM`, `ACT_XSTEP`, `ACT_TIL_XLIM`, `ACT_TIL_XSTEP` and the dilation pattern do not change: columns keep their
window coordinates.

## 5. Feeder datapath

The reference is `data_feeder/rtl_ref_ifmap_feeder_rtl.h` and `data_feeder/rtl_ref_ifmap_idxcnt.h` (names of the SAURIA
RTL in brackets). Everything below is gated by the enable bit; with it 0 the feeder is the SAURIA feeder bit for bit.

1. **Pad tags in the index counter** (`ifmap_idxcnt`). Next to the address, two tags describe where the address lies
   in the tile window:
   - `pad_col0` = value of the tile-X counter (window column of lane 0, kernel tap 0);
   - `pad_row` = (kernel-Y counter + tile-Y counter), masked to the counter width (window row x row step).

   They go through the same two register stages as the word offset, with the same clear and enable (stage 1 with
   `sram_idx_q`, stage 2 with `woffs` behind the output shim), so tag and address always belong to the same word.
2. **Word registers next to the lanes** (lane-independent copies of what `feed_data_manager` keeps per lane, same
   enable as the lanes' pipeline registers and same clear):
   - at an X transition (`x_ov_flag_q`, start or final push): latch `pad_gw_q` = global word offset (`glob_woffs`,
     without the per-lane part), `pad_col0_q`, `pad_row_q`, and reset the stream position `pad_sc_q` to 0;
   - otherwise `pad_sc_q` advances by 32 per enabled cycle, modulo 2^`PARAMS_W` (`PARAMS_W` = 8, as
     `shift_idx_cnt_q` of the lanes).
3. **Mask**, registered from the same pre-clock values as `shifted_dil_pat_q`, so that it describes the word the lanes
   gather on the next enabled cycle; for element `i` = 0 to 31 of that word:

   ```text
   col_i     = pad_col0_q + pad_sc_q + i - pad_gw_q                (window column of element i)
   row_ok    = ACT_PAD_ROW_LO <= pad_row_q < ACT_PAD_ROW_HI
   pad_ok[i] = row_ok and COL_LO <= col_i < COL_HI
   ```
4. **Multiplexer** on the word (`srama_data_mux`): when the final-push path is not active,
   `srama_data_mux[i] = (enable and not pad_ok[i]) ? pad_value : sram_data_q[i]`. The final-push path is unchanged.

Logic per cycle: 32 additions for the element columns, 64 column comparisons, 2 row comparisons, 32 two-input
multiplexers of 8 bits. Whether this fits one clock period at the target frequency has not been evaluated. A cheaper
equivalent: per word compute `lo = COL_LO - (pad_col0_q + pad_sc_q - pad_gw_q)` and `hi = COL_HI - (...)` once, then
compare the 32 constant indices `i` with `lo` and `hi`.

**Dilation.** The mask compares element positions, so dilation needs nothing in the mask; only `ACT_YSTEP` and
`ACT_YLIM` change (section 4). The limit `(KW - 1) x DILATION <= 63` of the 64-position dilation pattern is unchanged.

## 6. Padding generator of the Data Flow Controller

Reference: `has/has_pad_gen.h` (unit test `tools/has/tb_has_pad_gen.cpp`). Inputs per tile: image size, the tile
window `[iy0, iy1) x [ix0, ix1)`, kernel, stride, dilation, padding value. It decides whether the tile uses feeder
padding and computes the values of sections 3 and 4.

The staged way (way 1) is kept for a tile when:

| Case | Reason |
|---|---|
| 1x1 kernel, stride 1, dilation 1 | Such a layer can use the aligned address mode of the feeder (one SRAM word per group of positions, when the input channels and `Y_USED` are multiples of 32); that mode needs rows that are a multiple of 32 elements, which `wr` is not. The generator keeps way 1 for every such layer. In YOLOv8m their only padding comes from tile widths rounded up (`PAD_TAIL`) |
| The window lies entirely in the padding | Nothing to load |
| The window lies entirely inside the image | No padding in this tile |
| A value does not fit its register | `pl + wr` > 65,535, or `(pt + hr) x wr` or `ah x wr` exceed the counter width |

Hardware view: comparators and subtractors on the window bounds and a few multiplications that share operands
(`pt x wr`, `hr x wr`, `(hr x wr) x cin`, `wr x kh_eff`, `wr x stride` and its product with the tile height). The model
treats it as combinational, like every other per-tile configuration value it computes. In hardware it can be
combinational, a few sequential steps in the Data Flow Controller, or firmware.

## 7. Cost and limits

| Item | Value |
|---|---|
| Configuration | 4 register writes per core pass with feeder padding, 8 cycles in the model |
| Whole YOLOv8m | 4,006 core passes padded by the feeder; 62,666,123 cycles with way 1, 62,698,248 with way 2 (+0.05 %); core busy cycles equal (55,653,442); both measured with the vector-unit knobs off (`HAS_GVU_TOP=0 HAS_GVU_STALL=0 HAS_GVU_ELEM_CLK=0 HAS_GVU_REGS=0 HAS_GVU_AGU_ND=0`, `BUILD_AND_RUN.md`, section 6) |
| Core busy | Equal in total on the whole YOLOv8m and on the padding check programs. On the dilation check program one extreme tile (1x8 kernel, dilation 9, window 103 columns of which 40 real) runs 8 cycles longer, every other tile is equal: the real rows fall differently on the 32-element words |
| SRAM-A | `lead + Cin x hr x wr` elements instead of `Cin x ah x aw` |
| Row sweep | One row sweep of the feeder is limited to 256 positions by the 8-bit lane counters (`KNOWN_LIMITATIONS.md`, O24), with or without padding; the mask uses the same counter width. Instructions past it are refused |

## 8. Checking an RTL implementation against the model

The model with `HAS_FEEDER_PAD=1` is the reference for way 2; the model without it (way 1) and the independent integer
reference `tools/has/ref_program.py` are the references for the results.

| Material | Use |
|---|---|
| `tools/has/make_pad_gate.py <dir>` | Check program: 13 convolutions with the old `PAD` form and per-side padding, padding values -128, -121, -3, 0, 5, 7, 127, kernels 1x1 to 7x7, strides 1 and 2, 4 x 2 tiles, `PAD_TAIL`, 64 input channels in two passes. Golden image computed in the script |
| `make_pad_gate.py --set random`, `--set dilation`, `--set dilrandom` | Random padded layers; dilation 2 to 9 with and without padding; random dilation layers |
| `tools/has/ref_program.py <dir>` | Independent result for any of these programs |
| `tools/has/tb_has_npu_top <dir>` with and without `HAS_FEEDER_PAD=1` | Results and `core busy` of both ways; they must be equal (results) and equal or explained (core busy, section 7). Use the same vector-unit knobs in both runs; the knobs-off prefix of `BUILD_AND_RUN.md`, section 6, reproduces the numbers of this document |
| `tools/has/tb_has_pad_gen.cpp` | The padding generator alone: 4,000 random layers, every element the feeder reads addressed with the generator's registers and compared with the image or the padding value; a hardware or firmware generator can be compared with its values |
| Build flag `-DFX1_A3_FEED_SIG` (for `tools/has/tb_has_npu_top`) | One line per cycle and core pass with the feeder's status flags (`done`, `til_done`, `fifo_empty`, `fifo_full`, `stall`) and a hash of the 32 lane outputs (`SAURIA_FEED_SIG_FULL=1`: the 32 values); file named by `SAURIA_FEED_SIG`. Cycles are numbered from the start pulse of each pass, so an RTL simulation of the same tile can be compared cycle by cycle |
| `tools/has/regress_gvu.sh` | Runs the check programs, the random and dilation sets and the generator test in both ways |

Measured with the model (`VERIFICATION_REPORT.md`, V31 and V33): check program 13 of 13 in both ways and in four run
profiles; random layers (43 + 15 + 15) and random dilation layers (90) bit-exact in both ways; generator test 30,751
padded tiles and 22,392,897 elements without a failed check; 8 of 8 one-line faults in the feeder, its index counter,
the generator and the top level detected by the check program; feeder outputs identical cycle by cycle with both ways
on YOLOv8m instructions 86 to 88 and on a two-instruction window; whole YOLOv8m bit-exact in both ways. On some core
passes of the check program the `fifo_full` and `stall` flags change at other cycles than with way 1 while the lane
values and the core busy cycles stay equal (cause not established), so a cycle comparison of an RTL feeder should
compare way 2 of the model with way 2 of the RTL, not with way 1.

## 9. Questions for the hardware team

1. Way 1 or way 2? Way 1 keeps the core as in SAURIA and moves the padding to the DMA side; way 2 keeps SRAM-A smaller.
2. Way 2: does a multiplexer on the SRAM-A word stream before the lanes fit the feeder RTL of the team, or should the
   padding be applied elsewhere (inside each lane)?
3. Way 2: do the column additions and comparisons fit one clock period, or should the cheaper form of section 5 be
   used?
4. Who computes the per-tile values of section 6 (Data Flow Controller logic, sequential steps, firmware)?
5. Do the offsets `0x448` to `0x454` collide with anything in the team's register map?
6. Does the team keep `PARAMS_W` = 8 in the feeder lanes (row sweep limit 256)? Widening only the lane counters removes
   the limit in the SAURIA RTL (measured); the model would follow.
7. Does the team keep the 64-position dilation pattern and the limit `(KW - 1) x DILATION <= 63`?
