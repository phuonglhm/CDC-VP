# FU3 bus model contract

Reconstructed on 2026-10-17 from the current implementation, tests, README and
recorded project decisions. This document describes the functional SystemC/TLM
contract; it does not certify signal-level AXI/APB or cycle-accurate RTL behavior.

## Scope and evidence

The component models SYSBUS_1, SYSBUS_0, both A2P bridges and PERIBUS_0/1 for
software-visible behavior and VP data flow. CPU execution, peripheral registers,
interrupt controllers and boot sequencing are supplied by the platform.

| Topic | Basis | Implemented contract |
|---|---|---|
| Topology | HAS v2.0 Figures 4-1 and 7-1; user resolution | Both A2P bridges are downstream of SYSBUS_0; SYSBUS_1 reaches SYSBUS_0. |
| Address map | HAS pages 46-47 | Preserve published addresses despite the conflicting PERIBUS_1 SYS column. |
| Interface limits | HAS Table 7-1, with VP interpretations below | 32-bit address/data interfaces and bounded admission. |
| Strobe | User clarification | Full byte-write-strobe support includes partial and sparse writes. |
| APB version | User clarification | Both peripheral buses use APB4. |
| Arbitration | User decision where the HAS gives no priority scheme | FIFO at each contention point. |
| Access restrictions | HAS page 44 | ROM restriction, staging protection and non-executable DSRAM. |
| CPU security | HAS pages 40-41 and separate Common Spec | Full PMP/ePMP and permission matrix remain outside this bus model. |

The source documents remain private. Page references above retain the previously
recorded review references; this documentation restoration is not a new PDF audit.

## Elaboration and routing

`BusConfig::fu3()` defines four masters (`CVA6`, `IBEX`, `DBG`, `DMA0`) and 36
enabled physical windows. The maintained map is in [config.cpp](../src/config.cpp).
An independent expected map is in [bus_contract_tests.cpp](../tests/bus_contract_tests.cpp).
Regions are half-open intervals `[base, base + size)`.

Configure and disable unused targets before constructing `BusSystem`. Every
enabled target must be bound before simulation; unused initiators may remain
unbound. Configuration is fixed during elaboration. Invalid widths, duplicate
names, invalid paths, zero capacities and overlapping enabled regions are rejected.
Intermediate routing uses global addresses; leaf routing supplies local offsets.
Disabled windows are unmapped.

## Functional transactions

Call blocking transport from an `SC_THREAD`. Default FU3 masters require an
`Access` extension, normally attached with `ScopedAccess`. It identifies beat
size, address progression, instruction fetch and ordering stream.

- Beats are naturally aligned 1, 2 or 4 bytes; payload length is a nonzero
  multiple of the beat size. Streaming width must cover the payload.
- Incrementing requests advance addresses. Fixed requests keep the address and
  advance payload/byte-enable positions. Both are dispatched as single operations;
  this does not establish hardware AXI FIXED burst support.
- DMA accepts at most 16 beats. CPU/debug `max_beats = 256` is a configurable
  VP guard. DBG accepts only word-sized incrementing requests.
- The whole address footprint is checked before dispatch. Cross-window requests
  fail before target side effects. A downstream failure stops remaining beats;
  completed beats are not rolled back.
- Byte-enable patterns repeat over payload bytes and are preserved across beats.
  There is no generic read-modify-write operation on registers.
- Payload address, pointers, lengths and streaming fields are restored on return,
  including exception paths. Exceptions release queue and admission ownership.
- WRAP, atomics, streaming wrap, nonblocking transport and DMI are not implemented.

Custom initiators default to `require_access = false`. Their metadata-free requests
are legacy linear TLM blocks, not CPU/AXI transactions. Legacy A2P accepts aligned
1/2-byte requests or word-aligned blocks of 4 through 64 bytes. Use explicit
metadata for FU3 CPU/DMA integration.

## Width adaptation and endpoint responsibility

`word_interface` is enabled for ROM, CVA6_ISRAM, IBEX_ISRAM, DSRAM, AES, QSPI_XIP,
HYPERBUS and AP_PLIC. Narrow accesses become aligned four-byte operations with lane
enables; read results are extracted only for enabled lanes. Other targets receive
the original beat size. QSPI receives single operations with capacity one.

This is the implemented VP interpretation of `Size = 32`, not a blanket guarantee
that all register blocks support safe narrow reads. An IP with read-clear, FIFO
or other side effects must explicitly support the adapted operation. TLM read byte
enables are not APB PSTRB signals. Resolve such endpoint contracts before relying
on narrow register reads in firmware.

Timer0/1/2/3 are a documented exception to general strobe support: the Timer HDS
excludes partial writes. Their VP adapter accepts aligned full-word accesses and
rejects partial requests; the bus preserves strobes and does not emulate them by
read-modify-write. See the [Timer contract](../../timer/docs/MODEL_CONTRACT.md).

## Admission and ordering

