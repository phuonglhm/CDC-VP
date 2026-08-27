# TPU_V3 Standalone NEO-CORE DSE Audit

Status: **G0–G3 complete. MB1–MB4 are implemented and gated, and every run is
conservation-checked. Five earlier review rounds (§6–§10) found twenty-six
defects, all fixed and gated. The MB3/MB4 implementation and the G3
conservation work await independent review.**

Evidence date: 2026-08-27 (review rounds 3–5 and MB3/MB4); 2026-08-26 (rounds 1–2);
2026-08-25 (G1)

Authority: D27 in `TPU_V3_DECISION_RECORD.md` and
`NEO_CORE_MICROBENCH_DSE_PLAN.md`

## 1. Scope proved by G1

The executable `neo_core_microbench` instantiates exactly one existing Phase 7
`tpu_core` with these compatibility-map inputs:

```text
chip index       0
core index       0
guest mhartid    0
external socket  direct Boot ROM/global RAM/host-I/O target
chip composition absent
NoC              absent
```

The words `chip index` and `core index` identify existing absolute address-map
slots. They do not mean that a `tpu_chip` is instantiated.

G1 reuses the Phase 7 firmware and independent golden computation. It does not
copy the hart, DMA, Transform, MXU, RVV or SRAM model. The firmware source is
parameterized at build time so two independent gates remain available:

* the historical Phase 7 pipeline uses chip 1/core 1 and checks guest hart 3;
* the D27 baseline uses chip 0/core 0 and checks guest hart 0.

Both execute:

```text
DMA -> Transform (Im2Col) -> MXU 64x64 INT8/INT32 -> RVV
```

## 2. Implementation evidence

| Evidence | Location |
| --- | --- |
| D27 target, image and CTest registration | `components/TPU_V3/tpu_core/tests/neo_core/CMakeLists.txt` |
| shared Phase 7/D27 harness and manifest writer | `components/TPU_V3/tpu_core/tests/neo_core/test_neo_core_pipeline.cpp` |
| post-link chip/NoC symbol guard | `components/TPU_V3/tpu_core/tests/neo_core/check_standalone_dependencies.cmake` |
| parameterized guest identity/map | `fw/TPU_V3_SoC/neo_core_pipeline/neo_core_map.h` |
| parameterized firmware build | `fw/TPU_V3_SoC/neo_core_pipeline/Makefile` |

The configure-time guard rejects named dependencies on `tpu_chip`, chip-local
fabric, `chip_noc_endpoint`, FlooNoC or `noc_interconnect`. The post-link guard
uses the demangled symbol table to reject the corresponding implementation
classes. Its regex deliberately does not reject the plain-data historical
`tpu_chip_config` type in `tpu_v3_common`; configuration vocabulary is not an
instantiated component.

The negative-control CTest supplies a fake
`cdc::components::tpu_v3::chip::tpu_chip` implementation symbol and passes only
when the guard rejects it.

## 3. Reproduction

Run the host-toolchain preflight in the same shell as every command:

```bash
cd /home/duyptt_HW/Desktop/VP_INTER/upgit/CDC-VP

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
# sanity
$CC -dumpfullversion
$CXX --version | head

cmake -S . -B build-neo-d27 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCDC_BUILD_TPU_V3_SOC=ON \
  -DCDC_BUILD_TPU_V3_TESTS=ON \
  -DCDC_BUILD_TPU_V3_SAURIA_MATRIX=ON \
  -DCDC_BUILD_TPU_V3_IMAGE_TRANSFORM=ON \
  -DTPU_V3_SAURIA_ROOT=/home/duyptt_HW/Documents/work/fx1/hw/tlm/MP1_V1.1/v4.2_model \
  -DTPU_V3_TRANSFORM_ROOT=/home/duyptt_HW/Documents/work/fx1/hw/tlm/MP1_V1.1/v4.2_model \
  -DTPU_V3_SA_GEOMETRY=64x64

cmake --build build-neo-d27 --target \
  neo_core_microbench \
  test_tpu_v3_neo_core \
  test_tpu_v3_neo_core_pipeline \
  -j"$(nproc)"

ctest --test-dir build-neo-d27 \
  -R '^tpu_v3_neo_core$|^tpu_v3_neo_core_pipeline$|^tpu_v3_neo_core_d27_' \
  --output-on-failure
```

The cross-toolchain remains the D4 xPack RISC-V toolchain. The two
`TPU_V3_*_ROOT` paths are source locations; their configured content is still
hash-checked by the Phase 5/6 gates.

## 4. Measured result

Release configuration used GCC/G++ 11.5.0 and SystemC 2.3.4.

```text
tpu_v3_architecture_config                   PASS
tpu_v3_neo_core                            PASS
tpu_v3_neo_core_pipeline                   PASS  (historical hart 3)
tpu_v3_neo_core_d27_baseline               PASS  (standalone hart 0)
tpu_v3_neo_core_d27_dependency_guard_control PASS
skipped                                    0
```

The D27 run reported:

```text
core_count       1
hart_count       1
hart_id          0
local fabric     annotated
MXU              64x64 INT8 x INT8 -> INT32
Transform        Im2Col only
local SRAM       73 requests, 292 bytes from the hart
control          175 requests, 700 bytes from the hart
external         1076 requests, 4304 bytes from the hart
pipeline result  PASS
```

Those request counts are VP++/TLM element-access granularity. They are not
hardware AXI beats or proof of a calibrated throughput figure.

The runtime manifest is generated in the build tree at:

```text
build-neo-d27/components/TPU_V3/tpu_core/tests/neo_core/
  neo_core_microbench_manifest.json
```

It records one core, one hart with id 0, direct external binding,
`chip_composition=false`, `noc_instantiated=false` and the pipeline result.

## 5. G2 MB1–MB4 implementation evidence

G2 is **complete**. Implemented: the benchmark definitions, independent host
goldens, machine-readable result schema, standalone runner and five distinct
firmware images for ReLU, vector dot, RVV GEMV, MXU GEMV and MXU GEMM.

### 5.1 What was built

| Piece | Location |
| --- | --- |
| frozen MB1–MB4 case tables and validation | `components/TPU_V3/microbench/src/benchmark_case.cpp` |
| deterministic INT32/INT8 inputs and independent host goldens | `components/TPU_V3/microbench/src/golden.cpp` |
| §8 result-row schema and JSON writer | `components/TPU_V3/microbench/src/result_row.cpp` |
| firmware ELF digest for §8.1 identity | `components/TPU_V3/microbench/src/sha256.cpp` |
| the standalone one-case runner | `components/TPU_V3/microbench/runner/neo_core_bench_runner.cpp` |
| five benchmark images, shared startup, map and linker script | `fw/TPU_V3_NEO_CORE_MICROBENCH/` |
| unit gates that need no elaboration | `components/TPU_V3/microbench/tests/` |

`tpu_v3_microbench` links no SystemC and no core model. That is what keeps the
golden independent: a golden that could reach the model is a golden that can be
written to agree with it, which is the same reason the Phase 7 harness
recomputes Im2Col on the host rather than calling the Transform block.

