# Standalone NEO-CORE Microbenchmark and Design-Space Exploration Plan

Status: **active; G0–G4 complete. MB1–MB4 are implemented and gated, every
run is conservation-checked and the supported knobs are screened; G5 is next.
2026-08-27**

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

D28 fixes Neo Lite C1 and C2 as the first two executable target profiles. The
measurements compare and diagnose those two profiles; they do not permit a
current reference binary to be relabelled as either one.

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

### 7.1 Current implementation reference

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

This is the baseline implemented before D28. It is neither Neo Lite C1 nor Neo
Lite C2; `--config-id reference` continues to name only these exact values.

### 7.2 Knob readiness

| Knob | Candidate values | Current status |
| --- | --- | --- |
| VLEN | C1: 256; C2: 512 | **512 only today.** C1 requires a separately gated VP++ build and `zvl256b` firmware contract. |
| Local bank width | C1: 128; C2: 256 bits | Configurable now, but reports must keep the values labelled provisional until profile promotion. |
| Local bank count | C1: 4; C2: 8 | Configurable now. Use `arbitrated` mode for observable contention. |
| Fabric pipeline | 1, 2, 3 stages | Configurable now. |
| DMA max burst | C1: 8 x 16-byte beats = 128 bytes; C2: 8 x 32-byte beats = 256 bytes | Byte limits are configurable now, but beat size is still fixed at 8 bytes and must become profile-driven. |
| DMA channels | C1: 2; C2: 4 | **One descriptor/worker only today.** More channels require real register state, workers, shared-path arbitration and tests; changing a report field does not count. |
| SRAM capacity | C1: 768 KiB; C2: 1536 KiB | Exact targets. The current power-of-two validator refuses both; rounding to 1/2 MiB is forbidden by D28. |
| MXU geometry | C1: 32x32; C2: 64x64 | **64x64 only today.** C1 needs a named, hash-verified `int8_32x32` extraction and golden rather than a template default. |
| External AXI width | C1: 128; C2: 256 bits | **64 bits only today.** Width, framing and serialized read/write contention must become live model behaviour. |
| Core period | C1/C2: 1.25 ns | Configurable only on part of the current core timing path; promotion must prove every timing consumer used in a claim receives it. |
| External memory timing | latency, bandwidth, outstanding | **Not calibrated today.** A parameterized back-pressured memory model is required before DMA-channel or AXI-width conclusions. |

### 7.3 Approved executable target profiles

D28 in `TPU_V3_DECISION_RECORD.md` freezes two target profiles for this
standalone study. They are not labels for the current reference binary: each
must pass D28's identity and behavioural promotion gate before a row may use
the corresponding configuration ID.

| Field | Neo Lite C1 | Neo Lite C2 |
| --- | ---: | ---: |
| MXU | 32x32, 1024 PE | 64x64, 4096 PE |
| arithmetic | INT8 x INT8 -> INT32 | INT8 x INT8 -> INT32 |
| RVV | XLEN 32, VLEN 256, ELEN 64 | XLEN 32, VLEN 512, ELEN 64 |
| core period | 1.25 ns (800 MHz) | 1.25 ns (800 MHz) |
| backed Core SRAM | 768 KiB | 1536 KiB |
| local SRAM | 4 banks x 128 bits | 8 banks x 256 bits |
| DMA | one controller, 2 channels | one controller, 4 channels |
| external AXI | 128 bits | 256 bits |
| normal maximum burst | 8 beats = 128 bytes | 8 beats = 256 bytes |
| Transform | one Im2Col INT8 | one Im2Col INT8 |

PE count, frequency and burst bytes are derived fields, not independent knobs.
The exact SRAM sizes must not be rounded to power-of-two substitutes. MXU/VLEN
may be separate build artifacts, because both are compile-time properties
today, but a build must refuse a profile name that differs from its live model.

The target list does not specify external-memory latency, AXI clock relation,
SRAM pipeline depth or outstanding limits. Rows must state those common VP
inputs explicitly and label them provisional; no C1-versus-C2 performance
claim is accepted until their timing effects are modelled and gated.

