# TPU_V3 SystemC/TLM Implementation Plan

## 1. Document Purpose

This document is the authoritative implementation plan for the TPU_V3 virtual
platform in CDC-VP. It is written so that an AI coding agent or a human engineer
can continue the work without reconstructing architectural intent from chat
history.

The plan covers the complete path from repository scaffolding to a portable,
packaged SoC executable under `CDC-VP/out/`.

This is an engineering plan, not a claim that the current implementation is
complete. Every phase has explicit deliverables and acceptance gates. A later
phase must not be declared complete until its gate passes.

### 1.1 Intended repository locations

```text
/home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/
├── components/TPU_V3/            # Reusable TPU component models
├── cpu_models/riscv_vp_plusplus/ # RV32GCV runtime backend (scalar + RVV)
├── platforms/TPU_V3_SoC/         # SoC platform composition and executable
├── fw/TPU_V3_SoC/                # Bare-metal firmware and drivers
├── build-tpu-v3/                 # Out-of-source build directory
└── out/tpu_v3_soc/               # Portable packaged binary
```

The exact capitalization of existing requested directories is preserved in
this plan. CMake target names and C++ namespaces use lower-case/snake-case
conventions.

## 2. AI Execution Protocol

An AI agent implementing this plan must follow these rules:

1. Read this file completely before changing source code.
2. Read repository-level instructions and relevant component documentation.
3. Inspect the working tree before editing and preserve unrelated user changes.
4. Work on one phase gate at a time. Do not skip a failing prerequisite.
5. Separate facts, frozen decisions, assumptions, and proposed values.
6. Do not silently turn a proposed value into an architectural requirement.
7. Add a test with every material behavior change.
8. Run the narrowest relevant tests first, then the phase-level regression.
9. Never describe an analytical or functional model as cycle accurate.
10. Never describe model-to-model agreement as RTL equivalence.
11. Do not copy third-party source into CDC-VP without recording revision,
    license, provenance, and redistribution constraints.
12. Do not use an external process as a hidden runtime dependency of the
    portable package unless the package explicitly contains and launches it.
13. Prefer static linking for CPU/ISS backends used by the portable platform.
14. All long-running accelerator operations must be asynchronous from MMIO
    `b_transport`; a register access must not block until an MXU job completes.
15. Keep this document's status table and decision log current when a phase or
    architectural decision changes.

## 3. Project Mission

Build a parameterized SystemC/TLM SoC platform whose backbone is a 2D mesh NoC.
Each NoC node represents one TPU chip. Each TPU chip contains two TPU cores.

The final platform must:

- execute RV32 scalar and RISC-V Vector 1.0 firmware;
- expose two 128x128 MXUs per TPU core;
- provide one Shared Vector Memory per TPU core;
- connect chips, global memory, and system resources through a 2D mesh NoC;
- support functional/fast full-system simulation;
- support selected detailed timing experiments without overclaiming accuracy;
- build through the CDC-VP top-level CMake project;
- produce a portable package under `out/tpu_v3_soc/`;
- run without depending on the CDC-VP source tree at runtime.

## 4. Frozen Architectural Decisions

The following decisions are approved and must be treated as requirements.

### 4.1 Chip and core hierarchy

```text
TPU_V3 SoC
└── Parameterized 2D Mesh NoC
    └── One TPU chip per mesh node
        ├── One chip-local interconnect / NoC endpoint
        ├── TPU Core 0
        └── TPU Core 1
```

Each TPU core contains:

```text
TPU Core
├── One RISC-V scalar execution context
├── One RISC-V Vector execution unit
├── One Shared Vector Memory
├── MXU 0: 128 x 128 processing elements
└── MXU 1: 128 x 128 processing elements
```

### 4.2 RISC-V Vector architecture

```text
Vector specification : RISC-V Vector Extension 1.0
Base width           : XLEN = 32
Vector register size : VLEN = 512 bits
Maximum element size : ELEN = 64 bits
Supported SEW        : 8, 16, 32, and 64 bits
Target ISA           : RV32GCV + Zvl512b
Expected ISA string  : rv32gcv_zvl512b
Expected ABI         : ilp32d
vlenb CSR value      : 64
```

Scalar execution and vector execution belong to one architectural RISC-V hart.
They are not two independent CPUs and are not connected through a software-
visible MMIO accelerator interface. RISC-V VP++ is the selected implementation
of both portions of this hart; Spike is only its external differential oracle.

RVV 1.0 already provides reduction and permutation instruction groups. For
Revision 1, those TRP-like functions are exercised through the VP++ RVV
backend rather than a separately timed Transpose/Reduction/Permute component.
This is a functional abstraction, not a claim that Google TPU v3 implements
TRP inside an RVV hart. A dedicated TRP block remains an explicit architecture
decision if separate data paths, commands, counters or timing are required.

### 4.3 MXU dimensions

Each MXU has a logical physical shape of exactly 128 rows by 128 columns.
There are two independent MXUs in each TPU core.

The architectural dimension must remain 128x128 even if an analytical backend
is used. A 32x32 Sauria instance must not be presented as an exact 128x128 MXU.

### 4.4 NoC attachment policy

The two TPU cores in one chip connect to a chip-local fabric. The chip-local
fabric aggregates traffic into one NoC endpoint per chip.

Do not expose the scalar CPU, RVV unit, both MXUs, and SVM as unrelated NoC
managers. Local traffic must remain local when possible.

## 5. Explicitly Unfrozen Decisions

The following values are not yet approved and must remain configurable or be
resolved by a documented decision:

- mesh width and height;
- total number of chips;
- global HBM/RAM capacity;
- Shared Vector Memory capacity per core;
- SVM port count and arbitration policy;
- chip-local and core-local address strides;
- MXU production data types: INT8, FP16, BF16, FP32, or a supported subset;
- MXU pipeline depth, clock rate, and detailed latency formula;
- CPU, VPU, MXU, SVM, and NoC clock ratios;
- cache presence and cache coherence policy;
- DMA ownership: per core, per chip, or platform-level;
- whether a dedicated TRP component is required beyond the provisional RVV
  functional abstraction;
- final workload set and image input format.

Earlier ideas such as a fixed 4x4 mesh, exactly 32 TPU cores, or a mandatory
four-image workload are not architectural requirements in this revision. They
may be introduced later as named configurations or workloads after approval.

> **Partly resolved on 2026-08-08** by `TPU_V3_DECISION_RECORD.md`. Still
> configurable, but now with an approved reference value rather than a
> placeholder: SVM capacity per core (16 MiB reference, D6), MXU production
> arithmetic (BF16 × BF16 → FP32, D6), global RAM capacity (256 MiB bring-up
> default, D6). Also settled: DMA ownership and cache policy remain open, but a
> 32-core system is explicitly **outside Revision 1** and requires the
> coordinated change listed in D2. See §23 for the current state of each.

## 6. Terminology

Use these names consistently in code and documentation:

| Term | Meaning |
| --- | --- |
| `TPU_V3 SoC` | Complete SystemC/TLM platform executable |
| `TPU chip` | One NoC node containing exactly two TPU cores |
| `TPU core` | One RV32GCV hart, one SVM, and two MXUs |
| `Scalar core` | Scalar execution portion of the RISC-V VP++ RV32GCV hart |
| `VPU` | RVV 1.0 vector execution portion of the same RISC-V VP++ hart |
| `TRP functionality` | Revision 1 reduction/permutation functionality abstracted through RVV; a separate hardware block is not yet frozen |
| `MXU` | 128x128 matrix multiply unit |
| `SVM` | Shared Vector Memory local to one TPU core |
| `Chip local fabric` | Arbitration and decode between two cores and the NoC endpoint |
| `NoC endpoint` | Bidirectional interface between one TPU chip and one mesh router |
| `Fast model` | Functionally correct, approximately timed or untimed model |
| `Detailed model` | More detailed timing model with explicitly stated evidence |

Avoid using the unqualified word `core` when it could mean RISC-V hart, TPU
core, Sauria NPU core, or MXU processing element.

## 7. Target Architecture

```text
                                  Host / Loader
                                       |
                                  Global RAM/HBM
                                       |
       +-------------------------------+-------------------------------+
       |                         2D Mesh NoC                           |
       +-------------------------------+-------------------------------+
                    |                                     |
             Router / Node (x,y)                    Router / Node (...)
                    |                                     |
          +---------+----------+                +---------+----------+
          | TPU Chip           |                | TPU Chip           |
          |                    |                |                    |
          | Chip Local Fabric  |                | Chip Local Fabric  |
          |   |            |   |                |   |            |   |
          | Core 0       Core 1|                | Core 0       Core 1|
          +--------------------+                +--------------------+
```