The runner reuses the Phase 7 `tpu_core` composition and creates no second core
model, as D27 and plan §13 require.

### 5.2 One image per benchmark, ten cases each

The shape, implementation, mode and buffer addresses reach the guest in a
parameter block staged in global RAM, so all ten frozen sizes and both
implementations of one benchmark share one ELF and therefore one recorded ELF
hash. Ten compiled images per benchmark would put a different hash in every row
and make §8.1's identity field describe the case rather than the code that ran.

Benchmarks do *not* share an image. Each has its own ELF and refuses the
other's identifier at start-up, so the hash identifies the code that ran rather
than a multiplexer that contains it.

The firmware/host RAM split is enforced by the linker, not by arithmetic:
`link.ld` carries `ASSERT(__stack_top <= 0x80008000, ...)` and the runner
asserts that `bench_map.h` names the same boundary. An image whose data grew
into the staged buffers fails to link rather than overwriting the inputs and
producing a benchmark result for data nobody supplied.

### 5.3 What the vector numbers are, and are not

`vlenb`, `vlmax`, `vl_first`, `vl_last` and the iteration count are **read back
from the guest**: `vlmax` from a `vsetvli` with `rs1 = x0`, the others from the
strip-mined loop's own `vsetvli` results. Active elements, tail elements and
lane utilization are then **derived** from those, and every row carries the
formula in its `derivation` field. The distinction is not pedantry — deriving a
tail from a VLEN the host believes in would be deriving it from a configuration
value rather than from the machine.

Measured at the frozen sizes, `vlmax = 16` at SEW=32, LMUL=1:

```text
N = 15   1 iteration    tail 1     lane utilization 0.9375
N = 16   1 iteration    tail 0     lane utilization 1.0
N = 17   2 iterations   tail 15    lane utilization 0.53125
N = 1024 64 iterations  tail 0     lane utilization 1.0
```

This is the lane-boundary behaviour MB1's ten sizes were chosen to expose. It
is evidence about model execution, not a calibrated RTL lane-throughput result
(§12).

### 5.4 Measurements that are not available, and why

§8 permits `unavailable` and requires the reason. Three appear in every row and
are recorded here because each is a real model limitation rather than an
omission:

* **scalar and vector instruction counts** (§8.2). The pinned VP++ build does
  not define `ISS_CT_STATS_ENABLED`, so `iss.stats` is `ISSStatsDummy` and every
  counting call is optimised out. `instret` and `mcycle` are readable and are
  reported; the split is not available from any path in the current model.
* **DMA busy, service and wait cycles** (§8.3). `neo_dma` exposes byte,
  request, chunk and transfer counters and no cycle counters.
* **external memory latency, bandwidth and outstanding parameters** (§8.3).
  The standalone target is a fixed 1 ns annotated memory with no back-pressure.
  Every row states this rather than leaving a reader to assume a model exists,
  and an end-to-end row additionally carries the §7.2 note that no DMA
  sufficiency conclusion follows from it.

One measurement defect was found and corrected during implementation.
`neo_dma::bytes_done()` is the `BYTES_DONE` register, which the engine resets
when it accepts a descriptor; differencing it across an interval measures the
last job alone, and for a single-leg run that looks exactly like a correct
total. The row instead reports the lifetime path totals (`local_bytes`,
`external_bytes`) as measured, derives the in/out split from MB1's known
two-transfer structure, and **checks the derivation against those totals** —
reporting `unavailable` with the observed numbers when the check does not hold.

### 5.5 Poisoned destinations

Every destination buffer is filled with a poison value before the run, checked
against the case's own golden so it cannot collide with a legitimate result.

This closes a hole the frozen patterns would otherwise leave open. The `zero`
and `all_negative` patterns both have an all-zero golden output, and an
unwritten buffer reads as zero — so at those patterns a kernel that did nothing
at all would have passed. It was verified by running the `dma_out` control at
the `all_negative` pattern: without poisoning the run would agree with the
golden, and with it the row reports `element 0 is 1515870810, expected 0`.

The result therefore evidences that it was written, not only that it matches.

### 5.6 Negative controls

G2 requires a control for one arithmetic operation, one DMA leg and one
measurement-boundary marker. Each names the detection that must fire, through
`--expect-detect`, and a control is satisfied only when **both** halves hold:

1. the named detection fired;
2. nothing outside its category failed.

The second half is not decoration, and the first version of this harness did
not have it — see §6.1. Failures are classified as `guest`, `interval`,
`golden` or `harness`, and a control permits only its own category. Because
every destination is poisoned, a guest that never reaches the kernel also
mismatches the golden; without (2) such a run reports the mutation as detected
while the mutation never executed.

| Control | Injected | Detection, and how it is diagnosed |
| --- | --- | --- |
| `arith_rvv`, `arith_scalar` | the guest kernel clamps to 1 instead of 0 | `golden_mismatch`; the guest's own core-SRAM checksum also disagrees, so the row says the kernel computed the wrong answer |
| `dma_in_skipped` | the DMA-in leg is not issued | `golden_mismatch` on stale core SRAM |
| `dma_out_skipped` | the DMA-out leg is not issued | `golden_mismatch`, and the guest's checksum still **matches**, so the row says the kernel was right and the result did not reach the host |
| `measurement_boundary` | the closing marker is never written | `interval_not_closed`; no timing figure is emitted |
| `dma_fault_needs_end_to_end` | a DMA control asked for in kernel-only mode | refused as a usage error, so a control cannot pass in a mode where it proves nothing |
| `unimplemented_benchmark_refused` | `--benchmark gemm` | refused; a frozen case table is not an implementation |

The arithmetic fault is in the guest rather than in the host comparison. A
control that perturbed the golden would prove only that the comparison
compares.

The controls are themselves controlled. Five CTest entries assert that the
mechanism can refuse:

| Entry | Asserts |
| --- | --- |
| `detection_requires_a_complete_run` | an `arith` control with a watchdog too short to reach the kernel **fails**, because guest and interval failures fall outside the permitted category |
| `detection_requires_a_fault` | a clean run with `--expect-detect` fails; nothing fired |
| `usage_expectation_bites` | a run that is accepted fails a control expecting a usage refusal |
| `reference_id_requires_reference_config` | a row may not label itself `reference` while running something else |
| `cli_negative_seed`, `cli_nan_watchdog`, `cli_oversized_bank_count` | the command line refuses values it cannot represent, asserted by the refusal message rather than by a non-zero exit |

The two usage refusals name the refusal they expect through `--expect-usage`
rather than using `WILL_FAIL`, which would accept a failure caused by a typo in
the test's own command line.

### 5.7 Documented departures from plan §11

§11 calls its layout proposed. Three things differ and each is deliberate:

* the runner is `neo_core_bench_runner`, not `neo_core_microbench`. That name
  is already the G1 baseline executable with a different command line, and
  reusing it would have broken `tpu_v3_neo_core_d27_baseline`;
