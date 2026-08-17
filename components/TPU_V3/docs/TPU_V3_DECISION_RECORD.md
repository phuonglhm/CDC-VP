# TPU_V3 Architecture and Integration Decision Record

Status: **Final approved for implementation; NEO-CORE interconnect frozen by D15**

Initial decision date: 2026-08-08

Architecture rebaseline date: 2026-08-11

D15 final ratification date: 2026-08-12

Scope: TPU_V3 Phase 2 onward

This document records the decisions made after the Phase 0 and Phase 1 review
and their later amendments. It is an implementation authority for the items
listed below. Where it conflicts with a temporary value in
`TPU_V3_PHASE0_AUDIT.md` or the decision
log in `TPU_V3_IMPLEMENTATION_PLAN.md`, this document takes precedence until
those documents are synchronized.

The decisions do not claim that this virtual platform is RTL-equivalent or
cycle-equivalent to a Google TPU v3. They define the architecture and fidelity
boundaries of the CDC-VP TPU_V3 model.

## Decision summary

| ID | Topic | Approved decision |
| --- | --- | --- |
| D1 | FlooNoC `NoLoopback` | Keep `NoLoopback=1`; add an explicit owner-aware local bypass and permit co-located manager/subordinate endpoints only through that contract |
| D2 | Current system size | Revision 1 supports at most 8 chips / 16 TPU cores; a 32-core system requires a separate 16-chip NoC/address-map extension |
| D3 | RV32GCV runtime and RVV reference | Use RISC-V VP++ as the TPU core's primary RV32GCV runtime; retain Spike commit `16c0b60119f65a648643cf5d41e4e38e871f0bad` only as an independent golden/differential reference |
| D4 | RISC-V toolchain | Pin xPack `riscv-none-elf` GCC 15.2.0-1; use a freestanding `-nostdlib` RV32GCV smoke environment in Phase 2 |
| D5 | CPU identity/reset configuration | Put `hart_id` and `reset_pc` in `cpu_config`; do not use default no-op virtual setters |
| D6 | Memory capacity and matrix arithmetic | Reference core SRAM capacity is 16 MiB/core; target SA arithmetic is BF16 x BF16 with FP32 accumulation. The original Sauria-experimental scope is superseded by D14 |
| D7 | Vector memory access granularity | Accept VP++ element-wise traffic; no upstream fork and no inferred coalescing. Core SRAM supports 1..64-byte payloads; counters describe TLM requests, never vector instructions or hardware bus transactions |
| D8 | VP++ cycle-baseline defect (F11) | Controlled backport of exactly upstream `b710fa7b` onto the base pin. No baseline subtraction, no SystemC 3.0.1 migration. Recorded patch, script-applied, CMake-verified, manifest-tracked. A **Phase 2 closure gate** |
| D9 | Post-bump scalar-FP fixes | **Not** backported. All five stay in the upstream inventory; only the two observable at rv32gcv (`63524fbb`, `14e7fff5`, 7 fields) are expected diffs. The corpus fails on an unexpected diff *and* on one of those seven disappearing. The other three are ordinary regression checks — amended 2026-08-10 |
| D10 | `vstart` restart scope | VP++ treats vector instructions as atomic with respect to interrupts, so interrupt-driven restart is unobservable. Test `vstart` through a mid-vector trap instead, and say so in plan §11.2 |
| D11 | Differential method and oracle isolation | One image compiled once, run on both models, comparing a firmware-written canonical signature block located by linker symbols. Spike runs as a child process and is never linked into anything |
| D12 | RV32 index EEW=64 (F12) | **Downstream conformance patch**, not an accepted deviation. All 32 RV32 indexed encodings with index EEW=64 — four unit forms and 28 segment forms — raise an illegal instruction at the decode site, before `stats.inc_loadstore()` and `prepInstr()` |
| D13 | Trap cause for a failed bus access (F13) | **Downstream conformance patch.** Page faults only from MMU translation; a bus, decode or target failure is an access fault chosen by access origin — 1 fetch, 5 load, 7 store/AMO. Protocol errors are model defects, not guest faults |
| D14 | NEO-CORE architecture rebaseline | One VP++ RV32GCV hart, one shared core SRAM, one independent TPU_V3 DMA, one Sauria matrix engine and one Im2Col/Col2Im Transform engine. Integrate verified 64x64 Sauria first; promote to the NPU team's 128x128 source later. Never reuse the Sauria DMA. Its original single AXI-like-fabric wording is superseded by D15 |
| D15 | NEO-CORE internal interconnect | Split control and data: 32-bit AXI4-Lite for MMIO control; a native, pipelined, banked-SRAM request/response fabric for internal bulk data; full AXI4 only at the external chip/NoC boundary. NEO DMA owns bulk external movement; the required VP++ instruction/global path also exits through that boundary. Do not build a full AXI data crossbar inside NEO-CORE |
| D16 | Where the local-data plane may block | `neo_local_sram_fabric` has two timing modes. `annotated` is the default and never waits, so it is safe behind any NoC-reachable target; `arbitrated` blocks on a real per-bank round-robin arbiter and is the only mode in which fairness and back-pressure are behaviours rather than estimates. Every TLM target still never waits. A timing figure must name the mode that produced it |
| D17 | Sauria matrix adapter shape | **Buffered tile staging.** Operands are prefetched from core SRAM into private staging stores over `neo_local_sram_if`, the array runs against those stores at the source's own timing, results are written back the same way. Pass-through is not implemented: the source has feeder compute stalls but no SRAM-side response handshake, so a late read is captured as valid data. SRAM-B holds one physical 64-lane vector per K step and zero-pads partial N. Prefetch and writeback stay fully in D16's scope. Only `source_compute_time` may be called cycle-correlated. Requires hash-verified instrumentation-only patches and a binary gate proving the selected closure has no mutable function-local static state |

## D1. FlooNoC `NoLoopback` and local bypass

### Decision

Keep the frozen FlooNoC router configuration with `NoLoopback=1`. Do not solve
the problem by globally disabling the router protection, and do not merely
delete `reject_self_node_targets()`.

Add an explicit mapping between a target region and the manager port that owns
that target locally. A possible API shape is:

```cpp
add_target(base, size, node, kind, local_owner_port);
```

The exact API name may change, but the ownership information must be explicit
and mechanically validated.

### Required behavior

1. A transaction from `local_owner_port` to its owned local target bypasses
   the mesh and is delivered directly to the target socket.
2. That local transaction injects no request or response flit and consumes no
   NoC outstanding slot.
3. A transaction from another manager to the same target is injected into the
   mesh, routed to the target node, and ejected normally.
4. A manager and subordinate may share a node only when the co-location is
   registered through this owner-aware contract.
5. A self-addressed transaction without a valid bypass mapping is rejected
   before simulation traffic starts.

### Required verification

- Local access reaches the correct target with zero injected flits.
- Remote access to that target traverses and ejects from the mesh.
- Local and remote concurrent accesses preserve data and response ownership.
- A missing or incorrect owner mapping fails during elaboration.
- The detailed NoC RTL cross-check remains valid because `NoLoopback` is not
  changed.

### Schedule

Implement and verify this before NoC/mesh Phase 9. It is a Phase 9 prerequisite,
not a problem to discover during multi-chip traffic.

## D2. Chip/core limit for Revision 1

### Decision

Revision 1 is limited to:

```text
8 TPU chips
2 TPU cores per chip
16 TPU cores / RV32GCV harts total
2 Sauria matrix engines per chip
16 Sauria matrix engines total
```

The accelerator counts above are the D14 amendment to this size decision. The
earlier four-MXU-per-chip count is superseded; the two-core-per-chip and NoC
manager-limit reasoning are unchanged.

This is an implementation limit caused by the current 3-bit FlooNoC manager
ID and the one-aggregated-manager-per-chip architecture. It is not a general
statement about TPU v3 scalability.

The shipped configurations mean:

- `mesh_4x4.yaml`: 8 chips on 16 mesh nodes, not 16 chips;
- `mesh_2x2.yaml`: 3 chips while one dedicated node hosts the global targets;
- `single_chip.yaml`: 1 chip containing exactly 2 TPU cores.