One TPU core is composed as follows:

```text
                   RISC-V VP++ RV32GCV hart
                 +-----------+------------+
                 | Scalar     | RVV 1.0    |
                 | execution  | VLEN=512   |
                 +-----------+------------+
                              |
                      Core Local Fabric
                   +----------+----------+
                   |          |          |
                  SVM       MXU 0      MXU 1
                            128x128     128x128
```

The RVV unit uses normal RISC-V load/store instructions to access memory. MXUs
are controlled asynchronously through a documented register/descriptor
interface and move tensor data through local TLM master interfaces or an
explicit DMA path.

## 8. Existing Assets and Their Roles

### 8.1 FlooNoC SystemC/TLM model

```text
/home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/floo_noc_model
```

Use it for:

- 2D mesh routing;
- TLM address mapping and endpoint placement;
- detailed and fast timing backends;
- contention and router metrics in detailed mode;
- packaging through `cdc::components::noc_interconnect`.

Do not fork or duplicate this model under `components/TPU_V3`.

### 8.2 RISC-V CPU models and selected RVV runtime

```text
/home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/cpu_models/riscv_vp
```

The current Bremen-based wrapper is a single-core RV32 ISS with one combined
instruction/data TLM socket. It is useful as:

- a reference for the `cdc::cpu::cpu_base` contract;
- a reference for ELF loading, interrupt injection, PC/instret access, and
  SystemC thread integration;
- a non-vector platform baseline.

It does not implement RISC-V V and must not be advertised as RV32GCV.

The selected TPU_V3 runtime is the successor project:

```text
Upstream: https://github.com/ics-jku/riscv-vp-plusplus
Local integration target: cpu_models/riscv_vp_plusplus
License: MIT
Translated paper: /home/duyptt_HW/Desktop/TPU_V3/docs/RISC-V_VP++_ban_dich_tieng_Viet.docx.md
```

RISC-V VP++ integrates RVV 1.0 into its RV32 and RV64 ISSs. One VP++ RV32 ISS
therefore provides both the Scalar Unit and the RVV VPU of a TPU core. Do not
compose the Bremen scalar ISS with a separately advancing Spike vector ISS,
and do not instantiate the complete upstream VP++ platform per core. CDC-VP
continues to own memory, SVM, interrupt controllers, devices and the NoC; the
new backend is a thin wrapper around the required VP++ ISS/library pieces.

Spike remains pinned as an independent standalone golden/differential model.
It is not the primary TPU_V3 runtime and must not be a hidden executable
dependency of the portable package.

### 8.3 Sauria model

External source example:

```text
/home/duyptt_HW/Documents/work/fx1/hw/tlm/MP1_V1.1/v4.2_model
```

Existing CDC-VP integration:

```text
/home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/npu_tlm
```

Use these as:

- a reference implementation of systolic-array behavior;
- a reference for feeder, SRAM, controller, and partial-sum behavior;
- a source for a detailed MXU adapter only after the 128x128 mapping is
  specified and verified;
- a reference for asynchronous accelerator control and DMA integration.

The currently integrated instance is 32x32 and is an NPU top, not a bare
128x128 MXU. It cannot be reused unchanged as the final detailed MXU.

### 8.4 CDC-VP packaging infrastructure

```text
/home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/cmake/modules/CdcPortable.cmake
/home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/platforms/noc_soc
```

Use `noc_soc` as the reference for:

- linking reusable components into a platform executable;
- `$ORIGIN` runtime search paths;
- copying SystemC shared libraries;
- packaging configs and licenses;
- packaging regression in an isolated build tree.

## 9. Known Integration Constraints

These constraints are facts from current code and must be addressed explicitly.

### 9.1 Current NoC manager limit

`noc_interconnect` accepts at most eight upstream initiators because the frozen
manager ID is three bits. With one aggregated NoC manager per chip, the current
wrapper supports no more than eight TPU chips without modification.

A larger mesh requires a documented FlooNoC configuration change, wider ID
handling, updated ordering logic, and RTL/model cross-checks. Do not remove the
guard without updating the protocol evidence.

### 9.2 NoLoopback placement

The current wrapper rejects a target on the same mesh node as an initiator
because the frozen NoC uses `NoLoopback=1`. A TPU chip is naturally
bidirectional: it initiates remote accesses and exposes local targets to remote
chips.

The implementation must provide one of these verified solutions:

1. local address bypass before injection plus safe remote ejection to a target
   at the same node; or
2. a revised endpoint/NoC configuration with supported loopback behavior.

Local accesses must never be injected into the mesh and sent back to the same
node.

> **Phase 0 correction.** This is stronger than "the wrapper rejects a target
> on the same mesh node as an initiator". `reject_self_node_targets()` refuses
> **any** mapped target on a node hosting **any** upstream port — the check is
> not per initiator/target pair — and it fires at `end_of_elaboration`, so no
> configuration can work around it by ordering. Consequence: one chip plus
> global memory (Phase 7) is reachable today; **multi-chip traffic (Phase 8) is
> blocked** until this is resolved. Options, feasibility and the recommendation
> are in `TPU_V3_PHASE0_AUDIT.md` §5.1.

### 9.3 NoC burst limit

The current NoC wrapper has an 8-byte bus and a maximum 256-beat AXI burst,
which produces an aligned maximum frame of 2048 bytes. Larger tensor transfers
must be split into legal transactions by DMA/endpoint code.

Chunking must define:

- alignment;
- ordering;
- partial failure behavior;
- final completion semantics;
- metrics attribution.

### 9.4 Blocking target behavior

In detailed NoC mode, a downstream target that calls `wait()` inside
`b_transport` can stall the process that advances the entire mesh.

TPU targets must therefore:

- annotate short access latency in the TLM delay;
- return immediately from control-register writes;
- launch long jobs in worker threads;
- signal completion using status registers and interrupts;
- never wait for MXU completion inside MMIO `b_transport`.

### 9.5 No multicast/collectives

The existing model does not implement multicast or NoC collectives. Tensor or
weight broadcast must initially be represented as explicit unicast/DMA traffic.
If multicast becomes a requirement, it is a separate NoC feature phase with
RTL/source-of-truth justification.

### 9.6 RVV tool availability

At the time this plan was written, RISC-V VP++, `spike`, RISC-V QEMU, and
RISC-V cross-GCC were not visible in the active shell `PATH`. The
implementation must fetch pinned source revisions and use the pinned toolchain
before RVV acceptance tests can run.

> **Phase 0 result.** Still true of `PATH`, but a suitable compiler is
> installed: xPack `riscv-none-elf` GCC **15.2.0-1** at
> `/opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1/bin/`, already used by the
> existing `fw/` Makefiles. It accepts `-march=rv32gcv_zvl512b -mabi=ilp32d`
> and expands it with `v`, `zve64d` and `zvl512b`. One item stays open for
> Phase 2: there is **no vector multilib**, so a libc link for a vector build
> is unproven — a bare-metal `-nostdlib` link is unaffected. RISC-V VP++ has
> now been selected as the runtime backend, but its immutable candidate SHA,
> host build, embeddable ISS boundary, and `VLEN=512`/`ELEN=64` support remain
> to be proved. Spike stays pinned at
> `16c0b60119f65a648643cf5d41e4e38e871f0bad` as the differential oracle. See
> decision D3 and `TPU_V3_PHASE0_AUDIT.md` §3–§4.

## 10. Repository Structure

