# TPU_V3 Phase 8 Audit — the dual-core TPU chip

> **HISTORICAL / INACTIVE UNDER D27 — DO NOT CONTINUE THIS SCHEDULE.** The
> dual-core chip source and evidence are retained, but the active machine is one
> standalone NEO-CORE with no chip composition or NoC. This file is not a work
> queue or prerequisite. Start with [README.md](README.md).

Date: 2026-08-20

Result: **complete**. Gate items met, decision record D8's deferred item closed
by D22.

Phase 7 composed one NEO-CORE and found that composition is where the
interesting defects live. Phase 8 composes two of them, and the pattern held:
the defects it surfaced are all about what happens when a resource that had
exactly one user acquires a second.

---

## 1. What was built

| Piece | Why it exists |
| --- | --- |
| `chip_local_fabric` | Decode and arbitration between the two cores, the chip's register windows and the one external port. The component that makes local containment across a chip a decode consequence rather than a routing decision |
| `tpu_chip` | The composition: two NEO-COREs with distinct hart ids, chip register files, one aggregated NoC boundary, one shared bus lock, hierarchical reset |
| `cdc::cpu::shared_bus_lock` + `attach_bus_lock()` | The CPU backend's LR/SC and AMO lock, made shareable. Opaque in the public header, because that header exposes no VP++ type |
| `register_block::chip` / `::chip_counters` | Distinct identity values for the two chip windows, so a driver that computed a core base where it meant a chip base reads something wrong instead of something plausible |
| `test_bus_lock_atomicity` | The multi-hart AMO contention gate D8 made a Phase 8 precondition, with its negative controls registered as tests |
| outbound arbitration in `neo_external_bridge` | A core has two named outbound initiators and one external socket. Nothing arbitrated them, and the chip fabric correctly refuses a second transaction from one core — see §2 |
| `test_neo_outbound_arbitration` | The bridge arbiter's own gate. A separate executable because it needs a target that actually waits, which is the only condition in which the arbiter does anything; the Phase 3 bridge bench deliberately has none |

`tpu_v3_chip_fabric` builds in every TPU_V3 test build: it is a decoder and an
arbiter and needs neither a CPU backend nor an extracted accelerator.
`tpu_v3_tpu_chip` is gated on exactly what `tpu_v3_neo_core` is gated on,
because it is two of them.

## 2. Defects found by composing

**A per-hart bus lock excludes nobody.** Each `riscv_vp_plusplus_cpu`
constructed its own lock, and the comment in the wrapper had said since Phase 2
that this "is correct only for a single-hart system". It was correct, and it was
also invisible: no test in the tree had two harts sharing an address space. With
two harts and one lock each, an `amoadd.w` loop lands **exactly half** its
updates — the two harts interleave in lockstep and every second read-modify-write
is lost. Fixed by making the lock shareable and having `tpu_chip` create one.

**A core's hart and its DMA could both be inside the core's one external
socket.** `neo_external_bridge` has two named outbound initiators and one
`external` socket, and it forwarded straight through without arbitrating them.
The chip fabric treats each core as one initiator and allows it one transaction
at a time, so the second one throws a model defect — which is the right answer,
and which nothing could reach while every downstream target merely annotated
delay. Running the composition gate with the chip fabric in `arbitrated` mode
reproduced it in seven microseconds:

```
Error: (E549) uncaught exception: tpu_v3::chip_local_fabric[chip.chip_fabric]:
initiator 'core0' issued a second transaction while one was still in flight
In process: chip.core0.dma.worker @ 7360 ns
```

Fixed in the bridge, which is where it belongs: the bridge is what aggregates
two named initiators onto one port, so it is what arbitrates them — rotating
priority, held across the downstream call, with per-initiator grant and conflict
counters. In a fully annotated system the arbiter never yields and changes
nothing; the moment anything downstream blocks it is load-bearing, and Phase 9's
first real NoC hop blocks. The composition gate now runs in **both** timing
modes as separate CTest cases so the mode that can reach this stays in the
suite.

**The chip fabric's arbiter serialised its own bookkeeping, not the port.** The
first version granted a port, waited one fabric cycle, released it, and *then*
forwarded downstream. Two initiators were therefore inside one target at the
same time while the arbiter recorded a clean alternation. The bench's
`peak_in_flight` counter on the probe is what caught it; the fix is that acquire
and release are separate calls with the downstream transaction between them.