* knobs are explicit flags rather than `--config <config.yaml>`. Writing a YAML
  parser and schema now would mean specifying a file format whose contents G4
  has not decided, and a half-specified configuration file is exactly where a
  value nobody chose becomes a constant by accident. Every knob §7.2 lists as
  configurable today has a flag, and every one is recorded in the row;
* result rows are written into the build tree rather than
  `out/neo_core_microbench/`. Packaging an output bundle is a separate concern
  from the gate, and the build tree is where CTest can own the files.

`--config-id reference` is not a free-text label. The runner checks it against
every §7.1 value it can set — SRAM capacity, bank width, bank count, pipeline
depth, DMA burst and core period — and refuses the run when they disagree,
naming each mismatch. The source revision comes from a file regenerated on
every build, carrying a `-dirty` suffix when the working tree is modified: a row
produced from a modified tree is not reproducible from the commit it names.

### 5.8 Reproduction

The §3 preflight and configure apply unchanged. Then:

```bash
cmake --build build-neo-d27 --target \
  neo_core_bench_runner \
  test_tpu_v3_benchmark_case \
  test_tpu_v3_bench_result_row \
  test_tpu_v3_bench_sha256 \
  -j"$(nproc)"

ctest --test-dir build-neo-d27 -L microbench --output-on-failure
```

One case by hand, which is also how a row is produced outside CTest:

```bash
B=build-neo-d27/components/TPU_V3/microbench/runner
$B/neo_core_bench_runner \
  --elf $B/firmware/relu/neo_bench_relu.elf \
  --benchmark relu --case 8 --impl rvv --mode end_to_end \
  --pattern mixed --seed 20260825 \
  --source-revision-file $B/source_revision.txt \
  --build-type Release \
  --result /tmp/row.json
```

The revision and build type are not optional. §8.1 lists both, and the runner
refuses a run that cannot fill either rather than emitting a row that says
`unspecified` — a field that reads like a value and is not one.

Rows land in `build-neo-d27/components/TPU_V3/microbench/runner/results/`.

### 5.9 MB2, the vector dot product

MB2 is `result = sum(a[i] * b[i])` over the same ten frozen sizes (§5.3), and
it reuses everything MB1 built. What it does not reuse is the assumption that a
benchmark consumes and produces the same shape.

**Input and output became separate quantities.** MB1 is element-wise, so one
`bytes` served both. MB2 reads 2N operands and writes one INT32, and a shared
length would have made one of its two DMA legs the wrong size and its
comparison the wrong shape. The parameter block now carries `in_bytes` and
`out_bytes`, the runner sizes staging, poisoning and readback from
`input_bytes()`/`output_bytes()`, and the end-to-end derivation expects
`in + out` on each path rather than twice one leg. Measured at N=1024:
`dma transfers 2, in 8192, out 4`.

**The operand bound is the one §7.6 introduced.** `dot_product_magnitude_bound(N)`
= `floor(sqrt(INT32_MAX / N))`, 1448 at N=1024, so the exact sum is
representable at every frozen size. The host golden accumulates in 64-bit and
**throws** rather than wrapping if the result does not fit: a wrapped golden
would disagree with a correct machine and present as an arithmetic defect in
the model, which is the most expensive way to learn the operands were out of
contract.

**MB2 has its own boundary pattern.** MB1's boundary is `INT32_MIN`/`INT32_MAX`
because ReLU is defined across the representable range. A dot product is not —
at those magnitudes the specified result does not exist at any length — so
MB2's boundary is the operand bound itself. Two generators rather than one
parameter, because the two benchmarks mean different things by the word.

**Each benchmark is its own ELF, and each image refuses the other's
identifier.** A shared image would make the firmware hash in a row identify the
pair rather than the code that ran. Two controls point each image at the other
benchmark and require the guest's own status code `0xbad00003`, asserted by
message rather than by a non-zero exit: such a run fails several ways at once
and only that line says the image refused the benchmark.

The five mutations MB1 carries are repeated for MB2. The injected arithmetic
adds one to every product, shifting the sum by exactly N — detectable at every
frozen size because N is never zero, and a corruption of the operation rather
than of the comparison. Measured at N=17: `112878515 where 112878498 was
expected`, a difference of 17.

The scalar and RVV implementations perform the same additions in the same
order: `vredsum` is seeded with zero each iteration and the partial sums are
accumulated in a scalar register. Both orders are correct, and choosing the
matching one means a scalar-versus-vector disagreement is a defect rather than
a reassociation.

### 5.10 Identity read from live components (D28)

D28's promotion gate requires that "a report must obtain the instantiated MXU
identity, CPU `vlenb`, DMA channel count and AXI width from live components; it
must not reproduce literals from the profile factory and call that readback".
The MB1 schema predated D28 and carried literals. Four fields were converted:

| Field | Was | Now |
| --- | --- | --- |
| `vlen_bits` | literal 512 | `vlenb * 8` from the guest CSR |
| `mxu_geometry`, `mxu_datatype` | literals `64x64`, `int8xint8->int32` | `matrix_engine().identity()` |
| `mxu_source_revision` | absent | the engine's own `profile@hash` |
| `sram_capacity_bytes` | the command-line value | `sram().capacity_bytes()` |
| `dma_engines` | one field | `dma_controllers` and `dma_channels`, separately |

A measured row now carries
`int8_64x64@c1931405bfa6c8ce…`, which is what makes the geometry attributable
rather than merely stated.

Each readback is checked against what the build is supposed to contain, because
geometry and VLEN are compile-time in this source: a binary built for another
array or another VLEN would otherwise emit rows naming the one nobody built.
`--expect-vlen-bits` and `--expect-mxu-geometry` make both checks reachable and
two controls exercise them.

**This closes two of D28's four readbacks, not four.** The decision names MXU
identity, CPU `vlenb`, DMA channel count and AXI width. The first two are live.
The other two are not: `neo_dma` owns one worker and exposes no channel
accessor, and the external width is a fixed constant in the bridge — WP4 and
WP3 are what make them readbacks. The DMA field is nonetheless *split*, because
that part is a schema requirement rather than a readback one: channels are
contexts inside one controller sharing one external AXI boundary, and a
collapsed field would let a four-channel row read as four DMA engines and four
times the external bandwidth.

Claiming all four would be the defect D28 names. So every configuration field
now carries its own provenance in the row —`live_readback`, `configured` or
`structural_literal` — and `test_result_row.cpp` keeps the label set closed. A
current row reports twelve live, two configured and six structural:

```text
live_readback       vlen_bits, mxu_geometry, mxu_datatype,
                    mxu_source_revision, sram_capacity_bytes,
                    local_bank_width_bits, local_bank_count,
                    fabric_pipeline_stages, outstanding_per_requester,
                    bank_mapping, arbitration, timing_mode
configured          dma_max_burst_bytes, core_period_ns
structural_literal  dma_controllers, dma_channels,
                    external_axi_data_width_bits, xlen, elen_bits,
                    rvv_version
```

The core period stays `configured` rather than live for the reason WP5 exists:
a value that reaches only part of the timing path does not establish an
800 MHz core.

