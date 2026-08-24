# TPU_V3 Architecture and Integration Decision Record

Status: **Final approved for implementation; NEO-CORE interconnect frozen by D15; hart reset contract frozen by D19; architectural block names frozen by D20; bus-lock scope frozen by D22; Phase 9 NoC transport frozen by D23; endpoint
counter ownership frozen by D24; remote-read shaping frozen by D25; admission-slot ownership frozen by D26**

Initial decision date: 2026-08-08

Architecture rebaseline date: 2026-08-11

D15 final ratification date: 2026-08-12

D18 Im2Col-only baseline date: 2026-08-18

D19 hart reset contract ratification date: 2026-08-18

D21 distribution boundary date: 2026-08-19

D22 bus-lock scope date: 2026-08-20

D23 NoC rebaseline date: 2026-08-20

D24 endpoint counter-ownership date: 2026-08-22

D25 remote-read shaping date: 2026-08-22

D26 admission-slot ownership date: 2026-08-24

D20 architectural block naming date: 2026-08-18

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
| D6 | Memory capacity and matrix arithmetic | Reference core SRAM capacity is 16 MiB/core; target MXU arithmetic is BF16 x BF16 with FP32 accumulation. The original Sauria-experimental scope is superseded by D14 |
| D7 | Vector memory access granularity | Accept VP++ element-wise traffic; no upstream fork and no inferred coalescing. Core SRAM supports 1..64-byte payloads; counters describe TLM requests, never vector instructions or hardware bus transactions |
| D8 | VP++ cycle-baseline defect (F11) | Controlled backport of exactly upstream `b710fa7b` onto the base pin. No baseline subtraction, no SystemC 3.0.1 migration. Recorded patch, script-applied, CMake-verified, manifest-tracked. A **Phase 2 closure gate** |
| D9 | Post-bump scalar-FP fixes | **Not** backported. All five stay in the upstream inventory; only the two observable at rv32gcv (`63524fbb`, `14e7fff5`, 7 fields) are expected diffs. The corpus fails on an unexpected diff *and* on one of those seven disappearing. The other three are ordinary regression checks — amended 2026-08-10 |
| D10 | `vstart` restart scope | VP++ treats vector instructions as atomic with respect to interrupts, so interrupt-driven restart is unobservable. Test `vstart` through a mid-vector trap instead, and say so in plan §11.2 |
| D11 | Differential method and oracle isolation | One image compiled once, run on both models, comparing a firmware-written canonical signature block located by linker symbols. Spike runs as a child process and is never linked into anything |
| D12 | RV32 index EEW=64 (F12) | **Downstream conformance patch**, not an accepted deviation. All 32 RV32 indexed encodings with index EEW=64 — four unit forms and 28 segment forms — raise an illegal instruction at the decode site, before `stats.inc_loadstore()` and `prepInstr()` |
| D13 | Trap cause for a failed bus access (F13) | **Downstream conformance patch.** Page faults only from MMU translation; a bus, decode or target failure is an access fault chosen by access origin — 1 fetch, 5 load, 7 store/AMO. Protocol errors are model defects, not guest faults |
| D14 | NEO-CORE architecture rebaseline | One VP++ RV32GCV hart, one shared core SRAM, one independent TPU_V3 DMA, one MXU and one Transform block with source-gated operations. Implement the MXU first from verified 64x64 Sauria source; promote to the NPU team's 128x128 source later. Never reuse the Sauria DMA. Its original single AXI-like-fabric wording is superseded by D15 and its temporary assumption that both transform directions arrive together is superseded by D18 |
| D15 | NEO-CORE internal interconnect | Split control and data: 32-bit AXI4-Lite for MMIO control; a native, pipelined, banked-SRAM request/response fabric for internal bulk data; full AXI4 only at the external chip/NoC boundary. NEO DMA owns bulk external movement; the required VP++ instruction/global path also exits through that boundary. Do not build a full AXI data crossbar inside NEO-CORE |
| D16 | Where the local-data plane may block | `neo_local_sram_fabric` has two timing modes. `annotated` is the default and never waits, so it is safe behind any NoC-reachable target; `arbitrated` blocks on a real per-bank round-robin arbiter and is the only mode in which fairness and back-pressure are behaviours rather than estimates. Every TLM target still never waits. A timing figure must name the mode that produced it |
| D17 | MXU adapter shape for the Sauria backend | **Buffered tile staging.** Operands are prefetched from core SRAM into private staging stores over `neo_local_sram_if`, the array runs against those stores at the source's own timing, results are written back the same way. Pass-through is not implemented: the source has feeder compute stalls but no SRAM-side response handshake, so a late read is captured as valid data. SRAM-B holds one physical 64-lane vector per K step and zero-pads partial N. Prefetch and writeback stay fully in D16's scope. Only `source_compute_time` may be called cycle-correlated. Requires hash-verified instrumentation-only patches and a binary gate proving the selected closure has no mutable function-local static state |
| D18 | Phase 6 Transform capability | **Im2Col-only Revision 1.** Extract the CHW INT8, no-padding, cross-correlation address order proven by the pinned v4.2 IFMAP/layout/golden evidence into a standalone buffered Transform block. All four padding fields must be zero. Output is row-major `[OH*OW][C*KH*KW]`. Col2Im is absent from the audited source: its capability bit stays zero and a requested start returns `unavailable_operation` without SRAM traffic. This does not block the forward-inference pipeline `DMA -> Transform (Im2Col) -> MXU -> RVV`; no guessed inverse or fake success is permitted |
| D19 | Full architectural reset for a NEO-CORE hart | **Full deterministic reset implemented in the VP++ wrapper.** The register, CSR and vector state is all publicly reachable and needs no upstream patch, so `reset_cpu()` stops being a restart. Two items stay explicit rather than hidden: the cycle counter turned out to need the fourth D8-mechanism patch and has it (`0004-d19-cycle-baseline-survives-reset.patch`), and reviving a terminated hart is out of Revision 1 scope and refused loudly. Implementation also found that a hart parked in `wfi` is not resumed by reset — the wake is necessary, not sufficient. Four state classes, not one rule: identity/configuration preserved (`mhartid`, `misa`, `vlenb`); specification-defined fields set per the privileged spec, `vtype`/`vl` among them; `sp` written by `init()`; and the remainder zeroed for reproducibility rather than because the spec requires it — with `time`/`mtime` outside all four as live CLINT state. Reset also releases the LR/SC reservation and bus lock, flushes the MMU TLB, and wakes a hart parked in `WFI`. `RegFile_T::reset_zero()` is not a register-file clear and `csrs` must never be reset by struct assignment. Gated by enumerating `csrs.register_mapping`, which covers every CSR with backing storage; derived views and live time CSRs are asserted separately |
| D20 | Architectural block names | The NEO-CORE architecture and reports call the matrix-multiplication block **MXU** and the tensor-layout block **Transform**. **Sauria** is used only for source/backend provenance, and **Im2Col** is the currently implemented Transform operation, not the block name. Existing code/ABI identifiers (`sauria_matrix`, `image_transform`, `SA_CONTROL`) remain unchanged by this documentation-only naming decision |
| D21 | Distribution boundary for a NEO-CORE binary | **Internal-build artifact.** CDC-VP may go public; a binary containing a NEO-CORE does not. The platform composes cores only in the internal configuration (both accelerator options on) and instantiates none in the default/public one, whose manifest keeps `sauria.linked` and `sauria.selectable` false. A placeholder MXU to give a public build a nominal NEO-CORE is refused |
| D22 | LR/SC and AMO bus lock scope | **One lock per chip, shared by both harts.** The CPU backend's per-hart default excludes nobody; `tpu_chip` creates the lock and attaches it to both harts during elaboration, through an opaque handle so the public CPU header still exposes no VP++ type. Upstream `52d376d4` stays unbackported, on measured evidence rather than on deferral
| D26 | Admission-slot ownership across a bypass ordering hold | **A routed transaction owns its admission slot from the admission gate until it returns from `b_transport()`**, not until the mesh answers it. Releasing it in `complete_manager_transaction()` admitted a second caller while the first was parked in `await_earlier_bypass()`, so a bound of one ran two concurrent calls and `outstanding_transactions()` read zero for both. Requires splitting the two idle predicates: `network_idle()` stops consulting admission slots so the clock gate can still gate an empty mesh, `wrapper_idle()` starts consulting them so it cannot report idle while a caller has not returned |
| D25 | Remote reads into a chip aperture | **Keep the whole chip aperture `target_kind::mmio`, and shape outbound reads at the endpoint** so every chunk is one the interconnect will not widen: naturally-aligned narrow prefix, bus-aligned full-width bulk, naturally-aligned narrow suffix. Publishing a bus-alignment restriction instead was rejected because it would silently narrow `neo_dma`'s already-frozen Phase 4 contract, which is gated at lengths 1, 2, 3, 7, 15, 63, 65 and odd addresses. No FlooNoC, address-map or socket-structure change |
| D24 | Endpoint outstanding and latency counters | **Split by quantity, not by component.** The chip endpoint measures **transfer** latency, which only it can see, because a chunked transfer is one transfer to it and several transactions to the interconnect. Outstanding **is** measured here too, per direction — the original deferral reasoned from one core and was retracted on 2026-08-24 after the composition measured a peak of 2. Neither figure may be quoted as the other: a chunked transfer is one endpoint transfer and several interconnect transactions |
| D23 | Phase 9 NoC transport | **Keep the shared single-AXI FlooNoC network unchanged.** Control/data virtual channels are *deprecated* at the pinned revision and cannot be added without leaving it; the narrow-wide network is live but answers a throughput requirement nobody has stated. Traffic class is a function of the address, taken from `region_kind`. Widths, protocol behaviour and the reset-of-in-flight rule are frozen in `TPU_V3_PHASE9_NOC_REBASELINE.md`; no new signed FlooNoC configuration is created

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
2 MXUs per chip
16 MXUs total
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
NEO-CORE shared SRAM, one MXU replaces the old two-MXU composition, and
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

