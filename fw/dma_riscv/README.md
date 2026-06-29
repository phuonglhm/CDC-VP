# DMA Firmware Test

Bare-metal RISC-V firmware that verifies the DMA TLM peripheral inside
the Bremen virtual platform, by driving real memory-to-memory transfers
through the debug launch interface.

## Test Path
RISC-V firmware

-> writes DMA program bytes into RAM

-> writes DBGINST0/DBGINST1/DBGCMD to launch a channel

-> DMA fetches and executes the program itself (bus master)

-> DMA reads/writes RAM directly, raises events, reports faults

  -> firmware polls CSR/FTR registers and verifies behavior

## Addressing Note

The DMA's `master_socket` is routed through the same `bus_router` as the
CPU, so SAR/DAR/CPC values use the same `0x80000000`-based addresses as
CPU code. Buffers are placed at `0x80080000+` (512 KiB into RAM) to avoid
overlapping firmware code/stack, which live at the start of RAM.

## DBGINST0 Encoding Note

For manager instructions launched via the debug interface (e.g. `DMAGO`),
the channel number is encoded in **byte 1 of the instruction**, i.e. bits
`[31:24]` of `DBGINST0` — not bits `[10:8]`, which only select the channel
for *channel-thread* debug instructions (`DMAKILL`, `DMANOP`, `DMASEV`
issued directly to a channel). Mixing these up silently launches the wrong
channel; the DMA model does not warn about it.

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

1. **Buffer setup** — writes a 32-byte source pattern, clears a 32-byte
   destination buffer.
2. **Program build** — writes a DMA channel program into RAM:
   `DMAMOV CCR/SAR/DAR`, two `DMALD`/`DMAST` bursts (16 bytes each),
   `DMASEV` event 3, `DMAEND`.
3. **Debug launch** — starts the program on channel 0 via `DBGINST0`,
   `DBGINST1`, `DBGCMD`.
4. **Completion wait** — polls `CSR0` until the channel returns to
   `STATUS_STOPPED`.
5. **Result verification** — checks destination bytes match source,
   `SAR0`/`DAR0` advanced correctly, `FSRC` is clean, and the completion
   interrupt fired and cleared correctly via `INTCLR`.
6. **DMAKILL** — launches a channel running an infinite loop, confirms it
   is `STATUS_EXECUTING`, sends a channel-thread `DMAKILL` debug
   instruction, and confirms the channel returns to `STATUS_STOPPED`
   instead of running forever.
7. **Fault on undefined opcode** — launches channel 1 with a single
   invalid opcode (`0xFF`) as its "program," and confirms the channel
   enters `STATUS_FAULTING` with the `FTR_UNDEF_INSTR` bit set in its
   fault register (`FTR1`), proving the model rejects bad instructions
   instead of silently misbehaving.

## Expected Output
DMA platform start

[1] Prepare source/destination buffers

PASS src[0]==0x40

PASS dst[0]==0

[2] Build DMA channel program in RAM

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

[6] DMAKILL stops a running channel

PASS channel running before kill

PASS channel stopped after DMAKILL

[7] Undefined opcode triggers a fault on channel 1

PASS channel 1 faulting

PASS FTR1 has UNDEF_INSTR bit

DMA PASS