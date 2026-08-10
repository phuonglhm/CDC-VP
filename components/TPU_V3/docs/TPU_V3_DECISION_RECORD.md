# TPU_V3 Architecture and Integration Decision Record

Status: **Approved for implementation**  
Decision date: 2026-08-08  
Scope: TPU_V3 Phase 2 onward

This document records the six decisions made after the Phase 0 and Phase 1
review. It is an implementation authority for the items listed below. Where it
conflicts with a temporary value in `TPU_V3_PHASE0_AUDIT.md` or the decision
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
| D6 | SVM, MXU arithmetic and Sauria | Reference SVM capacity is 16 MiB/core; MXU arithmetic is BF16 x BF16 with FP32 accumulation; Sauria remains experimental and non-equivalent |
| D7 | Vector memory access granularity | Accept VP++ element-wise traffic; no upstream fork and no inferred coalescing. SVM supports 1..64-byte payloads; counters describe TLM requests, never vector instructions or hardware bus transactions |
| D8 | VP++ cycle-baseline defect (F11) | Controlled backport of exactly upstream `b710fa7b` onto the base pin. No baseline subtraction, no SystemC 3.0.1 migration. Recorded patch, script-applied, CMake-verified, manifest-tracked. A **Phase 2 closure gate** |
| D9 | Post-bump scalar-FP fixes | **Not** backported. Each becomes a named expected-diff against Spike, tied to its upstream commit; the corpus fails on an unexpected diff *and* on an expected diff disappearing |
| D10 | `vstart` restart scope | VP++ treats vector instructions as atomic with respect to interrupts, so interrupt-driven restart is unobservable. Test `vstart` through a mid-vector trap instead, and say so in plan §11.2 |

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

Implement and verify this before multi-chip Phase 8. It should be treated as a
Phase 7 prerequisite rather than deferred until the Phase 8 integration fails.

## D2. Chip/core limit for Revision 1

### Decision

Revision 1 is limited to:

```text
8 TPU chips
2 TPU cores per chip
16 TPU cores / RV32GCV harts total
4 MXUs per chip
32 MXUs total
```

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
CDC-VP owns the memory map, SVM, CLINT/PLIC integration, devices and NoC. Add a
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

## D6. SVM capacity, MXU arithmetic and Sauria scope

### SVM

The TPU_V3 reference configuration instantiates **16 MiB of SVM per TPU
core**, equal to the architectural SVM window.

Smaller SVM capacities may remain available only for explicitly labeled
bring-up or stress configurations. They are not the TPU_V3 reference result.
For any smaller capacity, the complete 16 MiB window still decodes to the SVM
target; the SVM component rejects accesses above its instantiated capacity and
must never alias them into valid storage.

This decision supersedes the temporary 4 MiB default recorded as Phase 0
decision P0-6.

### Global RAM

Keep 256 MiB as the configurable bring-up default, with the existing 1 GiB
window. This is simulated backing/global memory and must not be described as
an exact model of TPU v3 HBM capacity or bandwidth.

### Host-memory backing policy for Phase 3

Architectural capacity and host allocation are separate quantities. Phase 3
must provide sparse, page-backed storage for global RAM and SVM; global RAM
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
The `mesh_4x4` configuration has 1.25 GiB of logical storage (16 x 16 MiB SVM
plus 1 GiB global RAM) and must elaborate without committing that amount of
host memory.

### MXU arithmetic

The primary TPU_V3 fast MXU arithmetic contract is:

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

### Sauria

The available Sauria SystemC model is a 32x32 FP16/INT16 NPU top. It is not an
exact 128x128 TPU_V3 MXU and must not be put on the critical path for the fast
full-system model.

Sauria is restricted to an optional experimental/calibration role until the
Phase 9 audit proves a narrower claim. In particular:

- do not claim BF16 equivalence from FP16 behavior;
- do not claim exact 128x128 timing by composing 32x32 results;
- do not expose the Sauria backend as selectable until it builds, runs and has
  a verified redistribution policy;
- report Sauria-derived measurements separately from TPU_V3 reference results.

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

1. SVM must support TLM payloads of **1 to 64 bytes**.
2. VP++ currently issues **one transaction of 1, 2, 4 or 8 bytes per active
   element**.