### MXU implementation-source scope (Sauria v4.2), amended by D14/D20

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
| `52d376d4` AMO atomicity / lost bus lock | needed a **multi-hart AMO contention test before Phase 8**; a single-threaded differential run against Spike cannot demonstrate atomicity between harts. **Closed by D22 on 2026-08-20**: the test exists, and the commit stays out — see D22 for the evidence and for the one condition that reopens it |

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
├── one MXU (current implementation source: Sauria v4.2)
├── one Transform block (current operation: Im2Col; see D18)
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

### MXU staging with the Sauria implementation backend

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

This original D14 boundary is refined by D18. Transform operations remain
independently source-gated: Phase 6 has approved and implemented the pinned
v4.2 Im2Col subset, while Col2Im still has no identified implementation or
overlap/accumulation contract. Therefore:

- use the exact Im2Col source/layout/golden pins recorded by D18;
- ask the NPU team separately for any future Col2Im module, configuration
  contract, golden tests and supported layouts;
- do not infer Col2Im from PSM write ordering or implement a guessed inverse;
- unavailable Col2Im must reject start explicitly and may not report fake
  success;
- lack of Col2Im does not block the Revision 1 forward-inference pipeline.

### Rebaseline consequences

The following old requirements are superseded wherever they occur outside
historical Phase 0–2 audit evidence:

- two MXUs per core;
- `MXU0_CONTROL` and `MXU1_CONTROL` as current register names;
- Sauria only as a late optional Phase 9 backend;
- DMA ownership being open or reusing the Sauria DMA;
- treating the available Im2Col accelerator as an optional software-only step,
  or treating unavailable Col2Im as though it were implemented.

Phase 3 starts by migrating architecture configuration/address names and by
implementing core SRAM plus the D15 control/data interconnect split. The
detailed phase order and gates are authoritative in
`TPU_V3_IMPLEMENTATION_PLAN.md`.

## D15. NEO-CORE control/data interconnect split

### Decision

**Final ratification (2026-08-12):** the project owner approved this split as
the implementation architecture for both the SystemC/TLM model and the future
RTL handoff. It is no longer a proposal. Replacing the protocol split, making
MXU/Transform external AXI masters, or inserting a full AXI data crossbar
inside NEO-CORE requires a new recorded architecture decision.

NEO-CORE uses three deliberately different interconnect contracts:

1. **Control plane — 32-bit AXI4-Lite.** VP++ and the authorized external
   inbound adapter reach core, DMA, MXU and Transform register files
   through one narrow, in-order AXI4-Lite decoder. It has no bursts or AXI IDs
   and accepts at most one transaction per control initiator at a time. In the
   SystemC model this is represented at transaction level; signal-level
   AW/W/B/AR/R timing is not claimed.
2. **Local data plane — NEO Local SRAM Fabric.** VP++ local load/store, the
   DMA local port, the MXU feeder/result path, the Transform data port and
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
   `reset_pc` points at global boot ROM; it does not make MXU or Transform
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
- MXU and Transform bulk data are staged in core SRAM and use native local
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
| Record D1-D20 in the main decision log | Complete for documents | This document and the plan decision log agree; D14/D15 code migration landed in Phase 3; D17 and D18 record the Phase 5/6 adapter decisions; D19 freezes the hart reset contract; D20 freezes the MXU/Transform names |
| Rebaseline one NEO-CORE to VP++ + SRAM + independent DMA + one MXU + Transform + split control/data fabrics | Complete through Phase 7: the components are gated individually and composed into a `tpu_core` that boots one firmware image end to end | D14-D20, `ARCHITECTURE.md`, `ADDRESS_MAP.md`, `INTERFACE_CONTRACT.md`, the plan, and the component audits |
| Rename SVM to core SRAM while retaining the 16 MiB window/capacity contract | Complete | D6 as amended by D14; `address_map.h`, `architecture_config.h`, the four shipped configurations and the packaged `--print-address-map` output all use `CORE_SRAM`, and both the CLI regression and the packaging regression fail if the legacy names reappear |
| Set BF16 operands with FP32 accumulation as the target MXU arithmetic | Complete as a contract; not proven by the v4.2 bring-up type | D6/D14 |
| Integrate the MXU from Sauria 64x64 source first and promote the NPU-team 128x128 delivery later | 64x64 complete; 128x128 open | D14/D17 and Phase 5 audit |
| Obtain verified transform source | Im2Col complete; Col2Im remains an optional future promotion and is explicitly unavailable | D18 and Phase 6 audit |
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
| Decide full architectural reset semantics for a TPU core | **Closed by D19** (2026-08-18); implementation and gate are Phase 7 work | D19. The contract is a full deterministic reset in the VP++ wrapper; `reset_cpu()` as a restart plus cache reinitialisation is superseded. See `TPU_V3_PHASE2_AUDIT.md` F8 for the measurement that opened it |

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

## D17. The MXU adapter for the Sauria backend uses buffered tile staging

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
* a gate that elaborates **two** MXU instances and requires that no `trace_sysc/`
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

## D18. Phase 6 gives the Transform block an Im2Col-only baseline

### Decision

Revision 1 exposes **Im2Col only**. The audited v4.2 tree contains the address
generation required to lower an input feature map into matrix rows, but it
contains no identified Col2Im implementation or overlap/accumulation contract.
Those two capabilities are therefore independent: accepting Im2Col does not
authorize inventing the reverse operation.

The standalone TPU_V3 component implements this frozen transform:

```text
input     signed INT8, CHW contiguous
window    cross-correlation order c, ky, kx
output    signed INT8, row-major [OH * OW][C * KH * KW]
OH        1 + (H - (KH - 1) * dilation_h - 1) / stride_h
OW        1 + (W - (KW - 1) * dilation_w - 1) / stride_w
padding   unsupported; all four padding fields must be zero
```

This is a **no-padding** contract. Zero values disable padding; they do not
request implicit zero-fill outside the tensor. The pinned v4.2 contract has no
padding field, so adding that convention would be an invention.

### Source and extraction boundary

The evidence boundary is the pinned NPU-team `v4.2_model` tree:

| Evidence | SHA-256 | What it establishes |
| --- | --- | --- |
| `data_feeder/ifmap_feeder.h` | `156334177cfe1682faac049ce48745d9657ed4fcd5d58eb1c0bf5bffa3692ba0` | IFMAP window/stride/dilation address order |
| `driver/libsauria_mem.h` | `d5310c80b5e283a1ae133e9ba7b05d74c3559056f2a6ebd42f607391e328c982` | flattened CHW layout |
| `driver/sauria_golden.h` | `57327c4c91c7dcc1fa2d95509fee6f83e60eea96bf80be68afbd589a67aaae3b` | cross-correlation output semantics used by the differential gate |

`ifmap_feeder.h` is not copied wholesale. It is coupled to Sauria FIFO, skew,
controller and signal-level SRAM timing, so importing it as a nominal
standalone transform would retain unrelated matrix-engine behavior. The
address semantics are extracted into `components/TPU_V3/image_transform`, and
the build refuses a changed evidence hash until it is audited and rebaselined.
The golden gate factors the pinned NPU convolution reference through the new
Im2Col matrix and an independent integer GEMM; it therefore checks the layout,
stride and dilation rather than comparing two copies of the same helper.

### Adapter and timing boundary