### 5.11 MB3 GEMV and MB4 GEMM

Matrix inputs use the frozen layout `A` followed immediately by `B`, both
row-major INT8. Results are row-major INT32. `matrix_multiply_int8_golden()`
is host-only code: it accumulates in INT64, checks the exact result is
representable in INT32, and never calls SystemC or the matrix model. The input
generator is deterministic from the row seed and forces mixed-sign coverage;
the boundary pattern includes `-128`, `127`, `-1`, `0` and `1`.

MB3 has two separate images. The RVV image loads each B column with `vlse8.v`,
sign-extends INT8, uses signed widening multiply and reduces the INT32 products.
Its rows derive active and tail lanes from guest-reported `vlenb`, `vlmax` and
the actual vector-iteration count. The MXU image programs the live SA control
plane and polls its asynchronous status. MB4 uses the same common MXU firmware
body but a distinct expected benchmark ID and ELF, so a GEMV/GEMM image mix-up
is a guest-visible identity failure rather than an arithmetic mismatch.

That compile-time distinction has explicit negative gates in both directions:

```text
tpu_v3_mb3_gemv_rvv_control_wrong_image_is_refused
    gemv_rvv ELF asked to run gemv_mxu
tpu_v3_mb3_gemv_mxu_control_wrong_image_is_refused
    gemv_mxu ELF asked to run gemm
tpu_v3_mb4_gemm_control_wrong_image_is_refused
    gemm ELF asked to run gemv_mxu
```

The last two are deliberately symmetric. `gemv_mxu` and `gemm` compile the
same `common/matrix_main.c`; only `-DBENCH_EXPECTED_ID` separates them. Each
control requires guest status `0xbad00003`, so a generic failure or golden
mismatch cannot satisfy the gate.

The MXU rows populate §8.5 from live component state: accepted-job delta,
prefetch/source-compute/writeback timing, local requests/bytes and committed
result bytes. Operand/result footprints and useful operations per source cycle
are derived only after the live byte/job accounts match the executed job.
These counters are still not performance evidence until G3 supplies their
conservation and mutation controls.

Coverage added by MB3/MB4 is:

```text
MB3 GEMV RVV: 10 cases x {kernel,end_to_end}       20
MB3 GEMV MXU: 10 cases x {kernel,end_to_end}       20
MB4 GEMM MXU: 10 cases x {kernel,end_to_end}       20
four edge patterns x three execution paths          12
arith/DMA-in/DMA-out/boundary x three paths         12
wrong-image identity controls                        3
```

Every arithmetic control shortens K by one inside the guest. Every DMA and
boundary control is classified by the same `--expect-detect` mechanism as
MB1/MB2, so it passes only for the intended failure category and only when no
unrelated guest or harness failure accompanies it.

### 5.12 Measured result

Release, GCC/G++ 11.5.0, SystemC 2.3.4, cross toolchain xPack
`riscv-none-elf` 15.2.0-1.

The full-tree reproduction explicitly builds the otherwise unrelated legacy
RVV image, so the pass/skip count is a property of the command rather than the
caller's source-tree residue:

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:/opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1/bin
cmake --build build-neo-d27 \
  --target neo_core_bench_runner rvv_smoke_image -j4
ctest --test-dir build-neo-d27 --output-on-failure -L '^microbench$'
ctest --test-dir build-neo-d27 --output-on-failure -L '^tpu_v3$'
```

```text
tpu_v3_benchmark_case                            PASS
tpu_v3_bench_result_row                          PASS
tpu_v3_bench_sha256                              PASS
tpu_v3_bench_world                               PASS
tpu_v3_mb1_relu_c{0..9}_{scalar,rvv}_{kernel,end_to_end}_mixed
                                            40 x PASS
tpu_v3_mb1_relu_c8_{scalar,rvv}_kernel_{all_negative,all_positive,zero,boundary}
                                             8 x PASS
