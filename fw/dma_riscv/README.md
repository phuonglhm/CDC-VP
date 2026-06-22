# DMA-330 Firmware Test

Bare-metal RISC-V firmware that verifies the DMA-330 TLM peripheral inside
the Bremen virtual platform, by driving a real memory-to-memory transfer
through the debug launch interface.

## Test Path
RISC-V firmware

-> writes DMA-330 program bytes into RAM

-> writes DBGINST0/DBGINST1/DBGCMD to launch channel 0

-> DMA fetches and executes the program itself (bus master)

-> DMA reads source bytes from RAM, writes them to destination in RAM

-> DMA raises event 3 -> INTEN/INT_EVENT_RIS/INTMIS -> irq

-> firmware polls CSR0 for completion, verifies data and registers

## Addressing Note

The DMA's `master_socket` is routed through the same `bus_router` as the
CPU, so SAR/DAR/CPC values use the same `0x80000000`-based addresses as
CPU code. Buffers are placed at `0x80080000+` (512 KiB into RAM) to avoid
overlapping firmware code/stack, which live at the start of RAM.

## Build

```bash
make -C fw/dma_riscv
```

Produces `fw/dma_riscv/dma_test.elf`.

## Run

```bash
./build/bremen/platforms/tests/dma_platform/dma_platform \
  -c platforms/tests/dma_platform/configs/default.yaml \
  --fw fw/dma_riscv/dma_test.elf \
  --sim-ms 10
```

## What the Test Does

1. Writes a 32-byte source pattern and clears a 32-byte destination buffer
2. Builds a DMA-330 channel program in RAM: `DMAMOV CCR/SAR/DAR`, two
   `DMALD`/`DMAST` bursts (16 bytes each), `DMASEV` event 3, `DMAEND`
3. Launches the program on channel 0 via the debug interface
   (`DBGINST0`, `DBGINST1`, `DBGCMD`)
4. Polls `CSR0` until the channel returns to `STATUS_STOPPED`
5. Verifies destination bytes match source, `SAR0`/`DAR0` advanced
   correctly, `FSRC` is clean, and the completion interrupt fired and
   cleared correctly

## Expected Output
DMA platform start

[1] Prepare source/destination buffers

PASS src[0]==0x40

PASS dst[0]==0

[2] Build DMA-330 channel program in RAM

[3] Launch channel 0 via DBGINST/DBGCMD

PASS INTEN

[4] Wait for channel completion

PASS CSR0 stopped after DMAEND

[5] Verify transfer results

PASS all 32 bytes match

PASS SAR0 final

PASS DAR0 final

PASS FSRC clean

PASS INT_EVENT_RIS done bit

PASS INTMIS clear after INTCLR

DMA PASS