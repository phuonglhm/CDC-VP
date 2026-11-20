#!/bin/bash
# RTL oracle of the register file (vrf.sv): builds tb_oracle_g2.sv against the RTL with Verilator 5 and replays the file
# written by tb_rtl_ref_gvu_g2dump. The LUT unit, the Scratchpad and the other staged blocks: run_verilator_b.sh.
# usage: bash run_verilator_g2.sh <rtl gvu dir> <vector dir> <work dir>
set -u
R=${1:?rtl gvu dir}; V=${2:?vector dir}; W=${3:?work dir}
HERE=$(cd "$(dirname "$0")" && pwd)
command -v verilator > /dev/null || { echo "verilator not found"; exit 2; }
mkdir -p "$W"
SRC="$R/gvu_pkg.sv $R/stream_switch/vrf.sv"
rc=0
for top in tb_oracle_vrf; do
  nice -n 19 verilator --binary --timing -sv -Wno-fatal --top-module $top --Mdir "$W/$top" $SRC "$HERE/tb_oracle_g2.sv" > "$W/$top.build.log" 2>&1 \
    || { echo "BUILD FAIL $top (see $W/$top.build.log)"; rc=1; continue; }
  "$W/$top/V$top" +dir="$V" 2>&1 | tee "$W/$top.log" | grep -E "ORACLE|DETAIL|FIRST|Error|FATAL"
  [ "${ORACLE_CLEAN:-0}" = 1 ] && rm -rf "$W/$top"
done
grep -h "ORACLE" "$W"/tb_oracle_vrf.log 2>/dev/null | grep -v "mismatches 0" && rc=1
exit $rc
