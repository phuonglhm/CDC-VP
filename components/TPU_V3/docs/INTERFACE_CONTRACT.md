# TPU_V3 TLM Interface Contract

Binding on every component under `components/TPU_V3` and on the TPU_V3
platform. It expands plan §13 into rules a reviewer can check mechanically.

A component that violates one of these is broken even if its own tests pass,
because the failure mode is almost always somewhere else: a deadlock in the
detailed NoC, a response attributed to the wrong requester, or a partial write
that looks like data corruption.

---

## 1. Generic payload

On entry to any TPU_V3 `b_transport` / `transport_dbg`:

| Field | Rule on entry | Rule on return |
| --- | --- | --- |
| `command` | read or write only | unchanged |
| `address` | byte address, absolute in the platform map | unchanged |
| `data_ptr` | non-null when `data_length > 0` | unchanged |
| `data_length` | > 0 | unchanged |
| `streaming_width` | must be 0 or ≥ `data_length` | unchanged |
| `byte_enable_ptr` | null, or a pattern of `byte_enable_length` | unchanged |
| `response_status` | ignored | **always set** |
| `dmi_allowed` | ignored | set false |

`TLM_COMMAND_ERROR_RESPONSE` for a command that is neither read nor write.
`TLM_BURST_ERROR_RESPONSE` for a wrapped streaming transfer, an unsupported
width, or a misaligned MMIO access. `TLM_ADDRESS_ERROR_RESPONSE` for an address
that does not decode. `TLM_GENERIC_ERROR_RESPONSE` for a target that decoded
the access and refused it.

A target never returns `TLM_INCOMPLETE_RESPONSE`. If a caller sees one, a path
returned without setting status, which is a defect in the target and not a
condition for the caller to handle.

**Errors are never converted to zero data.** A failed read leaves the caller's
buffer untouched and sets an error status.

## 2. Byte enables

Memory-like targets honour arbitrary byte-enable patterns, including
non-contiguous ones, on both read and write. `byte_enable_length` may be
shorter than `data_length`, in which case the pattern repeats — the standard
TLM rule.

**The repeating pattern stops at the adapter.** The native local-data plane
carries `wstrb` as one byte per data byte, because that is what it is in RTL;
there is no repeat rule there. Any component translating a TLM payload into a
native request must therefore *expand* the pattern to `data_length` bytes.
Forwarding `byte_enable_ptr` unchanged reads past the end of the initiator's
array for every payload longer than the pattern, and applies whatever it finds
— a wrong result and an overread at once. A non-null pointer with
`byte_enable_length == 0` describes no bytes and cannot be repeated into
anything; it is refused with `TLM_BURST_ERROR_RESPONSE`.

A fully masked write is legal, transfers nothing, allocates no page-backed
storage and must not be counted as if it had moved a payload.

The bridge applies the command, streaming-width and byte-enable-shape rules at
all four boundaries: inbound/outbound `b_transport` and inbound/outbound
`transport_dbg`. Debug transport bypasses timing and workload counters, not
payload validity. A malformed debug request returns zero bytes and is not
forwarded to either the native SRAM plane or the external target.

MMIO targets require all-ones over the four bytes of the register. A
partial-strobe register write is refused with `TLM_BURST_ERROR_RESPONSE`, not
applied as a read-modify-write. Read-modify-write on a register with
write-1-to-clear or clear-on-read bits does the wrong thing silently, so the
model refuses instead of guessing.

## 3. Timing

**Short latency is annotated, never waited.**

```cpp
// correct
void b_transport(tlm::tlm_generic_payload& t, sc_core::sc_time& delay) {
    do_access(t);
    delay += access_latency_;          // annotate
}

// wrong: stalls the detailed NoC's mesh driver
void b_transport(tlm::tlm_generic_payload& t, sc_core::sc_time& delay) {
    do_access(t);
    wait(access_latency_);             // never do this in a TPU_V3 target
}
```

