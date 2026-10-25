#!/bin/bash
# RTL oracle of the vector-unit configuration registers (gvu_config_regs.sv) on ModelSim (ASE 18.1 or any with vlog / vsim in PATH or
# in $MODELSIM_BIN): replays the vectors of tb_rtl_ref_gvu_config_regs --dump on the RTL as delivered and on the RTL with the B13 patch
# (one line changed, made here by sed), plus the two crossed pairs (the model must NOT match the other variant: shows the oracle
# discriminates). usage: bash run_modelsim_cfgregs.sh <rtl gvu dir> <vector dir> <work dir>
set -u
R=${1:?rtl gvu dir}; V=${2:?vector dir}; W=${3:?work dir}
HERE=$(cd "$(dirname "$0")" && pwd)
M=${MODELSIM_BIN:-}; [ -n "$M" ] && M="$M/"
mkdir -p "$W"
tr -d '\r' < "$R/gvu_config_regs.sv" > "$W/cfg_dlv.sv"
sed 's/cfg_reg\[i_addr\]\[i\*8 +: 8\] <= i_wdata\[i\*8 +: 8\]/cfg_reg[i_addr][i +: 8] <= i_wdata[i +: 8]/' "$W/cfg_dlv.sv" > "$W/cfg_b13.sv"
echo "patched lines: $(diff "$W/cfg_dlv.sv" "$W/cfg_b13.sv" | grep -c '^>')"
for rtl in dlv b13; do
  rm -rf "$W/lib_$rtl"; ${M}vlib "$W/lib_$rtl" > /dev/null
  ${M}vlog -sv -work "$W/lib_$rtl" "$R/gvu_pkg.sv" "$W/cfg_$rtl.sv" "$HERE/tb_oracle_cfgregs.sv" > "$W/vlog_$rtl.log" 2>&1 || { echo "BUILD FAIL $rtl"; exit 1; }
done
run() { # rtl vec label
  ${M}vsim -c -lib "$W/lib_$1" tb_oracle_cfgregs -suppress 3009 +dir="$V" +file=cfgregs_$2.txt "+label=$3" -do "run -all; quit -f" 2>&1 | grep -E "ORACLE|FIRST|Error|Fatal" | sed 's/^# //'
}
run dlv dlv "(RTL as delivered, model b13 off)"
run b13 b13 "(RTL with B13 patch, model b13 on)"
run dlv dlvvl "(RTL as delivered, model flat_array_oob = Verilator behaviour of the out-of-range select; expected 0 on Verilator, > 0 on ModelSim)"
echo "-- crossed pairs, expected to differ:"
run b13 dlv "CROSS (RTL with B13 patch, model b13 off)"
run dlv b13 "CROSS (RTL as delivered, model b13 on)"
