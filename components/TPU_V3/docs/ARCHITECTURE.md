# TPU_V3 Architecture

Companion to `TPU_V3_IMPLEMENTATION_PLAN.md`. The plan says *what must be
built and in what order*; this file says *what the thing is*, at the level of
detail a person needs to write or review one of its components.

Nothing here overrides the plan's frozen decisions (§4) or unfreezes anything
in §5. Where this file picks a value the plan left open, it says so and names
the decision it comes from.

`TPU_V3_DECISION_RECORD.md` is the authority for the items it covers. Decision
D14 is the component rebaseline for the NEO-CORE and D15 freezes its
control/local-data/external interconnect split. They supersede the old
two-MXU/SVM/Sauria-experimental composition and D14's temporary single
AXI-like-fabric wording. Phase 0 through Phase 2 evidence remains valid, but
its old accelerator hierarchy is historical only.

D15 received final project-owner ratification on 2026-08-12, and Phase 3
implemented it: `core_sram`, `neo_control_fabric`, `neo_local_sram_fabric` and
`neo_external_bridge` exist, are gated, and carry the D14 names. What Phase 3
did not do is compose them — that is Phase 7, and until then the platform
instantiates the memories only and says so in its report. Decision record D16
records the one question D15 left open: where the local-data plane is allowed
to block. Phase 4 subsequently delivered the independent NEO DMA, and Phase 5
delivered the standalone 64x64 INT8/INT32 Sauria matrix engine; neither changes
the fact that their full NEO-CORE composition is Phase 7. See the editable
[D15 draw.io source](neo_core_architecture-d15.drawio) and its rendered
[JPG](neo_core_architecture-d15.jpg).
This is the project-defined NEO-CORE implementation architecture, not a claim
about Google TPUv3's unpublished internal interconnect.

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