The component has one 32-bit AXI4-Lite target, one
`neo_local_sram_if` requester and one level completion/error IRQ. `START`
returns immediately and one `SC_THREAD` performs the job. The input tensor is
prefetched through the native port into private staging, and output matrix rows
are written back through that same port in transfers no larger than 64 bytes.
There is no external AXI master and no pointer to SRAM backing.

This is a functional TLM staging policy. It preserves source-visible ordering
and exposes arbitration/errors/traffic, but it is not a claim about an RTL line
buffer, feeder schedule or cycle count. Reset/abort generation and traffic
epoch rules match the DMA/MXU rules already frozen by the project: committed
destination bytes remain committed, an abandoned worker cannot publish stale
status, and a replacement `START` cannot be lost.

### Col2Im and forward inference

Col2Im remains explicitly unavailable:

* its capability bit is zero;
* selecting it and issuing `START` completes with `ERROR` and
  `unavailable_operation`;
* it causes no local-SRAM transaction;
* a non-empty free-form source string is not allowed to turn it on in the
  common architecture configuration.

This does **not** block the current forward-inference model. After the
Transform block performs Im2Col and the MXU performs matrix multiplication,
the result is already the output-feature matrix;
RVV/software can apply bias/activation and interpret or reshape its rows as the
output tensor. Col2Im becomes necessary for operations that scatter and
accumulate overlapping columns, such as some backward-data, transposed
convolution or explicit fold workloads. Those are outside Revision 1. A future
Col2Im delivery needs its own source pin, mathematical layout and overlap rule,
golden tests and an explicit decision-record promotion.

Consequently Phase 7 uses:

```text
DMA -> Transform (Im2Col) -> MXU -> RVV
```

It must not insert a placeholder Col2Im step or report the pipeline as a
round-trip transform.

## D19. Full architectural reset for a NEO-CORE hart

Decision date: 2026-08-18. **Ratified by the project owner on 2026-08-18.**
Closes the item recorded as "**Open — due before Phase 7**" in the
synchronization table above, in plan §23 and in `TPU_V3_PHASE2_AUDIT.md` F8.

Changing the four state classes, the release/wake set, or the disposition of
the two items this decision leaves open — the cycle counter and revival of a
terminated hart — requires a new recorded architecture decision, on the same
terms D15 set.

### Decision

A NEO-CORE hart reset is a **full deterministic architectural reset**,
implemented in `cdc::cpu::riscv_vp_plusplus`. The
restart-plus-cache-reinitialisation that `reset_cpu()` performs today is not
the contract and may not be described as one.

**The register, CSR and vector state needs no upstream patch** — that is the
part audit F8 opened, and all of it is publicly reachable. Two adjacent items
are not, and this decision keeps them visible rather than folding them into a
tidy headline: the cycle counter has no wrapper-only answer yet, and reviving a
terminated hart is out of Revision 1 scope. Both have their own sections
below.

Rejected: leaving `reset_cpu()` as a restart and writing a narrower contract
around it. Phase 8's gate is that two harts boot and run independent workloads
and that neither can corrupt the other; a "reset" that leaves GPRs, CSRs and
the vector register file loaded with the previous workload's values makes that
gate untestable. Deferring is how it becomes a Phase 8 discovery, which is
exactly the failure mode D1 and P2-10 were written to prevent.

Also rejected: adding a broad `ISS::reset()` upstream patch to the D8/D12/D13
series. For the register, CSR and vector state such a patch would buy tidiness
only, and D12 already records the cost: a downstream patch has to be re-checked
against every new base, forever. That reasoning applies to the state that is
already reachable; it is not a blanket refusal, and the cycle-counter section
below records the one place a fourth patch may still be the right answer.

### What was measured

Read from the pinned tree on 2026-08-18, `third_party/riscv-vp-plusplus`,
base `7a36fe859cae242f513ca6ad16ab8238f1e82977`:

| State | Where | Reachable from the wrapper? |
| --- | --- | --- |
| GPRs | `iss_ctemplate.h:60`, public `RegFile regs` — a plain `T_sxlen_t regs[32]` | yes, direct |
| FP registers | `iss_ctemplate.h`, public `FpRegs fp_regs` | yes, direct |
| CSRs | `iss_ctemplate.h`, public `ISS_CT_T_CSR_TABLE csrs` | yes, **field by field only** |
| Vector registers | `v.h:28`, **private** `void* v_regs`; no reset method | yes, element-wise through the public `reg_write<T>(vec_idx, elem_num, val)` |
| Privilege level | `iss_ctemplate.h`, public `PrivilegeLevel prv` | yes, direct |
| Execution status | `iss_ctemplate.h:160`, public `set_status(CoreExecStatus)` | yes |
| PC | `iss_ctemplate.h:26`, **protected** `uxlen_t pc` | only through `ISS::init()`, which is what `reset_cpu()` already calls |
| LR/SC reservation | `iss_ctemplate.h:173`, public `release_lr_sc_reservation()`; the `lr_sc_counter` behind it is protected | yes, through the public call |
| Bus lock | `riscv_vp_plusplus_wrapper.cpp:184`, the wrapper's own `local_bus_lock` | yes, it is CDC-VP code |
| MMU TLB | `mmu.h:85` `tlb[][][]`, public `flush_tlb()` at `:91`; `ISS::init()` never calls it | yes |
| Cycle counter | `cycle_counter` public on the ISS, but the accumulator it is derived from (`dbbcache.h:620`) is **private** with only a getter, and `cycle_counter_raw_last` is protected | **no — see the cycle-counter section** |
| Termination | `shall_exit` protected (`iss_ctemplate.h:32`), set at `iss_ctemplate.cpp:7084`, re-applied at `:293`; `rv32::ISS` is `final` here | **no — out of Revision 1 scope** |

Three of the reachable ones carry a trap, and each has cost someone a day
somewhere:

1. **`RegFile_T::reset_zero()` is not a register-file clear.** Its whole body is
   `regs[zero] = 0` (`regfile.h:22`) — it re-zeroes `x0`, the hardwired-zero
   register, and nothing else. The name reads like the bulk clear this decision
   needs. It is not, and it must not be used as one.
2. **`csrs` must never be reset by struct assignment.** `csr_table` carries
   `std::unordered_map<unsigned, uint32_t*> register_mapping` populated in its
   constructor with pointers **into its own members** (`csr.h:663`, `:724`).
   `csrs = csr_table{}` copies pointers that address the temporary, which then
   dies — every subsequent CSR access through the mapping is undefined
   behaviour. The reset assigns fields and leaves `register_mapping` alone.
3. **Blind zeroing breaks frozen contracts.** `mhartid` carries D5 identity,
   `misa` carries `V` and zeroing it disables RVV, and `vlenb` is written once
   by the `VExtension` constructor (`v.h:83`) and never again. A reset that
   zeroed all three would elaborate, run, and report a hart that is not the one
   the configuration asked for.

Nothing under `components/TPU_V3` calls `reset_cpu()` today; the only caller in
the repository is the unrelated `platforms/tests/wdt_platform`. Phase 7 is
therefore the first NEO-CORE caller, and this contract does not have to
preserve any existing behaviour.

### The contract

Reset state falls into four classes, and the class decides the value. This is
deliberately not one rule, because the RISC-V privileged specification does not
define reset values for most of this state.

**Class 1 — preserved.** Identity and configuration survive reset, because they
are properties of the instance, not of the run: `mhartid`, `misa`,
`mvendorid`, `marchid`, `mimpid`, `vlenb`, and the `register_mapping` table.

**Class 2 — specified.** Set to what the privileged specification requires at
reset: `pc` = the configured `reset_pc` (placed by `init()`), privilege level =
Machine, `mstatus.MIE` = 0, `mstatus.MPRV` = 0.

`mcause` = **0**. The specification requires an indication of the reset cause
but leaves the encoding implementation-defined, and `reset_cpu()` carries no
argument that could distinguish one cause from another. Zero is chosen and
written down; inventing a numeric cause the API cannot supply would be a
distinction the model does not actually make.

`vtype` = **`vill` set, every other bit zero**, with `vl` = 0. RVV 1.0
*recommends* exactly this at reset — the hard requirement it states is only
that `vtype` and `vl` be readable and restorable by a single `vsetvl`, which a
zeroed pair would also satisfy. The model adopts the recommendation because the
alternative is worse in a specific way: a zeroed `vtype` is the *valid*
configuration SEW=8/LMUL=1, so the hart would come out of reset advertising a
vector configuration no firmware ever asked for, and a missing `vsetvli` would
run instead of trapping. These two are therefore Class 2 and **not** part of
the zeroed remainder.

