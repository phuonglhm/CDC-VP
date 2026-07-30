# SAURIA NPU — Unified SystemC Model

A self-contained, header-only **SystemC functional model** of the SAURIA NPU core (v1),
with a runtime `PROFILE` selector, multi-datatype support (INT8 / FP16 / INT16), a case-driven
demo/verification pack, and a **pure-C driver library** (`driver/`) for generating the core
configuration and DRAM images without any Python at runtime.

> **Status:** all **11 bundled demos** verify against the SAURIA golden reference —
> INT8 and INT16 are **exact (0 mismatch)**, FP16 matches within a small ULP tolerance.
> The C driver (config encoder + tensor packer + golden + register stream) is **bit-exact**
> vs the reference across every geometry and datatype. A brand-new test can be **generated and
> verified entirely in C** — no Python (`make selftest` / `goldtest` / `stimtest`).

Copy this folder anywhere and build from inside it — it has no external source dependencies
beyond a SystemC installation and a C++17 compiler.

---

## 1. Overview

- **One core.** A single v1 SAURIA datapath (templated on the element type). A runtime `PROFILE`
  register routes the _config decode_ only (`PROFILE_V1_SAURIA = 0`, default; `PROFILE_V4_LINEAR = 1`).
- **Three datatypes as build variants.** INT8, FP16 (half-precision), INT16 — selected at build
  time (the datatype is a hardware property, not a runtime flag). See §5.
- **Case-driven demos.** `tb_demo` (auto-generated from `tb_evaluate.cpp`) reads all inputs of a
  test case from a folder via `NPU_DEMO_*` environment variables, so new cases are added as data —
  no code changes. Each run prints the test title, decoded config, a sample match table, and a
  performance block (cycles, MACs, MAC/cycle, array utilization).
- **Pure-C driver library.** `driver/` turns a layer descriptor into the register program
  (`controller_args`) and the DRAM image, and reads the output back — no Python in the loop.

## 2. Requirements

- A C++17 compiler (`g++` 9+; validated on g++ 11.4). `fp16_t` is plain C++17 (no `_Float16`),
  so older toolchains compile it too.
- **SystemC 2.3.3** — either a system-wide package (headers on the default include path,
  `-lsystemc`) or an Accellera source install pointed to by `SYSTEMC_HOME` (see §6).
- `python3` — only to (re)generate `tb_demo.cpp` from `tb_evaluate.cpp`. The driver library and
  the bundled demos need **no** Python at runtime. Generating brand-new test cases (or the
  golden/`enctest`/`memtest` reference) additionally needs the SAURIA Python pipeline.

## 3. Quick start

```bash
make help                 # list all targets and knobs
make check                # build + run ALL demo cases + config round-trip  -> ALL PASS
make smoke                # profile-routing smoke test
make mvm                  # INT8 MVM 8x16 demo                 -> TEST PASSED, 0 mismatch
make demo CASE=demo_fp16_gemm_64x64    # any captured case
make list                 # list available demo cases
```

`make demo` builds `tb_demo` with the geometry, index widths, datatype flags and SRAM region
sizes stored in the case's `case.env`, then runs it and prints a PASS/FAIL summary. Full logs go
to `demo_runs_clean/<case>/logs/tb_demo.log`.

## 4. Makefile targets & knobs

| Target                           | Description                                                        |
| -------------------------------- | ------------------------------------------------------------------ |
| `make help`                      | Usage (default target).                                            |
| `make check`                     | Build + run **every** demo case + `cfgtest`; smoke-CI, fails on any mismatch. |
| `make smoke`                     | Build + run the profile-routing smoke test.                        |
| `make mvm` / `gemm32` / `gemm64` | Run a verified INT8 demo case.                                     |
| `make demo CASE=<name>`          | Build + run any captured case (geometry + datatype from `case.env`). |
| `make list`                      | List demo cases under `npu_demo_clean/cases/`.                     |
| `make eval`                      | Build + run `tb_evaluate` against `stimuli/`.                      |
| `make cfgtest`                   | Config encode↔decode round-trip self-test (no SystemC needed).     |
| `make goldtest`                  | C reference-conv golden vs every case's `gold_dram` — **no Python needed**. |
| `make stimtest`                  | C `GoldenStimuli` emitter vs every shipped case — **no Python needed**.     |
| `make selftest`                  | **Generate** brand-new cases entirely in C and run them through `tb_demo` — **fully Python-free**. |
| `make enctest` / `memtest`       | Driver bit-exact vs SAURIA Python (needs the SAURIA Python + `SAURIA_PY`). |
| `make shape SHAPE="..."`         | Generate + build + run a fresh SAURIA shape (needs SAURIA Python). |
| `make all` / `clean`             | Build all testbenches / remove built binaries.                     |

