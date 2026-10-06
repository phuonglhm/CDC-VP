# FX1 ISP — SystemC/TLM model

Loosely-timed SystemC/TLM-2.0 model of the FTEL ISP IP
(`FTEL_IP_ISP_HAS_v1.0`, `FTEL_IP_ISP_CSR_v1.0`), memory-to-memory topology.
It is built and verified as a standalone IP. Integration into a CPU platform
such as `VP_FX1_Full_SoC` comes in a later phase.

**Status: M0–M5 done; standalone handover.**

| Milestone | Content |
|---|---|
| M0 | Contracts |
| M1 | Component, CSR, reset, IRQ |
| M2 | DMA engines and frame lifecycle |
| M3 + MR | All 16 image blocks, AEC/AWB/AF statistics, independent Python reference |
| M4 | Standalone qualification |
| M5 | Programming guide, C reference driver, CMake package, per-register traceability, acceptance evidence |

The model is bit-exact with the project's own reference on:
- 32 vectors, at unit level, through the TLM/DMA path and through the
  reference driver;
- two 4K cases;
- a 102-frame synthetic continuous sequence with injected resets and AXI
  errors;
- all 28 real RAW frames under four profiles, one session per frame and as
  one back-to-back sequence (116 runs, 308 frames, every frame compared;
  baseline in [docs/evidence](docs/evidence/README.md)).

There is no owner golden (DEC-05): "bit-exact" means agreement between two
independent implementations of the same reading of the HAS. Where the HAS
is incomplete, the choice is a documented assumption (see the decisions log
and `plan/alg/OWNER_QUERIES.md`). Next: platform integration (F1, DEC-08).

Start here:
- firmware / driver: [docs/ISP_PROGRAMMING_GUIDE.md](docs/ISP_PROGRAMMING_GUIDE.md) and `driver/`;
- platform integration: [docs/ISP_MODEL_INTERFACE.md](docs/ISP_MODEL_INTERFACE.md) §5.1;
- verification status: [docs/ISP_REQUIREMENTS_MATRIX.md](docs/ISP_REQUIREMENTS_MATRIX.md)
  and [docs/ISP_M4_QUALIFICATION.md](docs/ISP_M4_QUALIFICATION.md).

## Build and test

The environment setup below is **mandatory** before configuring, building or
running, in the same shell. On this host a bare PATH resolves `nm` and other
tools to a Synopsys VP wrapper that recurses without end.

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
# sanity
$CC -dumpfullversion
$CXX --version | head

make test                        # configure (Ninja) + build + CTest, BUILD_DIR=build
# or, by hand:
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=$CXX
cmake --build build
ctest --test-dir build --output-on-failure
```

If a build directory was configured with another compiler, use a new one: CMake
keeps the cached compiler even after `CC`/`CXX` change. `make configure`
refuses such a directory.

Requirements: GCC ≥ 11 (C++17), CMake ≥ 3.21, Ninja, and SystemC 2.3.4 built
for C++17 (default `SYSTEMC_HOME=/opt/systemc-2.3.4`).

### CSR artefacts

The register header and table are generated from the confidential CSR
spreadsheet. The spreadsheet is not in the repository, and the build does not
need it. To regenerate or verify (needs Python 3 with `openpyxl`):

```bash
make csr-check CSR_XLSX=/path/to/FTEL_IP_ISP_CSR_v1.0.xlsx   # tree up to date + independent cross-check
make csr-gen   CSR_XLSX=/path/to/FTEL_IP_ISP_CSR_v1.0.xlsx   # regenerate
make test      CSR_XLSX=/path/to/FTEL_IP_ISP_CSR_v1.0.xlsx   # also runs both checks in CTest
```

## Running a real RAW frame

```bash
python3 tools/raw_fixture.py convert --help                  # RAW -> ISP container (.isp16)
python3 tools/make_profile.py --help                         # CSR profile for a sensor frame
build/fx1_isp_run_raw --input f.isp16 --width 2688 --height 1520 \
                      --profile f.csrw --out out/f           # -> out/f.nv12, out/f.txt
python3 tools/nv12_preview.py --help                         # NV12 -> PNG
```

For a BGGR 2688×1520 frame, the expected output is `NV12 2686x1518, 1 frame(s)
done, DMA_ERR=0x2`. `make_profile.py --preset` selects one of the four test
profiles (DEC-34). Bit 1 (`IDMA_UNDERRUN`) is expected. The tool queues one input
buffer and leaves `IDMA_EN` set, so after the frame the IDMA finds no valid
buffer and reports underrun. That is HAS §6.25.8.3 behaviour, not a frame
error. The tool says so, and its exit status is non-zero only if the frame did
not complete or another `DMA_ERR` bit is set. The NV12 is the project's own
reading of the HAS, with a test profile from `make_profile.py`. It is not
camera-tuned output and has no owner golden (DEC-05, DEC-10).

### Real-RAW regression

```bash
python3 tools/run_regression.py --inventory /path/plan/ISP_RAW_INVENTORY.json \
        --run-raw build/fx1_isp_run_raw --out /path/outside/the/tree --jobs 6
