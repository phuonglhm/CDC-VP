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
├── platforms/riscv_vpp_compiler_vp/ # Single-hart compiler-enablement platform
├── fw/riscv_vpp_compiler_vp/     # Freestanding SDK and compiler demos
├── platforms/TPU_V3_SoC/         # SoC platform composition and executable
├── fw/TPU_V3_SoC/                # Bare-metal firmware and drivers
├── build-tpu-v3/                 # Out-of-source build directory
├── out/riscv_vpp_compiler_vp/    # Portable pre-Phase-5 compiler handoff
└── out/tpu_v3_soc/               # Portable packaged binary
```

The exact capitalization of existing requested directories is preserved in
this plan. CMake target names and C++ namespaces use lower-case/snake-case
conventions.

## 2. AI Execution Protocol

An AI agent implementing this plan must follow these rules:

**Mandatory host-toolchain preflight.** Run the following block in the same
shell session or command invocation immediately before every configure, build,
test or simulator run. Do not rely on exports made by an earlier tool call.
Skipping this preflight has caused the build/simulation environment to hang.
If either sanity command fails or resolves to another compiler, stop and fix
the environment before continuing.

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
# sanity
$CC -dumpfullversion
$CXX --version | head
```

This selects the host compiler used to build CDC-VP. It does not replace the
pinned RISC-V cross-toolchain used to build guest firmware; invoke that
toolchain through the documented absolute path or CMake configuration.

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
    `b_transport`; a register access must not block until an MXU, DMA or
    Transform job completes.
15. Keep this document's status table and decision log current when a phase or
    architectural decision changes.

## 3. Project Mission

Build a parameterized SystemC/TLM SoC platform whose backbone is a 2D mesh NoC.
Each NoC node represents one TPU chip. Each TPU chip contains two TPU cores.

The final platform must:

- execute RV32 scalar and RISC-V Vector 1.0 firmware;
- compose each TPU/NEO core from one VP++ hart, one shared SRAM, one independent
  TPU_V3 DMA, one MXU, one source-gated Transform block
  (D18 Revision 1 exposes Im2Col and explicitly refuses Col2Im), and
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

Before accelerator Phase 5, the project also delivers a deliberately smaller
single-hart **RISC-V VP++ Compiler Enablement VP**. It exists so the compiler
team can compile and execute RV32GCV programs without waiting for MXU,
Transform, dual-core composition or the mesh. It is a separate platform
deliverable, not a reduced NEO-CORE and not evidence that the final TPU_V3 SoC
is complete.

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

#### Why two cores per chip, and not one

Recorded 2026-08-19, after the question was asked and the frozen value turned
out to have no rationale written anywhere. Every other frozen number here does
— eight chips comes from the 3-bit FlooNoC manager id, one MXU per core comes
from D14 — and this one did not, which is why it was re-litigated instead of
looked up.

It is not derived from anything in this repository. **It comes from the
reference architecture being modelled.** The project-owner brief states
`02 TPU Core/Chip`, and the source figures show it directly:

```text
/home/duyptt_HW/Desktop/TPU_V3/docs/Hinh02.jpg   one chip frame containing
                                                 Core 0 and Core 1, each with
                                                 its own Scalar Unit, Vector
                                                 Unit, Matrix Multiply Unit and
                                                 Transpose/Permute Unit, both
                                                 attached to one shared
                                                 Interconnect Router whose four
                                                 Links leave for other chips
/home/duyptt_HW/Desktop/TPU_V3/docs/Hinh01.jpg   the same two symmetric compute
                                                 halves around one router on a
                                                 floorplan
/home/duyptt_HW/Desktop/TPU_V3/docs/Hinh03.jpg   **one core**, not a chip
```

The figures live outside this repository, like the VP++ translated paper named
in §8.2.

`Hinh03.jpg` is where the confusion comes from and is worth naming: it is the
block diagram of a *single* TPU core, so read on its own it looks like a whole
chip, and from there a mesh of one-core nodes looks like the obvious topology.
It is not; the chip boundary is the frame in `Hinh02.jpg`.

The mapping the model implements is therefore:

| Reference figure | Model |
| --- | --- |
| chip frame | `tpu_chip`, one mesh node |
| Core 0 / Core 1 | two NEO-COREs |
| Scalar Unit + Vector Unit | one VP++ RV32GCV hart (D14: they are one hart, not two) |
| Matrix Multiply Unit | MXU |
| Transpose / Permute Unit | Transform block *by position only* — see below |
| Interconnect Router + Links | chip-local fabric plus **one** aggregated NoC endpoint |

Two honest qualifications, so the mapping is not read as an equivalence claim:

* **The Transform block is not a transpose/permute unit.** It occupies the same
  place in the diagram, and that is all. Its operation is the Im2Col lowering
  traced from the pinned NPU-team v4.2 source (D18), which is a different
  function; `ARCHITECTURE.md` §4.2 already says RVV's own permutation
  instructions are unrelated to it.
* **Memory placement differs.** The figure attaches HBM per core inside the
  chip. The model gives each NEO-CORE its own core SRAM and puts
  `GLOBAL_RAM_OR_HBM` on a separate mesh node, which `ADDRESS_MAP.md` §5
  already states is simulated backing memory and not a model of TPU v3 HBM
  capacity or bandwidth.

A consequence worth keeping in view, because it is what makes the value cheap
to live with: with a fixed budget of eight NoC initiators, two cores per node
yields sixteen cores where one core per node yields eight. That is arithmetic,
not the original reason — the eight-initiator limit was a Phase 0 *finding*
about the existing wrapper (§9.1), discovered after this decision was already
frozen, so it cannot have motivated it.

Each TPU core contains:

```text
TPU Core / NEO-CORE
├── One RISC-V VP++ RV32GCV hart (Scalar + RVV)
├── One shared core SRAM
├── One independent TPU_V3 DMA
├── One MXU (64x64 bring-up from Sauria v4.2; 128x128 target)
├── One Transform block (Im2Col available; Col2Im unavailable in Revision 1)
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
They are unrelated to D14's Transform block: that accelerator performs the
source-gated tensor-layout Im2Col operation and has its own MMIO/TLM
contract. Col2Im is not an RVV substitute; it is simply unavailable under D18.

### 4.3 MXU geometry

There is exactly one MXU in each NEO-CORE. Its current implementation is
extracted from the pinned Sauria v4.2 source and is staged:

1. the current bring-up geometry is the verified v4.2 64x64 configuration;
2. the architectural destination is the NPU team's future 128x128 source.

Geometry is a construction-time reported property. A 64x64 build must not be
described as 128x128. A 128x128 request must be rejected until the new source,
adapter, golden regression and scalability gate pass.

### 4.4 NoC attachment policy

The two TPU cores in one chip connect to a chip-local fabric. The chip-local
fabric aggregates traffic into one NoC endpoint per chip.

Do not expose the hart, MXU, Transform, DMA and SRAM as unrelated NoC managers.
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
- exact datatype available in the 64x64 MXU bring-up from Sauria source and the NPU team's
  128x128 delivery; D6 BF16 x BF16 -> FP32 remains the target contract;
- MXU pipeline depth, clock rate, and detailed latency formula;
- CPU, MXU, Transform, DMA, SRAM, and NoC clock ratios;
- cache presence and cache coherence policy;
- final DMA descriptor depth, burst policy and timing constants (ownership is
  frozen per-core and independent of Sauria by D14);
- future NPU-team Col2Im source revision, layout and overlap/accumulation
  contract. D18 has frozen the Revision 1 Im2Col source, CHW/row-major layout,
  INT8 datatype, stride/dilation and no-padding subset (all fields zero);
- final workload set and image input format.

Earlier ideas such as a fixed 4x4 mesh, exactly 32 TPU cores, or a mandatory
four-image workload are not architectural requirements in this revision. They
may be introduced later as named configurations or workloads after approval.

> **Partly resolved on 2026-08-08** by `TPU_V3_DECISION_RECORD.md`. Still
> configurable, but now with an approved reference value rather than a
> placeholder: core SRAM capacity per core (16 MiB reference, D6), MXU target
> arithmetic (BF16 × BF16 → FP32, D6), global RAM capacity (256 MiB bring-up
> default, D6). D14 settles DMA ownership and the NEO-CORE composition; cache
> policy remains open. A 32-core system is explicitly **outside Revision 1**
> and requires the
> coordinated change listed in D2. See §23 for the current state of each.

## 6. Terminology

Use these architectural names consistently in documentation, diagrams and
reports. Existing implementation/ABI identifiers are the explicit D20
exception described below:

| Term | Meaning |
| --- | --- |
| `TPU_V3 SoC` | Complete SystemC/TLM platform executable |
| `TPU chip` | One NoC node containing exactly two TPU cores |
| `NEO-CORE` / `TPU core` | One VP++ hart, one SRAM, one independent DMA, one MXU, one Transform block, AXI4-Lite control and a native local-SRAM data fabric |
| `Scalar core` | Scalar execution portion of the RISC-V VP++ RV32GCV hart |
| `VPU` | RVV 1.0 vector execution portion of the same RISC-V VP++ hart |
| `MXU` | Architectural matrix-multiplication unit; current 64x64 implementation comes from pinned Sauria v4.2 source, with a 128x128 target |
| `Core SRAM` | Shared local memory in one NEO-CORE; successor name for the old SVM contract |
| `Transform` | Architectural tensor-layout block; Revision 1 exposes pinned Im2Col while Col2Im remains an independently gated, unavailable future capability |
| `NEO DMA` | Independent TPU_V3 DMA; never the Sauria DMA |
| `NEO control fabric` | 32-bit AXI4-Lite MMIO decoder, represented at transaction level |
| `NEO Local SRAM Fabric` | Native pipelined request/response fabric with per-bank arbitration; not AXI |
| `NEO external bridge` | Bidirectional adapter for outbound VP++/DMA and inbound remote traffic at the AXI4/chip-NoC boundary |
| `NEO hart port` | The hart's single TLM attachment point; decodes one combined VP++ socket onto the native local plane, AXI4-Lite control and the external bridge |
| `Chip local fabric` | Arbitration and decode between two cores and the NoC endpoint |
| `NoC endpoint` | Bidirectional interface between one TPU chip and one mesh router |
| `Fast model` | Functionally correct, approximately timed or untimed model |
| `Detailed model` | More detailed timing model with explicitly stated evidence |

`Sauria` is a source/backend provenance name, not the architectural block name;
`Im2Col` is a Transform operation, not the Transform block name. Existing code
and ABI identifiers such as `sauria_matrix`, `image_transform` and
`SA_CONTROL` remain retained implementation names under D20.

Avoid using the unqualified word `core` when it could mean RISC-V hart,
NEO-CORE, the source NPU core, or an MXU processing element.

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
                 core regs  NEO DMA      MXU       Transform
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
to the native SRAM plane and register operations to AXI4-Lite. MXU, DMA and
Transform are asynchronous AXI4-Lite-controlled engines; their local bulk
data uses native ports. The independent DMA owns bulk external movement. VP++
also crosses the external boundary for boot-ROM instruction fetch and explicit
global accesses; MXU and Transform do not.

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

### 8.3 MXU implementation source/backend (Sauria v4.2)

External source example:

```text
/home/duyptt_HW/Documents/work/fx1/hw/tlm/MP1_V1.1/v4.2_model
```

Existing CDC-VP integration:

```text
/home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP/components/npu_tlm
```

Use these as:

- the implementation source for the matrix-multiply-only MXU adapter;
- the pinned evidence source for D18 Im2Col, and for Col2Im only if the NPU
  team later identifies and verifies that independent function;
- a reference for the minimum feeder/sequencer/result-collection logic required
  by matrix multiplication;
- golden/case data for source-to-adapter equivalence tests.

The v4.2 tree is an NPU top and cannot be reused unchanged. A source audit on
2026-08-14 established that its geometry is parameterized rather than fixed:
`NpuTop` and `sauria_types.h` default to 32x32, the legacy CDC-VP
`components/npu_tlm` wrapper explicitly instantiates a 32x32 floating-point
top, while `sauria_targets.h`, the Makefile and captured cases provide verified
32x32 and 64x64 profiles. The source includes `int8_64x64` and `FP16_64x64`,
but no 128x128 profile. The bare `make eval` defaults to a smaller 16x8
evaluation geometry and is not evidence for the Phase 5 configuration.

The Phase 5 bring-up target is explicitly `int8_64x64`: X=64 columns, Y=64
rows, INT8 activation/weight, INT32 accumulation/output, packed index widths
18/18/17 and 64 KiB A/B/C evaluation regions as selected by `GEO=64x64` in the
source Makefile. The adapter must pass these parameters at construction and
report them at runtime; it must never inherit the 32x32 template defaults. Use
`demo_gemm_64x64` as the source golden case. The source ViT program was audited
after the minimal path passed, but its nine matrix operations use an emulation
branch and leave the MXU at zero cycles/MACs/utilisation; it is therefore not an
array golden. Phase 5 uses a 64x64x64 matrix projection with the same
deterministic ViT operand recipe and an independent INT32 oracle, and refuses
the untiled 64x64x192 QKV shape explicitly.

The matrix adapter excludes `NpuTop`, instruction/profile routing, instruction
decoder, OBP/RCE, Sauria DMA and unrelated NPU-top behavior. It retains the
source `ConfigRegs` only as the internal signal-level distributor required by
the selected matrix closure; TPU_V3's separate `SA_CONTROL` remains the sole
firmware interface. The NPU team will provide the 128x128 extension later;
requesting 128x128 against v4.2 remains an error.

The Phase 6 audit traced the IFMAP address generation, CHW layout and
cross-correlation golden into the D18 standalone Im2Col adapter. It found no
Col2Im block or overlap contract. Do not guess that behavior: keep its
capability zero until the NPU team supplies source, configuration semantics and
golden tests. See `TPU_V3_PHASE6_AUDIT.md`.

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
- never wait for MXU, DMA or Transform completion inside MMIO `b_transport`.

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
│       ├── neo_dma/
│       │   ├── CMakeLists.txt
│       │   ├── DMA_MODEL.md
│       │   ├── include/tpu_v3/dma/neo_dma.h
│       │   ├── include/tpu_v3/dma/dma_registers.h
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
│       │   ├── IMAGE_TRANSFORM_MODEL.md
│       │   ├── include/tpu_v3/transform/im2col.h
│       │   ├── include/tpu_v3/transform/image_transform.h
│       │   ├── include/tpu_v3/transform/image_transform_registers.h
│       │   ├── src/im2col.cpp
│       │   ├── src/image_transform.cpp
│       │   └── tests/
│       ├── tpu_core/
│       │   ├── CMakeLists.txt
│       │   ├── include/tpu_v3/core/tpu_core.h
│       │   ├── include/tpu_v3/core/neo_control_fabric.h
│       │   ├── include/tpu_v3/core/neo_local_sram_fabric.h
│       │   ├── include/tpu_v3/core/local_sram_fabric_config.h
│       │   ├── include/tpu_v3/core/neo_external_bridge.h
│       │   ├── include/tpu_v3/core/neo_hart_port.h
│       │   ├── include/tpu_v3/core/core_registers.h
│       │   ├── src/tpu_core.cpp
│       │   ├── src/neo_control_fabric.cpp
│       │   ├── src/neo_local_sram_fabric.cpp
│       │   ├── src/neo_external_bridge.cpp
│       │   ├── src/neo_hart_port.cpp
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
>
> Added by Phase 4: `neo_dma/`, holding the component and `DMA_MODEL.md`. The
> directory is `neo_dma/` rather than the `dma/` this sketch first showed, so
> that nothing in the tree reads as a second copy of the shared
> `components/dma_tlm`; §11.5 names the same path. Its register map lives in
> `dma_registers.h` rather than a `dma_config.h`, because the configuration
> that matters — geometry, burst bound, apertures — is a handful of fields on
> the module while the frozen part is the programming model.
>
> Added by Phase 7: `tpu_core/neo_hart_port.{h,cpp}` and `tpu_core/tpu_core.{h,cpp}`.
> `neo_hart_port` was missing from every earlier version of this sketch, which
> is worth recording rather than quietly inserting: the three D15 planes were
> all listed, and the component that connects the hart to them was not, because
> the split reads as if the hart already speaks three protocols. It speaks one
> — VP++ has a single combined fetch/data socket — and `neo_local_sram_fabric`
> exposes no TLM target, so nothing in the Phase 3 tree can bind to the CPU
> without it. §11.7 specifies it.

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
    bool im2col_available;          // enabled at Phase 7 with D18 exact pin
    bool col2im_available;          // false until a future promotion gate
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
other than one MXU/DMA/Transform per core, geometry other than verified 64x64 or
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
independent DMA, the MXU, the Transform block and
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

### 11.4 MXU

Define a geometry-aware `sauria_matrix_if` for the MXU before extracting
Sauria source. It is one engine per core and implements matrix multiplication
only.

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
native local-SRAM requester port; the MXU is not an external AXI4/NoC master.
MMIO start must enqueue work and return. Completion occurs asynchronously.

Decision D17 fixes the adapter as buffered tile staging: prefetch A/B through
the native port, run the source against private fixed-latency stores, then
write C back through the same port. Phase 5 accepts one tile with `M,N <= 64`.
SRAM-B consumes one physical 64-lane vector per K step and zero-pads inactive
N lanes; flattening `K*N` across vectors is forbidden.

The 64x64 adapter is built first from the verified v4.2 source. It retains only
the MXU PE array and minimum required feeder/sequencer/result collector, plus
the source `ConfigRegs` as an internal configuration distributor. It must
exclude `NpuTop`, Sauria DMA, instruction/profile routing, instruction decoder,
OBP/RCE and other unrelated NPU-top behavior. Source-to-adapter regression must
compare results against the same Sauria golden cases at the same geometry and
datatype.

The 128x128 promotion keeps this firmware-visible interface unchanged. It must
audit masks and counters wider than 64, address arithmetic, memory capacity,
elaboration cost, reset and datatype behavior. The NPU-team source revision is
recorded in the build manifest.

### 11.5 Independent NEO DMA

The DMA is owned by TPU_V3, not Sauria. It is a new TPU_V3 component at
`components/TPU_V3/neo_dma`; it is **not** the repository-wide
`components/dma_tlm` component under a different name.

#### Existing `components/dma_tlm` review and reuse boundary

The existing model was reviewed on 2026-08-13. Its GCC 11.5 / SystemC 2.3.4
baseline test passes, but its architecture is a PL330-style, eight-channel,
32-event, microprogrammed DMA with `DMAMOV`/`DMALD`/`DMAST`, debug-command
launch and one generic TLM master socket. That is useful prior art, but it is
not the NEO-CORE programming or data-path contract.

`components/dma_tlm` remains unchanged because `noc_soc`,
`VP_FX1_Full_SoC` and the existing DMA platform use it. `neo_dma` must not:

- inherit from, contain, wrap or link `cdc::components::dma_tlm`;
- expose its PL330 register map, channel programs, debug launch path, MFIFO or
  event-vector IRQ model;
- route local SRAM traffic through its single generic TLM master socket.

The new implementation may reuse only generic design patterns: an event-driven
worker `SC_THREAD`, correctly formed TLM initiator payloads, response checking,
and the existing CMake/testbench style. If common helper code is ever extracted,
it must have no PL330 state or behavior and both original-platform regressions
must remain green.

#### Revision 1 public boundary

There is one DMA instance and one descriptor slot per NEO-CORE. There is no
descriptor queue, scatter/gather list, microcode engine or multiple DMA channel
model in Revision 1. Its public boundary is:

```cpp
tlm_utils::simple_target_socket<neo_dma> control;
sc_core::sc_port<sram::neo_local_sram_if> local;
tlm_utils::simple_initiator_socket<neo_dma> external;
sc_core::sc_out<bool> irq;
void reset();
```

- `control` is an absolute-address, 32-bit AXI4-Lite TLM target bound behind
  `neo_control_fabric` at `DMA_CONTROL`.
- `local` issues `neo_local_request` with requester identity
  `neo_requester::dma`; it never sees or stores a backing-memory pointer.
- `external` carries only non-local traffic through `neo_external_bridge` and
  the chip/NoC endpoint.
- `irq` is one level signal. It is asserted for successful completion or error
  while enabled and remains asserted until the corresponding W1C status is
  acknowledged. Reset/abort does not masquerade as successful completion.
- `reset()` is called by the NEO-CORE's synchronous hierarchical reset path;
  the DMA does not invent an independent reset domain.

All public addresses and address arithmetic use `std::uint64_t`, even though
the Revision 1 map occupies the RV32 4 GiB space. Length and individual native
request sizes remain explicitly bounded before narrowing.

#### Revision 1 programming model

The first implementation freezes the following 32-bit register layout within
the 64 KiB `DMA_CONTROL` window. Unlisted offsets are reserved and never alias
implemented registers.

| Offset | Register | Access and semantics |
| ---: | --- | --- |
| `0x000` | `ID` | RO, NEO DMA identity |
| `0x004` | `VERSION` | RO, programming-model version |
| `0x008` | `CONTROL` | W1S: bit 0 `START`, bit 1 `ABORT` |
| `0x00C` | `STATUS` | RO/W1C: bit 0 `BUSY`, bit 1 `DONE`, bit 2 `ERROR`, bit 3 `ABORTED` (`BUSY` is RO) |
| `0x010` | `SRC_ADDR_LO` | RW while idle |
| `0x014` | `SRC_ADDR_HI` | RW while idle |
| `0x018` | `DST_ADDR_LO` | RW while idle |
| `0x01C` | `DST_ADDR_HI` | RW while idle |
| `0x020` | `LENGTH` | RW while idle; non-zero byte count |
| `0x024` | `IRQ_ENABLE` | RW; bit 0 enables completion/error IRQ |
| `0x028` | `ERROR_CAUSE` | RO, latched first error |
| `0x02C` | `BYTES_DONE_LO` | RO, destination bytes committed for the current/last job |
| `0x030` | `BYTES_DONE_HI` | RO |
| `0x034` | `TRANSFER_COUNT` | RO, accepted jobs |
| `0x038` | `ERROR_COUNT` | RO |
| `0x03C` | `ABORT_COUNT` | RO |
| `0x040` | `OVERRUN_COUNT` | RO, rejected `START` while busy |
| `0x044` | `LOCAL_BYTES_LO` | RO, native local-SRAM bytes moved |
| `0x048` | `LOCAL_BYTES_HI` | RO |
| `0x04C` | `EXTERNAL_BYTES_LO` | RO, external-path bytes moved |
| `0x050` | `EXTERNAL_BYTES_HI` | RO |

`ERROR_CAUSE` values are stable in Revision 1: `0` none, `1` invalid/zero
length, `2` address overflow, `3` unsupported endpoint combination, `4` source
unmapped/straddling, `5` destination unmapped/straddling, `6` local read,
`7` local write, `8` external read, `9` external write, and `10` internal model
failure. Reset and explicit abort are not reported as transfer errors.

The target accepts exactly one naturally aligned 4-byte access with full
strobes, rejects wrapped streaming payloads, sets `dmi_allowed=false`, and sets
a response on every return path. It also implements `transport_dbg` with the
same absolute-address decode and bounds, but without workload counters or
timing. An address at `DMA_CONTROL + 0x1000` must not alias offset zero.

Writing `START` snapshots the descriptor, sets `BUSY`, notifies the worker and
returns without waiting for data movement. Descriptor validation is the first
worker action; an invalid descriptor therefore completes as an asynchronous
error under the same status/IRQ contract as a downstream failure. `BUSY` is
visible before the register transaction returns. Descriptor writes and a
second `START` while busy return `TLM_GENERIC_ERROR_RESPONSE`; a second start
additionally increments `OVERRUN_COUNT` and may not overwrite the active
snapshot. A write that requests `START` and `ABORT` together is rejected with
no state change. The worker owns all long-running reads, writes and simulated
waits.

#### Supported routes and transfer semantics

Revision 1 supports exactly two directions, inferred from absolute address
classification rather than from a redundant direction bit:

1. core-local SRAM → external/chip/global/NoC-visible memory;
2. external/chip/global/NoC-visible memory → core-local SRAM.

A descriptor with both endpoints local, both endpoints external, an unmapped
endpoint, integer overflow, or a source/destination span crossing a region
boundary is rejected with a defined descriptor/address error. Supporting
local-to-local, external-to-external or scatter/gather is a later explicit
revision, not behavior to infer in Phase 4.

The worker uses a bounded staging buffer of at most one legal external frame.
Local requests are split into 1..64-byte native accesses. External requests
obey the current 8-byte/256-beat limit:
`ceil((address % 8 + length) / 8) <= 256`; therefore the maximum payload is
2048 bytes only when aligned and is smaller at a lane offset. Chunks are issued
in ascending address order and never cross the source or destination region.

`BYTES_DONE` counts destination bytes committed, not source bytes fetched or
payload bytes merely named. A successful boundary response and its byte count
are recorded before the worker consumes the returned annotated delay, so a
reset during that delay cannot hide memory effects that already occurred. The
first failing local/native or external TLM chunk stops the job, latches the
origin-specific `ERROR_CAUSE`, leaves earlier destination chunks committed,
sets `ERROR`, clears `BUSY` and asserts the level IRQ when enabled. No later
chunk may issue. Local/external request, byte, error, chunk and latency counters
remain separately attributable; no beat count is relabelled as a request or
hardware transaction count. On success both path byte totals equal `LENGTH`.
On error/abort they need not equal: each equals its own successful boundary
transactions, while `BYTES_DONE` equals only committed destination bytes and
any source data still in the staging buffer is explicitly not completion.

`reset()` and explicit `ABORT` advance a job generation so an old worker cannot
resume under a new state epoch. Reset during an active job clears `BUSY`,
`DONE`, `ERROR` and `ABORTED`, deasserts IRQ, increments `ABORT_COUNT`, releases
admission and prevents further chunks. Explicit abort clears `BUSY`, sets the
sticky `ABORTED` bit, increments `ABORT_COUNT` and does not assert the
completion/error IRQ. Bytes committed before either form of abort remain
committed and are reported in `BYTES_DONE`. An active-job reset keeps that
job-local committed count *and its ownership of the register*, so a native or
external access still in flight can still report the bytes it committed —
"snapshot" understates it, because the count may still rise until a new
`START` claims the register. A reset with no active job initializes it to
zero. Path traffic counters follow the opposite rule: `reset()` clears them and
opens a new counter epoch, and a request from the closed epoch must not
re-populate them, matching what `neo_local_sram_fabric` already does with
old-generation responses. Conservation between the two is therefore a
per-epoch property. Neither path attempts rollback. A request
blocked in the native fabric must observe its `aborted` response and unwind. An
old job must not write completion/error state or counters into a new epoch.

Required contract summary:

- one 32-bit AXI4-Lite MMIO target, one native local-SRAM requester port and
  one external AXI4/NoC-facing initiator per core;
- source/destination address, length, control, busy/done/error and level IRQ;
- legal burst chunking, deterministic partial-error semantics and byte counts;
- no direct pointer to core SRAM/global memory and no Sauria DMA dependency;
- all traffic observes the D15 local-fabric and NoC endpoint constraints;
- reset aborts active transfer and releases all accounting.

### 11.6 Transform block

One engine per core has independently reported transform capabilities. D18
freezes Revision 1 Im2Col: signed INT8 contiguous CHW input, row-major
`[OH*OW][C*KH*KW]` output, `c,ky,kx` column order, stride/dilation and all four
padding values equal to zero. Required registers include source/destination,
dimensions, kernel, stride, dilation, padding, operation/datatype, derived
shape, status/error, IRQ and byte/request counters.

The engine is a 32-bit AXI4-Lite target and a native local-SRAM requester. It
stages the input through native transactions and writes matrix rows through the
same port, with no external master or SRAM backing pointer. This staging is a
functional TLM implementation and not a claim about RTL line-buffer timing.

No standalone Col2Im block or overlap/accumulation contract exists in v4.2.
The capability stays zero and a selected Col2Im `START` must complete as
`unavailable_operation` without SRAM traffic. Do not implement a guessed
inverse or relabel PSM output addressing. Lack of Col2Im does not block forward
inference: after MXU, RVV can post-process and interpret the output-feature
matrix. See D18, `TPU_V3_PHASE6_AUDIT.md` and
`image_transform/IMAGE_TRANSFORM_MODEL.md`.

### 11.7 NEO control, local-SRAM and external fabrics

The D15 split is an architectural RTL contract, not only a SystemC naming
convention.

**AXI4-Lite control fabric responsibilities:**

- decode 32-bit VP++ and authorized inbound MMIO accesses to core, DMA, MXU,
  Transform and counter registers;
- remain in order with no bursts or AXI IDs and accept at most one transaction
  per control initiator;
- enforce 4-byte alignment, full strobes and response propagation;
- be represented at transaction level without claiming channel-cycle accuracy.

**NEO Local SRAM Fabric responsibilities:**

- decode VP++ local data and native DMA/MXU/Transform requests to the
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
- keep MXU and Transform as local-SRAM requesters, not NoC masters.

**NEO hart port responsibilities:**

The hart does not attach to three planes; it attaches to one socket. RISC-V
VP++ exposes a single `CombinedMemoryInterface`, so `instr_bus()` and
`data_bus()` return the **same** TLM initiator socket, while
`neo_local_sram_fabric` exposes only `sc_export<neo_local_sram_if>` and no TLM
target at all. Something has to sit between them, and it is a component in its
own right rather than a few lines inside `tpu_core`:

- present one TLM target socket to the VP++ backend, carrying instruction
  fetch, scalar and vector data, and MMIO alike;
- decode each access by absolute address into exactly one destination: core
  SRAM to the native local plane as `neo_requester::cpu`, core-local MMIO to
  the AXI4-Lite control fabric, everything else to the external bridge's
  `local_outbound`. An access that straddles two of them is refused, not split;
- **expand TLM byte enables to one byte per data byte** before issuing a native
  request. `INTERFACE_CONTRACT.md` §2 was written for this boundary: TLM lets a
  short pattern repeat, native `wstrb` does not, and forwarding
  `byte_enable_ptr` unchanged both reads past the initiator's array and applies
  the wrong mask;
- never `wait()`; it is a TLM target on the hart's path and D16 binds it;
- attribute every request to `neo_requester::cpu` and keep D7 counter naming —
  it sees element-wise vector traffic and must not reassemble it into anything;
- convert native `neo_status` into TLM response status without inventing
  success, and propagate `aborted` from a reset in flight.

Naming: `neo_hart_port`, following `neo_control_fabric` /
`neo_local_sram_fabric` / `neo_external_bridge`. It is the hart's single
attachment point, so "port" is accurate; `neo_cpu_adapter` was considered and
rejected because "adapter" is already what the Sauria SRAM shim is called.

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
- one MXU;
- one Transform block;
- one 32-bit AXI4-Lite control fabric;
- one native banked-SRAM data fabric;
- one bidirectional external AXI4/NoC bridge;
- one NEO hart port, decoding the hart's single TLM socket onto those three;
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
- tests exercise simultaneous MXU/DMA/Transform/external-inbound and
  dual-core requests.

## 14. Accuracy and Performance Strategy

The platform needs multiple fidelity levels.

### 14.1 Full-system fast mode

Use for firmware, software integration, multi-chip workloads, and long runs.

- RISC-V VP++ functional RV32GCV execution;
- extracted MXU at its reported geometry/datatype and Sauria source revision;
- independent DMA and verified Transform capabilities;
- approximately timed AXI4-Lite control and native banked-SRAM fabrics;
- FlooNoC fast timing when contention is not the study target;
- deterministic results and metrics.

### 14.2 NoC detailed mode

Use for routing, arbitration, link back-pressure, and contention experiments.

- detailed FlooNoC backend;
- nonblocking/annotating targets;
- small or controlled workloads;
- no claim that CPU/MXU/Transform/DMA timing is cycle accurate unless separately calibrated.

### 14.3 MXU promotion/detail mode

Use for selected single-MXU or single-core experiments and for promoting the NPU
team's 128x128 source.

- MXU behavior only at verified geometry/datatype/source revision;
- explicit MXU-only extraction scope and array mapping;
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

option(CDC_BUILD_TPU_V3_SAURIA_MATRIX
    "Build the TPU_V3 MXU from pinned Sauria v4.2 source" OFF)

option(CDC_BUILD_TPU_V3_IMAGE_TRANSFORM
    "Build the pinned TPU_V3 Transform adapter with Im2Col" OFF)

# Phase 4.5. Deliberately not conditioned on CDC_BUILD_TPU_V3_SOC: the compiler
# handoff contains no NoC, no accelerator and no NEO composition, so requiring
# the SoC option to build it would make the package depend on parts it must not
# contain -- and would make a compiler team's build break for reasons that have
# nothing to do with them.
option(CDC_BUILD_RISCV_VPP_COMPILER_VP
    "Build the RISC-V VP++ Compiler Enablement VP handoff platform" OFF)

set(TPU_V3_SA_GEOMETRY "64x64" CACHE STRING
    "TPU_V3 MXU geometry: 64x64 or promoted 128x128")
```

