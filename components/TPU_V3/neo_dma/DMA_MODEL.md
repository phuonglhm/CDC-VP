# NEO DMA model

The independent DMA of one NEO-CORE. One instance per core, one descriptor
slot, owned by TPU_V3 (decision record D14, plan §11.5).

This document is the programming reference and the record of what the model
does **not** claim. `include/tpu_v3/dma/dma_registers.h` is the executable copy
of the register map; where the two disagree, the header is right and this file
is stale.

---

## 1. What it is, and what it is not

It moves bytes between core SRAM and chip/global/NoC-visible memory through two
ports and nothing else: a native local-SRAM requester and an external TLM
initiator. It holds no pointer into anyone's backing store — `core_sram`
exposes none, and this component never asks for one.

`tpu_v3_neo_dma` links the native-port interface and the address map, and
nothing else — in particular **not** `tpu_v3_core_sram`. The code never touched
the storage, but the build graph used to say it depended on it, purely because
one header lived there; the interface was split into `tpu_v3_native_port` so
the dependency says what the code does. A link edge nobody needs is the one a
later change quietly starts using, so the guard refuses that edge too.

It is **not** `components/dma_tlm`, the shared PL330-style model, under another
name. That model is eight channels of microprogram with `DMAMOV`/`DMALD`/
`DMAST`, a debug-command launch path, an MFIFO and an event-vector interrupt
model, behind one generic TLM master socket. It is useful prior art and the
wrong contract: NEO-CORE needs one descriptor, a split local/external data
path, and a level interrupt. The shared component is left untouched because
`noc_soc`, `VP_FX1_Full_SoC` and the DMA platform depend on it.

It is also not Sauria's DMA, which D14 forbids outright.

The `neo_dma_independence` test is what keeps all of that true: it reads the
sources with comments stripped, the symbols the compiler emitted, and the
CMake link interface. It distinguishes an include from an identifier on
purpose — naming `address_map::core_sram_window` is correct, because the window
is a fact about the map, while including `core_sram.h` would mean the DMA had
reached past the native port to the storage behind it.

## 2. Ports

```cpp
tlm_utils::simple_target_socket<neo_dma>    control;   // 32-bit AXI4-Lite
sc_core::sc_port<sram::neo_local_sram_if>   local;     // requester "dma"
tlm_utils::simple_initiator_socket<neo_dma> external;  // AXI4 / NoC-facing
sc_core::sc_out<bool>                       irq;       // level
void reset();
```

`irq` is driven by exactly one process. The state behind it changes from two —
a register write and the worker — which in RTL is unremarkable because the line
is combinational from the status bits, but a SystemC signal refuses two
writers. Both sides raise an event and one method writes the line. The
notification is immediate rather than delta-delayed, so the line settles within
one delta of the write that changed it; otherwise firmware acknowledging an
interrupt would still read it asserted on the next cycle.

`reset()` must be called from a SystemC process.

## 3. Register map

Absolute decode over the whole 64 KiB `DMA_CONTROL` window. Unlisted offsets
are reserved: they read zero with `TLM_OK_RESPONSE`, drop writes, and never
alias an implemented register — `DMA_CONTROL + 0x1000` answers zero, not the
identity register.

| Offset | Register | Access |
| ---: | --- | --- |
| `0x000` | `ID` | RO, `0x54503304` |
| `0x004` | `VERSION` | RO |
| `0x008` | `CONTROL` | W1S: bit 0 `START`, bit 1 `ABORT`. Reads zero |
| `0x00C` | `STATUS` | bit 0 `BUSY` RO; bits 1–3 `DONE`/`ERROR`/`ABORTED` W1C |
| `0x010`–`0x01C` | `SRC_ADDR_{LO,HI}`, `DST_ADDR_{LO,HI}` | RW while idle |
| `0x020` | `LENGTH` | RW while idle, non-zero byte count |
| `0x024` | `IRQ_ENABLE` | RW, bit 0 |
| `0x028` | `ERROR_CAUSE` | RO, latched first error |
| `0x02C`/`0x030` | `BYTES_DONE_{LO,HI}` | RO, destination bytes committed |
| `0x034` | `TRANSFER_COUNT` | RO, accepted jobs |
| `0x038` | `ERROR_COUNT` | RO |
| `0x03C` | `ABORT_COUNT` | RO |
| `0x040` | `OVERRUN_COUNT` | RO, `START` rejected while busy |
| `0x044`/`0x048` | `LOCAL_BYTES_{LO,HI}` | RO, native local-SRAM bytes |
| `0x04C`/`0x050` | `EXTERNAL_BYTES_{LO,HI}` | RO, external-path bytes |

`ERROR_CAUSE` values are stable for Revision 1: `0` none, `1` invalid length,
`2` address overflow, `3` unsupported endpoint combination, `4` source
unmapped/straddling, `5` destination unmapped/straddling, `6` local read,
`7` local write, `8` external read, `9` external write, `10` internal model
failure. A new condition takes a new number rather than renumbering these.