```

Every inventory frame runs with every preset. The 28 frames also run back
to back in one VP session for each preset. Every frame is compared bit for
bit against the Python reference. Plan §10 artefacts go to `--out`, with a `summary.md`. The same
regression runs as CTest `fx1_isp.regression.raw` when the build is
configured with `-DFX1_ISP_RAW_INVENTORY=…` (and optionally
`-DFX1_ISP_RAW_OUT=…`). It needs numpy and PIL and takes about 6 minutes. To run a subset, use
`--only <text>`, with `--mode image` for single frames only. With
`--baseline`, a subset compares the runs the baseline contains and reports
its own sequence run as not compared.

## Install and use as a package

```bash
cmake --install build --prefix /path/to/prefix
# in the platform's CMakeLists.txt:
#   find_package(fx1_isp 0.1 REQUIRED)        # CMAKE_PREFIX_PATH=/path/to/prefix
#   target_link_libraries(vp PRIVATE fx1_isp::fx1_isp_tlm fx1_isp::fx1_isp_drv)
```

### Handover package

| Part | Installed (`cmake --install`) | Needs the source checkout |
|---|---|---|
| Integration | libraries `fx1_isp_core`, `fx1_isp_tlm`, `fx1_isp_drv`; headers `fx1_isp_tlm.h`, `fx1_isp_csr.h`, `fx1_isp_drv.h`; CMake package | — |
| Documents | `share/doc/fx1_isp`: guide, interface, contract, decisions, requirements, traceability, M4 report, manifest, schema | — |
| Evidence | `share/doc/fx1_isp/evidence`: regression baseline (SHA-256 of 308 frames), CTest summary, mutation summary, environment | full re-run of every CTest test |
| Tools | `bin/fx1_isp_run_raw`; `share/fx1_isp/tools`: `run_regression.py`, `raw_fixture.py`, `make_profile.py`, `nv12_preview.py`, with the Python reference and the schema | vector generators, CSR generator, traceability generator |
| Samples | `share/fx1_isp/samples`: the four test profiles, the 4K cases, `run_4k_check.sh` | — |
| Tests and vectors | — | `tests/`, `tests/data/` (13 MB) |

From the prefix alone, `share/fx1_isp/samples/run_4k_check.sh` checks the
model bit for bit on the two 4K cases. With the dataset, the installed
regression reproduces the baseline. Everything else in the evidence needs
the source tree (CTest `fx1_isp.package.consumer` checks the installed
parts).

Inside CDC-VP, `add_subdirectory` works as well (the parent provides
`SystemC::systemc`).

## Layout

```text
include/fx1_isp/fx1_isp_tlm.h     public SystemC module and parameters
include/fx1_isp/fx1_isp_csr.h     generated C register header (FW/SW)
driver/                           C99 reference driver (fx1_isp_drv.h / .c)
cmake/                            package configuration template
src/registers/                    generated table + generic access semantics
src/control/                      reset, soft reset, IRQ/error aggregation, LUT ports, commit gates, statistics publication
src/dma/                          burst formation and start checks (no SystemC)
src/engine/                       IDMA, pipeline, ODMA threads and line FIFOs
src/pipeline/                     row-streaming image pipeline and statistics taps (no SystemC)
reference/fx1_isp_ref/            independent numpy reference (pipeline, statistics)
tools/gen_vectors.py, gen_block_vectors.py, gen_4k_vectors.py, gen_sequence.py
                                  vector generators (--check to verify)
tools/run_regression.py           real-RAW regression (plan §10 artefacts)
tools/run_raw.cpp, raw_fixture.py, make_profile.py, nv12_preview.py   real-RAW demo tools
src/fx1_isp_tlm.cpp               TLM adapter, reset and IRQ processes
tools/gen_csr.py                  CSR generator (pinned SHA-256, overrides with IDs)
tools/check_csr_header.py         independent header cross-check
tools/gen_traceability.py         per-register traceability report (--check)
tests/unit, tests/integration     CTest suites; tests/package: installed-package consumer
docs/                             contracts, decisions, guide, traceability
docs/evidence/                    acceptance evidence (regression baseline, CTest, mutation, environment)
```

## Documents

- [docs/ISP_SOURCE_MANIFEST.md](docs/ISP_SOURCE_MANIFEST.md): pinned spec revisions
- [docs/ISP_DECISIONS_AND_DISCREPANCIES.md](docs/ISP_DECISIONS_AND_DISCREPANCIES.md): DEC / SPEC / CSR log
- [docs/ISP_CSR_CONTRACT.md](docs/ISP_CSR_CONTRACT.md): register behaviour the model implements
- [docs/ISP_MODEL_INTERFACE.md](docs/ISP_MODEL_INTERFACE.md): ports, parameters, timing, backdoor
- [docs/ISP_REQUIREMENTS_MATRIX.md](docs/ISP_REQUIREMENTS_MATRIX.md): requirement → test → status
- [docs/ISP_M4_QUALIFICATION.md](docs/ISP_M4_QUALIFICATION.md): M4 results, regression baseline, benchmark
- [docs/ISP_PROGRAMMING_GUIDE.md](docs/ISP_PROGRAMMING_GUIDE.md): programming sequences, reference driver
- [docs/ISP_CSR_TRACEABILITY.md](docs/ISP_CSR_TRACEABILITY.md): every register → model → tests
- [docs/evidence/README.md](docs/evidence/README.md): acceptance evidence