Add a pinned RISC-V VP++ source path/cache variable with a clear failure if
missing. Keep the optional Spike reference path separate so a production
runtime build cannot accidentally acquire a Spike dependency. The TPU_V3 MXU
target consumes selected headers/source through its own adapter and must not
link `cdc::components::npu_tlm` or instantiate `NpuTop`. The existing
`CDC_ENABLE_SAURIA_NPU_V4` option remains for legacy platforms and is not the
NEO-CORE matrix-selection switch.

### 15.2 Component targets

Expected exported targets:

```text
cdc::components::tpu_v3_common
cdc::components::tpu_v3_native_port
cdc::components::tpu_v3_core_sram
cdc::components::tpu_v3_neo_dma
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
riscv_vpp_compiler_vp
riscv_vpp_compiler_vp_package
tpu_v3_soc
tpu_v3_soc_package
```

Use:

```cmake
cdc_make_portable(riscv_vpp_compiler_vp)
cdc_package_platform(riscv_vpp_compiler_vp)
cdc_make_portable(tpu_v3_soc)
cdc_package_platform(tpu_v3_soc)
```

`riscv_vpp_compiler_vp` is the Phase 4.5 single-hart compiler handoff. It links
the same `cdc::cpu::riscv_vp_plusplus` backend that later goes into every
NEO-CORE, but it does not link FlooNoC, MXU, Transform, the NEO DMA or
the NEO local/control fabrics. `tpu_v3_soc` remains the final multi-core/multi-
chip platform; neither target is an alias for the other.

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
$CC -dumpfullversion
$CXX --version | head

export TPU_V3_SAURIA_ROOT=/home/duyptt_HW/Documents/work/fx1/hw/tlm/MP1_V1.1/v4.2_model

cmake -S . -B build-tpu-v3 \
    -DCDC_BUILD_TPU_V3_SOC=ON \
    -DCDC_BUILD_TPU_V3_TESTS=ON \
    -DCDC_BUILD_TPU_V3_SAURIA_MATRIX=ON \
    -DTPU_V3_SAURIA_ROOT="$TPU_V3_SAURIA_ROOT" \
    -DTPU_V3_SA_GEOMETRY=64x64

cmake --build build-tpu-v3 \
    --target tpu_v3_soc tpu_v3_sauria_matrix \
             test_sauria_profile test_sauria_two_instances \
             test_tile_staging_store test_gemm_config \
             test_matrix_composition test_gemm_staged test_gemm_adapter \
             test_config_loader test_sa_control test_adapter_epoch \
             test_two_adapters \
    --clean-first \
    -j"$(nproc)"

ctest --test-dir build-tpu-v3 -L sauria --output-on-failure

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

The earlier compiler-team handoff is a separate package:

```text
out/riscv_vpp_compiler_vp/
├── riscv_vpp_compiler_vp
├── libsystemc.so*
├── configs/
├── sdk/
│   ├── Makefile
│   ├── common/{crt0.S, link.ld, link.ld.in, host_io.c, host_io.h}
│   ├── include/compiler_vp/host_io_map.h
│   └── examples/{scalar_hello, rvv_vector_add}/main.c
├── examples/
│   ├── scalar_hello/
│   └── rvv_vector_add/
├── docs/
│   ├── COMPILER_QUICKSTART.md
│   ├── ISA_ABI_CONTRACT.md
│   └── MEMORY_MAP.md
├── licenses/
│   ├── Apache-2.0.txt, CDC-VP-NOTICE.txt, THIRD_PARTY.md
│   ├── RISCV-VP-PLUSPLUS.MIT.txt
│   └── BERKELEY-SOFTFLOAT-3d.BSD-3-Clause.txt
└── BUILD_MANIFEST.json
```

The last two are not optional and not documentation. The executable statically
links the RISC-V VP++ ISS (MIT) and the Berkeley SoftFloat release vendored
inside it (BSD-3-Clause), and both licences require their notice to accompany a
binary distribution. SoftFloat ships no standalone licence file upstream — the
notice is the header of each of its source files — so the package extracts it
from a file that was actually compiled rather than carrying a transcription that
nothing compares against the code.

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
  renames these to core SRAM and one MXU and supersedes the old composition.

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
- Route synthetic CPU, MXU, DMA and Transform native requesters to SRAM and
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

- Create `components/TPU_V3/neo_dma` and its `DMA_MODEL.md`; do not modify or
  link the shared PL330-style `components/dma_tlm`.
- Implement the §11.5 single-descriptor register model, absolute 64 KiB decode,
  AXI4-Lite validation and side-effect-free debug transport.
- Bind one native local-SRAM port as `neo_requester::dma` and one external TLM
  initiator; implement only local→external and external→local in Revision 1.
- Snapshot the descriptor on `START`, set `BUSY` before returning, and execute
  the transfer in an event-driven worker `SC_THREAD`.
- Implement overflow-safe route/span validation, 1..64-byte local chunking and
  external chunks respecting the 8-byte/256-beat frame formula.
- Stop on the first error with destination-committed `BYTES_DONE`, a latched
  origin-specific cause and no transaction after the failing chunk.
- Implement level completion/error IRQ, W1C acknowledgement, start-while-busy
  rejection, overrun accounting, explicit abort and reset generation handling.
- Add source/build guards proving no `dma_tlm.h`, `cdc::components::dma_tlm`,
  Sauria DMA header/symbol or direct SRAM/global backing pointer is used.
- Keep the existing `dma_tlm`, `noc_soc`, `VP_FX1_Full_SoC` and DMA-platform
  regressions green as non-regression evidence for the reuse boundary.

#### Required tests

- Every register's reset/access/W1C behavior; exact 4-byte aligned MMIO, full
  strobes, invalid command, wrapped streaming, null pointer and 64 KiB bounds.