| Resource | Default capacity | Interpretation |
|---|---|---|
| Each CPU | 2 total reads/writes | Conservative combined interpretation; the HAS does not resolve per-direction capacity. |
| DBG | 1 | Serialized admission. |
| DMA0 | 4 reads plus 4 writes | Separate direction gates. |
| SRAM and AES targets | 4 | Bounded concurrent downstream operations. |
| ROM and QSPI targets | 1 | Serialized downstream operations. |
| HyperBus and EDHOC targets | 1 | Configurable VP defaults for unspecified table entries. |
| APB paths | 1 | Serialized bridge/peripheral access. |

Excess requests wait FIFO. An admission slot spans the whole request, including
its wait for ordering. Requests sharing master, direction and `ordering_id` are
serialized for their entire duration. Distinct IDs can progress independently;
ID zero is the default. This conservative ordering token is not a complete AXI ID
or independent-channel model.

Dispatch is FIFO per beat. Simultaneous arrivals use SystemC enqueue order;
independent outputs have no global completion order. Master AP stages and AP_SB0
release dispatch ownership before downstream completion. Leaf occupancy has a
separate limit. Slow targets therefore do not retain shared dispatch ownership,
although occupied master/target capacities can still backpressure their callers.
AP_PP0/1 and A2P remain serialized. No target timeout is invented.

## Timing and fairness

Default timing parameters are `axi_cycle_ns = 2`, `apb_cycle_ns = 10` and
`router_latency_ns = 2`. A2P adds setup plus access time (two APB cycles per beat),
in addition to other path stages and target latency. These are functional VP
parameters, not confirmed chip frequencies or throughput guarantees.

Input and downstream annotated delays are consumed once; the returned delay is
zero. Functional ingress yields to the kernel. Validation/policy rejection consumes
router latency so a repeated error loop does not starve a timed peer. With all
latencies zero, fairness applies to runnable processes; an infinite delta loop
cannot advance physical simulation time. `peak_outstanding()` measures admission,
not by itself downstream overlap.

## Boot policy, debug and reset

- ROM permits functional reads only from IBEX; no functional writes. Denial for
  DBG/DMA is a conservative choice pending the complete permission matrix.
- DSRAM rejects instruction fetches. CPU adapters must label fetches correctly.
- IBEX may write CVA6_ISRAM during staging. Stop and drain IBEX traffic before
  calling `finish_staging()` and releasing CVA6 reset. Pending IBEX traffic causes
  `std::logic_error` without changing the policy.
- `reset()` requires all masters quiescent. Otherwise it throws without policy
  changes. It reopens staging, retains peak counters and does not abort transfers
  or reset external IPs.
- `transport_dbg` is an untimed loader backdoor that bypasses access policy while
  preserving routing/window checks. It does not execute firmware or parse ELF.

Clock/reset wiring, IRQs, peripheral side effects, HyperRAM readiness and the
remaining CPU PMP/ePMP/W^X handoff are platform responsibilities. Passing bus tests
does not establish complete security or software boot correctness.

## Error responses

| Condition | TLM response |
|---|---|
| Unmapped address, cross-window footprint, misalignment | ADDRESS_ERROR |
| Unsupported command, denied policy, instruction write | COMMAND_ERROR |
| Unsupported beat size/count/burst/streaming | BURST_ERROR |
| Invalid byte-enable representation | BYTE_ENABLE_ERROR |
| Missing required metadata, null data, zero length | GENERIC_ERROR |

Downstream target errors propagate. These functional diagnostics do not imply an
exact mapping to hardware AXI response encodings or APB PSLVERR.

See [validation](VALIDATION.md) for evidence and
[repository integration](REPOSITORY_HANDOFF.md) for reproducible usage.
## Optional whole-burst endpoint contract

`TargetConfig::preserve_burst` defaults to false. An integration may explicitly enable it for an AXI endpoint that validates the complete original transaction, such as the HyperRAM VP. In that mode ingress retains `Access`, payload length and byte enables instead of decomposing the request into beats or widening subwords. Region, access-policy, ingress admission and ordering checks still apply. The endpoint must validate its supported beat size, burst kind, alignment and 4-KiB rule. Existing FU3 defaults are unchanged; `fu3_hyperram::Controller::configure_bus` opts HYPERBUS into this contract.

## Integration findings from the 2026-10-20 audit

- Production AP_DMA uses a separate TransferExtension and requires an adapter to
  the mandatory Access contract on default DMA0 ingress. Direct binding currently
  fails; fake-DMA integration tests do not prove that production path.
- AP_PLIC narrow reads can trigger word-wide claim side effects while extracting
  only the requested byte lanes. Safe firmware claim/complete should use aligned
  words pending an explicit enforced endpoint-specific subword policy.
- Optional preserved FIXED requests have inconsistent footprint checks between
  ingress and Router and can be rejected even when their repeated beat fits.
  This is an open API defect, not evidence against the default decomposed FIXED
  path or the HyperRAM endpoint's supported INCR contract.

See VALIDATION.md for fresh test evidence and the distinction between verified
fabric behavior, endpoint interoperability and full-platform reset/boot work.