**Reset and abort are not transfer errors.** They do not set `ERROR` and do not
appear in `ERROR_CAUSE`.

### Access rules

Exactly one naturally aligned 4-byte access with full strobes. A 1, 2, 8 or
64-byte payload, a misaligned address, a partial strobe pattern and a wrapped
streaming transfer are each refused with `TLM_BURST_ERROR_RESPONSE`; a command
that is neither read nor write gets `TLM_COMMAND_ERROR_RESPONSE`; an address
outside the window gets `TLM_ADDRESS_ERROR_RESPONSE`.

A null data pointer is **answered** with `TLM_GENERIC_ERROR_RESPONSE` rather
than thrown as a model defect, which is a deliberate difference from the Phase
3 register files. This target is reachable from a remote master through the
external bridge, and taking the simulation down on a malformed remote payload
would turn a diagnosable protocol error into a crash with no response to trace
it by.

`transport_dbg` uses the same absolute decode and bounds, updates no counter,
and answers every register identically to `b_transport` — including `STATUS`,
which is the register a driver checks first. It is **side-effect free**: a
debug write returns 0 and changes nothing. A host-side poke that could launch a
transfer would make the model's behaviour depend on how a test happened to set
it up.

## 4. Running a job

1. write the descriptor registers while idle;
2. write `START`. `BUSY` is set **before that access returns** and no data has
   moved;
3. the worker validates, then copies, consuming simulated time;
4. on completion or first error it clears `BUSY`, sets `DONE` or `ERROR`,
   publishes the byte counts and raises the level IRQ if enabled;
5. write 1 to the status bit; the IRQ deasserts.

Validation is the worker's first action, not the `START` write's. An invalid
descriptor therefore reports through exactly the same status and IRQ path as a
downstream failure — a driver that had to handle "rejected synchronously" and
"failed asynchronously" as two different shapes would get one of them wrong.

Refused with `TLM_GENERIC_ERROR_RESPONSE` and no state change:

* a descriptor write while busy — the active job holds its own snapshot, so
  applying it would change nothing about the running transfer while making the
  registers describe a job that is not running;
* a second `START` while busy, which additionally increments `OVERRUN_COUNT`
  and never overwrites the active snapshot;
* `START` and `ABORT` in the same access, which have no defined ordering.

## 5. Routes

Two directions, inferred from absolute address classification rather than from
a direction bit. A direction bit is a second source of truth that can disagree
with the addresses.

1. core-local SRAM → external;
2. external → core-local SRAM.

A span wholly inside the SRAM window is local, one wholly outside it is
external, and one overlapping the boundary is **straddling** — not a route.
Both endpoints local, both external, a straddling span, a zero length or a span
ending above the 4 GiB RV32 limit are rejected with the matching cause.
Local-to-local, external-to-external and scatter/gather are a later explicit
revision, not behaviour to infer.

## 6. Chunking

The staging buffer is at most one legal external frame.

* **Local** requests are split into 1..64-byte native accesses — 64 bytes being
  one RVV register at VLEN=512, the largest a single native access may be.
* **External** frames obey `ceil((address % 8 + length) / 8) <= 256`, which
  rearranges to `address % 8 + length <= 2048`. So 2048 bytes is reachable only
  at bus alignment and the limit shrinks by one byte per byte of lane offset.
  An implementation that assumed 2048 always fits would be refused by the
  interconnect at any offset; the test target counts violations rather than
  trusting the DMA to be right.
* Chunks are issued in ascending address order and never cross a region.

## 7. Accounting

`BYTES_DONE` counts **destination bytes committed**. Not source bytes fetched,
not payload bytes named. Source data still sitting in the staging buffer is
explicitly not completion, and a partial transfer that reported its full length
would be indistinguishable from a successful one.

A successful boundary response and its byte count are recorded *before* the
worker consumes the returned annotated delay, so a reset landing inside that
wait cannot hide a memory effect that already happened. Publishing at the end
of a chunk instead leaves exactly that window open: the target has committed
and the register still says it has not, and a reset arriving there freezes a
count **lower** than what memory holds. Under-reporting a commit is the same
class of error as inventing one, and it is the direction a test that only
checks "the destination holds at least `BYTES_DONE` bytes" will never catch.

On success, `LOCAL_BYTES` and `EXTERNAL_BYTES` both equal `LENGTH`. On error or
abort they need not: each equals only its own successful boundary transactions,
while `BYTES_DONE` equals only committed destination bytes.

The first failing local or external chunk stops the job. It latches the
origin-specific cause, leaves earlier destination chunks committed, sets
`ERROR`, clears `BUSY` and raises the IRQ if enabled. **No later chunk is
issued.**

Chunk, request and byte counters stay separately attributable, and no chunk
count is relabelled as a request or a hardware bus transaction count (decision
record D7).

