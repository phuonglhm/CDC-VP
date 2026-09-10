# TPU_V3 Phase 9 NoC rebaseline

> **HISTORICAL / INACTIVE UNDER D27 — DO NOT IMPLEMENT FROM THIS DOCUMENT.**
> The NoC decisions and evidence are retained for traceability only. The active
> DSE instantiates one standalone NEO-CORE and no NoC. Start with
> [README.md](README.md).

Date: 2026-08-20

Historical status: **the five freezes below were complete.** They were ratified
as decision record D23. D27 now suspends Phase 9 implementation.

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
(`floo_noc_model/src/noc_interconnect.cpp:2054`). So for those the class exists
for **attribution**, not for routing: there is no transport event to route.

**What a Phase 9 error test should expect, because it is not what the AXI names
suggest.** The two error classes divide by *where they are answered*, not by
severity:

| | Answered | TLM response | AXI |
| --- | --- | --- | --- |
| unmapped, straddling, oversized | **before injection**, locally | `TLM_ADDRESS_ERROR_RESPONSE` / `TLM_BURST_ERROR_RESPONSE` | — no flit exists |
| a target that reached and refused | **after injection**, by the subordinate | `TLM_GENERIC_ERROR_RESPONSE` | `SLVERR` |

`DECERR` therefore does **not** round-trip in this model. It exists in
`perform_downstream_access()` (`noc_interconnect.cpp:1144`) as defence in depth
for a subordinate-side decode miss, but the pre-injection check above means an
ordinary `b_transport` cannot reach it; and every non-OK target response after
injection is mapped to `SLVERR`, never `DECERR`
(`noc_interconnect.cpp:1228`). An earlier draft of this section described a
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
| Multi-chip contention | 2x2 concurrent inter-chip traffic completing without deadlock or response misattribution — **closed 2026-08-25**, `test_chip_multi_on_mesh`; see §7a for what it does and does not cover |
| Conservation | bytes and transactions reconcile across the endpoint boundary, as `test_neo_external_bridge` does at the core boundary |
| Reset | the §5 rule, with its own negative control |

Anything that *does* change the FlooNoC configuration — narrow-wide, a VC, a
ninth manager, `MaxUniqueIds > 1`, `NoLoopback = 0` — is a new signed
configuration and re-enters this document at §3.

## 7a. Multi-chip contention — closed 2026-08-25

`components/TPU_V3/tpu_chip/tests/chip/test_chip_multi_on_mesh.cpp`, both NoC
timing backends. Two real `tpu_chip`s, two endpoints, one 2x2 mesh:

```text
(0,0)  chip A endpoint  + chip A aperture (owner 0)
(1,0)  chip B endpoint  + chip B aperture (owner 1)
(0,1)  host             + boot ROM        (owner 2)
(1,1)  global RAM
```

Measured, and every figure below is deterministic — six consecutive runs gave
identical sample counts:

```text
                                   fast        detailed
round 1  chip <-> global RAM       0 samples   53 samples of 100 ns
round 2  chip <-> chip             0 samples   25 samples of 100 ns
accepted flits                     n/a         17478
runtime per case                   0.97 s      1.20 s
```

`fast` overlapping for zero samples is the correct answer, not a gap: nothing
there blocks, so no two managers are ever inside the interconnect at one
instant and there is no queue to contend for. The gate asserts `== 0` there and
`>= 20` in detailed, the same way the single-chip gate asserts in-flight peak
1 fast / 2 detailed.

**Two rounds, because they answer different questions.** Round 1 is two chips
against a shared target — contention on global RAM. Round 2 is traffic
*between the chip apertures*, both directions at once, which is what the §6
wording names and what round 1 does not provide. The first version of this gate
had only round 1 plus a single chip-to-chip transfer run alone after everything
else had drained, and marking §6 closed on that would have been closing it on a
measurement of a different question.

Also checked in both rounds:

```text
4 harts booted through the mesh, mhartid 0..3 in the right chips
2 x 4 KiB concurrent DMA from global RAM, each chip's own window, byte-exact
inter-chip DMA both ways at unaligned sources and odd lengths (3001, 2003),
  past the 2048-byte frame so the path is chunked as well as reshaped
```

