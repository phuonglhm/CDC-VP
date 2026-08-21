# TPU_V3 Phase 9 NoC rebaseline

Date: 2026-08-20

Status: **the five freezes below are complete.** Ratified as decision record
D23. Phase 9 implementation may begin.

Plan §16 Phase 9 opens with a mandatory rebaseline: before any implementation,
five things must be frozen against the pinned FlooNoC RTL and FlooGen
configuration, and the selected transport alternative must be recorded in the
decision record with its evidence. "No implementation choice may be inferred
from the architecture diagram alone."

This document is that evidence. It changes no code.

---

## 1. The evidence base

| Source | Where | Revision |
| --- | --- | --- |
| FlooNoC RTL and FlooGen | `~/Documents/work/Study_FlooNoC/FlooNoC` | `9a6972a` (`v0.8.4-10-g9a6972a`), the frozen revision named in `components/floo_noc_model/PROVENANCE.md` |
| The SystemC model | `components/floo_noc_model` | signed v1.4, `docs/STATUS.md` |
| The address map | `components/TPU_V3/common/include/tpu_v3/address_map.h` | — |

Every number below was read out of one of those, and each is cited where it is
used. Nothing here is taken from a diagram.

## 2. Freeze 1 — traffic classification

**Classification is by address, and the address map already carries it.**
`address_map::region_kind` has exactly two values, `mmio` and `memory`, and
`region_kind_is_memory()` is already what the platform translates into
`noc_interconnect::target_kind`. The traffic class is that same bit; a second
classifier would be a second source for one fact.

| Class | Regions |
| --- | --- |
| **control** | `GLOBAL_CONTROL`, `CHIP_CONTROL(chip)`, `CHIP_COUNTERS(chip)`, and per core `CORE_CONTROL`, `SA_CONTROL`, `DMA_CONTROL`, `TRANSFORM_CONTROL`, `CORE_COUNTERS` |
| **data** | `GLOBAL_BOOT_ROM`, `GLOBAL_RAM`, `CORE_SRAM(chip, core)` |

### The rule in full, including the addresses that are in no region

A table of regions is not yet a total function, and the plan requires the
classification to be deterministic for **error traffic** too (§16, freeze 1). An
unmapped address, a transfer straddling two regions and an access to an
uninstantiated chip all make `address_map::find_region()` return `nullptr`, so a
rule that only reads a region's `region_kind` produces no class for exactly the
accesses that are going to fail. Stated as three ordered cases, which is total:

1. a region **contains** `[address, length)` → that region's `region_kind`;
2. otherwise a region contains the **first byte** → that region's `region_kind`.
   This is the straddling case, and taking the first byte makes it deterministic
   without inventing a third class;
3. otherwise → **control**.

Case 3 is the honest default rather than an arbitrary one. An access to an
address nothing backs returns a status and no payload, so it cannot be bulk
data by construction; and treating a stream of bad addresses as data would let a
firmware pointer bug be accounted as tensor bandwidth, which is the one reading
that would actively mislead.

**Cases 2 and 3 are refused before injection on every path TPU_V3 has today** —
`neo_hart_port`, `neo_external_bridge` and `chip_local_fabric` each refuse a
straddle and an out-of-aperture address at their own boundary, and
`noc_interconnect` itself decodes before it injects: `b_transport` returns
`TLM_ADDRESS_ERROR_RESPONSE` locally when nothing is mapped or when the transfer
runs off the end of its region into another
(`floo_noc_model/src/noc_interconnect.cpp:1721`). So for those the class exists
for **attribution**, not for routing: there is no transport event to route.

**What a Phase 9 error test should expect, because it is not what the AXI names
suggest.** The two error classes divide by *where they are answered*, not by
severity:

| | Answered | TLM response | AXI |
| --- | --- | --- | --- |
| unmapped, straddling, oversized | **before injection**, locally | `TLM_ADDRESS_ERROR_RESPONSE` / `TLM_BURST_ERROR_RESPONSE` | — no flit exists |
| a target that reached and refused | **after injection**, by the subordinate | `TLM_GENERIC_ERROR_RESPONSE` | `SLVERR` |

