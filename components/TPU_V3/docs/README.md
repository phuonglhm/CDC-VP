# TPU_V3 Documentation Entry Point

> **MANDATORY FOR AI AGENTS AND HUMAN CONTRIBUTORS — active baseline
> 2026-08-25.** Read this file before using any other TPU_V3 document. The
> project has intentionally changed direction. Do not continue the old
> dual-core-chip, NoC or multi-chip schedule.

## Current direction

The only active machine is one standalone Phase 7 **NEO-CORE**:

```text
one RISC-V VP++ RV32GCV hart
├── scalar RV32 execution
├── RVV 1.0, VLEN=512, ELEN=64
├── one Core SRAM
├── one independent NEO DMA
├── one verified MXU 64x64, INT8 x INT8 -> INT32
├── one Transform block, Im2Col only
├── AXI4-Lite MMIO control plane
├── native banked local-SRAM data plane
└── direct Boot ROM/global RAM/host-I/O external binding

chip composition    absent
NoC                 absent
guest mhartid       0
```

The current goal is to characterize this one core with microbenchmarks and
select an evidence-based configuration. The frozen workloads are ReLU, vector
dot, GEMV and GEMM, each with ten cases. This is a standalone-core DSE, not a
SoC, mesh or scalability experiment.

## Current status and next task

```text
G0  documentation/scope contract                 COMPLETE
G1  one-core baseline + dependency evidence      COMPLETE
G2  benchmark correctness                        COMPLETE
    MB1 ReLU, MB2 vector dot                    scalar + RVV
    MB3 GEMV                                    RVV + MXU
    MB4 GEMM                                    MXU
    ten frozen cases, both modes, independent golden and controls
    MB3/MB4 code                                AWAITS INDEPENDENT REVIEW
G3  measurement conservation                    COMPLETE
    seven identities every run, eight end-to-end
    unbalanced                                  = run_valid false
G4  supported-knob screening                    COMPLETE
    sensitive: pipeline depth, bank width, DMA burst
    inert: bank count, SRAM capacity
    G4 code                                     AWAITS INDEPENDENT REVIEW
G5  C1/C2 promotion + justified extra extensions IN PROGRESS
    WP0 baseline lock + C1 feasibility          EVIDENCE COMPLETE,
                                                PENDING CLOSURE
      C1 MXU32 and VLEN256 source-proven
      Release 313/313, Debug 313/313, 0 skip
    WP1 canonical profile + refusal gate        EVIDENCE COMPLETE,
                                                PENDING CLOSURE
      C1 refused on 8 fields, C2 on 6, each
      naming the work package that owns it
      Release 325/325, Debug 325/325
    WP2..WP10                                   NOT STARTED

    no model code has changed: the machine is still one DMA
    channel, 64-bit AXI, 64x64 MXU and VLEN 512
G6  targeted exhaustive sweep/Pareto report      PENDING
```

No MB1–MB4 number is a validated measurement yet. G3 is what makes a
counter quotable; until it closes these rows are correctness evidence with
counters attached.

MB1 has been through five review rounds which found twenty-six defects between
them — the worst being a negative control that passed while the mutation under
test had never executed, a reserved D28 profile identity that could label a
machine that is not that profile, and a result row that wrote `passed: true`
for a run the identity gate had rejected. MB2 has been through one; the new
MB3/MB4 implementation awaits independent review. All known findings are fixed
and gated. See
`TPU_V3_STANDALONE_DSE_AUDIT.md` §6 to §10.

Do not label a current result `Neo Lite C1` or `Neo Lite C2`. D28 approves those
two exact executable targets, but VP++ is currently compiled for VLEN=512 and
the current NEO DMA has one descriptor/worker. G5 must promote the live MXU,
RVV, exact SRAM capacity, DMA-channel and external-AXI behaviour before either
configuration ID is valid. Arbitrary values outside C1/C2 still need G4
evidence before becoming model extensions.

## Required reading order

1. **This file** — current authority and document classification.
2. `NEO_CORE_MICROBENCH_DSE_PLAN.md` — benchmark contract, cases, metrics,
   fidelity limits and gates G0–G6.
3. `TPU_V3_DECISION_RECORD.md` D27 and D28 — formal one-core/no-NoC scope,
   correction of the old ManagerID/node-count reasoning, and the exact
   executable Neo Lite C1/C2 target profiles.
4. `NEO_LITE_C1_C2_IMPLEMENTATION_PLAN.md` — ordered common, C2-closure and
   C1-enablement work packages, tests, negative controls and promotion gates.
5. `TPU_V3_STANDALONE_DSE_AUDIT.md` — implemented G1/G2 evidence,
   the reproduction commands, and what each row cannot say.
6. `ARCHITECTURE.md` active section — the standalone component boundary.
7. `TPU_V3_PHASE5_AUDIT.md`, `TPU_V3_PHASE6_AUDIT.md` and
   `TPU_V3_PHASE7_AUDIT.md` — retained evidence for MXU, Transform and the
   reusable one-core composition.
