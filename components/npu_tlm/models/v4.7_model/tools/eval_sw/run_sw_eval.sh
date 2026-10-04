#!/bin/bash
# Standalone SW-EVAL (MODE B): shape table + program -> 9 CSV + parameters.json. NO SystemC, NO testbench, NO simulation.
# Run from the v4.5_model repo root (Windows Git-Bash or Linux). Needs: g++ (C++11+) and python3.
#   bash tools/eval_sw/run_sw_eval.sh [OUT_DIR]      (default tools/eval_sw/out)
set -e
R=$(cd "$(dirname "$0")/../.." && pwd); E=$R/tools/eval_sw; O=${1:-$E/out}
PY=""
for c in python3 python /c/Python313/python; do   # skip the Windows Store stub (present but non-functional)
  if "$c" -c "import sys; sys.exit(0 if sys.version_info[0]==3 else 1)" >/dev/null 2>&1; then PY=$c; break; fi
done
[ -n "$PY" ] || { echo "python3 not found"; exit 1; }
SCHEMA=$R/docs/sauria_model_reference/tools/eval_schema.csv
cd "$R"
g++ -std=c++11 -O2 -I. "$E/sw_eval.cpp" -o "$E/sw_eval.bin"
"$PY" "$E/sw_eval_prep.py" --program "$E/data/program.json" --jobs-dir "$E/data/jobs" --out-dir "$O/prep" --compat
"$E/sw_eval.bin" --shapes "$O/prep/shape_table.csv" --tiles "$O/prep/layer_tiles.csv" --layers "$O/prep/layer_table.csv" --schema "$SCHEMA" --out "$O/modeb"
"$E/sw_eval.bin" --compat --shapes "$O/prep/shape_table.csv" --tiles "$O/prep/compat_tiles.csv" --schema "$SCHEMA" --out "$O/compat"
"$PY" "$E/compat_check.py" --out "$O/compat" --jobs-dir "$E/data/jobs"          # must report 0 MISMATCHES (reproduces sauria_model cell-for-cell)
"$PY" "$E/eval_selfcheck.py" --dir "$O/modeb" --allow-no-shapes | tail -3      # identities + bounds, incl. A+B+C == l2_size
"$PY" "$E/render_table.py" "$O/modeb"
"$PY" "$E/core_eval_summary.py" --nine "$O/modeb" --report "$O/modeb/network_table.md" || true