**The fairness observable was measuring the wrong thing.** The bench identified
which initiator a port was serving from a variable the driver set before
calling. Between that assignment and the arrival the fabric blocks, so the
second contender overwrote it while the first was still queued — every grant was
attributed to whoever had called most recently. Once the arbiter was fixed this
produced a perfectly alternating sequence with the two labels swapped, so it
would have passed either way. The contenders now write their own index into the
payload and the target reads it out of the transaction it is actually serving.

## 2a. Defects found by review

A Codex review of the finished phase found five more, and four of them are one
mistake made twice plus its consequences.

**`reset()` cannot release a port it does not own.** Both new arbiters cleared
their `busy` flag in `reset()` and made the owner's release conditional on the
generation. That reads as "reset takes the port back", and it is not what
happens: an initiator inside `external->b_transport()` is blocked in a C++ call
stack that `reset()` cannot unwind, and it is still using the port. Clearing the
flag handed the port to somebody else *alongside* it — in the bridge, two
concurrent calls on the socket the arbiter exists to serialise; in the chip
fabric, two cores inside one target.

The comment I had written on the abandon path — "reset already released it" —
is where the error is visible in hindsight. Reset cleared a flag; it released
nothing.

Fixed the same way in both: **whoever takes a port releases it, reset or no
reset**, because ownership is a fact about a call stack rather than model state.
`reset()` now abandons only the *queued* waiters. The chip fabric needed one
extra case, since it waits a cycle between the grant and the forward: a request
abandoned in that window owns the port and will never forward, so it releases
before returning — otherwise the port wedges for the rest of the run.

Both are covered by regressions that fail against the old code: an initiator
holding a port through a one-microsecond downstream call, a reset partway
through, and a second request issued straight into that window. The observable
is `peak_in_flight` at the target, which reads 2 with the old behaviour.

**The bridge ignored the caller's TLM delay.** VP++ passes its quantum-keeper
local time in `delay` (`common/mem.h:105`), so a decoupled hart's request has
not logically arrived yet. A contending request now consumes its delay before
queueing, so the arbiter orders contenders by arrival rather than by which
process SystemC happened to run first. Measured: a hart carrying a 1 µs quantum
is served at 1 µs instead of at 200 ns.

An **uncontended** request still keeps its quantum and takes a free port
immediately — see §5, where the residual is recorded, along with why the
alternative is worse.

**The chip fabric's debug path forwarded a malformed payload.**
`common_payload_error()` checks the command, the streaming width and the
byte-enable shape, and says nothing about the data pointer; a non-empty debug
transaction with a null pointer went straight to a downstream target.
`b_transport` had always refused it. Removing the new check to confirm the
finding does not produce a wrong answer — it produces a **segmentation fault**,
which is what the downstream `memcpy` does with a null source.

**A recovery check that could not fail.** `CHECK(bridge.outbound_requests() >= 0)`
on an unsigned type is a tautology, and it carried the comment "And the bridge
still works". It sent no transaction. Replaced with a request issued after the
reset that has to complete — an arbiter left wedged now fails the test instead
of passing it.

## 3. Evidence

### 3.1 Multi-hart atomicity (decision record D8, closed by D22)

| Run | Counter | Expected | Contention waits |
| --- | --- | --- | --- |
| `amo shared` | 128 | 128 | 62 |
| `amo unshared` | **64** | 128 | 0 |
| `lrsc shared` | 128 | 128 | 40 |
| `lrsc unshared` | **64** | 128 | 0 |

The unshared runs are the negative control and they are registered tests: a
change that makes them produce the right answer fails the suite rather than
silently removing the reason to believe the shared runs.

Three things make the result mean something. The interleaving is forced — the
contended word's target performs its access *after* a `wait()`, so an AMO's load
and store are separated by a real yield. The harts are made to interleave — the
global quantum is set to ten ISS cycles before the cores are built, as
`test_fp_concurrency` does. And contention is measured rather than assumed: a
shared run ending with zero waits would mean the harts never overlapped.