That value has to be written explicitly, because the upstream default does not
provide it. `csr_vtype` initialises `uint32_t val = 0x8000000;` with the
comment `// vill=1 at reset` (`csr.h:276`). In that bitfield — `vlmul:3`,
`vsew:3`, `vta:1`, `vma:1`, `reserved:23`, `vill:1` — `vill` is bit 31, so the
value that sets it is `0x80000000`. `0x8000000` is bit 27, which lands in
`reserved`, and `fields.vill` consequently reads **0** on a freshly constructed
hart. The comment describes the intent and the constant is one hex digit short
of it. This is recorded as an upstream defect rather than worked around
silently, and it is the reason the gate below asserts the exact bit pattern
instead of asserting "vill is set" against a default that already claims to be.

**Class 3 — written by `init()`, not zeroed.** `sp` (`x2`). `ISS::init()`
assigns `kFallbackStackTop` to it after the wrapper has cleared the register
file, so a contract that also called `sp` zero would contradict its own
ordering. `sp` is therefore excluded from the zeroed-GPR assertion and checked
against the configured stack top instead.

**Class 4 — zeroed for determinism, not because the specification says so.**
The remaining GPRs, FP registers, the vector register file, `vstart`, `vxrm`,
`vxsat`, `vcsr`, `fcsr`, `instret`, `satp` (which returns the hart to Bare),
`mip`, `mie`, `mepc`, `mtval`, `mtvec`, `mscratch` and the PMP registers.
**The specification
leaves these undefined at reset**; the model zeroes them because a virtual
platform whose post-reset state depends on what ran before it is not
reproducible, and reproducibility is the same property D6 protects with a fixed
accumulation order and deterministic page backing. The distinction is recorded
rather than blurred: a future firmware that relies on a zeroed GPR after reset
is relying on this model, not on RISC-V.

**Not a class: state the model does not own.** `time` and `mtime` are read
live from the CLINT — `get_csr_value()` calls `clint->update_and_get_mtime()`
and only then fills `csrs.time` (`iss_ctemplate.cpp:6807`). They are a view of
platform time, which reset does not rewind, so they are excluded by name from
both the zeroing and the gate. Writing zero into `csrs.time` would be erased by
the next read anyway; excluding it says so instead of leaving a check that
appears to pass and measures nothing.

Order is fixed: clear Class 4 and set Class 2, **then** call `init()`, which
places the PC, writes Class 3 and reinitialises the decode/load-store caches.

Four things must be handled that are neither registers nor CSRs. Three of them
are reachable without patching upstream:

* **The LR/SC reservation and the bus lock.** `release_lr_sc_reservation()`
  (`iss_ctemplate.h:173`) is public and already does both — it clears the
  protected `lr_sc_counter` and calls `mem->atomic_unlock()`, so one call
  covers the reservation and the wrapper's own `local_bus_lock`
  (`riscv_vp_plusplus_wrapper.cpp:184`). `init()` calls neither. A reset
  between `LR.W` and `SC.W` would otherwise leave a reservation and a held lock
  behind. That is harmless while the lock is per-wrapper and becomes a hang the
  moment multi-hart atomicity makes it chip-shared, which is exactly the D8
  `52d376d4` work Phase 8 has to close.
* **A hart asleep in `WFI`.** `WFI` blocks the ISS in
  `sc_core::wait(wfi_event)` (`iss_ctemplate.cpp:6681`) on a **protected**
  event, inside the call stack of `core.run()`. None of the register or CSR
  writes above, and not `init()`, ends that wait, so without this the contract's
  claim to reset a *running* hart would be false for firmware that is idling —
  which is what firmware waiting on a DMA or MXU completion IRQ is doing. The
  public `maybe_interrupt_pending()` (`iss_ctemplate.h:163`) notifies
  `wfi_event` and forces the slow path **without injecting an interrupt**, so
  reset wakes the hart rather than fabricating a completion firmware never
  received. It is called after the state has been cleared and `init()` has
  placed the PC, so the woken hart resumes into the reset state; that ordering
  is a claim a test has to confirm, not an argument, which is why the gate
  below resets a hart parked in `WFI`.
* **The MMU TLB.** `MMU::flush_tlb()` (`mmu.h:91`) is public and `init()` does
  not call it. Stated honestly: at `satp.MODE = Bare` the TLB is never
  consulted (`mmu.h:96`) and TPU_V3 runs Bare, so this is insurance rather than
  a live defect today. It becomes load-bearing the moment any configuration
  enables translation, and the cost of flushing is one `memset` — the same
  posture D13 took with its unreachable page-table-walk path.
* **`cycle_counter`**, discussed next, because it is not simply reachable.

### The cycle counter is the one item without a wrapper-only answer

**Requirement:** reset must not make `mcycle` jump, and must not inject
simulated time into the quantum keeper.

**Measured obstacle.** `commit_cycles()` (`iss_ctemplate.h:99`) computes
`inc = dbbcache.get_cycle_counter_raw() - cycle_counter_raw_last`, then adds
`inc` to both `cycle_counter` and `quantum_keeper`. `ISS::init()` sets
`cycle_counter_raw_last = 0` but does **not** reset the accumulated raw count:
`DBBCacheBase_T::init()` (`dbbcache.h:145`) assigns only `enabled`,
`isa_config`, `hartId`, `instr_mem`, `opMap`, `fast_abort_labelPtr` and
`mem_word`, and the accumulator itself (`dbbcache.h:620`) is **private** with
only a getter. So the first `commit_cycles()` after a mid-run reset computes a
delta against zero and re-adds the entire pre-reset count — into simulated
time, not merely into a counter.

Zeroing `csrs.cycle` does not help and is not the fix: `MCYCLE_ADDR` is
recomputed on every read from `_compute_and_get_current_cycles()`
(`iss_ctemplate.cpp:6821`), so the CSR field is a cache of `cycle_counter`, and
`cycle_counter` (public `sc_time` on the ISS) is the state that matters.

**Disposition.** The implementation must measure the post-reset delta with a
test rather than assume it. If a wrapper-only path exists in the disabled-
dbbcache configuration TPU_V3 actually uses (P2-5), take it. If none does, this
single item — and nothing else in this decision — is the candidate for a
**fourth** patch under the D8 mechanism: classified, hash-verified,
script-applied, CMake-verified, manifest-recorded. It would be a downstream
patch in D12's sense, not a backport, because there is no upstream commit for
it.

This is why this decision does not claim "no patch anywhere". It claims the
register, CSR and vector state needs none, which is the part F8 opened.

### What the Phase 7 implementation measured

Added 2026-08-19, when the contract was implemented. Two of the things this
decision left open have answers now, and one of them contradicts what the
decision assumed.

**The cycle counter needed the fourth patch, and it is applied.** The
measurement D19 asked for rather than assumed: with the hart executing across
a reset, `mcycle` did not drop, because `ISS::init()` sets
`cycle_counter_raw_last = 0` while the dbbcache accumulator survives, so the
first `commit_cycles()` afterwards re-added the entire pre-reset count. No
wrapper-only path exists — the accumulator is private and
`cycle_counter_raw_last` is protected — and the obvious workaround, forcing a
commit to absorb the stale delta, would push exactly that count into the
quantum keeper, which is the thing this decision said must not happen.

So `0004-d19-cycle-baseline-survives-reset.patch` baselines against
`dbbcache.get_cycle_counter_raw()` instead of zero. It is a
`downstream-conformance` patch in D12's sense, carried under the D8 mechanism
with its content hash, its post-patch file hash and its manifest entry. It is
the same fix upstream `b710fa7b` made for the startup case, stated generally:
the baseline is whatever has already been accumulated, which is zero at
construction and is not zero at a reset.

**A hart parked in `wfi` is not resumed by reset, and the earlier wording here
was wrong.** This decision said `maybe_interrupt_pending()` "wakes the hart".
It is necessary and it is not sufficient. VP++ implements the instruction as
`while (!has_local_pending_enabled_interrupts()) wait(wfi_event);`, and reset
has just zeroed `mie`, so the woken hart re-evaluates the condition, finds it
false, and sleeps again without ever fetching from the reset vector.

The structural reason is the one the terminated-hart case already has: the
blocking wait sits inside the ISS's instruction execution and nothing outside
it can unwind the loop. Making the call fabricate a pending enabled interrupt
would exit the loop and is refused — it would hand firmware a completion it
never received. So Revision 1 resets a **running or trapped** hart; a hart
idling in `wfi` keeps its reset state but does not restart, and a platform must
not rely on resetting an idle hart to make it run. Lifting it needs a further
downstream patch and is not in this decision's scope.

`test_architectural_reset` asserts the limitation rather than only recording
it, on the D10 precedent: if a future backend does resume such a hart, that
check fails and this section is what has to be corrected.

### What this reset does not cover

Named so they are not mistaken for cleared items:

* **A hart that has already terminated is not revived, and reset says so
  loudly.** `sys_exit` sets the protected `shall_exit` (`iss_ctemplate.cpp:7084`),
  `exec_steps()` turns any `Runnable` status straight back to `Terminated`
  while it is set (`:293`), `rv32::ISS` is `final` in this build because
  `ISS_CT_ENABLE_POLYMORPHISM` is not defined, and the wrapper's
  `core_runner::run()` is one-shot — it calls `core.run()` once and returns, so
  its `SC_THREAD` is gone. `set_status(CoreExecStatus::Runnable)` is therefore
  necessary and not sufficient, and a reset that appeared to succeed would hand
  the platform a hart that silently never executes again. Revision 1 supports
  resetting a **running or trapped** hart; `reset_cpu()` on a terminated hart
  throws with a message naming this limitation. Lifting it needs either a
  downstream patch exposing `shall_exit` or a `core_runner` restructured to
  loop on a restart event — both are recorded here, neither is Revision 1, and
  neither is required by the Phase 7 or Phase 8 gates, which reset harts that
  are still running.
* **In-flight fabric traffic.** A hart blocked inside `b_transport` when its
  core is reset unwinds under the local fabric's own generation rules (D16);
  the CPU reset does not and cannot retract a request the fabric already
  accepted. NEO-CORE reset is hierarchical, so the fabric is reset in the same
  sequence — this is the same rule D17 states for the matrix adapter.
* **Simulated time.** Reset does not rewind `sc_time_stamp()`. A reset is an
  event in the run, not a new simulation. That is separate from, and must not
  be confused with, the requirement above that reset inject no *new* time.

### Gate

`reset_cpu()` is not complete until a test proves all of the following, in
Debug and Release:

* **Every CSR with backing storage is checked, and the test cannot silently
  miss one.** Iterate `csrs.register_mapping` and assert each entry against its
  class. The claim is deliberately narrower than "every CSR": the map
  enumerates the CSRs that *have storage* — including `vtype`, `vl`, `vstart`,
  `fcsr`, `satp`, the PMP block and the vector CSRs — while `sstatus`, `sie`,
  `sip`, `fflags`, `frm` and the hardwired debug/HPM reads have no entry
  because `get_csr_value()` derives them from `mstatus`, `mip`, `mie` and
  `fcsr` (`iss_ctemplate.cpp:6845` onward). Those derived views follow their
  backing register by construction, so they get one explicit assertion each
  rather than a map iteration; `time`/`mtime` are excluded by name as live
  CLINT state. A hand-written list of the *storage* CSRs would rot the first
  time upstream adds a register; the map cannot, and stating what it does and
  does not cover is what keeps the gate honest.
* `vtype` reads exactly `vill` set with all other bits zero, and `vl` is zero.
  Asserted as the full 32-bit pattern, because the upstream default claims
  `vill=1` in a comment while setting bit 27.
* Dirty-then-reset for each class: write a non-zero pattern into every GPR, FP
  register, vector register and Class 4 CSR, take the reset, and read back.
  "Unchanged" is asserted against a written pattern, never against a value that
  might already have been zero — the same positive-statement rule D12's
  `vstart` check uses.
* `mhartid`, `misa` (including `V`) and `vlenb` survive reset with their
  configured values, on a hart whose `hart_id` is deliberately not 0.
* `sp` holds the configured stack top after reset, and is the one GPR the
  zero assertion excludes.
* **`mcycle` does not jump.** Run a workload, reset mid-run, and require that
  the first post-reset `mcycle` sample is a plausible baseline rather than the
  pre-reset total, and that `sc_time_stamp()` does not advance by the pre-reset
  cycle count at the next commit. This is the check that would have caught the
  obstacle above; it fails today.
* A reset taken between `LR.W` and `SC.W` leaves no reservation and no held bus
  lock.
* A hart parked in `WFI` is reset and executes from `reset_pc`, under a
  watchdog — this is the case that hangs if the wake step is missing, and a
  hang is the failure mode a watchdog exists to convert into a result. A
  negative control removes the wake and requires the test to fail.
* Resetting a terminated hart throws, with the message naming the limitation —
  a negative control, so the unsupported case cannot silently become a hart
  that never runs.
* Two harts in one simulation: resetting one leaves the other's registers,
  CSRs and vector state untouched. This is the Phase 8 property, gated here
  rather than discovered there.
* A negative control: reverting the register-file clear fails the gate.


### Relationship to Phase 7

Phase 7 wires platform → chip → core → component reset and is the first caller.
Its gate already requires that reset cases "fail predictably under watchdogs";
with this contract that gate asserts a defined state instead of asserting that
nothing crashed. Plan §16 Phase 8 previously listed "full architectural reset"
among the gates it would close; that line is superseded by this decision, which
closes it before Phase 7 as the plan's own decision log always required.

## D20. Architectural block naming

Decision date: 2026-08-18.

### Decision

NEO-CORE architecture diagrams, plans, reports and new documentation use these
block names:

| Architectural block | Meaning | Implementation/provenance terms |
| --- | --- | --- |
| **MXU** | The matrix-multiplication unit | The current 64x64 implementation is extracted from pinned Sauria v4.2 source |
| **Transform** | The tensor-layout transformation block | Revision 1 currently implements the Im2Col operation; Col2Im remains unavailable under D18 |

`Sauria` is therefore not the architectural block name. It remains necessary
when identifying the third-party source/backend, its DMA that must not be
reused, source hashes, source symbols, test labels and implementation paths.
Likewise, `Im2Col` names an operation supported by Transform, not the block
itself.

This is a documentation and reporting decision. It does not silently rename
existing code or firmware ABI identifiers such as `sauria_matrix`,
`TPU_V3_SA_GEOMETRY`, `SA_CONTROL`, `image_transform`, or their test names.
Changing those identifiers requires a separate compatibility-controlled code
migration. Until then, documentation must describe them as retained
implementation identifiers for MXU and Transform rather than as architectural
block names.

## D21. The NEO-CORE binary is an internal-build artifact

Decision date: 2026-08-19. Project-owner decision, taken when Phase 7 finished
the composition and platform integration had to choose a shape.

### Decision

CDC-VP may become a public repository. **A binary containing a NEO-CORE is
internal-use only** and is not part of that release.

The consequence for the platform is a build split, not a feature flag:

* **Public configuration** — both accelerator options off, which is the
  default. `tpu_v3_soc` instantiates no NEO-CORE, says so in its report, and
  its manifest keeps `sauria.global_option_enabled`, `sauria.linked` and
  `sauria.selectable` all false. This is the configuration
  `tpu_v3_soc_packaging_regression` asserts, and those three assertions stay.
* **Internal configuration** — `CDC_BUILD_TPU_V3_SAURIA_MATRIX=ON` and
  `CDC_BUILD_TPU_V3_IMAGE_TRANSFORM=ON`. `tpu_v3_soc` composes real
  NEO-COREs, and the manifest declares `linked` true. That package is not
  published.

### Why

`licenses/SAURIA.PROVENANCE.md` separates two things that are easy to run
together. Building internally against the external SystemC implementation is
what `SAURIA_NPU_ROOT` exists for and is explicitly contemplated. *Publishing*
a binary containing it is not: "a public CDC-VP source release does not include
the external implementation or an NPU-enabled binary", and doing so needs the
rights owner's separate approval and a confirmed licence for that
implementation.

So `SAURIA_LINKED=FALSE` in the packaged manifest is not a stale line waiting
for Phase 7 to update it. It is a property the public package has to keep, and
the packaging regression is what keeps it.

### What this forbids

**A placeholder MXU so that a public build can still have a "NEO-CORE".** D14
freezes one MXU per core and the constructor validates the count, so a core
without one does not elaborate — which is the correct behaviour and must not be
worked around. Plan §10 already states the general form: an exported target
that links and does nothing is worse than a missing one, because a platform can
depend on it and appear to work. A public NEO-CORE with a stub matrix engine
would be exactly that, and every timing or throughput number taken from it
would describe nothing.

A public build therefore has **no** NEO-CORE, and its report says so rather
than presenting a reduced one.

### Recorded consequence for a public release

This working tree contains `components/npu_tlm/models/v4.2_model`, the external
SystemC implementation itself. The provenance notice describes a public tree as
one that does *not* contain it. Whoever prepares a public release has to
reconcile those two facts — by stripping those directories, or by obtaining the
approval the notice calls for. It is recorded here because it is invisible
until release day and expensive to discover then; it is not a Phase 7 item and
nothing in Phase 7 depends on the answer.

## D22. The LR/SC and AMO bus lock is chip-scoped

Decision date: 2026-08-20.

### Decision

**One bus lock per chip, shared by both NEO-CORE harts.** `tpu_chip` creates it
and attaches it to both harts during elaboration; a hart that is never given one
keeps the CPU backend's private default, which is correct for a single-hart
platform and correct for nothing else.

