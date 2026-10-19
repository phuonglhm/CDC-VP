# HDS decisions and VP policies

Reviewed on 2026-10-19 against both supplied QSPI PDFs, controller/helper source,
the generic NOR backend and current tests. This log expands the model choices
in the [README](../README.md#model-choices). It does not approve an HDS revision
or establish RTL equivalence.

## Source baseline

Both files were read from `D:/Intern_doc/Intern/VP/004_IP/013_QSPI/`.
References use 1-based physical PDF pages.

| Alias | Source | SHA-256 |
| --- | --- | --- |
| R | `QSPI_Register_Description.pdf`, 17 pages | `9685210de5699a0729545f66e9288588e797f39d3c281947d4f14320e44d70ea` |
| W | `QSPI_Wrapper_Interface.pdf`, 9 pages, revision v0.1 dated 10/09/2026 on p9 | `991cd8d40e443370ef7b5757a868b5ac7bb02ef0aee1231a7bf94507869482cc` |

The README records an earlier delivery directory. The hashes above identify
the copies actually reviewed; directory names alone are not a revision ID.
R does not provide an explicit revision history identifying an approved pair
with W.

## Explicit requirements and scope

R pp2-14 define the 16 registers, zero reset values, command triggers, indirect
write/read, automatic polling, mapped mode, flags, DMA requests and clear rules.
ABORT closes the frame, flushes FIFOs, self-clears and sets TCF (R p4).
FIFO threshold flags can reassert after W1C while their condition remains true
(R p5). These are explicit requirements, not missing definitions.

PTR is a polling timeout and completes a status read already in progress
(R p12). It is not an XIP inactivity timer. The divisor must be even in SDR and
divisible by four in DDR (R p2); software must program a legal divisor before
starting. W pp3-5 define three clock/reset domains, a default bus response
timeout of 5000 bus cycles and single 32-bit mapped transfers.

## Open decisions

`Conflict` marks inconsistent source statements. `Undefined` marks an omitted
outcome. `Integration` or `VP abstraction` identifies an external contract or a
hardware behavior not verified by this model. All items remain open for owner
confirmation; a passing policy test does not choose the hardware specification.

| ID | Source and unresolved issue | Current VP behavior | Required confirmation |
| --- | --- | --- | --- |
| QSPI-D01 | Conflict: R p3 describes TX/RX flag thresholds in words and a full FIFO of 32 words; R p5 describes byte levels/full=32 and byte flag thresholds; W p4 gives FIFO depth 32 without a unit | Storage and levels count bytes, default capacity 32; flag thresholds default to bytes with an optional word profile; DMA thresholds always count words | Authoritative FIFO capacity/unit and flag units; reconcile both register sections and wrapper parameters |
| QSPI-D02 | Undefined: R pp10/12-13 require word DR/DMA transfers but do not define all 1..3-byte tails | TX accepts only remaining DLR bytes from a final word; RX zero-fills unused high lanes; ceil(RX bytes/4) counts readable DMA words. Threshold encodings >=8 cannot fit a full burst in the default 32-byte FIFO | Packing, partial-word consumption, padding and DMA tail completion; avoid a driver waiting for an impossible threshold |
| QSPI-D03 | Undefined: R pp12-13 define request/clear conditions but omit full prefill/rearm sequencing and held-clear details | CCR/AR/DLR writes arm TX prefill; clear suppresses request for its sampled cycle; held clear keeps it low; abort/disable disarm. TX uses free words and not-all-bytes-written, without an extra remaining-length >= burst gate | Exact request assertion/rearm edges, minimum clear/deassert interval and interaction with DMA service sizing |
| QSPI-D04 | Undefined/integration: W pp3-4 assigns reset domains but does not define isolated reset ownership, assertion/release details or all cross-domain in-flight effects | Asynchronous core reset clears controller state; PRESET cancels AP callers; HRESET cancels XIP; isolated bus resets preserve register configuration | Per-domain reset matrix, retention, release synchronization and in-flight request behavior |
| QSPI-D05 | Undefined: R p4 defines EN but gives no complete disable-state/retention table, unlike explicit ABORT behavior | EN=0 cancels work and flushes FIFOs/DMA state while retaining configuration and sticky flags; does not invent an ABORT completion event | Disable versus abort semantics, frame closure, retained RX data and flag handling |
| QSPI-D06 | Undefined: W p5 exposes HWRITE/HWDATA but describes mapped access without a write command/programming contract | Reject all mapped writes with TLM command error; allow aligned single-word reads | Whether mapped writes are unsupported, ignored or mapped to flash operations, and required error response |
| QSPI-D07 | Naming inconsistency and undefined corners: CR[2] is PTCEN on R pp2-4 but TCEN in later prose; empty polling masks and simultaneous match/timeout precedence are not explicit | One enable at CR[2]; empty AND matches, empty OR does not; APMS match wins over a simultaneous timeout; PTR=0 is eligible at the first timeout evaluation | Canonical bit name, zero-mask/zero-timeout rules and simultaneous stop priority |
| QSPI-D08 | Undefined: R pp7-10 define phase encodings/triggers but not completely empty frames, no-data polling or truncation when AR exceeds ADSIZE | Reject empty frames, polling without data, mapped reads without address/data phases and an address too wide for ADSIZE; enforce explicit FSIZE bounds separately | Legal phase combinations and address reject-versus-truncate behavior; enumerate illegal configurations |
| QSPI-D09 | VP contract: R p2/W pp4-5 specify word accesses but not malformed TLM payloads, debug or RO write diagnostics | Little-endian aligned four-byte accesses; accept absent or all-enabled byte enables, reject partial lanes; ignore RO writes, mask reserved bits; non-destructive read-only AP debug; no DMI | Hardware illegal-access policy and approved TLM error mapping, independent of pin-level PSLVERR/HRESP timing |
| QSPI-D10 | VP abstraction: W pp3-4 specify CDC and APB/AHB asynchronous/command FIFO depths; exact CDC latency, queue arbitration and timeout cleanup are not defined | Separate PCLK/HCLK admission and clocked core service, serialized per target; bus-edge timeout and reset epochs cancel queued callers; no asynchronous/command FIFO occupancy model | CDC/queue contract, reset/timeout cleanup and whether an already-issued flash operation can have committed effects |
| QSPI-D11 | VP abstraction: R pp2/4/7-11 and W pp5-6 define serial phases/pins; functional descriptors do not establish waveform setup/hold or pad behavior | Track phase widths, SDR/DDR metadata and minimum CSHT/PIR; permit scheduling gaps; backend transactions replace SCLK/IO/OE waveforms | Approved timing abstraction plus separate RTL/pad verification for waveform compliance |
| QSPI-D12 | Undefined/integration: W p3 exposes configurable bus widths and PARA_SLAVE_NUM, but R has no flash-selection CSR contract | 32-bit AP/XIP endpoints and one flash backend; configurable byte FIFO capacities within the model's range | Legal non-default widths, multi-flash selection/address mapping and depth units |
| QSPI-D13 | External dependency: R/W specify controller transport, not a particular flash device's command/latency/QE/continuous-read behavior | Generic NOR backend is a support profile with its own command checks, program/erase and persistence limitations | Approved flash part/datasheet, opcodes, timing, reset/persistence and SIOO mode bits; generic flash tests do not add controller requirements |
| QSPI-D14 | Document/integration gap: R p2 leaves the base to integration; R lacks a version history tying it to W v0.1 | Pin the two hashes; use existing FU3 AP/XIP routing through platform configuration | Approved document pair and SoC address/IRQ/DMA mapping |

Prioritize QSPI-D01 through QSPI-D04 for DMA/FIFO and reset integration.
QSPI-D02/D03 must also be reconciled with the DMA endpoint service contract:
the request threshold is in words, whereas DMA burst limits are in bytes.
Changing a default to make one test pass would not resolve the source conflict.

## Regression evidence and closure

CTest names use the prefix `fu3_qspi_`. Tests check explicit register behavior,
documented policies and the reusable flash support profile; they are not a
per-requirement HDS sign-off report.

| Decisions / functionality | Existing test suffixes | Evidence boundary |
| --- | --- | --- |
| FIFO units/tails; D01-D02 | `fifo`, `threshold_words`, `p0_fifo`, `p0_dlr`, `p1_dma`, `p1_parameters_*` | Both selected threshold profiles and bounded tails; no proof of which unit the hardware owner intended |
| DMA requests; D03 | `dma`, `p1_dma` | Request/clear and prefill/tail policies; not a complete ADC/AES/DMA/CPU system test |
| Reset/disable/domains; D04-D05/D10 | `reset`, `clock`, `concurrency`, `p0_active_reset`, `p1_domains` | VP cancellation/retention and clock gating; no RTL CDC proof |
| Mapped/protocol/invalid config; D06/D08-D09 | `xip`, `protocol`, `errors`, `p0_triggers`, `p0_bus_errors` | Endpoint errors and chosen rejection rules, not AHB/APB wire responses |
| Polling/flag behavior; D07 | `polling`, `timeout`, `p1_polling`, `p0_irq_w1c` | Explicit polling behavior plus chosen empty-mask/priority cases |
| Serial phases and parameters; D11-D12 | `phases`, `spec_timing`, `p1_parameters_*` | Phase metadata, minimum timing and model capacities, not multi-flash/waveform equivalence |
| Generic flash and platform composition; D13-D14 | `flash_commands`, `flash_program`, `flash_erase`, `flash_protocol`, `flash_reset`, `flash_image`, `flash_sioo`, `flash_integrity`, `bus` | Generic NOR profile and existing FU3 bus composition; no CPU instruction execution |

The 2026-10-19 Debug rebuild and rerun passed **44/44 tests**, with no failures
or disabled tests. Evidence is in `build/hds-review/qspi-results.xml` and
`qspi-regression.log` from the repository root. This review did not rerun
UBSan. See [testing.md](testing.md) for scenario details and
[registers.md](registers.md) for the programming interface.

Close a decision only after recording an approved source revision/section,
reconciling related README/register guidance and updating affected tests.
All current VP choices remain provisional where the source is open; full HDS
and RTL sign-off are not claimed.
