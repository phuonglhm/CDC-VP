# TPU_V3 Phase 9 Audit — the chip NoC endpoint

> **HISTORICAL / INACTIVE UNDER D27 — DO NOT RESUME PHASE 9.** The statements
> below describe preserved Phase 9 engineering evidence as it existed before
> the 2026-08-25 scope reset. They are not the current status or next-task list.
> The active machine is one standalone NEO-CORE with no chip/NoC dependency.
> Start with [README.md](README.md).

Date of this entry: 2026-08-22

Historical status at the time of writing: **Phase 9 was in progress.** This
file was opened rather than at the end of the phase, because
`TPU_V3_PHASE9_NOC_REBASELINE.md` §9.2 names it as
where the endpoint's negative controls have to be recorded, and there is no
registered mutation harness under `components/TPU_V3` to record them anywhere
else.

What is closed and what is not is stated in §5. Nothing here claims the phase
is finished.

Two things landed on this date: the four metric findings (§2, §3), and decision
record **D24** with its implementation (§4a) — the §11.10 outstanding/latency
requirement the rebaseline had refused to let anyone build before the ownership
question was recorded.

---

## 1. Why this document has to exist at all

`components/floo_noc_model` has `rtl_crosscheck/run_negative_controls.sh`,
which runs every registered mutation to completion and reports detected/missed.
TPU_V3 has no equivalent. Its controls are **run by hand**, and a control that
was run by hand and not written down is indistinguishable from one that was
never run.

So the rule this file follows is the one the Phase 5, 7 and 8 audits already
established: a fix without a control that **fails before it and passes after**
is not evidence, and the failing output is quoted rather than summarised.

## 2. The four metric findings, closed

`TPU_V3_PHASE9_NOC_REBASELINE.md` §9.1 recorded four open findings against
`chip_noc_endpoint`. All four are accounting rather than transport — nothing
was ever mis-routed or corrupted by them — and all four are now fixed.

| # | Was | Is |
| --- | --- | --- |
| E-1 | bytes accumulated into a local and published only when the whole transfer unwound, while chunks were published per chunk | bytes are published with the chunk that moved them, under the same generation guard |
| E-2 | a target refusal that moved no bytes incremented nothing in either direction | `outbound_target_errors_` / `inbound_target_errors_`, incremented at the failing chunk regardless of progress |
| E-3 | inbound had `last_inbound_partial_bytes_` and no count | `inbound_partial_failures_`, matching the outbound counter |
| E-4 | `report()`'s inbound line printed nothing about failed or partial work | both direction lines print partial failures and target errors |

### E-1 — where bytes are published

`forward_chunked()` already incremented `outbound_chunks_` / `inbound_chunks_`
inside the loop. The byte total was not published there; it was accumulated in
`moved` and added to the member counter by the caller after the loop returned.

The consequence is not a rounding difference. For as long as a transfer was
suspended inside a downstream `b_transport()`, the endpoint reported chunks
with **zero** bytes, and a simulation ending with a transfer in flight lost
those bytes entirely — which is endpoint-boundary conservation failing, and
conservation is one of the six §6 evidence items this component owes.

It also made a comment in the reset path false. It read *"what has already been
transferred stays transferred and is reported (D23)"*. The first half was true;
the second was not, because the caller's generation guard deliberately skipped
the byte accounting for an old-epoch transfer, so those bytes reached no
counter anywhere. Publishing per chunk makes the sentence true: the bytes land
in the epoch that moved them, and `reset()` clears that epoch itself.

The generation guard did not have to move. It already sits above the chunk
counter, so the invariant the existing gate asserts — an old-epoch chunk must
not repopulate counters a reset had cleared — is preserved untouched, and that
check still passes.

### E-2 — a refusal that moved nothing is still a refusal

`*_partial_failures_` answers "how often did a transfer stop part-way", and it
is guarded on `moved > 0` because that is the right guard for that question: a
transfer that committed nothing is not a partial completion, and an earlier
round correctly stopped calling it one.

That fix removed the wrong counter without adding the right one. Walk the path:
the transfer passes every endpoint check, the first chunk is issued, the target
answers with an error, `moved == 0`, and the only accounting site is skipped.
`protocol_errors_` counts payload-rule violations; `outbound_local_refused_`,
`outbound_span_refused_` and `inbound_foreign_refused_` count refusals the
**endpoint** made. Nothing counted a refusal the **target** made.

The part that makes this worth a counter rather than an inference is that it
**cannot** be inferred. The residue is `chunks > 0 && bytes == 0`, and a
legitimate fully byte-disabled write produces exactly that residue while
succeeding. The two were not merely uncounted, they were indistinguishable.

The new counters are kept apart from the `*_refused_` ones on purpose: "the
endpoint refused this, having touched nothing" and "the target refused this,
possibly after touching something" are different events with different owners,
and merging them would destroy the distinction that makes the refusal counters
worth reading.

### E-3 and E-4 — the two directions now carry the same counters

Outbound had both a count and a most-recent value; inbound had only the value.
An inbound transfer failing after committed bytes therefore overwrote a number
and recorded nothing, so one occurrence and twenty were the same reading.

`report()` was asymmetric for the same reason: the outbound line printed splits,
partial failures and both refusal causes, the inbound line printed transfers,
chunks, bytes and foreign refusals. `last_inbound_partial_bytes_` was set and
appeared in no report at all — reachable only through its accessor, from a test.

The `last_*` values stay accessor-only. They are "most recent", not totals, and
a report is the wrong place for a value whose meaning depends on when it is
read.

## 3. The controls, and what they measured

Each control is the source mutation named below, applied on its own to an
otherwise clean tree, rebuilt and run. All four **failed before the fix and
pass after**. Run by hand on 2026-08-22, Release, GCC 11.5.0 / SystemC 2.3.4.

| Control | Mutation | Result with the mutation |
| --- | --- | --- |
| E-1 | publish bytes at the caller again instead of per chunk | `FAIL: mid-flight the endpoint reported 2 chunks and 0 bytes.` |
| E-2 | delete the `*_target_errors_` increments | `FAIL: a target refusal that moved no bytes was counted nowhere.` plus `FAIL: the inbound target refusals were not counted` |
| E-3 | delete the `++inbound_partial_failures_` | `FAIL: an inbound partial failure was not counted` plus `FAIL: the second inbound partial failure was not counted` |
| E-4 | drop the inbound partial/target fields from `report()` | `FAIL: the inbound report line prints no partial-failure count.` plus `FAIL: the inbound report line prints no target-error count` |
| D24-a | measure an `sc_time_stamp()` difference instead of logical time | `FAIL: transfer latency read 0 s, expected 4 ns.` plus two more |
| D24-b | sample latency for transfers the endpoint refused as well | `FAIL: a transfer the endpoint refused was counted as a latency sample` plus one more |
| READ-a | `is_read = false` — the pre-fix direct pointer, both halves reverted together | `FAIL: the failing read chunk overwrote the caller's buffer.` |
| READ-b | publish the whole staged chunk, ignoring byte enables | `FAIL: a masked read overwrote the caller's disabled bytes.` |
| CFG-a | drop the cap at `sram::neo_max_transfer_bytes` | `FAIL: an inbound SRAM limit above sram::neo_max_transfer_bytes was accepted` |
| D25-a | revert to the old `limit - lane` chunking for reads | **102 of 120** remote SRAM reads refused by the real interconnect |
| D25-b | shape remote MMIO as well instead of forwarding it whole | the remote target is entered for a read that should have been refused pre-injection |

READ-a and READ-b are covered in §4c, including why the first version of READ-a
did not count. Three notes on how the others are built, because each is the
difference between a control that bites and one that only looks like it does.

**E-1 needs a second process, not a stronger assertion.** Reading the counters
after the transfer returns cannot see the defect: by then the caller has
published the bytes and the two agree. The control adds `counter_probe`, an
`SC_THREAD` that samples `outbound_chunks()` and `outbound_bytes()` at 500 ns
while a four-chunk transfer is parked inside a downstream target that waits
200 ns per access — two chunks returned, the third in flight. It asserts the
probe caught exactly two chunks *before* asserting the byte count, so a probe
that fired at the wrong instant fails loudly instead of passing vacuously.

**E-2's control is the pairing, not the refusal.** Asserting that a refusal
increments a counter proves little on its own. The control puts the refusal and
a fully byte-disabled write on the same bench and requires that everything
published about them agrees — one chunk, zero bytes, in both cases — except the
counter added for exactly this. That is the check that would fail if someone
later "simplified" the two events back into one.

**E-3 runs twice on purpose.** One failure would pass against a most-recent
value as easily as against a count. Two failures with different byte totals
separate them: the count must read 2 while the value follows the second.

## 4a. D24 — outstanding and latency, split by quantity

`TPU_V3_PHASE9_NOC_REBASELINE.md` §9.3 recorded plan §11.10's outstanding and
latency counters as **absent, not incomplete**, and refused to let them be
built before the ownership question was answered: `noc_interconnect` already
publishes per-port completion queues and `last_latency_cycles(port)`, so an
endpoint-level duplicate would be a second, disagreeing source for one
quantity.

The answer, ratified 2026-08-22 as **decision record D24**, is that they are
two questions and not one:

* **latency is measured here**, per transfer, because only this component can.
  A transfer larger than the frame limit is one thing the chip asked for and
  several transactions to the interconnect, so `last_latency_cycles(port)`
  answers "how long did one transaction take" and cannot be made to answer
  "how long did the chip wait for what it issued". Deferring would not have
  avoided a duplicate; it would have lost a quantity nobody else holds;
* **outstanding is not measured here at all.** Count what could be in flight
  at an endpoint in the Revision 1 composition: `neo_external_bridge`
  arbitrates a core's two outbound initiators onto one external socket
  (Phase 8) and `chip_local_fabric` allows one transaction per initiator, so an
  outbound outstanding count here is **`<= 1` by construction**. That is a
  constant printed in the position of a measurement — the failure mode Phase 5
  §7 and Phase 7 §3a both had to correct, a check that exists, passes and could
  not have failed. The number that varies is the interconnect's, and it is
  already published.

### Logical time, and why the obvious implementation is empty

```text
latency = (sc_time_stamp() + delay) at return - the same sum at entry
```

Not an `sc_time_stamp()` difference. An annotating target consumes no simulated
time and adds to `delay`, so a stamp difference reads **zero** for every
transfer on a loosely timed path — which is the path a full-system run uses
(D16), so the counter would have been silently empty in exactly the
configuration it exists for. Control D24-a is that implementation, and it fails
with `transfer latency read 0 s, expected 4 ns`.

