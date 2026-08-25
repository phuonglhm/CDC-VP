# TPU_V3 Phase 7 Audit — the NEO-CORE composition

> **Active implementation baseline under D27, not a phase to extend toward a
> chip.** Reuse this one-core `tpu_core` directly for the standalone DSE. Do not
> proceed from this audit into Phase 8. The next task is G2/MB1 in
> [NEO_CORE_MICROBENCH_DSE_PLAN.md](NEO_CORE_MICROBENCH_DSE_PLAN.md).

Date: 2026-08-20

Result: **complete**, review findings closed.

Phase 7 built nothing new except one component. Everything else it did was
*wiring*, and every defect it surfaced was a wiring defect — each one invisible
to the component gates that had already passed, because each one only exists
when two correct parts are put together.

That is the useful summary of this phase: five of the six defects below were
found by composing, not by testing harder.

---

## 1. What was built

| Piece | Why it exists |
| --- | --- |
| `neo_hart_port` | VP++ exposes **one** combined fetch/data socket and `neo_local_sram_fabric` exposes no TLM target, so nothing could bind a hart to the D15 planes. Missing from plan §10 until this phase; §11.7 now specifies it |
| `tpu_core` | The composition: hart, core SRAM, three planes, DMA, MXU, Transform, IRQ aggregation, hierarchical reset |
| `neo_payload_rules.h` | The TLM-to-native payload rules, shared with `neo_external_bridge` rather than copied. `INTERFACE_CONTRACT.md` §2 singles out the byte-enable expansion as a rule whose violation is "a wrong result and an overread at once", and the bridge had already shipped that exact defect once |
| `fw/TPU_V3_SoC/neo_core_pipeline` | The firmware the gate asks for: drivers for DMA, Transform and MXU, and the `DMA -> Transform (Im2Col) -> MXU -> RVV` pipeline |

`tpu_v3_neo_core` builds only when both accelerator options and the CPU backend
are present. Half a NEO-CORE is not a NEO-CORE — D14 freezes one MXU per core
and the constructor validates it — and plan §10 already warns that an exported
target which links and does nothing is worse than a missing one.

## 2. Defects found by composing

Each of these passed every gate that existed before Phase 7.

**`reset_cpu()` threw on its second call.** `ISS::init()` calls `genOpMap()`,
which fills `opMap[].labelPtr` and refuses to overwrite a filled entry. The ISS
constructor is what leaves those null, so the map survives an `init()` and the
*second* call always failed. Audit F8 had established that `init()` is a restart
rather than an architectural reset — on the assumption that it could at least be
called. It could not. Fixed in the wrapper by clearing only `labelPtr`, leaving
`opId` and `instr_time`, which carry the per-operation timing model.

**The external bridge had one outbound socket for two initiators.** A NEO-CORE
has exactly two things that leave it — the hart, which must reach global boot
ROM for its first fetch, and the DMA. SystemC refused the second bind. Resolved
by giving the bridge a named `outbound_initiator` vector, which also gives
outbound traffic the identifiable owner `INTERFACE_CONTRACT.md` §5 requires and
it previously lacked.

**`sa_control` drove its IRQ from two processes.** The register paths run in
whichever process issued the MMIO — the hart's thread once a core is composed —
while `observe_completion` is a clocked method. In the standalone bench the
MMIO never came from the hart's thread, so the signal never acquired a second
driver. `neo_dma` and `image_transform` already used a single-writer method
woken by an event; `sa_control` now does too.

**The dbbcache cycle accumulator survived a reset.** With the hart executing
across a reset, `mcycle` did not drop: `init()` sets `cycle_counter_raw_last = 0`
while the accumulator keeps its value, so the first `commit_cycles()` afterwards
re-added the whole pre-reset count — into simulated time, not merely into a
counter. No wrapper-only path exists, and the obvious workaround would push
exactly that count into the quantum keeper. Closed by
`0004-d19-cycle-baseline-survives-reset.patch`, the fourth patch under the D8
mechanism, which decision record D19 had anticipated and authorised.

## 3. Review findings (2026-08-20) and their disposition