`DECERR` therefore does **not** round-trip in this model. It exists in
`perform_downstream_access()` (`noc_interconnect.cpp:1032`) as defence in depth
for a subordinate-side decode miss, but the pre-injection check above means an
ordinary `b_transport` cannot reach it; and every non-OK target response after
injection is mapped to `SLVERR`, never `DECERR`
(`noc_interconnect.cpp:1112`). An earlier draft of this section described a
`DECERR` round trip, which would have sent Phase 9's error tests looking for a
response the interconnect cannot produce.

The class of the one error that *does* traverse the network — `SLVERR` — is case
1 or 2, because a request that reached a target had decoded at its own endpoint.
It inherits that class by the rule below.

**Responses and error traffic take the class of their request**, and the class
is **endpoint-local metadata that the transport never sees**.

That distinction matters enough to state precisely, because the obvious wrong
version of it would quietly require a transport change. The signed
`flit_header` (`floo_noc_model/include/floo_noc_model/floo_types.hpp:135`)
carries `rob_req`, `rob_idx`, `dst_id`, `src_id`, `collective_mask`, `last`,
`atop`, `axi_ch` and `collective_op` — and **no traffic-class field**. There is
nowhere in the frozen single-AXI flit to put one, so a design that needed the
class in flight would need a new flit format and a new sign-off, contradicting
§3.

It does not need one. The endpoint keeps the class in its own per-request
record, and matches a response to that record by the ordering the frozen
configuration already enforces.

**That ordering is per channel, not total, and the record keeping has to match
it.** `MaxUniqueIds = 1` with `NoRoB` makes the chimney's response metadata a
plain in-order FIFO, and AXI's B and R are separate channels: a write response
and a read response outstanding on the same port complete independently. The
wrapper says so in as many words — "Read completions remain FIFO among reads and
write completions FIFO among writes" (`noc_interconnect.h:235`), over
independent B and R completion queues.

The rule is therefore **two records per port, not one**: a write record queue
matched to B, a read record queue matched to R, each FIFO within itself. A
response takes the class at the head of **its own channel's** queue. A single
head-of-request record would attribute a read's class to a write whenever both
are in flight, which is the ordinary case for a core whose hart is fetching
while its DMA is writing.

The class is therefore retained, never re-derived from a response, and never
transported.

**The consequence, stated here rather than discovered in Phase 11:** because the
network does not carry the class, it **cannot prioritise by it**. No arbitration
decision anywhere in the mesh can distinguish a control access from a bulk data
beat. This is the same fact §5 records as accepted head-of-line blocking, seen
from the other end, and it is not something an endpoint can mitigate — only a
transport change can.

**Deterministic at the NEO-CORE/NoC boundary**, as the plan requires: the class
is a function of the address alone, computed by the same `constexpr` code that
computes the decode, so the boundary cannot disagree with the decoder.

Two rejected alternatives, and why:

**Source-based classification is wrong here.** The NEO DMA is the bulk mover but
it is not the only bulk mover, and it is not exclusively one: firmware programs
engines through MMIO from the hart, and the hart's own reset PC is in boot ROM.
A rule keyed on "DMA means data, CPU means control" would misclassify every
engine start write and every instruction fetch.

**Opcode-based classification is wrong here.** Read versus write says nothing
about the class — a control register is read and written, and so is memory.

One consequence recorded rather than left to be discovered: **instruction fetch
is data-class by this rule.** It reads `GLOBAL_BOOT_ROM`, which is `memory`.
That is latency-critical, low-bandwidth traffic sharing a class with bulk tensor
movement. It is not reclassified — classification stays a function of the
address — but §5 states what it costs on a shared network.

## 3. Freeze 2 — transport structure

The plan names three alternatives. Their status at the pinned revision:

| Alternative | Status at `9a6972a` | Data path |
| --- | --- | --- |
| **Shared single-AXI network** | live: `hw/floo_axi_router.sv`, `hw/floo_axi_chimney.sv`; `floogen/examples/axi_mesh_xy.yml` sets `network_type: "axi"` with one protocol at `data_width: 64`. It is what the signed CDC-VP model implements | 64-bit |
| **Control/data virtual channels** | **deprecated.** `floo_vc_router.sv` and `floo_nw_vc_chimney.sv` live under `hw/deprecated/`, compiled only by the separate `floo_deprecated_hw` Bender target (`Bender.yml:118`) | — |
| **Separate narrow-control and wide-data networks** | live: `hw/floo_nw_router.sv`, `hw/floo_nw_chimney.sv`, `hw/floo_nw_join.sv`; `floogen/examples/nw_mesh_xy.yml` sets `network_type: "narrow-wide"` with narrow `data_width: 64` and wide `data_width: 512` | 64-bit + 512-bit |

### Selected: keep the shared single-AXI network, unchanged

Three reasons, in order of how much they settle:

**1. The VC alternative does not exist at this revision.** It is not a matter of
cost — the RTL is deprecated and out of the default build. The plan warned that
"a throughput requirement for a wider data path cannot be closed by adding one
VC alone"; at `9a6972a` the sharper statement is that a VC cannot be added at
all without leaving the frozen revision.

Worth stating precisely, because the word "virtual channel" does appear in live
RTL: `floo_nw_router.sv:91` sets
`NumWideVirtChannels = (WideRwDecouple == None) ? 1 : 2`, and the narrow channel
is instantiated with `NumVirtChannels: 1` (lines 176, 215). So the only
non-deprecated VC use at this revision is **read/write decoupling on the wide
channel of the narrow-wide network** — which is available only to a design that
has already taken narrow-wide. There is no VC option independent of the
transport choice.

**2. There is no throughput requirement for the 64-bit path to fail.** Searched
for and not found: neither the plan nor the decision record states a bandwidth,
throughput or NoC-latency target anywhere. The only occurrence of the phrase in
the plan is the conditional sentence in this very rebaseline section — "a
throughput requirement for a wider data path cannot be closed by adding one VC
alone" — which describes what to do *if* one exists. Phase 11 specifies metrics
to measure and no threshold to meet. A transport change justified by a
requirement nobody has written down would be a change justified by a guess.

(The plan does say elsewhere that a package "does not model target TPU pipeline
timing, memory bandwidth or NoC latency and must not publish such numbers", but
that sentence is scoped to the Phase 4.5 compiler-enablement handoff and is not
evidence about TPU_V3 as a whole. It is named here so that a later reader does
not press it into that service.)

**3. Narrow-wide is a new signed configuration, and the plan says the v0
evidence cannot be inherited.** CDC-VP's model states its own boundary in as
many words — "Not modelled: ATOPs, virtual channels, multicast/collectives, and
the narrow-wide network" (`noc_interconnect.h`). Taking it means modelling three
further RTL modules and re-running block-level sign-off, a project comparable to
the original NoC model effort.

### The condition that reopens this

A **stated** bandwidth requirement, or a measured workload in which 64-bit NoC
time dominates. If either arrives, the answer is **narrow-wide, not VCs**, on
the evidence above. Reopening it means a new signed FlooNoC configuration and
its own cross-check campaign; it is not a parameter change.

## 4. Freeze 3 — widths and adaptation

All measured against the model and its frozen configuration.

| Property | Frozen value |
| --- | --- |
| Network data width | **8 bytes / 64 bit**, one width for control and data alike |
| Address width | 48 bits upstream; every TPU_V3 address is below 4 GiB (`ADDRESS_MAP.md` rule 1) |
| Beats per burst | ≤ **256** — `AxLEN` is 8 bits and encodes `beats - 1` |
| Maximum frame | **2048 bytes of beat frame**, not of payload |
| Beat count | `ceil((address % 8 + length) / 8)`, so a transfer at lane offset `+k` reaches the limit at `2048 - k` bytes |
| Width conversion | **none.** There is one width; a narrower access uses `WSTRB`, including non-contiguous patterns |
| Widened reads | refused at a `target_kind::mmio` region, permitted at `memory`. Narrow aligned reads of 1/2/4/8 bytes are never widened |

