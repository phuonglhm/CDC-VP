# Standalone NEO-CORE Microbenchmark and Design-Space Exploration Plan

Status: **active; G0 and G1 complete, G2 next, 2026-08-25**

This document is the current execution plan for TPU_V3. It intentionally
narrows the work to **one standalone NEO-CORE**. Dual-core chip composition,
chip-local interconnect, NoC endpoints, mesh composition and multi-chip tests
are not prerequisites and must not be used to delay or qualify this work.

The previously completed Phase 8/9 source and audit material is retained as
historical evidence. It is not deleted, but it is outside the active scope
until the project owner explicitly reopens chip or NoC integration.

## 1. Mandatory execution preflight

Run this block in the same shell invocation immediately before every configure,
build, test or simulator run. Skipping it has caused the environment to hang.

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
# sanity
$CC -dumpfullversion
$CXX --version | head
```

The block selects the host compiler. Guest firmware continues to use the
pinned RISC-V cross-toolchain through its documented absolute path or CMake
configuration.

## 2. Objective

Characterize one NEO-CORE across representative vector and matrix workloads,
identify where time and traffic are spent, and produce an evidence-based set of
candidate configurations rather than selecting VLEN, SRAM banking or DMA
resources by intuition.

The result is a **Pareto set**, not automatically one largest configuration:

* highest measured performance;
* near-highest performance with lower hardware-cost proxies;
* balanced configuration for the intended workload mix.

A configuration cannot be called "best" until an objective and constraints
are stated. In particular, the SystemC model currently has no calibrated area,
power or post-PD frequency model.

## 3. Active architecture boundary

The D15 single-core diagram is the visual source of truth:

```text
components/TPU_V3/docs/neo_core_architecture-d15.drawio
components/TPU_V3/docs/neo_core_architecture-d15.jpg
```

Three labels in the legacy JPG are interpreted as follows:

* `SAURIA (SA)` means the architectural **MXU**;
* `ImageTransform` means **Transform (Im2Col)**; Col2Im is unavailable;
* `AXI4/NoC bridge` is, for this work, a generic **external memory interface**
  connected directly to the standalone Boot ROM/global RAM/host-I/O testbench.
  No NoC is instantiated.

```text
standalone_neo_core
├── one RISC-V VP++ RV32GCV hart
│   ├── scalar RV32 execution
│   └── RVV 1.0 vector execution
├── one 32-bit AXI4-Lite MMIO control fabric
├── one native banked local-SRAM data fabric
├── one Core SRAM
├── one independent NEO DMA
├── one 64x64 INT8/INT32 MXU
├── one Transform block with Im2Col only
└── one direct external-memory interface for hart fetch/global traffic and DMA
```

Scalar and RVV are two execution portions of one hart. RVV is not an MMIO
accelerator and has no independent memory master.

### 3.1 Explicitly out of current scope

* `tpu_chip` and any fixed core count per chip;
* chip-local fabric and shared chip endpoint;
* FlooNoC or another mesh;
* placement, routing, virtual channels and multi-chip contention;
* claims about maximum chip/core count derived from NoC identifiers;
* the unpromoted 128x128 MXU;
* Col2Im.

## 4. Verified starting point

Phase 7 already composed the required component. Do not rebuild it from
scratch:

* `tpu_core` contains the hart, Core SRAM, three D15 planes, DMA, MXU,
  Transform, interrupt aggregation and hierarchical reset;
* `fw/TPU_V3_SoC/neo_core_pipeline` drives
  `DMA -> Transform (Im2Col) -> MXU -> RVV` from one firmware ELF;
* `test_neo_core_pipeline` binds `neo.external()` directly to a memory/host-I/O
  target, without a NoC, and checks results against an independent host golden;
* Phase 7 structurally proves that instruction and data use the hart's unified
  bus and that MMIO, local SRAM and external traffic all occur.

The first implementation gate is to reproduce this result with the standalone
identity `mhartid=0`, then reuse its binding pattern for the benchmark harness.

## 5. Benchmark contract

Every benchmark has two independent obligations:

1. exact functional agreement with an independent host golden model;
2. a measurement record whose configuration, input shape, seed, mode and model
   limitations are explicit.

### 5.1 Arithmetic baseline

* RVV-only ReLU and reduction inputs use signed INT32 values small enough that
  the specified result cannot overflow INT32.
* MXU operands use the verified `INT8 x INT8 -> INT32` profile.
* Random inputs are deterministic and generated from the recorded seed.
* Edge patterns include all-negative, all-positive, mixed signs, zero and
  representable boundary values where relevant.
* No timing conclusion may depend on random values unless the model later adds
  a documented data-dependent feature such as sparsity or zero skipping.

### 5.2 MB1 — ReLU on RVV

```text
y[i] = max(x[i], 0), signed INT32
```

Ten element counts deliberately surround the SEW=32 lane boundaries for
VLEN 128, 256 and 512:

```text
N = 3, 4, 5, 7, 8, 9, 15, 16, 17, 1024
```

This benchmark measures RVV instruction reduction, tail utilization and local
memory pressure. The scalar implementation is retained as a correctness and
speedup baseline.

### 5.3 MB2 — Vector dot product on RVV

`Vector x Vector` is frozen here as a dot product rather than an element-wise
Hadamard product, because ReLU already supplies an element-wise bandwidth case.

```text
result = sum(a[i] * b[i])
N      = 3, 4, 5, 7, 8, 9, 15, 16, 17, 1024
```

Inputs are bounded so the INT32 result is exact. If an element-wise vector
multiply is later required, add it as MB2b; do not silently change MB2 because
the reduction and non-reduction traffic are different experiments.

### 5.4 MB3 — Vector by matrix (GEMV)

```text
A[1 x K] * B[K x N] -> C[1 x N]
```

Run every case through both an RVV implementation and the MXU implementation
where accepted:

```text
(K,N) = (4,4), (8,8), (16,16), (32,32), (64,64),
        (128,16), (128,64), (256,16), (256,64), (255,63)
