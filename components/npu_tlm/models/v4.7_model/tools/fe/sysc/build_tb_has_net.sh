#!/bin/bash
# Build tools/fe/sysc/tb_has_net.cpp with the same flags as build_tb_fe_core_net.sh, without touching the Makefile.
# Run from the repository root:  bash tools/fe/sysc/build_tb_has_net.sh
set -eu
SYSTEMC_HOME=${SYSTEMC_HOME:-/usr}
OUT=${OUT:-fe_work/has/tb_has_net}
EXTRA_DEFS=${EXTRA_DEFS:-}
PERF_DEFS=${PERF_DEFS--DFX1_NO_PERF}
mkdir -p "$(dirname "$OUT")"
nice -n 19 g++ -std=c++17 -O3 -DNDEBUG -DSAURIA_DEBUG=0 -DSAURIA_CORE_BACKEND_RTL_REF $PERF_DEFS \
    $EXTRA_DEFS -I. -I"$SYSTEMC_HOME/include" \
    tools/fe/sysc/tb_has_net.cpp \
    -L"$SYSTEMC_HOME/lib" -L/usr/lib/x86_64-linux-gnu -lsystemc -lm -pthread -o "$OUT"
echo "built $OUT"