The lock is handed out as an opaque `std::shared_ptr<cdc::cpu::shared_bus_lock>`
from `cdc::cpu::make_shared_bus_lock()`. The type derives from a VP++ interface
and is declared but never defined in the public header, so the rule that this
backend's public interface exposes no VP++ type survives the change.

`attach_bus_lock()` throws after `start_of_simulation()`. Swapping the lock
under a running ISS would drop an outstanding reservation and could leave a
waiter parked on an event nobody will notify again; refusing is the D5 posture.

**Scope is the address space, not the chip.** Revision 1 has no cross-chip
atomics, so the two happen to coincide. If a later revision gives the mesh
atomics, the scope follows the shared address space and this decision is
reopened rather than reinterpreted.

### Upstream `52d376d4` stays out, and this is now evidence rather than deferral

D8 held the commit back until a multi-hart AMO contention test existed. It
exists — `cpu_models/riscv_vp_plusplus/tests/test_bus_lock_atomicity.cpp` — and
what it measures is:

| Run | Result |
| --- | --- |
| `amo shared` | 128 of 128 increments land; 62 contention waits |
| `amo unshared` | **64** of 128 — exactly half, every second update lost |
| `lrsc shared` | 128 of 128; 40 contention waits |
| `lrsc unshared` | **64** of 128 |

The unshared runs are the negative control and they are registered as tests: a
change that makes them pass fails the suite instead of quietly removing the
evidence for the shared runs.

The defect `52d376d4` fixes is a bus lock lost **between an AMO's load and its
store**, because every store releases the lock and MMU address translation for
the store may itself perform stores when it walks page tables. TPU_V3 runs
`satp.MODE = Bare` in machine mode, where `translate_virtual_to_physical_addr()`
returns the address without touching memory (`common/mmu.h:96`), so the
mechanism has no way to occur. That is the reason the commit is not required —
not "the tests pass".

**The condition that reopens it:** any configuration that enables address
translation. The argument above is entirely about `Bare`, and the commit becomes
required the moment a TPU_V3 hart runs with `satp.MODE != Bare`.

### A related D19 prediction, corrected by measurement

D19 wrote that a reservation left behind by a reset "becomes a hang the moment
multi-hart atomicity makes it chip-shared". Measured, it does not. Upstream
implements the RISC-V forward-progress property by arming a 17-instruction
counter at `lr.w` and calling `release_lr_sc_reservation()` when it expires
(`rv32/iss_ctemplate.cpp:301`), which releases the bus lock as well. A hart
cannot strand the lock by taking a reservation and branching away.

`bus_lock_forward_progress` pins that: one hart takes the lock and spins forever
without an `sc.w`, and the sibling is required to be blocked for a while and
then to finish on its own. If the bound ever disappears upstream, that test
hangs into its timeout instead of the change going unnoticed.

Resetting a holder still releases the lock — `reset_cpu()` calls
`release_lr_sc_reservation()` — and that path is what `tpu_chip::reset()` relies
on. The chip does not clear the lock from outside: an owner-aware release is
correct at any scope, and a blind one would release a lock this chip's reset had
nothing to do with the moment the scope grows.

### What this does not claim

The lock serialises **every** access from other harts, not only atomic ones:
upstream calls `wait_for_access_rights()` on each load, store and instruction
fetch. That is a faithful model of a locked bus and a pessimistic model of a
modern coherent interconnect, and no throughput figure taken from a run with
contended atomics should be presented as either.

## D26. A routed call owns its admission slot until it returns

Decision date: 2026-08-24. Closes **R-P9-2**
(`docs/R-P9-2_DECISION_REQUEST.txt`), raised by an external review against the
uncommitted D1 work in `components/floo_noc_model`.

### Decision

A routed transaction owns its admission slot from the moment it passes the
admission gate (`noc_interconnect.cpp:2135`) until it has:

1. received the AXI response,
2. copied response and status into the TLM payload,
3. finished `await_earlier_bypass()`,
4. retired its completion ticket,
5. released the admission slot,
6. returned from `b_transport()`.

```cpp
await_earlier_bypass(port, is_write, ticket);
retire_ticket(port, is_write, ticket);
slot.release();          // no wait after this point
```

Step 4 was `mark_completed(port, is_write)` when this decision was written, and
the rename is not cosmetic. That entry point did not know *which* ticket had
finished, so it could not tell in-order retirement from the out-of-order
retirement the unwinding paths perform — see `TPU_V3_PHASE9_AUDIT.md` §4k
finding 1. The ownership rule D26 states is unchanged; only the call that
discharges step 4 is now identified.

`slot_available` is notified with `SC_ZERO_TIME`, so the next caller is
admitted in the following delta — after this one has left `b_transport`.

**Option B — leaving the release where it was and adding a `held` counter to
`outstanding_transactions()` — was rejected.** It makes the published number
honest and leaves the bound unenforced: the next caller is still admitted,
parked routed responses can accumulate without limit behind one slow bypass,
and `wrapper_idle()` can still report idle. An implementation that also gated
admission on `network_outstanding + held < max_outstanding_per_port` would be
this decision with two counters rather than one, not option B.

### The refactor this requires, which is not optional

Moving the decrement alone is wrong. `network_idle()` consults
`outstanding_by_port`, and it is what the clock gate reads
(`network_thread()`); holding the slot through an ordering wait would keep
`step_once()` clocking an **empty** mesh for the whole hold, inflating
`clock_gate_transitions()`, `mesh_quiescent_wrapper_busy_cycles()` and any
router utilisation derived from counted cycles. Measured by control RP92-b:
**100 mesh cycles in a 100 ns window** whose only remaining work was one
caller's ordering wait.

The two predicates are therefore separated by what they answer:

| | Question | Consults admission slots |
| --- | --- | --- |
| `network_idle()` | does the mesh or its adapters still hold physical work? | **no** |
| `wrapper_idle()` | does the wrapper still owe a caller its return? | **yes**, with `bypass_in_flight` |

```text
network transaction completed
    -> in_flight and the network queues drain
    -> the mesh may clock-gate

logical routed call completed
    -> ordering wait served
    -> admission slot released
    -> the wrapper may report idle
```

Network counters, latency and the completion observer stay where they were, at
the instant the final AXI response lands. Network latency is not dragged past
an ordering wait it did not cause.

`fast_transport()` needed no change: it already held its slot for the whole
call, which is this decision's semantics.

### Gate

`test_noc_interconnect_local_bypass`, in the window where a routed call has its
response and is parked behind a slow bypass:

```text
outstanding=1  peak=1  remote_accesses=1  mesh_quiescent=1  wrapper_idle=0  cycles 24->24
```

`remote_accesses == 1` is the load-bearing one: it says the second routed call
was never injected, rather than merely that a counter reads differently.

| Control | Registered name | Mutation | Result |
| --- | --- | --- | --- |
| RP92-a | `admission-slot-released-before-ordering-wait` | release the slot before the ordering wait rather than after | `outstanding=0`, `remote_accesses=2` — the second call was admitted and injected |
| RP92-b | `network-idle-consults-admission-slots` | let `network_idle()` consult admission slots again | mesh advanced 100 cycles inside the ordering-wait window |

Both live in `run_negative_controls.sh` rather than in prose. A control that an
audit announces and the registry does not carry is evidence nobody can re-run,
and this component — unlike TPU_V3 — has a harness that can carry it.

Two controls because the change has two halves, and each catches the half the
other cannot.

### What the bench could not be asked

An earlier version of this scenario counted "callers inside `b_transport`" and
read 2 both before and after the fix, because a caller blocked in the admission
gate is inside `b_transport` too. That number cannot distinguish an enforced
bound from an unenforced one and is not used.

### Regression evidence

The mutation suite was re-run after the change: **57 detected, 0 missed**
(2026-08-24) — the 55 the registry already carried, plus the two controls this
decision added to it. For a change to idle and clock-gating semantics that is the
question that matters — not that the suite passes, but that it can still fail.

The **12 RTL cross-checks were also re-run and pass** against FlooNoC at the
pinned `v0.8.4-10-g9a6972a` — route-select, both stream-fifo depths, three
wormhole-arbiter route counts, both router out-fifo depths, norob-ordering,
four chimney checks, mesh (1872 node-cycles) and axi-sizing. `12/12 passed`.

Worth separating, because an earlier draft of this record did not: the RTL
**cross-checks** compare model traces against the frozen RTL and are
mechanical; the RTL-level **mutation controls** are the manual ones, because
several legitimately pass where the frozen RTL is redundant. Only the latter
needs a person to judge.

### Sign-off status

This changes idle and clock-gating semantics in a component whose previous v1.4
baseline carried 12 RTL cross-checks and 55 mutation controls. The current D1 +
D26 tree passes all three regression layers: 42 component tests, 57 mutation
controls and 12 RTL cross-checks.

