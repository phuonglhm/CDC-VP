# TPU_V3 Standalone NEO-CORE DSE Audit

Status: **G0 and G1 complete; G2 is the next implementation gate**

Evidence date: 2026-08-25

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

## 5. Closure and next gate

G1 is closed. It proves the standalone architectural boundary and preserves
the earlier non-zero-hart pipeline gate. It does **not** yet prove any DSE
performance result.

G2 starts with MB1 ReLU, ten frozen sizes, scalar and RVV implementations, full
element-wise golden comparison and the machine-readable per-case result
schema. MB2–MB4 follow only after that foundation is reviewed.
