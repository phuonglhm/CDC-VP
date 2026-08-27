# Neo Lite C1/C2 Executable VP Implementation Plan

Status: **active plan; implementation has not started**

Plan date: 2026-08-26

Authority:

* `TPU_V3_DECISION_RECORD.md` D27 — one standalone NEO-CORE, `mhartid=0`,
  direct external memory and no NoC;
* `TPU_V3_DECISION_RECORD.md` D28 — exact executable Neo Lite C1/C2 profiles;
* `NEO_CORE_MICROBENCH_DSE_PLAN.md` — correctness and measurement gates
  G2–G6.

This plan turns D28 into reviewable implementation work. It does not redefine
the profiles and does not authorize a result row to call the current binary C1
or C2 before the corresponding promotion gate passes.

## 1. Required outcome

Produce two separately identifiable standalone VP builds:

```text
Neo Lite C1
  one RV32 Scalar/RVV hart, VLEN=256, ELEN=64
  one 32x32 INT8/INT32 MXU
  768 KiB Core SRAM, 4 banks x 128 bits
  one DMA controller with 2 channels
  128-bit external AXI, maximum normal burst 8 beats = 128 bytes
  one Im2Col INT8 Transform
  core period 1.25 ns

Neo Lite C2
  one RV32 Scalar/RVV hart, VLEN=512, ELEN=64
  one 64x64 INT8/INT32 MXU
  1536 KiB Core SRAM, 8 banks x 256 bits
  one DMA controller with 4 channels
  256-bit external AXI, maximum normal burst 8 beats = 256 bytes
  one Im2Col INT8 Transform
  core period 1.25 ns
```

Both builds execute the same benchmark contract: ReLU, vector-vector, GEMV and
GEMM, ten inputs per benchmark, independent host golden, kernel-only and
end-to-end modes where applicable.

### Explicit non-goals

* no `tpu_chip`, FlooNoC, mesh or multi-core composition;
* no 128x128 MXU, BF16 profile or Col2Im;
* no arbitrary VLEN, geometry, channel-count or bus-width sweep outside C1/C2;
* no claim of RTL, area, power or post-layout timing equivalence;
* no C1/C2 identity obtained only by changing a CLI string, manifest or report
  default.

## 2. Current baseline and gaps

The current standalone composition is a **C2 compute baseline**, not an
executable C2 profile:

| Capability | Current VP | C1 target | C2 target |
| --- | --- | --- | --- |
| XLEN/ELEN | 32/64 | 32/64 | 32/64 |
| VLEN | compile-time 512 | 256 | 512 |
| MXU | compile-time 64x64 INT8/INT32 | 32x32 | 64x64 |
| Transform | one Im2Col INT8 | same | same |
| backed SRAM | power-of-two only, reference 16 MiB | 768 KiB | 1536 KiB |
| local banks | runtime configurable | 4 x 128 bits | 8 x 256 bits |
| DMA | one descriptor/worker | 2 channels | 4 channels |
| external width | fixed 64 bits | 128 bits | 256 bits |
| maximum burst | byte limit over an 8-byte beat | 8 x 16-byte beats | 8 x 32-byte beats |
| core period | reaches only part of the timing path | 1.25 ns | 1.25 ns |
| report identity | several C2-like literals | live C1 identity | live C2 identity |

Consequently C2 closes first because its VP++ and MXU implementations already
exist. C1 reuses that common physical/configuration work and then adds the two
source-gated changes: MXU32 and VLEN256.

## 3. Implementation order

```text
G2 benchmark correctness
        |
G3 measurement conservation
        |
G4 current-baseline screening
        |
WP0 feasibility and frozen baseline
        |
WP1 one profile/config source of truth
        |
WP2 exact SRAM capacity
        |
WP3 parameterized shared external AXI
        |
WP4 one multi-channel DMA controller
        |
WP5 complete timing ownership
        |
WP6 live identity, firmware and runner selection
        |
WP7 C2 promotion
        |
        +---- WP8 explicit MXU32 source profile
        |
        +---- WP9 VP++ VLEN256 build profile
                    |
               WP10 C1 promotion
                    |
               WP11 G6 experiments/report
```

WP8 and WP9 are independently reviewable after WP1. They may be developed in
parallel, but neither is merged into a C1-labelled runner until both individual
gates pass.

## 4. Work packages