```

The MXU uses one of 64 rows for `M=1`; this is intentional. The comparison
locates the crossover at which launch/data-movement overhead and low PE
utilization make RVV preferable to MXU.

### 5.5 MB4 — Matrix by matrix (GEMM)

```text
A[M x K] * B[K x N] -> C[M x N]
```

The verified engine accepts one tile with `M,N <= 64`. Ten cases cover scale,
rectangular utilization, non-power-of-two masking and longer K:

```text
(M,N,K) = (4,4,4), (8,8,8), (16,16,16), (32,32,32),
          (64,64,64), (16,64,64), (64,16,64), (63,63,64),
          (64,64,128), (64,64,256)
```

Problems requiring `M>64` or `N>64` belong to a later explicit tiling phase;
the current MXU adapter must continue to refuse them rather than truncate.

### 5.6 Transform coverage

MB1–MB4 do not exercise Im2Col. That is deliberate: the initial study isolates
RVV, MXU, DMA and SRAM behavior. Retain the existing Phase 7
`DMA -> Im2Col -> MXU -> RVV` pipeline as the Transform integration gate. Add a
separate convolution-shaped benchmark only after MB1–MB4 measurements are
trusted.

## 6. Two measurement modes per case

### 6.1 Kernel-only

Inputs begin in Core SRAM and results remain there:

```text
Core SRAM -> RVV/MXU -> Core SRAM
```

This mode isolates compute, native-fabric behavior and SRAM banking. Host-side
debug writes used to initialize memory are outside the measured interval.

### 6.2 End-to-end

Inputs begin in external RAM and the final result is copied back:

```text
external RAM -> DMA-in -> Core SRAM -> RVV/MXU
             -> Core SRAM -> DMA-out -> external RAM
