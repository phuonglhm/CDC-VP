#!/usr/bin/env bash
set -euo pipefail

CASE_NAME="${1:-}"
if [ -z "$CASE_NAME" ]; then
    echo "Usage: bash npu_demo_clean/scripts/capture_demo_case_v2.sh <case_name>"
    exit 1
fi

ROOT_DIR="$(pwd)"
CASE_DIR="$ROOT_DIR/npu_demo_clean/cases/$CASE_NAME"
mkdir -p "$CASE_DIR/stimuli" "$CASE_DIR/sauria_tmp" "$CASE_DIR/config"

need_file() {
    if [ ! -f "$1" ]; then
        echo "[ERROR] Missing required file: $1"
        exit 1
    fi
}

need_file "$ROOT_DIR/stimuli/initial_dram.txt"
need_file "$ROOT_DIR/stimuli/gold_dram.txt"
need_file "$ROOT_DIR/stimuli/GoldenStimuli.txt"
need_file "/tmp/sauria_A_Mat_mvm_shape.txt"
need_file "/tmp/sauria_B_Mat_mvm_shape.txt"
need_file "/tmp/sauria_C_compute_mvm_shape.txt"

cp "$ROOT_DIR/stimuli/initial_dram.txt" "$CASE_DIR/stimuli/initial_dram.txt"
cp "$ROOT_DIR/stimuli/gold_dram.txt" "$CASE_DIR/stimuli/gold_dram.txt"
cp "$ROOT_DIR/stimuli/GoldenStimuli.txt" "$CASE_DIR/stimuli/GoldenStimuli.txt"
cp /tmp/sauria_* "$CASE_DIR/sauria_tmp/" 2>/dev/null || true

A_SHAPE="$(tr '\n' ' ' < /tmp/sauria_A_Mat_mvm_shape.txt | sed 's/[[:space:]]*$//')"
B_SHAPE="$(tr '\n' ' ' < /tmp/sauria_B_Mat_mvm_shape.txt | sed 's/[[:space:]]*$//')"
C_SHAPE="$(tr '\n' ' ' < /tmp/sauria_C_compute_mvm_shape.txt | sed 's/[[:space:]]*$//')"

cat > "$CASE_DIR/config/demo_manifest.json" <<JSON
{
  "case_name": "$CASE_NAME",
  "kind": "sauria_captured_case",
  "description": "Captured from current SAURIA generated data and SystemC stimuli.",
  "compile_time": {
    "EVAL_X": 16,
    "EVAL_Y": 8,
    "A_REGION_BYTES": 8192,
    "B_REGION_BYTES": 4096,
    "C_REGION_BYTES": 4096
  },
  "runtime_files": {
    "initial_dram": "stimuli/initial_dram.txt",
    "gold_dram": "stimuli/gold_dram.txt",
    "golden_stimuli": "stimuli/GoldenStimuli.txt",
    "sauria_tmp_dir": "sauria_tmp"
  },
  "sauria_shapes_flat": {
    "A_Mat_mvm": "$A_SHAPE",
    "B_Mat_mvm": "$B_SHAPE",
    "C_compute_mvm": "$C_SHAPE"
  }
}
JSON

{
    echo "CASE: $CASE_NAME"
    echo
    echo "A shape:"; cat /tmp/sauria_A_Mat_mvm_shape.txt
    echo "B shape:"; cat /tmp/sauria_B_Mat_mvm_shape.txt
    echo "C shape:"; cat /tmp/sauria_C_compute_mvm_shape.txt
    echo
    echo "stimuli hashes:"
    sha256sum "$CASE_DIR/stimuli/initial_dram.txt" "$CASE_DIR/stimuli/gold_dram.txt" "$CASE_DIR/stimuli/GoldenStimuli.txt"
    echo
    echo "sauria hashes:"
    sha256sum "$CASE_DIR/sauria_tmp"/sauria_* 2>/dev/null || true
} > "$CASE_DIR/CAPTURE_INFO.txt"

cat "$CASE_DIR/CAPTURE_INFO.txt"
echo "[DONE] Captured demo case: $CASE_DIR"
