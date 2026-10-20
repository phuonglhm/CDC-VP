# UART functional model contract

## Source precedence

Primary: supplied UART HDS PDF, 34 pages. The cover says V1.0, revision history
lists V2.0, and page headers still name FU2. These labels do not establish a new
FU3 version. Page references below are PDF page numbers. FU3 HAS v2.0 supplies
SoC wiring. Explicit local register definitions take precedence over generic
16750 behavior.

The local document claims 16750 compatibility but leaves some software semantics
undefined. The [TI TL16C750 datasheet](https://www.ti.com/lit/ds/symlink/tl16c750.pdf)
is a secondary reference for IER[3:0], IIR source IDs and FIFO indicators,
four-character receive timeout, and read-cleared modem deltas. Those selections
are VP compatibility choices, not additional statements from the FU3 HDS.

## Register and transport boundary

The HDS pp.20,22-28 specifies a 3-bit register address and explicit byte offsets
DLL=0x00, DLM=0x01, FCR=0x02 and LCR=0x03. The model uses offsets 0..7 exactly;
the unspecified SoC address adapter must not be assumed to shift them by two.
Only this subrange of each 64-KiB window decodes; other offsets return TLM address
errors. No mirror aliases are invented.

One transaction addresses one 8-bit register. Length 1 or 4 is supported; a
four-byte direct target transfer carries the register in the low byte and reads
zero in upper bytes. Length 2, bursts, wrapping payloads, null pointers and
invalid commands return diagnostics. Normal bus clients should use byte MMIO.
All-enabled TLM byte enables are accepted; sparse or disabled bytes are rejected.
The HDS APB3 interface has no strobes, despite APB4 wording elsewhere. This avoids
silently applying the SoC bus's general strobe policy to this endpoint.

HDS p.10 ties PSLVERR low; p.20 describes an error output without decode rules.
TLM diagnostics above are adapter decisions, not a claim that RTL asserts PSLVERR.
Accepted requests are serialized, consume incoming delay exactly once and commit
on a following system-clock tick. The mutex remains held through completion.
Every error path yields. No DMI or nonblocking transport is provided.

Debug reads are deliberately non-destructive: no RBR pop or LSR/MSR/IIR clear.
Debug writes are rejected. This debugger policy is not specified by the HDS.

## CSR behavior and reset profile

- RBR/THR share offset 0; DLL replaces both when DLAB=1. IER at 1 becomes DLM.
- FCR writes and IIR reads share offset 2. LCR/MCR/LSR/MSR/SCR are at 3..7.
- Reserved IER bits [7:4] and MCR bits [7:5] read zero. LSR/MSR writes do nothing.
- IER and IIR are implemented: the software sequence pp.28-29 explicitly uses them,
  overriding the contradictory "not needed" notes in the register table.
- IER enables RX/timeout, THRE, line errors, modem changes in bits 0,1,2,3.
- IIR low nibble: line error 6, RX threshold 4, RX timeout 12, THRE 2,
  modem 0, no interrupt 1. This is also the priority order, except RDA and timeout
  belong to the same class and RDA wins when both apply. Bits 7:6 indicate FIFO
  enable and bit 5 extended FIFO mode. IRQ is not gated by MCR.OUT2.
- IIR acknowledges THRE only when it is the sole active source, following p.29.
  THR writes clear its pending latch; a transition to empty or enabling IER.TX
  while already empty rearms it. THRE is not TEMT: a shifted frame can remain active.

Reset is active-low and synchronous (p.18). No separate UART reset domain or CDC
is invented. Reset cancels serial work and pending MMIO with a diagnostic, empties
FIFOs and drives TX/modem pins inactive. Default IER/FCR/LCR/MCR/SCR/DLL/DLM=0,
IIR=1, LSR=0x60. MSR upper bits follow sampled modem inputs after reset. These
numeric defaults are a deterministic VP profile because the local HDS omits a
reset-value table. Divisor zero disables launching TX/RX frames; it does not mean
a hidden maximum divisor. These defaults require confirmation against RTL.

## FIFO and receive errors

FCR[0] selects FIFO operation. Disabled mode has one holding entry; enabled mode
uses 16 entries or the configured extended capacity, selected by FCR[5]. The
extended capacity is 64 by default, or 32 for the HDS SIZE_E=5 parameter.
FCR[5] changes only while DLAB=1. Writes without DLAB preserve the bit. FCR[1:2]
are immediate reset commands, not stored bits. Changing enable/size flushes both
queues. TX reset preserves a frame already in the shift register; RX reset clears
queued data/errors but does not cancel an incoming frame. These reset/resize
details are VP choices where the document is silent.

Trigger tables are exactly 1/4/8/14 and 1/16/32/56 from p.25. A trigger above an
extended capacity of 32 never becomes true; timeout remains available. No
unpublished 32-byte table is invented. Empty RBR returns zero. Full TX discards
the incoming write; no TX overflow bit exists. Full RX discards the newly received
frame and latches OE, preserving unread data. The HDS describes overrun but does
not unambiguously specify which byte survives in non-FIFO mode.

Each RX entry carries PE/FE/BI and an acknowledgement flag. LSR[4:2] reports
unacknowledged errors at the head. LSR reads acknowledge those errors and OE.
Popping RBR exposes the next entry's errors. LSR[7] remains set while any queued
entry carries an error, following the local p.24 wording, even if acknowledged.
An RBR read clears timeout; an empty read does not fabricate an error flag.

## Serial behavior

The single configurable clock period defaults to 10 ns. Baud is
`1 / (clock_period * oversampling * divisor)`, default oversampling 16, matching
p.27. The contradictory fractional-generator prose on p.19 does not override
the explicit unsupported-fractional list on p.9. The RATIO parameter's relation
to oversampling is undefined; no extra factor is applied to the published formula.

For the HAS 20 MHz boot source, divisor 65 gives approximately 19,230.77 baud
(+0.1603% relative to 19,200). The integration fixture uses that integer divisor
and even parity; the HAS specifies 8P1 but does not select odd versus even parity.
The fixture is not evidence of bit-exact compatibility with an existing boot ROM.

TX emits start low, 5-8 data bits LSB first, optional odd/even/mark/space parity,
then 1, 1.5 (5-bit only) or 2 stop bits. Break overrides TX low. RX validates start,
uses a three-sample majority around each midpoint, checks parity and first stop,
and stores right-aligned data. A continuous low produces a single break entry
until the line rises. The RX FSM figure p.14 uses first-stop sampling; RX does
not require a second stop bit to accept a frame. Back-to-back reception is supported.

Format and divisor are captured per frame; software should configure while idle.
There is no claim about dynamic reconfiguration mid-frame matching RTL. The
receiver abstracts metastability and the unspecified prefilter transfer function;
it models midpoint majority voting and false-start rejection, not analog noise.

FIFO timeout is four complete character durations since the last received frame
or RBR read, including the programmed stop length. Once asserted, it remains
pending until an RBR read or RX reset. Timeout applies only in FIFO mode.

## Modem, flow control and DMA

MCR[3:0] drives active-low DTR/RTS/OUT1/OUT2. MSR samples active-low CTS/DSR/RI/DCD;
CTS/DSR/DCD transitions latch deltas, RI low-to-high latches TERI. Reading MSR
clears deltas. Loopback connects internal TX to RX and, as a compatibility choice,
maps RTS/DTR/OUT1/OUT2 to CTS/DSR/RI/DCD and releases external outputs.

Auto flow control can be enabled by `Config::auto_flow_control`. Auto-CTS gates
new frames, never truncates an active frame. Auto-RTS requires MCR.RTS and stops
at the receive trigger; it resumes when the FIFO empties. This hysteresis is an
explicit VP choice. No MCR enable bit is defined by the local HDS; reserved bits
remain zero. RS485 has no specified mode bit or driver-enable pin and is not
implemented as a speculative register extension.

FCR[3] enables DMA. TX requests when space exists. RX requests at trigger or
timeout. A sampled clear input suppresses the corresponding request while high;
when low, the request reflects the current condition again. This is one bounded
interpretation of p.29's allowed deassert/reassert behavior, not a SPI-style
four-phase handshake. The peer must check available capacity/threshold and finish
bus operations before acknowledging; UART does not perform memory transfers.

## Event ordering and integration limits

On each tick: reset takes precedence, otherwise one MMIO operation commits,
modem pins are sampled, RX advances, TX advances, timeout/RTS/outputs update.
A new serial error survives an LSR read on the same tick. FIFO slots freed by an
MMIO read can accept a simultaneous frame. This deterministic VP order is not
evidence of unspecified RTL simultaneous-event priorities.

Clock ticks continue while idle. Dynamic clocks, electrical RS232/RS485 behavior,
pad muxing, real PLIC and DMA implementations and CPU firmware execution are
outside the component tests. Known spec gaps are tracked separately from code
defects in REVIEW.md, with at most three repair attempts per issue.
