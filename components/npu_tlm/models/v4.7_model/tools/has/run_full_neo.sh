#!/bin/bash
# run_full_neo.sh -- whole YOLOv8m through HasNpuTop with the run profile "neo" (REV 1 of neo_lite RTL revision 1, every neo knob on, the
# proposals the RTL does not have off, no 3-D DMA descriptors), program insts_pe3 (concat by address: no DMA instruction).
# Meant for the VM, from the model root, started by the coordinator:
#   setsid nohup bash tools/has/run_full_neo.sh r1 > r1.out 2>&1 & disown        (from the model root; results under /var/tmp/npu_safe/<tag>)
# Result: /var/tmp/npu_safe/<tag>/status (V/Y rc, RESULT and "sim cycles" lines), tb_full, yolo.log, file "done" at the end.
#   usage: run_full_neo.sh <tag> [program dir, default fe_work/has/insts_pe3]
#   environment: RUN_D3=1 also runs the same program with --desc3d added (the price of the 3-D descriptors, second job after the first);
#                RUN_ASRTL=1 also runs neo_as_rtl; NICE (default 10).
# The run also sets HAS_HW_CHECK=1: one "[hw-check]" line per instruction in the log (changes no value and no cycle); compare with
#   python3 tools/has/hw_check_program.py <program dir>
#
# PREDICTION (written before, ESTIMATE; PERFORMANCE_REPORT.md, section 5.11):
#   PASS 98 / 98, 0 bad. Cycles band [64 000 000, 74 000 000], middle about 68 M.
#   Reference points: 62 422 935 (insts_pe3, defaults, neo knobs off); six neo knobs on insts_pe3_nd +5 708 261 (+8.9 %, 69 843 642 against
#   64 135 381); REV 1 + the new register map + ping-pong + CDC a few hundred thousand; 3-D descriptors off not measured (up to a few million).
#   neo --desc3d: not above the neo number. neo_as_rtl: more cycles than neo (ADD leaves the RESIDUAL unit when rounding is half away from zero)
#   and bad elements (the golden is half up) -- reported, not corrected.
# Deviation from the band is reported with its size; the model is not changed to fit.
set -u
TAG=${1:?usage: run_full_neo.sh <tag> [program dir]}
PROG=${2:-fe_work/has/insts_pe3}
S=/var/tmp/npu_safe/$TAG; mkdir -p $S
cd "$(dirname "$0")/../.." || exit 1
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}
N=${NICE:-10}
# no HAS_* variable of the calling shell leaks into the run
K=$(env | grep -o '^HAS_[A-Z0-9_]*' | sed 's/^/-u /' | tr '\n' ' ')
{ echo "free $(( $(df --output=avail -k / | tail -1) / 1024 )) MB"; echo "BUILD $(date '+%F %T')"; } > $S/status
OUT=$S/tb_full bash tools/has/build_tb_has_npu_top.sh > $S/build.log 2>&1 || { echo "BUILD FAILED" >> $S/status; touch $S/done; exit 1; }
md5sum has/has_npu_top.h has/has_profile_neo.h has/hw_capability.h tools/has/tb_has_npu_top.cpp >> $S/status
run() {   # run <label> <log> <profile> [testbench args]
  local label=$1 log=$2 prof=$3; shift 3
  echo "$label START $(date '+%F %T')" >> $S/status
  env $K HAS_PROFILE=$prof HAS_HW_CHECK=1 nice -n $N $S/tb_full $PROG "$@" > $S/$log 2>&1
  echo "$label rc $? $(date '+%F %T')" >> $S/status
  grep -h "RESULT:\|sim cycles\|core busy\|hw-check\] summary\|\[profile" $S/$log | cut -c1-300 >> $S/status
}
run Y yolo.log neo
[ "${RUN_D3:-0}" = 1 ] && run YD3 yolo_desc3d.log neo --desc3d
[ "${RUN_ASRTL:-0}" = 1 ] && run YR yolo_asrtl.log neo_as_rtl
df -h / | tail -1 >> $S/status
touch $S/done
