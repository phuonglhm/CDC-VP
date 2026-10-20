# UART spec review and repair ledger

Review policy: attempt at most three repairs per defect. If progress stops, record
the reproducer, attempted changes, residual impact and next action, then continue
with independent issues. A missing hardware definition is not a code defect to
"fix" by guessing. No speculative CSR bits are added.

## Source gaps identified before implementation

| ID | Evidence | Resolution / remaining action |
|---|---|---|
| S01 | Cover V1.0, history V2.0, headers FU2 | Identify by filename and PDF pages; request authoritative revision at hardware handoff |
| S02 | p.22 says IER/IIR not needed; pp.28-29 require both | Implement both, prioritize executable programming sequence; compatibility IER/IIR encodings are explicit |
| S03 | pp.19-20 promise auto flow; p.23 reserves MCR[7:5] | Platform config enables behavior; firmware enable encoding remains open |
| S04 | pp.8,10 promise RS485 via MCR; no register bits or DE/RE pins defined | No fabricated RS485 ABI; needs corrected register/pin definition |
| S05 | p.8 supports 8x/16x; no selector; RATIO listed without equation | Constructor selects 8/16; default 16 follows p.27 formula; software selector and RATIO relation open |
| S06 | pp.18,22 omit numeric reset values; FIFO "default" wording ambiguous | Deterministic reset profile documented; requires confirmation |
| S07 | p.9 excludes fractional baud; p.19 says fractional or integer | Implement integer DLL/DLM only |
| S08 | p.20 APB3/no PSTRB; table prose mentions APB4; p.10 ties PSLVERR low | No sparse writes; TLM errors explicitly adapter diagnostics |
| S09 | p.29 allows TX DMA request deassertion or persistence on clear | Document and test sampled clear suppression with condition-based reassertion |
| S10 | p.18 allows 32-byte capacity; trigger table covers only 16/64 | Preserve published table; trigger 56 cannot assert at capacity 32; timeout still works |
| S11 | pp.23-24 omit precise non-FIFO overwrite/error lifetime and flush/resize priorities | Explicit conservative policies; do not claim exact RTL race equivalence |

These gaps remain visible even when tests pass. They do not prevent testing the
specified UART data path, standard polling/IRQ flows, or bus integration.

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