Upstream `52d376d4` stays unbackported. The defect it fixes is a lock lost
between an AMO's load and its store because address translation for the store
can itself perform stores; TPU_V3 runs `satp.MODE = Bare` in machine mode, where
translation touches no memory. D22 records the one condition that reopens it:
any configuration that enables translation.

### 3.2 Where the contention counter belongs

Counting contention inside `lock()` reported **zero** for a two-hart AMO run
that was demonstrably serialised. The reason is that upstream calls
`wait_for_access_rights()` on every load, store and instruction fetch, not only
on atomics: the second hart never reached its own `amoadd.w`, because it was
already parked on its next instruction fetch. The counter moved to
`wait_until_unlocked()`, which is where a hart actually waits.

This is also a fidelity statement worth keeping in view. The lock serialises
*all* traffic from other harts. That is a faithful model of a locked bus and a
pessimistic model of a coherent interconnect, and no throughput figure from a
run with contended atomics should be presented as either.

### 3.3 A D19 prediction, corrected

D19 wrote that a reservation left behind by a reset "becomes a hang the moment
multi-hart atomicity makes it chip-shared". It does not. Upstream arms a
17-instruction counter at `lr.w` and calls `release_lr_sc_reservation()` when it
expires (`rv32/iss_ctemplate.cpp:301`), releasing the bus lock with it, so a
hart cannot strand the lock by taking a reservation and branching away.

`bus_lock_forward_progress` pins the behaviour rather than the prediction: one
hart takes the lock and spins without an `sc.w`; the sibling must be blocked for
a while — 3 contention waits, measured — and must then finish on its own, 8 of 8
increments. If the bound disappears upstream, that test hangs into its timeout.

### 3.4 The chip composition gate

One boot image, run by both harts, branching only on `mhartid`. Each hart writes
its id into its own SRAM and into its **sibling's**, so one pair of reads
afterwards states whether both booted, whether the cross-core path works in both
directions, and whether either went to the wrong core.

Measured on the passing run:

```
boot ROM fetches 175, global RAM accesses 3,
chip-local addresses offered to the mesh 0
core0 -> outside 163, core1 -> outside 15, local bypass 2, inbound 226
external port conflicts 4
```

`chip-local addresses offered to the mesh 0` is the containment result. With
`NoLoopback = 1` the mesh refuses a target on a node that hosts an upstream
port, so a single sighting would be a deadlock in the real system rather than a
slow path (D1).

The chip is built at **chip 1**, so its harts are 2 and 3. Chip 0's harts are 0
and 1, and 0 is what an uninitialised CSR reads as — the same trap the Phase 7
firmware gate had to be moved off.

### 3.5 Arbitration fairness, with negative controls

Both new arbiters were checked by breaking them.

**Chip fabric.** Replacing the rotating search with a fixed one drops the
alternation count from 11 in 12 grants to 1, and the test fails. Round-robin
fairness is only a behaviour in the `arbitrated` mode — under annotation alone,
requests are processed in call order and round-robin is indistinguishable from
first-come-first-served.

**Bridge outbound port.** Same fixed-priority substitution, same result: 11
alternations in 12 grants become 1. Removing the arbiter entirely is the more
interesting control, because the alternation count stays high — 10 in 12 — and
only the mutual-exclusion check notices: two transactions inside the target at
once, zero recorded conflicts, and the grant counters at zero. A fairness check
alone would have passed a bridge with no arbiter at all.

**Reset ownership, both components.** Restoring the reviewed-away behaviour —
`reset()` clearing `busy`, the release made conditional on the generation —
makes `peak_in_flight` read 2 at the target in each, and both tests fail.

**Delay consumption.** Skipping the `wait(delay)` on the contended path serves
the hart at 200 ns instead of 1 µs, and the arrival check fails.

**The debug null check.** Removing it does not produce a wrong result; it
produces a segmentation fault in the downstream target's `memcpy`.

### 3.6 Suite

| | |
| --- | --- |
| Release `ctest -L tpu_v3` | 51/51, 0 skips |
| Debug `ctest -L tpu_v3` | 51/51 |
| `--repeat until-fail:5` on the new and touched tests, both builds | pass |

51 = the 42 of Phase 7 plus five bus-lock cases, the chip-fabric gate, the
bridge outbound-arbitration gate, and the chip composition gate in each of its
two timing modes.

