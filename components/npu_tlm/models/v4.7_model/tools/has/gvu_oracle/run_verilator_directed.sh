#!/bin/bash
# Directed RTL checks (tb_oracle_directed.sv) with Verilator 5, and an elaboration attempt of stream_vrf.sv.
# usage: bash run_verilator_directed.sh <rtl gvu dir> <work dir>
set -u
R=${1:?rtl gvu dir}; W=${2:?work dir}
HERE=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$W"
S=$R/stream_switch
for top in tb_dir_vstream tb_dir_cmp; do
  nice -n 19 verilator --binary --timing -sv -Wno-fatal --top-module $top --Mdir "$W/$top" $R/gvu_pkg.sv $R/pipeline_reg.sv $S/vstream_reg_int32.sv $R/FUs/gvu_vcmp32.sv \
    "$HERE/tb_oracle_directed.sv" > "$W/$top.build.log" 2>&1 || { echo "BUILD FAIL $top"; grep -E "^%Error" -A4 "$W/$top.build.log" | head -20; continue; }
  grep -E "^%Warning-(UNOPTFLAT|LATCH|CASEINCOMPLETE)" "$W/$top.build.log" | cut -c1-230 | head -6
  timeout 60 "$W/$top/V$top" 2>&1 | tee "$W/$top.log" | grep -E "DIRECTED|Error|converge" | cut -c1-240
  [ "${ORACLE_CLEAN:-0}" = 1 ] && rm -rf "$W/$top"
done
echo "== lint of stream_vrf.sv as delivered"
verilator --lint-only -sv -Wno-fatal --top-module stream_vrf $R/gvu_pkg.sv $R/pipeline_reg.sv $S/vstream_reg_int32.sv $S/vstream_reg_int64_type0.sv $S/vstream_reg_int64_type1.sv $S/stream_vrf.sv > "$W/stream_vrf.lint.log" 2>&1
echo "lint rc=$?"; grep -E "^%(Error|Warning)" "$W/stream_vrf.lint.log" | sed 's/: \.\.\..*//' | cut -c1-230 | sort | uniq -c | sort -rn | head -14
