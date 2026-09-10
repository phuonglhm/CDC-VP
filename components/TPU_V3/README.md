# TPU_V3

> **Active scope rebaseline — 2026-08-25.** Current work targets exactly one
> standalone **NEO-CORE** and no NoC. Dual-core chip composition, chip-local
> fabric, chip endpoints, mesh composition and multi-chip tests are retained as
> historical Phase 8/9 work but are outside the current execution scope. Start
> with [the standalone microbenchmark/DSE plan](docs/NEO_CORE_MICROBENCH_DSE_PLAN.md),
> which overrides the old chip/NoC schedule wherever they differ.

The active component is one RV32GCV hart (RVV 1.0, reference VLEN 512), one
shared Core SRAM, one MXU, one independent DMA and one Transform block behind
the D15 split control/local-data/external interconnect. Phase 7 already composed
these into `tpu_core` and proved from one firmware ELF the end-to-end
`DMA -> Transform (Im2Col) -> MXU -> RVV` pipeline with the external socket
bound directly to memory/host I/O. The next milestone is a standalone
microbenchmark harness and single-core design-space exploration, not Phase 9.

## Where to start

| Document | What it answers |
| --- | --- |
| [docs/README.md](docs/README.md) | **mandatory entry point:** current direction, precedence order, inactive historical documents and the exact next task for any AI agent or contributor |
| [docs/NEO_CORE_MICROBENCH_DSE_PLAN.md](docs/NEO_CORE_MICROBENCH_DSE_PLAN.md) | **active plan:** one-core boundary, four microbenchmarks, ten cases each, measurement modes, knob readiness, fidelity limits and G0–G6 gates |
| [docs/TPU_V3_STANDALONE_DSE_AUDIT.md](docs/TPU_V3_STANDALONE_DSE_AUDIT.md) | active implementation evidence; G1 proves one hart/core, direct external memory, the Phase 7 pipeline and absence of chip/NoC implementation dependencies |
| [docs/TPU_V3_IMPLEMENTATION_PLAN.md](docs/TPU_V3_IMPLEMENTATION_PLAN.md) | what is being built, in what order, and what each phase gate requires |
| [docs/TPU_V3_DECISION_RECORD.md](docs/TPU_V3_DECISION_RECORD.md) | architectural decisions and their scope; D27 records the current standalone rebaseline |
| [docs/TPU_V3_PHASE0_AUDIT.md](docs/TPU_V3_PHASE0_AUDIT.md) | measured facts: revisions, toolchain, licences, and the constraints the existing NoC imposes. P0-6, P0-7 and P0-9 are superseded by the decision record |
| [docs/TPU_V3_PHASE2_AUDIT.md](docs/TPU_V3_PHASE2_AUDIT.md) | the RV32GCV backend: findings F1–F13, the patch series, and the Spike differential result |
| [docs/TPU_V3_PHASE5_AUDIT.md](docs/TPU_V3_PHASE5_AUDIT.md) | the 64x64 MXU and its pinned Sauria v4.2 implementation source |
| [docs/TPU_V3_PHASE6_AUDIT.md](docs/TPU_V3_PHASE6_AUDIT.md) | the Transform block's pinned Im2Col source/layout/golden evidence and explicit Col2Im boundary |
| [docs/TPU_V3_PHASE7_AUDIT.md](docs/TPU_V3_PHASE7_AUDIT.md) | the NEO-CORE composition: the defects composing surfaced, and the D19 reset implementation |
| [docs/TPU_V3_PHASE8_AUDIT.md](docs/TPU_V3_PHASE8_AUDIT.md) | historical dual-core evidence; not part of the active standalone scope |
| [docs/TPU_V3_PHASE9_NOC_REBASELINE.md](docs/TPU_V3_PHASE9_NOC_REBASELINE.md) | historical NoC work; not the live work list while D27 is active |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | what the machine is — hierarchy, data paths, ordering, fidelity levels |
| [docs/ADDRESS_MAP.md](docs/ADDRESS_MAP.md) | every region, and the rules the map satisfies |
| [docs/INTERFACE_CONTRACT.md](docs/INTERFACE_CONTRACT.md) | the TLM rules every component here must follow, with a reviewer checklist |

## Existing regression build

The commands below build the existing TPU_V3 regression/platform target. They
remain useful for non-regression, but they are **not** the standalone D27
microbenchmark harness. That new executable and its exact command become
publishable only after DSE gate G1 creates and verifies the target; do not use
the old `single_chip.yaml` run as one-core DSE evidence.

Off by default. The existing CDC-VP build is unaffected when it is off.

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
# sanity
$CC -dumpfullversion
$CXX --version | head

cmake -S . -B build-tpu-v3 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCDC_BUILD_TPU_V3_SOC=ON \
    -DCDC_BUILD_TPU_V3_TESTS=ON \
    -DCDC_BUILD_TPU_V3_IMAGE_TRANSFORM=ON \
    -DTPU_V3_SA_GEOMETRY=64x64