```text
CDC-VP/
├── components/
│   └── TPU_V3/
│       ├── CMakeLists.txt
│       ├── docs/
│       │   ├── TPU_V3_IMPLEMENTATION_PLAN.md
│       │   ├── ARCHITECTURE.md
│       │   ├── ADDRESS_MAP.md
│       │   ├── INTERFACE_CONTRACT.md
│       │   ├── TIMING_MODEL.md
│       │   ├── RVV_MODEL.md
│       │   ├── MXU_MODEL.md
│       │   └── VERIFICATION_PLAN.md
│       ├── common/
│       │   ├── CMakeLists.txt
│       │   ├── include/tpu_v3/architecture_config.h
│       │   ├── include/tpu_v3/address_map.h
│       │   ├── include/tpu_v3/tlm_extensions.h
│       │   └── include/tpu_v3/types.h
│       ├── shared_vector_memory/
│       │   ├── CMakeLists.txt
│       │   ├── include/tpu_v3/svm/shared_vector_memory.h
│       │   ├── include/tpu_v3/svm/svm_config.h
│       │   ├── src/shared_vector_memory.cpp
│       │   └── tests/test_shared_vector_memory.cpp
│       ├── mxu/
│       │   ├── CMakeLists.txt
│       │   ├── include/tpu_v3/mxu/mxu_if.h
│       │   ├── include/tpu_v3/mxu/mxu_config.h
│       │   ├── include/tpu_v3/mxu/mxu_fast.h
│       │   ├── include/tpu_v3/mxu/mxu_sauria_adapter.h
│       │   ├── src/mxu_fast.cpp
│       │   ├── src/mxu_sauria_adapter.cpp
│       │   └── tests/
│       ├── tpu_core/
│       │   ├── CMakeLists.txt
│       │   ├── include/tpu_v3/core/tpu_core.h
│       │   ├── include/tpu_v3/core/core_local_fabric.h
│       │   ├── include/tpu_v3/core/core_registers.h
│       │   ├── src/tpu_core.cpp
│       │   ├── src/core_local_fabric.cpp
│       │   └── tests/
│       ├── tpu_chip/
│       │   ├── CMakeLists.txt
│       │   ├── include/tpu_v3/chip/tpu_chip.h
│       │   ├── include/tpu_v3/chip/chip_local_fabric.h
│       │   ├── src/tpu_chip.cpp
│       │   ├── src/chip_local_fabric.cpp
│       │   └── tests/
│       └── noc_endpoint/
│           ├── CMakeLists.txt
│           ├── include/tpu_v3/noc/chip_noc_endpoint.h
│           ├── include/tpu_v3/noc/noc_address_decoder.h
│           ├── src/chip_noc_endpoint.cpp
│           └── tests/
├── cpu_models/
│   └── riscv_vp_plusplus/
│       ├── CMakeLists.txt
│       ├── include/riscv_vp_plusplus_cpu.h
│       ├── src/riscv_vp_plusplus_cpu.cpp
│       ├── src/vp_plusplus_tlm_adapter.cpp
│       ├── src/vp_plusplus_irq_adapter.cpp
│       └── tests/
├── platforms/
│   └── TPU_V3_SoC/
│       ├── CMakeLists.txt
│       ├── configs/
│       │   ├── single_core.yaml
│       │   ├── single_chip.yaml
│       │   ├── mesh_2x2.yaml
│       │   └── mesh_4x4.yaml
│       ├── src/main.cpp
│       ├── src/tpu_v3_soc_top.h
│       ├── src/tpu_v3_soc_top.cpp
│       └── tests/
├── fw/
│   └── TPU_V3_SoC/
│       ├── CMakeLists.txt
│       ├── common/
│       ├── drivers/
│       │   ├── mxu/
│       │   ├── svm/
│       │   └── platform/
│       ├── linker/
│       └── tests/
└── third_party/
    ├── riscv-vp-plusplus/  # Pinned runtime source; exact name follows setup script
    └── riscv-isa-sim/       # Pinned Spike differential reference
```

Do not create `build/` directories inside component source directories.

> **This is the target layout, not the current one.** Each subdirectory is
> created by the phase that first needs it, and `components/TPU_V3/CMakeLists.txt`
> lists which phase that is. Empty placeholder libraries were deliberately not
> created: an exported target that links and does nothing is worse than a
> missing one, because a platform can depend on it and appear to work.
> `common/include/tpu_v3/tlm_extensions.h` likewise arrives with the first
> component that needs a payload extension (Phase 3), not before.
>
> Present after Phase 1: `docs/`, `common/` (`types.h`, `address_map.h`,
> `architecture_config.h`, sources and tests).

## 11. Component Specifications

### 11.1 Architecture configuration

Provide one strongly typed C++ configuration object. Do not read YAML directly
inside low-level components.

Minimum fields:

```cpp
struct rvv_config {
    unsigned xlen = 32;
    unsigned vlen = 512;
    unsigned elen = 64;
    std::string version = "1.0";
};

struct mxu_config {
    unsigned rows = 128;
    unsigned columns = 128;
    unsigned count_per_core = 2;
    mxu_backend backend = mxu_backend::fast;
};

struct tpu_core_config {
    rvv_config rvv;
    mxu_config mxu;
    std::uint64_t svm_size_bytes;
};

struct tpu_chip_config {
    unsigned cores = 2;
    std::array<tpu_core_config, 2> core;
};

struct tpu_soc_config {
    unsigned mesh_x;
    unsigned mesh_y;
    tpu_chip_config chip;
};
```

Construction must reject invalid frozen values such as `cores != 2`,
`mxu.rows != 128`, `mxu.columns != 128`, or incompatible RVV parameters.

### 11.2 RV32GCV CPU backend

#### Selected runtime and reference roles

Use RISC-V VP++ as the runtime backend:

```text
https://github.com/ics-jku/riscv-vp-plusplus
```

RISC-V VP++ is the extended successor of the Bremen RISC-V VP and integrates
RISC-V V 1.0 into its RV32/RV64 ISSs. The TPU core uses one RV32GCV VP++ ISS
for scalar and vector execution. This is the model substitute for the Google
TPU Scalar Unit plus VPU; it is not a claim that Google TPU v3 implements the
RISC-V ISA.

The wrapper must embed or statically link only the required VP++ ISS/library
pieces. It must not instantiate VP++ RAM, bus, CLINT, PLIC, GUI, Qt/VNC stack,
or a complete upstream platform per TPU core. The packaged SoC must not
require a separately installed VP++ executable.

Use pinned Spike commit `16c0b60119f65a648643cf5d41e4e38e871f0bad`
as the independent standalone differential oracle. Spike is not linked into
the runtime platform unless a later decision explicitly changes that role.

#### Required `cpu_base` behavior

The backend must provide:

- instruction and data TLM initiator sockets, unified if required by backend;
- ELF loading through the bound platform memory path or a documented debug
  transport path;
- unique hart ID;
- reset and reset PC;
- machine software, timer, and external interrupt injection;
- PC and instruction-retired accessors;
- deterministic stop/error reporting;
- RVV configuration validation at construction.

#### Memory integration rule

Instruction fetches, scalar loads/stores, vector loads/stores, and MMIO must
ultimately traverse SystemC/TLM. VP++ internal/platform RAM must not silently
bypass SVM, global memory, or the NoC.

If the first integration uses a mirrored memory for bring-up, it must be a
temporary phase explicitly disabled before the NoC integration gate.

#### Required RVV architectural checks

- `misa.V == 1`;
- `mstatus.VS` reset and dirty transitions;
- `vlenb == 64`;
- 32 vector registers of 512 bits each;
- `vsetvli`, `vsetivli`, and `vsetvl` behavior;
- supported SEW values 8, 16, 32, 64;
- supported LMUL values including required fractional LMUL;
- vector load/store, integer, fixed-point, FP, mask, reduction, and permutation
  groups required by full V;
- precise illegal instruction and memory traps;
- RV32 restriction on 64-bit vector index EEW;
- `vstart` restart state after a **mid-vector trap** (decision record D10).
  Interrupt-driven restart is *not* checkable: VP++ has no interrupt check in
  its per-element vector loop, so a vector instruction is atomic with respect
  to interrupts and never leaves a partially executed one to resume. Recorded
  as unobservable in this backend rather than dropped.

#### Timing

RISC-V VP++ supplies functional instruction semantics, not target TPU cycle
timing.
The initial backend may use:

- a configurable scalar instruction cost;
- a configurable vector instruction base cost;
- a lane/element-dependent vector cost;
- TLM memory delay from the connected fabric.

Such timing must be labeled approximately timed. A future timing backend must
not change architectural results.

#### Pre-integration audit and pinning

Before wrapper code starts, Phase 2 must select and record an immutable VP++
candidate SHA, then prove all of the following on the target host:

- MIT license/provenance and transitive build dependencies are understood;
- GCC 11.5 and SystemC 2.3.4 can build the required RV32+RVV ISS pieces;
- GUI, Qt, VNC, Linux platform and unused peripheral dependencies can be
  excluded from the runtime library and portable package;
- `VLEN=512`, `ELEN=64`, `vlenb=64`, RVV 1.0 and `rv32gcv_zvl512b` are
  actually supported rather than inferred from the upstream feature list;
- more than one ISS instance can run without mutable global architectural or
  transaction state;
- the wrapper can control hart ID, reset PC, interrupts, stop state, PC and
  instruction-retired counters;
- all CPU memory traffic can be redirected to CDC-VP TLM sockets.

If any frozen parameter is unsupported, Phase 2 stops and records the mismatch;
it must not silently lower VLEN/ELEN or relabel another configuration as the
TPU_V3 reference.

