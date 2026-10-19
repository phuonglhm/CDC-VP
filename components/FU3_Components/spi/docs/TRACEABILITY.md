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

The HDS verification checklist also includes physical CDC, metastability,
electrical timing, detailed flush races, hardware/software simultaneous events,
extreme delay fields and exhaustive reset/transfer phase sweeps. Initial functional
tests do not establish exhaustive coverage of that checklist. See
[VALIDATION.md](VALIDATION.md) for actual execution evidence and limitations.