The second form freezes every node in the mesh, not just this target, because
one SystemC process advances the network clock. This is the single most
important rule in this document.

**Who it binds, exactly.** Every TLM *target* — core SRAM, every register file,
every future engine — and every component on a path the detailed NoC can reach.
`neo_control_fabric` never waits in any configuration. `neo_local_sram_fabric`
has two timing modes (decision record D16): `annotated`, the default, never
waits and is the only mode permitted behind a NoC-reachable target;
`arbitrated` blocks its own requesters on a real per-bank round-robin arbiter,
which is what a ready/valid interface does and the only way fairness and
back-pressure become behaviours rather than estimates. `neo_external_bridge`
inherits whichever mode its fabric was built with and reports it, because a
bridge on the inbound path must not be the one that stalls the mesh.

* `delay` on entry may be non-zero; a target adds to it and does not clear it.
* `transport_dbg` never advances time and never annotates delay.
* Long work runs in the component's own `SC_THREAD`, which may `wait()` freely.
* `b_transport` must never wait for accelerator completion. An MXU, DMA or
  Transform start write enqueues and returns in the same delta cycle.
* Timing mode is chosen at construction, never changed during simulation, and
  is reported in metrics.

## 4. Asynchronous accelerator contract

Every long-running operation in TPU_V3 follows the same shape:

1. firmware programs descriptor registers;
2. firmware writes the start bit; the write returns immediately, `busy` sets in
   the same access;
3. a worker thread performs the work and consumes simulated time;
4. on completion the worker sets `done`, clears `busy`, updates counters and
   asserts the level IRQ;
5. firmware acknowledges by writing 1 to the `done` bit (W1C), which deasserts
   the IRQ.

Constraints:

* a start write while `busy` is set is refused — `busy` stays, the descriptor
  is not overwritten, and an `overrun` counter increments. Queueing a second
  job behind the first would need an ordering contract that does not exist yet.
* reset during an active job abandons it: `busy` clears, `done` does **not**
  set, `abort_count` increments, the IRQ deasserts, and any admission slot is
  released. Cleanup happens on success, error, reset and exception alike.
* `error` is a separate status bit from `done` with a latched cause code. An
  errored job asserts the IRQ exactly like a completed one, because firmware
  must be woken either way.
* an abandoned job is **not** a completed one. Reset and explicit abort clear
  `busy` without setting `done`, and an explicit abort is reported through its
  own sticky status bit so a driver can tell "I stopped this" from "this
  finished".

`neo_dma` is the first component built to this shape; `neo_dma/DMA_MODEL.md`
records how each clause landed, including the two that only became concrete
once something implemented them: validation runs in the worker rather than in
the start write, so an invalid descriptor reports through the same status and
IRQ path as a downstream failure; and a completion byte count means bytes
committed at the destination, never bytes fetched into a staging buffer.

### 4.1 NEO-CORE block boundaries

Decision D15, finally ratified on 2026-08-12, separates three interfaces that
must not be collapsed into one generic internal AXI fabric:

* **AXI4-Lite control:** each active engine has a 32-bit MMIO target. It is
  in-order, has no bursts or IDs, accepts at most one transaction per control
  initiator, and follows this document's 4-byte MMIO rules. VP++ and the
  authorized external inbound adapter are its initiators. The SystemC model
  represents the transactions, not signal-level AW/W/B/AR/R channel timing.
* **Native local data:** VP++ local accesses, the DMA local port, MXU and
  Transform plus authorized inbound chip/NoC traffic issue requests to
  `neo_local_sram_fabric`. This is a pipelined request/response interface into
  physically banked core SRAM, not AXI and not a full data crossbar.
* **External AXI4/NoC:** the DMA external port and VP++ instruction/global-data
  path reach chip/global/remote memory through the external adapter and chip
  NoC endpoint. The CPU path is required to fetch from global boot ROM. Inbound
  remote MMIO/SRAM traffic traverses the reverse adapters into the appropriate
  local plane. Full AXI semantics stop at this boundary; MXU and Transform
  do not become external AXI4 masters in Revision 1.