### 32-core consequence

A requirement for 32 TPU cores means 16 chips and is outside Revision 1. It
requires all of the following as one coordinated architecture change:

1. widen the NoC manager ID from 3 bits to at least 4 bits;
2. rerun the FlooNoC protocol and RTL cross-checks;
3. resolve co-located target/manager behavior through D1;
4. redesign the current address map, which reserves only eight 128 MiB chip
   apertures in the remaining RV32 address space;
5. update firmware headers, placement validation and scalability tests.

Therefore `max_chips = 8` must be described as the **Revision 1 backend
limit**. A future 32-core milestone must not silently increase the constant
without completing the changes above.

## D3. RISC-V VP++ runtime and Spike golden reference

### Decision

Use RISC-V VP++ as the primary CPU/ISS backend instantiated in each TPU core:

```text
Repository: https://github.com/ics-jku/riscv-vp-plusplus
Revision:   OPEN -- select an immutable candidate SHA in the Phase 2 audit
License:    MIT
Role:       runtime RV32GCV hart (scalar execution plus RVV 1.0)
```

RISC-V VP++ integrates RVV 1.0 architectural state and instruction semantics
into its RV32 and RV64 ISSs. The TPU model must therefore use one VP++ RV32
ISS for scalar and vector execution; it must not run a scalar ISS and a second
vector ISS as two independently advancing processors.

Do not instantiate the complete upstream VP++ platform inside every TPU core.
CDC-VP owns the memory map, core SRAM, CLINT/PLIC integration, devices and NoC. Add a
thin `cdc::cpu::riscv_vp_plusplus` wrapper around the required ISS/library
pieces and route fetch, scalar data, vector data and MMIO through CDC-VP TLM
sockets. Keep the existing Bremen `cpu_models/riscv_vp` backend unchanged for
the platforms that already use it.

Retain the following Spike revision as an independent golden reference:

```text
Repository: https://github.com/riscv-software-src/riscv-isa-sim
Revision:   16c0b60119f65a648643cf5d41e4e38e871f0bad
License:    BSD-3-Clause
Role:       standalone differential oracle; not the TPU runtime backend
```

Fetch both projects through the existing `third_party/` mechanism at recorded
full SHAs; do not vendor source copies under `components/TPU_V3` or
`cpu_models`. Spike must not be linked into the portable runtime package unless
a future design explicitly makes it a runtime dependency.

### Promotion gate

The selected RISC-V VP++ candidate SHA becomes the verified runtime pin only
after Phase 2 proves:

- a reproducible 64-bit host build with GCC 11.5 and SystemC 2.3.4;
- an embeddable/static ISS integration that does not require Qt, VNC, an
  external VP++ executable or a complete upstream platform instance;
- RVV 1.0 execution;
- `VLEN=512`;
- `ELEN=64`;
- `vlenb=64`;
- successful execution of the RV32GCV architectural smoke ELF;
- expected traps for illegal/unsupported vector configurations;
- fetch, scalar load/store, vector load/store and MMIO traffic visible on the
  CDC-VP TLM path, with no private backend RAM bypass;
- correct `hart_id`, `reset_pc`, interrupt and multi-instance behavior;
- differential agreement with the pinned Spike oracle for the Phase 2 smoke
  and negative-control corpus.

The VP++ and Spike full SHAs, licenses and distinct runtime/reference roles
must be included in the package manifest. Pinning a commit makes the source
reproducible; it does not by itself prove functional correctness. The reported
81.44% upstream basic RVV coverage is useful evidence, not a substitute for
the TPU_V3 gate above.

## D4. RISC-V cross-toolchain and multilib policy

### Decision

Pin the installed xPack toolchain:

```text
Toolchain: xPack GNU RISC-V Embedded GCC
Version:   15.2.0-1
Prefix:    /opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1/bin/
ISA:       rv32gcv_zvl512b
ABI:       ilp32d
```

Phase 2 must use a freestanding smoke environment:

```text
-march=rv32gcv_zvl512b
-mabi=ilp32d
-ffreestanding
-nostdlib
-nostartfiles
```

Provide a project-owned `crt0.S`, linker script, stack initialization, trap
entry and minimal pass/fail reporting. The Phase 2 RVV smoke test must not
depend on libc or libm.

### Later firmware policy

The absence of a vector-specific multilib does not mean scalar newlib code is
incompatible with an RV32GCV application. A later firmware phase may
explicitly use the scalar `rv32imafdc/ilp32d` multilib for scalar library
routines, provided that:

- the ABI matches `ilp32d`;
- the choice is documented in the build manifest;
- the RVV application objects retain the required RISC-V ELF attributes;
- no vector implementation is falsely attributed to the scalar library.

## D5. Hart ID and reset PC configuration

### Decision

`hart_id` and `reset_pc` are static properties of a CPU instance. Add them to
the existing backend-agnostic `cdc::cpu::cpu_config`:

```cpp
struct cpu_config {
    unsigned xlen = 32;
    unsigned num_irq = 0;
    bool has_mmu = false;
    bool has_smp = false;
    std::uint32_t hart_id = 0;
    std::uint64_t reset_pc = 0;
};
```

Do not add default virtual setters that silently do nothing. A no-op setter can
make a multi-hart platform elaborate successfully while every backend still
reports `mhartid=0`.

### Required backend behavior

- The RISC-V VP++ wrapper consumes `hart_id` and `reset_pc` during
  construction/configuration.
- The existing Bremen `riscv_vp` wrapper is not the TPU_V3 runtime. It may keep
  its current behavior for existing single-hart platforms, but it must reject
  unsupported non-default identity/reset requests if constructed through the
  extended `cpu_config`.
- The base interface exposes read-only identity/configuration accessors, or
  the platform reads the immutable `cpu_config`; runtime mutation is not
  required.
- Unsupported non-default configuration must be rejected explicitly, never
  ignored.

The TPU_V3 mapping remains:

```text
mhartid = chip_linear_id * 2 + core_id
reset_pc = 0x00000000  // GLOBAL_BOOT_ROM
```

Tests must cover unique and firmware-visible hart IDs across all Revision 1
cores, reset at the configured PC, and rejection by a backend that cannot
honor a requested configuration.

## D6. Core SRAM capacity and matrix arithmetic

D14 later changes the component names and Sauria role: `SVM` becomes the
NEO-CORE shared SRAM, one Sauria SA replaces the old two-MXU composition, and
Sauria moves onto the implementation path. The 16 MiB capacity and BF16/FP32
numeric destination below remain in force unless a later numeric decision
changes them.

### Core SRAM (formerly SVM)

The TPU_V3 reference configuration instantiates **16 MiB of core SRAM per TPU
core**, equal to the architectural core SRAM window.

Smaller SRAM capacities may remain available only for explicitly labeled
bring-up or stress configurations. They are not the TPU_V3 reference result.
For any smaller capacity, the complete 16 MiB window still decodes to the core SRAM
target; the SRAM component rejects accesses above its instantiated capacity and
must never alias them into valid storage.

This decision supersedes the temporary 4 MiB default recorded as Phase 0
decision P0-6.

### Global RAM

Keep 256 MiB as the configurable bring-up default, with the existing 1 GiB
window. This is simulated backing/global memory and must not be described as
an exact model of TPU v3 HBM capacity or bandwidth.

### Host-memory backing policy for Phase 3

Architectural capacity and host allocation are separate quantities. Phase 3
must provide sparse, page-backed storage for global RAM and core SRAM; global RAM
must never be implemented as an eager 1 GiB host allocation. The reference
implementation uses deterministic 4 KiB pages and must satisfy all of the
following:

- an unallocated page reads as zero and consumes no page backing;
- the first write or writable debug access allocates only the touched pages;
- byte enables and transactions crossing page boundaries retain normal TLM
  semantics;
- reset releases allocated pages and restores the all-zero state;
- accesses outside instantiated capacity are rejected even though the full
  architectural window still decodes to the target;
- counters report logical capacity, current and peak allocated backing bytes,
  and allocated page count.