### WP0 — baseline lock and C1 feasibility audit

Purpose: detect an impossible C1 source assumption before changing the model.

Work:

1. record the current Release and Debug TPU_V3/microbench test totals;
2. record the current live identity: VLEN512, vlenb64, MXU64, one DMA worker,
   external width64 and the exact baseline configuration;
3. inspect the pinned Sauria v4.2 target table for a named `int8_32x32` target,
   its dimensions, datatype, index widths and source golden;
4. confirm VLEN256 can be selected by a recorded downstream VP++ patch and a
   compile definition, without editing the pinned checkout during configure;
5. create a C1 feasibility note containing every source path and hash used.

Exit criteria:

* current baseline is reproducible;
* C2 is unblocked;
* C1 is either source-proven or explicitly blocked. A C++ 32x32 template
  default does not satisfy this gate.

### WP1 — canonical profile and configuration propagation

Purpose: eliminate the two disconnected configuration views and prevent
profile labels from disagreeing with instantiated types.

Planned interface:

```text
TPU_V3_NEO_LITE_PROFILE=C1|C2       CMake/build identity
neo_lite_profile_id                 typed model identity
neo_lite_c1()/neo_lite_c2()         exact profile factories
```

The canonical profile contains:

* XLEN, VLEN, ELEN and RVV version;
* MXU rows, columns and datatype;
* core period;
* backed SRAM size and local-fabric width/count;
* one DMA controller plus channel count and per-channel role;
* external AXI width and burst beats;
* Transform count and operation.

The following are derived, never independently set:

```text
PE count        = rows * columns
frequency       = 1 / core period
burst bytes     = external width / 8 * burst beats
vlenb           = VLEN / 8, then checked against the hart CSR
```

Likely code areas:

```text
components/TPU_V3/common/include/tpu_v3/architecture_config.h
components/TPU_V3/common/src/architecture_config.cpp
components/TPU_V3/common/tests/test_architecture_config.cpp
components/TPU_V3/tpu_core/include/tpu_v3/core/tpu_core.h
components/TPU_V3/tpu_core/src/tpu_core.cpp
top-level and TPU_V3 CMakeLists.txt files
```

Required gates:

* exact C1 and C2 factory-value tests;
* derivation and overflow tests;
* configure-time refusal for an unknown profile;
* elaboration-time refusal when the requested profile disagrees with live
  VP++ VLEN or MXU identity;
* a mutation control that swaps one C1/C2 field and proves the mismatch gate
  fails before simulation.

Do not make the full-system `tpu_soc_config` a prerequisite for the D27
standalone runner. Extract/reuse a core-profile object rather than routing this
work through `tpu_chip` or NoC configuration.

### WP2 — exact non-power-of-two backed SRAM

Purpose: support D28's exact 768 KiB and 1536 KiB capacities without changing
the existing decoded aperture.

Work:

* keep the decoded SRAM window naturally aligned and power-of-two;
* remove the power-of-two restriction from **backed capacity only** in both
  common and component validators;
* validate nonzero capacity, containment in the window and whole physical
  stripes;
* keep sparse page-backed storage and forbid wrap/alias above capacity;
* audit firmware buffer placement against the smaller C1 capacity.

Required tests for both capacities:

* first byte and last byte succeed;
* the next byte returns `capacity_error`;
* a transfer straddling the last byte reports the existing partial/error
  contract and never wraps;
* reset and debug access preserve the same boundary;
* a deliberately non-stripe-aligned capacity is refused;
* existing 1/4/16 MiB component tests remain green where still relevant.

Likely code areas:

```text
components/TPU_V3/core_sram/src/core_sram.cpp
components/TPU_V3/core_sram/tests/test_core_sram.cpp
components/TPU_V3/common/src/architecture_config.cpp
components/TPU_V3/tpu_core/src/tpu_core.cpp
components/TPU_V3/microbench/runner/neo_core_bench_runner.cpp
```

### WP3 — parameterized shared external AXI model

Purpose: make 128/256-bit width and eight-beat bursts affect framing,
serialization and counters instead of report metadata only.

Work:

* replace the fixed `external_bus_bytes=8` assumption with the selected
  external width;
* make maximum burst bytes a derivation from width and beats;
* split misaligned transfers into legal prefix/full-beat/suffix frames;
* give the standalone external target deterministic read and write service
  queues, back-pressure and occupancy counters;