**Reset contradicted committed-data semantics — the one that mattered.**
`tpu_core::reset()` reset the engines and then called `sram_.reset()`, which
wipes the backing store. Meanwhile `neo_dma` and `image_transform` both keep
their committed-byte counts across a mid-job reset, because plan §11.5 and D17
say bytes already committed to the destination stay committed. Put together,
those two are incompatible: `BYTES_DONE` would report N bytes committed to
memory that had just been erased. The register would be right and the memory
empty.

The semantics are now pinned: **core SRAM keeps its contents across a core
reset.** Two reasons, and the second is what makes it a correctness question
rather than a preference. A real SRAM does not lose its cells to a logic reset;
reset is a control-path signal. And the committed-byte contracts of three
engines are only meaningful if the memory survives. `core_sram::reset()` remains
what it always was — a component-level operation for test setup and platform
initialisation, where a deterministic all-zero start is the point (D6).

The existing reset scenario could not have caught this: it reset an idle core,
which never puts the two claims in the same room. A mid-job scenario now does,
and reverting the change makes it fail with the reason named.

**Two more reset defects the same finding exposed.** `matrix_.reset()` was
called directly, immediately before `sa_control_.reset()` — which resets the
engine itself, and has to, because it first samples whether a job was in flight
and snapshots the engine's committed bytes for the abandoned-accounting owner.
Resetting the engine first zeroed exactly those counters and opened a new
traffic epoch, so the snapshot recorded zero and `C_BYTES_DONE` lost bytes the
job had committed. And `i_rstn` was written `true` once at construction and
never pulsed, so the clocked Sauria modules — feeders, array, PSM, Control FSM —
never saw a hardware reset at all while everything around them started clean.

**Firmware built into the source tree.** The image was produced next to its
sources, which is what `fw/TPU_V3_SoC/rvv_smoke` does and which is wrong for
four independent reasons: Release and Debug race for one output file, a
read-only checkout cannot build, a stale gitignored ELF can be run without
anyone noticing it was never rebuilt, and the two configurations silently share
whichever won. Phase 4.5 already builds firmware under
`${CMAKE_CURRENT_BINARY_DIR}`; this now follows it.

**`mhartid` was not proved through firmware.** The C++ test asserted the model's
own accessor, which shows the model agreeing with itself. `ARCHITECTURE.md` §2
says firmware derives chip and core index from `mhartid` and is given no other
identity, so the firmware now reads the CSR and the host checks it against
`chip * 2 + core`.

**RVV inline assembly named no vector-register clobbers.** The block uses `v8`,
`v12`, `v16` and `v17` while listing only `"memory"`. At `-O1` the result was
correct, which is the problem rather than a reason to leave it: the compiler is
entitled to keep a live value in one of those registers, and the failure would
appear as wrong data after an optimisation-level or compiler change.

**Not reproduced: `git diff --check` failing on the D19 patch.** The reported
space-before-tab is the unified-diff context character followed by the source's
own tab indentation, and it is present in all four patches — 0002 has 160 such
lines against this patch's 3. `git diff --check` is clean, staged or unstaged.
Recorded rather than silently dismissed, so the next reviewer does not re-raise
it.

## 3a. Second review round (2026-08-20)

The reset rewrite above was itself reviewed, and three things came back. All
three were real.

**The reset pulse opened a one-cycle window.** Asserting `i_rstn` and *waiting*
before telling the components to reset let a job that was less than one cycle
from done finish inside that window. The handlers then took their **idle**
branch — `neo_dma::reset()` zeroes `BYTES_DONE` when nothing is busy — so a
caller that reset an active job received the semantics of resetting an idle
one: data retained in SRAM, committed count reported as zero. The same class of
contradiction the rewrite existed to remove, reintroduced by the fix for it.

Closed by asserting the line, performing every synchronous reset in the same
instant, and only then holding the line for a clock period. No simulated time
passes between the components being told to reset and their state being
captured, so nothing can complete in between.

Worth recording how the gate for it behaved, because it nearly passed for the
wrong reason. A sweep at whole-cycle resolution **did not** catch the defect:
each iteration starts its job at a different phase, and the sweep stepped over
the window. Raised to quarter-cycle steps, the negative control fails at 21
cycles — an active job with 256 bytes committed dropping to zero. A negative
control that passes is not evidence; it is a test that has not been calibrated.