tpu_v3_mb1_relu_*                           48 x PASS
tpu_v3_mb2_vector_dot_*                     48 x PASS
tpu_v3_mb3_gemv_rvv_*                       29 x PASS
tpu_v3_mb3_gemv_mxu_*                       29 x PASS
tpu_v3_mb4_gemm_*                           29 x PASS
tpu_v3_mb1..mb4 and identity controls       39 x PASS
tpu_v3_mb1_control_vlen_readback_agrees      1 x PASS  (positive)
------------------------------------------------------
microbench label                          212 / 212
full tpu_v3 label                         271 / 271, 0 skipped
```

The full-tree number is conditioned on its build prerequisites, rather than on
whatever firmware happened to exist in the source tree. The evidence command
first builds both `neo_core_bench_runner` and `rvv_smoke_image`, then runs the
label. Without the unrelated `rvv_smoke_image`, two CPU tests skip and the same
tree reports 269 PASS/2 SKIP; that environment-dependent count is not used as
closure evidence. All five G2 images are target dependencies of the runner, so
the narrower 212/212 microbench count never depends on this legacy image. The
D6 sparse page-backed store means the 16 MiB reference capacity costs nothing
until firmware writes to it.

The self-contained SHA-256 was cross-checked against `sha256sum` on the built
image; the digest recorded in every row is the same value.

Two counter observations worth carrying forward, neither of which is a
performance conclusion:

* at `N = 1024` the hart issues 2048 local requests for 1024 elements — one
  per element load and one per element store. This is VP++ element-wise TLM
  granularity (D7), not 128 wide SRAM accesses;
* the hart's external request count includes instruction fetch, because the
  reset PC is in global boot ROM outside the core (D15). Every row carries that
  note so the number is not read as data traffic.

## 6. Review round 1 (2026-08-26) and its disposition

The first MB1 submission was **not signed off**. All five findings were
reproduced before being fixed, and each fix carries a control that fails
without it. What follows is what was wrong, not what was reported.

### 6.1 Blocker: a control could pass while the mutation never ran

`--expect-detect` checked only that the named detection had fired. It did not
check that nothing else had. Because every destination is poisoned before the
run, a guest that never reaches the kernel mismatches the golden on its own —
so a control whose watchdog expired during the parameter read reported:

```text
FAIL: the image never reached its exit protocol ... reading the parameter block
FAIL: the measured interval never opened
FAIL: element 0 is 1515870810, expected 0
negative control satisfied: 'golden_mismatch' fired as expected
EXIT_CODE:0
```

The arithmetic mutation under test had not executed. This is precisely the
class of false evidence a negative control exists to exclude, produced by the
control itself.

Failures are now classified `guest` / `interval` / `golden` / `harness`, and a
control permits only the category it names. The preconditions must hold — the
guest ran to completion with the right identity and parameters, the interval is
well formed, the harness is not at fault — except for the one that is itself the
mutation. `tpu_v3_mb1_control_detection_requires_a_complete_run` re-runs the
reported repro and requires it to fail.

The two `WILL_FAIL` usage controls had the same weakness in a milder form:
they accepted any non-zero exit, including one caused by a mistake in their own
command line. They now name the refusal through `--expect-usage`, and
`usage_expectation_bites` proves the flag can refuse.

### 6.2 Rows labelled `reference` did not run the reference configuration

The runner defaulted to 64 KiB of core SRAM while CTest labelled every row
`--config-id reference`, and §7.1 puts the reference at 16 MiB. `source_revision`
was the literal string `unspecified`.

Both are worse than a missing field: a configuration ID is the key an
aggregation groups by, so mislabelled rows are pooled with rows that ran
something else and the difference reappears as unattributable variance.

The default is now the D6 reference capacity, and `--config-id reference` is
checked against every §7.1 value the runner can set, refusing the run and naming
each mismatch. The revision comes from a file regenerated on every build with a
`-dirty` suffix when the tree is modified, and a row cannot be produced without
one.

### 6.3 The command line accepted values it could not represent

`std::stoull` accepts a leading minus and returns the two's-complement wrap, so
`--seed -1` ran with 18446744073709551615. `std::stod` accepts `nan`, and a NaN
watchdog compares false against every bound. 64-bit values were cast straight
into 32-bit fields, so `--bank-count 4294967297` ran with one bank while the row
said 4294967297.

Parsing now rejects any non-digit character before conversion, bounds each
value by its destination's width, and requires a finite positive double. Three
controls assert the refusal message rather than a non-zero exit.

### 6.4 More than sixteen banks produced a silently incomplete row

The sample held `bank_grants[16]` and clamped with `min(bank_count, 16)`. At
`--bank-count 32` the run exited zero and wrote a row stating
`local_bank_count: 32` beside grants for banks 0..15 — wrong in the direction
nobody checks, because it looks complete. The array is now sized from the
configuration.

### 6.5 The testbench target did not honour the TLM payload contract

`bench_world` copied four bytes unconditionally in its host-I/O path without
checking command, length, alignment, null data pointer, streaming width or byte
enables, and never cleared `dmi_allowed`. Nothing fired, because the runner
generates well-formed traffic — which is the reason it needed a gate rather
than a reason it did not.

It now applies §1, §2, §6, §8 and §9, and `tpu_v3_bench_world` drives the
payloads a correct initiator never sends. Writing that test found one further
defect, in the test: the expected little-endian word for a masked write was
byte-reversed. The model was right.

### 6.6 The MB1 operand bound was wrongly described as sufficient for MB2

`golden.h` claimed a bound of 2^20 kept 1024 products inside INT32. It does
not: `1024 * (2^20)^2 = 2^50`. Nothing was computed from the claim yet, but MB2
would have been built on it and produced a golden that disagrees with any
correct machine.

The comment is replaced by `dot_product_magnitude_bound(N)` —
`floor(sqrt(INT32_MAX / N))`, 1448 at `N = 1024` — and a test checks at every
frozen MB2 case that the worst-case exact result fits and that the bound is
tight.

## 7. Review round 2 (Codex, 2026-08-26) and its disposition

A second review of the round-1 fixes found eight further defects. All eight were
reproduced before being changed, and none was rejected. Three of them are in
code round 1 introduced, which is the point of running a second pass over a
first pass.

### 7.1 The MMIO strobe rule was a pointer test, not a strobe test

`bench_world` refused any access carrying a non-null `byte_enable_ptr`.
§2 requires *all-ones over the four bytes*, which an initiator may legitimately
spell as an explicit all-enabled array — so a correct initiator was refused.
The check now expands the pattern and refuses only when an addressed byte is
disabled.

The round-1 test did not catch this because it was wrong in the same direction:
it built an array of four `TLM_BYTE_ENABLED` entries, called it a partial
strobe, and asserted the refusal. It now asserts three cases — all-ones is
served, a disabled byte is refused, and a two-byte repeating pattern that
covers the register as `{E, D, E, D}` is refused.

### 7.2 Debug transport did not complete the payload contract

`transport_dbg` computed a response status and discarded it, returning the byte
count alone, and never cleared `dmi_allowed`. §1's return rules are written
against `b_transport` **and** `transport_dbg`, so a payload came back carrying
whatever it arrived with — usually `TLM_INCOMPLETE_RESPONSE`, the one state §1
says a target must never leave behind.

Both fields are now set on every return.

Noted rather than changed: `neo_dma::transport_dbg` returns a byte count the
same way, without setting either field. That component is signed off and
outside this gate; whether §1's table is meant to bind debug transport as
literally as it reads is a question for whoever owns that contract, not
something to settle by editing a gated component from here.

### 7.3 The new TLM test read past its own buffer

The final loop in `test_bench_world.cpp` sends payloads up to 64 bytes — the
largest a memory-like target must accept — from a 32-byte stack array. The
target correctly accepted the access and copied 64 bytes, so the fault was
entirely in the probe.

The buffer is now 64 bytes with a `static_assert` tying it to the largest
payload the file sends. AddressSanitizer is not installed on this host
(`libasan.so.6.0.0` is absent), so the fix is enforced at compile time rather
than demonstrated by a sanitizer run.

### 7.4 Inverted controls turned a skip into a pass

Four controls use `WILL_FAIL`, and CTest inverts *any* non-zero exit — including
the runner's 77 skip. On a machine without the cross toolchain they would have
reported the control mechanism as exercised while nothing ran.

Measured directly:

```text
WILL_FAIL only, exit 77                Passed
WILL_FAIL + SKIP_RETURN_CODE 77        Skipped
```

Every inverted control now carries `SKIP_RETURN_CODE 77`. `usage_expectation_bites`
got a second fix: its check moved ahead of the firmware check entirely, so it
does not depend on an image at all and returns 1 rather than 77 on a bare
machine.

### 7.5 and 7.6 The row omitted measurements §8 requires

§8.3 asks for busy, **service** and wait cycles; only two were emitted. §8.4
asks for arbitration wait cycles and **peak and average** outstanding; the row
carried neither the wait nor the average.

An absent key is the one failure the `measured<>` wrapper cannot catch, because
there is nothing there to wrap — and it defeats the schema's whole premise, that
a consumer can tell "the model does not measure this" from "the writer forgot
it". The three fields are added with their reasons.

`test_result_row.cpp` now checks every §8 measurement is present, from a list
transcribed from the plan rather than from the writer: a list derived from the
emitter would agree with whatever the emitter happens to emit.

### 7.7 The build type was optional while the revision was mandatory

Round 1 made `--source-revision` mandatory and left `--build-type` defaulting to
`unspecified`. §8.1 lists both for the same reason — a Release row and a Debug
row are not comparable — and CTest masked the gap by always passing
`$<CONFIG>`. Both are now required.

### 7.8 The documented hand-run command could not run

Round 1's own change made the revision mandatory without updating the
reproduction in §5.8, so the published command exits 2 with `no source
revision` and never writes the row it claims to. The command now passes the
generated revision file and a build type, and was re-run to confirm it produces
a valid row.

## 8. Review round 3 (Codex, 2026-08-27) and its disposition

Four findings, all reproduced, none rejected. Two of them are consequences of
**D28**, which was ratified on 2026-08-26 — after the MB1 harness was written.
A gate that predates a decision does not automatically comply with it, and this
round is what checked.

### 8.1 A reserved profile identity could label a machine that is not it

`--config-id` was free text with only `reference` validated, so
`--config-id neo_lite_c1` was accepted and produced:

```text
id = neo_lite_c1 | mxu = 64x64 | vlen = 512 | sram = 16777216 | period = 10 ns
D28 C1 requires: 32x32, 256-bit, 786432 bytes, 1.25 ns
```

Every profile value was wrong, and an aggregation grouping by configuration ID
would have pooled the row as promoted C1 evidence. D28 is explicit that
"changing a manifest, a result-row default or a configuration ID while the
component still uses another value is a configuration defect and must be
refused".

Both Neo Lite identities are now refused by name. Refusal rather than
validation is the honest answer today: D28 makes MXU geometry and VLEN
compile-time build profiles, so this binary cannot contain either one. Two
controls assert the refusal message.

### 8.2 CPU identity was thrown away on half the matrix

The guest reads `vlenb` from the CSR before it looks at the parameter block, so
every run has it — including scalar runs. The row reported it `unavailable`
whenever no vector loop ran, while `configuration.vlen_bits` carried the
literal 512. That is precisely the combination D28 forbids when it says a
report "must not reproduce literals from the profile factory and call that
readback": half of MB1 had no live CPU identity at all.

`vlenb` is now reported on every run and `configuration.vlen_bits` is derived
from it. The live value is also checked against what the build is supposed to
contain, so a VP++ compiled for another VLEN fails instead of producing rows
that claim 512 while the machine granted something else — with every lane
utilization derived from it silently wrong. `--expect-vlen-bits` makes that
check reachable, and a control exercises it.

### 8.3 `elements_compared` counted elements it had not compared

The comparison stopped at the first mismatch while the field still reported the
whole tensor: an arithmetic control failing at index 0 recorded
`elements_compared: 17` after making one comparison.

The scan now runs to the end. That also buys the more useful number — the same
control now reports `mismatch_count: 10 of 17`, which separates a tail defect
from a total failure in a way a first-mismatch index cannot.

### 8.4 The shared buffers could not hold a frozen case

`BENCH_HOST_BUF_BYTES` and `BENCH_SRAM_BUF_BYTES` were 16 KiB, sized against
MB1's largest case and a comment claiming they left room for MB3 and MB4. They
did not. The destination buffer began immediately after the source, so staging
frozen MB4 case `(64,64,256)` — 32768 operand bytes — would have overwritten
its own operands.

Latent rather than live, because the runner refuses MB4 today. It is the same
class of defect as the MB2 operand bound in §7.6: a claim in a comment that
arithmetic contradicts, waiting for the code that would rely on it.

Both buffers are now 64 KiB, and the sizing is a checked property rather than a
comment. The runner validates every frozen case of every benchmark at start-up,
and `test_benchmark_case.cpp` does the same in a compile-and-run that needs no
simulator. Reverting the buffers to 16 KiB makes that gate fail — and it names
**three** cases, not one:

```text
gemv_rvv case 8 needs 16640 operand bytes, more than BENCH_SRAM_BUF_BYTES
gemv_mxu case 8 needs 16640 operand bytes, more than BENCH_SRAM_BUF_BYTES
gemm     case 9 needs 32768 operand bytes, more than BENCH_SRAM_BUF_BYTES
```

MB3 was over the limit too. That is the argument for checking the whole frozen
table rather than the case that prompted the finding.

## 9. Review round 4 (2026-08-27) and its disposition

Two High, one Medium, one Low. Both High findings were in code the previous
round introduced.

### 9.1 High: the VLEN control never reached the path it named

The control ran under `--config-id reference` with `--expect-vlen-bits 256`.
Round 3 had added `vlen_bits` to the reference-profile check, so the runner
refused the combination during configuration validation — correctly, because
the reference profile *is* VLEN 512 — and exited 2 before elaboration. The
guest never read a CSR. `WILL_FAIL` accepts any non-zero exit, so the control
reported the live-readback path as exercised while nothing had reached it.

Two fixes, because either alone leaves the hole: the control now runs under its
own configuration id so it reaches simulation, and it asserts the live
diagnostic rather than an exit code. Only the readback path can produce
`the guest reports vlenb = 64 (512-bit VLEN) but this build is configured for
256-bit`. The MXU control was rewritten the same way, and a positive control
now requires that diagnostic to be **absent** when the expected VLEN matches.

This is the third time an inverted control has been found passing for the wrong
reason (§6.1, §7.4, here). The pattern is consistent enough to state as a rule:
a control that asserts an exit code asserts almost nothing.

### 9.2 High: a rejected run still wrote `passed: true`

`correctness.passed` was fixed before the MXU, SRAM and VLEN identity gates ran.
Those gates then appended failures and the process exited 1 — but the JSON on
disk said `"passed": true`, with nothing at the top level to contradict it. An
aggregation reads JSON, not exit codes.

Reproduced: live VLEN 512 against `--expect-vlen-bits 256`, exit 1, row
`"passed": true`.

The verdict is now taken once, after every check. The row also gained
`run_valid` and a `failures` array, because the two questions are different:
`correctness.passed` says the numbers match the golden, `run_valid` says the
run that produced them may be used at all. A run whose live VLEN disagreed with
its build has a correct arithmetic result from a machine that is not the one
the row describes. The live-VLEN control executes the real runner, requires
exit 1 and parses its JSON to require `run_valid=false`, the harness failure and
`correctness.passed=true`; a serializer-only unit test is not accepted as
evidence for this runner ordering.

### 9.3 Medium: the D28 identity claim was broader than the work

Recorded in §5.10 above rather than repeated here. The short version: two of
D28's four readbacks are live, the audit said the work was done, and the fix is
per-field provenance in the row rather than a broader claim. Serialization now
validates the exact configuration-field set, uniqueness and the closed label
set, so deleting or misspelling one provenance entry fails the gate.

### 9.4 Low: several places still said MB2 was unimplemented

The plan status, a CMake control comment, CLI help, the `benchmark_case.h`
header, two README passages and the MXU `unavailable` reason (which named MB1 in
MB2's own rows) were left behind when MB2 landed. All corrected; the MXU reason
is now derived from the benchmark being run rather than written out.

## 10. Review round 5 (2026-08-27) and its disposition

Four residual findings from the round-4 fixes were reproduced and closed. This
is a separate review round, not a footnote to round 4: each item below changes
what the evidence proves and therefore needs its own disposition.

### 10.1 Correct arithmetic was incorrectly coupled to run validity

The first round-4 fix made `correctness.passed` equivalent to `run_valid`.
That contradicted the schema introduced by the same fix: a row can have an
element-wise correct result while being unusable because live VLEN or MXU
identity disagrees with the requested build.

The two verdicts are now assigned independently after all gates run.
`correctness.passed` is true exactly when the complete output matches the host
golden; `run_valid` is true only when the failure list is empty. The rejected
VLEN row is the positive evidence for the distinction: correct arithmetic,
`correctness.passed=true`, identity failure and `run_valid=false`.

### 10.2 The rejected-row control did not exercise the runner

The test constructed a `result_row` directly and serialized it. That proved
the JSON writer could represent a rejected row, but not that the runner waited
for its late identity gates before writing the verdict.

`verify_vlen_rejected_row.cmake` now invokes the real runner with a deliberately
wrong expected VLEN, requires exit status 1, reads the produced JSON and checks
all four observable facts: `run_valid=false`, `correctness.passed=true`, a
harness-class identity failure and the live-readback diagnostic. A serializer
unit test alone can no longer satisfy this control.

### 10.3 Provenance coverage sampled labels instead of closing the schema

The provenance unit test checked four representative labels. A misspelled,
deleted or silently added fifth field could therefore escape while the test
continued to pass.

The test now compares the exact configuration-field set, rejects duplicate
keys and accepts only the closed provenance vocabulary (`configured`,
`live_readback` and `structural_literal`). This turns D28 provenance from
examples into schema coverage.

### 10.4 Stale MB2 statements survived the documentation sweep

Two remaining comments still described MB2 as unimplemented after its ELF and
matrix had landed. They were corrected, and the sweep was repeated across CLI
help, CMake controls, headers, README and the MXU-unavailable reason. The
reason is now derived from the benchmark/implementation rather than naming MB1
or MB2 as a literal.

The round-5 closure is therefore auditable as four dispositions, bringing the
review total to `6 + 8 + 4 + 4 + 4 = 26` findings across five rounds.

## 11. G3 measurement conservation

G3 asks that the numbers reconcile before any of them is quoted. Eight
identities are now evaluated — seven on every benchmark run and the eighth in
end-to-end mode, where a DMA leg exists to reconcile. They are not sampled and
not reported only on failure, and an unbalanced one is a `conservation` failure
that makes `run_valid` false.

### 11.1 The identities

Eight identities, of which seven are evaluated on every run and the eighth
(`dma_leg_bytes`) only in end-to-end mode, where a DMA leg exists to reconcile.

| Identity | Left | Right | Clause |
| --- | --- | --- | --- |
| `tensor_footprint` | core SRAM debug-written bytes | input + output tensor bytes | 1 |
| `kernel_local_bytes` | the kernel requester's local-plane bytes | bytes this algorithm declares it moves | 1 |
| `dma_local_bytes` | the DMA requester's local-plane bytes | bytes the DMA legs must move | 1, 2 |
| `dma_external_bytes` | DMA external-path bytes | bytes the DMA legs must move | 2 |
| `dma_leg_bytes` | DMA local-path bytes | input + output tensor bytes | 1, 2 |
| `external_boundary_bytes` | core external bytes (hart + DMA) | bytes served behind the socket (memory + host I/O) | 2 |
| `local_plane_bytes` | fabric requester bytes | core SRAM read + written bytes | 3 |
| `interval_accounted_ns` | sum of stage spans | measured interval | 4 |

Each carries its `basis` — one sentence saying why the two sides must be equal
— into the row. An identity whose justification lives only in the code that
computed it is one a reader cannot judge.

**Two observers wherever the model allows.** Two counters incremented by the
same line of code agree by construction and prove nothing.
`external_boundary_bytes` is the clean case: the memory is outside the model,
so neither end can satisfy it alone. `local_plane_bytes` is the next best — the
fabric counts what its requesters moved, core SRAM counts what it served, and
the debug path bypasses both, which is why staging cannot contaminate it.

**An agreeing total is not a correct total.** `local_plane_bytes` proves the
fabric and core SRAM see the same number; it cannot notice a kernel that reads
its input twice, because both observers would report the larger figure and the
golden would still be right. `kernel_local_bytes` closes that by comparing the
measurement against what the algorithm *declares* it moves:

```text
relu, vector_dot, gemv_mxu, gemm    input_bytes + output_bytes
gemv_rvv                           k*n + m*k*n + m*n*4
```

The GEMV form is not a fitted constant. Its kernel walks the output columns and
re-reads the whole of A inside that loop, so A is read `n` times; the model was
derived from the loop and then confirmed at all ten frozen cases, exactly,
including the non-power-of-two `(255, 63)`.

**The DMA's external side needs its own identity.**
`external_boundary_bytes` lumps hart with DMA and memory with host I/O, so an
extra DMA transaction increments both of its sides and stays balanced — the
volume can be wrong while the boundary reconciles. `dma_external_bytes` gates
that volume directly.

An unbalanced identity is a `conservation` failure and makes `run_valid` false.
A row whose bytes do not reconcile is not a slower correct measurement; it is a
measurement of something nobody can name.

### 11.2 What building it found

The first run of `external_boundary_bytes` did not balance. The gap was
constant and small — **8 bytes in kernel mode, 16 in end-to-end** — and it was
the guest's own stage and end markers: the hart reaches the simulator host-I/O
window through the same external port that reaches boot ROM, so a right-hand
side of memory alone was short by exactly the marks written inside the
interval.

The identity was wrong, not the model. The host-I/O window is now its own term
on the right, deliberately **not** folded into the memory figure, because a
memory-bandwidth number must never include it.

This is the gate working as intended on its first execution: the numbers had
been individually plausible for three review rounds and did not describe one
consistent boundary.

### 11.3 Stage timing

The guest already marked every stage for diagnostics; those marks are now
timed. A GEMM end-to-end run at 64x64x64 accounts for its interval exactly:

```text
interval prologue (before the first stage mark)      86 ns
DMA-in                                             7960 ns
the kernel                                        34075 ns
DMA-out                                           15994 ns
                                          total   58115 ns = interval