* permit one read and one write to overlap, while serializing two reads or two
  writes according to a deterministic arbitration rule;
* ensure hart fetch/global traffic and DMA share the same external resource;
* keep this model in the standalone path; do not introduce a NoC dependency.

Before performance use, freeze and report these common provisional inputs:

* AXI clock or its relationship to the 1.25 ns core period;
* fixed address/control latency;
* per-beat service time;
* maximum read/write outstanding transactions;
* arbitration policy.

Required gates:

* C1 observes 16-byte beats and no normal burst above 128 bytes;
* C2 observes 32-byte beats and no normal burst above 256 bytes;
* odd-address and short-transfer cases produce legal framing;
* two same-direction callers contend; a read and write follow the documented
  overlap rule;
* a negative control that gives each DMA channel an independent target is
  detected by the shared-bandwidth/occupancy assertion.

Likely code areas:

```text
components/TPU_V3/neo_dma/include/tpu_v3/dma/dma_registers.h
components/TPU_V3/tpu_core/include/tpu_v3/core/neo_external_bridge.h
components/TPU_V3/tpu_core/src/neo_external_bridge.cpp
components/TPU_V3/microbench/runner/bench_world.h
components/TPU_V3/microbench/runner/tests/test_bench_world.cpp
```

### WP4 — one DMA controller with multiple channels

Purpose: implement channel concurrency without multiplying DMA controllers or
external bandwidth.

Programming-model subgate, before code:

* channel register stride and total MMIO extent;
* descriptor/status/counter ownership per channel;
* channel IRQ presentation and acknowledgement;
* abort/reset behaviour when some channels are active;
* whether direction/role is hard-wired or checked at START;
* compatibility, if required, for the existing channel-0 register map.

Required architecture:

```text
one NEO DMA controller
  C1: 2 channel contexts/workers
  C2: 4 channel contexts/workers
  shared local-data requester scheduling
  shared external read arbiter
  shared external write arbiter
```

C2 roles come from the HAS:

```text
CH0 weight read
CH1 IFmap read
CH2 output write
CH3 configuration/LUT/bias read
```

Until separately approved C1 evidence arrives, use and report D28's provisional
mapping:

```text
CH0 input/weight/config read, reused sequentially
CH1 output write
```

Reset/abort/epoch rules from the existing DMA remain binding per channel. A
late completion from an abandoned generation must update neither another
channel nor a replacement job.

Required gates:

* independent descriptor/status/counters for all channels;
* simultaneous channel starts cannot lose an event;
* two reads arbitrate; read plus write follows the WP3 overlap rule;
* reset one/all channel cases, abort, error and replacement START;
* per-channel and aggregate byte conservation;
* IRQ remains asserted while any enabled unacknowledged channel requires it;
* report distinguishes `dma_controllers=1` from `dma_channels=2|4`;
* mutation controls for lost START, cross-channel status pollution and false
  independent bandwidth.

Likely code areas:

```text
components/TPU_V3/neo_dma/include/tpu_v3/dma/neo_dma.h
components/TPU_V3/neo_dma/include/tpu_v3/dma/dma_registers.h
components/TPU_V3/neo_dma/src/neo_dma.cpp
components/TPU_V3/neo_dma/tests/test_neo_dma.cpp
components/TPU_V3/tpu_core/src/tpu_core.cpp
```

### WP5 — clock and timing ownership

Purpose: make `1.25 ns` a real model input and prevent a partial propagation
from being reported as an 800 MHz whole core.

First produce a timing-ownership table for:

* hart instruction/retirement timing policy;
* local SRAM fabric service and pipeline;
* DMA issue, wait and service timing;
* external AXI service;
* MXU control, staging and source-compute timing;
* Transform prefetch/compute/writeback timing.

For every row state whether the block consumes core period, an independent
period or functional/untimed behaviour. Propagate 1.25 ns only where the model
has an owned timing contract; do not invent cycle accuracy by multiplying an
instruction count after the run.

Required gates:

* every timing figure used in a conclusion has an owner and unit;
* changing core period in a mutation build changes all and only the expected
  observations;
* external timing inputs are separately reported if not in the core domain;
* `800 MHz` is unavailable rather than printed when any required propagation
  gate is absent.

### WP6 — live identity, firmware selection and runner contract