The sum is also mode-independent, which is what makes one formula enough:
`absorb_incoming_delay()` moves time out of `delay` and into `sc_time_stamp()`
and leaves the sum alone. So the entry sample can be taken at the top of the
transport, before every check and before the delay is absorbed, and it cannot
drift as checks are added.

Measured both ways in one gate: four chunks through an annotating target read
4 ns with `sc_time_stamp()` never moving, and the same four chunks through a
target that waits 200 ns each read 804 ns — `4 × (200 waited + 1 annotated)`.

### What is not sampled

A transfer the endpoint **refused** — local containment, a multi-region span, a
payload-rule violation — returns almost immediately and describes nothing about
the mesh. Sampling it would drag the mean toward zero in proportion to how many
integration defects were present, which is the wrong direction for a metric to
move when something is wrong. Control D24-b samples them and fails.

A transfer **abandoned by a reset** is not sampled either, under the generation
rule that already governs every other counter here; the in-flight reset case
asserts it.

`report()` prints the latency line and, next to it, what the figure is not: per
transfer rather than per transaction, not comparable with the interconnect's,
and outstanding deliberately absent. That sentence is asserted by the E-4
control rather than merely written, because a reader with both numbers in front
of them will compare them unless the report says otherwise.

## 4b. R-P9-1 — the chip aperture is one `target_kind` — CLOSED by D25

Raised 2026-08-22 by an external review of this work and **closed the same day
as decision record D25**, after a second review round supplied the argument
this section had missed. The diagnosis below is kept as written; §4f records
what changed and why.

It was first kept as a **named risk for the composition task** rather than as a
defect, because the behaviour is a map decision taken before this component
existed and recorded with its cost.

### What is true

`from_noc` is one `simple_target_socket`, and that socket is `N = 1` upstream —
`tlm_target_socket<BUSWIDTH, TYPES, 1, POL>`
(`tlm_utils/simple_target_socket.h:50`). So a chip's 128 MiB aperture is one
`add_target()` call carrying one `target_kind`, and `ADDRESS_MAP.md` §7 says
which one:

> The chip aperture is declared `mmio` even though most of it is core SRAM. It
> is the conservative answer... The cost is that remote vector-width reads of a
> chip's SRAM must be bus-aligned. If that becomes a real limitation, the fix is
> to map core SRAM as a separate `memory` sub-region — a map change, made
> deliberately, not a `target_kind` downgrade.

The enforcement is at `noc_interconnect.cpp:2084`: a read whose beat frame is
full width and whose address or length is not bus-aligned is refused with
`TLM_BURST_ERROR_RESPONSE` **before injection**, and the target is never called.

Two qualifications, because the rule is narrower than it first reads and both
halves matter for what the composition test should expect:

* **writes are unaffected** — the condition is on `TLM_READ_COMMAND`;
* a naturally aligned power-of-two read of 1, 2, 4 or 8 bytes stays a *narrow*
  beat and is never widened (`axi_lanes.hpp:88`), so it is always allowed.

### What that does to this endpoint's own chunking, which is the part worth measuring

`chunk_span()` makes the first chunk `limit - lane` so a transfer pays for its
lane offset once. For an outbound **read** of a remote chip's SRAM starting at
lane `+k`, that first chunk is `limit - k` bytes at `+k` — neither bus-aligned
nor a narrow power of two. It is therefore refused, and the first-failing-chunk
rule stops the transfer having committed nothing.

So the reachable statement is concrete rather than theoretical: **an inter-chip
DMA read of remote SRAM at any address that is not 8-byte aligned fails at its
first chunk.** Plan §16's Phase 9 task list includes "test inter-chip DMA and
CPU traffic", so this is on the path, not off it.

### Why it is not being fixed here

`target_kind` is chosen by the platform at `add_target()` and enforced inside
the interconnect before this endpoint is called. Nothing this component does can
influence it, so "give the endpoint per-address kind classification" is not a
remedy it can implement — the decision point is upstream.

The remedy §7 names — core SRAM as separate `memory` sub-regions — would need
per-region ingress here. Recorded now so the shape is known when someone
reaches for it: a second bind to `from_noc` is **refused at elaboration**, not
silently mis-addressed, and `simple_target_socket_tagged` does not help because
the tagged variant is also `N = 1` (`:580`) — a trap, since "tagged" is exactly
what one would reach for. The multi-bind shape is
`multi_passthrough_target_socket`, or one plain socket per region. The rebase in
`noc_b_transport()` would also stop being a single aperture-base subtraction,
since the interconnect subtracts *the matched region's* base
(`noc_interconnect.cpp:1189`).

### The decision the composition task owes

One of these, recorded when it is taken:

1. **Keep `mmio`** and constrain remote SRAM reads to bus alignment, making that
   a stated property of the inter-chip path rather than something a test happens
   to avoid; or
2. **take §7's escape hatch** — separate `memory` sub-regions plus per-region
   ingress here — on evidence that a real workload needs offset remote reads.

Choosing (1) by default is legitimate. Choosing it *by not noticing* is what
this entry exists to prevent, and the way it would present is a Phase 9
inter-chip read test that quietly uses aligned addresses and passes.

### On the review that raised it

The finding arrived rated as a defect that made "a correct real-NoC binding
impossible". That part is wrong, and the reason is worth recording because it is
a failure mode this project has hit before from the other direction: the review
read `address_map.h` and `INTERFACE_CONTRACT.md` §7 but not `ADDRESS_MAP.md` §7
— and §7 of the contract document says only "Regions are registered with the
`target_kind` from `ADDRESS_MAP.md` §7", i.e. it points at the file that carries
the decision, the rationale and the escape hatch. A correct binding exists and
is specified; what the review actually found is its cost, and the cost was
already written down. The mechanism it described is nonetheless right, the
`chunk_span()` consequence above is new, and both are why this section exists
rather than a dismissal.

## 4c. A failed read overwrote the caller's buffer

Found 2026-08-22 by an external review, fixed the same day. Unlike R-P9-1 this
one is a defect in this component, and it is the first finding in Phase 9 that
the gate could not have caught because **the stub was politer than the thing it
stands in for**.

### The chain

`forward_chunked()` handed each chunk a direct pointer into the caller's buffer
(`chunk.set_data_ptr(data + committed)`). For a read that cannot honour
`INTERFACE_CONTRACT.md` §1 — *"Errors are never converted to zero data. A failed
read leaves the caller's buffer untouched and sets an error status"* — because
the downstream path writes the buffer before this component sees the status.

`noc_interconnect` writes zeroes there, deliberately:

| Step | Where |
| --- | --- |
| a decode miss assigns zeroed `read_data` | `noc_interconnect.cpp:1149` |
| a target refusal does the same | `:1234` |
| `unpack_read()` copies it into the caller's pointer **unconditionally**, one line before the status is set | fast/bypass `:1369-1373` |
| the routed mesh path has the same shape | `:2204-2207` |

**Both timing modes**, which is worth stating because the review named only the
target-refusal case: there is no configuration in which the endpoint could have
relied on the buffer being left alone.

### Why the fix belongs here and not in the interconnect

The interconnect's behaviour is AXI-faithful and intentional — RRESP is per
beat and the data lanes are still driven — and the comment at `:1233` says so:
*"Return the bytes in the lanes AXI would have used, so the initiator's
`unpack_read` finds them where it looks."*

`INTERFACE_CONTRACT.md` binds *"every component under `components/TPU_V3`"*.
`floo_noc_model` is outside that, with its own v1.4 sign-off, 12 RTL
cross-checks and a 57-control mutation registry; changing it to satisfy a
TPU_V3 rule
stricter than AXI means re-running that campaign. And this endpoint's own
header already states the job: *"absorb the constraints the interconnect
imposes, so that nothing above it has to know them."*

### The fix

A read chunk is staged; a write is not, because a target only reads the
caller's buffer and a failed write cannot modify it. On `TLM_OK_RESPONSE` the
staged bytes are published, and only the **enabled** ones — `unpack_read()`
skips disabled bytes (`axi_lanes.hpp:156`), so the target never wrote them and
the caller's own values are what belongs there. Chunks that succeeded before a
failure keep their data, as §7 requires.

One consequence beyond the headline: a read abandoned by a reset in flight now
discards its data as well as its accounting. Previously the bytes landed in the
caller's buffer while being counted in no chunk and no byte total — data
present, accounting absent, which is the worse of the two states.

### The design decision the controls forced

The first implementation seeded the staging buffer with the caller's existing
bytes, so that a successful read would be byte-for-byte what the direct pointer
produced. It was the conservative choice and it was wrong for a specific
reason: with the buffer seeded, a disabled position holds the caller's own
value either way, so publishing the whole staged chunk is **indistinguishable**
from publishing only the enabled bytes. The control written for the
byte-enable rule passed against the mutation — a check that exists, passes, and
could not have failed, which is the shape Phase 5 §7, Phase 7 §3a and Phase 8
§2a each had to correct.

Between the two mechanisms, the one that cannot be tested is the one that goes.
Staging is now zero-initialised and only enabled bytes are published, which
makes the byte-enable rule a behaviour with a control that bites. The residual
difference is recorded at the call site rather than left to be found: a
downstream target that reports success without delivering an enabled byte now
surfaces as a zero rather than as the caller's stale value. Both are wrong,
the target is what is wrong, and neither is reachable through `unpack_read()`,
which writes every enabled byte its beat frame covers.

### Controls

| Control | Mutation | Result |
| --- | --- | --- |
| READ-a | `is_read = false`, reverting both halves at once — the exact pre-fix behaviour | `FAIL: the failing read chunk overwrote the caller's buffer.` |
| READ-b | publish the whole staged chunk, ignoring byte enables | `FAIL: a masked read overwrote the caller's disabled bytes.` |

READ-a's first version mutated only the staging fill and left the copy-back, so
the test **segfaulted** instead of failing. A mutation that crashes is not a
control: it proves the code cannot survive an inconsistent edit, which nobody
doubted. It was rewritten to flip `is_read`, which reverts both halves together
and reproduces the defect exactly.

### What had to change in the bench

`recorder` gained `zero_fills_failed_reads`, set on the **mesh** stub only,
because that stub stands in for `noc_interconnect`. The chip-side stub keeps
the polite behaviour, and the asymmetry is faithful rather than convenient:
`core_sram` and the fabrics below the chip port already leave a refused
access's buffer untouched, which is the Phase 3 property.