**SRAM counters were left running across a new counter epoch.** Keeping the
data was right and keeping the counters with it was not: the fabric and every
requester open a new epoch on reset, so storage counters carried across it stop
reconciling with the fabric's, and conservation is a per-epoch property.
`core_sram` now separates the two — `reset()` still releases the pages and
clears the counters for component and platform use, while `reset_counters()`
does what a core reset needs.

**The `mhartid` check could not fail.** It ran on chip 0, core 0, so the
expected value was zero — which is also what an uninitialised CSR reads, so an
integration ignoring the configured identity entirely would have passed. The
pipeline now runs on chip 1, core 1, giving hart 3, which also separates
`chip * 2 + core` from `chip + core` and from plain `core`. Verified by making
the firmware write a constant instead of reading the CSR; the check fails.

This is the same failure mode audit `TPU_V3_PHASE5_AUDIT.md` §7 named — a check
that exists, passes, and could not have failed — and it appeared twice in one
review round, in a test written by the same hand that quoted the principle.

## 3b. Third review round (2026-08-20)

Two findings, both in the fix for the previous two. That pattern is the point
of this section.

**Requesters were not quiesced during the reset pulse.** The second version
performed the synchronous resets and *then* yielded to hold `i_rstn` low. The
hart that `reset_cpu()` had just restarted, and any inbound initiator, were free
to issue traffic during that yield, so `reset()` could return with counters or
registers already repopulated.

The fix is not a third ordering. `reset()` now consumes **no simulated time at
all**: it asserts the line, resets every component, and returns, atomically.
There is no window because there is no yield. The line still has to be held
across a clock edge for the clocked Sauria modules, so it is released by a
separate method one clock period later, and the residual window that leaves is
bounded and *defined* — the adapter's `drive_host` loop watches `i_rstn` and
abandons an operation with `error_cause::aborted` while it is low, so work
started in that period fails cleanly rather than running against modules held in
reset.

Getting there also produced the third instance in this phase of one signal with
two drivers: `reset()` runs in its caller's process and the release runs in a
method, so both writing `reset_n_` is an elaboration error. Routed through a
single writer, the same shape `neo_dma`, `image_transform` and — since this
phase — `sa_control` use for their interrupt lines.

**Counter-only reset cleared the loader's accounting.** `reset_counters()` also
zeroed `debug_bytes_written_`, which records that a loader ran. Now that a core
reset keeps the stored data, clearing that counter left the bytes present and
the report saying nothing had been loaded — the counter denying the memory it
describes. It survives `reset_counters()` and is cleared only by the full
`reset()`, alongside the pages it accounts for.

### Fourth round (2026-08-20)

**A job admitted during the reset pulse was not refused.** The comment written
in the previous round claimed the adapter's `drive_host` loop watched `i_rstn`
and would abandon work started while the line was low. It did not — only
`wait_for_done()` read the line, at a point operand prefetch normally reaches
*after* the one-cycle pulse has deasserted. So a `START` in that window
configured the clocked modules while they were held in reset, had those writes
dropped, and then ran the array on a configuration nobody applied: a wrong
result presenting as an arithmetic defect rather than a reset-timing one.

That comment is worth naming for what it was — a guarantee asserted from a
partial reading of the function it described, written in the same round that
recorded two earlier instances of the same habit.

Closed at all three points a job can enter the window: `sa_control` refuses
admission, `sauria_matrix_adapter::submit()` refuses it, and `drive_host()`
abandons a configuration write if the line drops part-way. `submit_status` and
the SA `error_cause` both gained `engine_in_reset`, distinct from `aborted`
because a job that never began and a job abandoned mid-flight call for
different firmware responses.

The gate for it exposed one more thing. Testing it against `i_rstn` failed:
`reset()` returns in zero time while an `sc_signal` updates a delta later, so a
`START` issued immediately after `reset()` returns still saw the line high. The
admission gate is therefore a plain flag that `reset()` sets synchronously, not
a read of the signal — the signal remains what the clocked modules watch.

### Fifth round (2026-08-20)

**The native path kept the hole the MMIO path had just lost.** The synchronous
admission flag was added to `sa_control`, which is the register file — and
`tpu_core::matrix_engine()` is public, so a caller reaches `submit()` without
going through it. That path still gated on `i_rstn` alone and therefore still
accepted work in the delta between `reset()` returning and the signal updating.

