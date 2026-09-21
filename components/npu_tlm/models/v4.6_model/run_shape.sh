#!/usr/bin/env bash
# Approach C runner: express a workload as a SAURIA conv/GeMM shape, generate stimuli via
# the BSC pipeline, build tb_evaluate at the matching geometry, and run it on the v1 core.
#
# Usage:
#   ./run_shape.sh "Bw Bh d s Cin Cw Ch Cout Xused Yused preload" EVAL_X EVAL_Y VERSION [REGION_BYTES]
#
# Examples:
#   ./run_shape.sh "1 1 1 1 64 8 1 16 16 8 1"  16 8  int8_8x16
#   ./run_shape.sh "1 1 1 1 64 32 1 32 32 32 1" 32 32 int8_32x32
#   ./run_shape.sh "3 3 1 2 16 8 4 32 32 8 1"   32 32 int8_32x32   # strided
#   ./run_shape.sh "3 3 1 1 16 8 4 64 32 8 1"   32 32 int8_32x32   # multi-tile
set -e

SHAPE="$1"; EVAL_X="${2:-16}"; EVAL_Y="${3:-8}"; VERSION="${4:-int8_8x16}"; RB="${5:-16384}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UNI="${UNI:-$HERE}"
GENPY="${GENPY:-$UNI/gen_stim.py}"
OUT="$HOME/gen_run_$$"

# Per-HW packed-config widths (from hw_versions IFM_IDX_W/WEI_IDX_W/PSM_IDX_W).
case "$VERSION" in
  int8_8x16)  W="-DSAURIA_ACT_IDX_W=15 -DSAURIA_WEI_IDX_W=16 -DSAURIA_OUT_IDX_W=14" ;;
  int16_8x16) W="-DSAURIA_ACT_IDX_W=15 -DSAURIA_WEI_IDX_W=16 -DSAURIA_OUT_IDX_W=14" ;;
  int16_32x32) W="-DSAURIA_ACT_IDX_W=17 -DSAURIA_WEI_IDX_W=17 -DSAURIA_OUT_IDX_W=16" ;;
  int8_32x32) W="-DSAURIA_ACT_IDX_W=17 -DSAURIA_WEI_IDX_W=17 -DSAURIA_OUT_IDX_W=16" ;;
  int8_64x64) W="-DSAURIA_ACT_IDX_W=18 -DSAURIA_WEI_IDX_W=18 -DSAURIA_OUT_IDX_W=17" ;;
  FP16_8x16)  W="-DSAURIA_ACT_IDX_W=15 -DSAURIA_WEI_IDX_W=15 -DSAURIA_OUT_IDX_W=15" ;;
  FP16_32x32) W="-DSAURIA_ACT_IDX_W=17 -DSAURIA_WEI_IDX_W=17 -DSAURIA_OUT_IDX_W=16" ;;
  FP16_64x64) W="-DSAURIA_ACT_IDX_W=18 -DSAURIA_WEI_IDX_W=18 -DSAURIA_OUT_IDX_W=17" ;;
  *) echo "Unknown VERSION $VERSION (add its IDX widths here)"; exit 1 ;;
esac

echo "[run_shape] gen ($VERSION): $SHAPE"
SAURIA_VERSION="$VERSION" python3 "$GENPY" $SHAPE "$OUT" >/tmp/gen.log 2>&1 || { tail -20 /tmp/gen.log; exit 1; }

cd "$UNI"
cp "$OUT"/stimuli/* stimuli/
BIN="tb_eval_${EVAL_X}x${EVAL_Y}"
echo "[run_shape] build $BIN (${EVAL_X}x${EVAL_Y})"

DTYPE_FLAGS=""
case "$VERSION" in
  FP16_*) DTYPE_FLAGS="-DNPU_FP16 -DNPU_DTYPE_IN=fp16_t -DNPU_DTYPE_OUT=fp16_t -DNPU_DTYPE_PSUM=float" ;;
  int16_*) DTYPE_FLAGS="-DNPU_INT16 -DNPU_DTYPE_IN=int16_t -DNPU_DTYPE_OUT=int64_t -DNPU_DTYPE_PSUM=int64_t" ;;
esac
g++ -std=c++17 -O2 -DEVAL_X=$EVAL_X -DEVAL_Y=$EVAL_Y \
    -DA_REGION_BYTES=$RB -DB_REGION_BYTES=$RB -DC_REGION_BYTES=$RB \
    $W $DTYPE_FLAGS -I. tb_evaluate.cpp -lsystemc -lm -pthread -o "$BIN"
mkdir -p trace_sysc
echo "[run_shape] run"
timeout 180 ./"$BIN" 2>/dev/null | grep -aE "total_c_elements|output_tiles|ncontexts=|RESULT|Mismatches" | tail -5
rm -rf "$OUT"
