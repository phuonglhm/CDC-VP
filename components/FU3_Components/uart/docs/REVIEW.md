# UART spec review and repair ledger

Review policy: attempt at most three repairs per defect. If progress stops, record
the reproducer, attempted changes, residual impact and next action, then continue
with independent issues. A missing hardware definition is not a code defect to
"fix" by guessing. No speculative CSR bits are added.

## Source gaps identified before implementation

| ID | Evidence | Resolution / remaining action |
|---|---|---|
| S02 | p.22 says IER/IIR not needed; pp.29-30 require both | Implement both, prioritize executable programming sequence; compatibility IER/IIR encodings are explicit |
| S03 | HDS v1.2 pp.24,27 defines MCR[5] AFE and the AFE/RTS combinations | Implemented through firmware-visible MCR.AFE; verified in behavior tests |
| S04 | pp.8,10 promise RS485 via MCR; no register bits or DE/RE pins defined | No fabricated RS485 ABI; needs corrected register/pin definition |
| S05 | p.19 supports 8x/16x, while p.28 divisor formula hardcodes 16; RATIO is listed without a clear relation | **Deferred until RTL correlation.** Config selects functional sampling cadence; VP does not claim exact baud at either oversampling setting |
| S06 | HDS v1.2 p.18 specifies synchronous UART reset and asynchronously clears MSR[3:0]; p.19 specifies safe MSR[7:4] reset sampling | Implemented explicit asynchronous delta clear plus synchronous main reset; sub-cycle reset pulse covered |
| S07 | p.9 excludes fractional baud; p.19 says the baud generator may be fractional or integer | **Deferred until RTL correlation.** VP uses integer DLL/DLM and does not model fractional-rate accuracy |
| S08 | HDS v1.2 pp.8,20 specify APB3; p.20 misspells PSLVERR as PSLVRR and gives no useful error decode behavior | Common now overrides this restriction with byte qualification; TLM errors remain adapter diagnostics |
| S09 | p.29 allows TX DMA request deassertion or persistence on clear | Document and test sampled clear suppression with condition-based reassertion |
| S10 | p.18 allows 32-byte capacity; trigger table covers only 16/64 | Preserve published table; trigger 56 cannot assert at capacity 32; timeout still works |
| S11 | pp.23-24 omit precise non-FIFO overwrite/error lifetime and flush/resize priorities | Explicit conservative policies; do not claim exact RTL race equivalence |
| S12 | p.8 gives 12.5 Mbps at 200 MHz; p.19 gives 95 bps–6.25 Mbps without naming its clock | Consistent with the p.28 formula at 200 MHz and 100 MHz respectively; document the p.19 clock assumption during RTL/spec cleanup. Not a VP blocker |
| S13 | pp.19,22 can read as RX DMA request on any nonempty FIFO; p.30 says trigger reached or timeout | Model uses nonempty data AND (threshold OR timeout), as agreed; HDS should publish the exact Boolean condition |
| S14 | p.8 feature summary lists none/even/odd parity; pp.23,29 also define stick/mark/space parity | Model follows the detailed LCR/operation definition; clarify the feature summary |
| S15 | p.22 interface table has malformed signal directions/descriptions; p.20 spells PSLVERR as PSLVRR | Correct the interface table before RTL pin-level integration |
| S16 | pp.19-20 say automatic RTS follows RX FIFO capacity but give no assert/release thresholds or hysteresis | VP uses trigger-level assertion and release when FIFO empties; keep labeled as a model policy |

These gaps remain visible even when tests pass. They do not prevent testing the
specified UART data path, standard polling/IRQ flows, or bus integration.

## HDS v1.2 implementation updates

The current implementation follows the supplied v1.2 details that were absent
from the earlier review baseline: MCR[5] AFE is retained and controls automatic
flow control, and MSR[3:0] clear asynchronously on reset assertion. The latter
is an explicit exception to synchronous reset, not a contradiction. RX DMA request
uses available data as a prerequisite and combines trigger-level and timeout
causes with OR. The HDS wording that says “not empty or reaches the trigger” is
broader if read literally; the model does not request on a lone below-threshold
byte unless timeout is set.