```

The prologue is a named span rather than a rounding residue. Without it the
stage list quietly failed to add up to the interval it described, by 86 ns.

**Adding up is not enough.** `interval_accounted_ns` cannot see a missing
intermediate marker: a neighbouring span simply widens and the total still
equals the interval. Measured on a run with the kernel marker dropped, that
identity still balanced at 2655 = 2655. The **expected stage sequence** is what
notices, and it is checked exactly — `DMA-in -> the kernel -> DMA-out` in
end-to-end mode, `the kernel` alone in kernel-only, with the DMA stages removed
when a control legitimately skips that leg. A per-stage duration cannot be
attributed to a stage the guest did not announce.

Overlap is zero by construction today — MB1–MB4 run their legs and kernel in
sequence — so `interval_accounted_ns` partitions the interval. A future
benchmark that overlapped two stages would break it, which is the correct
signal to extend the identity rather than relax it.

For MXU runs there is a second, non-tautological check: the engine's own
prefetch + compute + writeback must fit inside the kernel stage that contains
it. The three terms come from the engine and the span from the guest; neither
derives from the other.

### 11.4 Mutation controls

G3's last clause asks for a mutation control per counter. The controls fall
into two classes, and the audit keeps them apart because they prove different
things.

**Model-side — the machine really misbehaves.** These answer "does this counter
observe the thing it names".

| Control | Mutation | Caught by | Not caught by |
| --- | --- | --- | --- |
| DMA-leg controls (10) | a DMA leg is not issued | `golden_mismatch` **and** `conservation_broken` | — |
| `kernel_reads_input_twice` | the kernel re-reads its input inside the interval | `kernel_local_bytes` | the golden, which still passes; `local_plane_bytes`, which still balances |
| `stage_marker_dropped` | one stage marker is omitted while the stage still runs | the expected stage sequence | `interval_accounted_ns`, which still balances exactly |

The last two are the useful ones: each is caught by exactly one check and by
nothing else, which is what makes that check load-bearing rather than
decorative. Neither perturbs arithmetic — `kernel_reads_input_twice` leaves
`correctness.passed` true — so they cannot be passing for the wrong reason.

Requiring the DMA controls to name **both** detections is what surfaced the
need for `--expect-detect` to accept several at once; a control now names every
check that should fire and requires all of them.

**Comparator-side — the check really bites.** The remaining identities compare
two honest observers of the same bytes, and nothing outside the model can make
those observers disagree. What stays provable is that the identity would notice
if they did. There are two granularities, and the finer one exists because the
coarser one is not enough:

* `--inject-accounting <identity>` perturbs an identity's left-hand total. It
  shows the equality check works.
* `--inject-counter <source>` perturbs **one term** where it enters an
  identity. Several identities sum more than one counter —
  `external_boundary_bytes` adds four — and a mutation on the sum cannot tell
  whether each term is wired in. A term that was dropped, doubled or
  transposed would leave the aggregate mutation still detected and the defect
  still present. Fourteen sources have one control each:

```text
sram_debug_bytes          sram_bytes_read           sram_bytes_written
hart_external_bytes       dma_external_bytes        world_memory_read_bytes
world_memory_write_bytes  world_host_io_bytes       kernel_requester_bytes
dma_requester_bytes       dma_external_path_bytes   dma_local_path_bytes
fabric_cpu_bytes          fabric_dma_bytes          fabric_sa_bytes
fabric_transform_bytes    fabric_external_inbound_bytes
```

"Every term" means every one, and the first attempt stopped a level short in
two places. `local_plane_bytes` enumerates all five attached requesters, and
`transform` and `external_inbound` had no control — zero throughout MB1–MB4,
still summed, and a counter that is always zero is the easiest to stop summing
by accident and the hardest to notice. `world_memory_bytes` wrapped the sum of
its read and write deltas, so a dropped read or a doubled write was invisible;
it is now two terms with a control each.

The runner refuses a source no identity reads, and **fails when a perturbed
source goes unnoticed**. That second case is the interesting one: it means the
counter appears in an identity without affecting it.

**The matrix checks itself.** Both gaps in this area — two controls missing the
`g3` label, then three missing terms — were invisible to a green run, because a
control that does not exist cannot fail, and both were found by a reviewer
rather than by the suite. The runner now enumerates the sources it hooks as the
identities execute, and `tpu_v3_g3_counter_control_matrix_is_complete` compares
that enumeration against the registered controls in both directions: a hooked
source with no control fails, and a control naming a source the runner no
longer reads fails too. Verified by removing one control and by adding one for
a counter that does not exist; each produces the corresponding failure.

Both remain comparator-side and the audit says so. They mutate what an identity
is given, not what the machine did; only the three model-side controls do the
latter. Writing the "unread source" control surfaced the distinction that makes
this precise — `fabric_sa_bytes` looked like a counter no RVV run reads, and is
not: `local_plane_bytes` enumerates every attached requester, so the MXU's
counter is read on every run and is simply zero. Being unused is not being
unread.

A mutation naming an identity the run does not evaluate is refused — asking for
`dma_leg_bytes` in kernel-only mode proves nothing — and controls assert both
refusals.

### 11.5 Measured

Release, after building `neo_core_bench_runner` and `rvv_smoke_image`:

```text
full tpu_v3 label                         301 / 301 PASS, 0 skipped
microbench label                          242 / 242
G3 label                                   30 / 30
```

The `g3` label carries the two model-side mutations as well as the
comparator ones. They run MB1's image, so they were first registered with the
`mb1` label alone and a targeted `-L '^g3$'` run reported the gate green
without executing its two load-bearing controls. A control's subject is not
always its benchmark.

Result rows, with the population stated rather than implied — the earlier
draft of this section quoted a filtered count without saying what it had
filtered:

```text
rows written by the suite                        193
of which run_valid = true                        168
  identity evaluations on those rows            1246
  unbalanced                                       0
