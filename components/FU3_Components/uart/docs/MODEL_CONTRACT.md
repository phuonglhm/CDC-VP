# UART functional model contract

## Common Spec baseline (2026-10-21)

The supplied FU3 Common Spec v2.0 workbook is the highest authority, followed
by FU3 HAS v2.0 and the latest IP detail where higher-level documents are silent.
See the bus [rebaseline decisions](../../bus/docs/COMMON_REBASELINE.md) for exact
cross-IP choices, source conflicts and platform responsibilities. Earlier test
results below retain their historical source baselines; current joint results
are recorded in the bus validation document.

## Source precedence

Primary IP detail: supplied `FTEL_CDC_IP_UART_Specification_v1.3.pdf`, 35 pages. Page
references below are the printed page numbers. FU3 HAS v2.0 supplies SoC wiring.
Explicit local register definitions take precedence over generic 16750 behavior.

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

One transaction addresses one 8-bit register, using length 1 or 4. A four-byte
payload carries the register in byte zero; other bytes do not address neighboring
registers. Common strobes override the older APB3 profile: byte-zero enable
qualifies writes and destructive reads. Disabled lanes remain unchanged on read.
Length 2, invalid commands and malformed enable bytes are rejected.

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
- Reserved IER bits [7:4] and MCR bits [7:6] read zero. MCR[5] is AFE. LSR/MSR writes do nothing.
- IER and IIR are implemented: the software sequence pp.29-30 explicitly uses them,
  overriding the contradictory "not needed" notes in the register table.
- IER enables RX/timeout, THRE, line errors, modem changes in bits 0,1,2,3.
- IIR low nibble: line error 6, RX threshold 4, RX timeout 12, THRE 2,
  modem 0, no interrupt 1. This is also the priority order, except RDA and timeout
  belong to the same class and RDA wins when both apply. Bits 7:6 indicate FIFO
  enable and bit 5 extended FIFO mode. IRQ is not gated by MCR.OUT2.
- IIR acknowledges THRE only when it is the sole active source, following p.30.
  THR writes clear its pending latch; a transition to empty or enabling IER.TX
  while already empty rearms it. THRE is not TEMT: a shifted frame can remain active.

Reset is active-low and synchronous for UART state (p.18), with the explicit
exception that `MSR[3:0]` delta bits clear asynchronously when `preset_n` is
asserted low. `MSR[7:4]` hold zero during reset and capture modem inputs at the
first active edge after reset deassertion (pp.18-19). Reset cancels serial work
and pending MMIO with a diagnostic, empties FIFOs and drives TX/modem pins
inactive. Defaults are IER/FCR/LCR/MCR/SCR/DLL/DLM=0, IIR=1, LSR=0x60, MSR=0.
Divisor zero disables launching TX/RX frames; it does not mean a hidden maximum
divisor.

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

Baud-rate accuracy is deferred until RTL correlation because this VP targets
firmware-visible frame and data-flow behavior, not bit-rate sign-off. The HDS
initial sequence gives `divisor = fCLK / (16 * baudrate)` (p.28), but p.19 also
allows 8x or 16x sampling without explaining how 8x relates to that fixed factor.
The document excludes fractional baud on p.9 while p.19 says the generator may
be fractional or integer. Its 12.5 Mbps at 200 MHz (p.8) and 6.25 Mbps down to
95 bps (p.19) are consistent with the formula at 200 MHz and 100 MHz, though p.19
does not state that clock assumption. DLL/DLM remain integer software-visible
registers and provide deterministic relative frame timing in the VP; the model
makes no claim that its Config clock/oversampling combination reproduces the
specified baud at 8x or 16x. The `RATIO` relationship is likewise deferred.

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

With a 32-entry physical FIFO and extended FIFO mode, the HDS trigger table's
56-entry setting is deliberately not clamped: RDA and threshold-driven DMA cannot
reach it, and auto-RTS cannot stop at that threshold. Timeout service still works.
The HDS supplies no separate 32-entry trigger profile; use a reachable trigger
until that combination is resolved by the IP owner.

## Modem, flow control and DMA

MCR[3:0] drives active-low DTR/RTS/OUT1/OUT2. MSR samples active-low CTS/DSR/RI/DCD;
CTS/DSR/DCD transitions latch deltas, RI low-to-high latches TERI. Reading MSR
clears deltas. Loopback connects internal TX to RX and, as a compatibility choice,
maps RTS/DTR/OUT1/OUT2 to CTS/DSR/RI/DCD and releases external outputs.

MCR[5] AFE selects automatic flow control (pp.24,27). With AFE set, CTS gates
new frames without truncating an active frame. If MCR.RTS is also set, RTS is
automatically controlled from receive FIFO occupancy; otherwise only auto-CTS
is active. The model stops RTS at the receive trigger and resumes after the FIFO
empties, a bounded VP interpretation because the HDS does not specify exact
assert/deassert thresholds. MCR[7:6] are reserved. RS485 has no specified mode
bit or driver-enable pin and is not implemented as a speculative extension.

FCR[3] enables DMA. TX requests when space exists. RX requests when the FIFO is
nonempty and either the configured trigger is reached or receive timeout is
pending. Threshold and timeout are combined trigger causes; either can assert
the request, and both may be true together. A sampled clear input suppresses the
corresponding request while high; when low, the request reflects the current
condition again. The peer must check available capacity/threshold and finish
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

## Clock gate (platform integration)

The clock controller (FU3 CRM gate, through the platform) drives the gate; it is enabled by default, so a stand-alone bench needs no change. `set_clock_enabled(false)` removes clock edges: no RX sampling, no TX progress, no MMIO completion; state is kept. A gate during a period cancels that edge and the phase restarts on enable. Reset stays synchronous: a reset pulse that sees no clock edge (entirely inside the gate) does not reset the UART; a reset still asserted when the clock returns does.
