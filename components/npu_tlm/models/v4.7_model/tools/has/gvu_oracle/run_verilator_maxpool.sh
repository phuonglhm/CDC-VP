#!/bin/bash
# MAXPOOL of gvu_top.sv (latest hardware RTL drop, layout with vlsu/ and the pooling feeder) against the port GvuTop, cycle by cycle:
# for each vector set of target/test/maxpool, one run (the channels that fit vbank0) traced by tb_oracle_maxpool.sv on the
# RTL and by tb_gvu_top_maxpool --trace on the port, then the two traces compared line by line.
# The modules shift_reg, spill_register_flushable and fifo_v3 are taken from the *_standin.sv files (not delivered, B8).
# Prints "ORACLE maxpool <set> cycles <n> lines <rtl> / <port> mismatches <m>" per set and a RESULT line.
# usage: bash run_verilator_maxpool.sh <rtl gvu dir> <work dir> <target/test/maxpool dir> <tb_gvu_top_maxpool binary>
set -u
R=${1:?rtl gvu dir}; W=${2:?work dir}; V=${3:?vector dir}; P=${4:?tb_gvu_top_maxpool binary}
HERE=$(cd "$(dirname "$0")" && pwd)
command -v verilator > /dev/null || { echo "verilator not found (version 5 or later)"; exit 2; }
mkdir -p "$W"
SRC="$R/gvu_pkg.sv $R/pipeline_reg.sv $(ls $R/FUs/*.sv | tr '\n' ' ') $(ls $R/stream_switch/*.sv | tr '\n' ' ') $(ls $R/scratchpad/*.sv | tr '\n' ' ')
     $(ls $R/vlsu/*.sv | tr '\n' ' ') $R/gvu_pipe_controller.sv $R/gvu_top.sv
     $HERE/shift_reg_standin.sv $HERE/spill_register_standin.sv $HERE/fifo_v3_standin.sv"
nice -n 19 verilator --binary --timing -sv -Wno-fatal -Wno-ENUMVALUE --top-module tb_oracle_maxpool --Mdir "$W/tb_oracle_maxpool" $SRC "$HERE/tb_oracle_maxpool.sv" > "$W/build.log" 2>&1
rc=$?
echo "build rc=$rc; errors $(grep -c '^%Error' "$W/build.log")"
grep -E '^%Error' -A3 "$W/build.log" | cut -c1-240 | head -40
[ $rc = 0 ] || exit $rc
bad=0
# set  H/W  k  s  channels of one run (800 rows of vbank0 / (H * Wt))
for e in "single/stride_1/k2 20 2 1 40" "single/stride_1/k3 20 3 1 40" "multi/stride_1/k2 80 2 1 3" "multi/stride_1/k3 80 3 1 3" \
         "multi/stride_1/k4 80 4 1 3" "multi/stride_1/k5 80 5 1 3" "multi/stride_2/k2 80 2 2 3" "multi/stride_2/k4 80 4 2 3" "multi/stride_4/k4 80 4 4 3"; do
  set -- $e
  S=$1; N=$2; K=$3; ST=$4; C=$5; T=$(echo "$S" | tr '/' '_')
  timeout 1200 "$W/tb_oracle_maxpool/Vtb_oracle_maxpool" +in="$V/$S/input.txt" +out="$W/rtl_$T.txt" +h=$N +w=$N +k=$K +s=$ST +c=$C > "$W/rtl_$T.log" 2>&1
  "$P" --trace "$V/$S/input.txt" "$W/port_$T.txt" $N $N $K $ST $C > "$W/port_$T.log" 2>&1
  a=$(wc -l < "$W/rtl_$T.txt" 2>/dev/null || echo 0); b=$(wc -l < "$W/port_$T.txt" 2>/dev/null || echo 0)
  m=$(diff "$W/rtl_$T.txt" "$W/port_$T.txt" 2>/dev/null | grep -c '^[<>]')
  cyc=$(grep -o 'DONE [0-9]*' "$W/rtl_$T.txt" | cut -d' ' -f2)
  echo "ORACLE maxpool $S cycles ${cyc:-none} lines $a / $b mismatches $m"
  [ "$m" = 0 ] && [ -n "$cyc" ] || { bad=$((bad + 1)); diff "$W/rtl_$T.txt" "$W/port_$T.txt" | head -6 | cut -c1-200; }
done
# two runs back to back without a reset (the feeder keeps first_load_cnt): RTL and port traces compared; the vbank2 rows
# of the second run against the golden are checked separately (chk_trace.py), this line only compares RTL and port
S=single/stride_1/k3; T=b2b_single_stride_1_k3
timeout 1200 "$W/tb_oracle_maxpool/Vtb_oracle_maxpool" +in="$V/$S/input.txt" +out="$W/rtl_$T.txt" +h=20 +w=20 +k=3 +s=1 +c=40 +runs=2 > "$W/rtl_$T.log" 2>&1
"$P" --trace "$V/$S/input.txt" "$W/port_$T.txt" 20 20 3 1 40 2 > "$W/port_$T.log" 2>&1
a=$(wc -l < "$W/rtl_$T.txt" 2>/dev/null || echo 0); b=$(wc -l < "$W/port_$T.txt" 2>/dev/null || echo 0)
m=$(diff "$W/rtl_$T.txt" "$W/port_$T.txt" 2>/dev/null | grep -c '^[<>]')
echo "ORACLE maxpool back-to-back x2 $S done $(grep -o 'DONE [0-9]*' "$W/rtl_$T.txt" | cut -d' ' -f2 | tr '\n' ' ')lines $a / $b mismatches $m"
[ "$m" = 0 ] || { bad=$((bad + 1)); diff "$W/rtl_$T.txt" "$W/port_$T.txt" | head -6 | cut -c1-200; }
echo "RESULT: $([ $bad = 0 ] && echo PASS || echo FAIL) (sets with a difference or no done: $bad of 10)"
[ $bad = 0 ]
