# HDS decisions and VP policies

Reviewed on 2026-10-19 against the supplied DMA HDS, `src/ap_dma.cpp`, the
peripheral adapter, register documentation and current tests. DMA here means
the FU3 `DMA0/AP_DMA` component. This is an open decision log, not an approval
of the hardware specification or an assertion that every open item is a bug.

## Source baseline

Source: `FTEL_CDC_DMA_Hardware_Design_Specification_V1.0.docx.pdf`, 74 pages,
under `D:/Intern_doc/Intern/VP/004_IP/008_DMA/`.
SHA-256: `b97a264907b44f0c9d03d2dbdedf4b1dbc4485da911880de8061ffb7c4413623`.
References below use 1-based physical PDF pages. The filename identifies V1.0
while the body header identifies FU2 DMAC V1.1; the owner must identify the
approved FU3 integration baseline.

## Explicit requirements and scope

The detailed sections define eight channels, a 32-byte FIFO per channel,
32-bit addresses/data, up to four pending transactions per direction, bursts
of at most 16 beats, fixed/incrementing addressing and a 10-bit byte count.
The register map is on pp34-48. Memory-to-memory, memory-to-peripheral and
peripheral-to-memory operations, command chaining and endian conversion are
implemented. Peripheral endpoints have one credit, while memory endpoints
independently clamp their effective pending limit to 1..4 (p53).

Channel disable preventing new grants while accepted transactions drain is
explicit on pp45/50. It must not be confused with a fully specified abort.
FLUSH acknowledgement without data transfer, retained SINGLE/BURST metadata
and DRLAST not terminating the command are explicit on p21.

## Open decisions

`Conflict` marks inconsistent source statements. `Undefined` marks an omitted
outcome. `Integration` and `VP abstraction` mark external contracts or hardware
details that the transaction model does not verify. All items remain open for
the appropriate IP/SoC owner; tests do not resolve their normative status.

| ID | Source and unresolved issue | Current VP behavior | Required confirmation |
| --- | --- | --- | --- |
| DMA-D01 | Conflict: pp7-9 call the master AXI4; p32 calls the 16-beat maximum AXI3 and lists fixed WID as well as ARID/AWID | TLM master with 32-bit data, fixed ID4 metadata and at most 16 beats; no raw AXI channel interface | Protocol version, actual port list, ID/beat rules and approved adapter |
| DMA-D02 | Conflict: p11 feature 19 and p35 CORE_PRIORITY_CONFIG promise programmable priority; pp21/26 limit this build to weighted round-robin and disable priority control | Store priority bits for readback only; arbitrate read/write independently using tokens; clear priority capability bit | Whether fixed priority is supported; reconcile feature table, register description and capability value |
| DMA-D03 | Conflict: p28 describes 1..8 configurable channels, programmable transfer units and up to 256 units; pp9-10/32/40 fix eight channels and a 10-bit byte count without a unit-select CSR | Eight channels; 1..1023 bytes; derive 1/2/4-byte units from endpoint alignment and service size | Authoritative channel parameterization, byte-count encoding and unit-selection algorithm |
| DMA-D04 | Undefined: p33 describes active-low synchronous/asynchronous reset according to an unspecified RTL port; pp49-50 give no complete overlap/release timing | Asynchronous assertion clears core state; reset epochs discard stale responses; already committed downstream writes remain | Exact reset type, release requirements and handling of accepted work across reset |
| DMA-D05 | Undefined: pp24/33/47-48 name timer sources but provide no complete threshold programming, units or boundary/priority rules | EngineConfig supplies transaction, phase and watchdog times; zero disables a timer; equality succeeds, exceeding the threshold times out; watchdog pauses while disabled | Hardware thresholds/defaults, clock units, progress events and simultaneous timeout/response rules |
| DMA-D06 | Undefined: p37 leaves DESIGN_CAPABILITY_STATUS upper bits unspecified; p38 does not provide one complete optional-feature value; p41 contains an unexplained `Total = 0x0000_0F1F` in the read-config description | Default design capability=1; capability0=0x0A602258; capability1=0xF17 with chaining enabled, adjusted if disabled; timer support bits remain set when thresholds are zero | Complete fixed capability values and placement/correction of the p41 total; align priority/wait/divider bits with actual features |
| DMA-D07 | Undefined: p43 says addresses permit a full-FIFO burst without defining the exact predicate | Restriction bits 0/1 use source/destination 32-byte alignment; bit2 combines them; bits5/6 follow effective pending mode | Required alignment, length/endpoint constraints and update timing for these status bits |
| DMA-D08 | Undefined: pp40-42/53 do not fully define zero length, zero burst/tokens or partial endian groups; p24 explicitly withholds broad illegal-configuration claims | Reject zero-size/zero-burst starts, END_SWAP=3 and partial final swap groups; tokens=0 acts as one; reject peripheral-to-peripheral operation | Hardware response for each encoding and partial group, and whether errors are start-time or run-time |
| DMA-D09 | Undefined: pp36/45/66-67 define start pulses but not a global start containing both valid and invalid channels | Validate every selected channel before starting any; invalid global starts have no partial effect; active/disabled/invalid starts return a TLM command error | Per-channel versus atomic global start and repeated-start semantics |
| DMA-D10 | Undefined: pp42/46/48/50 define fields and software restrictions but not every prohibited access response | Reject RO writes, WO reads and active-channel configuration writes; mask reserved bits; unmapped access errors follow the explicit map | Hardware error/ignore semantics for prohibited accesses; detailed APB interface sections on p34 are empty |
| DMA-D11 | Undefined: pp45-46 define command/event counters and raw-status injection but not overflow or every clear interaction | Command counter wraps at 4096; pending events saturate at 15; clear bit0 consumes one event; raw writes are W1S and do not fabricate completed commands | Counter overflow behavior, clear-one versus clear-all and simultaneous clear/completion/injection ordering |
| DMA-D12 | Undefined: p10 and pp21-22/42 specify service credits and delay fields, but not every held-request, rearm and recovery edge; numeric DTYPE codes are absent | One outstanding credit per endpoint/direction; held external request must drop after service; clear lasts one configured peripheral cycle; recovery is shared by channels using that endpoint. Adapter encodings SINGLE=0, BURST=1, FLUSH=2 are VP-only | Actual DTYPE encoding, sampling clock, rearm and recovery counting, including combined software/external requests (p66 cautions against mixing them) |
| DMA-D13 | Undefined/integration: pp32-33 leave CACHE/PROT defaults to integration; pp42-43 define only two override PROT bits | Defaults are configurable and zero unless supplied; defaults accept three PROT bits, CSR overrides replace PROT with a two-bit value; descriptors use memory defaults | Full integration defaults, treatment of the third PROT bit on override and attributes of descriptor reads |
| DMA-D14 | Undefined: p21 says no defined abort; pp28-29 nevertheless mention aborted channels; pp69-70 do not fully define partial side effects or all fault recovery states | Disable pauses new grants; errors stop new work and drain accepted calls; reset cancels logical state without undoing external writes | Approved abort/cancel interface, retained diagnostics and partial-transfer recovery contract |
| DMA-D15 | VP abstraction: p44 defines outstanding counts at AR/AW acceptance, and p24 refers to actual AXI phase/protocol observations | Count logical worker contexts; TransferExtension can report phases, response IDs and injected faults; absent phase reports use ReadData/WriteResponse. No cycle-accurate AR/AW acceptance is modeled | Approved abstraction and an AXI adapter/RTL testbench for hardware timing, WSTRB and protocol-fault equivalence |
| DMA-D16 | Document/integration gap: V1.0 filename versus FU2 V1.1 body, empty interface/function subsections pp33-34 and template verification/integration material pp70-74; p41 calls CH_WRITE_CONFIG[6:0] a maximum read burst size | Use the pinned PDF and detailed register/operation content; interpret CH_WRITE_CONFIG as write-side configuration; platform supplies physical map and timing | Approved FU3 revision, corrected read/write wording, complete interface tables and normative verification requirements |

