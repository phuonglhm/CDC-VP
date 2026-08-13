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
    `b_transport`; a register access must not block until an SA, DMA or
    ImageTransform job completes.
15. Keep this document's status table and decision log current when a phase or
    architectural decision changes.

## 3. Project Mission

Build a parameterized SystemC/TLM SoC platform whose backbone is a 2D mesh NoC.
Each NoC node represents one TPU chip. Each TPU chip contains two TPU cores.

The final platform must:

- execute RV32 scalar and RISC-V Vector 1.0 firmware;
- compose each TPU/NEO core from one VP++ hart, one shared SRAM, one independent
  TPU_V3 DMA, one Sauria matrix engine, one Im2Col/Col2Im Transform engine and
  a D15 split interconnect: 32-bit AXI4-Lite control, native banked-SRAM local
  data, and AXI4 only at the bidirectional chip-NoC external boundary;
- integrate the verified 64x64 Sauria source first and preserve a gated upgrade
  path to the NPU team's 128x128 source;
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
TPU Core / NEO-CORE
├── One RISC-V VP++ RV32GCV hart (Scalar + RVV)
├── One shared core SRAM
├── One independent TPU_V3 DMA
├── One Sauria matrix engine
├── One ImageTransform engine (Im2Col + Col2Im)
├── One 32-bit AXI4-Lite control fabric
├── One native NEO Local SRAM Fabric
└── One bidirectional external AXI4/NoC bridge
```

This D15 interconnect split was finally ratified by the project owner on
2026-08-12. It is a frozen implementation requirement, not an optional backend
or illustrative proposal. The physical SRAM bank count, local data width, bank
mapping and pipeline depth remain open parameters because freezing those needs
SRAM-macro, frequency and PD evidence; that open physical tuning does not
reopen the three-plane protocol architecture.

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

RVV 1.0 reduction and permutation instructions remain part of the VP++ hart.
They are unrelated to D14's `ImageTransform` block: that accelerator performs
tensor-layout Im2Col/Col2Im operations and has its own MMIO/TLM contract.

### 4.3 Sauria matrix-engine geometry

There is exactly one Sauria matrix engine in each NEO-CORE. The implementation
is staged:

1. the current bring-up geometry is the verified v4.2 64x64 configuration;
2. the architectural destination is the NPU team's future 128x128 source.

Geometry is a construction-time reported property. A 64x64 build must not be
described as 128x128. A 128x128 request must be rejected until the new source,
adapter, golden regression and scalability gate pass.

### 4.4 NoC attachment policy

The two TPU cores in one chip connect to a chip-local fabric. The chip-local
fabric aggregates traffic into one NoC endpoint per chip.

Do not expose the hart, SA, Transform, DMA and SRAM as unrelated NoC managers.
One aggregated endpoint represents the chip to the mesh. Inside a NEO-CORE,
local data uses the native SRAM fabric and control uses AXI4-Lite; neither is
exposed as an unrelated NoC manager. Local traffic must remain local when
possible.

## 5. Explicitly Unfrozen Decisions

The following values are not yet approved and must remain configurable or be
resolved by a documented decision:

- mesh width and height;
- total number of chips;
- global HBM/RAM capacity;
- core SRAM capacity per core beyond the retained 16 MiB reference;
- physical SRAM bank count, native data width, low-order bank mapping and
  pipeline/register-slice depth (D15 freezes per-bank deterministic round-robin
  and at most one outstanding request per requester, not these PD parameters);
- chip-local and core-local address strides;
- exact datatype available in the 64x64 Sauria bring-up and the NPU team's
  128x128 delivery; D6 BF16 x BF16 -> FP32 remains the target contract;
- SA pipeline depth, clock rate, and detailed latency formula;
- CPU, SA, Transform, DMA, SRAM, and NoC clock ratios;
- cache presence and cache coherence policy;
- final DMA descriptor depth, burst policy and timing constants (ownership is
  frozen per-core and independent of Sauria by D14);
- NPU-team Transform source revision and the exact Im2Col/Col2Im layout,
  padding, stride, overlap/accumulation and datatype contract;
- final workload set and image input format.

Earlier ideas such as a fixed 4x4 mesh, exactly 32 TPU cores, or a mandatory
four-image workload are not architectural requirements in this revision. They
may be introduced later as named configurations or workloads after approval.

> **Partly resolved on 2026-08-08** by `TPU_V3_DECISION_RECORD.md`. Still
> configurable, but now with an approved reference value rather than a
> placeholder: core SRAM capacity per core (16 MiB reference, D6), SA target
> arithmetic (BF16 × BF16 → FP32, D6), global RAM capacity (256 MiB bring-up
> default, D6). D14 settles DMA ownership and the NEO-CORE composition; cache
> policy remains open. A 32-core system is explicitly **outside Revision 1**
> and requires the
> coordinated change listed in D2. See §23 for the current state of each.

## 6. Terminology

Use these names consistently in code and documentation:

| Term | Meaning |
| --- | --- |
| `TPU_V3 SoC` | Complete SystemC/TLM platform executable |
| `TPU chip` | One NoC node containing exactly two TPU cores |
| `NEO-CORE` / `TPU core` | One VP++ hart, one SRAM, one independent DMA, one Sauria SA, one ImageTransform engine, AXI4-Lite control and a native local-SRAM data fabric |
| `Scalar core` | Scalar execution portion of the RISC-V VP++ RV32GCV hart |
| `VPU` | RVV 1.0 vector execution portion of the same RISC-V VP++ hart |
| `SA` / `Sauria matrix engine` | Matrix-multiply-only accelerator; 64x64 bring-up, 128x128 target |
| `Core SRAM` | Shared local memory in one NEO-CORE; successor name for the old SVM contract |
| `ImageTransform` | NPU-team Im2Col/Col2Im block behind a TPU_V3 adapter |
| `NEO DMA` | Independent TPU_V3 DMA; never the Sauria DMA |
| `NEO control fabric` | 32-bit AXI4-Lite MMIO decoder, represented at transaction level |
| `NEO Local SRAM Fabric` | Native pipelined request/response fabric with per-bank arbitration; not AXI |
| `NEO external bridge` | Bidirectional adapter for outbound VP++/DMA and inbound remote traffic at the AXI4/chip-NoC boundary |
| `Chip local fabric` | Arbitration and decode between two cores and the NoC endpoint |
| `NoC endpoint` | Bidirectional interface between one TPU chip and one mesh router |
| `Fast model` | Functionally correct, approximately timed or untimed model |
| `Detailed model` | More detailed timing model with explicitly stated evidence |

Avoid using the unqualified word `core` when it could mean RISC-V hart,
NEO-CORE, Sauria NPU core, or SA processing element.

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
                           Scalar + RVV 1.0/512
                              | control
                    32-bit AXI4-Lite Control Fabric
                    |          |         |          |
                 core regs  NEO DMA   Sauria SA  ImageTransform
                               |          |          |
                       native local-data requesters
                               |          |          |
                        NEO Local SRAM Fabric
                                  |
                         physically banked SRAM

               VP++ fetch/global + NEO DMA external
                              ↕
                       AXI4 / Chip-NoC Bridge
```

The RVV unit uses normal RISC-V load/store instructions through the same VP++
memory path as scalar execution. Address decode sends local memory operations
to the native SRAM plane and register operations to AXI4-Lite. SA, DMA and
ImageTransform are asynchronous AXI4-Lite-controlled engines; their local bulk
data uses native ports. The independent DMA owns bulk external movement. VP++
also crosses the external boundary for boot-ROM instruction fetch and explicit
global accesses; SA and ImageTransform do not.

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
continues to own core SRAM, interrupt controllers, devices and the NoC; the
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

