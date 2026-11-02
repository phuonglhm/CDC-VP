# Slice and concat by 4-D DMA descriptors (`HAS_DMA_ND`)

Model proposal, default off. The RTL DMA (`redma_top`) copies one contiguous block (reader / writer start address and
bytes to transfer, registers 0x00..0x3C of `conf_regs.sv`); it has no strided or multi-dimensional mode. Nothing in this
note is in the RTL drop.

## 1. Two ways to slice and concat, both kept

| Path | Knob | Default | Where the data moves |
|---|---|---|---|
| Address generator of the vector unit (AGU_ND, Reg 16..24) | `HAS_GVU_AGU_ND` | 1 | rows of the vector-unit Scratchpad, `has/gvu_agu_nd*.h`, `SLICE_CONCAT_ON_GVU.md` |
| 4-D DMA descriptors (this note) | `HAS_DMA_ND` | 1 | DRAM to DRAM through the DMA, and the tile window / write-back of GEMM_FUSED |

The two knobs are independent. `HAS_DMA_ND=1` does not change the vector unit: its register file, its Scratchpad loads
and its cycles stay those selected by `HAS_GVU_*`. Running the vector unit without the AGU_ND programs is
`HAS_GVU_AGU_ND=0`, which already exists.

`HAS_DMA_ND` is separate from `HAS_DMA_TENSOR` (opcode 0x16 `DMA_TENSOR`, op COPY): the two encode the operation
differently (section 5, open question).

## 2. Descriptor (`has::DmaNdDesc`, `has/has_dma.h`)

Fields: `base` (byte address), `n[4]` loop counts from the fastest dimension (0) to the slowest (3), `s[4]` signed strides
in elements, `elem_bytes`. Item `k` of the transfer lies at

    i0 = k mod n0, i1 = (k / n0) mod n1, i2 = (k / (n0 n1)) mod n2, i3 = (k / (n0 n1 n2)) mod n3
    addr(k) = base + (i0 s0 + i1 s1 + i2 s2 + i3 s3) * elem_bytes

Builders: `linear`, `rows`, `tensor_chw` (`[C][H][W]`, dimension 0 = W), `sliced` / `sliced_chw` (offset, length, step
along one axis), `concat_chw` (sub-window of a destination tensor), `permuted`, `tile_window_3d` (input window of a tile),
`tile_writeback_3d` (output tile inside the destination tensor).

## 3. What `HAS_DMA_ND=1` changes in `HasNpuTop`

1. **Tile input window** (`fill_stage_a`): the real part of the window is gathered into the staging area with
   `tile_window_3d` (`HasDma::gather_dram_nd`). Same bytes as the loop it replaces; the bus cycles are those of the CH1
   transfer that follows, unchanged.
2. **Tile write-back** (`writeback`, and the sequential schedule): `HasDma::start_write_sramc_nd` moves the SRAM-C words
   on the clock as before and, at the end of the transfer, scatters the int8 results into the destination tensor with
   `tile_writeback_3d`. Same bus bytes and cycles; only the place of the scatter moves into the DMA.
3. **ELEM_WISE mode 6 (slice) and mode 7 (concat)** are accepted (otherwise refused with UNSUPPORTED_MODE as before) and
   run by the DMA, DRAM to DRAM, in pieces of at most `HAS_DMA_TENSOR_CHUNK` bytes (default 1,024) through bank 4 of the
   DMA-side SRAM, the stand-in of the DMA FIFO that `DMA_TENSOR` COPY uses: piece k + 1 is read on CH0 while piece k is
   written. The core SRAM is not touched, so a tensor of any size is moved. Each piece is a sub-descriptor that keeps the
   strides (blocks of the lower dimensions that fit the chunk). The bus time is that of the bytes moved; a strided
   access costs nothing more than a linear one (an optimistic model). A piece holds whole blocks of the lower dimensions
   (whole rows, or whole channels when a channel fits), so it can be smaller than the chunk, and the dimensions are not merged
   when the destination is contiguous: for the YOLOv8m concats the pieces hold 400 to 1,000 bytes instead of 1,024, and the 37
   concat instructions of `insts_pe3_nd` take 233,618 cycles more than the same copies as `DMA_TENSOR` COPY (MEASURED, 0.36 %
   of the network; every other instruction within 300 cycles).

### 3.1 Registers of modes 6 / 7 (ELEM_WISE, opcode 0x15)

