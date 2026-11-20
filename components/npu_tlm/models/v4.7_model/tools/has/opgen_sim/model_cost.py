#!/usr/bin/env python3
"""model_cost.py <cases dir> -- the segment cost formula measured in g1 / g3 / g6, tested on the other groups (descriptor START->DONE).
Formula (system clocks, ideal memory): COPY  P = A_COPY + max(bs, bd + (sm > dm));  FILL  P = A_FILL + bd;  descriptor = sum of P over its segments;
bs = ceil((sm + len) / 16), bd = ceil((dm + len) / 16), sm / dm = source / destination address mod 16."""
import csv, os, sys
A_COPY, A_FILL = 10, 5
out = sys.argv[1]
rows = list(csv.DictReader(open(os.path.join(out, "timing.csv"))))
def P(f, ra, wa, n):
    sm, dm = ra % 16, wa % 16
    bs, bd = -(-(sm + n) // 16), -(-(dm + n) // 16)
    return A_FILL + bd if f else A_COPY + max(bs, bd + (1 if sm > dm else 0))
worst = 0
for g in sorted(os.listdir(out)):
    if not os.path.exists(os.path.join(out, g, "expect_seg.txt")) or g in ("g7_legacy", "g9_regs", "g8_twostart"):
        continue
    es = [l.split() for l in open(os.path.join(out, g, "expect_seg.txt")).read().strip().split("\n") if l.strip()]
    ds = [r for r in rows if r["group"] == g and r["kind"] == "desc"]
    k, tot_m, tot_p, bad = 0, 0, 0, 0
    for d in ds:
        n = int(d["a"])
        pred = sum(P(int(e[0]) == 1, 0 if e[1].startswith("-") else int(e[1], 16), int(e[2], 16), int(e[3])) for e in es[k:k + n])
        meas = int(d["d"])
        k += n
        tot_m += meas; tot_p += pred
        if pred != meas:
            bad += 1
            worst = max(worst, abs(pred - meas) / meas)
            if bad < 4:
                print("  %s: descriptor of %d segments: formula %d, RTL %d" % (g, n, pred, meas))
    print("%-10s descriptors %3d  RTL %6d  formula %6d  (%+.2f %%)  exact %d / %d" % (g, len(ds), tot_m, tot_p, 100.0 * (tot_p - tot_m) / max(1, tot_m), len(ds) - bad, len(ds)))
print("worst relative error of one descriptor: %.1f %%" % (100 * worst))