- A negative alias control at `DMA_CONTROL + 0x1000` and normal/debug agreement.
- Local→external and external→local copies at lengths 1, 2, 3, 7, 8, 15, 16,
  63, 64, 65, an external-frame edge, and a multi-frame transfer.
- Odd addresses, local/native 64-byte boundaries, external lane offsets,
  top-of-region spans, overflow and region-straddling descriptors.
- Observable requester attribution: every local access is
  `neo_requester::dma`; every non-local access reaches the external stub and no
  local byte leaks there.
- A failing destination on a later chunk proves first-error propagation,
  committed-byte accounting and absence of transactions after failure.
- Reset/abort before start, during incoming delay, during native arbitration,
  after partial completion and during an external transaction; all cases run
  under a watchdog and prove the old job cannot publish into the new epoch.
- Concurrent synthetic CPU/MXU/Transform pressure on the local fabric proves DMA
  back-pressure, response ownership and lack of direct-memory bypass.
- START-while-busy and descriptor-write-while-busy negative controls; success
  and error IRQ remain level until W1C, while reset deasserts IRQ without DONE.
- Counter conservation: successful transfer bytes reconcile at source,
  destination, local fabric and external path; partial/error/abort cases
  reconcile each path against its successful transactions and count only
  destination-committed bytes in `BYTES_DONE`.

#### Gate

- DMA copies only through observable native local-SRAM transactions and the
  external AXI4/NoC-facing path; successful jobs have equal path byte totals,
  while partial jobs reconcile each total against the successful transactions
  actually observed on that path.
- A negative control using a failing destination proves response propagation and
  partial-byte accounting.
- No direct SRAM/global backing pointer and no `control/sauria_dma.h` dependency
  exists.
- MMIO start returns immediately; completion/error IRQ is level-sensitive.
- The shared `components/dma_tlm` has no TPU_V3-specific changes and all of its
  existing consumers still build and pass their regressions.
- Release and Debug TPU_V3 suites, packaging regression and watchdog cases pass
  with zero unexplained skip or hang.

### Phase 4.5: RISC-V VP++ Compiler Enablement VP

**Complete (2026-08-14).** Delivered as `platforms/riscv_vpp_compiler_vp` and
`fw/riscv_vpp_compiler_vp`, behind `CDC_BUILD_RISCV_VPP_COMPILER_VP`. See §17
for what was built and what was found; `platforms/riscv_vpp_compiler_vp/docs/MEMORY_MAP.md`
is the reference this section calls for, and it is the authority on the map
wherever it and this section differ.

One finding is worth reading before writing another platform: the two watchdogs
required below are **not sufficient on their own**, and the shortfall is
ordinary rather than exotic. Both are polled between slices of `sc_start()`, so
neither can end a run whose hart has stopped returning to the SystemC kernel —
and an image whose entry point lands on memory it never wrote does exactly that,
trapping, vectoring to an `mtvec` its startup never set, and faulting on the
fault. It retires nothing and never reaches a quantum boundary, so both bounds
are armed and neither is ever read again. Any later phase that runs guest code
under a time or instruction budget inherits this.

This phase is an explicit project-owner priority before Phase 5. It turns the
portable Phase 2 backend proof into a supported compiler-team deliverable. It
does **not** add a second processor and does **not** compose a NEO-CORE.

#### Architectural boundary

The platform contains exactly one architectural hart:

```text
RISC-V VP++ Compiler Enablement VP
├── one RISC-V VP++ RV32GCV hart
│   ├── scalar execution: RV32 IMAFDC + Zicsr + Zifencei
│   └── vector execution: RVV 1.0, VLEN=512, ELEN=64, vlenb=64
├── one SystemC/TLM address decoder
├── program/data RAM
└── simulator-only control target
    ├── character output
    ├── exit/pass/fail
    └── diagnostic status
```

Scalar and vector execution are parts of the **same** VP++ hart. They share PC,
GPRs, vector registers, CSRs, privilege/trap state, `mhartid` and one TLM memory
path. There is no scalar-to-vector MMIO command, second hart, vector doorbell or
independent vector memory master.

The CPU is named **RISC-V VP++ RV32GCV hart**. It is the functional SystemC/TLM
instruction-set model in `cpu_models/riscv_vp_plusplus`, based on the pinned
`ics-jku/riscv-vp-plusplus` source and approved patch series from Phase 2. It is
not Rocket, BOOM, CV32E40P, VexRiscv or any other named RTL microarchitecture,
and the package must not imply pipeline- or cycle-accurate behavior.

The compiler contract is frozen to:

```text
architecture string : rv32gcv_zvl512b
ABI                 : ilp32d
XLEN                : 32
RVV                 : 1.0
VLEN                : 512 bits
ELEN                : 64 bits
vlenb               : 64
hart count          : 1
execution profile   : bare-metal / freestanding
```

The exact compiler-VP memory and host-I/O register layout must be written once
in `platforms/riscv_vpp_compiler_vp/docs/MEMORY_MAP.md` and shared with firmware
through one generated or common header. The initial profile shall place
program/data RAM at the TPU_V3 global-RAM base `0x8000_0000`, honour the ELF
entry point, and reserve a clearly labelled **simulator-only** host-I/O window
inside the currently unmapped low-address space. Reuse the existing Phase 2
exit protocol where practical; if its `0x000F_0000` window is retained, extend
that one contract rather than inventing a second exit address. The host-I/O
window is not a TPU_V3 architectural peripheral and must not leak into the
full-SoC firmware ABI.

#### Explicit non-goals

- No FlooNoC or other mesh is instantiated or linked. A VP does not require a
  NoC to execute an ELF; the mesh remains a Phase 9 full-SoC concern.
- No MXU, Transform, NEO DMA, core SRAM, NEO local fabric, NEO control
  fabric or NEO external bridge is instantiated.
- No second hart, Linux, CLINT, PLIC, MMU, cache, GUI, Qt or VNC platform is
  introduced.
- No libc/newlib dependency is required for the first handoff. `Hello World`
  uses a documented freestanding MMIO `putchar`/`puts` shim. Supporting hosted
  `printf`, syscalls or an OS is a later and separately gated feature.
- Spike remains a standalone development oracle. It is not linked, copied or
  invoked by the compiler-team runtime package.
- The package does not model target TPU pipeline timing, memory bandwidth or
  NoC latency and must not publish such numbers.

#### Repository and build tasks

- Add an independent top-level option
  `CDC_BUILD_RISCV_VPP_COMPILER_VP`, default `OFF`, so building this handoff does
  not require `CDC_BUILD_TPU_V3_SOC`, Sauria or a NoC build.
- Create `platforms/riscv_vpp_compiler_vp` with a platform top, CLI, config,
  documentation and packaging regression. Do not duplicate or fork the VP++
  CPU implementation.
- Link `cdc::cpu::riscv_vp_plusplus` statically and use the existing Phase 2
  `rvv_runner` as proven reference code. Refactor common loader/memory/exit
  helpers only when this removes duplication without weakening the existing
  `riscv_vp_plusplus_portable` gate.
- Add `fw/riscv_vpp_compiler_vp` with project-owned `crt0.S`, trap entry, linker
  script, host-I/O headers, make rules and examples. All target compile commands
  use `-march=rv32gcv_zvl512b -mabi=ilp32d`; the initial examples remain
  `-ffreestanding -nostdlib -nostartfiles`.
- Load arbitrary valid RV32 ELF files through the CPU debug/TLM loader path,
  validate every loadable segment against the compiler-VP map, and start at the
  ELF entry point. Reject malformed, RV64, out-of-range and overlapping-MMIO
  images with a non-zero host exit code and a useful diagnostic.
- Route instruction fetch, scalar load/store, vector load/store and host-I/O
  MMIO through observable SystemC/TLM transactions. No direct pointer from the
  CPU backend to RAM is permitted.
- Provide both an instruction-retired watchdog and a simulated-time watchdog;
  a non-terminating guest must fail deterministically rather than hang the
  packaging or CI job.
- Implement at least these CLI options: `--elf`, `--config`, `--hart-id`,
  `--max-instructions`, `--timeout`, `--trace`, `--dump-signature`,
  `--print-config`, `--version` and `--help`. Unsupported values fail instead
  of silently falling back.
- `--print-config` and `--version` report the exact ISA/ABI/VLEN/ELEN values,
  one-hart count, functional/approximately-timed status, memory map, host
  compiler/SystemC version, CDC-VP revision, VP++ base revision and every
  effective VP++ patch/hash.
- Add `riscv_vpp_compiler_vp` and `riscv_vpp_compiler_vp_package`, using
  `cdc_make_portable` and `cdc_package_platform`. Add all SDK, example, license,
  provenance and manifest files explicitly; do not assume the base packaging
  helper knows about them.

#### Required demonstrations

**Scalar Hello World** is a freestanding RV32 program. It emits, through the
simulator-only TLM console, at least:

```text
Hello from RISC-V VP++ RV32GCV
XLEN=32
hart_id=0
SCALAR HELLO: PASS
```

It must not rely on host `printf`, a source-tree file, an external terminal
process or a separately installed VP++ executable. Disable compiler
auto-vectorization for this example and verify its application body contains no
vector instruction, so it is real scalar-path evidence.

**RVV Vector Add** is a freestanding program compiled for the frozen ISA/ABI.
It adds arrays with RVV, compares every result against a scalar golden result,
reads `vlenb`, and emits at least:

```text
RVV=1.0
VLEN=512
vlenb=64
VECTOR ADD: PASS
```

Its disassembly must contain actual `vsetvli`/`vsetivli`, vector load,
`vadd` and vector store instructions. Prefer a C RVV-intrinsic example for the
compiler-team handoff and retain a known assembly/inline-assembly reference so
a compiler-codegen failure can be distinguished from a simulator failure.

#### Packaging gate

The reference flow is:

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
$CC -dumpfullversion
$CXX --version | head

cmake -S . -B build-riscv-vpp-compiler-vp \
    -DCDC_BUILD_RISCV_VPP_COMPILER_VP=ON \
    -DCDC_BUILD_TPU_V3_SOC=OFF \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build-riscv-vpp-compiler-vp \
    --target riscv_vpp_compiler_vp_package \
    --clean-first \
    -j"$(nproc)"

./out/riscv_vpp_compiler_vp/riscv_vpp_compiler_vp \
    --elf ./out/riscv_vpp_compiler_vp/examples/scalar_hello/scalar_hello.elf

./out/riscv_vpp_compiler_vp/riscv_vpp_compiler_vp \
    --elf ./out/riscv_vpp_compiler_vp/examples/rvv_vector_add/rvv_vector_add.elf
