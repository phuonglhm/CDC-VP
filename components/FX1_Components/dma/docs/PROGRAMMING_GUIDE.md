# FX1 SYS_DMA — firmware programming guide

This guide is for firmware written against the VP model. Use the C header
`include/dma/fx1_dma_regs.h`; it is checked against the model's `registers.h`
at compile time. A worked example that runs on the FX1 platform is
`fw/fx1_soc/tests/dma_m2m/main.c`.

## Platform facts (FX1 VP)

| Item | Value |
|---|---|
| CSR base | `FX1_SYS_DMA_CSR_BASE` (0x1001_0000, VP placeholder), APB, 64 KiB slot; registers occupy `0x0000–0x10ff` |
| Access | **32-bit aligned only**. Byte/halfword accesses and byte enables are refused and reach the CPU as a load (mcause 5) or store (mcause 7) access fault |
| Interrupt | One level output, PLIC source `FX1_IRQ_SYS_DMA` (2) |
| Bus master | 32-bit addresses, any mapped memory (DDR at 0x8000_0000) |
| Reset | Held in reset for 20 ns after power-up; register accesses in reset get a slave error |
| Channels | 8 channels, stride 0x100; FIFO 32 bytes per channel; up to 4 outstanding reads and 4 outstanding writes |
| Transfer size | 0..1023 bytes per command (10-bit byte count); longer copies need command chaining |
| Peripheral handshake | `rx/tx_request` inputs are tied to 0 on FX1 today. P2M/M2P can be exercised with the software request registers (see below), but no real peripheral is connected yet |

The model is functional and approximately timed. It does not reproduce
AXI/APB pin timing.

## Memory-to-memory copy (channel `n`)

```c
#define CH(n, off) (FX1_SYS_DMA_CSR_BASE + FX1_DMA_CH((n), (off)))
const uint32_t cfg = FX1_DMA_ADDRESS_INCREMENT | FX1_DMA_CFG_TOKENS(1) | FX1_DMA_CFG_BURST_BYTES(32);

fx1_write32(CH(n, FX1_DMA_CH_ENABLE), 0);                         /* configure while disabled */
fx1_write32(CH(n, FX1_DMA_CH_INTERRUPT_CLEAR), FX1_DMA_INTERRUPT_MASK);
fx1_write32(CH(n, FX1_DMA_CH_CMD_READ_ADDR), src);
fx1_write32(CH(n, FX1_DMA_CH_CMD_WRITE_ADDR), dst);
fx1_write32(CH(n, FX1_DMA_CH_CMD_TRANSFER_SIZE), bytes);          /* 0..1023 */
fx1_write32(CH(n, FX1_DMA_CH_CMD_CONTROL), FX1_DMA_CMD_SET_INT | FX1_DMA_CMD_LAST);
fx1_write32(CH(n, FX1_DMA_CH_READ_CONFIG), cfg);
fx1_write32(CH(n, FX1_DMA_CH_WRITE_CONFIG), cfg);
fx1_write32(CH(n, FX1_DMA_CH_PERIPHERAL_CONFIG), 0);              /* memory on both sides */
fx1_write32(CH(n, FX1_DMA_CH_MODE_CONFIG), FX1_DMA_MODE_SWAP_NONE);
fx1_write32(CH(n, FX1_DMA_CH_ENABLE), 1);
fx1_mb();                                                         /* source data before the start */
fx1_write32(CH(n, FX1_DMA_CH_START), 1);                          /* or CORE_CHANNEL_START bit n */
```

Notes:
- Source and destination may have any byte alignment. The DMA picks 1/2/4-byte
  beats from the alignment and the burst size, and it never crosses a 4 KiB
  boundary within a burst.
- `READ_CONFIG`/`WRITE_CONFIG` fields: bit 31 increment (0 = FIXED address, for
  peripheral FIFOs), bit 30 outstanding enable, `[27:24]` maximum outstanding
  (clamped 1..4), `[21:16]` arbitration tokens, `[6:0]` maximum bytes per burst
  (1..127). A burst size of 0 is rejected (DECERR) when the size is non-zero.
- **Configuration writes while a channel is active or has outstanding
  transactions are ignored silently.** Program a channel only while it is idle
  (`CH_ACTIVE_STATUS == 0`, `CH_OUTSTANDING_STATUS == 0`). Control and interrupt
  registers stay usable at any time.
- Writing START to an active channel is ignored.
- `CH_ENABLE = 0` stops new grants; accepted transactions drain. Re-enabling
  continues the unfinished command.