tpu_core / NEO-CORE
├── riscv_vp_plusplus                          one RV32GCV hart, scalar + RVV
├── neo_control_fabric                         32-bit AXI4-Lite control plane
├── neo_local_sram_fabric                      native banked-SRAM data plane
├── core_sram                                  one shared SRAM
├── dma                                        independent CDC-VP DMA
├── sauria_matrix_engine                       one SA; 64x64 bring-up, 128x128 target
├── image_transform_engine                     Im2Col + Col2Im
└── external_bridge                            bidirectional AXI4 / chip-NoC boundary
```

Counts that are frozen and must be validated in constructors: 2 NEO-COREs per
chip; one hart, one SRAM, one independent DMA, one Sauria matrix engine and one
ImageTransform engine per NEO-CORE. The current Sauria integration baseline is
64x64. The architectural destination remains 128x128, supplied by the NPU team;
geometry is therefore explicit in configuration and package reports rather than
silently relabelled.

`N <= 8` is not an architectural preference. The frozen FlooNoC chimney manager
ID is 3 bits, so `noc_interconnect` refuses a ninth upstream initiator, and
plan §4.4 gives each chip exactly one. See `TPU_V3_PHASE0_AUDIT.md` §5.2.

## 2. Hart and accelerator boundaries

The scalar core and the VPU are **one architectural RISC-V hart** (plan §4.2).
There is no MMIO doorbell between them, no separate `mhartid`, and no
software-visible handoff. A vector instruction is an instruction in the same
instruction stream, and a vector load is an ordinary RISC-V load with a vector
destination.

The matrix engine, ImageTransform engine and DMA are **not** part of the hart.
They are memory-mapped engines, programmed through register/descriptor
interfaces, started asynchronously, and completed through status registers and
level interrupts. Firmware must wait for completion before consuming results.

The separate Scalar and Vector boxes in the approved diagram are logical
portions of the same VP++ hart. They share PC, privilege state, scalar/vector
register state, CSRs and traps. Revision 1 does not instantiate a second vector
processor or define a scalar-to-vector MMIO offload protocol.

### Matrix-engine arithmetic

The reference numeric destination remains the D6 contract:

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

Phase 5 implements an INT8 × INT8 → INT32 quantized bring-up path from the
pinned `int8_64x64` Sauria source. It is selected explicitly, is additive, and
does not replace the BF16/FP32 reference destination. The capability register
reports only INT8/INT32 for this engine; asking it for BF16 or FP16 is a
configuration refusal rather than a silent fallback.

The v4.2 64x64 bring-up model does not by itself prove this BF16 contract. Any
bring-up run using a datatype currently supported by Sauria must identify that
datatype and geometry in configuration, metrics and the package manifest and
must not be reported as the 128x128 BF16 reference result.

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

## 3. Interconnect and data paths

NEO-CORE intentionally does **not** use one full AXI crossbar internally. Its
interconnect is split by traffic type:

| Plane | Protocol/role | Users |
| --- | --- | --- |
| control | 32-bit AXI4-Lite, in order, no bursts or IDs | VP++ programming core/DMA/SA/ImageTransform registers and reading counters |
| local data | native pipelined request/response fabric into physically banked core SRAM | VP++ local load/store, DMA local port, SA, ImageTransform and authorized inbound traffic |
| external | bidirectional AXI4/TLM adapter into the existing chip/NoC endpoint | outbound VP++ fetch/global and NEO DMA; inbound remote SRAM/MMIO access |

The model represents AXI4-Lite and AXI4 at transaction level; it does not
claim signal-by-signal channel accuracy. The native fabric is intentionally a
small RTL-realizable structure: per-bank arbitration, back-pressure, explicit
response status and optional register slices. Its number of SRAM banks, data
width, bank mapping and pipeline depth are configuration values pending the
target SRAM macro, frequency and PD constraints. The architecture therefore
does not freeze an arbitrary 256-bit local datapath — and the C++ schema goes
further by having no default at all for those three values, so a number nobody
chose cannot become a constant by accident. The shipped configurations state
128-bit x 4 banks x 2 pipeline stages and every report prints them labelled
provisional.

The local plane has two timing modes (D16). `annotated` never waits and is what
a full-system run uses; `arbitrated` blocks its requesters on a real per-bank
round-robin arbiter and is what proves the fairness and back-pressure the
architecture claims. Any contention figure must name which one produced it.

The matrix engine is the one requester that cannot meet the plane on those
terms, and decision record **D17** records why and what follows. The Sauria
source has feeder-to-controller compute stalls, but its SRAM ports have no
response-valid or ready input: a read is captured on a fixed two-cycle schedule
that a memory cannot defer. A late answer from an arbitrated bank would
therefore be latched as though it were data. So the engine **stages tiles** —
operands are prefetched into private staging stores over `neo_local_sram_if`,
the array runs against those stores at the source's own timing, and results are
written back the same way.

Prefetch and writeback are ordinary local-plane traffic and remain fully within
D16. What staging buys is that D16's back-pressure is paid where a controller
can wait, instead of inside a compute pipeline that cannot. The cost is that
only the compute term of a matrix run is cycle-correlated with the Sauria
source; `total_time = prefetch + source_compute + writeback`, and no report may
call the whole thing cycle-accurate.

Five distinct paths matter for correctness and for what timing numbers mean:

**Hart path.** Instruction fetch, scalar load/store and vector load/store leave
the hart through its existing TLM memory interface. Decode sends core-SRAM data
to the native local fabric, core-local MMIO to the AXI4-Lite control fabric and
addresses outside the core to the existing chip/global path. Vector memory
access is ordinary RISC-V load/store — `vle*`/`vse*` and friends — so it lands
on the same path as scalar traffic. There is no second, independent RVV master.

**DMA path.** The per-core DMA is owned by TPU_V3 and is independent of Sauria.
Its register file is an AXI4-Lite target. The transfer engine has a native
local-SRAM port and an external AXI4/NoC-facing port; it moves data between core
SRAM and chip/global/NoC-visible memory. It must not reuse
`control/sauria_dma.h`, access a `std::vector` backing store directly, or bypass
TLM routing, arbitration, bounds checks and response status.

**Matrix path.** The Sauria matrix engine is a target for control and an
initiator on the native SRAM fabric for operand/result traffic. It contains
only the matrix-multiply function and the minimum feeder/result-collection
machinery required to run it. The source `ConfigRegs` remains inside as the
signal-level configuration distributor; it is not the firmware interface,
which is TPU_V3's separate `SA_CONTROL` AXI4-Lite target. NPU-top functions
unrelated to matrix multiplication are outside this block. It is not a full
AXI4 NoC master. Phase 5 supports one tile with `M,N <= 64`; each B row occupies
one 64-lane staging vector and unused columns are zero-padded.

**Transform path.** One ImageTransform engine implements Im2Col and Col2Im. It
is controlled through AXI4-Lite MMIO and reads/writes core SRAM through its
native data port. The implementation is imported or extracted from the NPU
team's source behind a TPU_V3 adapter. The current v4.2 IFMAP feeder contains
Im2Col-related address generation, but no standalone Col2Im block has yet been
established; Col2Im must not be invented or inferred from PSM output layout
while that source and its semantic contract are pending.

**Remote path.** An address outside the core aperture goes to the chip-local
fabric; an address outside the chip aperture goes to the NoC endpoint. This is
a decode consequence, not a routing decision made per transaction: local
traffic never enters the mesh because the fabric never hands it over. Plan §9.2
requires exactly this, and it is also what makes the NoLoopback constraint
survivable. The VP++ external path is architecturally required because its
reset PC is in global boot ROM; NEO DMA is the bulk mover, but it is not the
only source that may cross the external boundary.

## 4. Memory model and ordering

* One coherent backing store per core SRAM. No caches anywhere in the initial
  architecture — not "caches disabled", **absent**. Adding one is a separate
  coherence decision with its own test plan (plan §11.3).
* Storage is backed sparsely in deterministic 4 KiB pages (D6). Logical memory
  and host memory are different quantities and neither may be quoted as the
  other: the largest Revision 1 configuration describes 1.25 GiB and resides in
  about 10 MiB until firmware writes to it.
* Program-visible ordering follows blocking TLM completion: when `b_transport`
  returns, the effect is globally visible to everything on that fabric.
* An asynchronous DMA, SA or transform job is *not* ordered against the hart by
  the TLM completion of its start write. The write that starts a job returns
  immediately (plan §9.4); the job's memory effects become visible when the job
  completes. The synchronization boundary is the
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
| SA complete/error | level, held until acknowledged through the SA status register |
| DMA complete/error | level, one independent DMA instance per core |
| ImageTransform complete/error | level, operation identifies Im2Col or Col2Im |
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
component documents what an active-reset does to work in flight. For DMA, SA
and ImageTransform that means: an in-flight job is abandoned, `busy` clears,
`done` does **not** set, live IRQ/status are cleared, and any admission slot is
released. An explicit firmware abort is counted separately; reset is not
reported as an abort event. For SA and DMA the component and native local fabric
are reset in the same hierarchy; resetting only the requester cannot retract a
native request already accepted by the fabric. Silently completing a queued old
beat after a hierarchical reset would be worse than either alternative.

## 7. Fidelity levels

The platform is not one model, it is a set of backends selected at
construction, and every reported number must name which was used.

| Level | CPU | SA / Transform / DMA | NoC | Use |
| --- | --- | --- | --- | --- |
| 64x64 bring-up | RISC-V VP++ functional + approximate cost | extracted INT8/INT32 Sauria 64x64; independent DMA; Transform unavailable until its source gate passes | fast or detailed | block and single-core integration |
| NoC detailed | RISC-V VP++ functional + approximate cost | same functional engines, reported geometry/datatype | `timing_mode::detailed` | contention, routing, back-pressure |
| 128x128 target | RISC-V VP++ functional + approximate cost | NPU-team 128x128 Sauria update behind the same contract | either | target NEO-CORE integration after promotion gate |

For the same supported operation and datatype, logical results must match the
accepted golden model. A cross-datatype 64x64-versus-128x128 comparison is not
an equivalence test. Timing and utilization may differ and must identify the
selected source, geometry and datatype.

What may **not** be said about any of these (plan §14.4): none is cycle
accurate at the system level, RISC-V VP++ does not model Google TPU pipeline
timing, a 64x64 bring-up array is not 128x128 timing, and none of it is TPUv3
RTL equivalence.

## 8. Component responsibilities, one line each

| Component | Owns | Must not |
| --- | --- | --- |
| `architecture_config` | validated, strongly typed configuration | read YAML, touch SystemC |
| `address_map` | every base, size and stride; overflow-checked arithmetic | be duplicated anywhere else |
| `core_sram` | storage, byte enables, arbitration, per-requester counters | `wait()` inside `b_transport` |
| `neo_dma` | descriptor execution and TLM data movement | include or call the Sauria DMA; touch backing memory directly |
| `sauria_matrix_engine` | GEMM semantics, Sauria adapter, async worker, counters | retain unrelated NPU-top behavior; claim 64x64 is 128x128 |
| `image_transform_engine` | Im2Col/Col2Im descriptors and verified transform semantics | guess missing Col2Im behavior |
| `neo_control_fabric` | 32-bit AXI4-Lite MMIO decode and response routing | carry accelerator bulk data; claim signal-level AXI accuracy |
| `neo_local_sram_fabric` | native per-bank arbitration, back-pressure, ownership and counters | become a full AXI data crossbar; expose backing pointers |
| `external_bridge` | adapt outbound VP++/DMA and inbound remote traffic at the chip/NoC boundary | let SA/ImageTransform bypass local SRAM staging or inbound traffic bypass arbitration |
| `tpu_core` | composition, IDs, IRQ aggregation, reset sequencing | implement component behaviour |
| `tpu_chip` | two cores, chip aperture, outbound arbitration, inbound decode | expose more than one NoC manager |
| `chip_noc_endpoint` | placement, burst chunking, ownership, bypass | make TPU-specific changes inside routing primitives |
| `tpu_v3_soc` (platform) | config parsing, instantiation, placement, firmware, metrics, packaging | contain reusable IP behaviour |

Revision 1 implements the AXI4-Lite control plane and external AXI4 boundary
with SystemC/TLM sockets and explicit timing; it does not model AXI channels,
IDs or handshakes signal by signal unless a later fidelity decision adds such
a backend. The local SRAM plane is native by architecture, not “AXI-like.”

## 9. Known architectural blockers

**The NPU-team delivery boundary is not yet complete.** The v4.2 source proves
a 64x64 Sauria configuration and contains Im2Col-related address generation,
but the standalone Transform block, especially Col2Im, has not been located.
The 128x128 Sauria extension is also a future NPU-team delivery. This does not
block core SRAM, D15 fabrics or independent DMA work. It does block declaring
the Transform gate or 128x128 promotion gate complete. Both deliveries require
an immutable source revision, interface audit, provenance record and
cross-check against the accepted NPU-team golden tests.

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
not change, the detailed NoC RTL cross-check stays valid. It is a **Phase 9
prerequisite**, not a problem to discover during multi-chip traffic. Background in
`TPU_V3_PHASE0_AUDIT.md` §5.1.

**CPU identity/reset-PC prerequisite is closed.** Decision record D5 added
`hart_id` and `reset_pc` to `cdc::cpu::cpu_config` as static construction-time
properties, and the Phase 2 wrapper/tests prove them. Full architectural reset
semantics remain open before Phase 7; `reset_cpu()` is currently a restart plus
cache reinitialisation, not a complete register/CSR reset.

**RISC-V VP++ is a separate backend, not the existing Bremen wrapper.** TPU_V3 uses
a thin wrapper around the RV32+RVV ISS portions of
`ics-jku/riscv-vp-plusplus`; it does not instantiate the complete upstream
platform and does not replace `cpu_models/riscv_vp` for existing CDC-VP
platforms. Spike remains an external differential oracle, not the runtime CPU.
The pinned VP++ revision and `VLEN=512`/`ELEN=64` support passed the Phase 2
gate; `TPU_V3_PHASE2_AUDIT.md` remains the evidence.

**No vector multilib in the cross toolchain.** Compiling `rv32gcv_zvl512b` is
proven; linking against libc for a vector build is not. Decision record D4:
Phase 2 uses a freestanding `-ffreestanding -nostdlib -nostartfiles`
environment with a project-owned `crt0.S`, linker script and trap entry, so the
RVV smoke test depends on neither libc nor libm. A later firmware phase may use
the scalar `rv32imafdc/ilp32d` multilib for scalar library routines provided
the ABI matches and the choice is recorded in the build manifest. See audit §3.