### Controls

| Control | Bites | What it establishes |
|---|---|---|
| MC-1 D25 read shaping disabled in `chip_noc_endpoint` | yes — 5 checks, both inter-chip legs error and both deliver wrong bytes | the inter-chip path really traverses D25; the interconnect refuses the widened read into an `mmio` aperture |
| MC-2 both chips given the same RAM window | yes — both directions of the misattribution check fire | the check discriminates rather than passing on any data |
| MC-3 synchronised `START` removed | yes, **in round 2 only** — 18 samples against 25, below the floor | the two-phase start is load-bearing where transfers are short relative to the six register writes that precede them |

**MC-3's answer changed when round 2 was added, and that is the useful part.**
Against round 1 alone it did *not* fire — 49 samples against 53 — because the
shared 2 us RAM latency dominates everything there. Round 2 has no slow shared
resource, so the programming stagger is a large fraction of each transfer and
removing the synchronised release drops it below the floor. A source comment
that had generalised the round-1 result into "a tightening, not the mechanism"
is now stated per round.

The floor separates 18 from 25, which is a real but narrow band. It is safe to
rely on only because the measurement is deterministic; a reader changing the
transfer sizes should re-measure both numbers rather than assume the margin
survives.

## 7. D1 is implemented — closed 2026-08-20

`add_target()` now takes an optional `local_owner`. A target may sit on a node
that hosts an upstream port **iff** it names that port, and an access from that
port is short-circuited to the target socket without a flit ever being created.
The frozen router configuration is untouched: `NoLoopback = 1` stays, and the
tie-off is never exercised rather than reconfigured.

`reject_self_node_targets()` still refuses a co-located target with no owner
mapping, and `add_target()` / `place_initiator()` refuse an owner that does not
exist, an owner on another node, and an owner that tries to move off the node of
what it owns — all before any traffic.

### Evidence

The load-bearing claim is negative — that **no flit existed** — because "the
access returned the right data" is equally true of a routed access. The witness
is `accepted_flits` summed over both physical meshes, every router and every
port, read from the passive RTL-signed counters rather than from anything the
bypass maintains.

| | Baseline (before D1) | After D1 |
| --- | --- | --- |
| `floo_noc_model` component suite | 41/41 | **42/42** |
| RTL cross-checks (12 runners, FlooNoC `9a6972a`) | 12/12 | **12/12** |
| Mutation / negative controls | 51 detected, 0 missed | **55 detected, 0 missed** |
| `noc_soc` platform | 10/10 | **10/10** |

Four D1 mutation controls are registered: `local-bypass-not-owner-aware`,
`local-bypass-counted-as-network-traffic`, `local-owner-node-check-removed` and
`local-owner-may-abandon-its-target`.

**One control is deliberately not in the registry**, and it is the most
convincing one. Disabling the bypass dispatch entirely makes the co-located
access a self-addressed flit and the run **hangs** — `FAIL: the scenario did not
finish` against the bench's watchdog. That is the Phase 0 audit's "this would
hang rather than fail" reproduced as a measurement rather than a prediction. It
is recorded here instead of registered because a control that hangs has to be
killed by a timeout rather than observed, and that harness runs every mutation to
completion.

Two things this cost, worth carrying forward. The first version of
`local_bypass_transport` **copied** the downstream replay out of
`fast_transport` instead of sharing it; because the mutation controls patch by
first textual occurrence, two of them silently began landing in the copy and
stopped detecting anything — 51/0 became 48/3, and nothing else went red,
because losing detection makes no test fail. Both now call one
`replay_downstream()`. And a `noc_soc` control suite failed with eight "does not
build" errors that were really a link against a `libnoc_interconnect.a` nobody
had rebuilt — a failure that looks exactly like a regression and is not.

### The schedule in the documents disagreed

The Phase 0 audit scheduled D1 as "a **Phase 7 prerequisite**, not a Phase 8
discovery", while the README and plan §16 both call it a Phase 9 prerequisite.
Phase 7 was one chip plus global memory, which does not need it — the audit
itself says so in the same section — and Phase 8 composed two cores inside one
chip, which also does not. The Phase 0 audit is historical evidence and is not
rewritten; **the Phase 9 scheduling in the README and plan was correct**, and
this note records the supersession on the same terms as P0-6, P0-7 and P0-9.