Technical review on 2026-08-24 reproduced that evidence and concluded that
**D1+D26 meet the conditions for approval**, recommending a new v1.5 baseline.
That closes the engineering review gate and does not block continued Phase 9
work. The component owner selected and approved baseline v1.5 in the same
review session. The reviewed dirty tree is bound by the 133-file source
manifest and the raw 42/42, 57/57 and 12/12 transcripts under
`components/floo_noc_model/docs/signoff/v1.5/`. D1+D26 are therefore
**signed v1.5** for that exact snapshot; later source changes require the
affected evidence to be rerun.

**That has since happened.** R-P9-3 (`TPU_V3_PHASE9_AUDIT.md` §4h) changed four
files after the signature — `src/noc_interconnect.cpp`,
`include/floo_noc_model/noc_interconnect.h`,
`tests/test_noc_interconnect_local_bypass.cpp` and
`rtl_crosscheck/run_negative_controls.sh` — so `sha256sum -c` against the
v1.5 manifest now reports four mismatches. **v1.5 remains valid for the
snapshot it names and no longer describes the working tree.** The re-sign is
deliberately deferred so it can be taken once for a batch of component changes
rather than once per change; until it is taken, quote v1.5 for the manifest,
not for the tree.

## D25. A remote read into a chip aperture is shaped, not restricted

Decision date: 2026-08-22. Closes **R-P9-1**, raised in
`TPU_V3_PHASE9_AUDIT.md` §4b as a named risk the Phase 9 composition task owed
an explicit answer.

### Decision

**Keep the whole chip aperture registered `target_kind::mmio`**, as
`ADDRESS_MAP.md` §7 already specifies. **Shape outbound reads at the chip NoC
endpoint** so that every chunk is a shape `noc_interconnect` will not widen:

```text
naturally-aligned narrow prefix     (≤ 3 chunks: 1 + 2 + 4 covers any lane gap)
→ bus-aligned full-width bulk        (address and length both multiples of 8)
→ naturally-aligned narrow suffix    (≤ 3 chunks)
```

Applied to a transfer only when all three hold: it is a **read**, its region is
**memory-like**, and its destination is **inside a chip aperture**. Writes,
remote MMIO and reads into `GLOBAL_RAM` or `GLOBAL_BOOT_ROM` are untouched.

No FlooNoC configuration change, no address-map change, no change to the
endpoint's socket structure. The v1.4 sign-off carries.

### Why not simply publish the restriction

That was this record's first proposal and it is wrong, for a reason that only
appears when the refusal condition is read completely:

```cpp
if (command == tlm::TLM_READ_COMMAND && shape.size_log2 == 3
    && (address % bus_bytes != 0 || length % bus_bytes != 0)
    && ...kind != target_kind::memory)          // noc_interconnect.cpp:2084
```

`length % bus_bytes != 0` is a **separate clause**. A 65-byte read at a
perfectly aligned address is refused too. So the restriction is not "align your
addresses", it is "align your addresses **and** your lengths" — and plan §16
Phase 4 froze the DMA contract at exactly the shapes that violates:

> Local→external and external→local copies at lengths **1, 2, 3, 7, 8, 15, 16,
> 63, 64, 65** ... **Odd addresses**, ... external lane offsets

Publishing the restriction would therefore have narrowed a contract that is
already gated and already signed off, without a decision saying so. That is the
argument that settles this; the cost of the shaping is secondary to it.

### Why not §7's escape hatch

`ADDRESS_MAP.md` §7 offers one — map core SRAM as separate `memory`
sub-regions — and it remains available. It is not taken now because it is
strictly more expensive for the same outcome:

* the endpoint's inbound socket is `simple_target_socket`, which is `N = 1`
  (`tlm_utils/simple_target_socket.h:50`), so a second region cannot bind to it
  and the component's socket structure has to change. `simple_target_socket_
  tagged` does **not** help — the tagged variant is also `N = 1` (`:580`), which
  is a trap worth recording because "tagged" is what one reaches for;
* `noc_b_transport()`'s rebase stops being one subtraction against the aperture
  base, because the interconnect subtracts the **matched region's** base
  (`noc_interconnect.cpp:1189`);
* and the address map itself changes, which is a wider blast radius than a
  chunking rule inside one component.

It stays the escape hatch. The condition to take it is a Phase 11 measurement
showing the narrow prefix/suffix transactions cost something that matters.

### What it costs, stated rather than discovered

At most **six extra transactions** per misaligned remote SRAM read — three at
each end — and a correspondingly higher `outbound_chunks()`. Nothing else:
bytes moved, first-failing-chunk semantics, committed-byte accounting, byte
enables and the reset generation rule are all unchanged, and the chunk counter
was already documented as this component's own split rather than anything the
caller asked for.

`chip_endpoint_config::chip_apertures_are_mmio` carries the assumption this
rests on, so the one thing the endpoint believes about how the platform
registered its targets is stated where it can be changed rather than implied.

### Gate

`tpu_v3_endpoint_on_real_noc`, and it is deliberately against a **real**
`noc_interconnect` rather than the stub the component's own gate uses: whether
the shaping is *sufficient* is a question only the thing that enforces the rule
can answer. The bench registers the remote chip's aperture `mmio`, exactly as
§7 does — declaring it `memory` would make every check pass for the wrong
reason.

Measured: **120 cases** — offsets 0..7 across lengths 1, 2, 3, 7, 8, 15, 16, 63,
64, 65, 127, 128, 2047, 2048, 2049 — **0 refused, 0 with wrong data**, with the
bytes compared against a pattern so that reassembly is checked and not just
acceptance.

Two controls, both verified:

| Control | Mutation | Result |
| --- | --- | --- |
| D25-a | revert to the old `limit - lane` chunking for reads | **102 of 120** reads refused by the interconnect |
| D25-b | shape remote MMIO as well, instead of forwarding it whole | the remote target is entered for a read the interconnect should have refused before injection |

D25-a is the number that matters: 85% of the offset/length matrix was refused
before this decision, which is why "publish the restriction" was never a small
ask.

### Recorded limitation

The gate does not yet drive an inter-chip **DMA** — remote SRAM to local SRAM
at an odd source address and length. That needs `tpu_chip` composed with the
mesh, which is the next Phase 9 task, and it is listed there rather than
claimed here.

## D24. Endpoint outstanding and latency counters are split by quantity

Decision date: 2026-08-22. Closes the item
`TPU_V3_PHASE9_NOC_REBASELINE.md` §9.3 left explicitly undecided, which plan
§11.10 requires and which that section refused to let anyone implement before
it was recorded.

### Decision

**Latency is measured at the endpoint. Outstanding is not.**

* the chip NoC endpoint publishes **transfer** latency — total, maximum and the
  number of transfers sampled, per direction;
* the endpoint publishes **no outstanding counter**. That quantity belongs to
  `noc_interconnect`, which already bounds and tracks it per upstream port
  (`MaxTxns = 32`, D23 freeze 4);
* the two latency figures are **different quantities with different units of
  work**, and neither may be presented as the other, compared with the other,
  or summed with it.

### Why latency belongs here — and why the outstanding half was wrong

They looked like one question and are two, which is why §9.3 asked for a
decision instead of an implementation.

**Latency: only the endpoint can measure it.** A transfer larger than the
frame limit is one thing the chip asked for and several transactions to the
interconnect. `noc_interconnect::last_latency_cycles(port)` therefore answers
"how long did one transaction take", and nothing downstream can answer "how
long did the chip wait for the transfer it issued". Deferring would not avoid a
duplicate; it would lose a quantity nobody else holds. That half stands.

**Outstanding: the reasoning below was wrong, and is retracted.**

> Count what would be in flight at an endpoint in the Revision 1 composition:
> `neo_external_bridge` arbitrates a core's two named outbound initiators onto
> one external socket (Phase 8), and `chip_local_fabric` allows one transaction
> per initiator. So an outbound outstanding count at the endpoint is `<= 1` by
> construction.

That covers **one core**. Two cores are two initiators on the chip fabric, and
in `annotated` mode the fabric charges port occupancy to the caller's delay
rather than blocking — so while one core is suspended inside the NoC the other
enters the endpoint. `TPU_V3_PHASE8_AUDIT.md` §5 had already recorded exactly
that ("two initiators can be inside one downstream target at once if that
target waits"), and this record contradicted it.

Measured on the single-chip composition, 2026-08-24:

| NoC backend | peak outbound transfers inside the endpoint |
| --- | --- |
| `detailed` | **2** |
| `fast` | 1 |

So it is a real 0..2 transfer-level quantity, not a constant. The endpoint now
publishes `outbound_in_flight()` / `peak_outbound_in_flight()` and the inbound
pair, and `test_chip_on_mesh` asserts the backend-dependent value in each mode
— `>= 2` detailed, `== 1` fast. The fast figure is not a weaker check: a peak
above 1 there means something suspended where it should have annotated, which
is the loss of temporal decoupling `downstream_spends_delay` exists to prevent.

**Still true, and the reason the two counts are not interchangeable:** a
chunked transfer is one of these and several of `noc_interconnect`'s per-port
transactions. Neither may be quoted as the other.

### What this forbids

* comparing an endpoint latency total with an interconnect latency figure, or
  deriving one from the other. One counts transfers, the other transactions,
  and the ratio between them is the chunking this endpoint performs;
* ~~adding an endpoint outstanding counter later without reopening this
  decision~~ — **this prohibition was discharged, not deleted.** It said the
  constant stops being a constant if the composition changes, and required a
  recorded decision rather than a quiet addition. The composition measured 2,
  the decision is recorded above, and `outbound_in_flight()` /
  `peak_outbound_in_flight()` and the inbound pair exist. The clause did its
  job: it is why the counter arrived with a measurement and a retraction
  attached instead of appearing unannounced.

  What replaces it is narrower and still binding: **the in-flight figure is
  per transfer.** It may not be compared with, derived from or summed with
  `noc_interconnect`'s per-port outstanding count, which is per transaction,
  for the same reason the two latency figures may not be — one chunked
  transfer is one of these and several of those;
* quoting either figure without naming the local-fabric and chip-fabric timing
  modes that produced it. D16's rule is unchanged and applies to any number
  taken from this component.

### What is measured, precisely

Logical time, not wall-clock and not `sc_time_stamp()` alone:

```text
latency = (sc_time_stamp() + delay) at return
        - (sc_time_stamp() + delay) at entry
