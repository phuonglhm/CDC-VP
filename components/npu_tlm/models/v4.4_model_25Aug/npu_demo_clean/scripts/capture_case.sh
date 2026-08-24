#!/usr/bin/env bash
# One-command capture of a COMPLETE demo case at a chosen geometry, with metadata.
# It (1) generates + verifies a SAURIA shape via run_shape.sh (populates stimuli/ and
# /tmp/sauria_*), (2) copies those into the case folder, (3) writes case.env so
# `make demo CASE=<name>` rebuilds tb_demo with the right geometry + a test title.
#
# Usage:
#   bash npu_demo_clean/scripts/capture_case.sh \
#        <case_name> "<SHAPE>" <EVAL_X> <EVAL_Y> <VERSION> "<TITLE>" "<DESC>" [IDX_FLAGS] [REGION_BYTES]
#
# SHAPE = "Bw Bh d s Cin Cw Ch Cout Xused Yused preload"  (as for run_shape.sh)
set -euo pipefail

CASE="${1:?case_name}"; SHAPE="${2:?shape}"; EX="${3:?eval_x}"; EY="${4:?eval_y}"
VER="${5:?version}"; TITLE="${6:?title}"; DESC="${7:?desc}"
IDXF="${8:-}"; RB="${9:-65536}"
ROOT="$(pwd)"

echo "[capture_case] 1/3 generate + verify shape via run_shape.sh ($VER): $SHAPE"
bash "$HOME/run_shape.sh" "$SHAPE" "$EX" "$EY" "$VER" "$RB"

echo "[capture_case] 2/3 copy stimuli + sauria_tmp into case folder"
bash npu_demo_clean/scripts/capture_demo_case_v2.sh "$CASE" >/dev/null

CASE_DIR="$ROOT/npu_demo_clean/cases/$CASE"
cat > "$CASE_DIR/case.env" <<ENV
# Auto-written by capture_case.sh — build/run metadata for this demo case.
TITLE="$TITLE"
DESC="$DESC"
EVAL_X=$EX
EVAL_Y=$EY
IDX_FLAGS="$IDXF"
VERSION="$VER"
SHAPE="$SHAPE"
REGION_BYTES=$RB
ENV
echo "[capture_case] 3/3 wrote $CASE_DIR/case.env"
echo "[capture_case] DONE -> run:  make demo CASE=$CASE"