Purpose: make every result self-identifying and impossible to pool under the
wrong configuration ID.

Work:

* remove C2-like default literals from the result schema;
* populate identity from live CPU/MXU/DMA/external/SRAM components;
* build/select firmware matching the build profile;
* make `--config-id neo_lite_c1|neo_lite_c2` a checked identity, not free text;
* record profile, firmware ISA/ABI, ELF hash, source hashes, timing inputs and
  provisional fields in every result;
* keep the D27 dependency guard proving no chip or NoC link.

Required negative controls:

* C1 label on a C2 binary;
* C2 firmware on a C1 hart where the ELF minimum VLEN exceeds 256;
* hard-coded VLEN/MXU/DMA/AXI report field;
* profile factory changed without rebuilding the selected component;
* missing or provisional timing input presented as frozen.

### WP7 — close and promote Neo Lite C2

Instantiate the exact C2 profile:

```text
MXU64, VLEN512, SRAM1536 KiB, banks8x256,
DMA controller1/channels4, AXI256, burst8, period1.25 ns, Im2Col1
```

Promotion requires:

1. all WP1–WP6 unit and mutation gates;
2. live identity equality with D28;
3. exact SRAM boundary evidence;
4. four-channel concurrency and shared external bandwidth evidence;
5. current Phase 5 MXU golden unchanged;
6. firmware `vlenb=64` and lane-boundary behaviour;
7. MB1–MB4, ten inputs each, element-wise golden agreement;
8. Release and Debug TPU_V3/microbench regressions;
9. zero chip/NoC dependencies in the standalone executable.

Only after this gate may a row use `configuration.id=neo_lite_c2`.

### WP8 — explicit C1 MXU32 source profile

Purpose: add a verified 32x32 backend, not an accidental template instance.

Work:

* extend the Sauria preparation script to extract the named `int8_32x32`
  target verified in WP0;
* generate profile dimensions, datatype and index widths from the pinned
  source;
* instantiate the adapter explicitly from those generated values;
* add source hashes and profile identity to the manifest;
* create independent source/golden comparisons for full and tail tiles;
* retain the same reset, buffered staging and timing boundaries as MXU64.

Required controls:

* omitted profile selection must fail instead of falling to template defaults;
* wrong row, column, datatype or index width fails configure/compile;
* an MXU64 binary cannot report MXU32 and vice versa;
* two-instance and reset/abort tests remain isolated.

If the pinned source has no named and auditable INT8 32x32 target/golden, stop
this work package as blocked and request an NPU-team delivery. Do not create an
architectural C1 by editing only template arguments.

### WP9 — VP++ VLEN256 build profile

Purpose: create a separately gated VP++ binary whose hart really reports and
executes VLEN256.

Work:

* add a recorded downstream patch making VP++ `VLEN` selectable by an explicit
  compile definition while keeping ELEN64 and 32 vector registers;
* update patch hashes, patched-file hashes and manifest evidence;
* make the wrapper validate the requested build profile rather than always
  require 512;
* build C1 firmware with `-march=rv32gcv_zvl256b -mabi=ilp32d`;
* preserve C2's `zvl512b` build and existing Phase 2 gates;
* audit all `VLEN=512` constants and classify each as C2-specific, maximum
  native payload, documentation or a defect.

Required gates:

* C1 CSR `vlenb=32`, C2 CSR `vlenb=64`;
* at SEW32/LMUL1, C1 `vlmax=8` and C2 `vlmax=16`;
* frozen sizes around 7/8/9 and 15/16/17 prove strip-mining and tail behaviour;
* vector register reset covers exactly 32 x VLENB bytes;
* C1 refuses an ELF requiring minimum VLEN512;
* C2 continues to run both its exact firmware and VLEN-agnostic lower-minimum
  firmware according to the loader contract;
* all existing RVV differential/conformance gates remain green for C2, with a
  separate C1 functional corpus where a Spike comparison supports VLEN256.

### WP10 — integrate and promote Neo Lite C1

Instantiate the exact C1 profile:

```text
MXU32, VLEN256, SRAM768 KiB, banks4x128,
DMA controller1/channels2, AXI128, burst8, period1.25 ns, Im2Col1
```

Promotion repeats WP7's complete gate with C1 identities and additionally
requires the WP8/WP9 source/build evidence. Only after this gate may a row use
`configuration.id=neo_lite_c1`.

