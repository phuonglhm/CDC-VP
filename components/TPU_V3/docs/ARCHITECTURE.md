# TPU_V3 Architecture

Companion to `TPU_V3_IMPLEMENTATION_PLAN.md`. The plan says *what must be
built and in what order*; this file says *what the thing is*, at the level of
detail a person needs to write or review one of its components.

Nothing here overrides the plan's frozen decisions (§4) or unfreezes anything
in §5. Where this file picks a value the plan left open, it says so and names
the decision it comes from.

`TPU_V3_DECISION_RECORD.md` (D1–D6) is the authority for the items it covers
and supersedes the temporary Phase 0 values P0-6, P0-7 and P0-9.

---

## 1. Hierarchy

```text
tpu_v3_soc                                     platform, not reusable IP
├── noc_interconnect            (existing)     parameterized 2D mesh, FlooNoC
├── global boot ROM             (existing)     memory_tlm
├── global control                             platform registers
├── global RAM / HBM            (existing)     memory_tlm
└── tpu_chip[0..N-1]            N <= 8
    ├── chip_local_fabric                      decode + arbitration
    ├── chip_noc_endpoint                      one aggregated NoC manager
    ├── tpu_core[0]
    └── tpu_core[1]

tpu_core
├── riscv_vp_plusplus                          one RV32GCV hart, scalar + RVV
├── core_local_fabric                          decode + arbitration
├── shared_vector_memory                       one SVM
├── mxu[0]                                     128x128
└── mxu[1]                                     128x128
```

Counts that are frozen and must be validated in constructors: 2 cores per chip,
2 MXUs per core, 128x128 per MXU, one SVM per core, one hart per core.

`N <= 8` is not an architectural preference. The frozen FlooNoC chimney manager
ID is 3 bits, so `noc_interconnect` refuses a ninth upstream initiator, and
plan §4.4 gives each chip exactly one. See `TPU_V3_PHASE0_AUDIT.md` §5.2.

## 2. What is one hart, and what is not

The scalar core and the VPU are **one architectural RISC-V hart** (plan §4.2).
There is no MMIO doorbell between them, no separate `mhartid`, and no
software-visible handoff. A vector instruction is an instruction in the same
instruction stream, and a vector load is an ordinary RISC-V load with a vector
destination.

The MXUs are the opposite: they are **not** part of the hart. They are
memory-mapped accelerators, programmed through a register/descriptor interface,
started asynchronously, and completed via status registers and an interrupt.
Firmware that treats an MXU like a functional unit — writing a start bit and
then reading the result register without checking `done` — is wrong and the
model must make that visible rather than convenient.

### MXU arithmetic

The reference numeric contract (decision record D6):

```text
operand A       BF16
operand B       BF16
accumulation    IEEE FP32, fixed accumulation order
```

The fixed order is not a detail: a reduction that varies with host thread
scheduling or vectorisation makes results irreproducible, and an
irreproducible golden comparison is not a test. The model must also define and
test BF16 round-to-nearest-even conversion, special values, and its subnormal
policy.

An INT8 × INT8 → INT32 quantized path may be added later. It is selected
explicitly, it is additive, and it never replaces the reference path. Until it
exists, asking for it is a configuration error rather than a silent fallback.

The arithmetic is named in the configuration, in the platform report and in the
package manifest, because a numeric result is not interpretable without it.

Hart ID:

```text
hart_id  = chip_linear_id * 2 + core_id
reset_pc = 0x0000_0000            // GLOBAL_BOOT_ROM
```

so chip 3 core 1 is hart 7. Firmware reads it from `mhartid`. This is the only
identity that firmware needs; chip and core index are derived from it, not
supplied separately.