The backing index should be deterministic (for example, a vector of nullable
page pointers indexed by page number), rather than an unordered container
whose iteration behavior varies across hosts. A dense backing may exist only
as an explicit test or microbenchmark option; it is not the reference default.
The `mesh_4x4` configuration has 1.25 GiB of logical storage (16 x 16 MiB SRAM
plus 1 GiB global RAM) and must elaborate without committing that amount of
host memory.

### Matrix-engine arithmetic

The primary TPU_V3 target arithmetic contract is:

```text
operand A:      BF16
operand B:      BF16
multiplication: BF16 operands with explicitly defined conversion semantics
accumulation:   IEEE FP32
```

The model must define and test BF16 round-to-nearest-even conversion, special
values, and the chosen subnormal policy. The accumulation order must be fixed
so results are deterministic across hosts.

An optional quantized extension may later add:

```text
INT8 x INT8 -> INT32 accumulation
```

It must be selected explicitly and must not replace the BF16/FP32 TPU_V3
reference path. The earlier Phase 0 proposal of `INT8 + FP32` as the primary
Phase 4 pair is superseded by this decision.

The arithmetic choice follows the public Cloud TPU description: TPU matrix
multiplication uses BF16 operands and FP32 accumulation.

References:

- <https://docs.cloud.google.com/tpu/docs/system-architecture-tpu-vm>
- <https://docs.cloud.google.com/tpu/docs/bfloat16>
- <https://docs.jax.dev/en/latest/pallas/tpu/hardware.html>

### Sauria scope, amended by D14

The available v4.2 source proves Sauria configurations through 64x64 and is an
NPU top, not the approved NEO-CORE composition. D14 makes Sauria the source of
the matrix-multiply and Transform behavior, but does not permit importing the
whole NPU top.

- integrate and label a verified 64x64 matrix engine first;
- accept the 128x128 implementation only from the NPU team's updated source
  after a separate promotion gate;
- keep the interface geometry-independent so the promotion does not change
  firmware-visible control semantics;
- do not claim BF16 equivalence from an FP16 bring-up run;
- keep all geometry/datatype/source revisions in reports and manifests;
- use no Sauria DMA code in NEO-CORE; DMA is a standalone TPU_V3 block;
- obtain Im2Col and Col2Im from an approved NPU-team revision. The existing
  v4.2 IFMAP feeder's Im2Col-related address generation does not establish a
  standalone Transform block, and a Col2Im implementation has not yet been
  located.

## D7. Vector memory access granularity

### Decision

Accept the element-wise memory traffic RISC-V VP++ generates. Do not fork
upstream, and do not reassemble vector accesses by inference.

This is recorded as a decision rather than an audit finding because it changes a
written contract across Phase 2 and Phase 3, not just something observed about
the backend.

### Root cause

`INTERFACE_CONTRACT.md` §6 assumed an RVV instruction boundary still exists at
the TLM interface. It does not. VP++ decomposes a vector access inside
`vp/src/core/common/v.h` — a per-element loop over `evl`, each active element a
separate `iss.mem->load_*` / `store_*` call — before the CDC-VP wrapper is ever
invoked. `data_memory_if` exposes no vector-access boundary, so there is nothing
for the wrapper to coalesce on.

The assumption was also wrong on its own terms: **masked, strided, indexed and
fault-only-first loads cannot be expressed as a single 64-byte transaction at
all.** A masked access touches a subset of elements, a strided access touches
non-contiguous addresses, an indexed access touches addresses computed per
element, and a fault-only-first access may stop partway. A "one vector register,
one wide transaction" rule only ever described the unit-stride unmasked case.

### Phase 3 semantics

1. Core SRAM must support TLM payloads of **1 to 64 bytes**.
2. VP++ currently issues **one transaction of 1, 2, 4 or 8 bytes per active
   element**.
3. The wrapper and core SRAM **must not** infer, group or reassemble vector
   instruction boundaries.
4. Counters are named for what they measure:

   | Counter | Meaning |
   | --- | --- |
   | `tlm_request_count` | workload requests arriving at the target |
   | `transferred_bytes` | bytes actually transferred successfully |
   | `error_count` | requests that failed |
   | `arbitration_event_count` | the model's arbitration events, per TLM request |

5. These counters must **not** be called `vector_instruction_count`,
   `vector_register_count`, or a hardware bus transaction count. They are none
   of those things.
6. With the current VP++ backend one vector request usually corresponds to one
   active element. That is a **property of this backend**, not an invariant of
   core SRAM, and nothing downstream may assume it.
7. The 1..64-byte payload support is verified with a **synthetic initiator**.
   VP++ is not required to generate a 64-byte payload, and the absence of one in
   a VP++ trace is not a defect.
8. Timing and NoC metrics produced with this backend must record
   **`VP++ element-wise granularity`**. The arbitration-event counts must never
   be used to claim equivalence with TPU hardware.

### Synchronization required

`TPU_V3_PHASE2_AUDIT.md` F2, the implemented wrapper,
`INTERFACE_CONTRACT.md` §6 and the rebaselined Phase 3 gate follow this
direction. D14 changes the memory component name, not D7 semantics.

## D8. Controlled backport for the VP++ cycle-baseline defect

Decision date: 2026-08-10.

### Decision

Backport exactly one upstream commit onto the pinned base:

```text
base      7a36fe859cae242f513ca6ad16ab8238f1e82977   (tag 2025.09)
backport  b710fa7be2643b42cee92f5bbcb8cead4c0ed282
          "vp: core: dbbcache: fixed random cycle counting bug (e.g. mcycles)"
effective base + backport
```

Rejected: baseline subtraction, and migrating CDC-VP to SystemC 3.0.1.

The commit changes two lines of `dbbcache.h` (`curEntryIdx = 0` → `-1`), depends
on no SystemC API, and applies cleanly to the base. Baseline subtraction was
rejected on evidence: the bogus offset is not a stable value — with two harts it
was 46 080 ns on one and 0 s on the other, against 2 292 084 375 ns measured
with a single hart — so there is nothing stable to subtract.

### How the patch is managed

The point is that a patched dependency stays as auditable as an unpatched one.

1. base pin unchanged at `7a36fe85...`;
2. the exact upstream patch lives in
   `cpu_models/riscv_vp_plusplus/patches/`, with its SHA256 recorded;
3. `fetch_riscv_vp_plusplus.sh` verifies the patch hash, verifies the base SHA,
   then applies it — it is the only thing permitted to modify the checkout;
4. **CMake verifies, never patches.** It checks the patch hash, the base SHA and
   applied-ness (by reverse-apply, so a hand-edited lookalike does not pass). A
   configure step that edited third-party source would make the binary depend on
   when CMake last ran;
5. `BUILD_MANIFEST.json` records the base revision and, for every approved
   patch, its kind, reference, filename and SHA256; `effective_source` names the
   complete reconstructible series.

At D8, P2-3 was amended from "no patching" to allow the recorded F11 upstream
backport. D12 and D13 later generalised that rule to **"no unrecorded patching;
only the approved, classified and hash-verified patch series"**.

### Not bundled with it

| Commit | Why not, and when |
| --- | --- |
| `7a936cce` "fixed fast quantum" | a larger change to quantum accounting; needs its own audit **before Phase 9** |
| `52d376d4` AMO atomicity / lost bus lock | needs a **multi-hart AMO contention test before Phase 8**; a single-threaded differential run against Spike cannot demonstrate atomicity between harts |

### Closure gate

F11 is a **Phase 2 closure gate**, not deferred to Phase 7. Phase 2 may not be
declared complete while `mcycle` is wrong, however many functional tests pass.
The regression must:

- reject the old offset rather than tolerate it;
- use a normal watchdog — 1 ms is ample for the 28 µs workload;
- check the first TLM request carries no startup offset;
- check `mcycle` starts sane and increases monotonically;
- run Debug, Release, and at least two harts;
- fail if the defect returns.

Verified by reverting the patch: the configure step refuses, and the runtime
gate fails.

### If upstream backports it officially

Move the pin to that commit, delete the local patch, and re-run the whole
Phase 2 gate. Asking upstream for a maintenance backport on a SystemC
2.3-compatible branch may proceed in parallel.