## 8. Reset and abort

Both advance a job generation, so a worker that resumes into a new epoch
abandons its job rather than publishing into it. The worker re-checks the
generation after arbitration, after each transaction and after consuming an
annotated delay. A request blocked in the native fabric observes the fabric's
own `aborted` response and unwinds.

| | `reset()` | `ABORT` |
| --- | --- | --- |
| `BUSY` | cleared | cleared |
| `DONE`/`ERROR` | cleared | untouched |
| `ABORTED` | cleared | **set**, sticky |
| IRQ | deasserted | not raised |
| `ABORT_COUNT` | +1 if a job was active | +1 if a job was active |
| `BYTES_DONE` | keeps the active job's committed count; zeroed if idle | keeps it |
| path traffic counters | cleared | untouched |
| event counters | untouched | untouched |

An `ABORT` with no job to abort is **accepted and ignored**: the write returns
`TLM_OK_RESPONSE`, nothing changes, `ABORTED` is not set and `ABORT_COUNT` does
not move. Firmware asking twice, or aborting a job that finished a moment
earlier, is not an error — and counting an abort that aborted nothing would
make the counter mean something other than its name.

Neither attempts rollback: the destination really does hold the bytes that were
committed, and pretending otherwise would be a lie about memory. Nothing that
was abandoned is ever reported as completion — that is what the separate
`ABORTED` bit is for.

**A start request is a flag, not just an event.** Both paths clear `BUSY`
immediately, so firmware may legitimately start a new job while the old worker
is still unwinding out of a bank arbitration, an external transaction or a
chunk delay. At that moment nobody is waiting on the worker's start event, so
a bare notification is delivered to no one and is gone; the worker then blocks
and the new job holds `BUSY` for ever. The flag survives the gap, so the worker
finds the request whenever it gets back to the top of its loop.

**A transaction still in flight is attributed by two different rules**, and
they deliberately disagree. It completes after the reset — the DMA cannot
un-write it — so its bytes are in memory, and:

* `BYTES_DONE` **does** count them, provided no new `START` has claimed the
  register. Those bytes belong to the job that asked for them, the job's
  snapshot is what the register holds, and a reset does not hand that snapshot
  to anyone else. Refusing them would under-report a commit. Once a new job
  claims the register the old worker stops publishing, because one job's bytes
  in another job's count is what plan §11.5 forbids.
* the **path traffic counters do not**. `reset()` cleared them and opened a new
  measurement window; this request belongs to the one that just closed. Adding
  to them would let an old request re-populate a counter that was just zeroed —
  `LOCAL_BYTES` reading 64 immediately after a reset set it to 0.
  `neo_local_sram_fabric` excludes old-generation responses from its own
  counters for exactly this reason, so a DMA that did not would also stop
  reconciling with it.

The two rules answer different questions. "How much of this job's destination
was committed" is a property of a job; "how much traffic did this path carry in
this window" is a property of a measurement window. A reset ends a window
without ending a job's claim on its own register.

**Which counters a reset clears is forced by what they are compared against.**

The *path traffic* counters — `LOCAL_BYTES`, `EXTERNAL_BYTES` and the request
counts — are reconciled with `neo_local_sram_fabric`'s own counters, and that
fabric clears its counters on reset. In Phase 7 the core's hierarchical reset
drives both, so a DMA that kept lifetime traffic totals would permanently
disagree with a fabric that did not: conservation would break at the first
reset and stay broken. They are therefore cleared here too, and conservation is
a **per-epoch** property — a report that quotes it must say which epoch.

The *event* counters — `TRANSFER_COUNT`, `ERROR_COUNT`, `ABORT_COUNT`,
`OVERRUN_COUNT` — have no counterpart on the fabric and record this engine's
own history. `ABORT_COUNT` is incremented *by* the reset call, so the block
cannot also be zeroed by it.

**`BYTES_DONE` is published while this job still owns the register, not while
the epoch is unchanged.** The distinction is not academic. A native access can
write beats into SRAM, wait for arbitration, have a reset land, and only then
return with those bytes reported; refusing to count them because the epoch
moved would lose a commit that happened *before* the reset — exactly what plan
§11.5 says must be reported. What must not happen is one job's bytes landing in
another job's count, and that is a different condition: it holds only once a
new `START` has claimed the register. A reset alone does not claim it.

## 9. What this model does not claim

* `chunk_latency` and the annotated external delays are a cost so that a
  transfer takes simulated time and a reset has a window to land in. They are
  not a claim about DMA throughput, and no bandwidth figure may be quoted from
  them.
* The external side is modelled at transaction level. AW/W/B/AR/R channel
  timing is not modelled and is not claimed (decision record D15).
* An external address inside this core's own aperture — core MMIO, say — is
  classified external here and refused by `neo_external_bridge`, which is the
  component that owns local containment. The DMA does not duplicate that rule.
