#!/usr/bin/env bash
# Build tb_demo (case-driven demo testbench) inside the unified NPU model.
# Works with a system-wide SystemC (headers in /usr/include, -lsystemc) OR a
# source install via SYSTEMC_HOME (expects $SYSTEMC_HOME/include and lib-linux64).
set -euo pipefail

EVAL_X="${EVAL_X:-16}"
EVAL_Y="${EVAL_Y:-8}"
A_REGION_BYTES="${A_REGION_BYTES:-8192}"
B_REGION_BYTES="${B_REGION_BYTES:-4096}"
C_REGION_BYTES="${C_REGION_BYTES:-4096}"
# Optional packed-config index widths (needed for geometry != 16x8), e.g.
#   IDX_FLAGS="-DSAURIA_ACT_IDX_W=17 -DSAURIA_WEI_IDX_W=17 -DSAURIA_OUT_IDX_W=16"
IDX_FLAGS="${IDX_FLAGS:-}"

if [ ! -f tb_demo.cpp ]; then
    echo "[INFO] tb_demo.cpp not found. Generating from tb_evaluate.cpp"
    python3 npu_demo_clean/tools/create_tb_demo_from_tb_evaluate.py
fi

# SystemC: prefer a source install via SYSTEMC_HOME, else fall back to the
# system-wide package (headers in /usr/include, lib via -lsystemc).
SYSTEMC_HOME="${SYSTEMC_HOME:-}"
INC_FLAGS=""
LIB_FLAGS="-lsystemc -lm -pthread"
if [ -n "$SYSTEMC_HOME" ] && [ -d "$SYSTEMC_HOME/include" ]; then
    INC_FLAGS="-I$SYSTEMC_HOME/include"
    if [ -d "$SYSTEMC_HOME/lib-linux64" ]; then
        LIB_FLAGS="-L$SYSTEMC_HOME/lib-linux64 -Wl,-rpath,$SYSTEMC_HOME/lib-linux64 -lsystemc -lm -pthread"
    elif [ -d "$SYSTEMC_HOME/lib64" ]; then
        LIB_FLAGS="-L$SYSTEMC_HOME/lib64 -Wl,-rpath,$SYSTEMC_HOME/lib64 -lsystemc -lm -pthread"
    elif [ -d "$SYSTEMC_HOME/lib" ]; then
        LIB_FLAGS="-L$SYSTEMC_HOME/lib -Wl,-rpath,$SYSTEMC_HOME/lib -lsystemc -lm -pthread"
    fi
fi

DEBUG="${DEBUG:-0}"
DBG_FLAGS=""
if [ "$DEBUG" = "1" ]; then
    DBG_FLAGS="-DSAURIA_DEBUG=1"
fi

echo "[BUILD] tb_demo  EVAL_X=$EVAL_X EVAL_Y=$EVAL_Y IDX_FLAGS='$IDX_FLAGS' DEBUG=$DEBUG"
g++ -O3 -Wall -std=c++17 \
    -DEVAL_X="$EVAL_X" -DEVAL_Y="$EVAL_Y" \
    -DA_REGION_BYTES="$A_REGION_BYTES" \
    -DB_REGION_BYTES="$B_REGION_BYTES" \
    -DC_REGION_BYTES="$C_REGION_BYTES" \
    $IDX_FLAGS \
    $DBG_FLAGS \
    -I. $INC_FLAGS \
    tb_demo.cpp \
    $LIB_FLAGS \
    -o tb_demo

echo "[SUCCESS] Built tb_demo"
