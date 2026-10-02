#!/bin/bash
# Build tools/fe/sysc/tb_fe_core_net.cpp without touching the shared Makefile.
# Run from the repository root:  bash tools/fe/sysc/build_tb_fe_core_net.sh
set -eu
SYSTEMC_HOME=${SYSTEMC_HOME:-/usr}
OUT=${OUT:-fe_work/step6b/tb_fe_core_net}
# EXTRA_DEFS="-DFX1_A3_SRAM_BACKDOOR_LOAD" enables the SRAM backdoor load for the npu_top backend (it also
# needs FE_SRAM_BACKDOOR set at run time). -O3 -DNDEBUG -DSAURIA_DEBUG=0: ~31 % faster, identical results.
EXTRA_DEFS=${EXTRA_DEFS:-}
# PERF_DEFS: counters off by default (-DFX1_NO_PERF, no behaviour change). Metrics build:
# PERF_DEFS="" EXTRA_DEFS="-DFE_METRICS ..." (patch instrumentation/tb_fe_core_net_metrics.patch).
PERF_DEFS=${PERF_DEFS--DFX1_NO_PERF}
mkdir -p "$(dirname "$OUT")"
g++ -std=c++17 -O3 -DNDEBUG -DSAURIA_DEBUG=0 -DSAURIA_CORE_BACKEND_RTL_REF $PERF_DEFS \
    $EXTRA_DEFS -I. -I"$SYSTEMC_HOME/include" \
    tools/fe/sysc/tb_fe_core_net.cpp \
    -L"$SYSTEMC_HOME/lib" -L/usr/lib/x86_64-linux-gnu -lsystemc -lm -pthread -o "$OUT"
echo "built $OUT"