- the implementation source for the matrix-multiply-only Sauria adapter;
- the implementation source for Im2Col/Col2Im only where the NPU team identifies
  and verifies those functions;
- a reference for the minimum feeder/sequencer/result-collection logic required
  by matrix multiplication;
- golden/case data for source-to-adapter equivalence tests.

The v4.2 tree is an NPU top and cannot be reused unchanged. The verified
bring-up target is 64x64. The matrix adapter excludes NPU profile routing,
instruction decoder, OBP/RCE, Sauria DMA and unrelated NPU-top behavior. The
NPU team will provide the 128x128 extension later.

The source audit found Im2Col-related address generation embedded in the IFMAP
feeder but did not establish a standalone Col2Im block. Do not guess that
behavior. Record the Transform delivery as pending until the NPU team supplies
or identifies its module, configuration contract and golden tests.

`control/sauria_dma.h` is explicitly out of scope. The NEO DMA is designed and
implemented under TPU_V3 with TLM sockets; it never directly copies through the
Sauria SRAM or DRAM backing pointers.

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
- never wait for SA, DMA or ImageTransform completion inside MMIO `b_transport`.

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
│       │   ├── SAURIA_MATRIX_MODEL.md
│       │   ├── IMAGE_TRANSFORM_MODEL.md
│       │   ├── DMA_MODEL.md
│       │   └── VERIFICATION_PLAN.md
│       ├── common/
│       │   ├── CMakeLists.txt
│       │   ├── include/tpu_v3/architecture_config.h
│       │   ├── include/tpu_v3/address_map.h
│       │   ├── include/tpu_v3/tlm_extensions.h
│       │   └── include/tpu_v3/types.h
│       ├── core_sram/
│       │   ├── CMakeLists.txt
│       │   ├── include/tpu_v3/sram/core_sram.h
│       │   ├── include/tpu_v3/sram/sram_config.h
│       │   ├── src/core_sram.cpp
│       │   └── tests/test_core_sram.cpp
│       ├── dma/
│       │   ├── CMakeLists.txt
│       │   ├── include/tpu_v3/dma/neo_dma.h
│       │   ├── include/tpu_v3/dma/dma_config.h
│       │   ├── src/neo_dma.cpp
│       │   └── tests/
│       ├── sauria_matrix/
│       │   ├── CMakeLists.txt
│       │   ├── include/tpu_v3/sa/sauria_matrix_if.h
│       │   ├── include/tpu_v3/sa/sauria_matrix_config.h
│       │   ├── include/tpu_v3/sa/sauria_matrix_adapter.h
│       │   ├── src/sauria_matrix_adapter.cpp
│       │   └── tests/
│       ├── image_transform/
│       │   ├── CMakeLists.txt
│       │   ├── include/tpu_v3/transform/image_transform_if.h
│       │   ├── include/tpu_v3/transform/image_transform_config.h
│       │   ├── src/image_transform_adapter.cpp
│       │   └── tests/
│       ├── tpu_core/
│       │   ├── CMakeLists.txt
│       │   ├── include/tpu_v3/core/tpu_core.h
│       │   ├── include/tpu_v3/core/neo_control_fabric.h
│       │   ├── include/tpu_v3/core/neo_local_sram_fabric.h
│       │   ├── include/tpu_v3/core/local_sram_fabric_config.h
│       │   ├── include/tpu_v3/core/neo_external_bridge.h
│       │   ├── include/tpu_v3/core/core_registers.h
│       │   ├── src/tpu_core.cpp
│       │   ├── src/neo_control_fabric.cpp
│       │   ├── src/neo_local_sram_fabric.cpp
│       │   ├── src/neo_external_bridge.cpp
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
│       │   ├── sauria_matrix/
│       │   ├── image_transform/
│       │   ├── dma/
│       │   ├── core_sram/
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
>
> Added by Phase 3: `common/include/tpu_v3/sparse_memory.h`, `core_sram/`, and
> `tpu_core/` holding `neo_control_fabric`, `neo_local_sram_fabric`,
> `neo_external_bridge` and `core_registers`. `sparse_memory.h` sits in
> `common/` rather than under `core_sram/` because both the core SRAM and the
> platform's global RAM need it and it contains no SystemC, which keeps its
> test an ordinary program. `tpu_core/tpu_core.h` itself is Phase 7; the
> directory carries the name it will have then rather than being renamed
> later.

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

struct sauria_matrix_config {
    unsigned rows = 64;       // bring-up; 128 only after promotion gate
    unsigned columns = 64;
    unsigned count_per_core = 1;
    std::string source_revision;
    matrix_datatype datatype;
};

struct dma_config {
    unsigned count_per_core = 1;
    unsigned max_burst_bytes;
};

struct image_transform_config {
    unsigned count_per_core = 1;
    bool im2col_available;
    bool col2im_available;
    std::string source_revision;
};

struct local_sram_fabric_config {
    unsigned data_width_bits;       // required; no architectural default
    unsigned bank_count;            // required; no architectural default
    bank_mapping mapping = bank_mapping::low_order_interleaved;
    unsigned pipeline_stages;       // required PD/timing input
    unsigned max_outstanding_per_requester = 1;
    arbitration_policy arbitration = arbitration_policy::round_robin;
};