```

All of the following must hold before Phase 4.5 is complete:

- both demonstrations print their exact PASS marker and exit zero under bounded
  instruction and simulated-time watchdogs;
- `--print-config` proves `rv32gcv_zvl512b`, `ilp32d`, RVV 1.0, XLEN 32,
  VLEN 512, ELEN 64, `vlenb=64`, one hart and no NoC;
- ELF attributes and disassembly are checked, not inferred from compiler flags;
- scalar/vector fetch and data traffic are observed on TLM, and invalid TLM
  responses become deterministic guest/host failures according to the Phase 2
  conformance contract;
- bad ELF class/machine, an out-of-map segment, an MMIO-overlapping segment,
  missing file, guest fail exit and both watchdog expirations are negative
  tests with no crash or hang;
- a source/link/manifest guard proves FlooNoC, Sauria, NEO DMA, Spike, GUI and
  full upstream VP++ platform code are absent from the binary and package;
- the package runs from an otherwise empty temporary directory with no source
  tree, build tree, `LD_LIBRARY_PATH`, externally installed VP++ executable or
  host SystemC fallback;
- `ldd`/`readelf`, license inventory and `BUILD_MANIFEST.json` agree with the
  shipped files and `$ORIGIN` runtime resolution;
- Phase 2 CPU/conformance/differential regressions and the existing
  `riscv_vp_plusplus_portable` test remain green in Release and Debug;
- a packaged ELF built by the documented external cross-toolchain commands can
  be substituted for the bundled examples and produces the same result.

#### Reuse in the final TPU_V3 platform

Phase 4.5 creates no throwaway CPU. Phase 7 instantiates this same
`cdc::cpu::riscv_vp_plusplus` backend once per NEO-CORE; Phase 8 gives each hart
a unique `mhartid`; Phase 9 routes non-local traffic through FlooNoC. Compiler
code generation remains `rv32gcv_zvl512b`/`ilp32d`. Multi-hart startup, linker
layout, synchronization, workload partitioning and local/global placement are
runtime/firmware responsibilities, not reasons to fork the compiler backend.

### Phase 5: MXU, 64x64 bring-up from Sauria v4.2

#### Tasks

- Pin and record the NPU-team v4.2 source revision and redistribution policy.
- Freeze `sauria_matrix_if` and the geometry/datatype reporting contract.
- Instantiate/extract the `int8_64x64` profile explicitly; do not use the
  `NpuTop`/`sauria_types.h` 32x32 defaults or the legacy CDC-VP 32x32 wrapper.
- Record and enforce X=64, Y=64, INT8 activation/weight, INT32
  accumulation/output, index widths 18/18/17 and the accepted SRAM-region
  sizing. Add a negative control that fails if an omitted parameter silently
  falls back to 32x32.
- Extract/wrap matrix multiplication plus only required feeder/sequencer/result
  collection and its internal `ConfigRegs` distributor; exclude `NpuTop`,
  Sauria DMA, instruction/profile routing, instruction decoder, OBP and RCE.
- Connect operand/result accesses through the native local-SRAM port rather
  than direct source backing; do not add an external AXI4 master.
- Port `demo_gemm_64x64` as the source golden case for `int8_64x64`. Audit the
  source ViT path after it passes; do not call an emulation-only path an MXU
  golden. Retain a matrix-only ViT-derived case until tiling and the full
  workload pipeline exist.
- Measure elaboration, host memory and runtime for one engine and two cores.

#### Gate

- Source NPU and extracted adapter match on the accepted 64x64 corpus.
- Build/config/runtime reports all agree on `int8_64x64`, X=64 and Y=64; a
  deliberately omitted geometry and an explicit 32x32/128x128 request are
  refused by the Phase 5 configuration gate.
- GEMM dimensions, edge tiles, asynchronous start, IRQ, reset and errors pass.
- Dependency review proves unrelated NPU-top blocks and Sauria DMA are absent.
- The generated component profile and runtime identity say `64x64`,
  INT8/INT32 and the patched source hash. Until Phase 7 actually links the
  engine, the platform package manifest must continue to say the MXU is neither
  linked nor selectable; no package may claim 128x128/BF16 evidence it does not
  contain.

### Phase 6: Transform block, Im2Col capability

#### Tasks

- Audit and hash-pin the v4.2 IFMAP address generator, CHW layout helper and
  convolution golden that establish Im2Col semantics.
- Define the accepted Im2Col tensor layout, kernel, stride, dilation, padding
  and datatype; advertise Col2Im separately as unavailable because its source
  and overlap/accumulation semantics were not found.
- Implement one adapter with a 32-bit AXI4-Lite MMIO target and native
  local-SRAM requester; do not add an external AXI4 master.
- Cross-check the extracted Im2Col order against the pinned NPU-team
  convolution golden by factoring the convolution through Im2Col plus an
  independent integer GEMM.
- Gate asynchronous start, MMIO protocol, native traffic/counters, IRQ/W1C,
  reset/abort/replacement-start epochs and dependency independence.

#### Gate

- Every advertised capability maps to traced NPU-team source and a golden test.
- A missing operation is reported unavailable and rejects start; no placeholder
  returns success.
- Col2Im selection produces `unavailable_operation` and no native SRAM traffic.
- The adapter has no NPU-top, Sauria, DMA, external-master or SRAM-backing
  dependency.

**Complete 2026-08-18 for D18 Im2Col-only Revision 1.** See
`TPU_V3_PHASE6_AUDIT.md`. A future Col2Im delivery is a promotion, not an open
implementation item inside this completed phase.

### Phase 7: Single NEO-CORE integration

#### Tasks

- Implement `neo_hart_port` (§11.7) first. Nothing else in this phase can be
  wired until the hart's single combined TLM socket has somewhere to decode
  onto; it is the one genuinely new component here, the rest is composition.
- Instantiate VP++ RV32GCV, core SRAM, NEO DMA, MXU64, Transform and
  the AXI4-Lite control, native local-SRAM and external bridge components.
- Select `local_fabric_timing::annotated` (D16). `arbitrated` consumes the
  caller's quantum, so a hart behind it loses temporal decoupling on every
  local load and store; it is for the Phase 11 contention studies.
- Bind `neo_dma::external` to `neo_external_bridge::local_outbound`, not to a
  memory. The DMA classifies core MMIO as external and relies on the bridge to
  refuse it; the two were proved separately in Phases 3 and 4 and meet here
  for the first time.
- Wire reset and per-engine level interrupts. Reset is hierarchical and
  implements the D19 hart contract; the adapter and the local fabric are reset
  in the same sequence, because resetting only a requester cannot retract a
  beat the fabric has already accepted (D17).
- Add firmware drivers for SRAM, DMA, MXU and Transform.
- Run `DMA -> Transform (Im2Col) -> MXU -> RVV` using SRAM descriptors/buffers. RVV performs
  post-processing and interprets/reshapes the output-feature matrix; do not add
  a placeholder Col2Im operation.
- **Do not** link the composed core into the packaged platform, and leave the
  manifest saying the MXU is neither linked nor selectable — the build split
  decision record **D21** freezes. An earlier version
  of this task said the opposite — that once Phase 7 linked the engines the
  manifest statement became false — and that was wrong in a way worth
  recording: `licenses/SAURIA.PROVENANCE.md` states that "a public CDC-VP
  source release does not include the external implementation or an
  NPU-enabled binary", and that publishing one needs the rights owner's
  separate authorization. So `SAURIA_LINKED=FALSE` is not a stale line waiting
  to be updated; it is the property the package has to keep. The composition
  lives behind two default-off options and in test targets, which is what lets
  Phase 7 finish without producing a binary nobody has authorized.

#### Gate

- One firmware ELF boots and controls every available block through MMIO.
- Control traffic is visible on AXI4-Lite, local data on the native SRAM
  fabric, and remote data on the external bridge; final data matches the
  accepted golden model.
- `neo_hart_port` decode is proved per destination and at the boundaries: an
  access straddling core SRAM and MMIO is refused rather than split, and a
  repeating TLM byte-enable pattern reaches the native plane expanded to one
  byte per data byte. The expansion has a negative control, because forwarding
  the pattern unchanged is the defect `INTERFACE_CONTRACT.md` §2 predicts and
  it produces a wrong result and an overread at once.
- Scalar and RVV remain one hart/memory path; no second vector processor exists.
- The D19 reset gate passes: the four state classes, the `vtype` bit pattern,
  and the `mcycle` check — which required the fourth patch to pass, and whose
  failure mode is recorded rather than tolerated. The WFI case is gated as a
  *limitation*: a hart idling in `wfi` keeps its reset state and does not
  restart.
- Unmapped, unavailable, misaligned, reset and injected-error cases fail
  predictably under watchdogs.

### Phase 8: Dual-core TPU chip

#### Tasks

- Instantiate exactly two NEO-COREs with unique hart IDs.
- Implement chip-local aperture decode, outbound arbitration and inbound route.
- Test simultaneous CPU, DMA, MXU and Transform traffic on both cores.
- Close the pre-existing multi-hart AMO gate (upstream `52d376d4`, decision
  record D8). Full architectural reset is **not** a Phase 8 item: decision
  record D19 closed its contract before Phase 7, and Phase 7 implements and
  gates it. Phase 8 only adds the two-hart case, which D19's gate already
  requires.

#### Gate

- Both harts boot and run independent workloads concurrently.
- One core cannot corrupt the other core's private SRAM accidentally.
- Authorized remote/debug accesses map to the intended core.
- Ownership, IRQs and responses remain correct under cross-engine contention.

> **Phase 8 result.** Met on 2026-08-20 by `chip_local_fabric`, `tpu_chip` and
> the chip-wide bus lock of decision record D22. The AMO item is closed with
> evidence rather than with a backport: the contention test D8 required now
> exists, and upstream `52d376d4` stays out because the lost-lock mechanism it
> fixes runs through MMU page-table stores, which cannot occur at
> `satp.MODE = Bare`. D22 records the one condition that reopens it. Two things
> worth carrying forward: the bus lock blocks *all* traffic from other harts,
> not only atomics, so no throughput figure from a run with contended atomics
> describes a coherent interconnect; and D19's prediction that a stranded
> reservation would hang a sibling does not hold, because the ISS bounds a
> reservation to 17 instructions. See `TPU_V3_PHASE8_AUDIT.md`.

### Phase 9: NoC integration and mesh scalability

#### Mandatory pre-Phase-9 NoC rebaseline

Phases 4–8 retain the signed FlooNoC v0 transport unchanged. That baseline is
`single-AXI`: separate physical `req` and `rsp` meshes, one physical/virtual
channel per mesh, and a 64-bit AXI data path. It does **not** provide a
control/data traffic class split, a wide data network or modeled virtual
channels. The Phase 4 DMA must therefore remain width-independent and obey the
current NoC frame/chunking contract; this rebaseline is not permission to alter
FlooNoC during DMA bring-up.

Before any Phase 9 implementation begins, use the pinned FlooNoC RTL and
FlooGen configuration as evidence and freeze all of the following:

1. **Traffic classification:** the address, opcode and/or source rules that
   classify every CPU, DMA, MXU, Transform, SRAM and MMIO transaction as control
   or data, including responses and error traffic. Classification must be
   deterministic at the NEO-CORE/NoC boundary.
2. **Transport structure:** keep a shared network, add control/data virtual
   channels, or instantiate separate narrow-control and wide-data physical
   networks. A virtual channel separates queues and arbitration only; it does
   not create a wider physical data path. Therefore a throughput requirement
   for a wider data path cannot be closed by “adding one VC” alone.
3. **Widths and adaptation:** freeze control width, data width, flit format,
   burst/frame limits, width conversion and chunking rules in both directions.
4. **Protocol behavior:** freeze arbitration priority/fairness, ordering
   domains, buffering/credit or ready-valid back-pressure, response ownership,
   deadlock/head-of-line-blocking expectations and reset of in-flight traffic.
5. **RTL verification impact:** define the block and integration RTL
   cross-checks, contention/negative controls, conservation assertions and
   package/metrics updates required by the selected configuration. A new VC or
   narrow/wide network is a new signed configuration; the v0 evidence cannot
   simply be inherited.

No implementation choice may be inferred from the architecture diagram alone.
Record the selected alternative and its RTL/FlooGen evidence in the decision
record before proceeding with the tasks below.

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

### Phase 9B: MXU 128x128 promotion

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
- Add core SRAM, NEO DMA, MXU and Transform drivers.
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

MXU (current Sauria v4.2 backend):

- jobs;
- MAC operations;
- active/busy cycles;
- utilization estimate;
- bytes read/written;
- stalls by cause;
- error/reset counts.

DMA and Transform:

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
- Run packaged smoke, RVV, SRAM, DMA, MXU, Transform, single-chip, and selected mesh tests.
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
| Unit | MXU | Sauria source golden, 64x64 geometry, data type, async IRQ, reset |
| Unit | Transform | pinned Im2Col convolution golden, exact CHW/matrix order, stride/dilation, padding/datatype/Col2Im refusals, unavailable capability |
| Unit | NEO hart port | Per-destination decode, straddling refusal, byte-enable expansion with a negative control, `neo_requester::cpu` attribution |
| Unit | AXI4-Lite control fabric | 32-bit decode, alignment/strobes, ordering and errors |
| Unit | NEO Local SRAM Fabric | Bank decode, round-robin, back-pressure, ownership, independent-bank progress and errors |
| Unit | Chip fabric | Two-core ownership, inbound/outbound routing |
| Unit | NoC endpoint | placement, chunking, completion, partial errors |
| Integration | Single core | DMA -> Transform (Im2Col) -> MXU -> RVV over core SRAM; Col2Im remains unavailable |
| Integration | Single chip | two harts, two SRAMs, two DMAs, two SAs, two Transform engines |
| Integration | NoC | global RAM and remote chip traffic |
| System | Firmware | boot, IRQ, drivers, workload completion |
| Stress | Concurrency | simultaneous harts/DMA/MXU/Transform/NoC traffic |
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

### R2. MXU 64x64 bring-up versus 128x128 target

Risk: existing v4.2 Sauria verifies 64x64, while the target 128x128 source is a
future NPU-team delivery. Hard-coded 64-bit masks or address assumptions can
make a nominal template change incorrect.

Mitigation:

- integrate only the verified 64x64 source first behind a geometry-aware adapter;
- reject 128x128 until source, golden and width/resource gates pass;
- audit hard-coded masks/address widths during promotion;
- report actual geometry and datatype in every run and package.

### R2A. Transform capability promotion

Risk: Phase 6 has a verified Im2Col-only component, but v4.2 has no established
Col2Im block. Guessing Col2Im layout or overlap behavior can produce plausible
but wrong images and falsely expand the advertised capability.

Mitigation:

- retain the D18 Im2Col source/layout/golden pins;
- expose Col2Im as unavailable rather than a fake implementation;
- obtain Col2Im module/interface/overlap semantics/golden tests from the NPU
  team before any promotion;
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
   TPU_V3 DMA, one MXU, one source-gated Transform block
   whose advertised operations are verified (D18 requires Im2Col and explicitly
   permits Col2Im to remain unavailable), and
   the D15 split: 32-bit AXI4-Lite control, native banked-SRAM local data and an
   bidirectional external AXI4/NoC bridge; no internal full AXI data crossbar
   exists.
4. RVV 1.0 reports XLEN 32, VLEN 512, ELEN 64, and executes the required full-V
   instruction groups.
5. RVV memory traffic uses TLM and interacts with core SRAM/global memory correctly.
6. DMA, MXU and Transform execute asynchronously and concurrently with
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

Current work, in order (updated 2026-08-18):

1. ~~Migrate `architecture_config` and `address_map` from the legacy
   two-MXU/SVM names to one MXU, one independent DMA, one Transform block and
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
3. ~~Implement Phase 4 NEO DMA with a build/test guard against Sauria DMA
   use.~~ Done. `tpu_v3_neo_dma`, gated by `tpu_v3_neo_dma` and
   `neo_dma_independence`; the guard covers the shared PL330-style component
   and DMI as well as Sauria, and fails when a forbidden include is added.
4. ~~Implement and package Phase 4.5 `riscv_vpp_compiler_vp` for the compiler
   team. Keep it to one RV32GCV hart, RAM and simulator-only host I/O; do not
   pull in FlooNoC or call it a NEO-CORE.~~ Done. `platforms/riscv_vpp_compiler_vp`
   and `fw/riscv_vpp_compiler_vp`, behind `CDC_BUILD_RISCV_VPP_COMPILER_VP`,
   gated by `riscv_vpp_compiler_vp_cli`, `riscv_vpp_compiler_vp_independence`
   and `riscv_vpp_compiler_vp_packaging`. The boundary is enforced against the
   sources, the emitted symbols, the link interface, the VP++ compile list and
   the shipped bundle, and the distribution gate configures with
   `CDC_BUILD_TPU_V3_SOC=OFF`.
5. ~~Pin the accepted v4.2 source, complete the matrix-only dependency audit and
   implement the 64x64 adapter.~~ Done in Phase 5 with D17 buffered staging.
6. ~~Audit and implement the available Transform capability.~~ Done in
   Phase 6 under D18: Im2Col is pinned and gated; Col2Im is recorded unavailable
   and is not a blocker for the forward path.
7. Compose one NEO-CORE in Phase 7: VP++ RV32GCV, SRAM/fabrics, DMA, MXU64 and
   Transform. Add firmware drivers and run
   `DMA -> Transform (Im2Col) -> MXU -> RVV`. Start with
   `neo_hart_port` (§11.7): VP++ has one combined fetch/data socket and
   `neo_local_sram_fabric` exposes no TLM target, so nothing binds to the hart
   until it exists. It is the only new component in the phase.
8. Separately obtain future NPU-team deliveries for Col2Im and a 128x128 MXU
   implementation source;
   neither may be advertised before its own promotion gate.

Carried out of Phase 3 as scheduled work, not as open findings:

9. Phase 5 and Phase 7 must select `local_fabric_timing::annotated` for
   full-system runs. `arbitrated` consumes the caller's quantum, so a VP++ hart
   behind it loses temporal decoupling on every local load and store (decision
   record D16).
10. Phase 9 must assert `!neo_external_bridge::blocks_on_arbitration()` when the
    detailed NoC backend is selected: an `arbitrated` fabric on the inbound path
    would stall the one process that advances the mesh clock.
11. Phase 7 composes the three fabrics, the SRAM, the DMA and the hart into a
    `tpu_core`. Until then they are proved as components and the platform
    instantiates the memories only, and its report says so.
12. Phase 7 must bind the DMA's `external` port to `neo_external_bridge`, not to
    a memory. The DMA classifies core MMIO as external and relies on the bridge
    to refuse it; the two were tested separately in Phases 3 and 4 and are first
    wired together in Phase 7.
13. ~~Phase 7 implements and gates the D19 hart reset contract.~~ **Done
    (2026-08-19).** `reset_cpu()` performs the four-class reset and
    `architectural_reset` gates it. Two implementation findings are recorded in
    D19: the cycle counter did need the fourth D8-mechanism patch, and a hart
    parked in `wfi` is not resumed by reset.
14. Phase 11 must state which local-fabric timing mode produced any DMA
    throughput figure, and must not quote `chunk_latency` as bandwidth.

Carried out of Phase 2, still open:

15. `fw/TPU_V3_SoC/rvv_smoke/link.ld` still exists and five images still link
    against it, so the repository carries two firmware memory layouts. Phase 2
    decision P2-13 assigned the cleanup — move the remaining images onto the
    real address map and delete `link.ld` — to Phase 5, and Phase 5 closed
    without recording it. It does not block Phase 7; it must be closed by the
    Phase 10 firmware work at the latest, because a second layout is exactly
    the kind of thing that stops being visible once drivers depend on it.

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

15. ~~Decide full architectural core reset semantics (audit §F8).~~ **Closed by
    decision record D19 (2026-08-18).** `reset_cpu()` is still a restart in the
    code; what D19 froze is the contract, the four state classes and the gate.
    Implementing and gating it is Phase 7 work, tracked as item 13 above. D19
    also records the two items that are not simply reachable — the cycle counter
    and revival of a terminated hart.

Also outstanding:

* **Phase 9 prerequisite:** the owner-aware local bypass in
  `noc_interconnect` (D1). It needs a cross-checked change to an RTL-signed
  component and its own negative controls — a missing or wrong owner mapping
  must fail during elaboration, and a local access must inject zero flits.
* **Phase 9 NoC architecture rebaseline:** retain FlooNoC v0 unchanged through
  Phase 8, then freeze the control/data classification, shared-VC versus
  narrow/wide-physical-network choice, both widths, arbitration, ordering,
  back-pressure and the corresponding RTL verification scope. This is a hard
  entry gate for Phase 9, not an implementation detail to decide while coding.

Do not begin full mesh composition or claim 128x128 before the corresponding
D14 phase and promotion gates pass.

## 22. Status Table

Update this table when work progresses.

| Phase | Status | Evidence |
| --- | --- | --- |
| Plan document | Complete | This file |
| Decision record D1-D21 | Final approved through D20: D16 implemented by Phase 3; D17 implemented by Phase 5; D18 freezes the Transform block's Im2Col-only baseline; D19 freezes the hart reset contract; D20 freezes the MXU/Transform architectural names (2026-08-18) | `docs/TPU_V3_DECISION_RECORD.md` |
| Phase 0: audit/baseline | Complete (2026-08-08) | `docs/TPU_V3_PHASE0_AUDIT.md`, `docs/ARCHITECTURE.md`, `docs/ADDRESS_MAP.md`, `docs/INTERFACE_CONTRACT.md` |
| Phase 1: skeleton/package | Complete (2026-08-08), review findings closed | `out/tpu_v3_soc/` runs with `RPATH=$ORIGIN` and no source-tree path; ctest `tpu_v3_address_map`, `tpu_v3_architecture_config`, `tpu_v3_soc_cli`, `tpu_v3_soc_packaging_regression` all pass |
| D14/D15/D20 rebaseline synchronization | **Complete for architecture and editable source.** Architecture documents, the D15/D20 Draw.io source and the C++ config/address/fabric migration agree. The derived JPG is explicitly legacy until re-rendered | Decision-record synchronization table, `docs/neo_core_architecture-d15.drawio`, and the Phase 3 gate evidence below |
| Phase 2: RV32GCV backend | **Complete** (2026-08-10), review findings closed | Audit, pin, build proof and execution complete — `docs/TPU_V3_PHASE2_AUDIT.md`. VP++ pinned at `7a36fe859cae242f513ca6ad16ab8238f1e82977` (tag `2025.09`); `cdc::cpu::riscv_vp_plusplus` builds against SystemC 2.3.4; D5 `cpu_config` implemented; the freestanding RV32GCV image executes through the wrapper with 2096 observed TLM requests and all 14 RVV checks passing (`riscv_vp_plusplus_backend`, `rvv_smoke_execution`). **F11 closed** by the approved backport of upstream `b710fa7b`: first TLM request at 0 s, `mcycle` 15 → 2439 → 2848, verified in Debug and Release on two harts, and both the configure check and the runtime gate were shown to fail when the patch is reverted. D10 vector-trap gate and the F5 concurrency control both pass (`rvv_vector_trap`, `fp_concurrency_normal`, `fp_concurrency_swapped`). **Spike differential corpus complete** (D11): one image on both models, 71-field signature, **64 matched / 7 XFAIL / 0 OPEN / 0 unexplained**, green in Debug and Release. The two findings it produced were fixed, not accepted: **D12** (all 32 RV32 index-EEW=64 encodings, unit and segment, raise an illegal instruction at the decode site) and **D13** (a failed bus access is an access fault chosen by origin). Both have a `conformance_patches` gate and a verified negative control. Also closed in this pass: the ISS caches were found enabled contrary to P2-5 and are now off and pinned by a test; `set_irq()` gained its first gate (`interrupt_delivery`); and the portable-executable requirement is met by `riscv_vp_plusplus_portable`, which runs the backend from a directory containing only the binary, its SystemC libraries and one ELF |
| Phase 3: core SRAM + split fabrics + map migration | **Complete** (2026-08-12) | **Map migration.** `CORE_SRAM`, `SA_CONTROL`, `DMA_CONTROL`, `TRANSFORM_CONTROL` and a relocated `CORE_COUNTERS` replace the Phase 1 `SVM`/`MXU0`/`MXU1` symbols; 115 regions at 8 chips, non-overlap proved at every legal chip count and capacity; `TPU_V3_MXU_BACKEND` retired in favour of `TPU_V3_SA_GEOMETRY`, whose refusal of `128x128` and of an unnamed geometry each have a negative control in the packaging regression. The legacy names are absent from the schema, the shipped configurations, the manifest and the packaged address-map output, and two independent regressions fail if they return. **Host backing.** `sparse_memory` gives deterministic 4 KiB pages: an unallocated page reads as zero and costs nothing, a write commits only touched pages, a fully masked write commits none, reset releases them, and a refused access leaves the caller's buffer untouched. `mesh_4x4` reports 1.25 GiB logical with 0 B allocated and peaks near 10 MiB of RSS, bounded by the CLI regression's own `ru_maxrss` check. **Core SRAM.** Window and capacity are separate fields; an access above the capacity is `capacity_error` and never an alias; every payload from 1 to 64 bytes works at every alignment; byte enables, cross-page transfers, counters and a debug path that bypasses the counters but not the bounds all pass. **AXI4-Lite control fabric.** Five register files behind one in-order decoder; 1-, 2-, 8- and 64-byte payloads, misalignment, partial strobes and wrapped streaming are each refused with the documented status; an unmapped address is an address error; a target's own refusal is propagated and counted apart; one transaction per initiator is enforced against a target that deliberately re-enters; an overlapping control map is refused during elaboration. A 64-byte accelerator payload is refused, which is how bulk data is kept off the control plane. **Native local-SRAM fabric.** 128-bit x 4 banks (provisional), low-order interleaved; one 64-byte request is one request and four beats, and an unaligned one is five — the D7 distinction, measured. Three requesters on one bank are serialised, back-pressured and share the bank within a quarter of each other; three on different banks run with zero conflicts and carry more traffic in the same wall of simulated time; response ownership holds under contention; all five named requesters reach both storage and the error path; an unattached requester throws. **External bridge.** Inbound SRAM traffic is arbitrated as `external_inbound` and its bytes reconcile exactly with the SRAM's own totals, which is the bypass negative control given that `core_sram` exposes no backing pointer; inbound MMIO reaches the addressed register file through the control plane and gets the same refusals a local access would; an outbound access naming this core is refused and counted, and the external stub never sees it. **Blocking.** No TLM target waits; the control fabric never waits; the local fabric's default `annotated` mode never waits and its `arbitrated` mode is the one that proves the arbitration (decision record D16). No source file or public type is named `neo_axi_fabric`. **Review round (2026-08-12).** Four contract defects were found and fixed, each with a negative control that fails when the fix is reverted: the bridge forwarded TLM's repeating `byte_enable` pattern into a plane that has no repeat rule, overreading the initiator's array for any payload longer than the pattern; the one-outstanding-request-per-requester rule of D15 was assumed rather than enforced, so two processes sharing an identity interleaved under one name; `reset()` cleared `waiting[]` and `busy` without waking anyone, hanging any requester blocked in arbitration; and outbound local containment tested containment instead of overlap, forwarding a transfer that began inside the core and ran past it. Two counter definitions were tightened in the same pass — a refused request is still a request, and `transferred_bytes` counts bytes moved rather than bytes named — along with the debug path's command handling and its STATUS answer. **Cleanup review.** Reset generation is captured before input-delay consumption, old requests cannot repopulate a new counter epoch, `aborted` explicitly permits already-completed partial bytes, and the bridge now applies common payload validation consistently to inbound/outbound normal/debug traffic. Each behavior has a regression that checks both the refusal/abort and absence of side effects. 19/19 `tpu_v3` tests pass in both Release and Debug with zero skips |
| Phase 4: independent NEO DMA | **Complete** (2026-08-13) | `components/TPU_V3/neo_dma` plus `DMA_MODEL.md`. **Programming model.** The frozen §11.5 register map over an absolute 64 KiB decode; reserved offsets read zero and `DMA_CONTROL + 0x1000` does not alias offset zero; 1-, 2-, 8- and 64-byte payloads, misalignment, partial strobes, wrapped streaming and a bad command are each refused with the documented status, and a null pointer is answered rather than thrown because this target is reachable from a remote master. `transport_dbg` agrees with `b_transport` register for register and is side-effect free. **Data path.** One native requester (`neo_requester::dma`) and one external initiator; local→external and external→local only, classified by absolute address with no direction bit. Copies verified at lengths 1, 2, 3, 7, 8, 15, 16, 63, 64, 65, 2048 and 5000 in both directions at odd source and destination offsets; local accesses split to 1..64 bytes and external frames obey `address % 8 + length <= 2048`, checked by the target rather than trusted. **Refusals.** Zero length, both endpoints local, both external, a span straddling the SRAM boundary in either direction, and a span ending above the 4 GiB RV32 limit each latch their own cause and commit nothing. **Partial failure.** A destination refused inside the third frame stops the job with `external_write`, `BYTES_DONE` = 4096 — destination bytes committed, not the 5120 fetched — exactly three external transactions attempted and none after the failure. **IRQ.** Level, raised for completion and error alike, held across time until W1C, deasserted by acknowledgement, never raised by an abort. Driven by one process (a SystemC signal refuses two writers) with immediate notification so it settles within one delta. **Epochs.** Explicit abort clears `BUSY`, sets sticky `ABORTED`, counts an abort and raises nothing; reset does the same without `ABORTED`, keeps the committed count, and neither lets the abandoned worker publish a completion, an error or a path counter afterwards — `BYTES_DONE` is the exception and is governed by register ownership instead, so an interrupted job may still raise it until a new `START` claims the register. A reset landing while the DMA was queued behind another requester on a contended bank unwinds without hanging. **Independence.** `neo_dma_independence` scans the sources with comments stripped, the emitted symbols and the CMake link interface; adding `#include "tpu_v3/sram/core_sram.h"` makes it fail. **Review round (2026-08-13).** Two High defects were found in reset/abort semantics and fixed. A start request was delivered as a bare event, so a `START` issued while the old worker was parked — which firmware may legitimately do, because both paths clear `BUSY` at once — reached nobody and the new job held `BUSY` for ever; it is now a flag that survives the gap. And `BYTES_DONE` was published at the end of a chunk rather than as each destination access landed, so a reset arriving between a commit and its report froze a count *lower* than memory held. Both were invisible to the first test, which settled for microseconds before restarting and only checked that the destination held *at least* the reported bytes; the phase now parks the worker deterministically inside a blocking external target and checks the byte past `BYTES_DONE` is still filler. A third rule fell out of the fix: a transaction still in flight at reset completes afterwards and its bytes are deliberately left unattributed, since the epoch that could have claimed them is gone and attributing them to whatever job started next is what §11.5 forbids. `tpu_v3_neo_dma` also no longer links `tpu_v3_core_sram` — the native-port interface was split into `tpu_v3_native_port`, and the guard now refuses that link edge as well. **Second review round (2026-08-13).** One more High: the fix above guarded `BYTES_DONE` on epoch sameness, which also swallowed a commit made *before* a reset — a native access writes beats into SRAM, waits for arbitration, the reset lands, and the access then returns reporting bytes that are genuinely in memory. The guard is now ownership of the register, which a reset does not transfer and a new `START` does, so an interrupted job keeps reporting into its own snapshot while a later job is never polluted. Every earlier reset case copied local→external, so the native destination path had no coverage at all; two phases were added, one resetting the DMA mid-native-access and one resetting the *fabric* so the request returns `aborted` with a partial byte count, the latter on an isolated SRAM/fabric/DMA instance because `fabric.reset()` clears counters the main conservation check relies on. Also settled: the DMA now clears its path traffic counters on reset and keeps only its event counters, so that when Phase 7 resets the DMA and the fabric together their totals still reconcile — conservation is a per-epoch property and the test states the epoch; an idle `ABORT` is documented and tested as accepted-and-ignored rather than counted; and `chunks_issued` became `chunks_completed`, since a failing chunk was issued and is not counted. **Third review round (2026-08-13).** A last High in the same area: `reset()` cleared the path traffic counters but an old in-flight request still added its response to them, so an epoch that had just been zeroed came back reading `LOCAL_BYTES=64, LOCAL_REQUESTS=1`. `neo_local_sram_fabric` already excluded old-generation responses from its own counters, so the DMA was also drifting away from the component it must reconcile with. A `traffic_epoch_`, advanced by `reset()` and not by `ABORT`, is captured before each transaction and gates the path counters; `BYTES_DONE` keeps its separate ownership rule, because "how much of this job's destination was committed" and "how much traffic did this path carry in this window" are different questions and a reset ends the window without ending the job's claim. A hierarchical-reset phase now resets the DMA and the fabric together mid-request, drains, and requires all three: `BYTES_DONE` equal to the bytes SRAM holds, DMA path counters zero, and DMA/fabric reconciling — then a fresh job reconciling again in the new epoch. Three documentation sites that still described the superseded straggler rule, called the path counters lifetime totals, or said "snapshot at reset" were corrected. Eleven negative controls confirm the behavioural gates bite, plus one for the guard itself. `components/dma_tlm` is untouched, and `dma_tlm` with all ten `noc_soc` regressions pass while `dma_platform` and `vp_fx1_full_soc` build |
| Phase 4.5: RISC-V VP++ Compiler Enablement VP | **Complete** (2026-08-14) | `platforms/riscv_vpp_compiler_vp` plus `fw/riscv_vpp_compiler_vp` and `docs/MEMORY_MAP.md`. **Boundary.** One architectural RV32GCV hart, one TLM address decoder, program/data RAM and a simulator-only host-I/O target. `CDC_BUILD_RISCV_VPP_COMPILER_VP` is independent of `CDC_BUILD_TPU_V3_SOC`, and the packaging gate configures with the latter `OFF` so the handoff cannot quietly acquire the SoC tree. `cdc::cpu::riscv_vp_plusplus` is reused, not forked. **Map.** RAM at the TPU_V3 global-RAM base `0x8000_0000`, size configurable; the host-I/O window retains the Phase 2 exit protocol at `0x000F_0000` with the same four words at the same offsets and adds console, identity and measurement registers at 0x400 and above. One header, `compiler_vp/host_io_map.h`, is compiled by the platform, included by `crt0.S` from assembly, and preprocessed into the linker script, so the image's load address cannot drift from the address the platform maps. **Demonstrations.** Both print their exact PASS markers and exit zero. `scalar_hello` cross-checks the toolchain's `__riscv_xlen` against the platform's XLEN, `mhartid` against the configured hart id and `misa` against every letter of the frozen ISA, so its banner is a result rather than four literals; it is built with auto-vectorization off and *verified by disassembly* to contain no vector instruction. `rvv_vector_add` adds 1024 elements three ways — scalar golden, `<riscv_vector.h>` intrinsics and a hand-written `vsetvli`/`vle32.v`/`vadd.vv`/`vse32.v` loop, both kept `noinline` so a codegen failure stays distinguishable from a model failure — and earns `RVV=1.0` from behaviour, by observing that a reserved `vsew` sets `vtype.vill` and zeroes `vl` without trapping. **Observability.** No DMI (refused and counted), no ISS decode or load-store cache, so instruction fetch comes out at exactly one TLM transaction per retired instruction and the report prints the ratio. Fetch is separated from data by address, which required the shipped linker script to emit three program headers instead of the usual single RWX segment: with one segment every load is inside an executable segment and the split is meaningless. Vector traffic is *enforced*, not asserted — the intrinsic loop declares, from its own element count, the 3072 data accesses it must cause, and the platform fails the run if the bus does not see them. **Refusals.** Not an ELF, truncated, ELF64, wrong machine, big-endian, non-`ET_EXEC`, a float ABI other than `ilp32d` read from `e_flags`, an architecture string that is not `rv32*` or one guaranteeing a minimum vector length above 512 read from `.riscv.attributes`, a segment outside RAM or overlapping the host-I/O window or another segment, and an entry point outside RAM — each with its own exit code and a diagnostic naming the field. A scalar-only ISA string and a missing attributes section are accepted and reported, since checking scalar code generation is half the job. **Watchdogs, and a defect the gate found.** The two the plan asks for are not sufficient, and the shortfall is not exotic: both are polled between slices of `sc_start()`, and an image whose entry point lands on memory it never wrote traps, vectors to an `mtvec` its startup never set, and faults on the fault — retiring nothing and never reaching a quantum boundary, so neither bound is ever read again and the run hangs indefinitely with both armed. One mistyped load address in a linker script produces it. The decoder now recognises an unbroken run of 1024 refused accesses as a fault loop and throws, which is the only way out of a SystemC process that will not yield, and a `--wall-timeout` host thread is the backstop for whatever that does not cover. **Package.** `out/riscv_vpp_compiler_vp/` carries the binary at `RPATH=$ORIGIN`, the SystemC runtime, configs, both demonstrations with their disassembly, the SDK that built them, the map document, licences and a manifest recording the ISA/ABI/VLEN/ELEN/`vlenb`/hart count, the accuracy disclaimer, the VP++ base revision with every patch and hash, and every non-goal as absent. The gate moves the bundle, runs both demonstrations from it with no source tree and `LD_LIBRARY_PATH` unset, rebuilds the vector example from the shipped SDK with the documented commands, substitutes it, and requires identical guest output. **Independence.** `riscv_vpp_compiler_vp_independence` scans the sources with comments *and string literals* stripped — the `--version` banner has to be able to name what is absent — the emitted symbols, the CMake link interface, and the VP++ compile list for any `platform/`, Qt or VNC source; the packaging gate repeats the symbol and file-listing halves on the shipped bundle. **Evidence.** 103 CLI checks, the boundary gate and the distribution gate pass in Release and Debug; nine negative controls confirm each new gate bites when its fix is reverted, including the single-RWX-segment linker script (which reproduces `1024 of 3072 accesses`) and the disabled fault-loop detector (which reproduces the hang). TPU_V3 stays 35/35 in Release and Debug. **Review round (2026-08-14).** Two High defects, both in the package rather than the model. The package target *succeeded* without a cross toolchain, producing a bundle with neither demonstration and recording it as one `false` in a manifest field — a bundle that looks shippable and is not, and a packaging test that skipped rather than failed would have kept CI green over it. The executable still builds without the toolchain, deliberately, but `check_package_contents.cmake` now runs last in the package target and refuses an incomplete bundle, and the distribution gate has no skip at all. And the package shipped only CDC-VP's own Apache-2.0 while the binary statically links the RISC-V VP++ ISS (MIT) and Berkeley SoftFloat (BSD-3-Clause) — a redistribution blocker. Both licences are now shipped, SoftFloat's extracted at package time from a source file that was actually compiled because upstream ships no standalone licence file, `THIRD_PARTY.md` lists both, and the gate checks the text and not merely the filename. Two Medium: the firmware was generated *into* `fw/`, so Release and Debug clobbered each other's images and a read-only checkout could not build — it is now copied into the build tree and built there, and with `-ffile-prefix-map` and a two-stage compile the two build types produce byte-identical examples carrying no build-machine path. And the CLI accepted values `std::` accepts but a watchdog cannot: `--timeout nan` disarmed the simulated-time watchdog while `--print-config` still reported it armed, because every comparison against a NaN is false; `hart_id: 4294967296` in a configuration file truncated to hart 0; a leading `-` wrapped to an enormous limit. All are refused, on both the command-line and the file path. Also delivered: `COMPILER_QUICKSTART.md` and `ISA_ABI_CONTRACT.md`, which §15.3 names and the first package omitted, and §15.3's `sdk/` file list rebaselined to the delivered names. Six further negative controls confirm each fix bites |
| Phase 5: MXU 64x64 extraction from Sauria v4.2 | **Complete** (2026-08-18), review findings closed | `components/TPU_V3/sauria_matrix` and `docs/TPU_V3_PHASE5_AUDIT.md`. The legacy implementation target locally pins `v4.2_model` at base hash `418a8d88...`; its full source, adapter and regression evidence is recorded in the Phase 5 audit. D17 buffered staging connects the MXU to `neo_local_sram_if`. Current capability is `int8_64x64`, INT8/INT32; 128x128/BF16 remain promotion targets. Final result: 14/14 source-labelled `sauria` tests and 35/35 `tpu_v3` tests pass in both Release and Debug |
| Phase 6: Transform | **Complete** (2026-08-18) for D18 Im2Col-only Revision 1 | `components/TPU_V3/image_transform`, `IMAGE_TRANSFORM_MODEL.md` and `docs/TPU_V3_PHASE6_AUDIT.md`. The Transform block's pinned CHW INT8 Im2Col capability passes the NPU convolution golden and MMIO/native/reset gates; Col2Im is explicitly unavailable and causes no SRAM traffic. Standalone component gate 3/3 and full `tpu_v3` regression 38/38, zero skips |
| Phase 7: single NEO-CORE | **Complete** (2026-08-20), review findings closed | `neo_hart_port` (§11.7) and `tpu_core` compose VP++, core SRAM, the three D15 planes, the DMA, the MXU and the Transform block; `tpu_v3_neo_core` builds only when both accelerator options and the CPU backend are present. Gated by `tpu_v3_hart_port`, `tpu_v3_neo_core` and `tpu_v3_neo_core_pipeline`, the last of which boots one firmware ELF that drives every engine through MMIO and matches a host-computed golden for the Im2Col matrix, the INT32 GEMM and the RVV reduction. D19 implemented and gated by `architectural_reset`. Three integration defects surfaced and were fixed: `reset_cpu()` threw on its second call, `neo_external_bridge` had one outbound socket for two initiators, and `sa_control` drove its IRQ from two processes. Composing cores into a chip is Phase 8 and into the platform Phase 9; D21 keeps a NEO-CORE binary internal either way. `unavailable` (Col2Im) and `injected-error` are gated inside the composition too. Review on 2026-08-20 found and closed a reset-semantics contradiction — a core reset wiped core SRAM while every engine reported the bytes it had committed to that SRAM as still committed — plus a double engine reset that destroyed the accounting `sa_control` snapshots, an `i_rstn` that was never pulsed, and firmware building into the source tree. See `TPU_V3_PHASE7_AUDIT.md` |
| Phase 7: audit | Complete (2026-08-20) | `docs/TPU_V3_PHASE7_AUDIT.md` — what composing surfaced that the component gates could not, and the review findings it closed |
| Phase 8: dual-core chip | **Complete** (2026-08-20) | `chip_local_fabric` and `tpu_chip` compose two NEO-COREs with hart ids `chip * 2 + core`, the chip register windows and exactly one mesh boundary; core-to-core traffic is answered inside the chip and never offered to `noc_interconnect`. Gated by `tpu_v3_chip_fabric` and `tpu_v3_tpu_chip`, the latter booting one image on both harts that branches only on `mhartid`. The multi-hart AMO gate D8 deferred is closed by D22: `test_bus_lock_atomicity` shows 128 of 128 increments with one shared lock and exactly 64 with the per-hart default, and upstream `52d376d4` stays out on the reachability argument recorded there. Four integration defects surfaced and were fixed: the per-hart bus lock; a core's hart and its DMA both able to enter the core's one external socket, which `neo_external_bridge` now arbitrates; a chip-fabric arbiter that released its port before the downstream transaction; and a fairness observable that would have passed with its labels swapped. The composition gate runs in both chip-fabric timing modes, because only the blocking one can reach the second of those. Release and Debug `tpu_v3` 51/51, zero skips. See `TPU_V3_PHASE8_AUDIT.md` |
| Phase 8: audit | Complete (2026-08-20) | `docs/TPU_V3_PHASE8_AUDIT.md` — the multi-hart atomicity evidence, its negative controls, and the D19 prediction measurement corrected |
| Phase 9: NoC/mesh | Not started; mandatory NoC rebaseline before implementation | D1 prerequisite; freeze control/data classification, shared/VC versus narrow/wide physical transport, widths, arbitration, ordering, back-pressure and RTL verification scope; also assert the external bridge does not block on arbitration (D16) |
| Phase 9B: MXU 128x128 promotion | Waiting for NPU-team delivery | The current Sauria-derived 64x64 implementation must remain explicitly labelled until then |
| Phase 10: firmware/workloads | Not started | — |
| Phase 11: metrics/stress | Not started | — |
| Phase 12: packaging/signoff | Not started | — |