Both are **static properties of a CPU instance**, carried in
`cdc::cpu::cpu_config` (decision record D5). They are not runtime setters with
default no-op implementations: a no-op setter lets a 16-hart platform elaborate
cleanly while every backend still reports `mhartid = 0`, and nothing fails
until firmware tries to tell the cores apart. A backend that cannot honour a
requested `hart_id` or `reset_pc` must refuse to construct.

## 3. Data paths

Three distinct paths, and the difference matters for both correctness and for
what the timing numbers mean:

**Hart path.** Instruction fetch, scalar load/store and vector load/store leave
the hart through TLM initiator sockets into the core-local fabric. Vector
memory access is ordinary RISC-V load/store — `vle*`/`vse*` and friends — so it
lands on the same path as scalar traffic and is subject to the same decode,
arbitration and latency. There is no private path from the VPU to the SVM.

**MXU path.** Each MXU is both a target (its register file) and an initiator
(it fetches operands and writes results). Its initiator traffic enters the
core-local fabric like any other master, so an MXU reading SVM contends with
the hart reading SVM, and that contention is visible in the SVM counters.

**Remote path.** An address outside the core aperture goes to the chip-local
fabric; an address outside the chip aperture goes to the NoC endpoint. This is
a decode consequence, not a routing decision made per transaction: local
traffic never enters the mesh because the fabric never hands it over. Plan §9.2
requires exactly this, and it is also what makes the NoLoopback constraint
survivable.

## 4. Memory model and ordering

* One coherent backing store per SVM. No caches anywhere in the initial
  architecture — not "caches disabled", **absent**. Adding one is a separate
  coherence decision with its own test plan (plan §11.3).
* Program-visible ordering follows blocking TLM completion: when `b_transport`
  returns, the effect is globally visible to everything on that fabric.
* An MXU job is *not* ordered against the hart by TLM completion. The write
  that starts a job returns immediately (plan §9.4); the job's memory effects
  become visible when the job completes. The synchronization boundary is the
  completion status/IRQ, and firmware must respect it. A driver that polls
  `done` and then reads results is correct; one that reads results after the
  start write is not.
* No multicast and no collectives. A weight broadcast to several chips is
  several unicast transfers (plan §9.5). Reporting it as a broadcast would be
  an accuracy claim the model cannot support.

## 5. Interrupts

Per core, level-sensitive, aggregated in the core:

| Source | Semantics |
| --- | --- |
| MXU 0 complete | level, held until acknowledged through the MXU's status register |
| MXU 1 complete | level |
| MXU 0/1 error | level, distinct from completion |
| core-local fabric error | level, latched cause |

The aggregate line reaches the hart as machine external interrupt (cause 11)
through `cpu_base::set_irq`. Level, not edge, is deliberate: an edge-triggered
completion lost during a reset window is unrecoverable and produces a hang that
looks like a modelling bug. A level line with an explicit acknowledgement is
diagnosable.

## 6. Clocks and reset

One clock domain per component with an explicitly stated period; no
cross-domain adapters in the initial architecture. The NoC has its own clock
(it is cycle-stepped) and is already isolated behind `noc_interconnect`.

Reset is synchronous and hierarchical: platform → chip → core → component. Each
component documents what an active-reset does to work in flight. For the MXU
that means: an in-flight job is abandoned, `busy` clears, `done` does **not**
set, an abort is counted, and any admission slot is released. Silently
completing a job that was reset mid-flight would be worse than either
alternative.

## 7. Fidelity levels

The platform is not one model, it is a set of backends selected at
construction, and every reported number must name which was used.

| Level | CPU | MXU | NoC | Use |
| --- | --- | --- | --- | --- |
| full-system fast | RISC-V VP++ functional + approximate cost | fast analytical 128x128 | `timing_mode::fast` | firmware, integration, long runs |
| NoC detailed | RISC-V VP++ functional + approximate cost | fast analytical | `timing_mode::detailed` | contention, routing, back-pressure |
| MXU detailed | RISC-V VP++ functional + approximate cost | Sauria-derived, scope per Phase 9 | either | single-MXU microarchitecture |

