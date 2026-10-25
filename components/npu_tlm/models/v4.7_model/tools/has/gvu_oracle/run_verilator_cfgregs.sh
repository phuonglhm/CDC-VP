#!/bin/bash
# Same oracle as run_modelsim_cfgregs.sh on Verilator 5 (--binary --timing): gvu_config_regs.sv as delivered and with the B13 patch
# against the vectors of tb_rtl_ref_gvu_config_regs --dump, plus the two crossed pairs (expected to differ).
# usage: bash run_verilator_cfgregs.sh <rtl gvu dir> <vector dir> <work dir>
set -u
R=${1:?rtl gvu dir}; V=${2:?vector dir}; W=${3:?work dir}
HERE=$(cd "$(dirname "$0")" && pwd)
command -v verilator > /dev/null || { echo "verilator not found (version 5 or later)"; exit 2; }
mkdir -p "$W"
tr -d '\r' < "$R/gvu_config_regs.sv" > "$W/cfg_dlv.sv"
sed 's/cfg_reg\[i_addr\]\[i\*8 +: 8\] <= i_wdata\[i\*8 +: 8\]/cfg_reg[i_addr][i +: 8] <= i_wdata[i +: 8]/' "$W/cfg_dlv.sv" > "$W/cfg_b13.sv"
echo "patched lines: $(diff "$W/cfg_dlv.sv" "$W/cfg_b13.sv" | grep -c '^>')"
for rtl in dlv b13; do
  [ "${REUSE:-0}" = 1 ] && [ -x "$W/obj_$rtl/Vtb_oracle_cfgregs" ] && continue   # REUSE=1: keep the builds of an earlier run in the same work dir
  nice -n 19 verilator --binary --timing -sv -Wno-fatal -Wno-ENUMVALUE -Wno-WIDTH -Wno-lint -Wno-style --top-module tb_oracle_cfgregs --Mdir "$W/obj_$rtl" \
    "$R/gvu_pkg.sv" "$W/cfg_$rtl.sv" "$HERE/tb_oracle_cfgregs.sv" > "$W/build_$rtl.log" 2>&1 || { echo "BUILD FAIL $rtl (see $W/build_$rtl.log)"; exit 1; }
done
run() { "$W/obj_$1/Vtb_oracle_cfgregs" +dir="$V" +file=cfgregs_$2.txt "+label=$3" 2>&1 | grep -E "ORACLE|FIRST|Error|FATAL"; }
run dlv dlv "(RTL as delivered, model b13 off)"
run b13 b13 "(RTL with B13 patch, model b13 on)"
run dlv dlvvl "(RTL as delivered, model flat_array_oob = Verilator behaviour of the out-of-range select; expected 0 on Verilator, > 0 on ModelSim)"
echo "-- crossed pairs, expected to differ:"
run b13 dlv "CROSS (RTL with B13 patch, model b13 off)"
run dlv b13 "CROSS (RTL as delivered, model b13 on)"