Flexible build knobs (override on the command line): `GEO=8x16|32x32|64x64`, `EVAL_X=`/`EVAL_Y=`,
`IDX_FLAGS="-D..."`, `RB=` (SRAM region bytes), `DEBUG=1`, `OPT="-O0 -g"`, `EXTRA="-D..."`,
`SYSTEMC_HOME=/path`. Run `make help` for the full list.

## 5. Datatypes (INT8 / FP16 / INT16)

The datatype is a **build variant** (SAURIA fixes the element widths per hardware version), not a
runtime toggle. All per-version constants live in one source of truth, `sauria_targets.csv`
(→ `sauria_targets.h` via `tools/gen_targets.py`).

| Datatype | In / Out (bit) | Accumulate | Verify          | Build flags                                                        |
| -------- | -------------- | ---------- | --------------- | ------------------------------------------------------------------ |
| INT8     | 8 / 32         | int32      | exact (0)       | *(default)*                                                        |
| FP16     | 16 / 16        | float32    | ≤ 16 ULP        | `-DNPU_FP16 -DNPU_DTYPE_IN=fp16_t -DNPU_DTYPE_OUT=fp16_t -DNPU_DTYPE_PSUM=float` |
| INT16    | 16 / 64        | int64      | exact (0)       | `-DNPU_INT16 -DNPU_DTYPE_IN=int16_t -DNPU_DTYPE_OUT=int64_t -DNPU_DTYPE_PSUM=int64_t` |

- **INT8 / INT16** are integer, hence bit-exact (0 mismatch). INT16 uses a 64-bit accumulator
  (`OC_W=64`) to avoid overflow.
- **FP16** is compared within a ULP tolerance (`NPU_FP16_ULP_TOL`, default 16): floating-point add
  is not associative, so the array's summation order differs from the reference by last-bit
  rounding that grows with the reduction depth (8×16 exact, 32×32 ≤1 ULP, 64×64 ≤8 ULP). A logic
  bug would be off by hundreds of ULP / NaN.
- The right flags are baked into each demo's `case.env` and into `run_shape.sh` (by version name).

## 6. SystemC configuration

The Makefile resolves SystemC in this order:

1. **Company EDA server** (`hostname` = `eda-server-01`): uses the Accellera build at
   `/opt/arm/fastmodels/SystemC/Accellera/SystemC` with libs in `dynlib/Linux64_GCC-10.3`.
2. **Explicit source install:** pass `SYSTEMC_HOME=/path`. The lib dir is auto-detected
   (`lib-linux64`, then `lib64`); override with `SC_LIBDIR=/path` if it differs.
3. **System-wide package** (default): headers on the default path, linked with `-lsystemc`.

The last line of `make help` prints which SystemC is in effect. If auto-detection misses your
environment, pass `SYSTEMC_HOME=` (and `SC_LIBDIR=` if needed) explicitly.

## 7. Driver library (`driver/`) — config + data, no Python

A pure-C, SystemC-free library for a software driver/framework to prepare a core run:

- `driver/libsauria_cfg.h` — `sauria_encode_controller_args(desc, target, dram_bases)` builds the
  full register program (`args[0..21]` tiling + `args[22..]` core config). **Bit-exact** vs the
  SAURIA reference (`make enctest`).
- `driver/libsauria_mem.h` — `sauria_assemble_dram(...)` packs activations/weights/preloads into
  the DRAM image; `sauria_unpack_output(...)` reads results back. **Bit-exact** (`make memtest`).
- `driver/sauria_run.h` — `sauria_prepare(target, desc, A, B, Cpre)` → `{controller_args,
  initial_dram, offsets}`; after the core runs, `sauria_read_output(...)`.
- `driver/sauria_golden.h` — `sauria_reference_conv(...)` computes the expected output (the golden)
  as a plain C reference convolution: **no Python, no torch**. INT is bit-exact vs SAURIA; FP16 is
  within the same ULP tolerance as the model. Validated against every captured case (`make goldtest`).
- `driver/sauria_stim.h` — `sauria_emit_stim(controller_args)` builds the `GoldenStimuli.txt`
  register-command stream. Bit-exact vs the shipped files (`make stimtest`).
