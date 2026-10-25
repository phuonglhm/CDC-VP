#!/bin/bash
# RTL oracle of the blocks of the vector unit that changed or were added in the RTL with gvu_top (LUT unit with its own
# write address, comparator, 48x40 multiplier with three interfaces, stream registers, Scratchpad with memory wrappers,
# load / store unit): builds tb_oracle_b.sv against the RTL with Verilator 5 and replays the files written by
# tb_rtl_ref_gvu_bdump (and mulb.txt written by tb_rtl_ref_gvu_fu --dump).
# usage: bash run_verilator_b.sh <rtl gvu dir> <vector dir> <work dir>     (TOPS="..." selects the tops)
set -u
R=${1:?rtl gvu dir}; V=${2:?vector dir}; W=${3:?work dir}
HERE=$(cd "$(dirname "$0")" && pwd)
command -v verilator > /dev/null || { echo "verilator not found (version 5 or later is needed for --binary --timing)"; exit 2; }
mkdir -p "$W"
F=$R/FUs; S=$R/stream_switch; P=$R/scratchpad
SRC="$R/gvu_pkg.sv $R/pipeline_reg.sv $F/clz.sv $F/gvu_lut_mem.sv $F/gvu_lut_lookup_lane.sv $F/gvu_lut_unit.sv $F/gvu_vcmp32.sv $F/gvu_vmul48x40.sv
     $S/vstream_reg_int32.sv $S/vstream_reg_int64_type0.sv $S/vstream_reg_int64_type1.sv $S/stream_vrf.sv
     $P/sram_single_port.sv $P/sram_dual_port.sv $P/gvu_scratchpad.sv $R/vlsu/gvu_vlsu.sv $R/vlsu/gvu_pooling_feeder.sv $HERE/fifo_v3_standin.sv $HERE/shift_reg_standin.sv"
rc=0
for top in ${TOPS:-tb_b_lut tb_b_lut32 tb_b_cmp0 tb_b_cmp1 tb_b_mulb tb_b_svrf tb_b_spad tb_b_vlsu}; do
  nice -n 19 verilator --binary --timing -sv -Wno-fatal -Wno-ENUMVALUE --top-module $top --Mdir "$W/$top" $SRC "$HERE/tb_oracle_b.sv" > "$W/$top.build.log" 2>&1 \
    || { echo "BUILD FAIL $top (see $W/$top.build.log)"; grep -E "^%Error" -A3 "$W/$top.build.log" | head -20; rc=1; continue; }
  "$W/$top/V$top" +dir="$V" 2>&1 | tee "$W/$top.log" | grep -E "ORACLE|DETAIL|LATENCY|DIRECTED|FIRST|Error|FATAL" | cut -c1-400
done
grep -h "ORACLE" "$W"/tb_b_*.log 2>/dev/null | grep -v "mismatches 0" && rc=1
exit $rc