Functional results are identical across all three. Only timing differs. A
backend change that alters an architectural result is a defect, and the
cross-backend equivalence tests exist to catch it.

What may **not** be said about any of these (plan §14.4): none is cycle
accurate at the system level, RISC-V VP++ does not model Google TPU pipeline
timing, a 32x32 tiled approximation is not 128x128 timing, and none of it is
TPUv3 RTL equivalence.

## 8. Component responsibilities, one line each

| Component | Owns | Must not |
| --- | --- | --- |
| `architecture_config` | validated, strongly typed configuration | read YAML, touch SystemC |
| `address_map` | every base, size and stride; overflow-checked arithmetic | be duplicated anywhere else |
| `shared_vector_memory` | storage, byte enables, arbitration, per-requester counters | `wait()` inside `b_transport` |
| `mxu` | GEMM semantics, async worker, register contract, counters | block MMIO, share static state between instances |
| `core_local_fabric` | decode, arbitration, local containment, per-route counters | know what a register means |
| `tpu_core` | composition, IDs, IRQ aggregation, reset sequencing | implement component behaviour |
| `tpu_chip` | two cores, chip aperture, outbound arbitration, inbound decode | expose more than one NoC manager |
| `chip_noc_endpoint` | placement, burst chunking, ownership, bypass | make TPU-specific changes inside routing primitives |
| `tpu_v3_soc` (platform) | config parsing, instantiation, placement, firmware, metrics, packaging | contain reusable IP behaviour |

## 9. Known architectural blockers

**Multi-chip NoC traffic is blocked in the current NoC wrapper.**
`noc_interconnect` refuses any target on a node that hosts any upstream port,
and a TPU chip needs both on its node. One chip plus global memory works today;
chip-to-chip does not.

Decision record D1 settles the approach: keep `NoLoopback = 1` — do not delete
the guard and do not disable the router protection — and add an **owner-aware**
local bypass, registering which manager port owns a co-located target. A
transaction from the owner reaches the target directly, injecting no flit and
consuming no outstanding slot; a transaction from any other manager routes
through the mesh and ejects normally; a self-addressed transaction without a
valid bypass mapping is refused during elaboration. Because `NoLoopback` does
not change, the detailed NoC RTL cross-check stays valid. It is a **Phase 7
prerequisite**, not a Phase 8 problem to discover late. Background in
`TPU_V3_PHASE0_AUDIT.md` §5.1.

**`cpu_base` has no hart-ID or reset-PC accessor.** Decision record D5: add
them to `cdc::cpu::cpu_config` as static construction-time properties, *not* as
default no-op virtual setters. The TPU_V3 RISC-V VP++ wrapper must honor both
properties. A legacy backend that cannot honor a non-default request must
reject construction rather than silently use hart 0. See audit §6.

**RISC-V VP++ is a new backend, not the existing Bremen wrapper.** TPU_V3 uses
a thin wrapper around the RV32+RVV ISS portions of
`ics-jku/riscv-vp-plusplus`; it does not instantiate the complete upstream
platform and does not replace `cpu_models/riscv_vp` for existing CDC-VP
platforms. Spike remains an external differential oracle, not the runtime CPU.
The VP++ revision and `VLEN=512`/`ELEN=64` support must pass the Phase 2 gate
before the backend is called verified.

**No vector multilib in the cross toolchain.** Compiling `rv32gcv_zvl512b` is
proven; linking against libc for a vector build is not. Decision record D4:
Phase 2 uses a freestanding `-ffreestanding -nostdlib -nostartfiles`
environment with a project-owned `crt0.S`, linker script and trap entry, so the
RVV smoke test depends on neither libc nor libm. A later firmware phase may use
the scalar `rv32imafdc/ilp32d` multilib for scalar library routines provided
the ABI matches and the choice is recorded in the build manifest. See audit §3.