## 23. Decision Log

| Decision | Value | Status |
| --- | --- | --- |
| TPU cores per chip | 2 | Frozen. Rationale recorded in §4.1 (2026-08-19): it comes from the reference architecture — the project-owner brief says `02 TPU Core/Chip` and `Hinh02.jpg` shows Core 0 and Core 1 sharing one Interconnect Router inside one chip frame. Not derived from the NoC initiator budget, which was found later |
| MXUs per NEO-CORE | 1 | Frozen by D14/D20; supersedes the historical two-MXU composition |
| MXU geometry | 64x64 verified bring-up; 128x128 target from NPU team | Staged and gated by D14 |
| DMA per NEO-CORE | 1 independent TPU_V3 DMA | Frozen by D14; implemented in Phase 4 as `tpu_v3_neo_dma`. Sauria DMA and the shared PL330-style component both forbidden, gated by `neo_dma_independence` |
| NEO DMA implementation boundary | New `components/TPU_V3/neo_dma`; do not modify, wrap, inherit or link `components/dma_tlm` | Approved Phase 4 rebaseline under D14/D15 (2026-08-13); shared DMA remains owned by its existing platforms |
| NEO DMA programming model | One descriptor/in-flight job, no queue or microcode; local→external and external→local only in Revision 1 | Approved Phase 4 baseline; absolute 64-bit address arithmetic, register and error semantics frozen in §11.5 |
| Transform blocks per NEO-CORE | 1; Im2Col available in D18 Revision 1, Col2Im unavailable | Frozen composition and Im2Col semantics; Col2Im requires a future source/golden promotion |
| Control plane | 32-bit AXI4-Lite, in order, no bursts or IDs | Frozen by D15; SystemC is transaction-level, not channel-cycle accurate |
| Local data plane | Native pipelined request/response fabric into physically banked SRAM | Frozen by D15; no internal full AXI data crossbar |
| Local-fabric arbitration | Deterministic round-robin per bank; one outstanding request/requester initially | Frozen by D15; implemented and gated by `tpu_v3_local_sram_fabric` |
| Local-fabric timing mode | `annotated` (default, never waits) or `arbitrated` (blocks on a real per-bank arbiter) | Approved (D16). Every TLM target still never waits; a timing figure must name the mode |
| DMA byte accounting | `BYTES_DONE` counts destination bytes committed; path totals count each path's own successful boundary transactions | Frozen in Phase 4 (plan §11.5). Source bytes in the staging buffer are explicitly not completion |
| DMA reset/abort | Both advance a job epoch; abort is reported as `ABORTED`, never as `DONE`; committed bytes are never rolled back | Frozen in Phase 4 |
| Local-fabric physical parameters | Data width, bank count/mapping and pipeline depth | **Still open** pending SRAM macro, frequency and PD inputs. The C++ schema has no default and refuses zero; the shipped configurations state 128-bit x 4 banks x 2 stages and every report prints them labelled provisional |
| External data plane | Bidirectional AXI4/NoC bridge for outbound VP++/DMA and inbound remote traffic | Frozen by D15; DMA owns bulk movement, while MXU and Transform remain local-SRAM requesters |
| VPU ISA | RISC-V V | Frozen |
| RVV version | 1.0 | Frozen |
| XLEN | 32 | Frozen |
| VLEN | 512 bits | Frozen |
| ELEN | 64 bits | Frozen |
| Current NoC transport baseline | FlooNoC v0 `single-AXI`: separate physical `req`/`rsp` meshes, one physical/virtual channel per mesh, 64-bit AXI data path; control and bulk data are not separated | Retained unchanged through Phase 8; it does not imply a TPU_V3 final transport choice |
| Phase 9 control/data transport | Open: shared network, control/data VCs, or separate narrow-control/wide-data physical networks; control/data widths also open | **Mandatory pre-Phase-9 rebaseline.** Freeze from pinned RTL/FlooGen evidence together with classification, arbitration, ordering, back-pressure and a fresh RTL verification scope; VCs alone do not widen the data path |
| NoC topology | Parameterized 2D mesh | Frozen concept, dimensions open |
| NoC attachment | One aggregated endpoint per chip | Frozen |
| RV32GCV runtime | RISC-V VP++ (`ics-jku/riscv-vp-plusplus`), MIT | Phase 2 complete at the recorded pin and approved patch series |
| Compiler enablement VP | One RV32GCV hart, TLM program/data RAM and simulator-only host I/O; no NoC and no accelerator/NEO composition | Approved pre-Phase-5 deliverable (Phase 4.5). Output is `out/riscv_vpp_compiler_vp/`; it reuses, rather than forks, `cdc::cpu::riscv_vp_plusplus` |
| Compiler handoff ISA/ABI | `rv32gcv_zvl512b`, `ilp32d`; XLEN=32, RVV 1.0, VLEN=512, ELEN=64, `vlenb=64` | Frozen for the Phase 4.5 package and unchanged when the same hart is replicated in later NEO-COREs |
| RVV differential reference | Spike `16c0b60119f65a648643cf5d41e4e38e871f0bad` (2026-08-07), BSD-3-Clause | Built and in use as the Phase 2 oracle (D3, D11). Child process only; never linked into the platform or a package |
| Cross toolchain | xPack `riscv-none-elf` GCC 15.2.0-1, `/opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1` | Pinned (D4); Phase 2 uses a freestanding `-nostdlib` environment |
| MXU backend | Sauria matrix-only adapter | 64x64 first, 128x128 after NPU-team promotion; source revision and datatype reported |
| Maximum chips | 8 chips / 16 cores | **Revision 1 backend limit** (D2), from the 3-bit NoC manager id. 32 cores = 16 chips is a separate coordinated change |
| Mesh dimensions | Configurable within 2x2, 3x3, 4x4, 4x2, 2x4 | Frozen set, choice open |
| Address map layout | D14/D15 rebaseline (`docs/ADDRESS_MAP.md`) | Top/chip/core apertures retained; the core-local symbol and MMIO migration landed in Phase 3 and the legacy names are gated out of generated output |
| Window vs capacity | Full window always decodes; capacity is separately reported and the target refuses above it | Frozen (D6) |
| Core SRAM capacity | **Reference 16 MiB** per core (the full window); smaller values are labelled bring-up | D6 retained and renamed by D14 |
| Global RAM size | Configurable, 256 MiB bring-up default, 1 GiB window | Open capacity (D6); simulated backing memory, not a model of TPU v3 HBM |
| Host-memory backing | Sparse deterministic 4 KiB pages; logical capacity is not eagerly allocated | Implemented (D6). `sparse_memory`; `mesh_4x4` is 1.25 GiB logical at ~10 MiB of RSS |
| MXU target arithmetic | **BF16 x BF16 with IEEE FP32 accumulation**, fixed accumulation order | D6 target retained; v4.2 bring-up must report its actual supported datatype and is not reference equivalence |
| Same-node initiator/target | Keep `NoLoopback=1`; owner-aware local bypass | Approved (D1). **Phase 9 prerequisite** |
| Hart id / reset PC | Static fields in `cdc::cpu::cpu_config` | Approved (D5, supersedes P0-9). No default no-op virtual setters |
| Vector memory access granularity | Element-wise, as VP++ issues it; no fork, no inferred coalescing; counters named for TLM requests | Approved (D7) |
| Full architectural core reset | **Full deterministic reset in the VP++ wrapper**; identity/configuration preserved, specification-defined fields set per the privileged spec, the rest zeroed for reproducibility. Register/CSR/vector state needs no upstream patch; the cycle counter is the one open candidate for a fourth D8-mechanism patch, and reviving a terminated hart is out of Revision 1 scope | **Decided (D19, 2026-08-18).** Supersedes today's restart-plus-cache-reinit `reset_cpu()`. Implementation and gate are Phase 7 work; see `docs/TPU_V3_PHASE2_AUDIT.md` F8 |
| Differential method | One image on both models; compare a firmware-written 71-field signature block located by linker symbols; Spike as a child process, never linked | Approved (D11). `spike_differential` |
| RV32 index EEW=64 | All 32 indexed encodings — unit and segment — raise an illegal instruction at the RV32 decode site | Fixed (D12), downstream conformance patch. Audit F12 |
| Trap cause for a failed bus access | Access fault by origin — 1 fetch, 5 load, 7 store/AMO; page faults only from MMU translation | Fixed (D13), downstream conformance patch. Audit F13 |
| VP++ patch series | Base pin + `upstream-backport` and `downstream-conformance` patches, hash-verified at configure time and recorded per-patch in the manifest | Approved (D8 mechanism, extended by D12/D13) |
| Transform block | Pinned D18 Im2Col engine; future Col2Im capability separate | Im2Col complete. Col2Im source and overlap semantics open; it is never substituted by RVV or inferred from PSM ordering |
| MXU adapter shape for Sauria backend | Buffered tile staging: prefetch operands into private staging stores over `neo_local_sram_if`, run the array at the source's own timing, write results back. No pass-through, because the source has no SRAM-side response handshake. One physical X-lane B vector per K, with inactive N lanes zero-padded | Approved by D17; implemented and edge-gated in Phase 5 |
| Sauria source hygiene | The selected closure's trace/debug state is compiled out by an ordered, hash-verified two-patch set; binary-symbol, no-trace and two-full-adapter gates prove instance cleanliness | Implemented by Phase 5; patched tree `c1931405...` |
| Four-image workload | Optional future named workload | Not frozen |

The authority for D1–D21 is `docs/TPU_V3_DECISION_RECORD.md`.