Prioritize DMA-D01 through DMA-D06 before hardware integration sign-off. The
priority contradiction and capability literals are specification issues, not
evidence that the VP should silently add a new arbitration mode.

## Additional transport and scheduling conventions

The target consumes incoming delay and accepts aligned little-endian 32-bit
MMIO without byte enables. Its precise TLM response codes are documented in
[usage.md](usage.md). MMIO has no cycle-accurate APB setup/access phases.
Logical engines use a configurable scheduler quantum; a zero setting still
yields one simulation-resolution tick so cyclic command lists remain stoppable.
This is not a specified hardware throughput or fairness bound.

A downstream target must permit concurrent blocking calls. The VP cannot
cancel a target call that never returns. Timeouts stop new dispatch and record
errors, while outstanding calls retain their payloads until they return. This
is a TLM integration limit, not an assertion that hardware must stall forever.

## Regression evidence and closure

CTest names below use the prefix `fu3_ap_dma_`. A scenario can verify both
explicit HDS behavior and VP choices; this suite does not provide a separate
per-requirement HDS sign-off verdict.

| Decisions / functionality | Existing test suffixes | Evidence boundary |
| --- | --- | --- |
| Map, masks, capabilities, restriction and injected IRQs; D06-D07/D10-D11 | `registers`, `decode`, `capabilities` | Capability/alignment/injection policies are reproducible; missing HDS bits are not approved |
| Directions, sizes, endian, IDs and channels; D03/D08-D09 | `m2m`, `peripheral`, `endian`, `matrix`, `ids`, `channels`, `errors` | Functional byte transfers, including chosen invalid-start behavior |
| Arbitration, pending limits and ordering; D02/D15 | `arbitration`, `pending`, `limits`, `ordered`, `stress` | Logical contexts and token grants, not AXI wire arbitration |
| Reset, errors and timers; D04-D05/D14 | `reset_pending`, `fault_drain`, `timeout`, `watchdog`, `timer_boundary`, `phase_timers`, `timers_combined`, `transaction_boundary`, `phase_boundary` | VP cancellation, injected protocol phases and configured thresholds |
| Peripheral handshake/recovery; D12 | `handshake`, `adapter_rx`, `adapter_tx`, `peripheral_recovery` | Functional adapter contract, not raw wrapper DTYPE encoding |
| Attributes, descriptor chain and fault metadata; D13/D15 | `attributes`, `chain`, `mixed_chain`, `protocol_faults`, `boundaries` | Metadata and descriptor behavior, not pin-level AXI certification |
| Source baseline; D01/D16 | Source review | Owner approval cannot be established by simulation |

The 2026-10-19 standalone Debug build and rerun passed **36/36 tests**, with no
failures or disabled tests. Three seeded random scenarios are included in that
total. Evidence is in `build/hds-review/dma-results.xml`, `dma-regression.log`
and the isolated `build/hds-review/dma-debug/` build. This review did not rerun
UBSan or perform full-SoC firmware execution.

Close an item only after recording the owner's approved revision/section,
checking the implementation and updating affected tests. Passing this
regression does not imply full HDS or RTL equivalence.
