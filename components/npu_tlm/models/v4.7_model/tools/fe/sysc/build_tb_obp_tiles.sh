#!/bin/bash
# Build tools/fe/sysc/tb_obp_tiles.cpp without touching the shared Makefile.
# Run from the repository root:  bash tools/fe/sysc/build_tb_obp_tiles.sh
set -eu
SYSTEMC_HOME=${SYSTEMC_HOME:-/usr}
OUT=${OUT:-fe_work/step6b/tb_obp_tiles}
mkdir -p "$(dirname "$OUT")"
g++ -std=c++17 -O2 -DFX1_NO_PERF -I. -I"$SYSTEMC_HOME/include" tools/fe/sysc/tb_obp_tiles.cpp \
    -L"$SYSTEMC_HOME/lib" -L/usr/lib/x86_64-linux-gnu -lsystemc -lm -pthread -o "$OUT"
echo "built $OUT"
