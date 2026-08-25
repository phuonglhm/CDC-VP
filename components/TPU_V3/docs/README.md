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
G2  benchmark correctness                        NEXT
    immediate subtask: MB1 ReLU, ten sizes,
    scalar and RVV, full host-golden comparison
G3  measurement conservation                     PENDING
G4  supported-knob screening                     PENDING
G5  justified model extensions                   PENDING
G6  targeted exhaustive sweep/Pareto report      PENDING
```

Do not start with VLEN or multi-channel-DMA sweeps. VP++ is currently compiled
for VLEN=512 and the current NEO DMA is one engine/worker. Those are model
extensions under G5, to be implemented only after G4 measurements justify the
question they will answer.

## Required reading order

1. **This file** — current authority and document classification.
2. `NEO_CORE_MICROBENCH_DSE_PLAN.md` — benchmark contract, cases, metrics,
   fidelity limits and gates G0–G6.
3. `TPU_V3_DECISION_RECORD.md` D27 — formal scope rebaseline and correction of
   the old ManagerID/node-count reasoning.
4. `TPU_V3_STANDALONE_DSE_AUDIT.md` — implemented G1 evidence and reproduction.
5. `ARCHITECTURE.md` active section — the standalone component boundary.
6. `TPU_V3_PHASE5_AUDIT.md`, `TPU_V3_PHASE6_AUDIT.md` and
   `TPU_V3_PHASE7_AUDIT.md` — retained evidence for MXU, Transform and the
   reusable one-core composition.
7. `INTERFACE_CONTRACT.md` — generic TLM and D15 internal-plane rules. Its
   NoC-facing section is inactive under D27.

Where documents disagree, use this precedence:

```text
this README
  > NEO_CORE_MICROBENCH_DSE_PLAN.md
  > decision D27
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
* The model has no calibrated area, power or post-PD frequency model. Select a
  Pareto set with named cost proxies rather than claiming an absolute best
  hardware design.
* No standalone result is a NoC, multi-core or chip-level result.
