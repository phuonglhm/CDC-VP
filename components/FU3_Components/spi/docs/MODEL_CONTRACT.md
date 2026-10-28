# SPI functional model contract

## Common Spec baseline (2026-10-21)

The supplied FU3 Common Spec v2.0 workbook is the highest authority, followed
by FU3 HAS v2.0 and the latest IP detail where higher-level documents are silent.
See the bus [rebaseline decisions](../../bus/docs/COMMON_REBASELINE.md) for exact
cross-IP choices, source conflicts and platform responsibilities. Earlier test
results below retain their historical source baselines; current joint results
are recorded in the bus validation document.

Primary source: SPI m_spi_top HDS V1.0 (48 PDF pages). FU3 HAS v2.0 supplies the
SoC address map and interrupt routes. References below are PDF page numbers.
The model targets functional VP behavior, not RTL cycle or electrical compliance.

## Register behavior

The twelve registers and reset values follow HDS pp.31-39:

| Offset | Register | Reset (depth 16) | Access |
|---|---|---|---|
| 0x00 | CTRL | 0xE0 | RW, hardware-cleared flush bits |
| 0x04 | DIV | 0 | RW, low 16 bits |
| 0x08 | CS | 0x11 | RW, low 6 bits |
| 0x0C | TXDR | 0 | WO; one complete word pushed per write |
| 0x10 | RXDR | 0 | RO with pop side effect |
| 0x14 | STAT | 6 | RO, live status and FIFO levels |
| 0x18 | FTHR | 0x100 | RW, two 7-bit thresholds |
| 0x1C | IER | 0 | RW, low 7 bits |
| 0x20 | ISR | 1 | Level flags [1:0], sticky W1C [6:2] |
| 0x24 | DMACR | 0x10100 | RW, enables and thresholds |
| 0x28 | FVR | 0x100010 | RO, vacancies derived from configured depth |
| 0x2C | DLY | 0 | RW, four 8-bit delays |

Only FIFO depths 16, 32 and 64 are accepted. Invalid depth or zero clock period
throws at construction. Reserved configuration bits read zero and ignore writes;
RO writes have no effect and TXDR reads zero. Offsets 0x30..0xFFFF return zero and
ignore writes with OK response, matching the no-PSLVERR decode behavior.

Full-word operations ignore PADDR[1:0] as in the HDS; aligned byte/halfword
operations select little-endian lanes. Common Full strobes supersede the old
TXDR exception: any nonzero write mask pushes one masked, zero-filled word,
including narrow writes; zero strobes never push or overflow. No FIFO RMW occurs.

Byte enables on configuration writes preserve disabled lanes. ISR W1C only
acts through byte lane zero; level TXT/RXT cannot be cleared by writing ISR.
Every RXDR read pops once, including subword, disabled-lane and debug reads;
byte enables only select returned bytes. Empty reads return zero and set RXUDF.
Full TX writes drop the new word and set TXOVF. `transport_dbg` supports untimed
reads with these same read side effects; debug writes are rejected.

Malformed transports return TLM diagnostics; DMI and nonblocking transport are
unsupported. Calls execute in SystemC threads. Common TXDR strobe behavior is
an explicit override of the older HDS exception, as recorded in the rebaseline.

## Serial engine

Master and slave operation follow HDS pp.19-25. FSZ encodings 0..2 clamp to four
bits; otherwise frame length is FSZ+1. Transmission uses only the low frame bits;
reception is right-aligned and upper bits zero. All CPOL/CPHA modes and both bit
orders are supported. No CRC, packing, quad SPI or mode-fault logic is invented.

In master mode, the controller starts automatically with EN, available TX data
and RX space (unless TXONLY). Each half-SCK interval is `(DIV+1)*sys_period`.
Automatic select spans setup, frames, gaps and hold. CSSU/CSHD/IFD intervals use
the HDS's `2*field+1` half-period rule; CSIDLE retains an idle interval before
starting a new transfer. Internal load/state-transition overhead is a functional
approximation, not an exact RTL timing guarantee. Manual select follows CSLVL and
CSSEL while enabled. Several selected outputs are supported for TX-only broadcasts.

Master RX-full stalls preserve select and resume after space becomes available.
With nonzero IFD, the next TX word stays in the FIFO throughout the inter-frame
delay and any wait for RX space. Only continuous frames (IFD zero with RX space)
pop immediately at the previous frame's end. TX flush can therefore remove a
waiting frame, and abort during GAP does not consume an unstarted word. If flush
empties the waiting TX queue, the transfer can finish without requiring RX space.
No frame starts without TX data, and master traffic does not intentionally drop
received frames. EOT occurs after hold in automatic mode or the final frame in
manual mode. Clearing EN or leaving master mode aborts an unfinished master frame
without generating EOT. Already completed RX frames are retained.
The SCK output is zero in slave mode, including after a MSTR-clear abort with
CPOL high; CPOL continues to select the input edge convention. Master output
enables are removed when MSTR is cleared.