### 11.3 Shared Vector Memory

The SVM is local to one TPU core and shared by:

- scalar/RVV memory traffic;
- MXU 0;
- MXU 1;
- optional DMA or chip endpoint traffic.

Initial required properties:

- byte-addressed little-endian storage;
- configurable capacity, with the full architectural window decoding (D6);
- sparse page-backed host storage, never an eager allocation (D6);
- deterministic initialization;
- any payload from 1 to 64 bytes, verified with a synthetic initiator (D7);
- byte-enable support;
- range and overflow checks;
- multiple initiator arbitration;
- annotated access latency;
- counters per requester;
- debug transport for ELF/test initialization;
- reset policy documented explicitly.

#### Access granularity and counter naming (D7)

There is no vector-instruction boundary at the SVM interface. RISC-V VP++
decomposes vector accesses per active element before the CDC-VP wrapper sees
them, and masked, strided, indexed and fault-only-first accesses could not be
one contiguous transaction under any backend.

The SVM therefore:

- accepts any 1..64-byte payload, but never requires a 64-byte one;
- must **not** infer, group or reassemble vector instruction boundaries;
- names its counters `tlm_request_count`, `transferred_bytes`, `error_count`
  and `arbitration_event_count`, counted per TLM request;
- must **not** expose `vector_instruction_count`, `vector_register_count` or
  anything described as a hardware bus transaction count;
- records `VP++ element-wise granularity` alongside any timing figure derived
  from these counts, which must never be offered as evidence of equivalence
  with TPU hardware.

That one request usually carries one active element is a property of the
current backend, not an SVM invariant.

Initial coherence policy:

- no hidden caches;
- one coherent backing store;
- program-visible ordering follows blocking TLM completion;
- MXU completion interrupt/status acts as the synchronization boundary;
- any future cache requires a separate coherence decision and test plan.

The SVM must not call `wait()` inside `b_transport` when used behind detailed
NoC paths. Long internal operations, if any, use worker threads.

### 11.4 MXU interface

Define a backend-independent `mxu_if` before implementing Sauria adaptation.

Required logical parameters:

```text
Rows             : 128
Columns          : 128
Instances/core   : 2
Operation class  : GEMM first; convolution lowering may be added later
```

Required control contract:

- command/start;
- busy/done/error status;
- interrupt enable and level-sensitive completion IRQ;
- matrix/tensor base addresses;
- M, N, K dimensions;
- input/output strides;
- data type selection from the supported production set;
- optional accumulate/bias/activation controls only after specification;
- performance counters;
- W1C or explicit completion acknowledgment semantics.

MMIO start must enqueue work and return. Completion occurs asynchronously.

#### Fast MXU backend

Implement first. It must:

- produce numerically correct reference results;
- tile arbitrary supported M/N/K around a logical 128x128 array;
- expose a deterministic approximate latency model;
- handle partial edge tiles;
- report bytes read/written, operations, utilization estimate, and busy cycles;
- support two concurrent instances per core without shared static state.

#### Sauria/detailed backend

Implement only after the fast backend and interface are stable.

Before coding, audit:

- whether the model is safely parameterizable to 128x128;
- hard-coded 32-bit row/column masks;
- SRAM capacity and address generation assumptions;
- signal-level host interface behavior;
- internal `wait()` behavior;
- data types;
- reset and clock ownership;
- license and redistribution conditions.

Allowed outcomes:

1. a true parameterized 128x128 Sauria-derived backend with verification;
2. a tiled 32x32 approximation clearly labeled as such;
3. retain the fast 128x128 model as the full-system backend and use detailed
   Sauria only for limited microarchitectural experiments.

Do not instantiate a full detailed 128x128 PE hierarchy across every MXU in a
large mesh without measuring elaboration time, host memory, and runtime first.

### 11.5 Core-local fabric

Responsibilities:

- decode CPU/RVV accesses to SVM, MXU 0, MXU 1, and chip/global space;
- arbitrate local initiators;
- route MXU master traffic to SVM or chip/global memory;
- prevent local traffic from entering the global NoC unnecessarily;
- preserve response ownership under concurrent requests;
- expose per-route counters;
- enforce address alignment and target access policy.

The address map must be centralized in `address_map.h`. No component may carry
an independent copy of base addresses.

### 11.6 TPU core

One `tpu_core` instance owns:

- one RISC-V VP++ RV32GCV CPU backend;
- one SVM;
- two MXUs;
- one core-local fabric;
- core-local interrupt aggregation;
- core ID and globally unique hart ID;
- reset and optional clock-domain adapters.

Hart ID mapping should default to:

```text
hart_id = chip_linear_id * 2 + core_id
```

The mapping must be tested and visible to firmware through `mhartid`.

### 11.7 TPU chip

One `tpu_chip` instance owns exactly two `tpu_core` instances plus one
chip-local fabric and one NoC endpoint.

Responsibilities:

- create unique names and IDs;
- map both cores into one chip address aperture;
- arbitrate outbound traffic from both cores and accelerator masters;
- route inbound remote accesses to the correct local target;
- provide one aggregate NoC manager identity per chip;
- expose chip-level metrics;
- implement reset sequencing.

### 11.8 NoC endpoint

The endpoint must be bidirectional:

- outbound: local chip request to a remote/global address;
- inbound: remote chip or host request to a local chip aperture.

It must:

- attach one chip to one mesh coordinate;
- maintain requester ownership;
- split transfers that exceed NoC burst limits;
- bypass same-chip traffic;
- enforce deterministic response and partial-error semantics;
- support concurrent traffic from both local TPU cores;
- publish outstanding, latency, bytes, and error counters.

NoC protocol changes require dedicated negative controls and cross-checks. Do
not make TPU-specific changes directly in routing primitives if the behavior
belongs in an endpoint adapter.

### 11.9 Platform top

`platforms/TPU_V3_SoC` owns system composition, not reusable IP behavior.

It is responsible for:

- parsing command-line options and configuration;
- instantiating the mesh and TPU chips;
- placing chips and global targets;
- global RAM/HBM model;
- boot ROM if required;
- platform reset and clocks;
- firmware selection;
- simulation time limits and clean stop;
- metrics/report output;
- host-side workload loading;
- packaging configs, firmware, and license records.

## 12. Address Map Design Rules

The numerical map must be frozen in `ADDRESS_MAP.md` during the architecture
contract phase. Until then, use symbolic regions only:

```text
GLOBAL_BOOT_ROM
GLOBAL_CONTROL
GLOBAL_RAM_OR_HBM
CHIP_APERTURE(chip_id)
  CORE_APERTURE(core_id)
    CORE_CONTROL
    SVM
    MXU0_CONTROL
    MXU1_CONTROL
    CORE_COUNTERS
  CHIP_CONTROL
  CHIP_COUNTERS
```

Rules:

1. All RV32-visible physical addresses must fit below 4 GiB.
2. Regions must be power-of-two aligned where practical.
3. SVM capacity must fit within its core aperture.
4. No two chip/core apertures may overlap.
5. Address calculations must check 64-bit overflow even though RV32 is used.
6. MMIO accesses must define supported widths and alignment.
7. Memory-like targets and MMIO targets must be distinguished for widened
   reads.
8. Remote and local views of one resource must be unambiguous.
9. Firmware headers must be generated from or checked against the C++ map.
10. A unit test must enumerate every region and prove non-overlap.

## 13. TLM Contract

All TPU_V3 components must use one documented contract.

### 13.1 Generic payload rules

- command must be read or write;
- address is byte-addressed;
- data pointer must be non-null for nonzero length;
- streaming width smaller than data length is rejected unless explicitly
  implemented;
- byte-enable handling is target-specific and documented;
- response status is always set before return;
- debug transport must not advance simulated time;
- DMI is disabled initially unless a phase explicitly adds and invalidates it.

### 13.2 Timing rules

- short target latency is added to `delay`;
- `b_transport` must not wait for long accelerator work;
- workers may call `wait()` in their own SC_THREAD;
- detailed NoC targets must not suspend the mesh driver;
- fast and detailed backends must preserve functional results and error codes;
- timing mode is selected at construction and reported in metrics.

### 13.3 Concurrency rules

- no mutable global/static transaction state;
- each request has an identifiable owner;
- completion queues preserve the required ordering contract;
- reset during an active job has documented abort behavior;
- slot/accounting cleanup occurs on success, error, reset, and exception;
- tests exercise simultaneous MXU 0/MXU 1 and dual-core requests.

## 14. Accuracy and Performance Strategy