The adapter now carries the same synchronous flag, checked alongside the
signal: the flag closes the delta window for the core that owns the engine, and
the signal still covers a reset driven by anything else. Deliberately not a
defaulted virtual on `sauria_matrix_if` — that is the shape D5 rejected for CPU
identity, where a composition that forgot to wire it would elaborate and look
correct.

**The programming-model version had not moved.** `error_cause::engine_in_reset`
and the new `START` refusal are both firmware-visible, and `model_version` sat
at 1 next to a comment saying it is bumped exactly for that. Now 2, with the
change named, so a driver reading `VERSION` can tell this ABI from the one
where cause 13 did not exist.

### What three rounds on one function are worth recording

The same function produced a finding in each round: it wiped memory, then it
left a completion window, then it left an interface window, then it left a
window in which work could be admitted through MMIO, then the same window on
the native path. Each fix was
locally correct and each missed the same thing — that reset is not a sequence
of steps but an instant during which nothing may observe an intermediate state.
The version that finally holds is the one that takes no simulated time, and it
is shorter than the two before it.

The admission gates took two rounds for a narrower reason, and it is the one
worth carrying into Phase 8: an entry point was closed and declared shut
without first enumerating the entry points. There were two — the register file
and the public native handle — and the second was found by a reviewer rather
than by the person who had just written "closed at all three points".

Worth keeping in view when Phase 8 gives the chip a second core and a shared bus
lock: reset there is the same problem one level up.

## 4. Gate evidence

| Gate line | Evidence |
| --- | --- |
| One firmware ELF boots and controls every available block through MMIO | `tpu_v3_neo_core_pipeline` |
| Control on AXI4-Lite, local data on the native fabric, remote on the bridge; data matches the golden model | all three hart-port destinations non-zero; the Im2Col matrix *and* the INT32 product compared against a host recomputation, not against the model's own code |
| `neo_hart_port` decode, straddle refusal, byte-enable expansion with a negative control | `tpu_v3_hart_port`; the negative control was injected and observed to fail with the reason named |
| Scalar and RVV remain one hart/memory path | asserted structurally: `has_unified_bus()` and `&instr_bus() == &data_bus()` |
| The D19 reset gate | `architectural_reset`; the `mcycle` check required the fourth patch, and its own negative control was verified |
| Unmapped, unavailable, misaligned, reset and injected-error | `tpu_v3_hart_port` (unmapped, misaligned) and `tpu_v3_neo_core` (unavailable Col2Im with zero SRAM traffic, injected error, idle reset, mid-job reset) |

The golden comparison deliberately recomputes Im2Col from the D18 layout and the
product as an ordinary INT8→INT32 triple loop. Calling the model's own Im2Col or
GEMM would agree with the code under test by construction.

## 5. Measured

```text
Release   42/42 tpu_v3 tests, zero skips
Debug     42/42, zero skips
```

A full `cmake --build .` still reaches `isp_register_bank_test`, an unrelated
existing target that fails to link because it provides no `sc_main`. It is not
part of TPU_V3, was not modified, and is recorded here only so it is not
mistaken for Phase 7 evidence — the same disposition Phase 5 gave it.

## 6. Phase 8 handoff

Phase 7 composed one core. Two cores in a chip is Phase 8, and the platform
composing chips is Phase 9; decision record **D21** keeps a NEO-CORE binary
internal in either case, so the packaged `out/tpu_v3_soc/` keeps
`sauria.linked` false and the packaging regression keeps asserting it.

One thing Phase 8 will meet immediately: the bus lock is per-CPU. Each
`riscv_vp_plusplus_cpu` constructs its own `local_bus_lock`, so two harts
sharing an address space would not exclude each other for `lr`/`sc` or AMO.
That is the multi-hart AMO gate D8 deferred with upstream `52d376d4`, and it
needs an API decision first: `bus_lock_if` is an upstream type and the wrapper's
public header deliberately exposes none, so sharing a lock chip-wide should go
through a CDC-VP-owned handle rather than leaking the upstream header. The
wrapper's own comment still says "Phase 6", which is a stale phase number from
before the plan was renumbered.
