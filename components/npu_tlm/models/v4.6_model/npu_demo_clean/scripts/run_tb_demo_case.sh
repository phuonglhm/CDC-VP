#!/usr/bin/env bash
# Run one demo case on tb_demo. Reads per-case build/identity metadata from
# npu_demo_clean/cases/<case>/case.env (TITLE, DESC, EVAL_X, EVAL_Y, IDX_FLAGS)
# so a case built for 32x32 / 64x64 rebuilds tb_demo with the right geometry.
set -euo pipefail

CASE_NAME="${1:-}"
BUILD_MODE="${2:-}"
if [ -z "$CASE_NAME" ]; then
    echo "Usage: bash npu_demo_clean/scripts/run_tb_demo_case.sh <case_name> [build]"
    exit 1
fi

ROOT_DIR="$(pwd)"
CASE_DIR="$ROOT_DIR/npu_demo_clean/cases/$CASE_NAME"
RUN_DIR="$ROOT_DIR/demo_runs_clean/$CASE_NAME"
LOG_DIR="$RUN_DIR/logs"
TRACE_DIR="$RUN_DIR/traces"
mkdir -p "$LOG_DIR" "$TRACE_DIR" trace_sysc

for f in \
    "$CASE_DIR/stimuli/initial_dram.txt" \
    "$CASE_DIR/stimuli/gold_dram.txt" \
    "$CASE_DIR/stimuli/GoldenStimuli.txt" \
    "$CASE_DIR/sauria_tmp/sauria_A_Mat_mvm_shape.txt" \
    "$CASE_DIR/sauria_tmp/sauria_B_Mat_mvm_shape.txt" \
    "$CASE_DIR/sauria_tmp/sauria_C_compute_mvm_shape.txt"; do
    if [ ! -f "$f" ]; then
        echo "[ERROR] Missing case file: $f"
        exit 1
    fi
done

# Per-case metadata (title / description / build geometry). Optional.
TITLE=""; DESC=""
if [ -f "$CASE_DIR/case.env" ]; then
    # shellcheck disable=SC1090
    source "$CASE_DIR/case.env"
fi
export NPU_DEMO_TITLE="${TITLE:-$CASE_NAME}"
export NPU_DEMO_DESC="${DESC:-}"
export EVAL_X="${EVAL_X:-16}"
export EVAL_Y="${EVAL_Y:-8}"
export IDX_FLAGS="${IDX_FLAGS:-}"
if [ -n "${REGION_BYTES:-}" ]; then
    export A_REGION_BYTES="$REGION_BYTES" B_REGION_BYTES="$REGION_BYTES" C_REGION_BYTES="$REGION_BYTES"
fi

if [ "$BUILD_MODE" = "build" ]; then
    DEBUG="${DEBUG:-0}" EVAL_X="$EVAL_X" EVAL_Y="$EVAL_Y" IDX_FLAGS="$IDX_FLAGS" \
        bash npu_demo_clean/scripts/build_tb_demo.sh
fi

if [ ! -x ./tb_demo ]; then
    echo "[ERROR] Missing executable ./tb_demo. Run with second argument 'build'."
    exit 1
fi

rm -f trace_sysc/*.csv
rm -f "$TRACE_DIR"/*.csv

export NPU_DEMO_CASE_DIR="$CASE_DIR"
export NPU_DEMO_INITIAL_DRAM="$CASE_DIR/stimuli/initial_dram.txt"
export NPU_DEMO_GOLD_DRAM="$CASE_DIR/stimuli/gold_dram.txt"
export NPU_DEMO_GOLDEN_STIMULI="$CASE_DIR/stimuli/GoldenStimuli.txt"
export NPU_DEMO_SAURIA_DIR="$CASE_DIR/sauria_tmp"

echo "[RUN] tb_demo case=$CASE_NAME  (EVAL_X=$EVAL_X EVAL_Y=$EVAL_Y)"
./tb_demo > "$LOG_DIR/tb_demo.log" 2>&1 || true

cp trace_sysc/*.csv "$TRACE_DIR/" 2>/dev/null || true

# Compact summary: title, config, a few match rows, perf, result.
python3 - "$CASE_NAME" "$LOG_DIR/tb_demo.log" "$CASE_DIR" > "$LOG_DIR/summary.txt" <<'PY'
import re, sys
from pathlib import Path
case, log_path, case_dir = sys.argv[1], Path(sys.argv[2]), Path(sys.argv[3])
text = log_path.read_text(errors="ignore") if log_path.exists() else ""

def shape(name):
    p = case_dir / "sauria_tmp" / name
    return " ".join(p.read_text().split()) if p.exists() else "MISSING"

print(f"case: {case}")
print("A_shape:", shape("sauria_A_Mat_mvm_shape.txt"))
print("B_shape:", shape("sauria_B_Mat_mvm_shape.txt"))
print("C_shape:", shape("sauria_C_compute_mvm_shape.txt"))
print()
for pat in [r"# TEST : .*", r"# WHAT : .*",
            r"\[RUNTIME CONFIG\]", r"  ncontexts = .*", r"  cxlim     = .*",
            r"  cklim     = .*", r"  act_reps  = .*", r"  wei_reps  = .*",
            r"Execution cycles.*", r"Contraction depth.*", r"Total MACs.*",
            r"Throughput.*", r"Array utilization.*", r"Output throughput.*",
            r"Expected elements\s*:\s*\d+", r"Mismatches\s*:\s*\d+", r"\[RESULT\].*"]:
    for m in re.finditer(pat, text):
        print(m.group(0))
print()
if "[RESULT] TEST PASSED" in text:
    print("status: PASS")
elif "[RESULT] TEST FAILED" in text:
    print("status: FAIL")
else:
    print("status: UNKNOWN")
PY

cat "$LOG_DIR/summary.txt"
echo "[DONE] full log=$LOG_DIR/tb_demo.log"