## 8. Summary

| Freeze | Outcome |
| --- | --- |
| 1. Traffic classification | by address, from `region_kind`, as a total three-case rule so an unmapped, straddling or uninstantiated-chip access still has a class (`control`). Responses and errors take their request's class from endpoint-local metadata, held as **separate read and write record queues** because B and R complete independently. The transport carries no class field and therefore cannot prioritise by it |
| 2. Transport structure | **shared single-AXI, unchanged.** VCs are deprecated at the pinned revision; narrow-wide is live but answers a requirement nobody has stated |
| 3. Widths and adaptation | 64-bit, ≤256 beats, 2048-byte frame, no width conversion, oversized refused; chunking belongs to the endpoint |
| 4. Protocol behaviour | XY, wormhole rotating priority, in-order **per response channel** (not a total order per port), `MaxTxns = 32`, separate req/rsp meshes; HOL blocking accepted; reset rule frozen and to be implemented |
| 5. Verification impact | no new signed configuration; six model-level evidence items listed in §6 |

Open, and deliberately so: the reset-of-in-flight rule is frozen as a contract
and **not yet implemented** — a Phase 9 task, with the evidence it owes listed
in §5. D1 is now closed; see §7.

The chip NoC endpoint's four metric-accounting findings are **closed** as of
2026-08-22, each with a control that fails when its fix is reverted; the
evidence is `TPU_V3_PHASE9_AUDIT.md` §3 and the history is kept in §9 below.
The §11.10 outstanding and latency requirement is answered by decision record
**D24** and implemented: transfer latency is measured at the endpoint,
outstanding defers to the interconnect, and neither may be quoted as the other
(§9.3).

## 9. The chip NoC endpoint — implemented; the four metric findings are closed

Date of this section: 2026-08-22. **Updated the same day**: the four findings
below were fixed and each was given a control that fails when its fix is
reverted. The findings are kept in place rather than deleted, because the fix
is only meaningful next to what it fixed, and because §9.2's list of what each
one owed is what the controls were written against. `TPU_V3_PHASE9_AUDIT.md`
records the measurement.

`components/TPU_V3/noc_endpoint` exists and is gated by ctest
`tpu_v3_noc_endpoint` (labels `tpu_v3;unit;noc`). It implements the §11.10
bidirectional endpoint: outbound chunking at the §4 frame limit, aperture
containment, single-region span checking before any split, inbound address
rebase, and the **endpoint-local half** of §5's reset rule — a generation guard
that abandons at the next chunk boundary, an interruptible wait for a transfer
still spending its quantum, and the unelapsed remainder returned in `delay`.
The rule end to end at the NoC boundary remains open, as §8 records.

**Its own gate uses plain TLM stubs on both sides**, deliberately — the
split-and-route contract is visible without a mesh, and keeping the component
free of any dependency on `noc_interconnect` is what stops it reaching into the
interconnect for something it should have been told.

Attaching it to a real interconnect happened on 2026-08-22 and 2026-08-24, in
two places and for two different questions:
`test_endpoint_on_real_noc` asks whether the endpoint's read shaping is
sufficient against the thing that enforces the rule (D25), and
`test_chip_on_mesh` composes `tpu_chip`, this endpoint, a real
`noc_interconnect`, boot ROM and global RAM (`TPU_V3_PHASE9_AUDIT.md` §4i).
What remains of the original task is the **platform** composition — the plan
gives system composition to `platforms/TPU_V3_SoC` (§11.11) — and mesh scaling
to 2x2 and beyond.

The findings below were metric-accounting defects, not transport defects: no
transfer was routed to the wrong place and no data was corrupted by any of them.
They mattered because §11.10 requires the endpoint to "publish outstanding,
latency, bytes, and error counters", and because Phase 11 contention measurement
would have read these numbers as if they were true.

**All four are now fixed.** Each subsection keeps its diagnosis and marks what
landed.

### 9.1 The four findings, as diagnosed

