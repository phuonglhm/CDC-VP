# Ethernet on VP_FX1_V3.0: MAC, DMA and PHY

`fx1_soc` contains a model of the FTEL Ethernet MAC (MAS v1.0.7) with its
descriptor DMA, an RMII PHY on MDIO, and a scripted station on the line:

```
firmware ──CSR──► Ethernet MAC ──RMII──► PHY (MDIO address 1) ──line──► --eth-peer
                     │                                                 loopback | partner | none
   DDR ◄── ETH_DMA ◄─┘ (rings and buffers)        PLIC source 6 ◄── MAC interrupt (level)
```

The MAC is always present; nothing on the command line is needed to get it.
The model is functional and loosely timed: it validates register semantics,
descriptor ownership, interrupts and data movement, not cycle timing, RMII
signalling or throughput.

## Map

| Block | Base | IRQ | Header |
|---|---|---|---|
| Ethernet MAC (CSR) | `FX1_ETH_CSR_BASE` 0x1103_0000 | `FX1_IRQ_ETH` 6, level | `drivers/eth/eth_regs.h` |
| PHY | MDIO address 1 (`FX1_ETH_PHY`) | — | Clause 22 registers, ID 0x0007 / 0xC0F1 |
| DMA | bus master `ETH_DMA`, writes DDR through the exclusive-monitor write guard | — | — |

Only aligned 32-bit CSR accesses are accepted. A byte, halfword or misaligned
access faults (load mcause 5, store mcause 7), as does an access to a hole in
the register map. Descriptors and buffers are addressed by **IP offset**: the
CPU address minus `FX1_DDR_BASE` (`fx1_eth_offset()` / `fx1_eth_cpu()`); an
offset beyond the 512 MiB DDR aperture answers DECERR.

## Software

| File | What it is |
|---|---|
| `drivers/eth/eth_regs.h` | register map (generated from the CSR workbook) |
| `drivers/eth/eth_drv.[ch]` | portable driver (C99, HAL based, no OS): cold init (MAS 9.1), rings, send/receive, reclaim, stop/quiesce, per-direction recovery (MAS 9.3.3), MDIO |
| `drivers/eth/fx1_eth.[ch]` | **FX1 manager**: the driver on this SoC, its MMIO HAL, DMA memory in the linker section `.eth_dma`, the lifecycle and the interrupt split. Start here |
| `app/cmd_eth.c` | the FreeRTOS owner (service task, ISR, mutex) and the CLI's `eth` commands |
| `eth/eth_wiring.c` | bare-metal example (`make eth`): CSR and MDIO, DMA through DDR3, interrupts, error and recovery, LR/SC against the DMA, stop/restart |

The firmware's hart 1 boot check also covers the MAC: identity, BUILDCFG, a
hash register, byte and hole faults, the PHY ID over MDIO.

## Quick start

```bash
make -C sw/bootloader/sources/VP_FX1_V3.0 -j"$(nproc)" all eth
sw/bootloader/test/VP_FX1_V3.0/run_vp.sh eth          # bare-metal example, loopback, partner and none
sw/bootloader/test/VP_FX1_V3.0/run_vp.sh eth-cli      # FreeRTOS CLI, scripted eth session (cli_eth.txt)
sw/bootloader/test/VP_FX1_V3.0/run_vp.sh eth-load     # FreeRTOS CLI, Ethernet traffic with SYS_DMA and the ISP
```

Interactively (`run_vp.sh interactive 5000`, then `fx1_console.sh 5000`):
`eth probe`, `eth start`, `eth send 10 1514`, `eth stats`, `eth status`,
`eth stop`. `help` prints the usage. The line is chosen with `--eth-peer`
(see below); `run_vp.sh run --eth-peer partner --timeout-ms 3000` passes it
through.

## The manager (`drivers/eth/fx1_eth.h`)

```
fx1_eth_setup ─► fx1_eth_probe ─► fx1_eth_start ─► RUNNING ─► fx1_eth_stop ─► STOPPED ─► fx1_eth_start ...
                                                   │   ▲                         
                       halting DMA error ──► halted direction ── fx1_eth_recover
                       stop not quiescent ─► FAULTED ─────────── fx1_eth_recover (retry until it succeeds)
```

- **States:** UNINIT, STOPPED, RUNNING, STOPPING (transient), FAULTED.
- **`fx1_eth_setup(e)`** fills `e->cfg` with the defaults: board MAC
  `02:00:46:58:31:01`, 16-descriptor rings, 2048-byte buffer stride, RX buffer
  offset 2 (IP header aligned), interrupt coalescing off (`coal = 0`),
  broadcast accepted, 100 Mbps (`e->speed100`). Change `e->cfg` before the
  first `fx1_eth_start`. `e->rx_cb` receives each frame (DA..payload, no FCS)
  in the owner's context.