It is deliberately **not** applied to the frame-limit refusal, which the real
interconnect answers before injection (`noc_interconnect.cpp:2084`) and which
returns without ever reaching `unpack_read` — there the buffer really is
untouched.

This file's header already recorded the mirror image of this lesson about
inbound addresses: *"A stub that invents its own convention tests the stub."*
This is the same failure from the other side — a stub that refuses more gently
than the real thing.

## 4d. An inbound chunk limit above the native plane's own maximum

Found 2026-08-22 by an external review, fixed the same day. A configuration
defect rather than a runtime one, and the kind that elaborates cleanly.

`chip_endpoint_config::validate()` checked that `max_inbound_sram_bytes` was
non-zero and a whole number of `bus_bytes`, and nothing else. So a value like
128 was accepted — it is a whole number of 8-byte words — and the endpoint
would then emit 128-byte inbound chunks. `sram::neo_max_transfer_bytes` is
**64**, one RVV register at VLEN=512 and the largest payload any single native
access can need (D7), and `neo_external_bridge.cpp:142` refuses an inbound SRAM
access above it, saying exactly why:

> The chip endpoint chunks oversized transfers before they reach a core
> (`INTERFACE_CONTRACT.md` §7); one arriving here means that did not happen, so
> it is refused rather than silently split.

The endpoint would therefore have been configured to do precisely what that
comment describes as broken, and the failure would appear on the first inbound
SRAM transfer rather than at elaboration.

Worth naming for what it was: this component's own header already asserted the
relationship — *"The largest inbound access a NEO-CORE's SRAM path accepts"* —
and nothing checked it. A documented invariant with no gate, the same shape as
the checks §5 records having to correct.

Refused at construction now, with the field, the offending value and the
accepted range in the message (`INTERFACE_CONTRACT.md` §10). The gate checks
both sides of the bound: `neo_max_transfer_bytes + 8` throws and
`neo_max_transfer_bytes` itself is accepted, so the bound is a bound rather
than an off-by-one.

| Control | Mutation | Result |
| --- | --- | --- |
| CFG-a | drop the cap at `sram::neo_max_transfer_bytes` | `FAIL: an inbound SRAM limit above sram::neo_max_transfer_bytes was accepted` |

## 4e. A reported bypass ordering defect that did not reproduce

Investigated 2026-08-22 after an external review reported it as P1 against
`noc_interconnect.cpp`. **No defect was found and no code was changed.** The
investigation is recorded because the first measurement said the opposite and
was wrong.

### The claim, and why it looked sound

`await_earlier_bypass()` holds a routed access only while an earlier bypass is
outstanding:

```cpp
while (!outstanding_bypass[slot].empty()
       && *outstanding_bypass[slot].begin() < ticket)
    wait(*order_ready[slot]);
```

When the bypass erases its ticket, **every** waiter passes that loop in one
delta, and nothing after it re-imposes their relative order — routed-versus-
routed is left to the deques on purpose, because forcing ticket order here was
measured to blind `same-port-request-order-reversed`. The review's reading was
that two routed accesses released together could return out of issue order,
against the `MaxUniqueIds = 1` promise of per-channel FIFO.

The premise checks out. `noc_interconnect.h:238` states the FIFO contract, and
`max_outstanding_per_port` explicitly admits concurrent `b_transport` calls on
one socket. The existing `order_probe` covers only the mirror case — routed
first, bypass second — so nothing in the suite would have caught it.

### The first measurement was invalid

A scenario was added with a slow bypass at 0 ns and two routed accesses at 1 ns
and 2 ns. It reported `0 2 1` and that was taken as a reproduction. It was not.

Instrumenting ticket assignment showed **both routed accesses taking their
completion tickets in the same instant, at 4 ns** — both had been blocked in
`ensure_started()` while the mesh came up, and the kernel decided which entered
first. The probe that started *later* held the *earlier* ticket. Issue order is
ticket order, so the check was asserting tag order against a wrapper that never
promised tag order, and the reversal it saw was the test's own.

### What the corrected scenario measures

Separating the routed accesses to 100 ns and 150 ns makes issue order
observable: ticket 1 at 100 ns, ticket 2 at 150 ns, both parked behind the
bypass, released together at 404 ns. Completion order `0 1 2`.

Then the decisive control: **the same run with an explicit ticket-order release
removed also produces `0 1 2`.** The ordering is preserved by SystemC 2.3.4
resuming dynamic waiters in the order they began waiting, and wait order
follows issue order here.

### Disposition

The candidate fix — track whether an access was actually held, and if so fall
back into `await_bypass_turn()` on the way out — was written, measured to change
no observable outcome, and **reverted**. Adding ordering constraints to a signed
component on the strength of an argument is what D1's own design notes warn
against, and the standard here is that a change without a control that bites is
not evidence.

The scenario is **kept**, and its comment says plainly what it is.

**Corrected 2026-08-24.** This paragraph first read "No mutation of
`noc_interconnect.cpp` makes it fail, because the ordering it observes is not
performed there." That is wrong, and gathering evidence for R-P9-2 is what
exposed it: disabling `await_earlier_bypass()` entirely turns the release order
into `1 2 0` — two routed accesses overtaking a bypass issued before them — and
`release_order[0] == 0` fails. So the scenario **is** a control, for the
bypass-versus-routed half of the ordering, and that half is performed here.

The two halves have to be stated separately:

| Assertion | What it is |
| --- | --- |
| `release_order[0] == 0` — the bypass is not overtaken | a **control**: removing `await_earlier_bypass()` makes it fail |
| `release_order[1] == 1 && [2] == 2` — the two routed accesses keep issue order | a **characterisation**: nothing here performs it, and it holds because SystemC resumes dynamic waiters in wait order |

Only the second earns the "no mutation makes it fail" description. Collapsing
both into one sentence understated what the file already covered — the opposite
of the failure mode §5 records, and the same root cause: a claim written from a
partial reading of what the code does.

**What remains true and is worth carrying:** the wrapper's per-channel FIFO
guarantee, in the presence of a bypass release, rests on an unspecified kernel
property rather than on anything the wrapper does. Not a defect today, and now
written down and pinned.

`components/floo_noc_model` was left untouched **by this investigation**:
`noc_interconnect.cpp` was restored byte-for-byte, verified by diff against a
snapshot taken before the candidate fix. It has since been changed by D26 for a
different reason (§4g), so "the component was reverted to its prior state" is
true of §4e's candidate fix and not of the tree as it now stands.

## 4f. R-P9-1 closed — and the half of it §4b had missed

Date: 2026-08-22. Decision record **D25**.

§4b framed the choice as: keep `mmio` and publish "remote SRAM reads must be
bus-aligned", or take `ADDRESS_MAP.md` §7's escape hatch. It recommended the
first. A second review round rejected both and proposed a third, and it was
right, for a reason §4b had in front of it and did not follow through.

### What §4b got wrong

It quoted the refusal condition correctly and then derived only half of it.

```cpp
if (command == tlm::TLM_READ_COMMAND && shape.size_log2 == 3
    && (address % bus_bytes != 0 || length % bus_bytes != 0)
    && ...kind != target_kind::memory)          // noc_interconnect.cpp:2084
```

`length % bus_bytes != 0` is a **separate clause**, so a 65-byte read at an
aligned address is refused as surely as a 1-byte read at `+5`. §4b's derivation
spoke only of addresses, and the decision request built from it inherited that.

That matters because plan §16 Phase 4 froze `neo_dma` at exactly those shapes:

> lengths **1, 2, 3, 7, 8, 15, 16, 63, 64, 65** ... **Odd addresses**

So "publish the restriction" was not a documentation task with a small cost. It
would have **narrowed a contract that is already gated and signed off**, without
a decision saying so. Measured afterwards: **102 of 120** offset/length
combinations are refused without the fix. §4b's recommendation would have made
85% of that matrix a firmware constraint nobody had agreed to.

### What was missing from the proposal, and had to be added

The review's proposal said "with outbound `READ` aimed at remote `CORE_SRAM`".
The endpoint cannot express that with what it had. `decode_region()` returns the
**architectural** region kind, and `CORE_SRAM` is memory-like — but it sits
inside an aperture registered `mmio`, and it is precisely that mismatch which
creates R-P9-1. Using `region.is_memory` alone would also have shaped reads to
`GLOBAL_RAM`, a `memory` target that widens safely, adding up to six
transactions per offset read on the bulk path for nothing.

The implemented condition is therefore three-way — read, memory-like region,
**and** inside a chip aperture — with the last one determinable from the address
alone. Control D25-b is the one that would catch this being loosened: dropping
`region.is_memory` makes a remote MMIO read reach the target the interconnect
should have refused before injection.

### The pattern worth carrying

Both §4c and this section have the same shape: a rule was read, the part that
was easy to derive was derived, and the other half sat unexamined in the same
quoted snippet. In §4c it was that `noc_interconnect` writes zeroes into the
caller's buffer on a failed read; here it was `length % 8`.

Neither was found by a test, because in both cases the test was written from
the same partial reading as the code.

## 4g. R-P9-2 — the admission bound did not survive a bypass hold

Raised 2026-08-22 by an external review, decided and implemented 2026-08-24 as
decision record **D26**. Like §4e it is about `components/floo_noc_model`
rather than TPU_V3, and unlike §4e **it reproduced**.

### What was measured

`complete_manager_transaction()` released the admission slot on the mesh side,
when the final AXI response landed. The caller then returned into
`await_earlier_bypass()` and could park there — with its slot already given
away. With the bound set to one:

| | routed calls admitted concurrently | `outstanding_transactions(0)` while parked |
| --- | --- | --- |
| as found | **2** | **0** |
| with `await_earlier_bypass()` removed | 1 | 0 |

The second row is the control that identified the cause: without the ordering
wait a routed call never parks, so the window does not exist. The parking is
D1's, and D1 is uncommitted, so this is a defect the working tree carries and
not one the signed component shipped.

### Why the obvious fix is half a fix

Moving the release into `b_transport` and leaving everything else alone keeps
`outstanding_by_port` non-zero through the hold — and `network_idle()` consults
it, while the clock gate consults `network_idle()`. The mesh would then be
stepped for the whole hold with nothing in it.

Measured, by putting exactly that back: **100 mesh cycles inside a 100 ns
window** whose only remaining work was one caller's ordering wait. Every
counted-cycle metric — `clock_gate_transitions()`,
`mesh_quiescent_wrapper_busy_cycles()`, router utilisation — inflates by that
amount.