### 7.4 Timing modes

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
| G2 | **Complete; MB3/MB4 implementation awaits independent review** | `components/TPU_V3/microbench`, `neo_core_bench_runner`, five distinct firmware ELFs under `fw/TPU_V3_NEO_CORE_MICROBENCH`, all frozen MB1–MB4 cases in both modes and every supported implementation, edge patterns and classified negative controls. Regression after building `neo_core_bench_runner` and `rvv_smoke_image`: microbench 212/212 PASS; full `tpu_v3` 271/271 PASS, 0 SKIP |
| G3 | **Complete; awaits independent review** | eight conservation identities carried in every row, seven of them on every run; per-stage timing and an exact stage sequence derived from the guest's own marks; three model-side mutations plus a comparator mutation per identity. An unbalanced identity fails the run |
| G4 | **Complete; awaits independent review** | `components/TPU_V3/microbench/screening`, 170 one-factor points over five knobs and ten workload/mode combinations (five benchmarks in both modes), each passing its golden and its G3 identities. Sensitive: pipeline depth (21–62%), bank width (55–84% MXU, 1.4–9.8% hart end-to-end) and DMA burst (0.3–7.3%, end-to-end only). Inert: bank count and SRAM capacity, both with a mechanism. Reproducible by `ctest -L g4_full`; five controls plus aggregation unit tests |
| G5 | **Next** | D28's C1/C2 profiles, by the work breakdown in `NEO_LITE_C1_C2_IMPLEMENTATION_PLAN.md` starting at WP0 |
| G6 | Pending | start only after G5 closes |

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

**MB1 status: implemented and gated, not signed off.** Five review rounds on
2026-08-26 and 2026-08-27 found twenty-six defects between them — a blocker
that let a negative control pass while the mutation never executed, rows
mislabelled `reference`, a command line that accepted values it could not
represent, TLM contract violations in the testbench target, inverted controls
that turned a skip into a pass, measurements §8 requires that the row omitted,
a reserved D28 profile identity that could label a machine that is not that
profile, shared firmware buffers too small for frozen MB3 and MB4 cases, a
rejected row whose verdict was invisible to an aggregator, validity coupled to
arithmetic correctness, and provenance/rejected-row controls that did not test
the runner contract they claimed. All are fixed and each fix carries a control;
`TPU_V3_STANDALONE_DSE_AUDIT.md` §6 to §10 record them. The round-5 fixes have not
themselves been independently reviewed. Ten sizes, scalar and RVV, both modes, element-wise
comparison against a host golden that links neither SystemC nor the core model;
the four §5.1 edge patterns at the lane-boundary case; and seven controls, each
naming through `--expect-detect` the detection that must fire, so a control
cannot pass by failing for an unrelated reason. Evidence and the three
documented departures from §11 are in `TPU_V3_STANDALONE_DSE_AUDIT.md` §5.

**MB2 status: implemented and gated, one review round.** Ten sizes, scalar and RVV,
both modes, its own ELF, the four edge patterns at the lane-boundary case and
five mutations plus two cross-image controls. Its operand bound is
`dot_product_magnitude_bound(N)` and its golden throws rather than wraps.

**MB3–MB4 status: implemented and gated; independent review pending.** MB3
runs each of its ten frozen GEMV shapes through RVV and MXU, in kernel and
end-to-end modes. MB4 runs its ten frozen GEMM shapes through MXU in both
modes. Inputs are row-major INT8 tensors staged as `A` followed by `B`; the
independent host golden accumulates through INT64 and refuses a result outside
INT32 rather than wrapping. The RVV image uses signed widening multiply and
reduction with strided B-column loads. The two MXU images program the live
SA control plane and their rows populate §8.5 from live job, timing and native
traffic counters. Each path also passes the four edge patterns, arithmetic,
DMA-in, DMA-out and boundary controls; three wrong-image controls prove the
new ELFs refuse another benchmark identifier.

### Gate G3 — measurement conservation

* Stage byte totals reconcile with tensor footprints.
* DMA bytes reconcile with external-memory bytes.
* Local requester bytes reconcile with SRAM committed bytes after accounting
  for reads versus writes and explicitly named setup traffic.