- **One owner.** Every call except `fx1_eth_isr()` must come from one context
  at a time. On FreeRTOS that is a service task plus a mutex that every other
  caller takes (`app/cmd_eth.c`).
- **Interrupt split (MAS 9.3).** `fx1_eth_isr()` never waits: it takes and
  acknowledges `ETH_INT_STATUS` and masks the level error sources. The owner
  then calls `fx1_eth_service()`, which handles errors, delivers received
  frames, reclaims transmit descriptors and returns **after how many
  milliseconds it must run again even without an interrupt** (`0` only when
  not RUNNING). Honour that value: it is how write-backs that commit after
  DONE and halts without an interrupt are found (below).
- **Return codes:** `ETH_OK`, `ETH_E_STATE` (wrong state, or the direction is
  halted), `ETH_E_FULL` (transmit ring full: service, then retry),
  `ETH_E_TIMEOUT`, `ETH_E_LEN`, `ETH_E_MDIO` (no PHY), `ETH_E_BUILDCFG` (not
  the expected MAC), `ETH_E_CFG` (configuration refused, nothing written).

The FreeRTOS owner, as `app/cmd_eth.c` does it:

```c
static BaseType_t eth_irq(void)                     /* bsp_irq_register(FX1_IRQ_ETH, eth_irq) */
{
    BaseType_t woken = pdFALSE;
    fx1_eth_isr(&eth);
    vTaskNotifyGiveFromISR(eth_task, &woken);
    return woken;
}

static void eth_service_task(void* arg)             /* priority 3 */
{
    TickType_t wait = portMAX_DELAY;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, wait);
        xSemaphoreTake(eth_lock, portMAX_DELAY);
        const int again_ms = fx1_eth_service(&eth);
        xSemaphoreGive(eth_lock);
        wait = again_ms > 0 ? (pdMS_TO_TICKS(again_ms) ? pdMS_TO_TICKS(again_ms) : 1) : portMAX_DELAY;
    }
}
```

A caller that sends takes `eth_lock`, calls `fx1_eth_send()`, gives the lock
and notifies the task, so frames completed meanwhile are picked up.
`fx1_eth_hal_set_yield()` lets the driver's waits (stop, recover, MDIO) give
the CPU away about once per millisecond.

## DMA memory and ownership

- Rings and buffers live in `.eth_dma` (NOLOAD, 4 KiB aligned): nothing else
  of the image shares a 64-byte reservation granule with them. NOLOAD is not
  zeroed: `fx1_eth_start` builds the rings every time.
- A descriptor with OWN = 1 belongs to the DMA. The manager never reclaims or
  rewrites it, even when its DONE interrupt has come (next section).
- Hand buffers over with `fx1_dma_prepare()` / `fx1_dma_complete()`. They are
  no-ops on the VP (no caches); silicon will need them.
- Ethernet DMA writes cancel LR reservations on their granule and wait for an
  open AMO or SC on it, as SYS_DMA's do: atomics on memory the MAC writes are
  safe. Still, do not write a descriptor the DMA owns.

## DONE before the descriptor write-back (MAS 9.3)

The interrupt can come before the DES0 write-back (OWN = 0) is visible. The
manager treats a descriptor still owned after DONE as in flight: it polls it
every `FX1_ETH_POLL_FAST_MS` (1 ms), then every `FX1_ETH_POLL_SLOW_MS` (10 ms)
past a 2 ms deadline (counted once per episode in `wb_timeouts`), for as long
as it stays owned, also across ring wraps. With nothing in flight the service
still runs every `FX1_ETH_POLL_IDLE_MS` (100 ms) while RUNNING.

## Errors, stop and recovery

- **Classification by ERRINFO class** (MAS Table 6-32), at the moment ERR is
  seen: class 4 (TX_TOO_LONG, RX_TRUNC) is a soft error; the frame is dropped
  or truncated and the direction keeps running. Every other class (descriptor
  read/write, data read/write with SLVERR or DECERR, zero length, no first
  segment, ...) halts that direction: `fx1_eth_send` refuses at once
  (`ETH_E_STATE`), before HALTED is visible.
- **A halt behind a soft error.** ERRINFO latches only while ERR is 0. A
  halting error that comes while a class-4 error still holds ERR shows only
  as HALTED, with no new interrupt. The manager watches a direction after a
  class-4 clear, checks HALTED on every service pass and on every send.
- **`fx1_eth_recover(e)`**, for a halted direction: waits up to 2 ms for
  HALTED, then a receive or transmit soft reset and that ring rebuilt; the
  other direction keeps running. Frames still in the RX FIFO are delivered
  after the recovery (MAS Table 6-2 row 15). If HALTED does not come, the
  device goes FAULTED. `e->auto_recover = 1` makes the service recover by
  itself; the delivered firmware leaves it 0 (`eth recover` by hand).