So the two idle predicates are separated by the question they answer:
`network_idle()` is about physical work and stops consulting admission slots;
`wrapper_idle()` is about whether a caller is still owed its return and starts
consulting them.

### The bench had to be rewritten before it could judge anything

The first version counted callers inside `b_transport` and read **2 both before
and after** the fix. A caller blocked in the admission gate is inside
`b_transport` too, so that number cannot tell an enforced bound from an
unenforced one — the metric was measuring the wrong population, and it was the
reviewer who noticed rather than the author.

The gate now reads the wrapper's own accounting and what actually reached the
mesh:

```text
outstanding=1  peak=1  remote_accesses=1  mesh_quiescent=1  wrapper_idle=0  cycles 24->24
```

`remote_accesses == 1` carries it: the second routed call was never injected,
which is a fact about the mesh rather than about a counter.

### Controls

Both are **registered in `run_negative_controls.sh`**, not run by hand. That
matters here in a way it does not for the endpoint controls of §3: TPU_V3 has no
mutation harness, so its controls have to be hand-run and written down, but
`floo_noc_model` has one — and a control announced in an audit while the
registry does not carry it is evidence nobody can re-run.

| Control | Registered name | Mutation | Result |
| --- | --- | --- | --- |
| RP92-a | `admission-slot-released-before-ordering-wait` | release the slot before the ordering wait instead of after | `outstanding=0`, `remote_accesses=2` |
| RP92-b | `network-idle-consults-admission-slots` | let `network_idle()` consult admission slots again | mesh advanced 100 cycles in the hold window |

RP92-a is registered as a single move of `slot.release()` rather than as the
original two-site defect, and the reason is worth keeping: putting the release
back into `complete_manager_transaction()` while leaving `slot.release()` in
place releases twice, and the guard throws before any check runs. That control
would prove the guard works, not that anything covers the bound.

### The mutation registry still bites, and grew by two

Run after the change, on 2026-08-24: **57 detected, 0 missed** — the 55 the
registry already carried, plus the two this decision added. That is the
question worth asking of a change to idle and clock-gating semantics — not
whether the suite still passes, but whether it can still fail. The harness
copies the component to `/tmp` and never touches the working tree, so the run
is repeatable from a clean tree.

The **12 RTL cross-checks were run and pass**, 2026-08-24, against the FlooNoC
tree at the pinned `v0.8.4-10-g9a6972a`:

```text
route-select 12 cycles · stream-fifo depth 2 and 4, 133 each
wormhole-arbiter routes 5/4/2, 152 each · floo-router out-fifo 2 and 0, 214 each
norob-ordering 127 · chimney-request 16 flits · chimney-timing 141
chimney-response 8 flits · chimney-rsp-timing 221 · chimney-mgr-rsp 78
mesh 1872 node-cycles · axi-sizing 8 configurations
RTL cross-check summary: 12/12 passed
```

An earlier draft of this section said they had not been run and would need a
judgement call. That conflated two things `run_negative_controls.sh`'s header
keeps apart: the **RTL cross-checks**, which compare model traces against the
frozen RTL and are mechanical, and the **RTL-level mutation controls**, which
are manual because several legitimately pass where the frozen RTL is redundant
— a judgement the harness says it cannot make. Only the second kind needs a
person. The first kind needed Verilator, Bender and the RTL tree, all of which
are on this host.

### One false alarm, recorded because it looks exactly like a regression

Between running a control by hand and re-running the suite, the component test
failed with the *mutated* signature — `outstanding=0, remote_accesses=2` — while
`diff` said the source was byte-identical to the good snapshot. The binary was
stale: the control had been reverted with `cp`, and `ctest` does not build.

`TPU_V3_PHASE9_NOC_REBASELINE.md` §7 already records the same shape from the
other direction — a `noc_soc` suite failing with eight "does not build" errors
that were really a link against a `libnoc_interconnect.a` nobody had rebuilt,
"a failure that looks exactly like a regression and is not". Hand-running
mutations into a shared build tree invites it. The lesson is the cheap one:
rebuild after restoring, and when a failure's signature is exactly the mutation
you just ran, suspect the binary before the code.

### Sign-off consequence, for the component owner

This changes idle and clock-gating semantics inside a component whose previous
v1.4 baseline carried 12 RTL cross-checks and 55 mutation controls. Technical
review on 2026-08-24 reproduced 42/42 component tests, 57/57 mutation controls
and 12/12 RTL cross-checks, and concluded that **D1+D26 meet the conditions for
approval**, recommending baseline v1.5. The engineering review gate is closed
and does not block the next Phase 9 task. The component owner selected and
approved baseline v1.5 on 2026-08-24. Formal evidence is bound to the 133-file
tested-source manifest and retained under
`components/floo_noc_model/docs/signoff/v1.5/`; the component is therefore
**signed v1.5** for that exact snapshot — and §4h records why the tree has
since moved off it.

## 4h. R-P9-3 — the FIFO promise stops resting on the kernel

Raised 2026-08-24 by an external review against
`noc_interconnect.cpp:336-339`, fixed the same day. Sign-off is **deliberately
deferred**; see the end of this section.

### What it is, and what it is not

Not a defect that reproduces. It is the property §4e already recorded, taken
one step further: one `notify()` releases every waiter parked in
`await_earlier_bypass()` in a single delta, and the loop lets them all past
without any of them consulting its own ticket. Their relative order is then the
kernel's resumption order for dynamic waiters, which the SystemC LRM does not
specify.

The review cited §4e's own test comment as its evidence, which is the right
reading: the documentation was correct and the conclusion drawn from it —
"recorded and pinned is enough" — was the part worth challenging. `MaxUniqueIds
= 1` promises FIFO completion per channel. A promise resting on an unspecified
property of one kernel is a promise that holds until somebody builds against a
different one.

### The fix, and the honest limit of its evidence

A call that `await_earlier_bypass()` **actually held** now falls back into
ticket order on the way out, through `await_bypass_turn()` — the same function
the bypass path uses, not a second copy of its body. A call that was never held
is untouched, which is what keeps `same-port-request-order-reversed` seeing
everything it saw before: a scenario with no bypass never enters the loop.

**No control can make the outcome differ on this kernel.** Accellera 2.3.4
resumes dynamic waiters in the order they began waiting, which here is issue
order, so the completion order is `0 1 2` with the fallback and without it.
That is not a gap in the testing; it is the whole reason the fix exists, and
pretending otherwise would be the failure mode §5 already records twice.

What is testable is that the fallback is **on the path it was written for**.
`ordering_holds()` counts calls the hold caught, and the directed test requires
at least the two the scenario parks. Registered as
`bypass-ordering-fallback-removed`: delete the fallback and the test reports
that held calls never reached it. The registry is **58 detected, 0 missed**
with it in place. That keeps it from becoming dead code nobody
notices rotting — which is the realistic failure, rather than a wrong answer.

### Sign-off: v1.5 no longer describes this tree

This is the consequence worth stating loudly, because it is invisible from
inside the documents that claim a signature.

`SIGNOFF.md` binds v1.5 to "the exact source snapshot bound by
`tested_source_manifest.sha256`". R-P9-3 changed four files after that
signature:

```text
src/noc_interconnect.cpp
include/floo_noc_model/noc_interconnect.h
tests/test_noc_interconnect_local_bypass.cpp
rtl_crosscheck/run_negative_controls.sh
```

`sha256sum -c` against the v1.5 manifest now reports four mismatches. **v1.5
remains valid for the snapshot it names, and no longer describes the working
tree.** Quote it for the manifest, not for the tree.

The re-sign is deferred on purpose, by the component owner's decision: it is
taken once for a batch of component changes rather than once per change, and
R-P9-3 is not urgent enough to spend a sign-off cycle by itself.

The evidence a batch re-sign needs is nonetheless current, re-run against the
changed tree on 2026-08-24:

```text
component regression      42/42
mutation registry         58 detected, 0 missed   (57 + bypass-ordering-fallback-removed)
RTL cross-checks          12/12 passed
TPU_V3 Release / Debug    53/53 each
```

So the batch re-sign is a run and a manifest, not an investigation. What it
still owes is what a signature always owes and evidence never supplies: a
person deciding.

## 4i. Single-chip real-mesh composition gate

Built 2026-08-24 as `test_chip_on_mesh`, gated in both NoC timing modes
(`tpu_v3_chip_on_mesh_fast`, `tpu_v3_chip_on_mesh_detailed`).

**Named carefully.** This is a composition *testbench*, not the platform
composition plan §11.11 asks for: `platforms/TPU_V3_SoC` still instantiates no
hart, accelerator or NoC, and its top says so. Calling this "Phase 9
composition complete" would claim the platform work as well.

Everything below it had been proved separately: `tpu_chip` boots two harts
(Phase 8), `chip_noc_endpoint` splits and contains (§2–§4h),
`noc_interconnect` carries flits and is RTL cross-checked. What none of those
could show is what happens wired together — and both the Phase 7 and Phase 8
audits record that composition is where the interesting defects live.

### What it is

```text
chip.external()  ──> endpoint.from_chip
endpoint.to_noc  ──> noc.cpu_port(0)          chip at (0,0)
endpoint.to_chip <── chip.inbound()
remote master    ──> noc.cpu_port(1)                  at (1,0)

targets: chip aperture (0,0) mmio, local_owner = 0   -> endpoint.from_noc
         boot ROM      (0,1) memory
         global RAM    (1,1) memory
```

The chip's own aperture is a target **on the chip's own node**, which
`NoLoopback = 1` refuses outright and which is legal only through D1's
owner-aware mapping. It is also why the harts can boot: their reset PC is in
`GLOBAL_BOOT_ROM`, which is now a mesh hop away rather than a socket away.

### Measured

```text
fast      boot ROM 28 fetches, global RAM 3 accesses
          outbound 30 transfers / 30 chunks / 1136 bytes, 0 local refused
          inbound  25 transfers / 38 chunks / 1112 bytes, 0 foreign refused
detailed  the same, plus 2030 accepted flits
```

28 fetches is two harts times fourteen instructions. 1136 outbound bytes is
those fetches plus both 512-byte DMA legs. Conservation is asserted in **bytes**
against counters on the far side of the mesh, held by targets that know nothing
about the endpoint.

### The first version had a mapped target nothing touched

Global RAM was bound to a mesh node and never accessed: `global RAM accesses 0`
in both modes. The test therefore passed with the RAM's node or route wrong,
and the conservation formula that mentioned `ram.accesses` was multiplying by
zero. Raised in review, and it is the same shape as everything else in this
document — a check that exists, passes, and could not have failed.