## Completion: interrupt or polling

Interrupt flow (as in `dma_m2m`):

1. At init, set the PLIC priority of source 2, enable it for the hart's context,
   and set `MEIE` and `MIE`. `CH_INTERRUPT_ENABLE` resets to all events
   (`FX1_DMA_INTERRUPT_MASK`).
2. In the external-interrupt handler, `id = claim`. If `id == FX1_IRQ_SYS_DMA`:
   - read `FX1_DMA_CORE_STATUS` (bit n = channel n has an enabled event);
   - for each such channel, read `CH_INTERRUPT_STATUS` and write the bits back to
     `CH_INTERRUPT_CLEAR` (write 1 to clear);
   - then `complete(id)`.

   The DMA output is a level: clear at the device **before** completing at the
   PLIC, or the source becomes pending again immediately.

`COMMAND_COMPLETE` events are counted: one W1C of bit 0 acknowledges one
completed command (up to 15 queued). The bit stays set while more completions
are queued, which happens with chained commands.

Polling: wait for `CH_ACTIVE_STATUS == 0 && CH_OUTSTANDING_STATUS == 0`, then
read `CH_TRANSFER_COUNT`. Bits `[11:0]` count completed commands, bits `[31:16]`
count unacknowledged completion events. `CORE_IDLE_STATUS == 1` means every
channel is idle.

## Errors

| Status bit | Cause |
|---|---|
| `READ_DECERR` / `WRITE_DECERR` | Unmapped address, unsupported access, invalid channel configuration (reserved swap, P2P, zero burst size) |
| `READ_SLVERR` / `WRITE_SLVERR` | The target answered with a slave error |
| `FIFO_OVERFLOW` | Internal FIFO accounting error |
| `*_TIMEOUT`, `WATCHDOG_TIMEOUT` | Only when the platform enables the optional timeouts; off on FX1 today |

A failed command does not count as a completion and leaves the channel inactive
once its outstanding transactions drain. Clear the status, fix the
configuration and start again. `dma_m2m` step 3 shows an unmapped destination
raising `WRITE_DECERR`.

## Command chaining

If `CMD_CONTROL` does not have `FX1_DMA_CMD_LAST` set, bits `[31:2]` give the
16-byte-aligned address of the next descriptor: four little-endian words
`{source, destination, byte count, control}`, fetched by the DMA. Set
`FX1_DMA_CMD_SET_INT` on each command that should raise a completion event.
Covered by the model unit test `fx1_dma_chain` (`tests/test_dma.cpp`,
`chain_test`); there is no platform example yet.

## Peripheral transfers (M2P / P2M)

- `CH_PERIPHERAL_CONFIG`:
  - read side: peripheral ID in `[4:0]`, post-service delay in `[10:8]`;
  - write side: ID in `[20:16]`, delay in `[26:24]`.
  - ID 0 is memory. Use `FX1_DMA_CFG...` with address increment off (FIXED) on
    the peripheral side.
- A service needs a request for that peripheral ID:
  - Hardware: `rx_request` or `tx_request` bit `id`, held until the DMA pulses
    the matching `*_clear` bit. Not connected on FX1 yet.
  - Software: write bit `id` of `PERIPHERAL_RX_REQUEST` or
    `PERIPHERAL_TX_REQUEST`. The bit self-clears when the service ends.
- While a channel waits for a request, the DMA scheduler sleeps; the wait costs
  no simulation time.

Covered by `fx1_dma_peripheral` and `fx1_dma_idle` (model unit tests). The FX1
platform has no DMA-capable peripheral yet, so there is no platform example.

## Byte swapping

`CH_MODE_CONFIG` `[29:28]`: `FX1_DMA_MODE_SWAP_16` or `FX1_DMA_MODE_SWAP_32`
reverses each 2- or 4-byte group on the way through the FIFO. The transfer size
must be a multiple of the group; encoding 3 is reserved (DECERR).

## Caches and ordering

The FX1 VP does not model caches: the CPU, SYS_DMA and the ISP DMAs see the
same RAM. Firmware for silicon must still:
- clean source buffers before starting the DMA;
- invalidate destination buffers before reading results;
- keep buffer ownership clear (CPU or DMA, never both).

On the VP, keep at least `fx1_mb()` between writing the source data and the
START write, and `fx1_acquire()` after observing completion. Correct cache
maintenance cannot be verified on this VP.