### WP11 — two-profile experiment and report

After both promotions:

* run MB1 ReLU, MB2 vector-vector, MB3 GEMV and MB4 GEMM;
* run all ten frozen inputs for each benchmark/profile;
* retain kernel-only and end-to-end separation;
* repeat enough times to expose deterministic versus host-wall variance;
* reconcile stage bytes, DMA bytes, external bytes and total intervals;
* compare C1/C2 utilization, vector iterations, MXU tile occupancy, bank
  contention, DMA wait/service and external read/write occupancy;
* publish correctness separately from performance;
* state every provisional timing input and accuracy boundary.

The report may identify a bottleneck or a Pareto preference. It may not infer
area/power superiority without an independent cost model.

## 5. Planned build interface

The exact CMake option is delivered by WP1. The planned interface and commands
are:

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
$CC -dumpfullversion
$CXX --version | head -n 1

cmake -S . -B build-neo-c2 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCDC_BUILD_TPU_V3_SOC=ON \
  -DCDC_BUILD_TPU_V3_TESTS=ON \
  -DCDC_BUILD_TPU_V3_SAURIA_MATRIX=ON \
  -DCDC_BUILD_TPU_V3_IMAGE_TRANSFORM=ON \
  -DTPU_V3_NEO_LITE_PROFILE=C2

cmake -S . -B build-neo-c1 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCDC_BUILD_TPU_V3_SOC=ON \
  -DCDC_BUILD_TPU_V3_TESTS=ON \
  -DCDC_BUILD_TPU_V3_SAURIA_MATRIX=ON \
  -DCDC_BUILD_TPU_V3_IMAGE_TRANSFORM=ON \
  -DTPU_V3_NEO_LITE_PROFILE=C1

cmake --build build-neo-c2 --target neo_core_bench_runner -j"$(nproc)"
cmake --build build-neo-c1 --target neo_core_bench_runner -j"$(nproc)"

ctest --test-dir build-neo-c2 -L '^tpu_v3$' --output-on-failure
ctest --test-dir build-neo-c2 -L '^microbench$' --output-on-failure
ctest --test-dir build-neo-c1 -L '^tpu_v3$' --output-on-failure
ctest --test-dir build-neo-c1 -L '^microbench$' --output-on-failure
```

`CDC_BUILD_TPU_V3_SOC=ON` currently causes the component/CPU tree to be
available; it does not authorize the standalone runner to instantiate or link
the SoC/NoC path. The existing D27 dependency guard remains mandatory.

## 6. Review protocol

The implementer submits one work package at a time. Each review handoff must
contain:

1. scope and files changed;
2. exact configure/build/test commands;
3. Release and Debug results relevant to the package;
4. the new positive gate;
5. at least one mutation/negative control proving that gate can fail;
6. known limitations and any provisional input;
7. confirmation that unrelated user changes were preserved.

Do not combine profile schema, SRAM, DMA and VP++ changes into one review. A
failure in such a patch cannot be attributed to one contract and a green
end-to-end test can hide several compensating errors.

Blocker/High findings close before the next dependent work package. Medium/Low
findings are either closed or recorded with an owner and explicit effect on the
promotion claim.

## 7. Global no-go checks

Every work package must keep these assertions true:

* one standalone NEO-CORE and `mhartid=0`;
* no `tpu_chip`, NoC endpoint or FlooNoC link in the runner;
* one DMA controller, even though it owns 2/4 channels;
* exact 768/1536 KiB backed SRAM, never rounded substitutes;
* MXU32 comes from an explicit source profile, never a template default;
* C1/C2 channels share external bandwidth;
* PE/frequency/burst-byte fields are derived;
* live identity and firmware readback agree with the build profile;
* no performance claim uses an unavailable or uncalibrated timing quantity;
* all ten inputs pass an independent golden before any number is compared.

## 8. Completion definition

This plan is complete only when both `neo_lite_c1` and `neo_lite_c2` satisfy
D28's full promotion gate in Release and Debug, the standalone dependency guard
passes, MB1–MB4 pass all ten inputs, machine-readable rows contain live profile
identity, and the two-profile G6 report states its timing and fidelity limits.

A buildable C1/C2 factory, a printed configuration table or one passing ReLU
case is intermediate evidence, not completion.