Closed by having the **chip itself** move bytes: the scenario programs core 0's
DMA through its MMIO window across the mesh, fetches 512 bytes from global RAM
into core SRAM, compares them against what the RAM holds, then writes them back
the other way and reads the RAM to confirm. That path is
`dma.external -> neo_external_bridge -> chip fabric -> endpoint -> mesh -> RAM`,
which is the bulk path the plan cares about rather than a CPU load.

Control **MESH-c** shifts the RAM's mapped base by 0x1000: the DMA no longer
completes, the compared bytes differ, and outbound bytes read 624 instead of
1136.

### A conservation formula that had to be corrected

The first version asserted
`outbound_transfers == rom.fetches + ram.accesses` and it is wrong once the
scenario is complete: the remote master reads global RAM **directly through the
mesh** at the end, which never crosses this endpoint, so the targets
legitimately see one access more than the endpoint forwarded. The assertion now
names what the endpoint carried — the fetches plus the two DMA legs — and the
reason the two counts differ is recorded at the check rather than left for the
next person to rediscover by watching it fail.

The inbound path is exercised end to end — remote master, mesh, eject at the
chip's node, endpoint rebase from aperture-relative to absolute, chip fabric,
core SRAM — and the 256-byte write proves the inbound split at the core's real
64-byte native limit, with the block read back and compared.

### Containment: what this composition can and cannot prove

The gate's first line is "local SRAM/MMIO traffic injects zero NoC flits". This
composition asserts it and **cannot fail the assertion**, which is worth
stating plainly rather than leaving for a reader to infer from its presence.

Two mutations were run looking for a control:

| Mutation | Result |
| --- | --- |
| disable the endpoint's own containment check | **no effect** — no chip-local address ever arrives to be checked |
| delete core 1's window from `chip_local_fabric::decode()` | the run fails loudly — 210541 runaway outbound transfers, a hart that never writes its mark — but `outbound_local_refused()` still reads **zero** |

The second is the informative one. `chip_local_fabric` has two layers: a window
that decodes locally, and an explicit refusal for an address inside the
aperture matching no window. Breaking the first leaves the second, so the
endpoint still never sees a local address. Containment is therefore gated by
`tpu_v3_chip_fabric`, and the endpoint's check is defence in depth.

What the composition does add is the **positive** half, which is what the
`local_refused == 0` check lacks on its own: `fabric().local_bypass() > 0`
proves core-to-core traffic actually happened. Without it, a chip that
generated no local traffic at all would satisfy the containment check
perfectly.

### What it deliberately does not re-prove

Outbound chunking at the frame limit. `test_endpoint_on_real_noc` already
drives 2047/2048/2049-byte transfers through a real `noc_interconnect` and
compares the bytes (D25). Repeating it here needs the chip's DMA programmed by
firmware and would be a second copy of a rule that already has one test.

### Controls

| Control | Mutation | Result |
| --- | --- | --- |
| MESH-a | endpoint stops refusing chip-local addresses | **does not bite** — recorded above, and why |
| MESH-a' | `chip_local_fabric` stops decoding core 1 locally | run fails, but not through the containment counter |
| MESH-b | endpoint stops splitting inbound SRAM traffic | `FAIL: a 256-byte inbound SRAM write was refused` plus two more |
| MESH-c | global RAM mapped 0x1000 off its real base | the DMA does not complete, bytes differ, outbound bytes 624 instead of 1136 |
| MESH-d | the endpoint issues every outbound read chunk twice | `4 accesses, expected exactly 3`, `3 reads, expected exactly 2`, `1536 bytes to reads, expected 1024`, and 56 boot fetches instead of 28 |

MESH-d exists because the RAM-side assertions were first written loosely —
`reads > 0`, `read_bytes >= kMove` — and a duplicated read satisfies both.
That is exactly the failure this section claims conservation rules out, so the
loose form let the claim outrun the check. The counts are now exact, and none
of the five depends on scheduling: the scenario issues a fixed set of accesses,
so any other number is traffic nobody asked for.

Its first attempt is also worth recording: the mutation used
`tlm_generic_payload`'s copy constructor, which TLM does not provide, so the
build failed — and because the build output was hidden, the **stale binary ran
and passed**. Second time in this session, after §4g recorded the same trap.
The lesson does not change and evidently needs repeating: never hide the build
output of a mutation run, and when a control does not bite, suspect the binary
before the theory.

### Sign-off state this runs on

The FlooNoC tree under this composition is **post-v1.5 and unsigned**: R-P9-3
moved four files off the signed manifest (§4h). Nothing here is blocked by
that, but a final technical sign-off needs a v1.6 baseline or a batch re-sign
covering the delta.

## 4j. Six findings from the composition review

Raised 2026-08-24 against the single-chip composition and the tree around it.
All six are fixed; the first is the one that matters, because it retracts part
of a decision record.

### 1 — D24's outstanding reasoning was wrong, and contradicted this project's own audit

D24 said an endpoint-level outbound outstanding count is `<= 1` **by
construction**, and deferred the metric on that basis. Measured on this
composition: **peak 2** in detailed mode.

The reasoning covered **one core**. `neo_external_bridge` does arbitrate a
core's two outbound initiators onto one socket — but two cores are two
initiators on the chip fabric, and `annotated` mode charges port occupancy to
the caller's delay rather than blocking, so while one core is suspended in the
mesh the other enters the endpoint.

`TPU_V3_PHASE8_AUDIT.md` §5 had already written that down:

> **`annotated` chip-fabric mode does not serialise a blocking target.** Port
> occupancy is a charge on the caller's delay, so two initiators can be inside
> one downstream target at once if that target waits.

Two documents in this repository, both mine, disagreeing. The endpoint now
publishes the counters and D24 carries the retraction.

**How the first measurement lied.** The instrumentation put its scope guard
inside a nested block, so the live count returned to zero immediately and the
peak read 1 — which is exactly what I expected to see, and would have closed
the finding as unreproducible. Fixing the scope gave 2. A measurement that
confirms the thing you already believe deserves a second look at the
measurement.

### 2 — a ticket could leak past the exception cleanup

`local_bypass_transport()` took its completion ticket and registered it in
`outstanding_bypass` **before** the `try`. An allocation failure in
`std::set::insert` therefore escaped without reaching any cleanup, leaving
`bypass_in_flight` raised and the ticket unretired — after which the wrapper
never reports idle and later same-channel bypasses block for good.

Fixed with **two** flags rather than one, because the two steps fail
differently: a ticket that was *taken* has advanced `issue_seq` and has to be
retired; a ticket that was also *registered* additionally needs erasing from
the set. The routed path had the same shape — a ticket consumed before the
packing and queue allocations — and its `slot_guard` now retires it. The guard
only notifies; `await_bypass_turn()` would `wait()`, and waiting inside a
destructor during unwinding is a different problem.

**This fix was incomplete, and §4k finding 1 completes it.** Retiring during
unwinding is retiring *out of ticket order*, which the scalar `done_seq` of the
time could not represent: it moved the count past tickets that had not
completed. Both call sites named here now go through `retire_ticket()`.

### 3 — the debug transports answered nothing

Both `transport_dbg` callbacks returned a bare `0` on three refusal paths
without setting a response status or clearing DMI, and passed a successful call
straight back however the downstream target left it. `INTERFACE_CONTRACT.md` §1
binds `transport_dbg` as well as `b_transport`: status **always set**, DMI
false. A caller was left reading the `TLM_INCOMPLETE_RESPONSE` it had set
itself, which §1 calls a defect in the target rather than a condition for the
caller to handle.

Now every return path states an outcome, including the successful one — the
stub in this component's own gate is an example of a target that leaves the
status untouched, so the endpoint cannot rely on delegation.

### 4 — the fast-mode test was running detailed-mode timing

`endpoint_config()` left `downstream_spends_delay = true` for both backends, so
in `fast` the endpoint sat out the caller's annotation before entering an
interconnect that annotates rather than blocking. The "full-system fast" case
was therefore not exercising fast timing semantics at all.

The flag is now taken from the selected NoC mode, and the fix is **visible in
finding 1's numbers**: the peak in fast mode is 1 with it and was 2 without,
because an endpoint that waits makes callers overlap. That is why the gate
asserts a different value per backend instead of the same one twice.

### 5 — `STATUS.md` misstated the sign-off scope

It read "supersedes v1.4 for the current component snapshot". v1.5 supersedes
v1.4 **for the snapshot its manifest binds**, and the tree has moved off that
snapshot. Corrected there, where a reader of the component would look, rather
than only in this audit.

### 6 — `.gitignore` swallowed the sign-off evidence, and v1.4 is already in that state

The four v1.5 transcripts match the root `*.log` rule, so adding the sign-off
directory would commit an `artifact_manifest.sha256` that hashes four files a
fresh checkout does not have. Fixed with a scoped negation:

```gitignore
!components/*/docs/signoff/**/*.log
```

The negation immediately exposed something the finding did not reach: **v1.4 is
already committed in exactly that broken state.** Its artifact manifest hashes
`systemc_tests.log` and `mutation_controls.log`, and `git ls-files` on that
directory returns three files, none of them a log. The signed baseline in the
repository today has an artifact manifest nobody can verify. Fixing that is the
v1.4 owner's call, not this audit's — it is recorded here because it is
invisible until someone tries to check a signature.

### Measured after all six

```text
floo_noc_model            42/42
mutation registry         58 detected, 0 missed
TPU_V3 Release            55/55, 0 Skipped
TPU_V3 Debug              55/55, 0 Skipped
```

## 4k. Five findings from the unwinding review

Two of these are in the frozen, signed component. R-P9-3's batch decision
applies: fix now, re-sign once.

### 1 — a call that unwinds retired its completion ticket out of order

**The defect.** Completion order per port and channel was a scalar count.
`mark_completed()` did `++done_seq[slot]`, and `await_bypass_turn(ticket)`
waited for `done_seq >= ticket` — reading "the count reached my ticket" as
"everything below me is done". Those two agree only while retirement happens in
ticket order.

The unwinding paths break exactly that. A call that throws after taking its
ticket cannot serve an ordering wait, so it retires wherever it happens to be:

* `slot_guard::complete()` on the routed path, for a throw between the ticket
  and the queued request — `pack_write`, `reserve`, two `push_back`s, all
  allocating;