| Register | Offset | Mode 6 (slice) | Mode 7 (concat) |
|---|---|---|---|
| `A_ADDR` | 0x444 | source tensor | part A |
| `B_ADDR` | 0x448 | unused | part B, or 0 for one part |
| `OUT_ADDR` | 0x408 | destination (contiguous) | destination tensor |
| `MODE_PACK` | 0x454 | 6 | 7 |
| `STRIDE` | 0x424 | step along the axis (0 or 1 = every element) | unused |
| `X_IN_C/H/W` | 0x46C..0x474 | source shape | destination shape when `X_OUT_*` is not written |
| `X_OUT_C/H/W` | 0x478..0x480 | not used | destination shape |
| `X_POOL_MODE` | 0x4D0 | axis: 0 = C, 1 = H, 2 = W | axis |
| `X_POOL_P` | 0x4CC | first index along the axis | one part: where part A goes; two parts: must be 0 |
| `X_POOL_K` | 0x4C8 | length along the axis (output length = ceil(K / step)) | length of part A; part B fills the rest of the axis |

Refused with BAD_GEOM: a shape or length of 0, an axis above 2, `POOL_P + POOL_K` past the axis, and for two parts a
non-zero `POOL_P` or a part A that fills the whole axis. `tools/has/ref_program.py` computes both modes with the same
rules (it computes them whatever the knob; the model runs them only with `HAS_DMA_ND=1`).

## 4. Checks

- `tools/has/tb_has_dma.cpp`: D1 parity of the 1-D API with the v4.5 DMA (400 random transfers), D2 timing, D3 4-D slice /
  split / concat on the C / H / W axes and the SPPF 4-way concat, D4 SRAM-C write-back scatter, D5 C2f 3-way split /
  concat and 300 random 4-D descriptors.
- `tools/has/make_ref_gate_dma_slice_concat.py`: 27 instructions (slice / concat on the three axes, a C2f block, an SPPF
  block, average pool) on `HasNpuTop`.
- `tools/has/make_yolov8m_full_gate.py`: the YOLOv8m graph lowered with modes 6 / 7 for every split and concat, at reduced
  channels and size by default (`--ch-div`, `--hw-div`, `--section`).
- `tools/has/make_dma_nd_big_gate.py`: 10 slices and concats of 256 KB tensors, larger than SRAM-A, golden written with
  numpy (needs `numpy`).
- With the knob off the model is unchanged; with the knob on, the programs without modes 6 / 7 give the same cycles
  and the same DRAM image as with it off.

MEASURED (`bash tools/has/regress_gvu.sh`, the `HAS_DMA_ND` group, and `tools/has/tb_has_dma.cpp`):

| Check | Result |
|---|---|
| `tb_has_dma` D1 to D5 | 0 mismatches; D3 16 4-D reads and 16 4-D writes, D5 393 reads and 394 writes (252,928 bytes) |
| 27-instruction program, `HAS_DMA_ND=1` | PASS 27 / 27, 0 elements different; `tools/has/ref_program.py` gives the same DRAM image |
| 256 KB tensors, `HAS_DMA_ND=1` | PASS 10 / 10, 0 elements different |
| The same two programs with `HAS_DMA_ND=0` | the 14 and the 10 slice / concat instructions are refused, as intended |
| 6-instruction check program with the tile window and write-back by descriptors | same DRAM image and same cycles as with the knob off |
| Transformer check (no slice / concat), knob on (default) and off | 263,883 cycles both |
| YOLOv8m program `insts_pe3_nd` (`fe_emit_insts.py --plan pe3 --slice-concat dma_nd`): every run of concat instructions, with the instruction before it, from the golden image | PASS 13 / 13 windows; `tools/has/ref_program.py` MATCH 13 / 13 |

The steps of this flow and the check of each step are listed by `tools/has/verify_flows.py` (`VERIFICATION_REPORT.md`,
section on the verification by instruction flow).

## 5. Open questions

- Encoding: the `DMA_TENSOR` mode (`DMA_TENSOR_MODE.md`, opcode 0x16 with COPY as op 0) and modes 6 / 7 of ELEM_WISE are two
  encodings of the same idea; one of the two should be kept.
- Hardware: a strided or 4-D mode of `redma_top` (descriptor registers beyond 0x3C) would be a new RTL feature; its
  register map and cost per row change are not decided.