The platform needs multiple fidelity levels.

### 14.1 Full-system fast mode

Use for firmware, software integration, multi-chip workloads, and long runs.

- RISC-V VP++ functional RV32GCV execution;
- fast/analytical 128x128 MXU;
- approximately timed SVM/local fabric;
- FlooNoC fast timing when contention is not the study target;
- deterministic results and metrics.

### 14.2 NoC detailed mode

Use for routing, arbitration, link back-pressure, and contention experiments.

- detailed FlooNoC backend;
- nonblocking/annotating targets;
- small or controlled workloads;
- no claim that CPU/MXU timing is cycle accurate unless separately calibrated.

### 14.3 MXU detailed mode

Use for selected single-MXU or single-core experiments.

- Sauria-derived behavior where verified;
- explicit scope and array mapping;
- measured host runtime and memory before scaling;
- separate results from full-system fast-mode results.

### 14.4 Prohibited accuracy claims

Do not claim:

- Google TPUv3 RTL equivalence;
- cycle-accurate RVV or Google TPU timing from RISC-V VP++;
- exact 128x128 timing from a 32x32 tiled approximation;
- full-system cycle accuracy merely because detailed NoC is enabled;
- multicast behavior when it is implemented as repeated unicast.

## 15. CMake and Packaging Plan

### 15.1 Top-level options

Add options without changing existing default platform behavior:

```cmake
option(CDC_BUILD_TPU_V3_SOC
    "Build the TPU_V3 NoC SoC platform" OFF)

option(CDC_BUILD_TPU_V3_TESTS
    "Build TPU_V3 component tests" ON)

set(TPU_V3_MXU_BACKEND "fast" CACHE STRING
    "TPU_V3 MXU backend: fast or sauria")
```

Add a pinned RISC-V VP++ source path/cache variable with a clear failure if
missing. Keep the optional Spike reference path separate so a production
runtime build cannot accidentally acquire a Spike dependency.

### 15.2 Component targets

Expected exported targets:

```text
cdc::components::tpu_v3_common
cdc::components::tpu_v3_svm
cdc::components::tpu_v3_mxu
cdc::components::tpu_v3_core
cdc::components::tpu_v3_chip
cdc::components::tpu_v3_noc_endpoint
cdc::cpu::riscv_vp_plusplus
```

Every reusable component target must:

- declare public and private include paths correctly;
- link dependencies by target, not raw library filename;
- install public headers;
- join the existing CDC component export where appropriate;
- avoid absolute runtime paths to source trees.

### 15.3 Platform targets

```text
tpu_v3_soc
tpu_v3_soc_package
```

Use:

```cmake
cdc_make_portable(tpu_v3_soc)
cdc_package_platform(tpu_v3_soc)
```

Extend package commands to include:

- platform configs;
- selected firmware ELF/images;
- CDC-VP license and NOTICE;
- FlooNoC license/provenance;
- RISC-V VP++ license, pinned revision and runtime role;
- Spike license, pinned revision and golden-reference-only role when reference
  tests are distributed;
- Sauria license/provenance if the Sauria backend is included;
- a build manifest recording compiler, configuration, and revisions.

### 15.4 Reference build flow

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
export SAURIA_NPU_ROOT=/home/duyptt_HW/Documents/work/fx1/hw/tlm/MP1_V1.1/v4.2_model

cmake -S . -B build-tpu-v3 \
    -DCDC_BUILD_TPU_V3_SOC=ON \
    -DCDC_BUILD_TPU_V3_TESTS=ON \
    -DCDC_ENABLE_SAURIA_NPU_V4=ON \
    -DSAURIA_NPU_ROOT="$SAURIA_NPU_ROOT" \
    -DTPU_V3_MXU_BACKEND=fast

cmake --build build-tpu-v3 \
    --target tpu_v3_soc \
    --clean-first \
    -j"$(nproc)"

cmake --build build-tpu-v3 \
    --target tpu_v3_soc_package \
    -j"$(nproc)"
```

Expected package:

```text
out/tpu_v3_soc/
├── tpu_v3_soc
├── libsystemc.so*
├── configs/
├── firmware/
├── licenses/
└── BUILD_MANIFEST.json
```

## 16. Implementation Phases and Gates

### Phase 0: Source audit and decision baseline

#### Tasks

- Record current CDC-VP revision and clean/dirty status.
- Record FlooNoC frozen revision and existing test status.
- Audit `riscv_vp` CPU wrapper contract.
- Audit Sauria v4.2 source, configuration, and license.
- Record the initial Spike differential-reference revision. The later discovery
  of RISC-V VP++ supersedes Spike as the planned runtime backend (D3).
- Locate or provision a compiler supporting `rv32gcv_zvl512b` and `ilp32d`.
- Create `ARCHITECTURE.md`, `INTERFACE_CONTRACT.md`, and `ADDRESS_MAP.md`.
- Resolve initial SVM capacity and MXU data types, or mark them as config
  values with explicit temporary defaults.

#### Gate

- Dependencies and licenses are documented.
- Frozen/unfrozen decisions match this plan.
- No source has been copied without provenance.
- Architecture and address-map reviews contain no unresolved contradiction
  blocking scaffolding.

### Phase 1: Repository and portable-platform skeleton

#### Tasks

- Create component, CPU backend, platform, and firmware directories.
- Add top-level CMake options.
- Add placeholder exported component targets.
- Add `tpu_v3_soc` executable that parses a config and prints the architecture.
- Add `tpu_v3_soc_package` using CDC portable helpers.
- Copy configs and license skeleton into the package.
- Add a packaging regression that runs outside the source directory.

#### Gate

Commands:

```bash
cmake --build build-tpu-v3 --target tpu_v3_soc
cmake --build build-tpu-v3 --target tpu_v3_soc_package
./out/tpu_v3_soc/tpu_v3_soc \
    --config ./out/tpu_v3_soc/configs/single_chip.yaml
