# TPU_V3

SystemC/TLM components for a 2D-mesh NoC SoC in which every mesh node is a TPU
chip containing two TPU cores. Each core is one RV32GCV hart (RVV 1.0, VLEN
512), one Shared Vector Memory, and two 128x128 MXUs.

**Status: Phase 1 of 12.** Configuration, address map and packaging exist. No
cores, MXUs, SVM or NoC are instantiated yet, and the platform binary says so
in its own output. Nothing here produces a simulation result.

## Where to start

| Document | What it answers |
| --- | --- |
| [docs/TPU_V3_IMPLEMENTATION_PLAN.md](docs/TPU_V3_IMPLEMENTATION_PLAN.md) | what is being built, in what order, and what each phase gate requires |
| [docs/TPU_V3_DECISION_RECORD.md](docs/TPU_V3_DECISION_RECORD.md) | D1–D6, approved. The authority where it and any other document disagree |
| [docs/TPU_V3_PHASE0_AUDIT.md](docs/TPU_V3_PHASE0_AUDIT.md) | measured facts: revisions, toolchain, licences, and the constraints the existing NoC imposes. P0-6, P0-7 and P0-9 are superseded by the decision record |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | what the machine is — hierarchy, data paths, ordering, fidelity levels |
| [docs/ADDRESS_MAP.md](docs/ADDRESS_MAP.md) | every region, and the rules the map satisfies |
| [docs/INTERFACE_CONTRACT.md](docs/INTERFACE_CONTRACT.md) | the TLM rules every component here must follow, with a reviewer checklist |

## Building

Off by default. The existing CDC-VP build is unaffected when it is off.

```bash
export CC=/usr/bin/gcc CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

cmake -S . -B build-tpu-v3 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCDC_BUILD_TPU_V3_SOC=ON \
    -DCDC_BUILD_TPU_V3_TESTS=ON \
    -DTPU_V3_MXU_BACKEND=fast

cmake --build build-tpu-v3 --target tpu_v3_soc -j"$(nproc)"
cmake --build build-tpu-v3 --target tpu_v3_soc_package -j"$(nproc)"

./out/tpu_v3_soc/tpu_v3_soc --config ./out/tpu_v3_soc/configs/single_chip.yaml
```

Tests:

```bash
cd build-tpu-v3 && ctest -R tpu_v3 --output-on-failure
```

## Components

| Target | Phase | Contents |
| --- | --- | --- |
| `cdc::components::tpu_v3_common` | 1 | `types.h`, `address_map.h`, `architecture_config.h`. Plain C++, no SystemC — a configuration object and an address calculation are testable without an elaboration. |
| `cdc::components::tpu_v3_svm` | 3 | Shared Vector Memory |
| `cdc::components::tpu_v3_mxu` | 4 | `mxu_if` plus the fast 128x128 backend |
| `cdc::components::tpu_v3_core` | 5 | one core and its local fabric |
| `cdc::components::tpu_v3_chip` | 6 | two cores and the chip-local fabric |
| `cdc::components::tpu_v3_noc_endpoint` | 7 | placement, chunking, local bypass |

A directory appears when its phase starts. There are no placeholder libraries.

## Four things worth knowing before reading any code

**At most 8 chips / 16 cores — the Revision 1 backend limit.** The frozen
FlooNoC chimney manager id is three bits, so `noc_interconnect` accepts at most
eight upstream initiators, and each chip presents exactly one aggregated
initiator. A 32-core system means 16 chips and needs the coordinated change
listed in decision D2, including a redesigned address map.

**Multi-chip traffic is blocked today.** `noc_interconnect` refuses any target
on a node that hosts any upstream port, and a TPU chip needs both. One chip
plus global memory works; chip-to-chip does not. Decision D1 keeps
`NoLoopback = 1` and adds an owner-aware local bypass; it is a **Phase 7
prerequisite**. Background in
[docs/TPU_V3_PHASE0_AUDIT.md](docs/TPU_V3_PHASE0_AUDIT.md) §5.1.

**A window is not a capacity.** SVM (16 MiB) and global RAM (1 GiB) always
decode their whole window; the instantiated capacity is separate, and the
target — not the decoder — refuses an access above it. The decoded map is
identical for every legal configuration, so a firmware pointer bug cannot
change symptom with the memory size.

**The MXU backend is a build-time choice, and the arithmetic is BF16/FP32.**
`TPU_V3_MXU_BACKEND` is validated at configure time and compiled into the
binary, which refuses any configuration selecting a different one. Reference
arithmetic is BF16 operands with IEEE FP32 accumulation and a fixed
accumulation order (D6).
