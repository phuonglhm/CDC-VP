#!/bin/bash
# Build a unit testbench of the has/ blocks. Run from the repository root:
#   bash tools/has/build_tb_has.sh tb_gvu_quant        -> tools/has/tb_gvu_quant
set -eu
NAME=${1:?testbench name, e.g. tb_gvu_quant}
SYSTEMC_HOME=${SYSTEMC_HOME:-/usr}
nice -n 19 g++ -std=c++17 -O2 -Wall -Wno-unused-variable -I. -I"$SYSTEMC_HOME/include" "tools/has/$NAME.cpp" \
    -L"$SYSTEMC_HOME/lib" -L/usr/lib/x86_64-linux-gnu -lsystemc -lm -pthread -o "tools/has/$NAME"
echo "built tools/has/$NAME"