```

must succeed. Runtime dependency inspection must show no source-tree RPATH.

### Phase 2: RV32GCV/RVV functional backend

#### Tasks

- Audit RISC-V VP++ source, transitive dependencies, license and build options;
  select an immutable candidate SHA before integration code starts.
- Build the required VP++ RV32+RVV ISS pieces on the 64-bit host with GCC 11.5
  and SystemC 2.3.4, excluding GUI/Qt/VNC and complete upstream platforms.
- Build a minimal RV32GCV bare-metal ELF.
- Prove VP++ configuration: RVV 1.0, VLEN 512, ELEN 64 and `vlenb=64`.
- Build the pinned Spike oracle and run the same architectural corpus on it.
- Implement `cdc::cpu::riscv_vp_plusplus` following `cpu_base`.
- Implement TLM-backed fetch/load/store/MMIO.
- Implement unique hart ID and interrupts.
- Prove at least two VP++ instances execute independently without mutable
  global ISS state.
- Add vector architectural state/CSR tests.
- Add integer, FP, mask, reduction, permutation, and memory smoke tests.
- Add negative controls for unsupported configuration and illegal instruction.

#### Minimum smoke program

Must exercise:

```text
vsetvli for e8/e16/e32/e64
vle*/vse*
vadd
vmul
one widening operation
one mask operation
one reduction
one permutation
one FP operation
vlenb read == 64
```

#### Gate

- RV32GCV ELF executes through the SystemC wrapper.
- Fetch and data traffic are observed on TLM sockets.
- VP++ runtime results match pinned Spike standalone/reference behavior for
  the agreed differential corpus.
- No private backend RAM bypasses the final memory path.
- Backend can be statically linked into a portable test executable.
- The portable executable has no runtime dependency on a VP++ executable,
  Spike executable, Qt, VNC, source tree or build tree.
- **Cycle baseline is correct (audit F11).** Phase 2 is not complete while
  `mcycle` is wrong, regardless of how many functional tests pass:
  - the approved upstream backport `b710fa7b` is applied to the pinned base and
    verified by CMake, not applied by it;
  - the first workload TLM request carries no startup time offset;
  - `mcycle` starts at a plausible reset baseline and increases monotonically,
    sampled by firmware through the CSR;
  - the gate runs at least two harts, in Debug and in Release, under a watchdog
    small enough that a recurrence fails rather than being absorbed;
  - reverting the backport makes both the configure step and the test fail.
- Any third-party patch is recorded: patch file under
  `cpu_models/riscv_vp_plusplus/patches/`, content hash checked, applied only by
  the acquisition script, and base + backport + hash + effective source written
  into `BUILD_MANIFEST.json`.

### Phase 3: Shared Vector Memory

#### Tasks

- Implement deterministic sparse 4 KiB page-backed storage for SVM and global
  RAM; do not eagerly allocate their logical capacities.
- Keep decoded window, instantiated capacity and allocated host backing as
  three separate quantities.
- Make unallocated pages read as zero, allocate only pages touched by writes,
  and release all pages on reset.
- Implement bounds, byte enables, cross-page transactions, and reset policy.
- Implement multiple tagged/requester ports.
- Implement approximate latency without blocking `b_transport`.
- Accept any 1..64-byte payload, exercised by a synthetic initiator (D7).
- Implement debug initialization.
- Add logical-capacity, current/peak backing-byte and page-count counters, plus
  concurrent requester tests.
- Name the access counters `tlm_request_count`, `transferred_bytes`,
  `error_count` and `arbitration_event_count`, counted per TLM request (D7).

#### Gate

- Every payload size from 1 to 64 bytes passes, driven by a synthetic
  initiator. The 64-byte case is **not** required to come from VP++: it
  decomposes vector accesses per active element, so a VP++ trace containing no
  64-byte payload is expected, not a defect (D7).
- No SVM counter is named `vector_instruction_count`,
  `vector_register_count` or a hardware bus transaction count, and nothing in
  SVM infers, groups or reassembles a vector instruction boundary (D7).
- Any timing figure derived from these counters carries
  `VP++ element-wise granularity`.
- Out-of-range, overflow, bad byte enable, and reset tests pass.
- Unallocated reads return zero; a first write allocates only the touched
  page(s); boundary-crossing writes preserve byte enables; reset releases the
  backing pages.
- Concurrent CPU/MXU-style traffic preserves data and owner attribution.
- Same results are produced with counters enabled and disabled.
- `mesh_4x4` (1.25 GiB logical storage) elaborates without eagerly committing
  1.25 GiB of host memory.

### Phase 4: MXU fast backend

#### Tasks

- Freeze initial register/descriptor contract.
- Implement backend-independent `mxu_if`.
- Implement functional 128x128 fast model.
- Implement asynchronous worker and level IRQ.
- Implement partial edge tiles and arbitrary supported M/N/K.
- Add data-type-specific golden reference.
- Instantiate and run two independent MXUs.

#### Gate

- GEMM results match a simple trusted C++ golden model.
- Boundary dimensions around 127/128/129 pass.
- Two MXUs run concurrently with independent state and completion.
- MMIO start returns immediately.
- Reset during idle and active operation follows the documented contract.
- Timing and utilization counters are deterministic.

### Phase 5: Single TPU core

#### Tasks

- Implement core-local address decoder and arbiter.
- Instantiate RV32GCV, SVM, MXU 0, and MXU 1.
- Wire interrupts and core reset.
- Generate unique core/hart IDs.
- Build firmware drivers for MXU/SVM.
- Run an RVV-preprocess -> MXU -> RVV-postprocess pipeline.

#### Gate

- One firmware ELF boots on the core.
- RVV reads/writes SVM through TLM.
- Firmware launches both MXUs and handles completion IRQs.
- Final output matches golden data.
- Unmapped and misaligned accesses fail predictably.
- No global NoC is required for local-only workload completion.

### Phase 6: Dual-core TPU chip

#### Tasks

- Instantiate exactly two TPU cores.
- Implement chip-local fabric and address apertures.
- Implement `mhartid = chip_id * 2 + core_id`.
- Add outbound arbitration and inbound target decode.
- Add chip reset and counters.
- Test simultaneous firmware on both harts.

#### Gate

- Both cores boot and report unique IDs.
- Both cores can run independent workloads concurrently.
- One core cannot corrupt the other core's private SVM accidentally.
- Authorized remote/debug access maps to the intended core.
- Outbound request ownership and responses remain correct under concurrency.

### Phase 7: Single-chip NoC integration

#### Tasks

- Connect one TPU chip endpoint to FlooNoC.
- Add global RAM/HBM target.
- Implement local-address bypass.
- Implement legal burst chunking.
- Run firmware from a defined boot/global memory arrangement.
- Verify fast and detailed NoC compatibility.

#### Gate

- Chip can read/write global RAM through the NoC.
- Local SVM/MMIO traffic does not enter the NoC.
- Transfers larger than 2048 bytes are chunked correctly.
- No target blocks the detailed mesh driver.
- Reset and error paths leave the mesh and endpoint idle.

### Phase 8: Multi-chip mesh and NoC scalability

#### Tasks

- Bring up a 2x2 mesh first.
- Assign one coordinate and one aggregated manager identity per chip.
- Test inter-chip reads/writes and global memory traffic.
- Add deterministic contention tests.
- Measure current 8-manager limit behavior.
- If more than eight chips are required, widen manager identity/configuration
  with FlooNoC RTL/model evidence and negative controls.
- Resolve bidirectional same-node target placement safely.

#### Gate

- 2x2 mesh completes concurrent inter-chip traffic without deadlock.
- Source/destination data and response ownership are correct.
- Detailed metrics satisfy transaction/flit conservation.
- Fast mode matches detailed no-contention functional effects and stays within
  its documented latency tolerance.
- Any manager-ID extension is cross-checked and guarded by configuration tests.

### Phase 9: Sauria/detailed MXU feasibility

This phase may proceed after the fast MXU contract is stable. It must not block
functional full-system progress.

#### Tasks

- Audit parameterization to 128x128.
- Decide true 128x128, tiled approximation, or limited detailed model.
- Implement adapter behind the unchanged `mxu_if`.
- Cross-check small matrices and boundary cases against fast/golden model.
- Measure elaboration and runtime at one MXU, one core, and one chip.
- Define the maximum supported detailed configuration.

#### Gate

- Functional results match the accepted golden model.
- Timing/accuracy scope is documented honestly.
- Detailed backend can be selected by CMake/config without changing firmware.
- Full-system configurations refuse impractical detailed combinations rather
  than exhausting host resources unexpectedly.

### Phase 10: Firmware and workload integration

#### Tasks

- Add startup code and per-hart stacks.
- Add trap/interrupt handling.
- Add RVV compile flags and intrinsic/assembly tests.
- Add MXU and SVM drivers.
- Add global memory allocator or fixed buffer contract.
- Add host-side tensor/image loader.
- Add named workloads rather than baking one workload into architecture.
- Add an optional four-image workload only when its format and scheduling are
  approved.

#### Gate

- Firmware build is reproducible.
- Multi-hart startup is deterministic.
- Workloads produce golden outputs.
- Input data and outputs traverse documented memory paths.
- Timeout/error reporting identifies chip, core, MXU, PC, and status.

### Phase 11: Metrics, performance, and stress

#### Required metrics

CPU/RVV:

- instructions retired;
- scalar/vector instruction counts;
- vector element operations by SEW;
- vector loads/stores and bytes;
- approximate execution cycles.

MXU:

- jobs;
- MAC operations;
- active/busy cycles;
- utilization estimate;
- bytes read/written;
- stalls by cause;
- error/reset counts.

SVM/local fabric:

- accesses and bytes per requester;
- arbitration stalls;
- conflicts;
- latency totals/maxima.

NoC:

- transactions/flits/bytes;
- latency per source/destination;
- route/link utilization;
- back-pressure/lock cycles;
- outstanding peak;
- errors and chunking counts.

#### Gate

- Metrics do not alter functional results.
- Conservation checks pass.
- Stress tests cover concurrency, reset, errors, odd lengths, and boundary
  addresses.
- Performance reports always name timing backends and configuration.

### Phase 12: Portable package and signoff

#### Tasks

- Build a clean Release tree.
- Package executable, SystemC runtime, configs, firmware, and licenses.
- Record source revisions and build configuration.
- Run from a temporary directory without source-tree access.
- Inspect runtime dependencies and RPATH.
- Run packaged smoke, RVV, MXU, single-chip, and selected mesh tests.
- Archive test manifest and checksums.

#### Gate

```text
out/tpu_v3_soc/tpu_v3_soc
```

must run on a compatible clean AlmaLinux 9.x environment without requiring the
CDC-VP source or build directories. The package must contain all applicable
license/provenance records and a reproducible build manifest.

## 17. Verification Matrix

| Level | Subject | Required evidence |
| --- | --- | --- |
| Unit | RVV config/CSR | VLEN/ELEN/SEW/LMUL and illegal configs |
| Unit | RVV instructions | Integer, FP, mask, reduction, permutation, load/store |
| Unit | SVM | Widths, byte enables, boundaries, concurrency, reset |
| Unit | MXU | GEMM golden, dimensions, data types, async IRQ, reset |
| Unit | Core fabric | Decode, arbitration, local bypass, errors |
| Unit | Chip fabric | Two-core ownership, inbound/outbound routing |
| Unit | NoC endpoint | placement, chunking, completion, partial errors |
| Integration | Single core | RVV -> SVM -> dual MXU -> RVV |
| Integration | Single chip | two harts, two SVMs, four MXUs |
| Integration | NoC | global RAM and remote chip traffic |
| System | Firmware | boot, IRQ, drivers, workload completion |
| Stress | Concurrency | simultaneous harts/MXUs/NoC traffic |
| Negative | Fault injection | decode, ordering, reset, chunking, IRQ defects detected |
| Package | Distribution | clean build and source-independent execution |

Every positive regression for a critical protocol should have at least one
negative control proving the test detects the intended defect.

## 18. Risk Register

### R1. Full RVV integration complexity

Risk: extracting the RV32+RVV ISS boundary from RISC-V VP++, redirecting all
memory semantics to CDC-VP TLM, and keeping multiple instances isolated may be
nontrivial. The upstream project is a complete VP, not a pre-qualified CDC-VP
CPU library.

Mitigation:

- use a pinned VP++ runtime and pinned Spike differential oracle;
- build a narrow TLM memory bridge first;
- require standalone-vs-wrapper golden comparison;
- do not write a custom RVV implementation or combine two independently
  advancing ISSs unless a separate architecture decision justifies it.

### R2. Sauria 32x32 versus required 128x128

Risk: existing Sauria cannot represent the required MXU unchanged.

Mitigation:

- make the 128x128 fast backend the first functional source;
- keep Sauria behind a stable adapter;
- audit hard-coded widths before parameterization;
- label tiled approximations accurately.

### R3. Simulation scalability

Risk: many chips times two cores times two detailed 128x128 MXUs can make
elaboration and runtime impractical.

Mitigation:

- full-system fast mode;
- detailed mode restricted by configuration guard;
- benchmark host memory/time before scaling;
- allow mixed fidelity only with explicit reporting.

### R4. NoC initiator and loopback constraints

Risk: current wrapper supports only eight managers and forbids same-node
manager/target placement.

Mitigation:

- one aggregated manager per chip;
- local bypass;
- 2x2 bring-up first;
- protocol/RTL cross-check before ID-width changes.

### R5. TLM blocking deadlock

Risk: a target waits inside `b_transport` and freezes detailed NoC progress.

Mitigation:

- asynchronous workers;
- annotated register latency;
- watchdog and concurrency tests;
- code review rule forbidding long waits in target `b_transport`.

### R6. Toolchain incompatibility

Risk: compiler accepts a different RVV revision or lacks RV32GCV/ILP32D.

Mitigation:

- pin compiler/toolchain version;
- compile and disassemble smoke ELF;
- check generated ISA attributes;
- run the same corpus on pinned VP++ and Spike before SystemC integration.

### R7. Licensing and redistribution

Risk: a portable binary includes Sauria, RISC-V VP++, Spike, or unwanted VP++
GUI/platform dependencies without complete license and provenance records.

Mitigation:

- license audit in Phase 0;
- static dependency manifest;
- package regression checks required license files;
- do not enable redistributable Sauria package until authorized.

### R8. Address-space exhaustion under RV32

Risk: per-chip apertures and global memory exceed 4 GiB.

Mitigation:

- freeze address map before large mesh;
- calculate maximum chips from aperture size;
- use windowing or revise XLEN only through an explicit architecture change.

## 19. Coding and Review Rules

- Namespace reusable TPU code under `cdc::components::tpu_v3`.
- Keep one public header root: `include/tpu_v3/...`.
- Avoid raw owning pointers; use RAII and `std::unique_ptr` where appropriate.
- Create all SystemC sockets/modules before elaboration completes.
- Validate configuration in constructors and fail with actionable messages.
- Avoid unnamed magic addresses and timing constants.
- Use fixed-width integer types for architectural fields.
- Check address addition/subtraction for overflow.
- Keep functional computation separate from timing accounting.
- Make backends selectable without changing firmware-visible behavior.
- Use deterministic seeds and record them in randomized tests.
- Add watchdogs to every concurrency/system test.
- Do not hide errors by converting them to zero data.
- Keep build artifacts out of source directories.
- Preserve existing CDC-VP build targets and default options.

## 20. Definition of Done

The TPU_V3 project is complete only when all of the following are true:

1. The SoC hierarchy matches the frozen specification.
2. Every chip contains exactly two TPU cores.
3. Every TPU core contains one RV32GCV hart, one SVM, and two logical 128x128
   MXUs.
4. RVV 1.0 reports XLEN 32, VLEN 512, ELEN 64, and executes the required full-V
   instruction groups.
5. RVV memory traffic uses TLM and interacts with SVM/global memory correctly.
6. Both MXUs in one core can execute concurrently and return correct results.
7. Both TPU cores in one chip can boot and run concurrently with unique IDs.
8. Multiple chips communicate through the parameterized 2D mesh without
   deadlock or response misattribution.
9. Local accesses bypass the global NoC.
10. Large tensor transfers are safely chunked to legal NoC bursts.
11. Fast and detailed modes have documented, tested accuracy boundaries.
12. Firmware, drivers, and at least one end-to-end workload pass golden checks.
13. Metrics pass conservation and noninterference tests.
14. Component, platform, firmware, stress, negative-control, and packaging
    regressions pass in a clean build.
15. `out/tpu_v3_soc/` contains a portable executable, configs, selected
    firmware, SystemC runtime, licenses, provenance, and build manifest.
16. The packaged executable runs without the source/build tree.

## 21. Immediate Next Actions

Done (2026-08-08):

1. ~~Complete Phase 0 dependency, revision, toolchain, and license audit.~~
2. ~~Create the Phase 1 directory/CMake skeleton.~~
3. ~~Add `tpu_v3_soc` and `tpu_v3_soc_package` smoke targets.~~
4. ~~Prove the portable skeleton runs from `out/tpu_v3_soc/`.~~
5. ~~Pin the initial Spike differential reference and the RV32GCV
   cross-toolchain.~~ The runtime role was later reassigned to RISC-V VP++ by
   D3; the VP++ candidate SHA is deliberately still open pending its audit.

Phase 2, done (2026-08-08) — evidence in `docs/TPU_V3_PHASE2_AUDIT.md`:

6. ~~Fetch RISC-V VP++, audit it, select an immutable candidate SHA, record
   MIT/transitive dependency provenance, do not vendor.~~ Pinned at
   `7a36fe859cae242f513ca6ad16ab8238f1e82977` (tag `2025.09`) — **not** master,
   which requires SystemC 3.0.1.
7. ~~Build only the required VP++ RV32+RVV ISS pieces with GCC 11.5 and
   SystemC 2.3.4; prove RVV 1.0, VLEN 512, ELEN 64, `vlenb == 64`,
   multi-instance isolation and redirectable memory traffic.~~ Nine translation
   units plus softfloat, zero warnings, no Qt/VNC/nlohmann/gdb-mc and no Boost
   runtime dependency.
10. ~~Add `hart_id` and `reset_pc` to `cdc::cpu::cpu_config` (D5) as static
    fields.~~ Done, with one documented refinement: the default is an explicit
    `reset_pc_unspecified` sentinel, because `0` is a legitimate reset vector
    under the TPU_V3 map and cannot also mean "unset".

Phase 2, outstanding, in order:

8. Build the freestanding RV32GCV smoke environment: `-ffreestanding
   -nostdlib -nostartfiles`, a project-owned `crt0.S`, linker script, stack
   setup, trap entry and pass/fail reporting, depending on neither libc nor
   libm (D4).
9. Build and disassemble the RV32GCV smoke ELF listed in Phase 2, and check its
   ISA attributes.
11. Execute the smoke ELF through `cdc::cpu::riscv_vp_plusplus` and *observe*
    fetch, scalar, vector and MMIO traffic on the TLM socket — routing it there
    is proven, traffic on it is not yet.
12. Build the pinned Spike oracle and run the differential corpus, which must
    include the scalar-FP and AMO cases from audit §3 that are unavailable at
    the pinned revision.
13. ~~Add the vector-trap tests.~~ Done as `rvv_vector_trap` (D10): a fault at
    element 5 of a vector load resumes from `vstart` with elements below it
    provably not re-accessed; `mstatus.VS` Off makes a vector instruction
    illegal and Initial becomes Dirty; a reserved `vtype` sets `vill` and zeroes
    `vl` without trapping. The RV32 index-EEW=64 outcome is **recorded, not
    asserted** — see audit F12; the differential run decides it and this section
    then gains a spec citation.
14. Add the audit §F5 control: two instances running FP work with different
    `frm` values interleaved. `softfloat_roundingMode`,
    `softfloat_detectTininess` and `softfloat_exceptionFlags` are process-global
    — `THREAD_LOCAL` expands to nothing — so the control must assert exception
    **flags** as well as numeric results.

Resolved 2026-08-10:

16. ~~Audit §F11, the bogus cycle baseline.~~ Closed by a controlled backport of
    upstream `b710fa7b` onto the base pin — not baseline subtraction, and not a
    SystemC 3.0.1 migration. It is a **Phase 2 closure gate**, verified in Debug
    and Release across two harts.

Two upstream fixes are deliberately *not* taken with it, each needing its own
evidence first:

17. `7a936cce` "fixed fast quantum" — a larger change to quantum accounting.
    Audit it separately **before Phase 7**.
18. `52d376d4` AMO atomicity / lost bus lock — needs a **multi-hart AMO
    contention test before Phase 6**. A single-threaded differential run against
    Spike cannot demonstrate atomicity between harts.

Optional, in parallel: ask upstream for a maintenance backport on a SystemC
2.3-compatible branch. If they publish one, move the pin to that commit, drop
the local patch, and re-run the whole Phase 2 gate.

Due before Phase 5, and not a Phase 5 discovery:

15. Decide full architectural core reset semantics (audit §F8). `reset_cpu()` is
    a restart at the reset PC plus cache reinitialisation; it clears none of the
    GPRs, FP/vector registers, CSRs, `vstart`, `instret`, pending interrupts or
    privilege level, so the hierarchical reset in `ARCHITECTURE.md` §6 is not
    yet implementable and a reset test built on it would prove nothing.

Also outstanding:

* **Phase 7 prerequisite, not a Phase 8 item:** the owner-aware local bypass in
  `noc_interconnect` (D1). It needs a cross-checked change to an RTL-signed
  component and its own negative controls — a missing or wrong owner mapping
  must fail during elaboration, and a local access must inject zero flits.

Do not begin full mesh composition or detailed Sauria scaling before the RVV
functional backend and single-core contracts pass their gates.

## 22. Status Table

Update this table when work progresses.

| Phase | Status | Evidence |
| --- | --- | --- |
| Plan document | Complete | This file |
| Decision record D1-D7 | Approved (D1-D6 2026-08-08, D7 2026-08-10) | `docs/TPU_V3_DECISION_RECORD.md` |
| Phase 0: audit/baseline | Complete (2026-08-08) | `docs/TPU_V3_PHASE0_AUDIT.md`, `docs/ARCHITECTURE.md`, `docs/ADDRESS_MAP.md`, `docs/INTERFACE_CONTRACT.md` |
| Phase 1: skeleton/package | Complete (2026-08-08), review findings closed | `out/tpu_v3_soc/` runs with `RPATH=$ORIGIN` and no source-tree path; ctest `tpu_v3_address_map`, `tpu_v3_architecture_config`, `tpu_v3_soc_cli`, `tpu_v3_soc_packaging_regression` all pass |
| Decision-record synchronization | Documentation/build contracts complete; CPU configuration and sparse backing remain Phase 2/3 code | `docs/TPU_V3_DECISION_RECORD.md` synchronization table |
| Phase 2: RV32GCV backend | **In progress** (2026-08-10) | Audit, pin, build proof and execution complete — `docs/TPU_V3_PHASE2_AUDIT.md`. VP++ pinned at `7a36fe859cae242f513ca6ad16ab8238f1e82977` (tag `2025.09`); `cdc::cpu::riscv_vp_plusplus` builds against SystemC 2.3.4; D5 `cpu_config` implemented; the freestanding RV32GCV image executes through the wrapper with 1006 observed TLM requests and all 14 RVV checks passing (`riscv_vp_plusplus_backend`, `rvv_smoke_execution`). **F11 closed** by the approved backport of upstream `b710fa7b`: first TLM request at 0 s, `mcycle` 15 → 2439 → 2848, verified in Debug and Release on two harts, and both the configure check and the runtime gate were shown to fail when the patch is reverted. D10 vector-trap gate passes (`rvv_vector_trap`). **Outstanding before Phase 2 can be declared complete:** the F5 interleaved-`frm` control, and the Spike oracle with its differential corpus — audit §9 |
| Phase 3: SVM | Not started | — |
| Phase 4: fast MXU | Not started | — |
| Phase 5: single core | Not started | — |
| Phase 6: dual-core chip | Not started | — |
| Phase 7: single-chip NoC | Not started | — |
| Phase 8: multi-chip mesh | Not started | — |
| Phase 9: detailed MXU | Not started | — |
| Phase 10: firmware/workloads | Not started | — |
| Phase 11: metrics/stress | Not started | — |
| Phase 12: packaging/signoff | Not started | — |

## 23. Decision Log

| Decision | Value | Status |
| --- | --- | --- |
| TPU cores per chip | 2 | Frozen |
| MXUs per TPU core | 2 | Frozen |
| MXU dimensions | 128x128 PE | Frozen |
| VPU ISA | RISC-V V | Frozen |
| RVV version | 1.0 | Frozen |
| XLEN | 32 | Frozen |
| VLEN | 512 bits | Frozen |
| ELEN | 64 bits | Frozen |
| NoC topology | Parameterized 2D mesh | Frozen concept, dimensions open |
| NoC attachment | One aggregated endpoint per chip | Frozen |
| RV32GCV runtime | RISC-V VP++ (`ics-jku/riscv-vp-plusplus`), MIT | Selected by D3; immutable candidate SHA and verified pin remain Phase 2 work |
| RVV differential reference | Spike `16c0b60119f65a648643cf5d41e4e38e871f0bad` (2026-08-07), BSD-3-Clause | Golden/reference-only role under D3; not the TPU runtime backend |
| Cross toolchain | xPack `riscv-none-elf` GCC 15.2.0-1, `/opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1` | Pinned (D4); Phase 2 uses a freestanding `-nostdlib` environment |
| Full-system MXU backend | Fast analytical 128x128 first | Planned; the only selectable backend, enforced at CMake configure time |
| Detailed MXU backend | Sauria-derived, experimental only | Open (D6); not selectable until it builds, runs and has a redistribution policy |
| Maximum chips | 8 chips / 16 cores | **Revision 1 backend limit** (D2), from the 3-bit NoC manager id. 32 cores = 16 chips is a separate coordinated change |
| Mesh dimensions | Configurable within 2x2, 3x3, 4x4, 4x2, 2x4 | Frozen set, choice open |
| Address map layout | Frozen (`docs/ADDRESS_MAP.md`) | Bases, strides and window sizes frozen; two capacities configurable |
| Window vs capacity | Full window always decodes; capacity is separately reported and the target refuses above it | Frozen (D6) |
| SVM capacity | **Reference 16 MiB** per core (the full window); smaller values are labelled bring-up | Frozen reference (D6, supersedes P0-6) |
| Global RAM size | Configurable, 256 MiB bring-up default, 1 GiB window | Open capacity (D6); simulated backing memory, not a model of TPU v3 HBM |
| Host-memory backing | Sparse deterministic 4 KiB pages; logical capacity is not eagerly allocated | Frozen implementation contract for Phase 3 (D6) |
| MXU arithmetic | **BF16 x BF16 with IEEE FP32 accumulation**, fixed accumulation order | Frozen reference (D6, supersedes P0-7). INT8 x INT8 -> INT32 is an optional later extension |
| Same-node initiator/target | Keep `NoLoopback=1`; owner-aware local bypass | Approved (D1). **Phase 7 prerequisite**, blocks Phase 8 |
| Hart id / reset PC | Static fields in `cdc::cpu::cpu_config` | Approved (D5, supersedes P0-9). No default no-op virtual setters |
| Vector memory access granularity | Element-wise, as VP++ issues it; no fork, no inferred coalescing; counters named for TLM requests | Approved (D7) |
| Full architectural core reset | Undecided | **Open — due before Phase 5.** Today's `reset_cpu()` is a restart plus cache reinit, not a reset; see `docs/TPU_V3_PHASE2_AUDIT.md` F8 |
| Dedicated TRP block | Revision 1 provisionally covers reduction/permutation through RVV | Open; add a separate component only after interface/timing requirements are approved |
| Four-image workload | Optional future named workload | Not frozen |

The authority for D1–D6 is `docs/TPU_V3_DECISION_RECORD.md`.
