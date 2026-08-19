# TPU_V3

SystemC/TLM components for a 2D-mesh NoC SoC in which every mesh node is a TPU
chip containing two **NEO-COREs**. Each NEO-CORE is one RV32GCV hart (RVV 1.0,
VLEN 512), one shared core SRAM, one MXU, one independent DMA and one Transform
block, behind the split control / local-data /
external interconnect of decision record D15.

**Status: Phase 6 of 12 complete.** Configuration, the address map, packaging,
the RV32GCV backend, sparsely page-backed core SRAM, the three NEO-CORE
fabrics, independent DMA, extracted 64x64 INT8/INT32 MXU and the Transform
component with its Im2Col capability exist and are gated. Nothing is composed
into a NEO-CORE yet — that is Phase 7 — and the platform manifest must still
report every unlinked component honestly.

## Where to start

| Document | What it answers |
| --- | --- |
| [docs/TPU_V3_IMPLEMENTATION_PLAN.md](docs/TPU_V3_IMPLEMENTATION_PLAN.md) | what is being built, in what order, and what each phase gate requires |
| [docs/TPU_V3_DECISION_RECORD.md](docs/TPU_V3_DECISION_RECORD.md) | D1–D20, approved. The authority where it and any other document disagree |
| [docs/TPU_V3_PHASE0_AUDIT.md](docs/TPU_V3_PHASE0_AUDIT.md) | measured facts: revisions, toolchain, licences, and the constraints the existing NoC imposes. P0-6, P0-7 and P0-9 are superseded by the decision record |
| [docs/TPU_V3_PHASE2_AUDIT.md](docs/TPU_V3_PHASE2_AUDIT.md) | the RV32GCV backend: findings F1–F13, the patch series, and the Spike differential result |
| [docs/TPU_V3_PHASE5_AUDIT.md](docs/TPU_V3_PHASE5_AUDIT.md) | the 64x64 MXU and its pinned Sauria v4.2 implementation source |
| [docs/TPU_V3_PHASE6_AUDIT.md](docs/TPU_V3_PHASE6_AUDIT.md) | the Transform block's pinned Im2Col source/layout/golden evidence and explicit Col2Im boundary |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | what the machine is — hierarchy, data paths, ordering, fidelity levels |
| [docs/ADDRESS_MAP.md](docs/ADDRESS_MAP.md) | every region, and the rules the map satisfies |
| [docs/INTERFACE_CONTRACT.md](docs/INTERFACE_CONTRACT.md) | the TLM rules every component here must follow, with a reviewer checklist |

## Building

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
| `cdc::components::tpu_v3_chip` | 8 | two cores and the chip-local fabric |
| `cdc::components::tpu_v3_noc_endpoint` | 9 | placement, chunking, local bypass |

A directory appears when its phase starts. There are no placeholder libraries.

The RV32GCV backend lives outside this tree, in
[`cpu_models/riscv_vp_plusplus`](../../cpu_models/riscv_vp_plusplus): it is a
CPU model like the others, not a TPU_V3 component.

## Six things worth knowing before reading any code

**At most 8 chips / 16 cores — the Revision 1 backend limit.** The frozen
FlooNoC chimney manager id is three bits, so `noc_interconnect` accepts at most
eight upstream initiators, and each chip presents exactly one aggregated
initiator. A 32-core system means 16 chips and needs the coordinated change
listed in decision D2, including a redesigned address map.

**Multi-chip traffic is blocked today.** `noc_interconnect` refuses any target
on a node that hosts any upstream port, and a TPU chip needs both. One chip
plus global memory works; chip-to-chip does not. Decision D1 keeps
`NoLoopback = 1` and adds an owner-aware local bypass; it is a **Phase 9
prerequisite**. Background in
[docs/TPU_V3_PHASE0_AUDIT.md](docs/TPU_V3_PHASE0_AUDIT.md) §5.1.

**A window is not a capacity, and neither is host memory.** Core SRAM (16 MiB)
and global RAM (1 GiB) always decode their whole window; the instantiated
capacity is separate, and the target — not the decoder — refuses an access
above it. Behind the capacity is a third quantity: storage is sparsely backed
in deterministic 4 KiB pages (D6), so the largest configuration describes
1.25 GiB of logical memory and costs a few megabytes until firmware writes to
it.

**The internal interconnect is deliberately not one AXI crossbar.** D15 splits
it three ways: a 32-bit AXI4-Lite control plane, a native banked local-data
plane, and an external AXI4/NoC boundary. The local plane is not AXI and must
not be implemented or described as a full AXI data crossbar; no accelerator
obtains a pointer into SRAM backing, and inbound remote traffic is arbitrated
like any other requester.

**The datapath width, bank count and pipeline depth are not frozen.** D15
leaves them to the SRAM macro, the clock target and PD constraints, so the C++
schema has no default and refuses zero. The shipped configurations state
provisional values and every report prints them with that word attached.

**The matrix geometry is a build-time choice, and the arithmetic is BF16/FP32.**
`TPU_V3_SA_GEOMETRY` is validated at configure time and compiled into the
binary, which refuses any configuration selecting a different one. 64x64 is the
verified v4.2 bring-up array; 128x128 is the architectural destination and is
refused until the NPU team's promotion gate passes (D14). Reference arithmetic
is BF16 operands with IEEE FP32 accumulation and a fixed accumulation order
(D6).
