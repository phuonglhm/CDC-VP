# DMA tensor mode: concat by DMA COPY in the instruction path

This document describes the `DMA_TENSOR` instruction of the model (opcode `0x16`, op COPY): what it does, how it maps onto the DMA
of the RTL, how the model executes it and what it costs, how the frontend uses it for the concats of YOLOv8m, and what is still to be
decided with the hardware team. It is a **proposal** of the model team: the RTL has the DMA that can do the copy, but no block that
decodes such an instruction.

Labels: MEASURED (run in the model), DERIVED (computed from measured numbers), READ (read in the RTL or the code), PROPOSAL.

Slice and concat in general (compiler alias, AGU_ND on the vector unit, MOVE) are compared in
[SLICE_CONCAT_ON_GVU.md](SLICE_CONCAT_ON_GVU.md), section 2; this mode is option (D) there.

## 1. Why

Both networks need only slices and concats on the outermost axis of their DRAM layout ([C][H][W] for YOLOv8m, [C][L] for ViT-B/16),
so every piece is one contiguous block. The default programs place the producers of a concat directly in the cells of the output
(compiler alias, 0 cycles). The company asked for slice and concat as data movement inside the instruction path instead. The model
therefore copies each concat input into its cell with the DMA, in order with the other instructions; slices stay views at an offset.

## 2. Three layers

| Layer | What exists | Status |
|---|---|---|
| RTL DMA (`redma`, READ) | One copy per start: reader start address, writer start address, byte count, then start of reader and writer; any byte alignment; AXI4 128 bit (16 bytes per beat); a demultiplexer sends addresses outside the SAURIA SRAM window to memory, so DRAM to DRAM works. Its configuration window is 64 bytes. No block decodes an instruction for it | in the RTL |
| A wider DMA descriptor (4-D shapes, transpose, slice) | proposed by the vector-unit owner; it needs a wider configuration window and new registers in the RTL | proposal, not modelled here |
| This mode | one COPY of one contiguous block, mapped one to one onto the RTL registers above | PROPOSAL, modelled behind `HAS_DMA_TENSOR` |

## 3. The instruction

No new register address: the instruction reuses registers of the delivery interface (`INTERFACE_SPEC.md`, section 6).

| Register (address) | Meaning for `0x16` | RTL DMA register |
|---|---|---|
| `A_ADDR` (`0x40000444`) | source address in DRAM | reader start address (`0x10`) |
| `OUT_ADDR` (`0x40000408`) | destination address in DRAM | writer start address (`0x20`) |
| `LEN` (`0x40000450`) | number of bytes | byte count (`0x30`) |
| `MODE_PACK` (`0x40000454`) | op: 0 = COPY; any other value is refused (`UNSUPPORTED_MODE`) | control (`0x00`) = reader start + writer start |
| `PUSH_A` (`0x40000310`) = `0x16` | push | — |

- The four registers keep their values between pushes, so a program writes all four before each push.
- **Option `HAS_DMA_TENSOR`** (environment, default 1). With 0 the push of `0x16` is refused with `UNSUPPORTED_OP`, as before this mode.
- **Checks at the push** (error `BAD_GEOM`, instruction dropped): `LEN` = 0; source or destination plus `LEN` beyond 2^32; source and
  destination ranges that overlap.
- **Check at execution**: a source or destination that reaches the staging regions of the data flow controller (beyond the program
  image) prints a `WARNING`, counts a framing error and copies nothing.
- Through a system wrapper with the model root `core_rtl/`, a push of `0x16` reaches `HasNpuTop` unless `HAS_DMA_TENSOR=0` is set
  for the wrapper process.

## 4. Execution and cycles

The data flow controller executes the COPY in order: the next instruction starts when the copy is complete, so a read after the copy
sees the copied data. It waits until no other DMA transfer is active, then copies in chunks of `C` bytes (`HAS_DMA_TENSOR_CHUNK`,
default 1,024, range 16 to 98,304) through the DMA's own SRAM bank 4 used as two halves: chunk k + 1 is read while chunk k is written.
The core SRAM is not touched. The instruction retires when both the read port and the write port are done.

Cycles from the pop to the retire (MEASURED equal, cycle for cycle, on every COPY of the check program and of the YOLOv8m windows):

    T(N) = sum over k = 0..n of max(R_k, W_(k-1)) + (n + 1)
    R_k = W_k = L + 8 * ceil(c_k / (8 * B)),   R_n = W_(-1) = 0

with n = ceil(N / C), c_k = min(C, N - k * C), `B` the bytes per beat of the run profile (16 in the default profile: 128 bytes per burst
of 8 beats) and `L` the DRAM latency. Examples in the default profile with C = 1,024 and L = 0: 1 to 128 bytes take 18 cycles,
1,024 bytes 130, 1,025 bytes 139, 151,296 bytes 9,669, 200,000 bytes 12,765. For large N, T is about N / 16 x 65 / 64 + 65 cycles.