**An oversized payload is refused, never split.** `TLM_BURST_ERROR_RESPONSE`,
before anything is injected, with the target never called. The reason is
`MaxUniqueIds = 1`: splitting needs an ordering rule between the pieces and a
rule for combining their responses, and neither is free to choose under an
in-order response contract.

**Chunking is therefore the endpoint's job, in both directions**, and it is a
Phase 9 deliverable rather than an interconnect property. Its contract must
define alignment, ordering, partial-failure behaviour, final completion
semantics and metrics attribution — the five points plan §9.3 already lists. A
read is symmetric: N requested bytes return the same beat count, so the same
limit governs the response.

## 5. Freeze 4 — protocol behaviour

| Property | Frozen value | Evidence |
| --- | --- | --- |
| Routing | XY, dimension-ordered | `floo_pkg.sv:359`, `RouteDefaultCfg.RouteAlgo = XYRouting` |
| Arbitration | wormhole arbiter over a rotating-priority `rr_arb_tree`; a granted route holds its output until the tail flit | `floo_output_arbiter.sv:69`, model P3/P5 signed |
| Ordering domain | one AXI ID per upstream port; responses return **in request order within each response channel** — reads FIFO among reads, writes FIFO among writes, over independent B and R queues. There is **no** total order between a read and a write outstanding together | `ChimneyDefaultCfg.MaxUniqueIds = 1` (`floo_pkg.sv:346`); `noc_interconnect.h:235` |
| Reorder buffers | none — `BRoBType`/`RRoBType` are `NoRoB` | `floo_pkg.sv:348,350` |
| Outstanding | ≤ **32** per upstream port, hard bound | `ChimneyDefaultCfg.MaxTxns = 32`, `floo_pkg.sv:345` |
| Back-pressure | ready/valid with router input FIFOs; no credit scheme in this configuration | model P3, signed |
| Physical structure | separate `req` and `rsp` meshes over the same coordinates | model P9, signed |
| Response ownership | per-port completion queues; `last_latency_cycles(port)` is the attributable form, the unqualified one is not | `noc_interconnect.h` |
| Manager identity | 3-bit chimney manager id → **at most 8 upstream initiators**, one per chip (D2) | constructor |

**Head-of-line blocking is expected and accepted.** One channel per mesh plus
wormhole arbitration means a long data burst can hold an output while a control
access waits behind it. That is the direct, deliberate consequence of §3's
choice, and it is recorded here so that a Phase 11 contention measurement reads
as a known property rather than as a discovery.

**Deadlock freedom** rests on two independent things, and both must survive any
future change: XY routing is dimension-ordered, and requests and responses ride
**separate physical meshes**, so the request-to-response dependency cannot close
a cycle.

**Reset of in-flight traffic is the one item that is frozen as a rule rather
than as an existing behaviour.** `noc_interconnect` exposes no reset entry
point. Phase 8's `tpu_chip::reset()` abandons queued work inside the chip and
leaves whatever the network holds. The frozen rule for Phase 9:

* a chip reset **abandons the chip's own queued and in-flight NoC work at the
  endpoint**, exactly as `chip_local_fabric::reset()` does at its ports, and
  reports it as an error to the abandoned initiator rather than completing it;
* it **does not** reset the mesh, which is shared with other chips and is not
  this chip's to reset;
* a transaction already inside a downstream `b_transport()` keeps its port and
  releases it on unwind. This is the Phase 8 finding restated at the next level
  up: a reset cannot unwind a blocked C++ call, so it must not pretend to;
* a transaction **waiting out its own quantum** before it may contend is a third
  population, registered nowhere — not queued on an arbiter, not downstream —
  and a reset must still reach it. That means an interruptible timed wait, not a
  bare `wait(delay)`: the bare form returns only when the quantum expires, which
  holds the caller for up to a full TLM global quantum after the reset;