| # | Symptom (before the fix) | Where | Class |
| --- | --- | --- | --- |
| E-1 | bytes are published only when the whole transfer returns, while chunks are published per chunk | `chip_noc_endpoint.cpp`, `chip_b_transport` / `noc_b_transport` (`outbound_bytes_ +=`, `inbound_bytes_ +=`) | conservation |
| E-2 | a downstream target error that moved zero bytes increments **no** counter in either direction | same two functions, the `status != TLM_OK_RESPONSE && moved > 0` guard | error accounting |
| E-3 | `inbound_partial_failures_` does not exist; inbound partial failures are countable nowhere | `chip_noc_endpoint.h` counter block | error accounting |
| E-4 | `report()` prints no inbound partial/error information at all | `chip_noc_endpoint::report()` | observability |

### E-1 — publish bytes as each chunk commits

`forward_chunked()` increments `outbound_chunks_` / `inbound_chunks_` inside the
loop, immediately after each `b_transport()` returns and after the generation
re-check. The byte total is not published there: `moved` is accumulated in a
local and added to the member counter only after the whole transfer unwinds.

Two consequences, both real:

* a five-chunk transfer blocked in chunk three reports "2 chunks, 0 bytes" to
  anything that reads the counters while it is suspended — `report()` from a
  platform monitor, or the accessors from a test harness driving traffic from
  another process;
* a simulation that ends with a transfer in flight loses those bytes entirely,
  which breaks endpoint-boundary conservation — the §6 evidence item that must
  reconcile the way `test_neo_external_bridge` does at the core boundary.

**It also makes a comment false.** `forward_chunked()`'s reset path says *"what
has already been transferred stays transferred and is reported (D23)"*. The
first half is true — there is no rollback. The second half is not: the caller's
generation guard deliberately skips the byte accounting for an old-epoch
transfer, so those bytes are never reported anywhere. Accounting per chunk makes
the comment true, because the bytes then land in the epoch that moved them and
`reset()` clears that epoch's counters itself.

**Fix, landed 2026-08-22:** the member byte counter is incremented where the
chunk counter is, `moved` stays as the local the caller still needs for
`last_partial_bytes_`, and both callers stopped accumulating. The existing
generation guard did not have to move — it already sits above that point — so
the invariant the gate asserts ("an old-epoch chunk repopulated counters the
reset had cleared", tested against `outbound_transfers()`, `outbound_chunks()`
and `outbound_bytes()` all being zero after an in-flight reset) is preserved
unchanged and still passes.

### E-2 — record target failures even when no bytes moved

An earlier round correctly stopped classifying a zero-progress failure as a
*partial* completion: an unsplit over-frame MMIO transfer is refused by the
target having moved nothing, and calling that "partial" overwrote
`last_partial_bytes_` with zero. That fix removed the wrong counter and did not
add the right one, and this is the hole it left.

Walk the path: the transfer passes every endpoint check, `forward_chunked()`
issues the first chunk, the target answers with an error, the loop returns that
status, `moved == 0`, and the `moved > 0` guard skips the only accounting site.
`protocol_errors_` counts payload-rule violations only; `outbound_local_refused_`,
`outbound_span_refused_` and `inbound_foreign_refused_` count refusals the
*endpoint* made. Nothing counts a refusal the *target* made.

**It cannot be inferred either.** The residue is `chunks > 0 && bytes == 0` —
which is exactly what a legitimate, fully byte-disabled write also produces.
The two are indistinguishable in the published metrics, so a real error is not
merely uncounted, it is unrecoverable from the report.

**Fix, landed 2026-08-22:** `outbound_target_errors_` and
`inbound_target_errors_`, incremented inside `forward_chunked()` at the failing
chunk and therefore independent of whether anything moved. They are kept
distinct from the `*_refused_` counters: "the endpoint refused this, having
touched nothing" and "the target refused this, possibly after touching
something" are different events with different owners, and merging them
destroys the distinction that makes the refusal counters worth reading.

### E-3 — inbound has no partial-failure count

Outbound has both `outbound_partial_failures_` (a count) and
`last_partial_bytes_` (the most recent value). Inbound has only
`last_inbound_partial_bytes_`. An inbound transfer that fails after committing
bytes therefore increments nothing at all — the value is overwritten, and how
often it happened is not recorded.

