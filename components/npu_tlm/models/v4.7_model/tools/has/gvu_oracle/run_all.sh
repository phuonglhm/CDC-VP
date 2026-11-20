#!/bin/bash
# Every RTL oracle of the vector unit in one go: builds the C++ dump programs of the ported blocks, writes the vectors,
# replays them on the RTL with Verilator 5 and prints one summary line per block.
# Run from the repository root on the Linux machine:
#   bash tools/has/gvu_oracle/run_all.sh <rtl gvu dir> <work dir>
#   <rtl gvu dir>  copy of neo_core/gvu (read-only source of the hardware team)
#   <work dir>     scratch directory (about 500 MB of vectors and simulator builds)
# Result: <work dir>/summary.txt and the exit code (0 = every block without a mismatch).
set -u
R=${1:?rtl gvu dir}; W=${2:?work dir}
HERE=$(cd "$(dirname "$0")" && pwd)
command -v verilator > /dev/null || { echo "verilator not found (version 5 or later)"; exit 2; }
mkdir -p "$W/bin" "$W/vec" "$W/sim_fu" "$W/sim_g2" "$W/sim_b" "$W/sim_dir" "$W/sim_top"
for t in tb_rtl_ref_gvu_fu tb_rtl_ref_gvu_g2dump tb_rtl_ref_gvu_bdump; do
  nice -n 19 g++ -std=c++17 -O2 -I. tools/has/$t.cpp -o "$W/bin/$t" 2> "$W/bin/$t.build.log" || { echo "BUILD FAIL $t (see $W/bin/$t.build.log)"; exit 1; }
done
"$W/bin/tb_rtl_ref_gvu_fu" --dump "$W/vec" > "$W/bin/tb_rtl_ref_gvu_fu.log" 2>&1
tail -1 "$W/bin/tb_rtl_ref_gvu_fu.log"
"$W/bin/tb_rtl_ref_gvu_g2dump" "$W/vec" > /dev/null
"$W/bin/tb_rtl_ref_gvu_bdump" "$W/vec" > /dev/null
{
  bash "$HERE/run_verilator.sh" "$R" "$W/vec" "$W/sim_fu"
  TOPS=tb_oracle_adder64p bash "$HERE/run_verilator.sh" "$R" "$W/vec" "$W/sim_fu"
  bash "$HERE/run_verilator_g2.sh" "$R" "$W/vec" "$W/sim_g2"
  bash "$HERE/run_verilator_b.sh" "$R" "$W/vec" "$W/sim_b"
  bash "$HERE/run_verilator_directed.sh" "$R" "$W/sim_dir"
  bash "$HERE/run_verilator_top.sh" "$R" "$W/sim_top" "8" "$W/vec"
} > "$W/run.log" 2>&1
grep -hE "^(ORACLE|KNOWN|VALUE|STREAM|LATENCY|DIRECTED|TOP end)" "$W/run.log" | sort -u > "$W/summary.txt"
cat "$W/summary.txt"
n=$(grep -c "^ORACLE" "$W/summary.txt"); bad=$(grep "^ORACLE" "$W/summary.txt" | grep -vc "mismatches 0$")
fail=$(grep -c "BUILD FAIL" "$W/run.log")
echo "RESULT: $n oracle lines, $bad with mismatches, $fail harness builds failed"
[ "$bad" = 0 ] && [ "$fail" = 0 ] && [ "$n" -gt 0 ]
rc=$?
# After a replay without a mismatch: digest of the vector files = digest of what the RTL produced on this stimulus.
# Copy <work dir>/rtl_trace_digest.txt to tools/has/gvu_oracle/ to make it the reference of tools/has/check_rtl_trace.sh.
if [ $rc = 0 ]; then
  g++ -std=c++17 -O2 -I. tools/has/tb_rtl_ref_gvu_trace.cpp -o "$W/bin/tb_rtl_ref_gvu_trace" 2> "$W/bin/tb_rtl_ref_gvu_trace.build.log" || { echo "BUILD FAIL tb_rtl_ref_gvu_trace"; exit 1; }
  {
    echo "# Digest of the vector files of the ported vector-unit blocks after a replay on the RTL without a mismatch."
    echo "# Simulator: $(verilator --version | cut -d" " -f1-2)"
    echo "# RTL: sha256 of the .sv files of the gvu directory, sorted by path and concatenated: $(find "$R" -name '*.sv' -not -path '*/deprecated/*' | LC_ALL=C sort | xargs cat | sha256sum | cut -d' ' -f1)"
    echo "# The digest covers the complete files; the replay compared the beats and the fields its harnesses list:"
    grep "^ORACLE" "$W/summary.txt" | sed 's/^/#   /'
    echo "# Format: file, first line of the chunk, lines in the chunk, FNV-1a 64 of the chunk"
    "$W/bin/tb_rtl_ref_gvu_trace" digest "$W/vec"
  } > "$W/rtl_trace_digest.txt"
  echo "digest written: $W/rtl_trace_digest.txt ($(grep -vc '^#' "$W/rtl_trace_digest.txt") chunks)"
fi
exit $rc