## D9. Post-bump scalar-FP fixes stay as expected diffs

Decision date: 2026-08-10.

`c7140542` (`fsh`/`fsw` raw value), `91777991` (`fmv_x_h` raw value),
`63524fbb` (`fmax`/`fmin` NaN), `14e7fff5` (F/D/Zfh load/store dirty bit and
disabled-float trap) and `b92c01d8` (float load/store harmonisation) are **not**
backported. D8's mechanism stays scoped to exactly one commit.

Instead the differential corpus names each of them. The gate fails when:

* a diff appears that is **not** on the expected list — a real defect; or
* an expected diff **disappears** — the pin changed underneath us, and the
  corpus, the audit and the manifest all need re-checking.

The second condition is the point. A plain allowlist would rot silently; this
one notices when the ground moves. It also means the corpus, not a comment,
is what records which upstream defects we are carrying.

## D10. `vstart` is tested through a trap, not an interrupt

Decision date: 2026-08-10.

Plan §11.2 requires checking "interrupt restart state, including `vstart`
behavior". Measured: `vp/src/core/common/v.h` contains no interrupt check
anywhere in its per-element loop, so **VP++ executes a vector instruction
atomically with respect to interrupts** and never leaves a partially executed
one to resume. The requirement is not satisfiable as written, by any test.

`vstart` itself is live — `v.h:777` sets it per element — so the restart state
is exercised by a **mid-vector trap**: a vector load whose element *k* touches
an unmapped address. That is what Phase 2 tests.

Plan §11.2 is amended to say so. Interrupt-driven restart is recorded as
unobservable in this backend rather than quietly dropped, so a future backend
change can revisit it. A real CLINT in later platform integration does not change this: atomicity
is a property of the ISS loop, not of the interrupt source.

## Phase 2 decision history

## D11. Differential method and oracle isolation

Decision date: 2026-08-10.

**One image, compiled once, run on both models.** `fw/TPU_V3_SoC/rvv_smoke/rvv_sig.elf`
is built by the same cross toolchain as every other Phase 2 image and handed
unmodified to RISC-V VP++ and to Spike. Two images built from one source would
leave the toolchain free to differ between them, and a differential result is
only as trustworthy as the claim that both models saw the same bytes.

**Compare what the program can see.** The firmware itself computes 71 named
words into a `.signature` section — architectural CSRs, trap outcomes, vector
and floating-point results, and checksums over the result buffers — and both
harnesses read exactly that range, located by the `begin_signature` /
`end_signature` linker symbols that fesvr already understands.

The alternative, reading each simulator's private register file, compares more
state but does it through two different debug interfaces, where a difference in
the interfaces is indistinguishable from a difference in the models. The
program's own view is the one interface both are obliged to implement
identically. The limit is recorded rather than glossed: two models that
disagree internally and hide it from the program will match here.

**Spike runs as a child process, never linked.** Beyond D3's rule that Spike
stays out of the platform and the package, a linked oracle would share this
process's allocator, build flags and — decisively — one copy of Berkeley
SoftFloat's process-global rounding mode and exception flags. That is the exact
hazard audit finding F5 exists to guard against, and it would make the oracle
capable of corrupting the model it is judging. A separate address space makes
the independence structural instead of a claim.

**Verdicts.** Equal is a pass. A difference listed in the corpus table is XFAIL
when decision record D9 covers it, and OPEN when it is a reproduced deviation
with no decision yet. Anything else is a failure — including a listed
difference that has *disappeared*, which means one of the two pins moved and
the audit is now describing something that no longer exists.

**The table is derived from source, not from output.** Every entry was written
by reading the pinned VP++ implementation before the first run. A table filled
in from observed behaviour cannot fail, because it agrees with the model by
construction. All seven predicted differences appeared; the two that were not
predicted are F12 and F13, and they are the findings worth having.

### Open items this produced

Neither is covered by D9, and neither has been accepted by anyone:

* **F12** — `vluxei64.v` on RV32. RVV 1.0 §18.2: the V extension does not
  support EEW=64 for index values when XLEN=32; §7.3 requires an
  illegal-instruction exception. Spike raises it, VP++ executes the
  instruction, and VP++'s vector unit has no XLEN-dependent check at all.
* **F13** — an unmapped physical access reports `mcause` 13 (load *page*
  fault) instead of 5 (load *access* fault). The core has no MMU and `satp` is
  Bare, so a page fault cannot legitimately occur; `mem.h` maps every TLM error
  to a page fault without consulting the response status.

Phase 2 sign-off is a decision about these two, not about the gate: the gate is
green because every difference is accounted for, and "accounted for" is not
"agreed".

## D12. RV32 index EEW=64 — downstream conformance patch

Decision date: 2026-08-10. Supersedes the "open" disposition of audit finding
F12.

### Decision

Fix it in the model. Not an expected diff, and not merely a toolchain
restriction.

RVV 1.0 §18.2 states the `V` extension does not support EEW=64 for index values
when XLEN=32, and §7.3 requires an illegal-instruction exception when an offset
EEW is unsupported. This is a defect in RVV itself — the component the TPU VPU
is built on — and Phase 4 hand-written kernels and intrinsics can emit these
encodings even though a compiler will not. A restriction expressed only in the
toolchain would hold exactly until someone wrote the instruction on purpose.

### Mechanism

D8's management applies unchanged — recorded patch, content hash, script-applied,
CMake-verified, manifest-tracked — but this is **not** an upstream backport and
must not be called one. There is no upstream commit. It is a *downstream
conformance patch*, written here against the specification, and the distinction
is operational: a backport disappears when the pin moves past it, while this has
to be re-checked against any new base and ideally offered upstream. The series
records the kind of each patch in the fetch script, the CMake verification and
the build manifest.

### What is patched

`vp/src/core/rv32/iss_ctemplate.cpp`, **32** `OP_CASE` blocks: the four unit
forms `V[SL][OU]XEI64_V` and the 28 segment forms `V[SL][OU]XSEG[2-8]EI64_V`.
That file is the RV32 build, so the restriction is expressed by refusing the
encodings there rather than by an XLEN test at run time; the RV64 translation
unit is untouched and keeps all 32.

The segment forms matter as much as the unit ones. The restriction is on the
*index* EEW, which all 32 carry, and Spike applies it to all of them through a
single `VI_CHECK_ST_INDEX`. A first version of this patch covered only the four
unit forms; the differential corpus did not notice, because it probes
`vluxei64.v`, so 28 encodings stayed wrong behind a green gate. Compiler-emitted
RV32 code will not produce any of them, but Phase 4 intrinsics and hand-written
kernels can.

`RAISE_ILLEGAL_INSTRUCTION()` is the **first** statement, before
`stats.inc_loadstore()` and before `prepInstr()`. Placing it inside
`vLoadStore()`, where the index-EEW arithmetic already lives, would give the
right `mcause` and still be wrong: `prepInstr()` sets `mstatus.VS` to Dirty
before it runs. An illegal instruction must not count a load, must not produce
bus traffic, and must not modify architectural state.

### Gate

`conformance_patches`, plus the differential corpus:

* all 32 encodings trap, each with `mcause == 2`, asserted as a 32-bit mask —
  a patch covering only the four unit forms is rejected (verified);
* no TLM request is issued — only the host can see this, and the harness counts
  accesses to the operand buffer across a window the firmware delimits;
* `vstart` is unchanged, checked against a non-zero value written beforehand so
  "unchanged" is a positive statement;
* the destination vector register is unchanged, likewise against a pattern;
* `mstatus.VS` is still Clean after each of the 32 probe runs starts with
  VS=Clean — the check that pins every guard before `prepInstr()`;
* Spike and VP++ match exactly on both F12 signature fields.

Negative controls verified, both of them: reverting the patch makes configure
refuse by name and makes `conformance_patches`, `rvv_vector_trap` and
`spike_differential` all fail; and restoring only the four unit forms fails the
gate at check 1.

## D13. Trap cause for a failed bus access — downstream conformance patch

Decision date: 2026-08-10. Supersedes the "open" disposition of audit finding
F13.

### Decision