**Fix, landed 2026-08-22:** `inbound_partial_failures_`, incremented under the
same `moved > 0` condition the outbound side uses.

### E-4 — `report()` is asymmetric

The outbound line prints splits, partial failures and both refusal causes. The
inbound line prints transfers, chunks, bytes and foreign refusals — nothing
about partial or failed inbound work. `last_inbound_partial_bytes_` is set but
appears in no report; it is reachable only through its accessor from a test.

**Fix, landed 2026-08-22:** both direction lines print partial failures and
target errors. The `last_*` values stay accessor-only — they are "most recent",
not totals — and the report gained one line saying what separates a target
error from a refusal, since the two counters now sit side by side.

### 9.2 Evidence these fixes owe

Under the standing rule that a fix without a control that bites is not evidence,
each of the four owes a control that **fails before the fix and passes after**:

| Finding | Control |
| --- | --- |
| E-1 | park a multi-chunk transfer inside a blocking downstream target, read the counters from another process while it is suspended, and require `bytes` to be consistent with `chunks` — not merely non-zero at the end |
| E-2 | a target that refuses the first (or only) chunk; require the failure to appear in a counter, and require it to be distinguishable from a fully byte-disabled write that legitimately moves zero bytes |
| E-3 | an inbound transfer failing after a committed chunk; require the count, not just the byte value |
| E-4 | assert on `report()` text, since the defect is that the number never reaches the report |

**Controls in TPU_V3 are run by hand and recorded in the phase audit document.**
There is no registered mutation harness under `components/TPU_V3` — the
`run_negative_controls.sh` suite belongs to `components/floo_noc_model` and
covers the RTL-signed model only. A new agent should not go looking for a script
to add these to; it should run them by hand, record the before/after counts, and
say plainly which ones it ran.

**Done 2026-08-22.** All four controls exist in
`noc_endpoint/tests/test_chip_noc_endpoint.cpp`, all four were run by hand
against a mutated tree, and all four failed with the intended message and pass
after the fix. `TPU_V3_PHASE9_AUDIT.md` now exists and §3 quotes each failure.
Two of them needed shapes worth carrying forward: E-1 is unobservable without a
**second process** sampling the counters mid-flight, because after the transfer
returns the two numbers agree either way; and E-2's evidence is the **pairing**
of a zero-progress refusal with a fully byte-disabled write, not the refusal
alone, since a bare refusal check would not notice the two being merged back
together.

### 9.3 A larger §11.10 gap — answered by D24 on 2026-08-22

§11.10 requires "outstanding, latency, bytes, and error counters". Bytes exist.
Error counters became complete with E-2 and E-3. Outstanding and latency did not
exist at the endpoint at all — not incomplete, absent — and this section refused
to let them be built before the ownership question was recorded, because
`noc_interconnect` already publishes per-port completion queues and
`last_latency_cycles(port)` (§5) and an endpoint-level duplicate could easily
become a second, disagreeing source for the same quantity.

**Answered as decision record D24: split by quantity, not by component.**

* **Latency is measured at the endpoint**, per transfer. Only it can: a chunked
  transfer is one transfer to the endpoint and several transactions to the
  interconnect, so `last_latency_cycles(port)` answers a different question.
  Deferring would not have avoided a duplicate, it would have lost a quantity
  nobody else holds.
* **Outstanding is not measured at the endpoint.** In the Revision 1
  composition `neo_external_bridge` arbitrates a core's two outbound initiators
  onto one external socket and `chip_local_fabric` allows one transaction per
  initiator, so an endpoint-level outbound outstanding count is `<= 1` **by
  construction** — a constant printed where a measurement belongs. The figure
  that varies is the interconnect's `MaxTxns` accounting, already published.
* Neither figure may be quoted as, compared with, or summed with the other.

The measured quantity is **logical** time, `sc_time_stamp() + delay`, not an
`sc_time_stamp()` difference: an annotating target consumes no simulated time,
so a stamp difference reads zero on every loosely timed path — the path a
full-system run uses. Implemented and gated; the two controls are D24-a and
D24-b in `TPU_V3_PHASE9_AUDIT.md` §3.