The native local request contains requester identity, absolute byte address,
read/write command, transfer size, write data and byte strobes. Its response
contains read data and an explicit status. Revision 1 is strictly in order and
allows at most one outstanding request per requester. Each SRAM bank has an
independent deterministic round-robin arbiter: same-bank conflicts
back-pressure the loser, while different banks may progress concurrently.

The RTL-facing shape is a conventional ready/valid pair: request
`valid/ready`, `address`, `write`, `size`, `wdata`, `wstrb`; response
`valid/ready`, `rdata`, `error`. Requester identity may be implicit in a
dedicated port and is explicit after arbitration. Signal names may follow the
RTL coding standard, but dropping ready/back-pressure or error propagation is
not a legal simplification.

Completion is reported only after all physical beats of one logical request
finish. Ordering is guaranteed per requester and per bank; a multi-beat access
is not an atomic primitive against another requester. Firmware must use the
accelerator completion/status synchronization rules rather than depend on an
undocumented wide-access atomicity.

The number of banks, physical data width, low-order bank mapping and pipeline
depth are validated construction-time configuration. Tests and reports state
their actual values; this contract does not freeze 256 bits or any other
illustrative datapath width before SRAM-macro, clock and PD inputs exist. The
C++ schema therefore carries **no default** for the three physical values and
refuses zero: a number nobody chose is exactly what would become an
architectural constant by accident. The shipped configurations state them and
every report that uses them prints the word "provisional".

An access is refused with an explicit status and transfers nothing. The plane
distinguishes a decode miss from a capacity overrun, because a driver reacts
differently to a wrong pointer and to a configuration too small for its
workload, and D6 forbids aliasing the second into valid storage.

**DMA is independent.** `neo_dma` is implemented and owned under TPU_V3. It
must not include, instantiate or call Sauria's DMA, and must not obtain direct
pointers to core SRAM or global-memory backing. Every transferred byte crosses
its native local initiator and external bridge as applicable and observes
normal decode, arbitration, byte-enable, response and metrics rules.

**The MXU boundary is matrix multiplication.** From the Sauria v4.2 source,
the implementation adapter may retain only the PE array and the minimum
feeder/sequencer/result-collection logic
needed to implement the accepted GEMM descriptor. OBP, RCE, NPU profile
routing, NPU instruction decoding, Sauria DMA and other unrelated NPU-top
behavior are outside the NEO MXU contract. Operand and result traffic uses the
native SRAM port; the MXU has no external AXI4 master port.

**The Transform boundary is capability- and source-controlled.** D18 approves
only the pinned v4.2 Im2Col subset: signed INT8 CHW input, row-major
`[OH*OW][C*KH*KW]` output, stride/dilation, and no padding (all four descriptor
padding fields must be zero). Its
tensor traffic uses the native SRAM port and it has no external AXI4 master.
Col2Im is independent, not implied by the existence of Im2Col or by PSM output
addressing. Its capability bit is zero; selecting it and starting must report
`unavailable_operation` without SRAM traffic. A configuration string alone may
not enable it, and no missing operation may return fake success.

MXU geometry and datatype are runtime-reportable, construction-time properties.
The accepted bring-up pair is the verified v4.2 64x64 configuration. A 128x128
selection is legal only after the NPU-team source, adapter, golden regression
and resource/scalability checks pass. No configuration or manifest may call a
64x64 run 128x128.

## 5. Concurrency and ownership

* **No direct pointer into backing storage.** `core_sram` exposes no `data()`,
  no `raw()` and no `get_direct_mem_ptr()`, and that omission is load-bearing:
  it is what makes "no accelerator bypasses arbitration" a structural property
  rather than a convention. The check that keeps it true as the tree grows is a
  conservation one — the bytes the SRAM recorded must reconcile with the bytes
  the fabric carried, which `test_neo_external_bridge` asserts.