identity evaluations across all 193 rows        1402
  unbalanced                                      31   all in negative
                                                       controls, each the
                                                       failure its control
                                                       requires
```

The count per run is not uniform and the plan text now says so: seven
identities in kernel-only mode and eight in end-to-end, the extra one being
`dma_leg_bytes`, which has nothing to reconcile when no DMA leg runs. A row
rejected before its interval closed carries fewer still.

No number above is a performance result. G3 makes the counters reconcilable;
what may be concluded from them is G4's question.

## 12. Closure and next gate

G1 is closed. G2 is closed: MB1–MB4 pass their ten frozen cases in every
supported implementation and both measurement modes against a golden that links
neither SystemC nor the core model. G3 is closed: every run reconciles seven
accounting identities — eight in end-to-end mode — and a run whose bytes do not
balance is rejected rather than reported.

Neither G2's MB3/MB4 work nor G3 has had an independent review round. Five
earlier rounds found twenty-six defects in code that had already passed its own
tests, and three of those rounds found defects in the fixes from the round
before. Treat the current state accordingly.

**G4 is next**: one-factor-at-a-time screening around the current
implementation reference — local bank width, bank count, pipeline depth, DMA
burst size and SRAM capacity where footprint is relevant. Not the Cartesian
product; the plan is explicit that the nominal six-knob grid is 1,296
configurations before modes and repeats.

Two things G4 must carry forward:

* **the knobs it may move are the ones §7.2 lists as configurable today.** VLEN,
  MXU geometry, DMA channel count and external AXI width are not among them,
  and D28 puts them behind G5 and the Neo Lite work packages;
* **`arbitrated` timing mode is required for any contention claim.** Every run
  so far is `annotated`, which never blocks and therefore cannot prove bank
  contention or fairness. A screening result that varied bank count under
  `annotated` would be measuring the configuration field, not the fabric.

One observation the G3 data already supports, recorded so G4 knows where to
look rather than as a conclusion: a GEMM 64x64x64 end-to-end run spends 7960 ns
in DMA-in, 34075 ns in the kernel and 15994 ns in DMA-out, and inside that
kernel the MXU reports 768 prefetch, 1012 source-compute and 1536 writeback
cycles. Both splits point the same way — at data movement rather than at the
array — which is the second and sixth rows of the plan's §9 classification
table. It is not yet a bottleneck claim: §9 requires the reason to be derived
from counters under a stated timing mode, and `annotated` is not that mode.