```

Only this mode may be used to reason about DMA sufficiency or the benefit of
overlapping movement and compute. Preloading SRAM and then quoting the run as a
DMA benchmark is prohibited.

## 7. Reference configuration and experimental knobs

### 7.1 Reference configuration

```text
XLEN                         32
RVV                          1.0
VLEN                         512 bits
ELEN                         64 bits
MXU                          64x64, INT8 x INT8 -> INT32
DMA engines/channels         1
DMA max burst                2048 bytes
external AXI data width      64 bits
Core SRAM capacity           16 MiB
local SRAM bank width        128 bits
local SRAM banks             4
bank mapping                 low-order interleaved
fabric pipeline stages       2
outstanding/requester        1
arbitration                  round-robin
core model period            10 ns
```

The period and physical SRAM values are provisional model inputs, not
post-layout frequency or SRAM-macro claims.

### 7.2 Knob readiness

| Knob | Candidate values | Current status |
| --- | --- | --- |
| VLEN | 128, 256, 512 | **512 only today.** The schema rejects other values; a sweep requires a separately gated experimental VP++ configuration and toolchain/ELF contract. |
| Local bank width | 64, 128, 256 bits | Configurable now, but reports must keep the values labelled provisional. |
| Local bank count | 1, 2, 4, 8 | Configurable now. Use `arbitrated` mode for observable contention. |
| Fabric pipeline | 1, 2, 3 stages | Configurable now. |
| DMA max burst | 64, 256, 512, 2048 bytes | Configurable now; alignment still limits an individual frame. |
| DMA channels | 1, 2, 4 | **One only today.** More channels require real workers, register state, requester identity, arbitration and tests; changing a report field does not count. |
| SRAM capacity | 1, 4, 16 MiB | Useful for footprint/tiling, not by itself a throughput knob. |
| MXU geometry | 64x64 | 128x128 remains source-gated on an NPU-team implementation and golden evidence. |
| External memory | latency, bandwidth, outstanding | **Not calibrated today.** A parameterized back-pressured memory model is required before DMA-channel conclusions. |

### 7.3 Timing modes

Use both, and never mix their claims:

* `annotated`: fast functional/reference regression; never blocks and therefore
  cannot prove bank contention or fairness;
* `arbitrated`: required for local-fabric bottleneck experiments because bank
  ownership, back-pressure and round-robin wait are real model behavior.

## 8. Required measurements

Every result row must include the following or explicitly say `unavailable`:

### 8.1 Identity and reproducibility

* configuration ID and full configuration manifest;
* benchmark, case index, shape, datatype and deterministic seed;
* kernel-only or end-to-end mode;
* firmware ELF SHA-256 and relevant source revisions;
* Release/Debug build type and timing mode;
* correctness status and output checksum.

### 8.2 Total and hart

* measured interval start/end and simulated cycles/time;
* retired instructions;
* scalar and vector instruction counts;
* active elements, tail elements and effective lane utilization;
* local, control and external hart transactions/bytes.

### 8.3 DMA and external memory

* DMA-in, DMA-out and total bytes;
* chunks/transfers and average effective chunk size;
* busy, service and wait/stall cycles;
* achieved bytes/cycle;
* overlap with compute;
* external target latency, bandwidth and outstanding parameters.

The current DMA `chunk_latency` is only an annotated cost and is explicitly not
a throughput claim. Do not use it alone to justify a second DMA channel.

### 8.4 Local SRAM fabric

* bytes, requests and errors per requester;
* grants per requester and bank;
* bank conflicts and arbitration wait cycles;
* peak/average outstanding count;
* achieved bytes/cycle and percent of configured peak.

### 8.5 MXU

* prefetch, source-compute and writeback cycles separately;
* M/N/K and active-array utilization;
* operand/result bytes and native requests;
* useful operations/cycle.

Only the source-compute term is cycle-correlated with the pinned Sauria
implementation. Total MXU time includes model-defined staging and must not be
called RTL cycle accurate.

## 9. Bottleneck classification rules

The report must derive a reason from counters rather than label the slowest
block by inspection:

| Evidence | Interpretation |
| --- | --- |
| RVV instruction count falls with VLEN but total time does not | memory/data-path bound; a wider VLEN alone is not useful |
| high RVV tail fraction | vector-length/shape utilization loss |
| DMA busy while compute is idle | DMA/external-memory supply bottleneck |
| more DMA requesters increase local wait without increasing bytes/cycle | local fabric is saturated; more channels are counterproductive |
| high per-bank wait with idle banks elsewhere | bank mapping or access-stride conflict |
| MXU prefetch+writeback dominates source-compute | local data movement dominates matrix compute |
| GEMV MXU utilization is near one active row out of 64 | compare against RVV; the MXU is under-filled by construction |
| GEMM 64x64 compute dominates and array utilization is high | workload is compute-suited to the MXU |
| footprint exceeds backed SRAM | capacity/tiling issue, not bandwidth |

## 10. Experiment sequence

| Gate | Status | Evidence / next action |
| --- | --- | --- |
| G0 | **Complete** | D27, this plan, README and architecture documents define one standalone core and mark Phase 8/9 outside current scope |
| G1 | **Complete** | `neo_core_microbench`, `tpu_v3_neo_core_d27_baseline`, the dependency-guard negative control and `TPU_V3_STANDALONE_DSE_AUDIT.md` |
| G2 | **Next** | implement the result schema and MB1 ReLU ten-case correctness baseline before adding MB2–MB4 |
| G3–G6 | Pending | start only after the preceding gate closes |

### Gate G0 — documentation and harness contract

* This document is linked from the component README and live implementation
  plan.
* Historical Phase 8/9 work is marked outside the active scope, not deleted.
* Benchmark names, arithmetic, shapes, modes and output schema are frozen.

### Gate G1 — standalone baseline reproduction

* Instantiate exactly one `tpu_core` as chip 0/core 0, `mhartid=0`.
* Bind its external socket directly to Boot ROM/global RAM/host I/O.
* Run the existing Phase 7 pipeline unchanged in result semantics.
* Record zero linked or instantiated NoC components in the benchmark manifest.

### Gate G2 — benchmark correctness

* MB1–MB4, ten cases each, pass independent golden comparison.
* GEMV runs both RVV and MXU implementations where supported.
* Kernel-only and end-to-end intervals exclude host setup/debug initialization.
* Negative controls corrupt at least one arithmetic operation, one DMA leg and
  one measurement-boundary marker and are detected.

### Gate G3 — measurement conservation

* Stage byte totals reconcile with tensor footprints.
* DMA bytes reconcile with external-memory bytes.
* Local requester bytes reconcile with SRAM committed bytes after accounting
  for reads versus writes and explicitly named setup traffic.
* Stage times reconcile with the total interval, including overlap.
* Every counter used in a conclusion has a mutation/negative control.

### Gate G4 — supported-knob screening

Start with one-factor-at-a-time screening around the reference configuration:

* local bank width;
* bank count;
* pipeline depth;
* DMA burst size;
* SRAM capacity where footprint is relevant.

Do not first run the complete Cartesian product. The nominal six-knob grid
including experimental VLEN and DMA-channel values is 1,296 configurations,
or 51,840 benchmark/case runs before modes and repeats.

### Gate G5 — model extensions justified by screening

Implement an experimental VLEN sweep, calibrated external-memory model or
multiple DMA channels only when G4 evidence states what question the extension
will answer. Each extension needs its own configuration validation, functional
gate and negative control before its performance result is accepted.

### Gate G6 — targeted exhaustive sweep and Pareto report

* Exhaustively combine only the sensitive knobs and promising values selected
  by G4/G5.
* Report per-workload results plus a stated weighted aggregate; do not hide a
  workload regression inside an arithmetic average.
* Publish the Pareto frontier with explicit hardware-cost proxies.
* Keep functional/model results separate from RTL synthesis, area, power and
  post-PD frequency evidence.

## 11. Harness and output layout

Proposed source layout; create it only as each gate begins:

```text
components/TPU_V3/microbench/
├── CMakeLists.txt
├── include/tpu_v3/bench/
├── src/
└── tests/