cmake --build build-tpu-v3 --target tpu_v3_soc -j"$(nproc)"
cmake --build build-tpu-v3 --target tpu_v3_soc_package -j"$(nproc)"

./out/tpu_v3_soc/tpu_v3_soc --config ./out/tpu_v3_soc/configs/single_chip.yaml
```

Tests:

```bash
cd build-tpu-v3 && ctest -L tpu_v3 --output-on-failure
```

## Components

| Target | Phase | Contents |
| --- | --- | --- |
| `cdc::components::tpu_v3_common` | 1, 3 | `types.h`, `address_map.h`, `architecture_config.h`, `sparse_memory.h`. Plain C++, no SystemC — a configuration object, an address calculation and a page-backed store are testable without an elaboration. |
| `cdc::components::tpu_v3_core_sram` | 3 | the shared core SRAM: window/capacity policy, sparse page backing, byte enables, counters, debug transport |
| `cdc::components::tpu_v3_tpu_core` | 3 | the D15 split — `neo_control_fabric` (32-bit AXI4-Lite), `neo_local_sram_fabric` (native banked data plane), `neo_external_bridge`, and the core-local register files. `tpu_core` itself is Phase 7. |
| `cdc::components::tpu_v3_neo_dma` | 4 | the independent NEO DMA — see [neo_dma/DMA_MODEL.md](neo_dma/DMA_MODEL.md) |
| `cdc::components::tpu_v3_sauria_matrix` | 5 | MXU implementation: `sauria_matrix_if` plus the extracted Sauria v4.2 64x64 matrix engine |
| `cdc::components::tpu_v3_image_transform` | 6 | Transform implementation: pinned CHW INT8 Im2Col capability; Col2Im is explicitly unavailable — see [image_transform/IMAGE_TRANSFORM_MODEL.md](image_transform/IMAGE_TRANSFORM_MODEL.md) |
| `cdc::components::tpu_v3_chip_fabric` | 8, retained | historical chip-local fabric source; outside the active standalone dependency closure |
| `cdc::components::tpu_v3_tpu_chip` | 8, retained | historical two-core chip-composition source; outside the active standalone dependency closure |
| `cdc::components::tpu_v3_noc_endpoint` | 9, retained | historical chip/mesh endpoint source and evidence; outside the active standalone dependency closure |

A directory appears when its phase starts. There are no placeholder libraries.

The RV32GCV backend lives outside this tree, in
[`cpu_models/riscv_vp_plusplus`](../../cpu_models/riscv_vp_plusplus): it is a
CPU model like the others, not a TPU_V3 component.

## Things worth knowing before reading any code

**The active machine is one core.** Instantiate chip 0/core 0, require
firmware-visible `mhartid=0`, and bind its external interface directly to the
standalone Boot ROM/global RAM/host-I/O target. A result from this harness is
not a chip, multi-core or NoC result.

**Do not infer topology capacity from the three-bit AXI transaction ID.** The
existing wrapper assigned an upstream port number to AXI ID and therefore
imposed an eight-port adapter limit. FlooNoC routing identifies nodes with
`src_id`/`dst_id`; the old D2 argument is not an RTL node-count rule and is
irrelevant to the standalone study.

**Phase 8/9 source remains in the tree only as retained work.** It must not be
linked into the standalone benchmark executable or package, and the dependency
guard in DSE gate G1 must prove that boundary.

**A window is not a capacity, and neither is host memory.** The active core's
SRAM has a 16 MiB reference capacity. Its standalone external-memory target
has a separately configured address window and capacity; the target — not the
decoder — refuses an access above that capacity. Storage is sparsely backed in
deterministic 4 KiB pages (D6), so host allocation follows pages touched rather
than the full decoded window. Multi-chip aggregate capacities previously
quoted by Phase 8/9 are not properties of this one-core experiment.

**The internal interconnect is deliberately not one AXI crossbar.** D15 splits
it three ways: a 32-bit AXI4-Lite control plane, a native banked local-data
plane, and an external-memory boundary. In D27 that external boundary binds
directly to the standalone memory/host target; no NoC is present. The local
plane is not AXI and must not be implemented or described as a full AXI data
crossbar; no accelerator obtains a pointer into SRAM backing.

**The datapath width, bank count and pipeline depth are not frozen.** D15
leaves them to the SRAM macro, the clock target and PD constraints, so the C++
schema has no default and refuses zero. The shipped configurations state
provisional values and every report prints them with that word attached.

**The verified MXU baseline is 64x64 INT8 x INT8 -> INT32.**
`TPU_V3_SA_GEOMETRY` is validated at configure time and compiled into the
binary, which refuses any configuration selecting a different geometry. The
64x64 v4.2 profile is the only backend admitted to the active DSE. A 128x128
array remains a future NPU-team promotion, and BF16 x BF16 with FP32
accumulation remains an architectural target from D6; neither may be reported
as implemented or benchmarked by the present model.
