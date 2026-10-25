#!/bin/bash
# Elaborates and runs gvu_top.sv (softmax) with Verilator 5 and prints what it does (tb_top_softmax.sv, lines "TOP ...").
# The modules shift_reg and spill_register_flushable are taken from the two *_standin.sv files. A report, not a check.
# usage: bash run_verilator_top.sh <rtl gvu dir> <work dir> [token lengths, default "8 1"] [vector dir]
set -u
R=${1:?rtl gvu dir}; W=${2:?work dir}; LENS=${3:-"8 1"}
HERE=$(cd "$(dirname "$0")" && pwd)
command -v verilator > /dev/null || { echo "verilator not found (version 5 or later)"; exit 2; }
mkdir -p "$W"
SRC="$R/gvu_pkg.sv $R/pipeline_reg.sv $(ls $R/FUs/*.sv | tr '\n' ' ') $(ls $R/stream_switch/*.sv | tr '\n' ' ') $(ls $R/scratchpad/*.sv | tr '\n' ' ')
     $(ls $R/vlsu/*.sv | tr '\n' ' ') $R/gvu_pipe_controller.sv $R/gvu_top.sv $HERE/shift_reg_standin.sv $HERE/spill_register_standin.sv $HERE/fifo_v3_standin.sv"
nice -n 19 verilator --binary --timing -sv -Wno-fatal -Wno-ENUMVALUE --top-module tb_top_softmax --Mdir "$W/tb_top_softmax" $SRC "$HERE/tb_top_softmax.sv" > "$W/build.log" 2>&1
rc=$?
echo "build rc=$rc; errors $(grep -c '^%Error' "$W/build.log"); warnings by kind:"
grep -o '^%Warning-[A-Z]*' "$W/build.log" | sort | uniq -c | sort -rn
grep -E '^%Error' -A3 "$W/build.log" | cut -c1-240 | head -40
grep -E '^%Warning-(UNDRIVEN|PINMISSING|IMPLICIT|MULTIDRIVEN|UNOPTFLAT|CASEINCOMPLETE|LATCH)' "$W/build.log" | cut -c1-240 | head -40
[ $rc = 0 ] || exit $rc
for L in $LENS; do
  echo "== token length $L"
  timeout 600 "$W/tb_top_softmax/Vtb_top_softmax" +len=$L 2>&1 | tee "$W/run_len$L.log" | grep -E "^TOP|Error|FATAL" | cut -c1-330
done
# optional 4th argument: vector directory of tb_rtl_ref_gvu_bdump -> replay of the table-load sequence (tb_oracle_top.sv)
V=${4:-}
if [ -n "$V" ]; then
  nice -n 19 verilator --binary --timing -sv -Wno-fatal -Wno-ENUMVALUE --top-module tb_t_lutload --Mdir "$W/tb_t_lutload" $SRC "$HERE/tb_oracle_top.sv" > "$W/build_lutload.log" 2>&1 \
    || { echo "BUILD FAIL tb_t_lutload (see $W/build_lutload.log)"; exit 1; }
  "$W/tb_t_lutload/Vtb_t_lutload" +dir="$V" 2>&1 | tee "$W/tb_t_lutload.log" | grep -E "ORACLE|DETAIL|FIRST|Error|FATAL" | cut -c1-330
  grep -q "ORACLE lut_load .* mismatches 0$" "$W/tb_t_lutload.log" || exit 1
fi