struct tpu_core_config {
    rvv_config rvv;
    sauria_matrix_config sa;
    dma_config dma;
    image_transform_config transform;
    local_sram_fabric_config local_sram_fabric;
    std::uint64_t sram_size_bytes;
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

Construction must reject invalid frozen values such as `cores != 2`, counts
other than one SA/DMA/Transform per core, geometry other than verified 64x64 or
promoted 128x128, an unavailable requested Transform operation, or incompatible
RVV parameters. Requesting 128x128 before its promotion gate is an error. It
must also reject a zero/non-byte-multiple local data width, a zero/non-power-of-
two bank count, unsupported bank mapping, `max_outstanding_per_requester != 1`
or an arbitration policy other than deterministic round-robin. Reference YAML
must state the open physical values explicitly; the C++ schema must not hide a
256-bit assumption in a default initializer.

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
bypass core SRAM, global memory, or the NoC.

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
- RV32 restriction on 64-bit vector index EEW. RVV 1.0 §18.2: *"the V extension
  does not support EEW=64 for index values when XLEN=32"*, and §7.3: *"An
  implementation must raise an illegal instruction exception if the EEW is not
  supported for offset elements"* — ELEN is not the yardstick here, XLEN is.
  VP++ accepted all 32 indexed EEW=64 encodings — the four unit forms and the 28
  segment forms (audit **F12**); fixed by the D12 downstream conformance patch
  and gated by `conformance_patches`;
- trap cause for a failed bus access: an access fault chosen by access origin
  (1 fetch, 5 load, 7 store/AMO), never a page fault, since `satp.MODE` is Bare
  (audit **F13**, decision record **D13**);
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

### 11.3 Core SRAM

The SRAM is local to one NEO-CORE and shared by scalar/RVV traffic, the
independent DMA, the Sauria matrix engine, the ImageTransform engine and
authorized chip/NoC traffic.

Initial required properties:

- byte-addressed little-endian storage;
- configurable capacity, with the full architectural window decoding (D6);
- sparse page-backed host storage, never an eager allocation (D6);
- deterministic initialization;
- any payload from 1 to 64 bytes, verified with a synthetic initiator (D7);
- byte-enable support;
- range and overflow checks;
- one logical address space mapped to configurable physical banks (D15);
- deterministic round-robin arbitration per bank and back-pressure on bank
  conflict, with independent-bank progress;
- at most one outstanding native request per requester in Revision 1;
- annotated access latency;
- TLM-request, physical-beat, bank-conflict, byte and latency counters per
  requester;
- debug transport for ELF/test initialization;
- reset policy documented explicitly.

#### Access granularity and counter naming (D7)

There is no vector-instruction boundary at the SRAM interface. RISC-V VP++
decomposes vector accesses per active element before the CDC-VP wrapper sees
them, and masked, strided, indexed and fault-only-first accesses could not be
one contiguous transaction under any backend.

The SRAM therefore:

- accepts any 1..64-byte payload, but never requires a 64-byte one;
- must **not** infer, group or reassemble vector instruction boundaries;
- names its counters `tlm_request_count`, `physical_beat_count`,
  `bank_conflict_count`, `transferred_bytes`, `error_count` and
  `arbitration_event_count`, with each counter's unit stated explicitly;
- records one TLM request and the actual number of physical beats if the native
  fabric splits a payload; neither number is relabelled as the other;
- must **not** expose `vector_instruction_count`, `vector_register_count` or
  anything described as a hardware bus transaction count;
- records `VP++ element-wise granularity` alongside any timing figure derived
  from these counts, which must never be offered as evidence of equivalence
  with TPU hardware.

That one request usually carries one active element is a property of the
current backend, not an SRAM invariant.

Initial coherence policy:

- no hidden caches;
- one coherent backing store;
- program-visible ordering follows blocking TLM completion;
- accelerator completion interrupt/status acts as the synchronization boundary;
- any future cache requires a separate coherence decision and test plan.

The SRAM must not call `wait()` inside `b_transport` when used behind detailed
NoC paths. Long internal operations, if any, use worker threads.

### 11.4 Sauria matrix engine

Define a geometry-aware `sauria_matrix_if` before extracting source. It is one
engine per core and implements matrix multiplication only.

Required logical parameters:

```text
Rows/Columns     : 64x64 bring-up; 128x128 after NPU-team promotion
Instances/core   : 1
Operation class  : GEMM
```

Required control contract:

- command/start;
- busy/done/error status;
- interrupt enable and level-sensitive completion IRQ;
- matrix/tensor base addresses;
- M, N, K dimensions;
- input/output strides;
- data type selection from the supported production set;
- optional accumulation only after specification;
- performance counters;
- W1C or explicit completion acknowledgment semantics.

The registers are a 32-bit AXI4-Lite target. Operand/result traffic uses a
native local-SRAM requester port; the SA is not an external AXI4/NoC master.
MMIO start must enqueue work and return. Completion occurs asynchronously.

The 64x64 adapter is built first from the verified v4.2 source. It retains only
the SA PE array and minimum required feeder/sequencer/result collector. It must
exclude Sauria DMA, NPU profile routing, instruction decoder, OBP/RCE and other
unrelated NPU-top behavior. Source-to-adapter regression must compare results
against the same Sauria golden cases at the same geometry and datatype.

The 128x128 promotion keeps this firmware-visible interface unchanged. It must
audit masks and counters wider than 64, address arithmetic, memory capacity,
elaboration cost, reset and datatype behavior. The NPU-team source revision is
recorded in the build manifest.

### 11.5 Independent NEO DMA

The DMA is owned by TPU_V3, not Sauria. Required contract:

- one 32-bit AXI4-Lite MMIO target, one native local-SRAM requester port and
  one external AXI4/NoC-facing initiator per core;
- source/destination address, length, direction/control, busy/done/error and
  level IRQ;
- legal burst chunking, deterministic partial-error semantics and byte counts;
- no direct pointer to core SRAM/global memory and no Sauria DMA dependency;
- one native local-SRAM requester port and one external AXI4/NoC-facing port;
- all traffic observes the D15 local-fabric and NoC endpoint constraints;
- reset aborts active transfer and releases all accounting.

### 11.6 ImageTransform engine

One engine per core provides Im2Col and Col2Im. The implementation and semantic
contract must be traced to an approved NPU-team revision. Required descriptor
fields include tensor bases, dimensions, layout, datatype, kernel, stride,
padding and dilation; Col2Im additionally defines overlap/accumulation behavior.

The current v4.2 source contains Im2Col-related IFMAP address generation but no
standalone Col2Im block has been established. Do not implement a guessed inverse
or relabel PSM output addressing. Until source arrives, advertise the capability
as unavailable and reject start explicitly. This open input does not block
SRAM/control/local-fabric/DMA/SA64 work.

### 11.7 NEO control, local-SRAM and external fabrics

The D15 split is an architectural RTL contract, not only a SystemC naming
convention.

**AXI4-Lite control fabric responsibilities:**

- decode 32-bit VP++ and authorized inbound MMIO accesses to core, DMA, SA,
  ImageTransform and counter registers;
- remain in order with no bursts or AXI IDs and accept at most one transaction
  per control initiator;
- enforce 4-byte alignment, full strobes and response propagation;
- be represented at transaction level without claiming channel-cycle accuracy.

**NEO Local SRAM Fabric responsibilities:**

- decode VP++ local data and native DMA/SA/ImageTransform requests to the
  logical shared SRAM, with authorized inbound traffic as a named requester;
- map the logical SRAM onto configurable physical banks;
- arbitrate independently per bank using deterministic round-robin;
- begin with at most one outstanding request per requester and preserve
  response ownership;
- allow different banks to progress concurrently and back-pressure same-bank
  conflicts;
- expose TLM-request, physical-beat, bank-conflict, byte and latency counters;
- enforce bounds, byte strobes and target response policy;
- never expose a direct pointer to backing storage and never become a full AXI
  data crossbar.

**External bridge responsibilities:**

- connect VP++ instruction/global traffic and the independent DMA's external
  port to AXI4/chip-global/NoC traffic;
- decode inbound remote MMIO to AXI4-Lite and inbound remote SRAM traffic to a
  named native requester; never bypass local arbitration;
- preserve requester ownership and partial-error accounting;
- prevent local SRAM/MMIO traffic from entering the global NoC unnecessarily;
- keep SA and ImageTransform as local-SRAM requesters, not NoC masters.

The native `request` includes requester identity, byte address, command, size,
write data and strobes; `response` includes read data and explicit status. The
physical data width, number of banks, low-order bank mapping and pipeline depth
remain validated construction-time parameters until SRAM-macro, frequency and
PD inputs are available. Do not freeze an illustrative 256-bit width in an API
or serialized schema.

The address map must be centralized in `address_map.h`. No component may carry
an independent copy of base addresses.

### 11.8 TPU core / NEO-CORE

One `tpu_core` instance owns:

- one RISC-V VP++ RV32GCV CPU backend;
- one shared core SRAM;
- one independent TPU_V3 DMA;
- one Sauria matrix engine;
- one ImageTransform engine;
- one 32-bit AXI4-Lite control fabric;
- one native banked-SRAM data fabric;
- one bidirectional external AXI4/NoC bridge;
- core-local interrupt aggregation;
- core ID and globally unique hart ID;
- reset and optional clock-domain adapters.

Hart ID mapping should default to:

```text
hart_id = chip_linear_id * 2 + core_id
```

The mapping must be tested and visible to firmware through `mhartid`.

### 11.9 TPU chip

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

### 11.10 NoC endpoint

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

### 11.11 Platform top

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
    CORE_SRAM
    SA_CONTROL
    DMA_CONTROL
    TRANSFORM_CONTROL
    CORE_COUNTERS
  CHIP_CONTROL
  CHIP_COUNTERS
```

Rules:

1. All RV32-visible physical addresses must fit below 4 GiB.
2. Regions must be power-of-two aligned where practical.
3. Core SRAM capacity must fit within its core aperture.
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
- tests exercise simultaneous SA/DMA/ImageTransform/external-inbound and
  dual-core requests.

## 14. Accuracy and Performance Strategy

The platform needs multiple fidelity levels.

### 14.1 Full-system fast mode

Use for firmware, software integration, multi-chip workloads, and long runs.

- RISC-V VP++ functional RV32GCV execution;
- extracted Sauria matrix engine at its reported geometry/datatype;
- independent DMA and verified ImageTransform capabilities;
- approximately timed AXI4-Lite control and native banked-SRAM fabrics;
- FlooNoC fast timing when contention is not the study target;
- deterministic results and metrics.

### 14.2 NoC detailed mode

Use for routing, arbitration, link back-pressure, and contention experiments.

- detailed FlooNoC backend;
- nonblocking/annotating targets;
- small or controlled workloads;
- no claim that CPU/SA/Transform/DMA timing is cycle accurate unless separately calibrated.

### 14.3 Sauria promotion/detail mode

Use for selected single-SA or single-core experiments and for promoting the NPU
team's 128x128 source.

- Sauria behavior only at verified geometry/datatype/source revision;
- explicit SA-only extraction scope and array mapping;
- measured host runtime and memory before scaling;
- separate results from full-system fast-mode results.

### 14.4 Prohibited accuracy claims

Do not claim:

- Google TPUv3 RTL equivalence;
- cycle-accurate RVV or Google TPU timing from RISC-V VP++;
- exact 128x128 behavior or timing from the 64x64 bring-up source;
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

option(TPU_V3_ENABLE_SAURIA_MATRIX
    "Build the matrix-only Sauria adapter" ON)

option(TPU_V3_ENABLE_IMAGE_TRANSFORM
    "Build the verified NPU-team Im2Col/Col2Im adapter" OFF)

set(TPU_V3_SA_GEOMETRY "64x64" CACHE STRING
    "TPU_V3 Sauria geometry: 64x64 or promoted 128x128")
```

Add a pinned RISC-V VP++ source path/cache variable with a clear failure if
missing. Keep the optional Spike reference path separate so a production
runtime build cannot accidentally acquire a Spike dependency. The TPU_V3 SA
target consumes selected headers/source through its own adapter and must not
link `cdc::components::npu_tlm` or instantiate `NpuTop`. The existing
`CDC_ENABLE_SAURIA_NPU_V4` option remains for legacy platforms and is not the
NEO-CORE matrix-selection switch.

### 15.2 Component targets

Expected exported targets:

```text
cdc::components::tpu_v3_common
cdc::components::tpu_v3_core_sram
cdc::components::tpu_v3_dma
cdc::components::tpu_v3_sauria_matrix
cdc::components::tpu_v3_image_transform
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
    -DSAURIA_NPU_ROOT="$SAURIA_NPU_ROOT" \
    -DTPU_V3_ENABLE_SAURIA_MATRIX=ON \
    -DTPU_V3_ENABLE_IMAGE_TRANSFORM=OFF \
    -DTPU_V3_SA_GEOMETRY=64x64

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
  values with explicit temporary defaults. **Historical Phase 0 wording:** D14
  renames these to core SRAM and Sauria SA and supersedes the old composition.

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
  the acquisition script, and base revision + per-patch kind/reference/hash +
  effective source written into `BUILD_MANIFEST.json`.

### Phase 3: NEO core SRAM, split fabrics and map migration

#### Tasks

- Rename the Phase 1 SVM/MXU address symbols to the D14 `CORE_SRAM`,
  `SA_CONTROL`, `DMA_CONTROL` and `TRANSFORM_CONTROL` contract; update C++ map,
  tests, config serialization and generated firmware names together.
- Implement deterministic sparse 4 KiB page-backed storage for core SRAM and
  global RAM; do not eagerly allocate logical capacity.
- Implement the transaction-level 32-bit AXI4-Lite control decoder: in order,
  4-byte accesses, full strobes, no bursts or IDs, and at most one transaction
  per control initiator.
- Define the native local-SRAM request/response types and implement
  `neo_local_sram_fabric` with named requester ownership, configurable physical
  banks, deterministic per-bank round-robin arbitration, back-pressure and
  independent-bank progress.
- Add validated configuration/report fields for physical data width, bank
  count, low-order bank mapping and pipeline depth. Do not freeze 256 bits
  without SRAM-macro, frequency and PD evidence.
- Keep decoded window, capacity and allocated backing separate; cover bounds,
  byte enables, cross-page traffic, reset and debug initialization.
- Accept every 1..64-byte payload and retain D7 element-wise counter semantics.
- Route synthetic CPU, SA, DMA and Transform native requesters to SRAM and
  error targets; include a named external-inbound requester and route synthetic
  CPU/inbound control accesses to each AXI4-Lite target. No block-specific
  compute is required yet.

#### Gate

- The legacy `SVM`/`MXU0_CONTROL`/`MXU1_CONTROL` symbols are absent from current
  architecture config and new firmware-visible address generation; historical
  audits may still quote them.
- Address-map non-overlap, decode, all 1..64-byte widths, byte enables,
  cross-page, reset, sparse allocation and requester attribution tests pass.
- AXI4-Lite alignment/strobe/error tests pass; no accelerator bulk payload is
  routed through the control fabric.
- Same-bank round-robin, different-bank concurrency, back-pressure, response
  ownership and TLM-request-versus-physical-beat counters pass under watchdog.
- Inbound remote SRAM/MMIO traffic traverses the adapters and normal
  arbitration; an attempted bypass is covered by a negative test.
- No target waits inside `b_transport`; arbitration/timing is deterministic.
  Decision record D16 states which components this binds and why the local
  fabric's `arbitrated` mode — the one in which round-robin fairness is a
  behaviour rather than an estimate — is nonetheless allowed to block its own
  requesters.
- No source file or public type named `neo_axi_fabric` remains after migration.
- `mesh_4x4` logical memory elaborates without eager host commitment.

### Phase 4: Independent NEO DMA

#### Tasks

- Freeze DMA descriptor/status/IRQ and partial-transfer semantics.
- Implement one TPU_V3 DMA per core with an AXI4-Lite MMIO target, native
  local-SRAM requester and external AXI4/NoC-facing initiator.
- Split transfers to legal downstream burst sizes and propagate the first error
  with completed-byte accounting.
- Add reset/abort, overlap, odd-length, boundary and concurrent-request tests.
- Add a source/build guard proving no Sauria DMA header or symbol is used.

#### Gate

- DMA copies only through observable native local-SRAM transactions and the
  external AXI4/NoC-facing path; local and external byte counts conserve.
- A negative control using a failing destination proves response propagation and
  partial-byte accounting.
- No direct SRAM/global backing pointer and no `control/sauria_dma.h` dependency
  exists.
- MMIO start returns immediately; completion/error IRQ is level-sensitive.

### Phase 5: Sauria matrix engine, 64x64 bring-up

#### Tasks

- Pin and record the NPU-team v4.2 source revision and redistribution policy.
- Freeze `sauria_matrix_if` and the geometry/datatype reporting contract.
- Extract/wrap matrix multiplication plus only required feeder/sequencer/result
  collection; exclude Sauria DMA, profile/instruction top, OBP and RCE.
- Connect operand/result accesses through the native local-SRAM port rather
  than direct source backing; do not add an external AXI4 master.
- Port the source golden cases for the selected verified 64x64 datatype.
- Measure elaboration, host memory and runtime for one engine and two cores.

#### Gate

- Source NPU and extracted adapter match on the accepted 64x64 corpus.
- GEMM dimensions, edge tiles, asynchronous start, IRQ, reset and errors pass.
- Dependency review proves unrelated NPU-top blocks and Sauria DMA are absent.
- Reports and manifests say `64x64` and the actual datatype; no 128x128/BF16
  claim is made without evidence.

### Phase 6: ImageTransform extraction

#### Tasks

- Obtain from the NPU team the source revision, interfaces and golden tests for
  both Im2Col and Col2Im.
- Define tensor layout, kernel, stride, padding, dilation, datatype and Col2Im
  overlap/accumulation semantics before implementation.
- Implement one adapter with a 32-bit AXI4-Lite MMIO target and native
  local-SRAM requester; do not add an external AXI4 master.
- Cross-check both operations against NPU-team golden output, including edge
  dimensions and overlapping Col2Im contributions.

#### Gate

- Every exposed operation maps to traced NPU-team source and a golden test.
- A missing operation is reported unavailable and rejects start; no placeholder
  returns success.
- Im2Col and Col2Im round-trip tests are used only where the mathematical/layout
  contract says a round trip is valid.

This phase is waiting for NPU-team clarification if v4.2 has no standalone
Transform module. That wait does not block Phases 3–5.

### Phase 7: Single NEO-CORE integration

#### Tasks

- Instantiate VP++ RV32GCV, core SRAM, NEO DMA, Sauria SA64, ImageTransform and
  the AXI4-Lite control, native local-SRAM and external bridge components.
- Wire reset and per-engine level interrupts.
- Add firmware drivers for SRAM, DMA, SA and ImageTransform.
- Run `DMA -> Im2Col -> SA -> Col2Im -> RVV` using SRAM descriptors/buffers.

#### Gate

- One firmware ELF boots and controls every available block through MMIO.
- Control traffic is visible on AXI4-Lite, local data on the native SRAM
  fabric, and remote data on the external bridge; final data matches the
  accepted golden model.
- Scalar and RVV remain one hart/memory path; no second vector processor exists.
- Unmapped, unavailable, misaligned, reset and injected-error cases fail
  predictably under watchdogs.

### Phase 8: Dual-core TPU chip

#### Tasks

- Instantiate exactly two NEO-COREs with unique hart IDs.
- Implement chip-local aperture decode, outbound arbitration and inbound route.
- Test simultaneous CPU, DMA, SA and Transform traffic on both cores.
- Close the pre-existing multi-hart AMO and full architectural reset gates.

#### Gate

- Both harts boot and run independent workloads concurrently.
- One core cannot corrupt the other core's private SRAM accidentally.
- Authorized remote/debug accesses map to the intended core.
- Ownership, IRQs and responses remain correct under cross-engine contention.

### Phase 9: NoC integration and mesh scalability

#### Tasks

- Close D1 owner-aware local bypass before co-locating manager/target endpoints.
- Connect one dual-core chip and global RAM to FlooNoC; validate local containment
  and legal burst chunking.
- Bring up a 2x2 mesh, then allowed Revision 1 configurations up to eight chips.
- Test inter-chip DMA and CPU traffic, contention, errors and reset.
- Keep any manager-ID widening/32-core work as its separate coordinated change.

#### Gate

- Local SRAM/MMIO traffic injects zero NoC flits; remote traffic routes normally.
- Transfers larger than the NoC frame limit are chunked with defined partial
  failure semantics.
- 2x2 concurrent inter-chip traffic completes without deadlock or response
  misattribution and detailed conservation checks pass.
- Missing/wrong local-owner mappings fail at elaboration.

### Phase 9B: Sauria 128x128 promotion

This phase begins when the NPU team supplies the updated source. It does not
block 64x64 bring-up, but it is required before claiming the 128x128 target.

#### Tasks and gate

- Pin source and audit all masks, widths, address generators and memory sizing
  affected by 128 rows/columns.
- Reuse the Phase 5 interface and corpus, add 127/128/129 and full-array cases,
  and verify the actual supported datatype contract.
- Measure one engine, two-core chip and selected mesh scalability.
- Permit `TPU_V3_SA_GEOMETRY=128x128` only after configure-time provenance and
  runtime geometry checks pass; otherwise reject it.

### Phase 10: Firmware and workload integration

#### Tasks

- Add startup code and per-hart stacks.
- Add trap/interrupt handling.
- Add RVV compile flags and intrinsic/assembly tests.
- Add core SRAM, NEO DMA, Sauria matrix and ImageTransform drivers.
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
- Timeout/error reporting identifies chip, core, engine, PC, descriptor and status.

### Phase 11: Metrics, performance, and stress

#### Required metrics

CPU/RVV:

- instructions retired;
- scalar/vector instruction counts;
- vector element operations by SEW;
- vector loads/stores and bytes;
- approximate execution cycles.

Sauria matrix engine:

- jobs;
- MAC operations;
- active/busy cycles;
- utilization estimate;
- bytes read/written;
- stalls by cause;
- error/reset counts.

DMA and ImageTransform:

- jobs and operation type;
- bytes read/written;
- busy cycles, partial transfers, errors and reset/abort counts.

AXI4-Lite control fabric:

- reads/writes by target;
- decode/alignment/strobe errors;
- response latency totals/maxima.

Core SRAM/NEO Local SRAM Fabric:

- TLM requests, physical beats and bytes per requester;
- arbitration stalls and bank conflicts;
- independent-bank overlap;
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
- Run packaged smoke, RVV, SRAM, DMA, SA, Transform, single-chip, and selected mesh tests.
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
| Unit | Core SRAM | Widths, byte enables, sparse pages, boundaries, concurrency, reset |
| Unit | NEO DMA | TLM-only copies, burst splitting, partial errors, async IRQ, reset |
| Unit | Sauria SA | Source golden, 64x64 geometry, data type, async IRQ, reset |
| Unit | ImageTransform | NPU-team Im2Col/Col2Im golden, layouts, overlap, unavailable capability |
| Unit | AXI4-Lite control fabric | 32-bit decode, alignment/strobes, ordering and errors |
| Unit | NEO Local SRAM Fabric | Bank decode, round-robin, back-pressure, ownership, independent-bank progress and errors |
| Unit | Chip fabric | Two-core ownership, inbound/outbound routing |
| Unit | NoC endpoint | placement, chunking, completion, partial errors |
| Integration | Single core | DMA -> Im2Col -> SA -> Col2Im -> RVV over core SRAM |
| Integration | Single chip | two harts, two SRAMs, two DMAs, two SAs, two Transform engines |
| Integration | NoC | global RAM and remote chip traffic |
| System | Firmware | boot, IRQ, drivers, workload completion |
| Stress | Concurrency | simultaneous harts/DMA/SA/Transform/NoC traffic |
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

### R2. Sauria 64x64 bring-up versus 128x128 target

Risk: existing v4.2 Sauria verifies 64x64, while the target 128x128 source is a
future NPU-team delivery. Hard-coded 64-bit masks or address assumptions can
make a nominal template change incorrect.

Mitigation:

- integrate only the verified 64x64 source first behind a geometry-aware adapter;
- reject 128x128 until source, golden and width/resource gates pass;
- audit hard-coded masks/address widths during promotion;
- report actual geometry and datatype in every run and package.

### R2A. Transform delivery and semantics

Risk: v4.2 has Im2Col-related feeder logic but no established standalone
Col2Im block. Guessing Col2Im layout or overlap behavior can produce plausible
but wrong images.

Mitigation:

- obtain module/interface/golden tests from the NPU team;
- expose unavailable capability rather than a fake implementation;
- trace every copied/extracted path to an immutable source revision;
- do not equate PSM output addressing with Col2Im without evidence.

### R3. Simulation scalability

Risk: many chips times two cores and detailed 64x64/128x128 SAs can make
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
3. Every NEO-CORE contains one RV32GCV hart, one core SRAM, one independent
   TPU_V3 DMA, one Sauria matrix engine, one Im2Col/Col2Im Transform engine and
   the D15 split: 32-bit AXI4-Lite control, native banked-SRAM local data and an
   bidirectional external AXI4/NoC bridge; no internal full AXI data crossbar
   exists.
4. RVV 1.0 reports XLEN 32, VLEN 512, ELEN 64, and executes the required full-V
   instruction groups.
5. RVV memory traffic uses TLM and interacts with core SRAM/global memory correctly.
6. DMA, SA and ImageTransform execute asynchronously and concurrently with
   correct data, ownership, errors and level interrupts.
7. Both TPU cores in one chip can boot and run concurrently with unique IDs.
8. Multiple chips communicate through the parameterized 2D mesh without
   deadlock or response misattribution.
9. Local accesses bypass the global NoC.
10. Large tensor transfers are safely chunked to legal NoC bursts.
11. 64x64 bring-up and promoted 128x128 modes have documented, tested geometry,
    datatype and accuracy boundaries.
12. Firmware, drivers, and at least one end-to-end workload pass golden checks.
13. Metrics pass conservation and noninterference tests.
14. Component, platform, firmware, stress, negative-control, and packaging
    regressions pass in a clean build.
15. `out/tpu_v3_soc/` contains a portable executable, configs, selected
    firmware, SystemC runtime, licenses, provenance, and build manifest.
16. The packaged executable runs without the source/build tree.

## 21. Immediate Next Actions

Current D14/D15 work, in order (updated 2026-08-12):

1. ~~Migrate `architecture_config` and `address_map` from the legacy
   two-MXU/SVM names to one SA, one independent DMA, one Transform engine and
   core SRAM.~~ Done in Phase 3. The legacy names are absent from the
   configuration schema, the map, the shipped configurations, the CMake
   variables, the manifest and the packaged `--print-address-map` output, and
   both the CLI regression and the packaging regression fail if they reappear.
2. ~~Implement Phase 3 core SRAM, 32-bit AXI4-Lite control fabric and native
   banked-SRAM data fabric, leaving bank count/width/pipeline depth
   configurable.~~ Done — `tpu_v3_core_sram` and `tpu_v3_tpu_core`, gated by
   `tpu_v3_core_sram`, `tpu_v3_local_sram_fabric`, `tpu_v3_control_fabric` and
   `tpu_v3_external_bridge`. The three physical values have no default and the
   schema refuses zero.
3. Implement Phase 4 NEO DMA with a build/test guard against Sauria DMA use.
4. Pin the accepted v4.2 source and perform the Phase 5 matrix-only dependency
   extraction audit before writing the adapter.
5. Ask the NPU team to identify/provide the standalone Im2Col/Col2Im Transform
   module, semantic contract and golden tests, and separately provide the later
   128x128 Sauria source.

Carried out of Phase 3 as scheduled work, not as open findings:

6. Phase 5 and Phase 7 must select `local_fabric_timing::annotated` for
   full-system runs. `arbitrated` consumes the caller's quantum, so a VP++ hart
   behind it loses temporal decoupling on every local load and store (decision
   record D16).
7. Phase 9 must assert `!neo_external_bridge::blocks_on_arbitration()` when the
   detailed NoC backend is selected: an `arbitrated` fabric on the inbound path
   would stall the one process that advances the mesh clock.
8. Phase 7 composes the three fabrics, the SRAM and the hart into a
   `tpu_core`. Until then the fabrics are proved as components and the platform
   instantiates the memories only, and its report says so.

Items below are retained as closure history for Phases 0–2; they are not the
current execution queue.

Done (2026-08-08):

1. ~~Complete Phase 0 dependency, revision, toolchain, and license audit.~~
2. ~~Create the Phase 1 directory/CMake skeleton.~~
3. ~~Add `tpu_v3_soc` and `tpu_v3_soc_package` smoke targets.~~
4. ~~Prove the portable skeleton runs from `out/tpu_v3_soc/`.~~
5. ~~Pin the initial Spike differential reference and the RV32GCV
   cross-toolchain.~~ The runtime role was later reassigned to RISC-V VP++ by
   D3; the VP++ candidate SHA is deliberately still open pending its audit.

Phase 2, done (2026-08-10) — evidence in `docs/TPU_V3_PHASE2_AUDIT.md`:

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

Additional Phase 2 closure evidence:

8. ~~Build the freestanding RV32GCV smoke environment: `-ffreestanding
   -nostdlib -nostartfiles`, a project-owned `crt0.S`, linker script, stack
   setup, trap entry and pass/fail reporting, depending on neither libc nor
   libm (D4).~~
9. ~~Build and disassemble the RV32GCV smoke ELF listed in Phase 2, and check its
   ISA attributes.~~
11. ~~Execute the smoke ELF through `cdc::cpu::riscv_vp_plusplus` and observe
    fetch, scalar, vector and MMIO traffic on the TLM socket — routing it there
    is proven, traffic on it is not yet.~~ Closed with 2096 observed requests
    and the 14-check RVV smoke gate.
12. ~~Build the pinned Spike oracle and run the differential corpus.~~ Done as
    `spike_differential` (D11). Spike is built at pin `16c0b601...` by
    `oracle/fetch_spike.sh` and run as a child process; one image
    (`rvv_sig.elf`) executes on both models and a 71-field canonical signature
    block is compared word by word. Result: **64 matched, 7 XFAIL against D9's
    reachable commits, 0 OPEN, 0 unexplained** — Debug and Release.
    The AMO cases stay out: a single-hart differential run cannot demonstrate
    atomicity between harts, so `52d376d4` keeps its own pre-Phase-6 test.
13. ~~Add the vector-trap tests.~~ Done as `rvv_vector_trap` (D10): a fault at
    element 5 of a vector load resumes from `vstart` with elements below it
    provably not re-accessed; `mstatus.VS` Off makes a vector instruction
    illegal and Initial becomes Dirty; a reserved `vtype` sets `vill` and zeroes
    `vl` without trapping. The RV32 index-EEW=64 outcome was **recorded, not
    asserted** until the differential run decided it; the image now asserts it,
    §11.2 carries the citation, and the model is fixed under D12.
14. ~~Add the audit §F5 control.~~ Done as `fp_concurrency_normal` /
    `fp_concurrency_swapped`: two harts with different `frm`, 200 iterations
    each, operands whose two roundings differ, results **and** `fflags`
    asserted per iteration, both creation orders, and the interleaving measured
    (1890 hart switches) rather than assumed. No leak observed, so the
    save/restore mitigation is deliberately not implemented.

Resolved 2026-08-10:

16. ~~Audit §F11, the bogus cycle baseline.~~ Closed by a controlled backport of
    upstream `b710fa7b` onto the base pin — not baseline subtraction, and not a
    SystemC 3.0.1 migration. It is a **Phase 2 closure gate**, verified in Debug
    and Release across two harts.

Two upstream fixes are deliberately *not* taken with it, each needing its own
evidence first:

17. `7a936cce` "fixed fast quantum" — a larger change to quantum accounting.
    Audit it separately **before Phase 9**.
18. `52d376d4` AMO atomicity / lost bus lock — needs a **multi-hart AMO
    contention test before Phase 8**. A single-threaded differential run against
    Spike cannot demonstrate atomicity between harts.

Optional, in parallel: ask upstream for a maintenance backport on a SystemC
2.3-compatible branch. If they publish one, move the pin to that commit, drop
the local patch, and re-run the whole Phase 2 gate.

Due before Phase 7, and not a Phase 7 discovery:

15. Decide full architectural core reset semantics (audit §F8). `reset_cpu()` is
    a restart at the reset PC plus cache reinitialisation; it clears none of the
    GPRs, FP/vector registers, CSRs, `vstart`, `instret`, pending interrupts or
    privilege level, so the hierarchical reset in `ARCHITECTURE.md` §6 is not
    yet implementable and a reset test built on it would prove nothing.

Also outstanding:

* **Phase 9 prerequisite:** the owner-aware local bypass in
  `noc_interconnect` (D1). It needs a cross-checked change to an RTL-signed
  component and its own negative controls — a missing or wrong owner mapping
  must fail during elaboration, and a local access must inject zero flits.

Do not begin full mesh composition or claim 128x128 before the corresponding
D14 phase and promotion gates pass.

## 22. Status Table

Update this table when work progresses.

| Phase | Status | Evidence |
| --- | --- | --- |
| Plan document | Complete | This file |
| Decision record D1-D16 | Final approved through D15 interconnect ratification (2026-08-12); D16 added by Phase 3 | `docs/TPU_V3_DECISION_RECORD.md` |
| Phase 0: audit/baseline | Complete (2026-08-08) | `docs/TPU_V3_PHASE0_AUDIT.md`, `docs/ARCHITECTURE.md`, `docs/ADDRESS_MAP.md`, `docs/INTERFACE_CONTRACT.md` |
| Phase 1: skeleton/package | Complete (2026-08-08), review findings closed | `out/tpu_v3_soc/` runs with `RPATH=$ORIGIN` and no source-tree path; ctest `tpu_v3_address_map`, `tpu_v3_architecture_config`, `tpu_v3_soc_cli`, `tpu_v3_soc_packaging_regression` all pass |
| D14/D15 rebaseline synchronization | **Complete.** Architecture documents, the D15 diagram and the C++ config/address/fabric migration all agree | Decision-record synchronization table, `docs/neo_core_architecture-d15.drawio`, and the Phase 3 gate evidence below |
| Phase 2: RV32GCV backend | **Complete** (2026-08-10), review findings closed | Audit, pin, build proof and execution complete — `docs/TPU_V3_PHASE2_AUDIT.md`. VP++ pinned at `7a36fe859cae242f513ca6ad16ab8238f1e82977` (tag `2025.09`); `cdc::cpu::riscv_vp_plusplus` builds against SystemC 2.3.4; D5 `cpu_config` implemented; the freestanding RV32GCV image executes through the wrapper with 2096 observed TLM requests and all 14 RVV checks passing (`riscv_vp_plusplus_backend`, `rvv_smoke_execution`). **F11 closed** by the approved backport of upstream `b710fa7b`: first TLM request at 0 s, `mcycle` 15 → 2439 → 2848, verified in Debug and Release on two harts, and both the configure check and the runtime gate were shown to fail when the patch is reverted. D10 vector-trap gate and the F5 concurrency control both pass (`rvv_vector_trap`, `fp_concurrency_normal`, `fp_concurrency_swapped`). **Spike differential corpus complete** (D11): one image on both models, 71-field signature, **64 matched / 7 XFAIL / 0 OPEN / 0 unexplained**, green in Debug and Release. The two findings it produced were fixed, not accepted: **D12** (all 32 RV32 index-EEW=64 encodings, unit and segment, raise an illegal instruction at the decode site) and **D13** (a failed bus access is an access fault chosen by origin). Both have a `conformance_patches` gate and a verified negative control. Also closed in this pass: the ISS caches were found enabled contrary to P2-5 and are now off and pinned by a test; `set_irq()` gained its first gate (`interrupt_delivery`); and the portable-executable requirement is met by `riscv_vp_plusplus_portable`, which runs the backend from a directory containing only the binary, its SystemC libraries and one ELF |
| Phase 3: core SRAM + split fabrics + map migration | **Complete** (2026-08-12) | **Map migration.** `CORE_SRAM`, `SA_CONTROL`, `DMA_CONTROL`, `TRANSFORM_CONTROL` and a relocated `CORE_COUNTERS` replace the Phase 1 `SVM`/`MXU0`/`MXU1` symbols; 115 regions at 8 chips, non-overlap proved at every legal chip count and capacity; `TPU_V3_MXU_BACKEND` retired in favour of `TPU_V3_SA_GEOMETRY`, whose refusal of `128x128` and of an unnamed geometry each have a negative control in the packaging regression. The legacy names are absent from the schema, the shipped configurations, the manifest and the packaged address-map output, and two independent regressions fail if they return. **Host backing.** `sparse_memory` gives deterministic 4 KiB pages: an unallocated page reads as zero and costs nothing, a write commits only touched pages, a fully masked write commits none, reset releases them, and a refused access leaves the caller's buffer untouched. `mesh_4x4` reports 1.25 GiB logical with 0 B allocated and peaks near 10 MiB of RSS, bounded by the CLI regression's own `ru_maxrss` check. **Core SRAM.** Window and capacity are separate fields; an access above the capacity is `capacity_error` and never an alias; every payload from 1 to 64 bytes works at every alignment; byte enables, cross-page transfers, counters and a debug path that bypasses the counters but not the bounds all pass. **AXI4-Lite control fabric.** Five register files behind one in-order decoder; 1-, 2-, 8- and 64-byte payloads, misalignment, partial strobes and wrapped streaming are each refused with the documented status; an unmapped address is an address error; a target's own refusal is propagated and counted apart; one transaction per initiator is enforced against a target that deliberately re-enters; an overlapping control map is refused during elaboration. A 64-byte accelerator payload is refused, which is how bulk data is kept off the control plane. **Native local-SRAM fabric.** 128-bit x 4 banks (provisional), low-order interleaved; one 64-byte request is one request and four beats, and an unaligned one is five — the D7 distinction, measured. Three requesters on one bank are serialised, back-pressured and share the bank within a quarter of each other; three on different banks run with zero conflicts and carry more traffic in the same wall of simulated time; response ownership holds under contention; all five named requesters reach both storage and the error path; an unattached requester throws. **External bridge.** Inbound SRAM traffic is arbitrated as `external_inbound` and its bytes reconcile exactly with the SRAM's own totals, which is the bypass negative control given that `core_sram` exposes no backing pointer; inbound MMIO reaches the addressed register file through the control plane and gets the same refusals a local access would; an outbound access naming this core is refused and counted, and the external stub never sees it. **Blocking.** No TLM target waits; the control fabric never waits; the local fabric's default `annotated` mode never waits and its `arbitrated` mode is the one that proves the arbitration (decision record D16). No source file or public type is named `neo_axi_fabric`. **Review round (2026-08-12).** Four contract defects were found and fixed, each with a negative control that fails when the fix is reverted: the bridge forwarded TLM's repeating `byte_enable` pattern into a plane that has no repeat rule, overreading the initiator's array for any payload longer than the pattern; the one-outstanding-request-per-requester rule of D15 was assumed rather than enforced, so two processes sharing an identity interleaved under one name; `reset()` cleared `waiting[]` and `busy` without waking anyone, hanging any requester blocked in arbitration; and outbound local containment tested containment instead of overlap, forwarding a transfer that began inside the core and ran past it. Two counter definitions were tightened in the same pass — a refused request is still a request, and `transferred_bytes` counts bytes moved rather than bytes named — along with the debug path's command handling and its STATUS answer. **Cleanup review.** Reset generation is captured before input-delay consumption, old requests cannot repopulate a new counter epoch, `aborted` explicitly permits already-completed partial bytes, and the bridge now applies common payload validation consistently to inbound/outbound normal/debug traffic. Each behavior has a regression that checks both the refusal/abort and absence of side effects. 19/19 `tpu_v3` tests pass in both Release and Debug with zero skips |
| Phase 4: independent NEO DMA | Not started | Must prove no Sauria DMA dependency; the native local port it needs exists and is gated |
| Phase 5: Sauria SA 64x64 extraction | Not started | v4.2 source audit/golden required |
| Phase 6: Im2Col/Col2Im Transform | Waiting for NPU-team source clarification | Does not block Phases 3–5. `TRANSFORM_CONTROL` elaborates today and reports its capability as unavailable rather than faking readiness |
| Phase 7: single NEO-CORE | Not started | Composes the Phase 3 fabrics, the SRAM and the Phase 2 hart. Must select `annotated` fabric timing (D16) |
| Phase 8: dual-core chip | Not started | — |
| Phase 9: NoC/mesh | Not started | D1 prerequisite; must also assert the external bridge does not block on arbitration (D16) |
| Phase 9B: Sauria 128x128 promotion | Waiting for NPU-team delivery | 64x64 must remain explicitly labelled until then |
| Phase 10: firmware/workloads | Not started | — |
| Phase 11: metrics/stress | Not started | — |
| Phase 12: packaging/signoff | Not started | — |

## 23. Decision Log

| Decision | Value | Status |
| --- | --- | --- |
| TPU cores per chip | 2 | Frozen |
| Sauria matrix engines per NEO-CORE | 1 | Frozen by D14; supersedes 2 MXUs/core |
| SA geometry | 64x64 verified bring-up; 128x128 target from NPU team | Staged and gated by D14 |
| DMA per NEO-CORE | 1 independent TPU_V3 DMA | Frozen by D14; Sauria DMA forbidden |
| ImageTransform per NEO-CORE | 1, Im2Col + Col2Im | Frozen composition; source/interface pending NPU team |
| Control plane | 32-bit AXI4-Lite, in order, no bursts or IDs | Frozen by D15; SystemC is transaction-level, not channel-cycle accurate |
| Local data plane | Native pipelined request/response fabric into physically banked SRAM | Frozen by D15; no internal full AXI data crossbar |
| Local-fabric arbitration | Deterministic round-robin per bank; one outstanding request/requester initially | Frozen by D15; implemented and gated by `tpu_v3_local_sram_fabric` |
| Local-fabric timing mode | `annotated` (default, never waits) or `arbitrated` (blocks on a real per-bank arbiter) | Approved (D16). Every TLM target still never waits; a timing figure must name the mode |
| Local-fabric physical parameters | Data width, bank count/mapping and pipeline depth | **Still open** pending SRAM macro, frequency and PD inputs. The C++ schema has no default and refuses zero; the shipped configurations state 128-bit x 4 banks x 2 stages and every report prints them labelled provisional |
| External data plane | Bidirectional AXI4/NoC bridge for outbound VP++/DMA and inbound remote traffic | Frozen by D15; DMA owns bulk movement, while SA and ImageTransform remain local-SRAM requesters |
| VPU ISA | RISC-V V | Frozen |
| RVV version | 1.0 | Frozen |
| XLEN | 32 | Frozen |
| VLEN | 512 bits | Frozen |
| ELEN | 64 bits | Frozen |
| NoC topology | Parameterized 2D mesh | Frozen concept, dimensions open |
| NoC attachment | One aggregated endpoint per chip | Frozen |
| RV32GCV runtime | RISC-V VP++ (`ics-jku/riscv-vp-plusplus`), MIT | Phase 2 complete at the recorded pin and approved patch series |
| RVV differential reference | Spike `16c0b60119f65a648643cf5d41e4e38e871f0bad` (2026-08-07), BSD-3-Clause | Built and in use as the Phase 2 oracle (D3, D11). Child process only; never linked into the platform or a package |
| Cross toolchain | xPack `riscv-none-elf` GCC 15.2.0-1, `/opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1` | Pinned (D4); Phase 2 uses a freestanding `-nostdlib` environment |
| Matrix backend | Sauria matrix-only adapter | 64x64 first, 128x128 after NPU-team promotion; source revision and datatype reported |
| Maximum chips | 8 chips / 16 cores | **Revision 1 backend limit** (D2), from the 3-bit NoC manager id. 32 cores = 16 chips is a separate coordinated change |
| Mesh dimensions | Configurable within 2x2, 3x3, 4x4, 4x2, 2x4 | Frozen set, choice open |
| Address map layout | D14/D15 rebaseline (`docs/ADDRESS_MAP.md`) | Top/chip/core apertures retained; the core-local symbol and MMIO migration landed in Phase 3 and the legacy names are gated out of generated output |
| Window vs capacity | Full window always decodes; capacity is separately reported and the target refuses above it | Frozen (D6) |
| Core SRAM capacity | **Reference 16 MiB** per core (the full window); smaller values are labelled bring-up | D6 retained and renamed by D14 |
| Global RAM size | Configurable, 256 MiB bring-up default, 1 GiB window | Open capacity (D6); simulated backing memory, not a model of TPU v3 HBM |
| Host-memory backing | Sparse deterministic 4 KiB pages; logical capacity is not eagerly allocated | Implemented (D6). `sparse_memory`; `mesh_4x4` is 1.25 GiB logical at ~10 MiB of RSS |
| SA target arithmetic | **BF16 x BF16 with IEEE FP32 accumulation**, fixed accumulation order | D6 target retained; v4.2 bring-up must report its actual supported datatype and is not reference equivalence |
| Same-node initiator/target | Keep `NoLoopback=1`; owner-aware local bypass | Approved (D1). **Phase 9 prerequisite** |
| Hart id / reset PC | Static fields in `cdc::cpu::cpu_config` | Approved (D5, supersedes P0-9). No default no-op virtual setters |
| Vector memory access granularity | Element-wise, as VP++ issues it; no fork, no inferred coalescing; counters named for TLM requests | Approved (D7) |
| Full architectural core reset | Undecided | **Open — due before Phase 7.** Today's `reset_cpu()` is a restart plus cache reinit, not a reset; see `docs/TPU_V3_PHASE2_AUDIT.md` F8 |
| Differential method | One image on both models; compare a firmware-written 71-field signature block located by linker symbols; Spike as a child process, never linked | Approved (D11). `spike_differential` |
| RV32 index EEW=64 | All 32 indexed encodings — unit and segment — raise an illegal instruction at the RV32 decode site | Fixed (D12), downstream conformance patch. Audit F12 |
| Trap cause for a failed bus access | Access fault by origin — 1 fetch, 5 load, 7 store/AMO; page faults only from MMU translation | Fixed (D13), downstream conformance patch. Audit F13 |
| VP++ patch series | Base pin + `upstream-backport` and `downstream-conformance` patches, hash-verified at configure time and recorded per-patch in the manifest | Approved (D8 mechanism, extended by D12/D13) |
| Transform block | NPU-team Im2Col/Col2Im engine | Required by D14; exact source and semantics open, never substituted by RVV reduction/permutation |
| Four-image workload | Optional future named workload | Not frozen |

The authority for D1–D16 is `docs/TPU_V3_DECISION_RECORD.md`.