A failed **bus** access is an **access** fault. Page faults belong to address
translation and are raised in the MMU. Not an accepted deviation.

### The rule, and what it is not

The cause is decided by *where the failure happened* and *what the access was
for* — never by which objects exist in the model:

| Failure | Cause |
| --- | --- |
| translation, PTE or permission, inside the MMU | page fault, 12 / 13 / 15 |
| bus, decode or target refusal, after translation | access fault, by origin |

| Access origin | `mcause` |
| --- | --- |
| instruction fetch | 1, instruction access fault |
| load, including vector load | 5, load access fault |
| store, AMO, vector store | 7, store/AMO access fault |

Deciding by `mmu != nullptr` was rejected. The wrapper does pass `&mmu` — an
earlier draft of audit F13 wrongly said `nullptr` — so that test would have
given the wrong answer in exactly the configuration that exposed the bug, and
would have become wrong again the moment an MMU was enabled.

Deriving the cause from the TLM command was also rejected: an instruction fetch
is a TLM read, and an AMO that fails on its read half must still report 7,
because the privileged specification has no "AMO load access fault". The access
type is therefore threaded from the caller into `_do_transaction`.

### TLM response policy

| Response | Treated as |
| --- | --- |
| `TLM_ADDRESS_ERROR_RESPONSE`, `TLM_GENERIC_ERROR_RESPONSE` | a target legitimately refusing the access → guest access fault |
| `TLM_INCOMPLETE_RESPONSE`, command / burst / byte-enable errors | a model or integration defect → `std::runtime_error`, never a guest fault |

Turning a protocol error into a guest fault would hand firmware a plausible trap
for a bug it cannot have caused, and bury the real failure.

### Recorded residual

A bus error during a page-table walk is reported as a load access fault:
`mmu_memory_if` does not carry the originating access type. Strictly it should
report the origin's. Widening that interface is outside this decision's scope
and the path is unreachable at `satp.MODE = Bare`; the limitation is commented
at the call site in `mem.h` rather than left to be rediscovered.

### Gate

`conformance_patches` exercises all six origins — instruction fetch, scalar
load, scalar store, vector load, vector store, AMO — and checks `mcause`,
`mtval`, and `vstart` for the two vector cases. It also asserts that no cause is
12, 13 or 15, so a regression is reported as "a page fault on a core running
with satp.MODE = Bare" rather than as a bare number mismatch. The host
separately confirms that the six refused accesses are the six the firmware
aimed at, so a stray access cannot masquerade as a working probe.

Negative control verified the same way as D12.

## D14. NEO-CORE architecture rebaseline

### Decision

The approved reusable core is `NEO-CORE`. Each instance contains exactly:

```text
NEO-CORE
├── one RISC-V VP++ RV32GCV hart (Scalar + RVV 1.0, VLEN=512)
├── one shared core SRAM
├── one independent TPU_V3 DMA
├── one Sauria matrix engine
├── one ImageTransform engine (Im2Col + Col2Im)
└── one internal transaction fabric (its protocol split is superseded by D15)
```

Two NEO-CORE instances remain in one TPU chip, and one aggregated chip endpoint
remains attached to the parameterized 2D mesh NoC. D14 supersedes the old
per-core composition of one SVM and two independent MXUs. It does not reopen
the approved CPU/RVV, two-cores-per-chip, NoC endpoint or Revision 1 chip-limit
decisions.

The original D14 wording called this one internal AXI-like transaction fabric.
D15 supersedes that interconnect choice with separate control and local-data
planes; D14 remains authoritative for the component composition.

### CPU and RVV interpretation

The Scalar and Vector boxes are logical portions of one RISC-V VP++ hart. They
share PC, privilege state, CSRs, scalar/vector registers and traps. Revision 1
does not instantiate a second vector ISS or expose RVV as an MMIO accelerator.
Scalar and vector memory operations therefore use the VP++ wrapper's existing
TLM memory path. D15 determines whether an address reaches the local SRAM data
plane, the AXI4-Lite control plane or the external bridge.

### DMA ownership

DMA is a new TPU_V3 component and is completely independent of Sauria. It must:

- have its own MMIO target and TLM initiator socket;
- move data only through TLM transactions and normal address decode;
- propagate target errors and preserve partial-transfer accounting;
- never include, instantiate or call `control/sauria_dma.h`;
- never copy through a direct pointer to SRAM/global-memory backing.

**Implemented in Phase 4** as `components/TPU_V3/neo_dma`, with
`neo_dma/DMA_MODEL.md` as its programming reference. The independence
requirement was widened during that phase, not weakened: the shared
PL330-style `components/dma_tlm` is forbidden on the same terms as Sauria's
DMA, because it is the component someone would actually reach for. The
distinction the guard draws is between an *include* and an *identifier* —
naming `address_map::core_sram_window` is right, since the window is a fact
about the map, while including `core_sram.h` would mean the DMA had reached
past its native port to the storage behind it. A guard that banned the
identifier would forbid correct code along with wrong code, and the usual next
step is that someone weakens or deletes the guard.

### Sauria matrix-engine staging

The matrix engine keeps only matrix multiplication and the minimum verified
feed/sequence/result-collection logic required by that contract. NPU-top
profile routing, instruction decoder, OBP/RCE, Sauria DMA and other unrelated
features are excluded.

Implementation proceeds in two explicitly labelled stages:

1. **64x64 bring-up:** integrate the verified v4.2 configuration and cross-check
   it against its source golden tests;
2. **128x128 promotion:** consume the NPU team's updated source, rerun the same
   contract/golden tests, validate hard-coded masks/address widths, and measure
   elaboration/runtime before making 128x128 a selectable reference geometry.

The architectural destination is 128x128, but no build, manifest or report may
identify the 64x64 bring-up engine as 128x128. D6's BF16 x BF16 with FP32
accumulation remains the numeric destination. A v4.2 run using another supported
datatype is useful integration evidence only and must report the actual type.

### Transform source boundary

Im2Col and Col2Im must come from an approved NPU-team source revision. The
current v4.2 tree contains Im2Col-related address generation embedded in the
IFMAP feeder; no standalone Col2Im block has been established by the audit.
Therefore:

- ask the NPU team for the Transform module, its configuration contract,
  golden tests and supported layouts;
- do not infer Col2Im from PSM write ordering or implement a guessed inverse;
- while the source is pending, the component may expose an unavailable
  capability and reject start explicitly, but it may not report fake success;
- the missing Transform delivery does not block core SRAM, internal fabrics,
  independent DMA or 64x64 SA work.

### Rebaseline consequences

The following old requirements are superseded wherever they occur outside
historical Phase 0–2 audit evidence:

- two MXUs per core;
- `MXU0_CONTROL` and `MXU1_CONTROL` as current register names;
- Sauria only as a late optional Phase 9 backend;
- DMA ownership being open or reusing the Sauria DMA;
- treating Im2Col/Col2Im as an optional software-only step.

Phase 3 starts by migrating architecture configuration/address names and by
implementing core SRAM plus the D15 control/data interconnect split. The
detailed phase order and gates are authoritative in
`TPU_V3_IMPLEMENTATION_PLAN.md`.

## D15. NEO-CORE control/data interconnect split

### Decision

**Final ratification (2026-08-12):** the project owner approved this split as
the implementation architecture for both the SystemC/TLM model and the future
RTL handoff. It is no longer a proposal. Replacing the protocol split, making
SA/ImageTransform external AXI masters, or inserting a full AXI data crossbar
inside NEO-CORE requires a new recorded architecture decision.

NEO-CORE uses three deliberately different interconnect contracts:

1. **Control plane — 32-bit AXI4-Lite.** VP++ and the authorized external
   inbound adapter reach core, DMA, SA and ImageTransform register files
   through one narrow, in-order AXI4-Lite decoder. It has no bursts or AXI IDs
   and accepts at most one transaction per control initiator at a time. In the
   SystemC model this is represented at transaction level; signal-level
   AW/W/B/AR/R timing is not claimed.