- Per-version constants via `sauria_find_target("int8_32x32")` (reads `sauria_targets.h`).

These share the packed-config bit-layout (`sauria_cfg_layout.h`) with the model's decoder, so the
encoder and decoder cannot drift. **Config + DRAM packing + golden + GoldenStimuli are all in C**, so
`tools/gen_case.cpp` (`make selftest`) generates a complete, runnable test case — inputs, register
program and golden — entirely in C, no SAURIA Python. (The golden order currently covers the
`Cw == Yused` regime; W-splitting output reorder is a documented follow-up.)

## 8. Demo pack (`npu_demo_clean/`)

```
npu_demo_clean/
  README.md                 pack overview
  docs/CONFIG_GUIDE.md      configuration meaning & impact (for the software team)
  scripts/                  build_tb_demo.sh, run_tb_demo_case.sh, capture_case.sh, ...
  tools/                    create_tb_demo_from_tb_evaluate.py  (generates tb_demo.cpp)
  cases/<name>/             stimuli/, sauria_tmp/, case.env  (one folder per test case)
```

Bundled cases: `demo_mvm_8x16`, `demo_gemm_32x32`, `demo_gemm_64x64`, `demo_strided_32x32`,
`demo_multitile_32x32` (INT8); `demo_fp16_mvm_8x16`, `demo_fp16_gemm_32x32`, `demo_fp16_gemm_64x64`
(FP16); `demo_int16_mvm_8x16`, `demo_int16_gemm_32x32` (INT16); `conv5x5_demo`.

- `tb_demo.cpp` is **auto-generated**: `make` regenerates it whenever `tb_evaluate.cpp` changes.
- Each case's `case.env` records `TITLE`, `EVAL_X/EVAL_Y`, `IDX_FLAGS` (incl. datatype flags), and
  `REGION_BYTES`, so a case rebuilds `tb_demo` with the correct flags automatically.
- Capturing a new case (requires the SAURIA Python pipeline):
  ```bash
  bash npu_demo_clean/scripts/capture_case.sh <name> "<shape>" EVAL_X EVAL_Y <version> \
       "TITLE" "DESC" "IDX_FLAGS" REGION_BYTES
  ```
  where `<shape>` = `Bw Bh d s Cin Cw Ch Cout Xused Yused preload`.

## 9. Directory layout

```
Core (header-only SystemC):
  npu_top.h            top module (config_regs + datapath)
  config_regs.h        profile-aware config registers (table-driven decode + read-back)
  config_map.h         V1/V4 address->field maps + cfg_lookup()
  npu_profile.h        PROFILE enum + register address (override-able)
  debug.h              DBG_COUT toggle (OFF unless -DSAURIA_DEBUG=1)
  sauria_types.h       shared types + memory address map
  control/ data_feeder/ systolic_array/ psm/ sram/ instrumentation/

Shared headers (core decoder + driver encoder):
  fp16.h               software IEEE-754 half type (SystemC-free)
  sauria_cfg_layout.h  packed-config bit-layout (shared encode/decode)
  sauria_targets.h     HW-version manifest (generated from sauria_targets.csv)

driver/ (pure-C, SystemC-free):
  libsauria_cfg.h  libsauria_mem.h  sauria_run.h

Testbenches:
  tb_unified_smoke.cpp profile-routing proof     tb_evaluate.cpp drive + verify
  tb_demo.cpp          auto-generated case-driven demo

Tools & build:
  tools/               gen_targets.py, dump_cfg.py, dump_mem.py, test_*.cpp, SERVER_SETUP.md, ...
  Makefile             see `make help`
  run_shape.sh gen_stim.py   one-shot generate a shape (SAURIA Python)
  .gitattributes .gitignore  repo hygiene (LF line endings; ignore build artifacts)
  npu_demo_clean/      demo pack (see §8)
```

## 10. Debug output

All module trace `std::cout` is routed through `DBG_COUT`. With `SAURIA_DEBUG` unset/0 it compiles
to a null sink (no output, optimized away). Testbench report output uses plain `std::cout` and is
never gated. Enable traces with `make <target> DEBUG=1`.

## 11. Further reading

- `npu_demo_clean/docs/CONFIG_GUIDE.md` — configuration parameters (meaning, impact, flow),
  datatype selection, and how to read the performance block, for the software team.
- `tools/SERVER_SETUP.md` — running on a server: bundled demos (no SAURIA Python) vs. regenerating
  test cases (SAURIA Python + venv), and the line-ending / path-portability notes.