## 4. Gate items

| Plan §16 Phase 8 gate | Where it is met |
| --- | --- |
| Both harts boot and run independent workloads concurrently | `both_harts_booted_through_the_one_external_port`, `both_cores_move_data_concurrently` — and the concurrent run asserts the external port was actually contended, so a serialised run cannot pass it |
| One core cannot corrupt the other core's private SRAM accidentally | `a_remote_write_lands_in_the_named_core_only`; a transfer straddling both apertures is refused rather than assigned |
| Authorized remote/debug accesses map to the intended core | `debug_access_maps_to_the_named_core`, which reads both cores' marks and requires them to differ |
| Ownership, IRQs and responses remain correct under cross-engine contention | `one_cores_interrupt_stays_off_the_other_cores_hart` — checked at the *hart*, not at the signal, because a stray `set_irq` would leave `irq_pending()` false and still wake the other ISS |
| Close the multi-hart AMO gate (upstream `52d376d4`, D8) | §3.1, closed by D22 |

Instantiating exactly two cores with unique hart ids, the chip aperture decode,
the outbound arbitration and the inbound route are the components themselves;
`the_two_harts_have_different_identities` checks the ids against `mhartid` as
firmware reads it rather than against the model's own arithmetic.

## 5. Limitations recorded

**A hart parked in the shared bus lock's wait is not restarted by a reset.** It
resumes there when the lock frees and finishes the instruction it was in the
middle of, after the reset, and can write one register doing so. Same root cause
as the D19 `wfi` limitation: a reset cannot unwind a SystemC process's C++
stack. Resetting the *holder* is what frees the lock, so the waiter does not
hang.

**A reset abandons a hart's in-flight fetch, and the hart takes a trap for
it.** Only reachable when something downstream blocks. A hart waiting in the
bridge's outbound arbiter when `reset()` arrives is woken with
`TLM_GENERIC_ERROR_RESPONSE`, which the ISS turns into an access fault; `mtvec`
is zero after the reset, so it traps to the reset vector and effectively
restarts, having dirtied `mepc`, `mcause` and `mstatus` on the way. The
alternative is worse: leaving it blocked would hang it, because the initiator
holding the port has been reset too and will not release it. Same root cause as
the D19 `wfi` limitation — a reset cannot unwind a suspended process's C++
stack. Visible in the `arbitrated` run as one `[ISS] Warn: Taking trap handler
in machine mode to 0x0`.

**An uncontended outbound request does not consume its TLM delay.** A hart
running ahead of simulated time can therefore claim a free external port before
a DMA that is, in simulated time, earlier. Consuming the delay on every outbound
access would fix it and would also synchronise the hart to global time on every
instruction fetch, removing temporal decoupling entirely — the cost D16 refuses
for the local plane, for the same reason. Contending requests do consume it, so
the ordering *among initiators that actually collide* is right. Arbitration
order between a decoupled hart and a DMA is approximate and no fairness figure
taken from it is a hardware claim.

**`annotated` chip-fabric mode does not serialise a blocking target.** Port
occupancy is a charge on the caller's delay, so two initiators can be inside one
downstream target at once if that target waits. That is inherent to loosely
timed modelling and is why the mode exists: a fabric that waits stalls the one
process that advances the mesh (`INTERFACE_CONTRACT.md` §3). The `arbitrated`
mode is where one-at-a-time is a behaviour, and where the Phase 8 fairness
checks run.

**The chip has no interrupt controller and no chip-level DMA.** Each core
aggregates its own engines and delivers to its own hart; nothing aggregates
across the chip. Phase 8 tests that independence rather than assuming it.

## 6. What Phase 9 inherits

The chip presents exactly two sockets to the outside, which is what plan §4.4
requires and what keeps the system inside the 8-initiator FlooNoC budget (D2).
Everything the NoC endpoint has to do — placement, burst chunking, ownership,
same-chip bypass — now has one place to attach.

The D1 owner-aware local bypass remains a **Phase 9 prerequisite** for
chip-to-chip traffic. What Phase 8 supplies is the half of it that lives below
the mesh: traffic between the two cores of a chip is already answered inside the
chip and is never offered to `noc_interconnect`.