In slave mode, SS falling loads the TX head without popping. The first SCK edge
commits the pop or underrun. Late TX writes are reserved for the next frame.
Empty TX shifts zeros and sets UDR. Full RX drops the completed frame and sets
OVR. TXONLY suppresses RX storage and OVR. SS rising sets EOT and discards a
partial frame. SS rising observed with a last SCK edge wins and discards that frame.

Slave pins are sampled on functional system ticks. Metastability and the exact
two-flop RTL delay are not simulated. External peers should follow the HDS timing
envelope (at least four system periods per half-SCK and setup, two for hold/idle).
MISO output enable is asserted only while enabled, in slave mode and selected.
SCK/SS/MOSI enables follow MSTR even with EN clear. System reset clears all enables.

Quasi-static fields must be configured while disabled and ENS clear. Live changes
other than supported abort/control actions are outside the contract; they are not
silently presented as a supported dynamic reconfiguration feature.

## Interrupts, DMA and flush

TXT is `TXLVL <= TXTH`. RXT is `RXLVL != 0 && RXLVL >= RXTH`. Sticky flags are
EOT, OVR, UDR, TXOVF and RXUDF. IRQ is the registered OR of enabled causes,
including level causes while EN is zero. A hardware set at the same simulation
timestamp takes precedence over W1C. Register operations commit before serial
events on a functional tick; simultaneous FIFO push/pop ordering follows this
explicit VP rule rather than reproducing all RTL nonblocking-assignment details.

TX/RX DMA each use IDLE, REQ and WAIT states (HDS p.27). Conditions use TX vacancy
and RX level with thresholds clamped to at least one. Conditions are checked only
in IDLE; REQ persists until synchronized acknowledgement, then WAIT prevents
retrigger until acknowledgement falls. Disabling EN or a DMA channel cancels its
request. A peer must perform at most its threshold's words per request and complete
bus accesses before releasing acknowledge. The model exposes request/ack pins;
it does not move data on behalf of a DMA controller.

RX flush drains one entry per system tick and self-clears when empty. TX flush
drains through a request/acknowledge/return sequence, preventing a new frame load
while draining; a frame already shifting can finish. Hardware completion wins
over a software flush write on the same tick. Flush must normally be requested
after traffic stops, per HDS p.42. Corner tests exercise flush completion against
a same-tick software set, rewrite during acknowledgement return, concurrent TX
refill, RX reception during drain and stopping RX flush. The selected same-tick
RX drain/push result follows the explicit VP ordering above; arbitrary CPU/DMA/
flush interleavings are not claimed to reproduce every RTL race.

## Reset and APB abstraction

System reset clears registers, FIFOs and serial/IRQ/DMA state asynchronously.
APB reset only resets the transport side and preserves serial activity/configuration.
Reset during APB setup or admission wait cancels a request before launch with
GENERIC_ERROR and no target side effect; an epoch captured at arrival catches
reset pulses that end before setup completes.
An already launched request still commits once; if APB reset occurs before its
response is delivered, the caller receives GENERIC_ERROR instead of a response
being silently lost. Do not automatically retry such a side-effecting request.

Only one blocking APB request is admitted at a time. Incoming delay is consumed
once. The adapter waits one APB period before launch, commits on the next system
tick and waits three APB periods for response. This is bounded functional CDC
latency, not a model of APB phase/synchronizer ratios. Responses return zero delay.
APB reset epochs detect pulses during an outstanding request. System-only reset
must be issued while APB is quiescent, as required by HDS p.16; misuse returns a
diagnostic and is not claimed to preserve an outstanding access.

The model advances an internal system tick thread even when idle. Dynamic clock
frequency, clock gating and physical CDC are outside this initial interface.

## FU3 integration boundary

The maintained bus already routes SPI0/1 through PERIBUS_0 and SPI2/3 through
PERIBUS_1. The bank does not own interrupt routing. Tests follow PLIC sources
9/16/22/23 and Ibex fast6=SPI0, fast3=SPI1|SPI2|SPI3 from HAS tables. SPI2/3 pad
assignment and DMA channel numbering remain platform responsibilities; no mapping
is invented. Existing HAS DMA assignments are SPI0 RX/TX 0/1 and SPI1 RX/TX 6/7.

Test fixtures provide serial peers, interrupt receivers and a DMA initiator. They
validate controller communication, not CPU firmware execution or complete boot.

## Clock gate (platform integration)

The clock controller (FU3 CRM gate, through the platform) drives the gate; it is enabled by default, so a stand-alone bench needs no change. `set_clock_enabled(false)` removes system clock edges: a transfer stops where it is (SCK, shift state and FIFOs kept) and continues on enable. A gate during a period cancels that edge. Reset stays asynchronous and applies while gated.