* Stage times reconcile with the total interval, including overlap.
* Every counter used in a conclusion has a mutation/negative control.

**Status: met, pending independent review.** Eight identities are evaluated
with their reason carried into every row — seven on every run, plus
`dma_leg_bytes` in end-to-end mode where a leg exists to reconcile:

| Identity | Reconciles | Clause |
| --- | --- | --- |
| `tensor_footprint` | core SRAM debug-written bytes against the input plus output tensor | 1 |
| `kernel_local_bytes` | the kernel requester's local bytes against what its algorithm declares it moves | 1 |
| `dma_local_bytes` | the DMA requester's local bytes against the legs it must run | 1, 2 |
| `dma_external_bytes` | the DMA's external-path bytes against the same legs | 2 |
| `dma_leg_bytes` | the DMA's local-path total against the tensor it was asked to move | 1, 2 |
| `external_boundary_bytes` | the core's external bytes against what the memory and host-I/O window served | 2 |
| `local_plane_bytes` | fabric requester bytes against core SRAM's own read plus written bytes | 3 |
| `interval_accounted_ns` | the sum of stage spans against the measured interval | 4 |

Wherever the model allows, an identity pairs an observer inside the core with
one outside it: two counters incremented by the same line of code agree by
construction and prove nothing. `external_boundary_bytes` is the clearest case
— the memory is outside the model, so neither end can satisfy it alone.

Two identities exist because an agreeing total is not a correct total.
`local_plane_bytes` cannot notice a kernel that reads its input twice, since
both of its observers would report the larger figure; `kernel_local_bytes`
compares the measurement against the traffic the algorithm declares.
Similarly `external_boundary_bytes` stays balanced when an extra DMA
transaction increments both of its sides, so `dma_external_bytes` gates that
volume on its own.

Clause 4 needs the stage **sequence** as well as the sum: a missing
intermediate marker only widens a neighbouring span, leaving
`interval_accounted_ns` balanced. The expected sequence is therefore checked
exactly for the mode being run.

An unbalanced identity is a `conservation` failure and makes `run_valid` false.
A row whose bytes do not reconcile is not a slower correct measurement; it is a
measurement of something nobody can name.

Overlap (clause 4) is currently zero by construction: MB1–MB4 run their DMA
legs and their kernel in sequence. `interval_accounted_ns` therefore partitions
the interval rather than allowing for overlap, and the identity would fail if a
future benchmark overlapped two stages — which is the correct signal to extend
it rather than to relax it.

Clause 5 is met in three classes, kept apart because they prove different
things, and the plan records what each does **not** prove.

Three controls are **model-side**: a skipped DMA leg, a kernel that re-reads
its input, and a dropped stage marker. The last two are each caught by exactly
one check and by nothing else, which is what makes those checks load-bearing.

The remaining identities compare two honest observers of the same bytes, which
nothing outside the model can make disagree. Two **comparator-side** classes
cover them: `--inject-accounting` perturbs an identity's total and proves the
equality check works, and `--inject-counter` perturbs one term where it enters
an identity — seventeen sources, one control each, with a test that keeps the
matrix complete in both directions — and proves every term of a
sum is actually wired in. A mutation on a total cannot do the second: several
identities sum more than one counter, and a term that was dropped, doubled or
transposed would leave the aggregate mutation still detected. The runner
refuses a source no identity reads and fails when a perturbed source goes
unnoticed.

**Accepted limitation.** For counters whose two observers cannot be made to
disagree from outside the model, no control proves the counter observes the
hardware; it proves the accounting around it is sound. Closing that would need
per-counter test hooks inside the components, which is a change to signed
components and outside G3's scope. Any conclusion resting on such a counter
must name it.

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

**Status: met, pending independent review.** 170 one-factor points over five
knobs and ten workload/mode combinations — five benchmarks in both modes — run by
`components/TPU_V3/microbench/screening/neo_core_screen.py`, which reads result
rows rather than logs and refuses to summarise a run whose bytes did not
reconcile.