* `local_bypass_transport`'s `catch`, for the `!registered` window where
  `outstanding_bypass[slot].insert()` itself threw.

With ticket 0 routed and still in the mesh, ticket 1 a bypass parked in
`await_bypass_turn`, and ticket 2 unwinding, ticket 2's retire moved the count
0 → 1 and released ticket 1. A bypass completed ahead of a routed access issued
before it, on a channel whose `MaxUniqueIds = 1` promises FIFO.

**The fix.** Retirement carries its ticket. `retire_ticket(port, is_write,
ticket)` replaces `mark_completed(port, is_write)` at all four call sites;
`done_seq` becomes the **contiguous retired prefix**, and a ticket retiring
above it is recorded in `retired_ahead` until the prefix reaches it.
`await_bypass_turn()` is unchanged and now means what it always read as.

Two details are load-bearing rather than incidental:

* **`retired_ahead` is a 64-bit word, not a `std::set`.** The out-of-order
  retire runs *while an exception unwinds*, including inside
  `slot_guard::~slot_guard()`. A container that allocates there would throw
  out of a destructor and terminate — and the exception that gets it there is
  most often `bad_alloc` in the first place. The word allocates nothing and
  cannot throw, which is why `retire_ticket()` is `noexcept`. A gap wider than
  the window is counted in `prefix_overflows()` rather than absorbed silently;
  a test asserts it stays zero.
* **The old entry point is gone, not kept as a wrapper.** An entry point that
  does not know which ticket finished cannot tell in-order retirement from
  out-of-order retirement, and every caller that reached it during unwinding
  was silently the second kind. Taking the ticket as an argument makes that
  impossible to write again.

The bypass `catch` also stops calling `await_bypass_turn()` before retiring.
That wait was wrong twice over: a call that is throwing owes no response, so
parking it behind traffic it will never answer only delays the exception, and
the wait ran inside the unwinding, suspending a half-destroyed frame for an
arbitrary span of simulated time. Out-of-order retirement is now representable,
so the ordering it owed is enforced by the prefix instead of by that frame.

**The control, and what made one possible.** The routed path's own exception
sources are all allocation failures, which no test can inject here. The bypass
path calls its target directly from inside `b_transport`, so a target that
throws *is* injectable — and removing the wait from the `catch` is what puts
that path on the same `retire_ticket()` the routed path uses. One rule, one
implementation, exercised through the half that can be driven.

`throwing_target` and `failing_probe` in
`tests/test_noc_interconnect_local_bypass.cpp`; scenario `unwound`:

```text
ticket 0  routed to a 400 ns remote target, still in the mesh
ticket 1  bypass, answers at once, parks in await_bypass_turn()
ticket 2  bypass whose target throws, unwinds at ~150 ns

fixed        unwinding retire order: 2 0 1
mutated      unwinding retire order: 2 1 0   <- bypass overtook routed
```

Registry entry `unwinding-retire-ignores-ticket-order` restores the counter for
the out-of-order case only (`retired_ahead[slot] |= ...` → `++done_seq[slot]`).
It trips two checks, not one: the ordering check above, and
`prefix_overflows() == 0` — because the still-pending ticket then retires
*below* the count, which the window reports as a wrapped gap. The second
witness was not designed in; it is what the counter looks like from the other
side.

The registry needle for `admission-slot-released-before-ordering-wait` had to
be updated in the same edit, since it quotes the renamed call. The runner fails
loudly on a needle it cannot find, which is the only reason that did not become
a control that silently stopped biting — this file has recorded that failure
mode twice already.

### 2 — a debug refusal named the wrong kind of error

`INTERFACE_CONTRACT.md` §1 separates an invalid **command** from an invalid
**burst shape**, and `check_common_payload_rules()` reports that separation on
the normal transport path. Both debug transports folded the two into
`TLM_BURST_ERROR_RESPONSE`, so a `TLM_IGNORE_COMMAND` debug access came back
describing a burst it never had. §8 relaxes timing for debug; it does not
relax the error taxonomy — a caller that moves a payload between the two
transports must not have to know which one it used to read the status.

`debug_refusal_status()` now maps `payload_rule_error::command` to
`TLM_COMMAND_ERROR_RESPONSE`. Zero length and a null data pointer stay burst
errors deliberately: both describe a payload whose *shape* names no bytes.

Both directions are checked, because they are separate functions with separate
refusal branches. The burst case is checked too — without it, a debug path that
answered everything with `TLM_COMMAND_ERROR_RESPONSE` would satisfy the new
checks just as well.

### 3 — reset published a peak below the count it bounds

`reset()` deliberately leaves `*_in_flight_` alone: those calls are inside C++
stacks a reset cannot unwind, and zeroing the live counter under them makes
their destructors underflow — the Phase 8 lesson about reset releasing a port
it does not own, one level down. It then cleared the **peaks** to zero anyway.

The result is `peak < live`, which the definition of a high-water mark makes
impossible, and it stayed wrong after the old calls drained: the new epoch
reported a peak of zero for transfers it had begun with. The peak of an epoch
that starts with `n` calls in flight is at least `n`, so each peak is now
restarted from its surviving live count.

Sampled 50 ns after the existing in-flight reset, while chunk three is still
inside the target's 200 ns wait: `in_flight == 1` is asserted first, because
without it the check below would pass on a scenario where nothing was in
flight at all.

### 4 — the artifact manifests are scoped wrong, in both baselines

v1.4's and v1.5's `artifact_manifest.sha256` each cover two **living**
documents — `STATUS.md` and `NOC_MODEL_ARCHITECTURE.vi.md` — alongside the
frozen package. A living document changes by design, so each manifest goes red
the moment the component moves on:

```text
v1.4   docs/NOC_MODEL_ARCHITECTURE.vi.md: FAILED   (changed for v1.5)
v1.5   docs/STATUS.md:                    FAILED   (changed by §4j finding 5)
```

The review asked for `STATUS.md` to be restored. That was not done, and the
reason is that it does not fix anything: v1.6 has to change `STATUS.md` again,
and v1.5 would go red again — permanently. It would buy a green line that lasts
until the next baseline and then breaks for good, while hiding a defect that is
now visible in two baselines rather than one.

Neither manifest is edited either. A signature rewritten when it becomes
inconvenient signs nothing.

What is done instead is recorded in `STATUS.md` itself: the erratum, the
verification recipe that still works for the living documents
(`git show 8d94ca3:...`, **not** the `963466e` that `SIGNOFF.md` names — that
is the base of the dirty tree, and the signed bytes reached git one commit
later), and the scope rule that binds v1.6: an artifact manifest covers the
sign-off package only, and a living document the sign-off relies on has its
hash quoted inside `SIGNOFF.md`, which is itself hashed.

The four v1.5 logs were **untracked** when this was written; the `.gitignore`
rule that hid them was fixed in §4j finding 6, and commit `8652fb0` on
2026-08-24 tracked them along with v1.4's three. A fresh checkout can now
verify both log sets — what it still cannot verify is the living documents,
which is the scope error above and is not closed by any commit.

### 5 — the header claimed a measurement it had not made

`chip_noc_endpoint.h` said the outbound in-flight peak is 2 "in both NoC timing
modes". The composition asserts 2 in detailed and exactly **1** in fast, and the
difference is the quantity rather than noise in it: nothing downstream blocks in
fast mode, so a peak above 1 there means something suspended where it should
have annotated. Corrected in the header, which is where a user of the endpoint
API reads it.

### Measured after all five

```text
floo_noc_model            42/42
mutation registry         59 detected, 0 missed
TPU_V3 Release            55/55, 0 Skipped     (ctest -L tpu_v3)
TPU_V3 Debug              55/55, 0 Skipped
full tree Release        107/108, 0 Skipped
```

**The filter is quoted with the number because it changes the number.**
`ctest -L tpu_v3` selects by label and runs 55; `ctest -R tpu_v3` matches test
*names* and runs 38 — a subset that silently drops seventeen gates. This entry
was first written with the 38 and the wrong flag, which is the failure the
`Skipped`-counting rule exists to prevent arriving through a different door: a
green summary line over a set that was never the gate.

Three entries in that table are stated rather than rounded off, because each is
the kind of thing a summary line hides.

**`isp_register_bank_test` does not run, in either tree.**
`libsystemc.so: undefined reference to sc_main` — a component this work does
not touch, failing before it starts. It is named here rather than netted out of
the count, because "107/108" with no explanation is indistinguishable from a
regression.

**The four `noc_soc_*` gates are load-bearing for this change, and the Debug
tree was not running them.** `platforms/noc_soc` links
`cdc::components::noc_interconnect`, so those gates are the only place the
ticket-retirement change is exercised inside a whole SoC rather than a directed
bench. They pass in Release — `noc_soc_measurement_baseline` alone runs 498 s.
In Debug they reported `FAIL: noc_soc binary not found; set NOC_SOC_BIN`: the
platform target had never been built in that tree, so the gate was failing for
the absence of the thing it tests. Built and rerun rather than written off as
environmental; a gate that cannot find its binary is not evidence either way.

Rerun, two of them fail for a real reason — **and it is not this change**:

```text
rv32::CombinedMemoryInterface::_do_transaction, mem.h:65
Assertion `local_delay >= quantum_keeper.get_local_time()' failed
firmware mode, detailed NoC (DMA transfer), exited 134
```

Reverting `src/noc_interconnect.cpp` and its header to `HEAD` and rebuilding
reproduces it unchanged, so it is pre-existing rather than introduced here. It
is also older than Phase 9: the detailed contract that causes it — "**`delay`
is `SC_ZERO_TIME` on return**, always; everything the transaction cost has
already elapsed" — is present at `963466e`, before any of this work, and is
part of the signed v1.4/v1.5 architecture.

The conflict is structural. The wrapper spends time and returns zero; riscv-vp's
quantum keeper asserts the returned delay never falls below the local time it
has already accumulated. Release passes only because `NDEBUG` removes the
assertion, which means **the Release green on these gates is weaker evidence
than it looks** — the same disagreement is there, unchecked.

Not fixed here, and deliberately so: reconciling them is a decision about the
delay contract at the CPU boundary — either the detailed path stops returning
zero to a quantum-keeping initiator, or the platform drains the keeper before
the call — and neither is in the scope of these five findings. Recorded as an
open item rather than absorbed.

## 4l. Two findings from the window review

Both are defects **in the §4k fixes themselves**, which is the useful thing
about them: one is a false claim I wrote in a comment, the other is a parity
break I introduced while repairing a parity break.

### 1 — the completion-order window was sized by a bound that does not exist

§4k justified a fixed one-word window like this:

> The window bounds the *gap*, not the number of failures: it has to span the
> tickets concurrently outstanding on one port and channel, which the admission
> gate holds to `max_outstanding_per_port` plus the bypasses in flight. 64 is
> far above that.

**The admission gate does not hold it.** A local bypass takes no admission slot
— not creating routed work is the entire point of D1 — so bypass concurrency on
one port and channel is capped by nothing. Sixty-four parked bypasses put the
next ticket 65 above the prefix, off the end of the word, and the overflow
branch resolved that by advancing the prefix anyway: the exact FIFO violation
§4k existed to remove, reintroduced at the boundary, with `prefix_overflows`
recording it only after the affected callers had already returned. A counter
that fires after the damage is a weaker guarantee than an impossibility.

**The fix is where the growing happens, not how big the window is.** Any fixed
size is the same bug with a different constant, and growing it inside
`retire_ticket()` is impossible — that runs while an exception unwinds, where
allocating can terminate the process. So the window is grown in
`take_completion_ticket()`:

* taking a ticket is on the normal path. It may allocate, and if it throws it
  throws before the ticket is issued, which is a state the callers' guards
  already handle;
* retiring one only ever writes into storage that taking reserved, so it stays
  `noexcept` and allocation-free;
* `done_seq` never moves backwards, so a window sized when ticket `T` was
  issued still spans `T` whenever `T` retires.

That makes the overflow branch **unreachable by construction** rather than
unlikely. It is still counted rather than removed, because it runs during
unwinding where `SC_REPORT_ERROR` would terminate — and it now refuses to
advance the prefix. The failure mode chosen for the impossible branch is a
visible stall on the channel, not a silent reordering: wrong in a way that
stops, rather than wrong in a way that hands back plausible answers out of
order.

**The control needs sixty-six callers**, because the defect only exists past
the width of one word:

```text
ticket 0       routed to a 2 us remote target, still in the mesh
tickets 1..64  bypasses to an instant local target, all parked in
               await_bypass_turn()
ticket 65      bypass whose target throws, unwinding with gap 65

fixed     routed at index 1, first parked bypass at index 2
mutated   routed at index 65, first parked bypass at index 1
```

Registry entry `completion-window-overflow-advances-prefix` restores the fixed
64-bit test and the advancing overflow together — both halves, because either
alone is not the defect: shrinking the window without advancing merely trips
the counter, and advancing without shrinking is unreachable and would register
as a control that detects nothing.

### 2 — the debug taxonomy fix broke a different parity

§4k made `transport_dbg` distinguish a command error from a burst error, to
match `check_common_payload_rules()`. It then folded zero length and a null
data pointer into the burst case, with a comment calling that deliberate:
"both describe a payload whose *shape* names no bytes".

Both transports check those two conditions **separately from the payload
rules**, and both answer `TLM_GENERIC_ERROR_RESPONSE`
(`chip_b_transport`, `noc_b_transport`). So the same malformed payload came
back as a burst error through `transport_dbg` and a generic error through
`b_transport` — "the status depends on which API you used", which is precisely
what the fix was for. Reasoning independently about what each condition *ought*
to mean is how that happened; the mapping now mirrors the normal path in full
instead.

The parity checks call **both transports with the same payload and compare**,
rather than asserting a status literal on the debug side. A literal is a second
copy of the taxonomy that can drift from the first, and it is what let the
original break through. The null-pointer case is checked separately from the
zero-length one even though the transports test them in one `if`: nothing
forces a future split of that branch to keep them together.

### Measured after both

```text
floo_noc_model            42/42
mutation registry         60 detected, 0 missed
TPU_V3 Release            55/55, 0 Skipped     (ctest -L tpu_v3)
TPU_V3 Debug              55/55, 0 Skipped
```

## 4m. Three documentation findings

Nothing in this entry changes behaviour. That is what makes it worth recording:
every one of the three is a **statement about the code that the code stopped
making true**, written by me in the same edits that changed the code.

### 1 — the public header still described the window I had just replaced

`prefix_overflows()`'s contract in `noc_interconnect.h` said the window is
"a fixed 64-ticket window" and that a wider gap "falls back to advancing the
prefix regardless". §4l made both false: the window grows when a ticket is
taken, and the branch refuses to advance.

The consequence was worse than staleness. The old text told a reader that a
non-zero counter means "the ordering guarantee is degraded" — reordering. The
actual failure is now the opposite: the ticket is dropped and the channel
**stalls**. Someone debugging a hang would have read the one published
diagnostic pointing at it and been sent to look for a reordering that never
happened.

I updated the implementation comment in the `.cpp` and not the header in the
same edit. §4l closed with the observation that a wrong comment is more
dangerous than wrong code, because it is what the next reader believes instead
of checking. This is that, one file over, in the same change.

### 2 — the erratum about manifests was itself wrong about a manifest

§4l's predecessor stated that v1.4's and v1.5's artifact manifests "each cover
two living documents — this file and `NOC_MODEL_ARCHITECTURE.vi.md`". Reading
the manifests:

```text
v1.4   NOC_MODEL_ARCHITECTURE.vi.md, NOC_MODEL_ARCHITECTURE.vi.docx
       -- no STATUS.md at all
