# SPI requirement traceability

Page numbers refer to SPI m_spi_top HDS V1.0. Decisions about the TLM boundary
are separated from source requirements in [MODEL_CONTRACT.md](MODEL_CONTRACT.md).

| Requirement | Source | Verification |
|---|---|---|
| Register defaults, RO/WO, reserved decode | pp.31-39 | behavior: defaults, zero/ignored reserved offsets, status and FIFO vacancy |
| Configuration strobes, TXDR exception | pp.25, 35, 43 | behavior: masked configuration writes, zero-strobe TX pushes, W1C lane qualification |
| Frame format and all four modes | pp.19-21 | behavior: all 32 FSZ encodings x four modes x two bit orders, two master frames each; external slave-mode bit stimulus |
| Depth 16/32/64 | p.15 | same behavior suite at each supported depth |
| Master backpressure and CS hold | pp.22-23 | behavior: full RX, retained queued words, drain and ordered recovery |
| Nonzero IFD preserves queued TX data | pp.22-23 | behavior: all four modes, GAP levels/vacancy, normal resume, flush discards waiting frame, abort preserves unstarted frame |
| Manual select and TXONLY | pp.24-25, 33-35 | behavior: multi-select, software deselect, no RX storage |
| Abort without EOT | pp.23, 34, 43 | behavior: disable during long-divider transfer; MSTR clear with CPOL high and either CPHA drives SCK zero and removes master output enables |
| Slave peek/pop, underrun and overrun | pp.23-25 | behavior: SS preload without pop, late TX, zero response, full RX and partial-frame discard |
| Sticky and threshold interrupt | pp.26, 37-38 | behavior: errors, W1C, TX threshold 127 with EN clear, RX threshold zero with empty/nonempty FIFO; integration: four IRQ routes and clearing one OR contributor |
| DMA four-phase handshake | pp.27-28, 42 | behavior: held request despite level change, ACK/WAIT/rearm, disable; integration: actual fixed-address bus bursts |
| Flush completion | pp.25-26, 33 | behavior: TX drain/self-clear and RX completion |
| Separate reset domains | pp.16-17 | behavior: APB reset retention, in-flight response cancellation with exactly one TX push, subsequent access recovery, system defaults; integration: all four controllers reset |
| Debug reads pop RX | p.35 | behavior: debug read removes a frame and raises underflow when empty |
| SoC map and routing | FU3 HAS memory/IRQ tables | integration: independent golden bases and loopback through both PERIBUS paths |

## Added corner coverage (2026-10-20)

All scenarios use public MMIO and pins. `corners.cpp` builds independently of the
bus library and is also run against installed static/shared packages.

| Requirement | Source | Added verification |
|---|---|---|
| Slave frame widths and independent bit observation | pp.19-24,43 | `slave_formats`: all 32 FSZ encodings, four modes, both bit orders, two distinct frames under one SS; depths 16/32/64; independent MOSI source and MISO observer |
| Flush ownership and priority | pp.25,33,43 | `fifo_flush`: concurrent TX writes, post-completion retention, same-tick hardware-clear priority, acknowledgement-return rewrite, RX frame arrival while draining, ordered tail after cancelling RXFLUSH; depths 16/32/64 |
| Sticky hardware event wins over W1C | pp.26,43 | `event_priority`: UDR on first slave edge, EOT on deselect and OVR on final frame edge collide with W1C; later W1C succeeds and prior RX data survives |
| Abort and system reset across master states | pp.16,23,43 | `abort_states`: EN/MSTR abort and quiescent-APB system reset during pin-observed IDLE/SETUP/SHIFT/GAP/HOLD/CS_IDLE; no new EOT, preserved completed RX on abort, full reset profile on reset |
| DMA threshold and return-to-zero behavior | pp.27-28,39,43 | `dma_boundaries`: TX/RX thresholds 0/1/DEPTH, requests held across condition changes, ACK-held WAIT, rearm, and cancellation with DMACR/EN |
| Reset recovery under different functional clock ratios | pp.16-17,43 | `reset_phases`: APB periods 1/10/13/80 ns against SYS=10 ns, three reset pulses after operation commit, exactly one side effect, serial progress through APB-only reset, reset/recovery of a partial slave frame |
| Extreme divider and delay programming | pp.19-22,40,43 | `master_delays`: DIV 0/1/2/65535 and DLY fields 0/1/255 in selected profiles; independently sampled MOSI, ordered RX, no premature EOT, setup/hold/inter-frame/minimum-idle functional intervals |
| Concurrent masters and independent real IP progress | FU3 bus contract and HAS topology | `integration`: IBEX and DMA stand-in write the same SPI0 FIFO concurrently; eight frames appear exactly once in per-origin order, while SPI2 transfers on the other peripheral bridge |

The HDS verification checklist also includes physical CDC, metastability,
electrical timing and exhaustive reset/event/interleaving sweeps. Added corner
tests cover selected functional boundaries without establishing exhaustive
coverage or exact RTL timing. See
[VALIDATION.md](VALIDATION.md) for actual execution evidence and limitations.
