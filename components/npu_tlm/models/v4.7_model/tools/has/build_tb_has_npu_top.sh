#!/bin/bash
# Build the whole-network testbench tools/has/tb_has_npu_top.cpp: RTL-ref core, perf counters on, per-tile metrics,
# SRAM backdoor. Run from the repository root:  bash tools/has/build_tb_has_npu_top.sh   [OUT=<binary> to rename]
# EXTRA_CXXFLAGS adds build options, e.g. EXTRA_CXXFLAGS=-DGVU_GEMM_STREAM_DEPTH=9 for the epilogue depth of the specification.
set -eu
SYSTEMC_HOME=${SYSTEMC_HOME:-/usr}
OUT=${OUT:-tools/has/tb_has_npu_top}
nice -n 19 g++ -std=c++17 -O3 -DNDEBUG -DSAURIA_DEBUG=0 -DSAURIA_CORE_BACKEND_RTL_REF -DFE_METRICS -DFX1_A3_SRAM_BACKDOOR_LOAD ${EXTRA_CXXFLAGS:-} \
    -I. -I"$SYSTEMC_HOME/include" tools/has/tb_has_npu_top.cpp \
    -L"$SYSTEMC_HOME/lib" -L/usr/lib/x86_64-linux-gnu -lsystemc -lm -pthread -o "$OUT"
echo "built $OUT"