* **No mutable global or static state.** MXU, DMA and Transform instances,
  and 16 cores in one simulation, share nothing. A `static` scratch buffer in a
  compute kernel is a defect even when tests pass single-threaded, because
  SystemC processes interleave at `wait()` boundaries.
* Every in-flight request carries an identifiable owner (requester id / port
  index). Counters and latency are attributed to that owner, never to "the most
  recent" anything. `noc_interconnect::last_latency_cycles()` without a port
  argument is a global convenience accessor and must not be used for
  attribution.
* Completion queues preserve the ordering the upstream protocol requires. The
  frozen NoC has `MaxUniqueIds = 1`, so responses are FIFO within reads and
  FIFO within writes on one port; nothing downstream may reorder them.
* Tests exercise simultaneous MXU / DMA / Transform / external-inbound
  activity, simultaneous core 0 / core 1, and concurrent NoC traffic. A
  concurrency test without a watchdog is not a test —
  a deadlock must fail, not hang the suite.

## 6. Access widths and vector granularity

| Target class | Widths | Alignment | Byte enables |
| --- | --- | --- | --- |
| memory (`CORE_SRAM`, RAM, ROM) | any size from 1 to 64 bytes | any | arbitrary |
| MMIO register file | 4 only | 4-byte natural | all-ones only |

A memory target must accept any payload from 1 to 64 bytes. 64 is the size of
one RVV register at VLEN=512, so it is the largest a single access can need.

**There is no vector-instruction boundary at this interface, and no component
here may invent one.** This is decision record **D7**; an earlier version of
this section got it wrong, so the reasoning is worth stating rather than just
the rule.

The old wording said core SRAM must accept a 64-byte vector transfer as one
transaction and that splitting it "inside the model" would fabricate
arbitration events. That assumed the ISS hands over a whole vector access. It
does not. RISC-V VP++ decomposes vector loads and stores inside
`vp/src/core/common/v.h`, one call per active element, before the CDC-VP
wrapper is reached; `data_memory_if` carries no vector-access boundary to
recover. The assumption was also wrong in principle: masked, strided, indexed
and fault-only-first accesses cannot be one contiguous transaction under any
backend, because they touch a subset, a non-contiguous set, a computed set, or
a set truncated by a fault.

The rules that follow from it:

* the wrapper and every target **must not** infer, group or reassemble vector
  instruction boundaries — guessing where one ended is not a measurement;
* with the current backend one request usually carries one active element, but
  that is a property of **that backend**, not an invariant anything may rely on;
* 1..64-byte payload support is proven with a **synthetic initiator**. VP++ is
  not required to emit a 64-byte payload, and its absence from a VP++ trace is
  not a defect;
* counters are named for what they measure — `tlm_request_count`,
  `physical_beat_count`, `bank_conflict_count`, `transferred_bytes`,
  `error_count`, `arbitration_event_count` — and never
  `vector_instruction_count`, `vector_register_count`, or a hardware bus
  transaction count;
* the request count includes **refused** requests, so `request_count >=
  error_count` always holds and the failure rate is a ratio of two numbers
  counting the same thing. A counter that silently dropped failures would hide
  exactly the case worth looking at;
* `transferred_bytes` is bytes **moved**, not bytes named. A masked write
  moves fewer bytes than its payload length, and counting the payload would
  credit the model with bandwidth it never carried;
* if `neo_local_sram_fabric` splits one TLM payload across physical beats or
  banks, it records one TLM request and the actual number of physical beats;
  neither value may be silently presented as the other;
* any timing or NoC figure produced this way records **`VP++ element-wise
  granularity`**, and arbitration-event counts must never be offered as
  evidence of equivalence with TPU hardware.

## 7. NoC-facing rules

The endpoint is the only TPU_V3 component that talks to `noc_interconnect`, and
it absorbs every constraint the interconnect imposes:

* **Burst limit.** The bus is 8 bytes wide and one AXI burst is at most 256
  beats, so the largest accepted frame is 2048 bytes at bus alignment and fewer
  at an offset — `ceil((address % 8 + length) / 8) <= 256`. The interconnect
  **refuses** a longer payload rather than splitting it. The endpoint therefore
  splits, and its chunking contract states: chunks are bus-aligned except
  possibly the first; chunks are issued in ascending address order; the first
  failing chunk stops the transfer and its status is returned; bytes already
  transferred stay transferred and the completion reports how many; every chunk
  is attributed to the originating requester in metrics.
* **Local bypass.** An address inside this chip's own aperture is never handed
  to the interconnect. This is required for correctness, not only for
  efficiency — see the NoLoopback blocker in `TPU_V3_PHASE0_AUDIT.md` §5.1.
  The test is **overlap**, not containment: a transfer that begins inside the
  aperture and runs past it is not *contained* by it, and one byte of local
  traffic in the mesh is still local traffic in the mesh. A transfer that
  overlaps a region without lying inside it decodes to two targets at once and
  is refused with `TLM_ADDRESS_ERROR_RESPONSE`, because there is no correct
  answer for which should have answered. All of it is computed with the
  overflow-safe comparison of plan §12 rule 5.
* **Outstanding bound.** At most `max_outstanding_per_port` (≤ 32) concurrent
  calls per upstream port. Above the bound the endpoint waits for a slot; it
  does not drop, reorder or re-tag.
* **Target kind.** Regions are registered with the `target_kind` from
  `ADDRESS_MAP.md` §7. Declaring MMIO as `memory` to make a widened read
  succeed is forbidden.

## 8. Debug transport

`transport_dbg` is for host-side loading and test setup:

* it never advances simulated time and never annotates delay;
* it bypasses arbitration and latency but **not** decode or bounds checks —
  a debug write outside a region fails like any other;
* it does not update performance counters, because a loader is not workload
  traffic. Counting it would corrupt every metric that follows;
* it applies the same command rule as `b_transport`: a command that is neither
  read nor write returns 0. Treating "not a write" as "a read" turns
  `TLM_IGNORE_COMMAND` into a read that overwrites the caller's buffer;
* it answers every register identically to `b_transport`. A debug read that
  disagreed would make a loader and the firmware it loaded see different
  hardware;
* it may write `GLOBAL_BOOT_ROM`, which refuses ordinary writes.

## 9. DMI

Disabled. Every target sets `dmi_allowed = false` and returns false from
`get_direct_mem_ptr`. Enabling DMI is a phase of its own: it needs an
invalidation contract for every path that can change memory behind a granted
pointer, and it would silently bypass exactly the counters and latency the
platform exists to produce.

## 10. Configuration validation

Configuration objects are validated in the constructor and throw
`std::invalid_argument` with an actionable message naming the field, the
offending value and the accepted range. Validation happens before any socket is
bound, so a bad configuration fails during elaboration rather than on the first
transaction.

Frozen values that must be rejected rather than accepted-and-warned:
`cores != 2`, an MXU count other than one per core, a DMA count other than one
per core, a Transform count other than one per core, MXU geometry other
than the explicitly supported 64x64 bring-up or promoted 128x128 target,
`xlen != 32`, `vlen != 512`, `elen != 64`, or an RVV version other than
`"1.0"`. A configuration requesting 128x128 must fail unless the selected NPU
source and adapter have passed the 128x128 promotion gate.

## 11. Reviewer checklist

- [ ] no `wait()` on any path reachable from `b_transport`
- [ ] `response_status` set on every return path, including early errors
- [ ] no `static` or global mutable state
- [ ] every counter update attributed to a named requester
- [ ] reset path releases slots and clears in-flight state
- [ ] errors surfaced, never turned into zero data
- [ ] address arithmetic in 64-bit with an overflow check
- [ ] no address constant outside `address_map.h`
- [ ] concurrency test has a watchdog
- [ ] a negative control exists for each new protocol rule