```

`sc_time_stamp()` alone would read **zero** for every transfer on a loosely
timed path, because an annotating target adds to `delay` and consumes no
simulated time — so the counter would be silently empty in exactly the
configuration a full-system run uses. The sum is the same quantity D16 and the
Phase 8 arbiters already treat as a caller's logical time, and it is
mode-independent: absorbing an incoming delay moves time from `delay` into
`sc_time_stamp()` and leaves the sum unchanged.

Only transfers the endpoint **forwarded** are sampled. A transfer refused at
the endpoint — local containment, a multi-region span, a payload-rule
violation — returns almost immediately and describes nothing about the mesh;
including it would drag the mean toward zero in proportion to how many
integration defects were present. A transfer abandoned by a reset is not
sampled either, under the generation rule that already governs every other
counter here.

## D23. The Phase 9 NoC transport stays the shared single-AXI network

Decision date: 2026-08-20.

Plan §16 requires this decision to be recorded here, with its RTL and FlooGen
evidence, before any Phase 9 implementation begins: "No implementation choice
may be inferred from the architecture diagram alone." The full evidence is
`TPU_V3_PHASE9_NOC_REBASELINE.md`; this is the decision and the reasons that
carry it.

### Decision

**Keep the shared single-AXI FlooNoC network exactly as frozen.** No new signed
FlooNoC configuration is created, so the v1.4 block-level sign-off of
`components/floo_noc_model` carries into Phase 9 unchanged.

The other four freezes the plan requires:

* **traffic class is a total function of the address**, taken from
  `address_map::region_kind` — `mmio` is control, `memory` is data — in three
  ordered cases so that it is defined for **error traffic** too, as the plan
  requires: a region containing the whole transfer decides; otherwise the region
  containing the first byte decides, which settles a straddle; otherwise, for an
  unmapped address or an uninstantiated chip, the class is **control**, because
  such an access returns a status and no payload and so cannot be bulk data.
  Those last two cases are refused before injection on every path TPU_V3 has —
  `noc_interconnect` decodes before it injects and answers them locally with
  `TLM_ADDRESS_ERROR_RESPONSE` — so there the class is for attribution rather
  than routing. The one error that *does* traverse the network is a target that
  reached and refused, which comes back as **`SLVERR`**; `DECERR` does not round
  trip in this model, and a Phase 9 error test that waited for one would wait
  forever.

  It is **endpoint-local metadata**: the frozen `flit_header` has no class field,
  so a response takes its request's class from the endpoint's own record. The
  record keeping is **per channel** — a write queue matched to B and a read queue
  matched to R — because `MaxUniqueIds = 1` gives FIFO order *within* each
  channel and B and R complete independently; one shared head-of-request record
  would misattribute a read's class to a write whenever both are in flight. The
  network therefore cannot prioritise by class, which is the same fact as the
  accepted head-of-line blocking below. Source-based and opcode-based
  classification are both rejected, because the DMA is not the only bulk mover
  and read-versus-write says nothing about the class;
* **widths**: 64-bit, at most 256 beats, a 2048-byte beat frame, no width
  conversion, and an oversized payload refused rather than split. Chunking is
  the chip endpoint's responsibility and owes the five points of plan §9.3;
* **protocol**: XY routing, wormhole arbitration with rotating priority,
  in-order responses **within each response channel** under `MaxUniqueIds = 1` —
  reads FIFO among reads and writes FIFO among writes, with no total order
  between them — `MaxTxns = 32` per port, ready/valid back-pressure, separate
  `req` and `rsp` physical meshes;
* **reset of in-flight traffic** is frozen as a rule and is a Phase 9 task: a
  chip reset abandons that chip's queued and in-flight work **at its endpoint**
  and reports it as an error, does not reset the shared mesh, and leaves a
  transaction already inside a downstream `b_transport()` holding its port until
  it unwinds.

### Why not virtual channels

They are **deprecated at the pinned revision**. `floo_vc_router.sv` and
`floo_nw_vc_chimney.sv` sit under `hw/deprecated/` and are compiled only by the
separate `floo_deprecated_hw` Bender target. This is not a cost judgement: a VC
cannot be added at `9a6972a` without leaving the frozen revision.

Precisely, because the term does appear in live RTL: at this revision the only
non-deprecated virtual-channel use is `NumWideVirtChannels` in
`floo_nw_router.sv:91`, a read/write decoupling on the **wide** channel, with
the narrow channel instantiated at `NumVirtChannels: 1`. There is therefore no
VC option independent of the transport choice — VCs are available only to a
design that has already taken the narrow-wide network.

### Why not narrow-wide

It is live and real: `floo_nw_router.sv`, `floo_nw_chimney.sv`,
`floo_nw_join.sv`, and `floogen/examples/nw_mesh_xy.yml` configures a 64-bit
narrow plus a **512-bit** wide protocol. It is the correct answer to a
bandwidth problem.

There is no stated bandwidth problem. Neither this document nor the plan states
a throughput, bandwidth or NoC-latency target anywhere; Phase 11 specifies
metrics to measure and no threshold to meet. Taking narrow-wide would also mean
modelling three further RTL modules that `noc_interconnect` names as explicitly
not modelled, and re-running block-level sign-off — the plan is explicit that
"a new VC or narrow/wide network is a new signed configuration; the v0 evidence
cannot simply be inherited".

### What this costs, stated rather than discovered

One channel per mesh plus wormhole arbitration means **head-of-line blocking is
accepted**: a long data burst can hold an output while a control access waits
behind it. Instruction fetch is data-class by the address rule and shares that
channel. Both are direct consequences of this decision and are recorded so that
a Phase 11 contention measurement reads as a known property.

### The condition that reopens it

A **stated** bandwidth requirement, or a measured workload in which 64-bit NoC
time dominates. The answer then is **narrow-wide, not a virtual channel**, on
the evidence above — and it is a new signed configuration with its own
cross-check campaign, not a parameter change.

### Unblocked, and still blocked

D1's owner-aware local bypass was closed on 2026-08-20.
`noc_interconnect::add_target()` takes an optional `local_owner`; an access from
that port reaches a co-located target without a flit being created, and every
wrong mapping is refused before traffic. The frozen FlooNoC configuration is
unchanged — the 12 RTL cross-checks still pass — and the mutation-control suite
went from 51 to 55 controls, still with zero misses. Evidence in
`TPU_V3_PHASE9_NOC_REBASELINE.md` §7.

What remains blocked is nothing in D1; the next Phase 9 tasks are attaching a
chip to the mesh and implementing the reset-of-in-flight rule this decision
froze.

`TPU_V3_PHASE0_AUDIT.md` §5.1 scheduled D1 as a Phase 7 prerequisite. Phase 7
was one chip plus global memory and Phase 8 was two cores inside one chip;
neither needs it. The Phase 9 scheduling in the README and plan §16 is the
correct one, and the audit's earlier note is superseded on the same terms as
P0-6, P0-7 and P0-9.