v1.5   NOC_MODEL_ARCHITECTURE.vi.md, STATUS.md
```

So the claim was wrong for v1.4 twice over: it names a file that is not there
and misses a `.docx` that is. The recovery recipe was incomplete to match — it
gave the command for `STATUS.md` only, which does nothing for the one member
v1.4 actually fails on.

And the two baselines need **different commits**, which the corrected text now
says explicitly: v1.5's `STATUS.md` is at `8d94ca3`, one commit past the
`963466e` its own `SIGNOFF.md` names, while v1.4's architecture document is the
copy `963466e` still carries — because v1.5 is what changed it. Reaching for
one commit for both is the mistake the recipe now warns about.

Writing an erratum about unverified claims without verifying the erratum is a
particular way to be wrong, and it is recorded rather than quietly corrected.

### 3 — the restored handoff presented a v1.4 snapshot as current guidance

`AI_HANDOFF_CONTEXT.md` was restored earlier in Phase 9 after a review found
its deletion had removed an architecture source artifact still referenced as
required evidence. Restoring it was right; leaving it saying "an AI or engineer
taking over this work should read this document before making changes" was not.
It stops at 2026-08-07: 41 tests, 51 controls, no D1, no D24–D26, no completion
ordering, no admission-slot ownership.

The risk is concrete rather than tidiness. Several invariants added since are
not derivable from anything in it, so someone following its own instruction and
then editing `src/noc_interconnect.cpp` could undo them without noticing.

Labelled rather than rewritten. Rewriting 180 KB to match a tree the document
was never verified against replaces a dated record with an unverified one,
which is worse than a dated record that says so. A new section 0 states the
snapshot date, tabulates what has changed, and names the reading order for the
current state.

## 4n. Two findings the parity claim brought into view

Both are in code that predates this phase's edits. They surface now because
§4l–§4m claimed the debug path mirrors the normal path's taxonomy and that D24
had been retracted — and a claim is what makes the places it does not hold
findable.

### 1 — a silent downstream target's byte count was read as a refusal

`transport_dbg` has no obligation to set a response status; many targets just
return a byte count. So a payload that passes every check in this endpoint and
then finds **no mapped target** downstream comes back with the count zero and
the status untouched, and the endpoint has to say what happened.

It said `TLM_GENERIC_ERROR_RESPONSE` for anything short of a full serve.
`INTERFACE_CONTRACT.md` §1 reserves that for "a target that decoded the access
and refused it", and gives `TLM_ADDRESS_ERROR_RESPONSE` to "an address that
does not decode". Nothing decoded this one — and the same address through
`b_transport` comes back an address error. So the debug and normal paths
disagreed again, one layer below where §4m fixed them.

The byte count is the only evidence available, and it carries exactly the
distinction §1 draws:

```text
served == requested   OK
served == 0           nothing claimed the address     -> ADDRESS_ERROR
0 < served < needed   something decoded it and stopped -> GENERIC
```

The one case this cannot separate is a target that decodes an access and
refuses all of it, and that is the case §1 tells the *target* to set its own
status for. Guessing wrong for a target that stayed silent when the contract
told it to speak is the right way round, and it is stated in the helper rather
than left implicit.

Both branches are checked, not just the interesting one: without the partial
case a path that answered every short serve with an address error would pass.
`recorder::debug_bytes` models the three downstream behaviours and deliberately
never sets a status.

### 2 — the report contradicted itself, and a test held the contradiction open

D24 was retracted in §4j when the composition measured a peak of 2, and
`outbound_in_flight()` / `peak_outbound_in_flight()` were added. Three things
were left behind:

* `report()` printed the in-flight peaks and then, three lines later, "outstanding
  is deliberately not measured here (decision record D24)" — the same function
  printing a number and denying it exists;
* **the gate asserted that sentence was present**, so the test was not merely
  failing to catch the contradiction, it was requiring it. A correct `report()`
  would have failed the suite;
* D24's "What this forbids" still banned adding an endpoint outstanding counter
  without reopening the decision, while the counter was already there.

The test now asserts the retracted sentence is **absent**. That direction is
the point: asserting the presence of correct text lets a revert pass, and this
is the second time in Phase 9 that a check written to lock in a claim locked in
the wrong one instead.

D24's prohibition is marked discharged rather than deleted. It required a
recorded decision instead of a quiet addition, and it got one — the clause is
why the counter arrived with a measurement and a retraction attached. What
replaces it is narrower and still binding: the in-flight figure is per
**transfer** and may not be compared with `noc_interconnect`'s per-port
outstanding count, which is per transaction.

### Measured after both

```text
TPU_V3 Release            55/55, 0 Skipped     (ctest -L tpu_v3)
TPU_V3 Debug              55/55, 0 Skipped
```

`floo_noc_model` and its 60 controls are unchanged by this entry — it touches
`components/TPU_V3` only — so the v1.5-delta figures in §4l stand.

## 4o. Multi-chip contention: seven ways the gate was green for the wrong reason

`test_chip_multi_on_mesh` closes the §6 evidence item "2x2 concurrent
inter-chip traffic completing without deadlock or response misattribution". The
scenario, the measurements and the boundaries are in
`TPU_V3_PHASE9_NOC_REBASELINE.md` §7a. This entry records how it got there,
because the gate passed **seven** times before it was worth anything, and the
last three were only found by review.

### Found while building it

**1. `error_cause = 3` — both DMA endpoints external.** Revision 1 implements
exactly `local -> external` and `external -> local`. The first draft moved chip
B's SRAM into chip A's **core 1** SRAM, and core 1 is on the same chip but
reached through the chip fabric — external too. "Same chip" and "local to this
DMA" are different properties.

**2. `wait()` outside a process (E519), in detailed mode only.** The read-back
checks were in `sc_main` after `sc_start`; the detailed backend spends time
inside `b_transport`, so those reads call `wait()` with no process to suspend.
**`fast` hides it completely.** The single-chip gate carries a comment saying
exactly this, and this file was written against that file and still learned it
from the kernel.

**3. The sampler stepped over the window it existed to measure.** Global RAM
answered in 2 ns while `contention_watch` sampled every microsecond. This one
did not fail — it reported zero overlap, which in `fast` is the correct answer.
Found by asking how wide the window was compared with the sampling period.

**4. `> 0` is not a measurement.** With the sampler fixed the run reported
**three** samples and passed. The floor is now 20.

### Found by review

**5. The overlap predicate accepted the case it existed to exclude.** It read
`oa > 0 || a_.outbound_in_flight() > 0`. A caller blocked in the *admission
gate* has already incremented the endpoint's in-flight count, so a serialisation
regression parking port B before admission would still have shown both managers
busy while `ob == 0`. The predicate is now `oa > 0 && ob > 0` — D26 makes a
routed call own its admission slot from admission until `b_transport` returns,
so that is non-zero exactly while the manager is in the network. "In the
endpoint but not admitted" is kept as a separate published figure rather than
folded in.

**6. The evidence answered a different question from the one it closed.** Round
1 puts both chips on the mesh at once — but both are reading *global RAM*. That
is multi-chip traffic to a shared target, not **inter-chip** traffic, which is
what §6's wording names. The one chip-to-chip transfer ran alone, after
everything else had drained. Every check passed and the measurement was real;
it was a measurement of something else. Round 2 now runs both chips reading
each other's SRAM simultaneously, in opposite directions.

This is the most dangerous kind of green in this file. The other six are code
that is wrong. This one is code that is right, measuring honestly, filed under
a claim it does not support — and a sign-off inheriting §6 would have inherited
it silently.

**7. The runtime diagnosis was wrong, including the reviewer's.** The gate took
113 s per case against 7.5 s for the single-chip composition next door. The
review attributed it to the payload readback: 8 KiB verified as 2048 serialised
four-byte mesh transactions. Batching those into 1 KiB blocks cut the flit
count from 27110 to 11940 — and left the runtime **unchanged at 113 s**.

The cost was `sc_start(60 ms)`: four harts stepping 60 ms of simulated time
after the verifier had finished at about 1 ms. `sc_stop()` when the work is
done takes it to **0.97 s fast, 1.20 s detailed** — a hundredfold, and now
faster than the gate it was compared against.

The batching was kept anyway, for a reason the review did not give: 2048
verification transactions were burying the `accepted flits` figure the run
reports about *itself*. Fixing a symptom on a wrong diagnosis is worth doing
when the symptom is independently real — and worth recording as such, so the
next person looking for the runtime does not follow the same wrong lead.

### One more, in the fix for 6

Round 2's verification read 56 bytes at a 4-aligned address and was refused. A
chip aperture is `mmio`, and the interconnect will not widen a read whose
address or length is not a multiple of 8 — the D25 rule. The endpoint reshapes
for the *chip*; the host is a plain master with nothing to reshape for it, so
it has to ask only for spans the mesh will carry. `read_bytes()` now reads the
enclosing 8-aligned span and trims.

### The controls

| Control | Bites | Establishes |
|---|---|---|
| MC-1 D25 read shaping disabled | yes — 5 checks; both inter-chip legs error and deliver wrong bytes | the inter-chip path really traverses D25 |
| MC-2 both chips given the same RAM window | yes — both directions of the misattribution check | the check discriminates rather than passing on any data |
| MC-3 synchronised `START` removed | yes, **round 2 only** — 18 samples against 25 | the two-phase start is load-bearing where transfers are short relative to the register writes preceding them |

**MC-3's answer changed when round 2 was added.** Against round 1 alone it did
not fire (49 against 53), because the shared 2 us RAM latency dominates there;
an earlier draft generalised that into a source comment claiming the
synchronised start was "a tightening, not the mechanism". Round 2 disproved the
generalisation. The comment is now stated per round, and the earlier negative
result is kept rather than deleted — it is what makes the distinction visible.

MC-1 is still the only one of the three that tests the **system** rather than
the test. What it leaves open is in §7a: it disables the shaping branch
outright, so it says nothing about a subtle lane error inside a reshaped read,
which the byte-exact comparison would catch and no control has yet injected.

### Measured

```text
TPU_V3 Release   57/57, 0 ***Skipped   (ctest -L tpu_v3)
TPU_V3 Debug     57/57, 0 ***Skipped
```

Counted with `grep -c '***Skipped'`, not `grep -c Skipped`: ctest names a
skipped test twice, so the plain form returns double and reads correctly only
when the answer is zero — which is exactly when nobody checks it.

## 4. What these controls do not cover

Stated so they are not read as broader than they are.

* **The endpoint's own gate runs against TLM stubs**, deliberately — the
  split-and-route contract is visible without a mesh. A real
  `noc_interconnect` appears in two other places for two other questions:
  `test_endpoint_on_real_noc` (§4f, D25) and `test_chip_on_mesh` (§4i), and it
  is the second that supplies mesh-side conservation counters.
* **No control covers a reset landing between a chunk's commit and its byte
  publication**, because with per-chunk publication there is no longer a gap
  there to land in. That is an argument, not a measurement, and it is recorded
  as such.
* **The `*_target_errors_` counters count transfers, not chunks.** The first
  failing chunk stops the transfer, so at most one increment per transfer. That
  is the intended meaning and not an accident, but nothing forces it: a future
  change that kept issuing chunks after a failure would silently change what
  the counter means.

## 5. Measured, and what is still open

```text
Release  ctest -L tpu_v3   55/55, 0 failed, 0 skipped
Debug    ctest -L tpu_v3   55/55, 0 failed, 0 skipped
   (verified by counting Skipped lines, not by reading "100% tests passed" --
    see the note below, which is why that distinction is spelled out here)
tpu_v3_noc_endpoint --repeat until-fail:5, both build types: pass
eleven endpoint negative controls, run by hand: 11 detected, 0 missed
two floo_noc_model controls for R-P9-2 (RP92-a, RP92-b): 2 detected, 0 missed
floo_noc_model mutation suite after R-P9-3: 58 detected, 0 missed
  (55 pre-existing + the two D26 controls, now registered rather than hand-run)
floo_noc_model RTL cross-checks after R-P9-3: 12/12 passed
floo_noc_model component suite: 42/42 after D26
```

Two controls had to be **rewritten before they counted**, and both are worth
recording rather than quietly fixing. READ-a's first mutation crashed instead
of failing (§4c), and E-1's mutation was first written against
an anchor that D24's own change then moved, so the script applied half of the
mutation, threw, and left the tree in a state that failed four checks instead
of one. Four failing checks look like a stronger result and are a weaker one:
the mutation under test was not the one that ran. It was re-anchored and
re-run, and it now fails with exactly the one check it was written for. A
control whose mutation only partly applied is not a control, and the way it
presents is *more* failures, not fewer.

52 is Phase 8's 51 plus the endpoint gate.

Still open in Phase 9, and none of it is closed by this entry:

1. ~~Plan §11.10's outstanding and latency counters.~~ **Closed by D24**, see
   §4a: transfer latency is measured here, outstanding defers to the
   interconnect, and neither figure may be quoted as the other.
2. **The D23 reset-of-in-flight rule end to end at the NoC boundary.** The
   endpoint-local half is implemented and gated; the rule across the boundary
   is not.
3. **Single-chip real-mesh composition gate: implemented** 2026-08-24 (§4i),
   in both NoC timing modes. That is deliberately not called "composition
   done": plan §11.11 gives system composition to `platforms/TPU_V3_SoC`, and
   the platform top still instantiates no hart, accelerator or NoC. What exists
   is a **testbench** that wires `tpu_chip`, the endpoint, a real
   `noc_interconnect`, boot ROM and global RAM. Still open under this heading:
   the platform composition itself, and mesh scaling to 2x2 and up to eight
   chips.
4. The six §6 evidence items, of which only the D1 local-bypass ones are done.
5. ~~**R-P9-1**: the chip aperture is one `target_kind`.~~ **Closed by D25**
   (§4f): `mmio` is kept and the endpoint shapes outbound reads into a chip
   aperture so the interconnect never widens them. What the composition task
   still owes is the inter-chip **DMA** case — remote SRAM to local SRAM at an
   odd source address and length — which needs `tpu_chip` on the mesh.

## 5a. A reporting error of mine, corrected

Every "0 skipped" in the earlier revisions of this document was **unverified**.
`ctest` prints `100% tests passed, 0 tests failed out of N` whether or not some
of those N were skipped, and I grepped that line and reported "0 skipped" from
it. Raised in review on 2026-08-24, and true at the time: `rvv_smoke_execution`
and `riscv_vp_plusplus_portable` were both `***Skipped` in each build because
`fw/TPU_V3_SoC/rvv_smoke/rvv_smoke.elf` was not present, so the real figure was
53 executing and 2 skipped.

Those two are the CPU-execution gates, so it is not a cosmetic difference: the
suite was reported as fully green while the two tests that actually run guest
code were not running at all.

Fixed by building the image — `cmake --build <tree> --target rvv_smoke_image` —
after which both trees are genuinely 55/55 with zero `Skipped` lines. The
numbers in §5 are now taken by counting those lines rather than by reading the
summary.

Worth naming as a habit rather than a slip: a summary line that stays green
when something silently did not run is the most expensive kind of output to
trust, and this file spends most of its length on exactly that failure mode in
other people's code.

## 6. One observation, recorded rather than fixed

`test_chip_noc_endpoint` constructs many `bench` objects whose members carry
fixed names (`mesh`, `chip`, `chip_side`, `noc_side`), so SystemC emits a
`W505 object already exists` warning per collision and renames the duplicates.
It is cosmetic — every check indexes its own bench object, not a SystemC name —
and it predates this entry. It is noted because the renamed names appear in any
`report()` taken from those benches, which would be confusing to read, and
because the fix is one prefix away should anyone care.
