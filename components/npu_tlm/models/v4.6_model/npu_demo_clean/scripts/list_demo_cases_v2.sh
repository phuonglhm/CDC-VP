#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(pwd)"
CASE_ROOT="$ROOT_DIR/npu_demo_clean/cases"
if [ ! -d "$CASE_ROOT" ]; then
    echo "No cases yet: $CASE_ROOT"
    exit 0
fi
for c in "$CASE_ROOT"/*; do
    [ -d "$c" ] || continue
    echo
    echo "=================================================="
    echo "CASE: $(basename "$c")"
    echo "=================================================="
    echo "A shape:"; cat "$c/sauria_tmp/sauria_A_Mat_mvm_shape.txt" 2>/dev/null || echo MISSING
    echo "B shape:"; cat "$c/sauria_tmp/sauria_B_Mat_mvm_shape.txt" 2>/dev/null || echo MISSING
    echo "C shape:"; cat "$c/sauria_tmp/sauria_C_compute_mvm_shape.txt" 2>/dev/null || echo MISSING
    echo "Hashes:"
    sha256sum "$c/stimuli/initial_dram.txt" "$c/stimuli/gold_dram.txt" "$c/stimuli/GoldenStimuli.txt" 2>/dev/null || true
done