2. **Local data plane — NEO Local SRAM Fabric.** VP++ local load/store, the
   DMA local port, the SA feeder/result path, the ImageTransform data port and
   authorized inbound chip/NoC traffic access the shared core SRAM through a
   native request/response fabric. The logical SRAM remains one 16 MiB
   addressable resource but is physically banked. This plane is not AXI and
   must not be implemented or described as a full AXI data crossbar.
3. **External plane — bidirectional AXI4/NoC boundary.** Bulk transfers between
   core SRAM and chip/global/remote memory are owned by the independent NEO
   DMA. Its external port and the VP++ instruction/global-memory path reach the
   existing chip endpoint through an AXI4/TLM adapter or the equivalent NoC
   bridge. Inbound remote transactions are decoded back to the AXI4-Lite
   control plane or native local-SRAM plane. The CPU path is required because
   `reset_pc` points at global boot ROM; it does not make SA or ImageTransform
   an external master. Full AXI semantics belong at this boundary, not between
   the internal
   accelerators and local SRAM.

This is an RTL-implementable architectural split, not merely a model naming
choice. AXI4-Lite remains useful for standard software-visible control, while
the high-bandwidth path avoids five-channel AXI logic, IDs and a central full
AXI crossbar where those features provide no benefit.

### Native local-SRAM contract

The native request carries at least requester identity, byte address, read or
write command, transfer size, write data and write strobes. The response
carries read data and an explicit success/error indication. Revision 1 is
strictly in order and begins with at most one outstanding request per requester.
Arbitration is independent per SRAM bank and deterministic round-robin within a
bank. Bank conflicts cause back-pressure; requests to different banks may
progress concurrently.

The physical data width, number of banks, low-order bank mapping and pipeline
register depth are construction-time configuration values and must be reported
in metrics and the package manifest. They are **not frozen by D15**: those
values require the target SRAM macro, clock target and PD constraints. Phase 3
must validate them and provide a reference configuration, but must not silently
turn an illustrative 256-bit datapath into an architectural constant.

The SystemC implementation may transport 1..64-byte TLM payloads and split a
payload into physical bank beats internally. That split is an implementation
detail of the chosen fabric configuration. D7 still applies: VP++ vector
traffic is element-wise at the wrapper boundary, and counters must say whether
they count TLM requests, physical beats, bank conflicts or transferred bytes.

### Routing and ownership rules

- Control registers are reached only through the AXI4-Lite control plane.
- SA and ImageTransform bulk data are staged in core SRAM and use native local
  ports; neither engine becomes a full AXI4 NoC master in Revision 1.
- NEO DMA has one native local-SRAM port and one external AXI4/NoC-facing port.
  It remains independent of, and may not call, the Sauria DMA.
- A VP++ access is decoded by address: local SRAM uses the native data plane,
  local MMIO uses AXI4-Lite, and an address outside the core is handed to the
  existing chip/global path. This does not create a second RVV processor or a
  separate RVV master.
- Authorized inbound chip/NoC accesses use the reverse adapters: MMIO reaches
  AXI4-Lite and SRAM data enters as a named native requester. They use the same
  physical addresses and never bypass normal arbitration.
- No accelerator may bypass arbitration by taking a direct pointer to SRAM
  backing storage.

### RTL and PD constraints

The intended RTL structure is a small AXI4-Lite control decoder plus one
arbiter per SRAM bank and optional register slices on long paths. It must not
instantiate a general-purpose full AXI crossbar for local data. Banking and
register slicing are physical implementation parameters, so the model exposes
contention and pipeline latency without pretending to predict a final SRAM
macro or post-route frequency.

### Phase gate

Phase 3 must prove control/data address decode, per-bank ownership, independent
bank progress, deterministic same-bank arbitration, back-pressure, response
propagation, reset, 1..64-byte accesses, byte strobes and direct-backing-pointer
prohibition. Phase 4 must prove that the DMA moves every byte through its native
local port and external bridge, including partial-error accounting. A module or
metric named `neo_axi_fabric` after this migration is a stale D14 artifact.

The architecture documentation is rebaselined through D15 and finally
ratified as of 2026-08-12. Code and generated-address migration start in
Phase 3:

| Item | Status | Evidence / remaining implementation |
| --- | --- | --- |
| Record D1-D17 in the main decision log | Complete for documents | This document and the plan decision log agree; D14/D15 code migration landed in Phase 3; D17 added by the Phase 5 source audit |
| Rebaseline one NEO-CORE to VP++ + SRAM + independent DMA + one SA + Transform + split control/data fabrics | Documentation complete; SRAM and all three fabrics implemented and gated; DMA/SA/Transform pending Phases 4-6; composition pending Phase 7 | D14/D15, `ARCHITECTURE.md`, `ADDRESS_MAP.md`, `INTERFACE_CONTRACT.md`, the plan, and `tpu_v3_local_sram_fabric` / `tpu_v3_control_fabric` / `tpu_v3_external_bridge` |
| Rename SVM to core SRAM while retaining the 16 MiB window/capacity contract | Complete | D6 as amended by D14; `address_map.h`, `architecture_config.h`, the four shipped configurations and the packaged `--print-address-map` output all use `CORE_SRAM`, and both the CLI regression and the packaging regression fail if the legacy names reappear |
| Set BF16 operands with FP32 accumulation as the target SA arithmetic | Complete as a contract; not proven by the v4.2 bring-up type | D6/D14 |
| Integrate Sauria 64x64 first and promote the NPU-team 128x128 delivery later | Planned | D14 promotion gate |
| Obtain verified Im2Col/Col2Im source from the NPU team | Open external input; does not block SRAM/fabric/DMA/SA64 | D14 Transform boundary |
| Keep TPU_V3 DMA independent of Sauria DMA | Complete | D14 DMA boundary; `tpu_v3_neo_dma` implemented in Phase 4 and gated by `neo_dma_independence`, which scans the sources with comments stripped, the emitted symbols and the CMake link interface. The guard covers the shared PL330-style `components/dma_tlm` and DMI as well as Sauria, and it fails when a forbidden include is added |
| Replace default no-op CPU setters with D5 `cpu_config` properties | Complete | Phase 2 wrapper and configuration tests |
| Retire legacy `TPU_V3_MXU_BACKEND` and validate `TPU_V3_SA_GEOMETRY` | Complete | The CMake variable, the compiled-in value, `--version` and the manifest are all `TPU_V3_SA_GEOMETRY`; `128x128` and an unnamed geometry are both refused at configure time, with the packaging regression's negative control covering each |
| Make build provenance valid without Git and separate build/package revisions | Complete | Manifest schema 2 and packaging regression |
| Align core SRAM window/capacity decode behavior with D6/D14 | Complete | `core_sram` classifies an access above the capacity as `capacity_error` and never aliases it; `tpu_v3_core_sram`, `tpu_v3_address_map` and the CLI regression each check it |
| Separate the global Sauria option from linked/selectable binary state | Complete | Manifest and packaging regression |
| Require sparse host backing for the maximum logical memory configuration | Complete | `sparse_memory` with deterministic 4 KiB pages; `mesh_4x4` reports 1.25 GiB logical with 0 B allocated and peaks around 10 MiB of RSS, checked by `tpu_v3_sparse_memory` and by the CLI regression's own `ru_maxrss` bound |
| Replace the Spike runtime plan with RISC-V VP++ and retain Spike as a golden reference | Complete | D3; effective source `7a36fe85...` + approved F11/D12/D13 patch series, backend builds and passes `riscv_vp_plusplus_backend` |
| Backport `b710fa7b` and make F11 a Phase 2 closure gate | Complete | D8; `rvv_smoke_execution` runs two harts under a 1 ms watchdog with `mcycle` 15 → 2439 → 2848 |
| Scalar-FP fixes as expected diffs | Complete, wording amended | D9 as amended: all five stay in the upstream inventory, only `63524fbb` and `14e7fff5` are expected diffs (7 fields), and the disappearing-diff rule applies to exactly those seven. `91777991` is asserted illegal on both models, `c7140542` and `b92c01d8` asserted to match — regression checks, not tolerated differences |
| `vstart` via mid-vector trap | Complete | D10; `rvv_vector_trap`, and again in the differential corpus against Spike |
| Spike differential corpus | Complete | D11; `spike_differential` — **64 matched, 7 XFAIL, 0 OPEN, 0 unexplained** |
| F12 — RV32 index EEW=64 | Complete | D12 downstream conformance patch; `conformance_patches` with negative control |
| F13 — page fault reported for a failed bus access | Complete | D13 downstream conformance patch; `conformance_patches` covers six access origins, with negative control |
| Upstream maintenance-backport request | Deferred by decision, not blocking | revisit after Phase 2 closes, with the measured evidence |
| Accept element-wise vector memory traffic and rename the access counters | Complete | D7; `INTERFACE_CONTRACT.md` §6, plan §11.3 and the Phase 3 gate all synchronized |
| Decide full architectural reset semantics for a TPU core | **Open — due before Phase 7** | `reset_cpu()` is currently a restart at the reset PC plus cache reinitialisation; GPRs, FP/vector registers, CSRs, `vstart`, `instret`, pending interrupts and privilege level survive it. See `TPU_V3_PHASE2_AUDIT.md` F8 |