- **`fx1_eth_stop(e, drain_us)`**: lets queued frames go out for up to
  `drain_us`, then halts both directions (a second HALT_REQ if needed), clears
  the MAC enables only after HALTED, waits for both MACs to be idle, flushes
  the FIFOs and masks the interrupts. `drain_us = 0` skips only the drain:
  queued frames are dropped, but the call still waits for quiescence before it
  reports STOPPED. `e->last_stop` tells what was dropped or forced, and
  `tx_fifo_flushed` that frames already written back may have been lost in the
  TX FIFO (how many is not observable).
- **FAULTED**: a stop or recovery that could not reach quiescence. The DMA may
  still own memory: no restart, no buffer reuse. `fx1_eth_recover` retries the
  quiesce; once both directions are HALTED (every write of the old session has
  landed) it restarts. Otherwise it returns `ETH_E_TIMEOUT` and changes
  nothing; call it again later. There is no global reset in this path (MAS 9.1
  requires both directions HALTED first).
- **Accounting.** A frame submitted between a halting error and HALTED can be
  accepted. Still queued at the recovery, it is counted in `tx_dropped`; one
  the DMA completed or failed is counted by its completion or descriptor
  error; one in the TX FIFO may be flushed.

## FreeRTOS CLI (`eth`)

| Command | Does |
|---|---|
| `eth probe` | ID, CAP, BUILDCFG, PHY ID/BMCR/BMSR, link and speed |
| `eth status` | state, speed, enables, interrupt registers, ring indices and CUR_DESC, halted direction and last error snapshot, last stop |
| `eth stats [clear]` | RMON (reads do not clear), drops, FIFO state, driver counters; `clear` zeroes them |
| `eth start`, `eth stop [DRAIN_US]` | start (cold or restart); stop with a drain of DRAIN_US (default 20000) |
| `eth recover` | recover a halted direction, or leave FAULTED once quiescent |
| `eth selftest` | PHY loopback at 100 and 10 Mbps, 60..1514-byte frames, interrupt path; state restored |
| `eth mdio read P R`, `eth mdio write P R V` | Clause 22; a BMCR write is refused while running |
| `eth send N [LEN] [PATTERN]` | N frames to ourselves; prints how many came back |
| `eth dump rx\|tx` | the ring's descriptors |
| `eth cycle N` | N start/frame/stop cycles (ends stopped), heap and stack check |
| `eth load start\|stop` | background 1 KiB frames with sequence and payload check |
| `eth mdiopoll start\|stop` | a second task reading the PHY ID during the lifecycle |
| `eth wait MS` | scripts: let time pass without holding the device |

The scripts `test/VP_FX1_V3.0/cli_eth.txt` and `cli_eth_load.txt` show a
complete session.

## Simulator options

| Option | Meaning |
|---|---|
| `--eth-peer loopback` (default) | frames sent come back to the MAC |
| `--eth-peer partner` | a station that echoes each frame to its sender (DA = the original SA, SA = 02:00:00:00:00:FE), broadcasts included |
| `--eth-peer none` | no cable: link down |
| `--eth-wb-delay-us N` | fault injection: every DES0 write-back is held N µs (0..100000) after its completion is signalled, so firmware sees DONE with OWN still 1 |
| `--ddr3-ecc off --ddr3-fault T:write-result:1` | the DDR3 controller answers the next write after T µs with SLVERR, whichever master (an Ethernet DMA write if T falls in its traffic) |
| `--eth-unguarded-dma` | negative control only: the Ethernet DMA bypasses the write guard (its writes no longer respect LR/SC and AMOs). Not for normal use |

The platform report at the end of a run gives frames sent and received, the
line peer's counts and PHY drops.

## Limits

- **No host network.** The line is a loopback plug, an echo station or no
  cable. There is no TAP/host interface and no TCP/IP stack in the firmware.
- **Link fixed for a run.** `--eth-peer` does not change while running; the
  manager does not watch the link (BMSR is read by `eth probe` only).
- **DDR reset or remap while the DMA runs** is not coordinated: call
  `fx1_eth_stop` before resetting or remapping DDR, start again after.
- **Interrupt coalescing.** The FX1 configuration is `coal = 0`. Non-zero
  `COAL_CNT`/`COAL_TIMER` (timer unit 128 MAC clocks) is modelled, but with
  delayed write-backs it was accepted on a host bench only, not on FX1.
- **Timing.** Functional, loosely timed. The harts fetch every instruction
  from DDR (no cache) and the manager copies frames byte by byte: one 1 KiB
  frame costs about 1 ms of hart 0, so the service task can hold the CPU for
  several milliseconds under load. Do not measure throughput or latency on
  the VP; give other code budgets that tolerate such a preemption.
- **TX FIFO occupancy** is not observable (no level register).
- **Not a sign-off reference** for RMII electrical behaviour, the CDC block or
  AXI channel timing.