## 5. Frontend

`tools/fe/fe_emit_insts.py --slice-concat dma` (default `alias`) writes the YOLOv8m program to `$FE_WORK/has/insts_<plan>_dma/`:

- each concat input and the concat output get regions of their own; one COPY per run of inputs that are adjacent in memory, placed
  after the last input is written; slices stay views (no COPY);
- the frontend checks that every input is written before its COPY, that the regions are inside the image and do not overlap, that
  the golden bytes of source and destination are equal, and that the image stays below `--dram-limit` (default 256 MiB, an ESTIMATE of
  the target DRAM);
- it never binds an AGU_ND destination into a concat cell: no other instruction of the program writes into a concat output;
- `manifest.json` gains the keys `slice_concat` and `dma_tensor` (count, bytes, list of copies, `requires: HAS_DMA_TENSOR=1`); the
  alias program is unchanged byte for byte.

| YOLOv8m | Alias (default) | COPY program |
|---|---:|---:|
| Instructions | 98 | 135 (98 + 37 COPY for 13 concats) |
| Bytes copied | 0 | 23,270,400 |
| DRAM image | 85,779,392 bytes | 109,049,792 bytes |
| Cycles | 62,422,935 (MEASURED, default configuration) | 63,902,431 (MEASURED; predicted 63,902,467 from the formula, the difference of 36 cycles is the poll step of the bench) |

ViT-B/16 needs no COPY program: the heads read and write 64-channel blocks of channel-major tensors, so every slice and concat of the
network is an address on the outermost axis.

## 6. Checks

| Check | Result |
|---|---|
| `tools/has/tb_has_dma_tensor_isa.cpp` (encoding and refusals, including `MODE_PACK` = `0x01000000`) | PASS (MEASURED) |
| `tools/has/make_dma_tensor_gate.py` check program: 25 instructions, 19 COPY (concat read by a GEMM, COPY of an ADD output read by a max pool, COPY of a COPY, COPY over a later input, 13 sizes from 1 to 200,000 bytes) | 25 / 25 on the model and MATCH 25 / 25 on `tools/has/ref_program.py` in the default configuration, with the vector-unit knobs at 0, with the legacy profile, with chunks of 128 and 4,096 bytes and with a DRAM latency of 20; 16 / 16 regions equal to a third reference built from the input data (MEASURED) |
| Option off | the 19 COPY instructions refused (MEASURED) |
| Earlier check programs with the option off and on | logs (cycles included) and memory images identical (MEASURED) |
| YOLOv8m COPY program, 13 windows around the copies | 13 / 13 PASS, `ref_program.py` MATCH 13 / 13, 37 / 37 COPY with the cycles of section 4 (MEASURED) |
| Whole YOLOv8m with the COPY program (`HAS_DMA_TENSOR=1`, the vector-unit options on) | PASS 135 / 135, 0 elements different, 63,902,431 cycles (predicted 63,902,467: the default 62,422,935 plus 1,479,496 cycles of the COPY instructions against 1,479,532 by the formula; the 36 cycles are the poll step of the bench), core busy cycles equal to the default (55,653,442); 52,017,408 epilogue lanes, 6,374,400 residual-add lanes and 345,600 max pool lanes on `GvuTop`, 0 different, no stall needed (MEASURED) |

The items are part of `tools/has/regress_gvu.sh`; the summary is in `VERIFICATION_REPORT.md`, V42.

## 7. Questions for the hardware team

1. Who issues the COPY in the RTL: the data flow controller, extended with one instruction type that writes the four DMA registers
   and waits for the reader and writer interrupts (its DMA sequence already writes these registers), or the firmware through the
   configuration port of the DMA?
2. Is an overlapping source and destination defined for the DMA? The model refuses it at the push.
3. When is the DMA port towards the vector unit connected? It is needed for any copy into the Scratchpad (not used by this mode).

## 8. How to run

```bash
python3 tools/fe/fe_emit_insts.py --plan pe3 --slice-concat dma
HAS_DMA_TENSOR=1 tools/has/tb_has_npu_top $FE_WORK/has/insts_pe3_dma
python3 tools/has/make_dma_tensor_gate.py <dir>
python3 tools/has/ref_program.py <dir> --image <dir>/dram_init.bin --write-ref <dir>/dram_golden.bin
HAS_DMA_TENSOR=1 FE_SNAPSHOT_DIR=<snap> tools/has/tb_has_npu_top <dir>
python3 tools/has/ref_program.py <dir> --image <snap>/dram_snapshot.bin
```

The first `ref_program.py` call writes the reference image (it exits with 1 because it also compares with the input image); the
second compares the model's final image with the reference.
