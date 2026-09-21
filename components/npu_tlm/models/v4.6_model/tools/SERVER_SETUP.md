# Server setup — running the NPU model + testcase generation

Two independent capabilities. Pick what the server needs.

## A. Run captured demos (NO Python SAURIA needed)
Self-contained. Needs only: `g++` (C++17), SystemC, `python3` (for tb_demo codegen).
```
make check          # build+run ALL demo cases + cfg round-trip, prints PASS/FAIL
make demo CASE=demo_fp16_gemm_64x64
```
SystemC is auto-detected (see `make help`): company EDA server `eda-server-01`
(`/opt/arm/fastmodels/.../Accellera`), else `SYSTEMC_HOME=/path`, else system `-lsystemc`.

## B. Generate NEW testcases (needs patched SAURIA Python — offline/CI only)
Heavy deps: **numpy + torch** (torch.nn.Conv2d is the golden reference). Keep this in a
venv on the CI/verify box only — it is NOT on the runtime/driver path.

```
python3 -m venv sauria-env && . sauria-env/bin/activate
pip install -r <sauria>/Python/requirements_pip.txt   # pin numpy+torch versions
```

Re-apply the extra HW versions this project needs (lost on a fresh sauria clone):
```
SAURIA_PY=/path/to/sauria/Python python3 tools/hw_versions_ext.py
```
This inserts FP16_32x32, FP16_64x64, int16_8x16, int16_32x32 (idempotent; `--check` to
audit). Also verify `config_helper.py` packs 64-bit fields (o_rows_active/o_cols_active at
64x64) across >2 registers and that `np.int -> int` is applied.

Then generate + run a fresh shape (portable, uses in-tree run_shape.sh):
```
SAURIA_PY=/path/to/sauria/Python make shape \
    SHAPE="1 1 1 1 64 32 1 32 32 32 0" EVAL_X=32 EVAL_Y=32 VER=FP16_32x32 RB=65536
```

## Push hygiene (Windows -> Linux)
`.gitattributes` forces LF so shell scripts don't break on the server. If a `.sh`
fails with `$'\r': command not found`, line endings were mangled — re-checkout with LF.