## Deferred baud-rate fidelity

Per the VP scope decision, exact baud, fractional generation, and the 8x/16x
divisor relationship are not acceptance criteria before RTL correlation. The
model retains deterministic serial progress for firmware data-flow tests; these
timings must not be used to claim baud-rate compliance.

## VP issues still open

These items affect what the current model can represent or what its tests can
claim. They are separate from document corrections that do not change VP behavior.

| VP item | Current status | Impact / next step |
|---|---|---|
| Compatibility behavior where the HDS is incomplete (IER/IIR encodings and priority, receive-timeout duration, debug side effects, FIFO resize/flush and non-FIFO overflow details) | Bounded behavior is selected in MODEL_CONTRACT.md and covered by component tests where applicable | Firmware can test against this VP contract, but those choices are not guaranteed to match final RTL. Revisit only when an authoritative definition or RTL is available |
| Simultaneous FIFO, interrupt, and MMIO event ordering | Deterministic tick order is implemented and documented; targeted cases pass | Unspecified same-tick races may differ from RTL; expand tests when integration exposes a concrete dependency |
| Automatic RTS thresholds/hysteresis | VP asserts at the receive trigger and releases when the FIFO empties | Backpressure is usable for functional tests, but threshold-level behavior is a model policy pending an authoritative rule |
| Baud timing, fractional generation, RATIO and prefilter SIZE | Exact baud fidelity deferred; unsupported/undefined parameters are not modeled independently | Current serial timing supports deterministic frame/data-flow tests only; do not use it for baud sign-off. Revisit baud when correlating with RTL |
| RS485 firmware behavior | Not modeled because the firmware-visible mode and driver-enable interface are undefined | RS485-specific firmware flow cannot be validated until its VP/RTL interface is defined |
| Full-platform boot and real interrupt/DMA paths | Component integration uses fake PLIC/Ibex routing and a fake DMA peer; no CPU firmware/boot ROM runs | The UART VP is not yet evidence of end-to-end boot or production DMA/PLIC behavior; validate those at platform integration |
| Broader robustness coverage | Exhaustive noise patterns, all reset phases, and all same-tick race combinations are not covered | Current tests establish the listed cases, not exhaustive serial-interface or concurrency coverage |

The UART is ready for component-level firmware data-flow work under the documented
contract. The open items above define what remains before claiming full-platform
boot-flow validation or conformance to final UART RTL.

## Repair attempts

| ID | Problem | Attempts | Result |
|---|---|---|---|
| B01 | Local WSL validation script had Windows CRLF after generation | 1: normalize ignored script to LF | Build script starts; no production source change needed |
| T01 | Integration expected a four-byte DMA trigger while extended FIFO mode was still selected | 1: change FIFO mode under DLAB before selecting trigger encoding 01 | Static/shared integration passes; preserves HDS DLAB protection rather than weakening model |
| C01 | Signed/unsigned comparison warning in TX frame construction | 1: use unsigned literals for the unsigned loop bound | Static/shared library rebuilds emit no warnings |

## Post-build spec reread

Re-read pp.22-29 after the initial build, including DLAB qualification, FIFO error
summary, interrupt clear sequences and DMA trigger semantics. The failed DMA
expectation was a fixture configuration error (T01), not a reason to remove the
DLAB requirement. Static behavior tests at all four depth/oversampling settings,
integration and installed consumer passed after that correction.

The review found missing evidence for concurrent accesses, queued accesses during
reset, back-to-back external RX and multiple pending interrupt classes. Added a
separate boundary suite rather than claiming these from ordinary loopback tests.
All six tests in both static/shared builds and both installed-consumer runs pass
(14/14). The final execution evidence is in VALIDATION.md. No issue has reached the
three-attempt limit; unresolved source-definition items above remain open.