3. The wrapper and SVM **must not** infer, group or reassemble vector
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
   SVM, and nothing downstream may assume it.
7. The 1..64-byte payload support is verified with a **synthetic initiator**.
   VP++ is not required to generate a 64-byte payload, and the absence of one in
   a VP++ trace is not a defect.
8. Timing and NoC metrics produced with this backend must record
   **`VP++ element-wise granularity`**. The arbitration-event counts must never
   be used to claim equivalence with TPU hardware.

### Synchronization required

`TPU_V3_PHASE2_AUDIT.md` F2 and the implemented wrapper already follow this
direction. `INTERFACE_CONTRACT.md` §6 and the Phase 3 gate must be brought into
line before Phase 3 code depends on the old wording.

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
5. `BUILD_MANIFEST.json` records base revision, backport commit, patch filename,
   patch SHA256, and `effective_source = base+backport`.

P2-3 is amended from "no patching" to **"no unrecorded patching; only the
approved F11 upstream backport"**.

### Not bundled with it

| Commit | Why not, and when |
| --- | --- |
| `7a936cce` "fixed fast quantum" | a larger change to quantum accounting; needs its own audit **before Phase 7** |
| `52d376d4` AMO atomicity / lost bus lock | needs a **multi-hart AMO contention test before Phase 6**; a single-threaded differential run against Spike cannot demonstrate atomicity between harts |

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
change can revisit it. A real CLINT in Phase 5 does not change this: atomicity
is a property of the ISS loop, not of the interrupt source.

## Synchronization status before Phase 2 implementation

The documentation/build contracts are synchronized as of 2026-08-08:

| Item | Status | Evidence / remaining implementation |
| --- | --- | --- |
| Record D1-D6 in the main decision log | Complete | This document and the plan decision log agree |
| Set the reference SVM capacity to 16 MiB | Complete | Configuration, address-map tests and documentation |
| Set BF16 operands with FP32 accumulation as the primary MXU arithmetic | Complete | Configuration, manifest and documentation |
| Replace default no-op CPU setters with D5 `cpu_config` properties | Contract complete; Phase 2 code pending | Phase 0 audit and plan now specify only the D5 approach |
| Validate `TPU_V3_MXU_BACKEND` at configure time | Complete | Only implemented backends configure successfully |
| Make build provenance valid without Git and separate build/package revisions | Complete | Manifest schema 2 and packaging regression |
| Align SVM window/capacity decode behavior with D6 | Complete | Code, tests and `ADDRESS_MAP.md` |
| Separate the global Sauria option from linked/selectable binary state | Complete | Manifest and packaging regression |
| Require sparse host backing for the maximum logical memory configuration | Contract complete; Phase 3 code pending | D6 and the Phase 3 gate |
| Replace the Spike runtime plan with RISC-V VP++ and retain Spike as a golden reference | Complete | D3; effective source `7a36fe85...` + backport `b710fa7b`, backend builds and passes `riscv_vp_plusplus_backend` |
| Backport `b710fa7b` and make F11 a Phase 2 closure gate | Complete | D8; `rvv_smoke_execution` runs two harts under a 1 ms watchdog with `mcycle` 15 → 2439 → 2848 |
| Scalar-FP fixes as expected diffs | Contract complete; corpus pending | D9 |
| `vstart` via mid-vector trap | Contract complete; test pending | D10 |
| Upstream maintenance-backport request | Deferred by decision, not blocking | revisit after Phase 2 closes, with the measured evidence |
| Accept element-wise vector memory traffic and rename the access counters | Complete | D7; `INTERFACE_CONTRACT.md` §6, plan §11.3 and the Phase 3 gate all synchronized |
| Decide full architectural reset semantics for a TPU core | **Open — due before Phase 5** | `reset_cpu()` is currently a restart at the reset PC plus cache reinitialisation; GPRs, FP/vector registers, CSRs, `vstart`, `instret`, pending interrupts and privilege level survive it. See `TPU_V3_PHASE2_AUDIT.md` F8 |

Phase 0 remains accepted and the Phase 1 build/package gate remains passed.
There is no remaining documentation synchronization blocker for Phase 2. The
actual `cpu_config`/RISC-V VP++ integration remains Phase 2 implementation
work, and the sparse memory backend remains Phase 3 implementation work.
