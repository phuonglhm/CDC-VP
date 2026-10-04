#!/bin/bash
# Build tools/fe/sysc/tb_fe_core_tile.cpp without touching the shared Makefile.
# Run from the repository root:  bash tools/fe/sysc/build_tb_fe_core_tile.sh
set -eu
SYSTEMC_HOME=${SYSTEMC_HOME:-/usr}
OUT=${OUT:-fe_work/core/tb_fe_core_tile}
mkdir -p "$(dirname "$OUT")"
g++ -std=c++17 -O3 -DNDEBUG -DSAURIA_DEBUG=0 -DSAURIA_CORE_BACKEND_RTL_REF -DFX1_NO_PERF -I. -I"$SYSTEMC_HOME/include" \
    tools/fe/sysc/tb_fe_core_tile.cpp \
    -L"$SYSTEMC_HOME/lib" -L/usr/lib/x86_64-linux-gnu -lsystemc -lm -pthread -o "$OUT"
echo "built $OUT"
