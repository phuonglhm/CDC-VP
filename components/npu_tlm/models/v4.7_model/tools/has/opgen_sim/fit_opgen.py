#!/usr/bin/env python3
"""fit_opgen.py <cases dir> -- cost of one segment and of one descriptor from timing.csv + expect_seg.txt (system clocks, ideal memory)."""
import csv, os, sys
import statistics as st
out = sys.argv[1]
rows = list(csv.DictReader(open(os.path.join(out, "timing.csv"))))
def exp_segs(g):
    return [l.split() for l in open(os.path.join(out, g, "expect_seg.txt")).read().strip().split("\n") if l.strip()]
# per segment: latency (SEG -> SEGEND = e), period to the next SEG (d... stored in column e)
print("== per-segment latency SEG->SEGEND and period (cycles), group g1 (one segment per descriptor)")
g1 = [r for r in rows if r["group"] == "g1_copy1" and r["kind"] == "seg"]
es = exp_segs("g1_copy1")
print("len  srcmis dstmis  latency  (n=%d)" % len(g1))
tab = {}
for r, e in zip(g1, es):
    ra, wa, n = int(e[1], 16), int(e[2], 16), int(e[3])
    key = (n, ra % 16, wa % 16)
    tab[key] = int(r["d"])
for n in sorted({k[0] for k in tab}):
    print("%5d  " % n + "  ".join("(%d,%d):%3d" % (k[1], k[2], v) for k, v in sorted(tab.items()) if k[0] == n))
print("== g6: steady state, period between consecutive SEG of one descriptor (row size)")
g6s = [r for r in rows if r["group"] == "g6_slope" and r["kind"] == "seg"]
g6d = [r for r in rows if r["group"] == "g6_slope" and r["kind"] == "desc"]
es6 = exp_segs("g6_slope")
k = 0
for dsc in g6d:
    n = int(dsc["a"])
    seg = g6s[k:k + n]
    per = [int(x["e"]) for x in seg[:-1]]
    print("row %4s B: %3d segments, period median %s min %s max %s | START->DONE %s | first SEG %s after START" %
          (es6[k][3], n, st.median(per), min(per), max(per), dsc["d"], dsc["e"]))
    k += n
print("== group g1 descriptors: START->DONE vs bytes (one segment each)")
d1 = [r for r in rows if r["group"] == "g1_copy1" and r["kind"] == "desc"]
for r, e in list(zip(d1, es))[::4]:
    print("bytes %5s  START->DONE %s  first SEG +%s" % (e[3], r["d"], r["e"]))
print("== group g3 (FILL) latency SEG->SEGEND vs bytes; group g4/g5 per descriptor")
g3 = [r for r in rows if r["group"] == "g3_fill" and r["kind"] == "seg"]
e3 = exp_segs("g3_fill")
for r, e in list(zip(g3, e3))[:5]:
    print("fill %5s B: latency %s" % (e[3], r["d"]))
print("== legacy (CONTROL B response -> writer interrupt), conf_regs 1-D")
lg = [r for r in rows if r["group"] == "g7_legacy" and r["kind"] == "leg"]
sizes = [(n, a) for n in (16, 64, 256, 1024, 2048, 4096) for a in ("aligned", "unaligned 3/5")] + [(64, "write_zero"), (100, "write_zero, dst+3")]
for r, s_ in zip(lg, sizes):
    print("bytes %5d (%s): %s cycles" % (s_[0], s_[1], r["d"]))
