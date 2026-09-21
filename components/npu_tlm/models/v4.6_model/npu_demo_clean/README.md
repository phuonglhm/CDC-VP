# NPU SystemC Demo Testbench Pack

Case-driven demo/verification pack for the software team. `tb_demo` runs the same verified logic
as `tb_evaluate.cpp`, but reads every input of a test case from a **case folder** (via `NPU_DEMO_*`
environment variables) instead of hard-coded paths — so new cases are added as data, not code.

Run from the model root (`unified/`).

## Run

```bash
make demo CASE=<name>          # build tb_demo for the case + run + PASS/FAIL summary
make list                      # list all cases
make check                     # run every case + the config round-trip (smoke-CI)
```

`make demo` sources `cases/<name>/case.env` for the build geometry (`EVAL_X/EVAL_Y`), packed-config
index widths and **datatype flags** (`IDX_FLAGS`), and SRAM `REGION_BYTES`, then rebuilds `tb_demo`
accordingly. Logs: `demo_runs_clean/<name>/logs/tb_demo.log`.

## Bundled cases (all PASS)

| Case | Datatype | Shape note |
| ---- | -------- | ---------- |
| `demo_mvm_8x16` | INT8 | 1×1 MVM, Cin=64, 16×8 array |
| `demo_gemm_32x32` / `demo_gemm_64x64` | INT8 | GeMM on 32×32 / 64×64 array |
| `demo_strided_32x32` | INT8 | conv 3×3 stride 2 |
| `demo_multitile_32x32` | INT8 | Cout=64 > Xused → multi-output-tile |
| `demo_fp16_mvm_8x16` / `demo_fp16_gemm_32x32` / `demo_fp16_gemm_64x64` | FP16 | half-precision (verify within ULP tolerance) |
| `demo_int16_mvm_8x16` / `demo_int16_gemm_32x32` | INT16 | int16 in, int64 accumulate |
| `conv5x5_demo` | INT8 | conv 5×5 |

## How a case runs

1. Source `case.env` → build `tb_demo` with the case's geometry + datatype flags.
2. `tb_demo` reads the case inputs (`initial_dram`, `gold_dram`, `GoldenStimuli`, `sauria_tmp/`)
   from the folder pointed to by `NPU_DEMO_*`.
3. It preloads ACT/WEI/PSUM into SRAM, applies the decoded config, asserts `start`, waits `done`,
   reads SRAM-C, adds the preload if `preload_en`, and compares against `gold_dram`
   (exact for INT8/INT16; within ULP tolerance for FP16).

## Capture a new case (needs the SAURIA Python pipeline)

```bash
bash npu_demo_clean/scripts/capture_case.sh <name> "<shape>" EVAL_X EVAL_Y <version> \
     "TITLE" "DESC" "IDX_FLAGS" REGION_BYTES
```

`<shape>` = `Bw Bh d s Cin Cw Ch Cout Xused Yused preload`. `<version>` is one of the entries in
`sauria_targets.csv` (`int8_*`, `FP16_*`, `int16_*`); the matching `IDX_FLAGS` (index widths +
datatype flags) are what you pass so the build decodes the packed config correctly.

## Contents

```
scripts/    build_tb_demo.sh, run_tb_demo_case.sh, capture_case.sh, capture_demo_case_v2.sh, ...
tools/      create_tb_demo_from_tb_evaluate.py   (generates tb_demo.cpp from tb_evaluate.cpp)
cases/      one folder per case: stimuli/, sauria_tmp/, case.env
docs/       CONFIG_GUIDE.md — configuration parameters, datatypes, performance block
```

See the top-level `README.md` (§5 Datatypes, §7 Driver library) and `docs/CONFIG_GUIDE.md` for the
configuration model and how to generate config in C without Python.