* and an abandonment that interrupts such a wait must **return the unelapsed
  remainder in `delay`**. A caller's logical time is `sc_time_stamp() + delay`
  and a decoupled initiator *sets* its keeper from what comes back
  (`common/mem.h:105`), so returning zero moves it into its own past — a 3 µs
  quantum cut at 500 ns loses 2.5 µs. Both Revision 1 arbiters do all of this
  and both gate it, including the rollback, which a promptness check alone does
  not catch.

Implementing and gating that is a Phase 9 task.

## 6. Freeze 5 — RTL verification impact

**The FlooNoC configuration is unchanged, so the v0 sign-off carries.** No new
signed configuration is created, which is the practical payoff of §3.

What Phase 9 adds is above the signed boundary — in the wrapper and the chip
endpoint — and needs model-level evidence:

| Work | Required evidence |
| --- | --- |
| D1 owner-aware local bypass | a negative control proving the bypass path is taken **and that no flit is injected**; a missing or wrong owner mapping must fail at elaboration, not at first traffic |
| Local containment | local SRAM/MMIO traffic injects **zero** flits — asserted on the mesh counters, not inferred from a latency |
| Burst chunking | over-frame transfers chunked with defined partial-failure semantics, in both directions |
| Multi-chip contention | 2x2 concurrent inter-chip traffic completing without deadlock or response misattribution |
| Conservation | bytes and transactions reconcile across the endpoint boundary, as `test_neo_external_bridge` does at the core boundary |
| Reset | the §5 rule, with its own negative control |

Anything that *does* change the FlooNoC configuration — narrow-wide, a VC, a
ninth manager, `MaxUniqueIds > 1`, `NoLoopback = 0` — is a new signed
configuration and re-enters this document at §3.

## 7. D1 is not implemented, and the schedule in the documents disagreed

Measured, not assumed:

* `noc_interconnect::add_target(base, size, where, kind)` has **no owner
  parameter**;
* `reject_self_node_targets()` (`src/noc_interconnect.cpp:430`) still refuses
  **any** mapped target on a node hosting **any** upstream port, unconditionally.

So chip-to-chip traffic is blocked exactly as `TPU_V3_PHASE0_AUDIT.md` §5.1
described, and D1 is the first Phase 9 implementation task.

On the schedule: the Phase 0 audit scheduled D1 as "a **Phase 7 prerequisite**,
not a Phase 8 discovery", while the README and plan §16 both call it a Phase 9
prerequisite. Phase 7 was one chip plus global memory, which does not need it —
the audit itself says so in the same section — and Phase 8 composed two cores
inside one chip, which also does not. The Phase 0 audit is historical evidence
and is not rewritten; **the Phase 9 scheduling in the README and plan is
correct** and this note records the supersession, on the same terms as P0-6,
P0-7 and P0-9.

## 8. Summary

| Freeze | Outcome |
| --- | --- |
| 1. Traffic classification | by address, from `region_kind`, as a total three-case rule so an unmapped, straddling or uninstantiated-chip access still has a class (`control`). Responses and errors take their request's class from endpoint-local metadata, held as **separate read and write record queues** because B and R complete independently. The transport carries no class field and therefore cannot prioritise by it |
| 2. Transport structure | **shared single-AXI, unchanged.** VCs are deprecated at the pinned revision; narrow-wide is live but answers a requirement nobody has stated |
| 3. Widths and adaptation | 64-bit, ≤256 beats, 2048-byte frame, no width conversion, oversized refused; chunking belongs to the endpoint |
| 4. Protocol behaviour | XY, wormhole rotating priority, in-order **per response channel** (not a total order per port), `MaxTxns = 32`, separate req/rsp meshes; HOL blocking accepted; reset rule frozen and to be implemented |
| 5. Verification impact | no new signed configuration; six model-level evidence items listed in §6 |

Open, and deliberately so: the reset-of-in-flight rule is frozen as a
contract and not yet implemented, and D1 is not implemented. Both are Phase 9
tasks and both are listed above with the evidence they owe.