| knob | verdict | mechanism |
| --- | --- | --- |
| `fabric_pipeline_stages` | sensitive everywhere, 21–62% | every requester pays the response latency once per request, and the hart issues the most because VP++ decomposes vector accesses element-wise (D7) |
| `local_bank_width_bits` | sensitive wherever a wide requester runs: 55–84% on the MXU, 1.4–9.8% for hart kernels **end-to-end**, 0% for the same kernels alone | a 4-byte element-wise access is one beat at any width; tile staging and DMA chunks halve their beats each doubling |
| `dma_max_burst_bytes` | sensitive in four of five end-to-end workloads, 0.3–7.3%; inert in kernel-only mode, which has no DMA | 384 chunks at 64 bytes against 12 at 2048 |
| `local_bank_count` | inert | MB1–MB4 run their stages in sequence; 32 of 170 rows record a single DMA bank conflict from temporal decoupling at a stage boundary, which cannot move a 58 µs interval |
| `sram_capacity_bytes` | inert | sparse page-backed storage (D6); every frozen case fits at 1 MiB. A footprint knob, not a throughput one |

Bank count being inert is a structural result, not a flat curve, and it is what
stops G6 spending configurations on it. It becomes screenable when a workload
overlaps requesters — WP4's multi-channel DMA is the first such thing in the
Neo Lite plan.

The screening was run twice, `annotated` and `arbitrated`. Elapsed and stage
timing agree on every observation; bank conflicts and per-requester latency do
not — 32 rows against zero — which is consistent with those conflicts being a
decoupling artefact rather than contention. Neither run may be quoted as
contention or fairness (D16): that needs `arbitrated` **and** a workload with
genuinely overlapping requesters, and MB1–MB4 have no such workload.
Interactions are outside what one-factor screening can see, by construction.

### Gate G5 — promote the approved profiles and any justified extensions

Implement and promote D28's exact C1/C2 profiles here, including VLEN256,
explicit 32x32 MXU source extraction, exact non-power-of-two SRAM capacities,
multi-channel DMA and 128/256-bit serialized external AXI behaviour. D28 is the
project-owner authorization for those two targets; G4 does not need to
re-justify their existence. It still supplies the baseline measurements and
questions their comparison must answer.

The ordered implementation, review handoff and promotion details are in
`NEO_LITE_C1_C2_IMPLEMENTATION_PLAN.md`. That plan implements D28; it does not
change the profile values or move C1/C2 ahead of the G2–G4 prerequisites here.

Any value outside C1/C2 — an arbitrary VLEN sweep, another channel count or a
third geometry — still requires G4 evidence. Every extension needs its own
configuration validation, functional gate, behavioural mutation control and
live-identity report before its performance result is accepted.

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

G0–G4 are complete. MB1–MB4 pass their frozen cases against an independent
golden, every run reconciles its accounting identities, and the supported knobs
have been screened one at a time.

**The next task is G5**: implement and promote D28's Neo Lite C1 and C2
profiles, following the ordered work breakdown in
`NEO_LITE_C1_C2_IMPLEMENTATION_PLAN.md` from WP0. That plan is subordinate to
D28 — changing a work-package sequence does not change a profile value or waive
a promotion criterion.

What G4 hands it:

* the sensitive supported knobs are pipeline depth, bank width and DMA burst.
  The first two are C1/C2 profile values and the third is derived from AXI
  width and burst beats, so the profiles differ on axes already known to be
  live. That is a reason to expect a measurable C1/C2 difference, not evidence
  of one;
* bank count is inert until a workload overlaps requesters. WP4's
  multi-channel DMA is the first thing that creates one, and D28's promotion
  gate item 5 already requires a concurrent-channel test;
* `arbitrated` runs and agrees with `annotated` on every elapsed figure, so it
  is exercised but not yet tested in substance. WP4 is where that changes.

Two things that must not happen while G5 is open:

* **no performance conclusion from a knob outside C1/C2 without G4 evidence.**
  D28 pre-authorises exactly two model-extension targets and no others;
* **no C1/C2 identity from a label.** A build must refuse a profile name whose
  live components report something else, and D28's promotion gate item 8
  requires a negative control proving it.