fw/TPU_V3_NEO_CORE_MICROBENCH/
├── common/
├── relu/
├── vector_dot/
├── gemv/
└── gemm/

out/neo_core_microbench/
├── bin/
├── firmware/
├── configs/
├── results/raw/
├── results/summary/
└── manifest.json
```

One command-line invocation runs one fresh case so state cannot leak between
configurations:

```text
neo_core_microbench \
  --config <config.yaml> \
  --benchmark <relu|vector_dot|gemv_rvv|gemv_mxu|gemm> \
  --case <0..9> \
  --mode <kernel|end_to_end> \
  --seed <integer> \
  --result <row.json>
```

The runner emits machine-readable JSON for one case. A separate aggregation
script validates manifests and produces CSV/plots; it must never parse
human-readable simulator logs as the source of numeric truth.

## 12. Accuracy boundaries

* RISC-V VP++ is an instruction-functional model. Its vector memory interface
  currently emits element-wise TLM accesses; a 512-bit register operation is
  not automatically one 64-byte SRAM transaction.
* Reduced RVV instruction count is evidence about software/model execution,
  not a calibrated RTL lane-throughput result.
* The local fabric in `arbitrated` mode provides architectural contention
  evidence, not post-layout timing.
* DMA channel benefit is unknowable without a bandwidth/back-pressure model on
  the external side.
* MXU source-compute timing is the strongest cycle-correlated portion; staging,
  firmware, RVV and total-system timing have different fidelity.
* No result from this standalone study is a NoC or multi-core performance
  result.

## 13. Immediate implementation task

G1 is complete. Implement the **G2 MB1 foundation** next:

1. create `components/TPU_V3/microbench/` for benchmark definitions, case
   validation and the machine-readable result writer; do not copy `tpu_core`;
2. create the freestanding MB1 ReLU firmware under
   `fw/TPU_V3_NEO_CORE_MICROBENCH/relu/`;
3. run the ten frozen MB1 sizes in scalar and RVV variants and compare every
   output element with an independent host golden;
4. record kernel-only and end-to-end boundaries explicitly, but do not draw a
   performance conclusion until G3 validates the counters;
5. add corrupt-arithmetic and corrupt-boundary negative controls;
6. keep the G1 dependency guard on every new standalone executable.

Do not begin a VLEN sweep in G2. The current VP++ build is hard-locked to
VLEN=512; alternate VLEN values require the model-extension gate G5.