Phase 0 remains accepted and the Phase 1 build/package gate remains passed.
Every Phase 2 gate has a passing test in Debug and Release, and both
conformance patches have a verified negative control. No Phase 2 item is open.
D14/D15 do not invalidate those CPU results.

Phase 3 closed the core SRAM, the three D15 fabrics and the address-name
migration; D16 below records the one question Phase 3 had to answer that D15
did not. What remains for later phases is the DMA (Phase 4), the matrix engine
(Phase 5), the transform engine (Phase 6) and the composition of all of them
into a `tpu_core` (Phase 7).

## D16. Where the local-data plane is allowed to block

### Decision

`neo_local_sram_fabric` has two timing modes, selected at construction:

* **`annotated`** — loosely timed. No process ever waits. Bank occupancy is
  tracked as a busy-until timestamp and the resulting serialisation is added to
  the caller's `delay`. This is the **default** and the only mode permitted on
  any path reachable from a TLM target that the detailed NoC can reach.
* **`arbitrated`** — approximately timed. Requesters block on a real per-bank
  arbiter with deterministic rotating priority.

Both modes decode identically, split into identical beats, and return identical
data and status. Only how contention is charged differs.

### Why two modes exist

The Phase 3 gate asks for two things that pull against each other:
"same-bank round-robin ... back-pressure ... pass under watchdog", and "no
target waits inside `b_transport`".

Annotation alone cannot satisfy the first. With every request processed in call
order and nothing ever queued, there is never more than one contender at the
arbiter, so a round-robin arbiter and a fixed-priority one produce byte-for-byte
identical results. "Deterministic round-robin arbitration" would then be an
assertion in a document with no test capable of failing, which is the failure
mode this project has already had to fix once (audit §5a).

Blocking alone cannot satisfy the second. `INTERFACE_CONTRACT.md` §3 exists
because one SystemC process advances the detailed mesh clock, so a target that
waits freezes every node in the network and not merely itself.

Two modes resolve it honestly rather than by weakening either requirement.
The contention properties are proved where blocking is safe — a closed
core-local bench with a watchdog — and the default mode keeps the property that
makes the fabric safe to place behind a NoC-reachable target.

### The rule, stated precisely

"No target waits inside `b_transport`" binds:

* every TLM **target**: `core_sram` and `mmio_register_file` never wait;
* `neo_control_fabric`: never waits, in any configuration;
* `neo_local_sram_fabric` in `annotated` mode: never waits;
* `neo_external_bridge`: waits only if the fabric behind it was built
  `arbitrated`, which is why `blocks_on_arbitration()` exists and why the
  bridge's own report names the mode it got.

A platform that attaches an `arbitrated` fabric to the chip/NoC path is
misconfigured. Phase 9 must assert `!bridge.blocks_on_arbitration()` when the
detailed NoC backend is selected.

### Reset while a requester is blocked

Blocking creates a second obligation that annotation never had: a reset must
release the requesters it interrupts.

`reset()` bumps a generation counter, clears every bank, and notifies every
bank's event. A request carrying an older generation abandons itself at its
next resume point — waiting for a grant, holding a bank, or draining the
pipeline — and returns `neo_status::aborted` with whatever beats had already
completed. Beats that landed before the reset stay landed; the beat in flight
is dropped rather than written.

This is `ARCHITECTURE.md` §6 applied to the fabric: an in-flight job is
abandoned, and silently completing one that was reset mid-flight would be worse
than either alternative. The first implementation cleared `waiting[]` and
`busy` without waking anybody, which left a blocked requester both
unselectable and unwoken — it waited forever for a grant no arbiter could
issue. It presents as a deadlock, so the gate for it is a watchdog test that
resets in the middle of contention.

`in_flight_` is deliberately *not* cleared by reset: the processes owning those
flags are still unwinding out of `b_access` and their guards release them.

The generation is captured when `b_access` accepts the request, **before** it
consumes any caller-supplied temporal-decoupling delay. A reset during that
delay therefore aborts the old request before arbitration or storage access;
the request cannot resume under the new generation. Reset also starts a new
counter epoch. An old-generation request still returns its own partial
`bytes`/`beats` and `aborted` status to its caller, but its unwind does not add
those values or an error to the freshly cleared counters. This preserves both
the meaning of reset and the invariant `request_count >= error_count`.

### One request in flight per requester

D15 allows exactly one, and a blocking call enforces it only while one process
owns one requester identity. Two processes sharing an identity overlap the
moment the first waits for a bank, and from then on their beats interleave
under one name: the arbiter sees one contender where there are two, the
counters stay plausible, and the response ownership the fabric promises is
gone. The fabric therefore keeps a per-requester in-flight flag and throws on a
second entry, released by a guard on every path out including the one where the
storage throws.

### Consequence for temporal decoupling

`arbitrated` mode consumes the caller's unconsumed quantum before it can
contend — a requester still carrying `delay` is not "here yet", and charging it
against another requester's present would be meaningless. That means a VP++
hart attached to an `arbitrated` fabric synchronises to global time on every
local load and store, which removes the benefit of temporal decoupling.

This is a real cost and it is deferred deliberately, not overlooked. Phase 5
and Phase 7 must select `annotated` for full-system runs and reserve
`arbitrated` for the contention studies of Phase 11. Any timing figure must
name which mode produced it, exactly as `noc_timing` already has to be named.

### What would reopen this

A third mode that queues requests without blocking the caller — a genuine
approximately-timed fabric with non-blocking transport — would make both modes
unnecessary. It is not built now because nothing in Phase 3 needs it and
because the payload-ownership and response-routing contract such a mode
requires does not exist yet (D15 fixes one outstanding request per requester).

## D17. The Sauria matrix adapter is a buffered tile-staging design

### Decision

Phase 5's adapter **stages tiles**. Operands are prefetched from core SRAM into
private staging stores over `neo_local_sram_if`, the Sauria array runs against
those stores at its own signal-level timing, and results are written back the
same way:

```text
CORE_SRAM
   │  native local-SRAM fabric
   ▼
prefetch controller
   ▼
A/B tile staging store ──> Sauria feeders ──> 64x64 array ──> PSM
                                                               │
                                                               ▼
                                                        C staging store
                                                               │
                                                               ▼
                                                      writeback controller
                                                               ▼
                                                           CORE_SRAM
```

Job state is `IDLE → PREFETCH_A/B → COMPUTE → WRITEBACK_C → DONE/ERROR`.

SRAM-A receives A in the source's channel-major layout. SRAM-B receives exactly
one physical X-lane vector per K step: firmware's `B[K][N]` row fills lanes
`0..N-1` and lanes `N..X-1` are zero. Flattening `K*N` across vectors is
forbidden because it crosses B-row boundaries whenever `N != X`.

**Pass-through — one native transaction per SRAM access — is not implemented**,
and is reconsidered only if the NPU team supplies a feeder/controller interface
with `ready`/`valid` or another genuine stall mechanism.

### Why

The source **does** have stall signalling, and an earlier draft of this record
wrongly said it had none. The feeders drive `o_stall` (`ifmap_feeder.h:81`,
`wei_feeder.h:79`), the controller consumes it as `i_act_stall`/`i_wei_stall`
(`main_controller.h:54`, `:60`) and gates the compute pipeline on it
(`main_controller.h:564`).