8. `INTERFACE_CONTRACT.md` — generic TLM and D15 internal-plane rules. Its
   NoC-facing section is inactive under D27.

Where documents disagree, use this precedence:

```text
this README
  > NEO_CORE_MICROBENCH_DSE_PLAN.md
  > decisions D27/D28
  > NEO_LITE_C1_C2_IMPLEMENTATION_PLAN.md
  > active header/section of ARCHITECTURE.md
  > historical implementation plan and phase audits
```

## Historical/inactive material

The following files and sources are retained to preserve completed engineering
evidence. They are **not** a work queue, dependency or architecture target:

* `TPU_V3_PHASE8_AUDIT.md`;
* `TPU_V3_PHASE9_AUDIT.md`;
* `TPU_V3_PHASE9_NOC_REBASELINE.md`;
* `R-P9-1_DECISION_REQUEST.txt` and `R-P9-2_DECISION_REQUEST.txt`;
* `block_diagram.txt` and `neo_core_TPUChip2DMeshNoCSystem.png`;
* `tpu_chip`, `chip_local_fabric`, `chip_noc_endpoint` and FlooNoC composition
  source/tests when working on the standalone DSE.

Do not delete this material. Do not infer from its presence that Phase 8/9 is
active. Reopening any chip or NoC work requires a new explicit project-owner
decision that supersedes D27.

## Implementation starting point

Reuse, do not fork, the existing Phase 7 composition:

```text
components/TPU_V3/tpu_core/                  tpu_core composition
components/TPU_V3/tpu_core/tests/neo_core/   G1 standalone harness
fw/TPU_V3_SoC/neo_core_pipeline/             reused G1 pipeline firmware
```

G1's `neo_core_microbench` target uses chip/core indices 0/0 only as retained
address-map inputs, requires guest-visible `mhartid=0`, binds
`neo.external()` directly to memory/host I/O and writes a manifest with
`chip_composition=false` and `noc_instantiated=false`. A configure/post-link
guard rejects chip or NoC implementation dependencies.

G2 adds benchmark code under the locations specified by
`NEO_CORE_MICROBENCH_DSE_PLAN.md`; it must not create a second `tpu_core` model.
Its complete MB1–MB4 correctness set is implemented:

```text
components/TPU_V3/microbench/            definitions, golden, result schema
components/TPU_V3/microbench/runner/     neo_core_bench_runner, one case per run
fw/TPU_V3_NEO_CORE_MICROBENCH/           five workload images, shared startup
```

`neo_core_bench_runner` is a separate executable from G1's
`neo_core_microbench`; the two have different command lines and reusing the name
would have broken the D27 baseline gate. Both carry the same dependency guard.
MB1 ReLU, MB2 vector dot, MB3 RVV GEMV, MB3 MXU GEMV and MB4 MXU GEMM each use
their own firmware image. Every image refuses a different benchmark identifier.
All ten frozen cases run in kernel and end-to-end modes; matrix operands are
INT8 and results are exact INT32 comparisons against the independent host
golden. The G2 regression is 212/212 microbench tests. After explicitly
building both `neo_core_bench_runner` and the unrelated `rvv_smoke_image`
prerequisite, the full `tpu_v3` regression is 271/271 PASS, 0 SKIP.

Every row carries `run_valid` beside `correctness.passed`: the first says the
run may be used at all, the second says its numbers match the golden. A row
whose live VLEN or MXU identity disagreed with its build has a correct
arithmetic result from a machine that is not the one the row describes.

## Mandatory build/run preflight

Run this block in the **same shell invocation** immediately before every
configure, build, test or simulator run:

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
# sanity
$CC -dumpfullversion
$CXX --version | head
```

Skipping the block has caused the environment to hang. It selects the host
compiler; guest firmware still uses the pinned xPack RISC-V cross-toolchain.

## Reporting limits

* VP++ vector loads/stores appear as element-wise TLM accesses. Do not report
  them as one 512-bit hardware transaction.
* The G1 local fabric runs in `annotated` mode. Performance/contention DSE must
  use and explicitly report the appropriate timing mode.
* The verified current MXU is 64x64 INT8/INT32. BF16/FP32 and 128x128 are
  targets, not implemented benchmark configurations.
* A benchmark row's vector figures — active elements, tail elements, lane
  utilization — are **derived** from `vsetvli` results the guest reported, and
  each row carries the formula. The model exposes no scalar/vector instruction
  split at all: the pinned VP++ build compiles `ISSStatsDummy`, so those two
  fields of §8.2 are `unavailable` with that reason recorded in every row.
* The hart's external request count includes instruction fetch, because the
  reset PC is in global boot ROM outside the core. It is not a data-traffic
  figure.
* The model has no calibrated area, power or post-PD frequency model. Select a
  Pareto set with named cost proxies rather than claiming an absolute best
  hardware design.
* No standalone result is a NoC, multi-core or chip-level result.
