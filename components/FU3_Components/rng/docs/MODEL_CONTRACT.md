# RNG HDS interface and VP collection policy

Scope follows the user's instruction to implement only the existing RNG HDS, not the additional HAS health-test/test-mode requirements. Source: RNG HDS V1.0, PDF pp.6-15. No FIFO, DMA, seed CSR, interrupt mask/clear CSR, test mode, health test or entropy conditioner is implemented. Private PDFs and extracts remain outside this component.

## Spec-defined ABI

| Offset | Register | Behavior |
|---|---|---|
| 0x00 | CONTROL | ENABLE bit0, reset0; lane0 writes, other bits ignored/read0 |
| 0x04 | COMMAND | NEXT bit0, RESTART bit1; lane0-qualified command, read0 |
| 0x08 | DELAY | 32-bit RW, reset0, independent byte strobes |
| 0x0C | STATUS | VALID bit0, RO, reset0 |
| 0x10 | DATA | 32-bit RO, zero while invalid; reads never consume/clear |

Unaligned/unmapped addresses error with zero read data. RO writes error even with all write strobes disabled. Valid register addresses are exact; no aliasing. Reset is synchronous active-low and samples at clock boundaries. Reads have no register side effects. The HDS wrapper has no added wait cycles; model MMIO waits only for the next processing boundary after consuming incoming annotated delay, independently of upstream bus latency.

IRQ is VALID OR a completed local APB access error. There is no stored error or mask. The LT adapter represents each local error contribution as one model clock period; a later error can extend it. An upstream bus rejection does not reach the RNG and cannot cause its error IRQ. Physical PLIC sampling of a short pulse still needs platform verification.

## Explicit model decisions, not established RTL behavior

The PDF does not specify complete generator equations, collection bit ordering or counter recurrence. To execute data-flow tests, this model uses a replaceable input bit callback and the following provisional schedule. Tests of this schedule do not prove the missing RTL rules.

- Collection starts only after NEXT, with cleared VALID/data/accumulator/count/counter. Exactly 32 callback bits are shifted MSB-first into the result. Collection stops when VALID becomes true, preserving the result until NEXT/RESTART/reset.
- Each sample occurs after DELAY+1 enabled model edges, excluding the command edge. This formula is a VP choice, not a quoted hardware timing guarantee. DELAY is compared live with a wrapping unsigned 32-bit counter. Changing DELAY below the counter can therefore require wraparound; NEXT resets the counter.
- Disabling pauses collection/counter and preserves an existing valid result. No reset is implied. Re-enable resumes.
- RESTART clears state and source index and waits for NEXT. Combined NEXT+RESTART resets the source index and starts a fresh collection. NEXT alone preserves the source index. Every accepted COMMAND write is a separate pulse, including consecutive writes; exact RTL nextTrigger_REG pulse-edge behavior remains unqualified.
- Reset wins over MMIO/collection at a boundary. A command suppresses sampling on its boundary. Otherwise MMIO executes before sampling: a read sees the previous result when completion happens on that same boundary.
- Sample callback index restarts on hardware reset/RESTART. This supports reproducible testing; it is not a software-programmable seed, hardware oscillator or LFSR implementation.

The source callback is required so callers consciously provide simulation data. It does not establish non-determinism, entropy rate, statistical health or suitability for cryptographic use.

## TLM adaptation

Four-byte payloads with streaming width at least four are supported. Little-endian bytes and repeated TLM byte-enable patterns map write lanes directly. Reads honor TLM read enables without interpreting them as physical APB PSTRB. Narrow payloads are rejected by the adapter; software should use word accesses with masks for partial writes. Malformed transport shapes do not raise the physical APB-error IRQ. No DMI is granted. Debug supports untimed, non-destructive reads only and never creates an IRQ. Pending normal MMIO during reset returns a generic adapter error.

## Open fidelity items

Confirm collection without NEXT, disable behavior, exact DELAY/tick recurrence, bit ordering, simultaneous-command priority, and consecutive NEXT edge detection against an updated spec or RTL before claiming full generator-state equivalence. Runtime clock gating/frequency changes and actual CPU firmware boot are not tested. These gaps do not justify introducing extra registers or unsupported HAS features.