What the source lacks is a **memory-side response handshake**. There is no
`mem_ready`, no `response_valid` and no way to defer a read's capture: the
feeder recovers data on a fixed schedule,

```cpp
bool mem_data_valid = rden_q2;   // ifmap_feeder.h:509
rden_q2 = rden_q1;
rden_q1 = false;
```

an unconditional two-cycle shift register. The existing feeder stalls throttle
the *compute* pipeline; they cannot postpone that capture, because nothing in
the SRAM port can tell the feeder the data is not there yet.

That is what makes pass-through unsafe rather than merely slow. Routing each
access through the `arbitrated` local fabric lets a bank conflict return late,
and because the capture window cannot move, the feeder latches whatever the
port happens to be driving. The result is not degraded timing — it is **wrong
data**, and it is wrong in a way that looks like an arithmetic defect.

Making pass-through correct would mean editing the feeders and the controller to
carry back-pressure. That is a change to the source, and it destroys the one
thing the extraction exists to establish: a differential against unmodified
Sauria. An adapter that had to modify the source before it could be compared
with the source proves nothing about the source.

Buffering is also RTL-realizable and consistent with the source's own on-chip
SRAM organization: the array is fed from SRAM-A/SRAM-B and drains to SRAM-C
rather than from a system memory, so staging follows the structure the model
already has instead of working around it.

That is deliberately weaker than "buffering is what the hardware does". No
Sauria RTL was examined for this decision — the SystemC model is not the RTL
(see `TPU_V3_PHASE5_AUDIT.md` §2) — so a claim about what the hardware does
would be unsupported.

### What this constrains

* The staging controllers reach core SRAM **only** through `neo_local_sram_if`.
  No backing pointer, matching the rule `core_sram` already enforces by
  exposing none.
* **No `SauriaDma`.** The controllers move data internally; the adapter is not
  an external AXI master, and plan §11.5 already gave TPU_V3 its own DMA.
* Sauria's `Sram` is **replaced**, not wrapped. The tile store must present the
  same signal-level interface and the same latency the feeders expect.
* The feeders, the array, the PSM and their **functional compute logic and
  timing are unmodified**. A diagnostic-only hygiene patch is permitted, and
  required — see below. It is recorded and hash-verified like the VP++ series,
  and it may not touch arithmetic, control flow or timing.
* Native transactions are issued from an `SC_THREAD`. A blocking transaction is
  never called from one of Sauria's `SC_METHOD` processes.

### The source is not instance-clean, and the adapter may not ship it as is

The dependency audit inventoried which modules to keep and missed what is
*inside* them. The kept modules carry mutable process-global state and write
trace files from the compute path:

| Where | What |
|---|---|
| `debug.h:16` | `#ifndef SAURIA_DEBUG / #define SAURIA_DEBUG 1` — debug defaults **on** |
| `sa_array.h:74`, `:100` | `static std::ofstream` writing `trace_sysc/sa_macq*.csv`, behind no macro, reachable from compute |
| `ifmap_feeder.h:1234` | `static std::ofstream`, gated by a **runtime instance-name test**, not a macro |
| `wei_feeder.h`, `psm_top.h` | four more `static std::ofstream` |

Seven `trace_sysc/*.csv` writers in total. A function-local `static` is shared
by every instance of the template, so two NEO-CORE SAs share one file handle and
one set of counters.

`INTERFACE_CONTRACT.md` §"No mutable global or static state" already forbids
this, in terms that fit exactly: "a `static` scratch buffer in a compute kernel
is a defect even when tests pass single-threaded, because SystemC processes
interleave at `wait()` boundaries."

`-DSAURIA_DEBUG=0` alone does **not** fix it, because the worst offenders are
not macro-guarded at all. So Phase 5 requires:

* `SAURIA_DEBUG=0` **and** `SAURIA_TRACE_FILES=0` are mandatory for the adapter
  build, not defaults a caller may override;
* a controlled **instrumentation-only patch set** that compiles out every trace
  stream and static debug object, carried like the VP++ downstream conformance
  patches: ordered patch files, a set hash, and the post-patch source hash
  recorded beside the base and oracle hashes in `TPU_V3_PHASE5_AUDIT.md` §1;
* a gate that elaborates **two** SA instances and requires that no `trace_sysc/`
  directory is created, that they share no state, and that their results are
  independent.

The Phase 5 implementation satisfies this with two ordered patches, a run-time
two-adapter independence test and an `nm` gate that refuses every Sauria
function-local-static symbol in both Release and Debug binaries.

### Buffer semantics under reset and error

These are settled here rather than discovered during the register-map freeze,
because firmware behaviour depends on them:

* **Firmware must not modify the A, B or C regions between `START` and
  completion.** The adapter does not snapshot them and does not detect the
  modification.
* **Prefetch is not an atomic snapshot.** Operands are read over several
  transactions; a concurrent writer produces a torn mixture, and that is the
  firmware's defect, not the engine's.
* **Writeback is not atomic either.** Data already written before a reset or an
  error **stays written**. The C region after a failed job is partially updated,
  and the completed byte count is the only thing that says how far it got.
* **Reset abandons the old job, and NEO-CORE reset is hierarchical.** The
  adapter generation prevents an old worker from publishing completion, error,
  timing or new-epoch traffic counters. The local fabric must be reset in the
  same hierarchy so a queued old beat returns `aborted` before it reaches SRAM.
  Resetting only the adapter cannot retract a request already accepted by the
  fabric; this is an integration error, not a supported reset sequence.
* **A replacement START is not an event that can be lost.** It remains pending
  while the one sequencer thread unwinds an abandoned blocking native access.
  The new job owns `C_BYTES_DONE` as soon as it is accepted, so a late response
  from the old generation cannot pollute the new job's register account.
* **On explicit abort:** `busy` clears, `aborted` sets, `done` does not set, no
  IRQ is raised, and the abort counter increments. **On execution error:**
  `busy` clears, `error` sets, the error counter increments and the shared
  completion/error IRQ is raised when enabled. Reset clears live status and IRQ
  without reporting an explicit abort event.
* **Accumulation and C-preload are refused** for the whole of Phase 5. Whether
  a partially written C region may be accumulated into is a semantics question
  this record does not answer, and an engine that silently accumulated into
  torn data would be the worst possible answer to it.

### Relationship to D16

Buffering does not remove this adapter from D16's scope. Prefetch and writeback
are ordinary local-data-plane traffic and are subject to D16 in full.

What buffering does is keep D16's latency and back-pressure **out of Sauria's
compute pipeline** — the one place that cannot absorb it. Contention is paid
during `PREFETCH_*` and `WRITEBACK_C`, where a controller is free to wait,
rather than during `COMPUTE`, where nothing is.

### What may and may not be claimed about timing

The adapter reports its time split:

```text
total_time = prefetch_time + source_compute_time + writeback_time
```

Only `source_compute_time` is cycle-correlated with the Sauria source. The other
two are this adapter's own traffic through a fabric the source never had.

**No report, manifest or measurement may describe the adapter as a whole as
cycle-accurate against the Sauria RTL.** The correlated part is one of three
terms, and it is reported separately so the distinction survives being quoted.

### Source pin

Phase 5 requires `TPU_V3_SAURIA_ROOT` to name
`components/npu_tlm/models/v4.2_model`, verified against the hash recorded in
`TPU_V3_PHASE5_AUDIT.md` §1.

The `v4.2_model_Aug01` fallback is **not** used by Phase 5: it is a different
source with different golden vectors, so a differential against it answers a
different question. The shared fallback inside `components/npu_tlm` is left
alone — other consumers depend on it, and changing which copy *they* silently
get is not Phase 5's decision to make.

The exact compiler input is the base hash plus the ordered patch set; the
post-patch tree and the three-file `demo_gemm_64x64` oracle each have their own
hash. Release and Debug binaries are scanned for Itanium `_ZZN6sauria...`
symbols so redirecting a trace to a null stream cannot masquerade as removal of
the shared static state.
