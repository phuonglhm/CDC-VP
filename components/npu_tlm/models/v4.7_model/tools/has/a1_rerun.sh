#!/bin/bash
# Tile-shape coverage at the specified SRAM capacity (VERIFICATION_REPORT.md, V2 and V3): runs the 150 tile-shape jobs of
# $FE_WORK/core/jobs.tsv with tb_rtl_ref_npu_top_tile at the real per-buffer capacity (A 79 KB, B 81 KB) with the capacity
# checker on, and compares every job (verdict, mismatches, elements, cycles) with a reference log of the same job set.
# Outputs under tools/has/capchk/. Run from anywhere; the script changes to the repository root.
cd "$(dirname "$0")/../.." || exit 1
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}
O=tools/has/capchk; mkdir -p $O
BIN=$O/tb_tile_a1
REF=fe_work/core/port_150jobs_stallfix_2249.log
EXTRA_DEFS="-DFX1_A3_SRAM_BACKDOOR_LOAD -DFX1_SRAM_CAP_CHECK" OUT=$BIN nice -n 5 bash tools/fe/sysc/build_tb_rtl_ref_npu_top_tile.sh || exit 1
LOG=$O/a1_150jobs.log
/usr/bin/time -f "WALL %e s" env FE_SRAM_BACKDOOR=1 FE_JOBS_TSV=fe_work/core/jobs.tsv nice -n 5 timeout 21600 ./$BIN > $LOG 2>&1
echo "PASS $(grep -c '	PASS	' $LOG)  FAIL $(grep -c '	FAIL	' $LOG)"
grep RESULT $LOG; grep SRAM_CAP_CHECK $LOG; grep WALL $LOG
# per-job comparison with the reference log: name, verdict, mismatch, elems, cycles (drop load_ms / wall-clock columns)
cut5() { grep -E "^[a-z].*	(PASS|FAIL)	" "$1" | awk -F'\t' '{print $1, $2, $3, $4, $5}' | sort; }
cut5 $REF > $O/a1_ref.txt; cut5 $LOG > $O/a1_new.txt
echo "jobs identical to the reference (verdict, mismatch, elems, cycles): $(comm -12 $O/a1_ref.txt $O/a1_new.txt | wc -l) / $(wc -l < $O/a1_ref.txt)"
diff $O/a1_ref.txt $O/a1_new.txt | head -20
echo done
