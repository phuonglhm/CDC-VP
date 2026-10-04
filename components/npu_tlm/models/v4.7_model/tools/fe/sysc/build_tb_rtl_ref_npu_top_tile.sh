#!/bin/bash
# Build tools/fe/sysc/tb_rtl_ref_npu_top_tile.cpp without touching the shared Makefile.
# Run from the repository root: bash tools/fe/sysc/build_tb_rtl_ref_npu_top_tile.sh
set -eu
SYSTEMC_HOME=${SYSTEMC_HOME:-/usr}
OUT=${OUT:-fe_work/core/tb_rtl_ref_npu_top_tile}
# EXTRA_DEFS: empty by default. EXTRA_DEFS="-DFX1_A3_SRAM_BACKDOOR_LOAD" enables the SRAM backdoor load
# (still off unless FE_SRAM_BACKDOOR is also set at run time -- see tb_rtl_ref_npu_top_tile.cpp).
EXTRA_DEFS=${EXTRA_DEFS:-}
mkdir -p "$(dirname "$OUT")"
g++ -std=c++17 -O3 -DNDEBUG -DSAURIA_DEBUG=0 -DFX1_NO_PERF $EXTRA_DEFS -I. -I"$SYSTEMC_HOME/include" \
    tools/fe/sysc/tb_rtl_ref_npu_top_tile.cpp \
    -L"$SYSTEMC_HOME/lib" -L/usr/lib/x86_64-linux-gnu -lsystemc -lm -pthread -o "$OUT"
echo "built $OUT"
