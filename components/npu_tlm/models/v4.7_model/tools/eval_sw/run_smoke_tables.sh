#!/bin/bash
# [S2-ME] Bridge the two rollups: REAL-RUN OUTPUT (metrics_tiles.csv, MODE A through the core port) -> agg_bigrun (layer/network table, FSM) + sw_eval (9 CSV + parameters.json, A+B).
#   bash tools/eval_sw/run_smoke_tables.sh <metrics_tiles.csv> [OUT_DIR]      (Windows Git-Bash or Linux; needs g++ and python3)
# Tiles not measured in this smoke run are filled from the shape table (MODE B); coverage.csv states each layer's A / A+B / B; parameters.json records the measured fraction.
# SAFETY GATE: busy = 0 on every tile (FSM histogram disabled) -> both scripts refuse (exit code 2) instead of producing an empty table.
set -e
CSV=${1:?metrics_tiles.csv}
R=$(cd "$(dirname "$0")/../.." && pwd); E=$R/tools/eval_sw; O=${2:-$E/out/smoke}
PY=""
for c in python3 python /c/Python313/python; do
  if "$c" -c "import sys; sys.exit(0 if sys.version_info[0]==3 else 1)" >/dev/null 2>&1; then PY=$c; break; fi
done
[ -n "$PY" ] || { echo "python3 not found"; exit 1; }
cd "$R"
g++ -std=c++11 -O2 -I. "$E/sw_eval.cpp" -o "$E/sw_eval.bin"
[ -f "$O/prep/shape_table.csv" ] || "$PY" "$E/sw_eval_prep.py" --program "$E/data/program.json" --jobs-dir "$E/data/jobs" --out-dir "$O/prep" > /dev/null
mkdir -p "$O/agg"
"$PY" tools/metrics/agg_bigrun.py --csv "$CSV" --program "$E/data/program.json" --shapes tools/metrics/out/per_shape.csv --fill --out-dir "$O/agg" > "$O/agg/bigrun_report.md" || echo "[agg_bigrun] exit code $? (see report)"
"$E/sw_eval.bin" --measured "$CSV" --shapes "$O/prep/shape_table.csv" --tiles "$O/prep/layer_tiles.csv" --layers "$O/prep/layer_table.csv" \
    --schema "$R/docs/sauria_model_reference/tools/eval_schema.csv" --out "$O/nine_csv"
"$PY" "$E/eval_selfcheck.py" --dir "$O/nine_csv" --allow-no-shapes | tail -3
"$PY" "$E/render_table.py" "$O/nine_csv"
"$PY" "$E/smoke_table.py" --csv "$CSV" --nine "$O/nine_csv" --out "$O/smoke_table.md" --prep "$O/prep"
"$PY" "$E/core_eval_summary.py" --nine "$O/nine_csv" --csv "$CSV" --report "$O/smoke_table.md" || true
# cross-check per layer: busy/exec measured (agg_bigrun) must equal sw_eval's totals on the same tiles
echo "[TABLE FOR THE USER] $O/smoke_table.md"
echo "[coverage] $O/nine_csv/coverage.csv ; table: $O/nine_csv/network_table.md ; rollup report: $O/agg/bigrun_report.md"
