#!/usr/bin/env python3
"""PER-TILE coverage of one sweep of the ported core.

In : (1) sweep log of tb_rtl_ref_npu_top_tile (one line per job, column 1 = job name,
          column 2 = PASS/FAIL), (2) per_shape.csv of tools/metrics/agg_network_metrics.py
          (columns `job`, `n_tiles`) -- the ONLY tile source, no number typed by hand.
Out : PASS tiles / all tiles, with the list of FAIL jobs (and their tile counts) to show where the weight is.
"""
import csv, sys

scan_log, per_shape = sys.argv[1], sys.argv[2]

status = {}
with open(scan_log, errors="replace") as f:
    for line in f:
        p = line.rstrip("\n").split("\t")
        if len(p) >= 2 and p[1] in ("PASS", "FAIL"):
            status[p[0]] = p[1]

tiles, total_tiles = {}, 0
with open(per_shape, newline="") as f:
    for r in csv.DictReader(f):
        n = int(r["n_tiles"])
        tiles[r["job"]] = tiles.get(r["job"], 0) + n
        total_tiles += n

pass_t = sum(n for j, n in tiles.items() if status.get(j) == "PASS")
fail_t = sum(n for j, n in tiles.items() if status.get(j) == "FAIL")
miss_t = sum(n for j, n in tiles.items() if j not in status)

print("job trong log      : %d (PASS %d / FAIL %d)"
      % (len(status), sum(1 for v in status.values() if v == "PASS"),
         sum(1 for v in status.values() if v == "FAIL")))
print("tile trong per_shape: %d" % total_tiles)
print("  tile PASS        : %d (%.1f %%)" % (pass_t, 100.0 * pass_t / total_tiles))
print("  tile FAIL        : %d (%.1f %%)" % (fail_t, 100.0 * fail_t / total_tiles))
print("  tiles not run     : %d (%.1f %%)" % (miss_t, 100.0 * miss_t / total_tiles))
bad = sorted(((n, j) for j, n in tiles.items() if status.get(j) == "FAIL"), reverse=True)
if bad:
    print("\nJob FAIL, sap theo so tile:")
    for n, j in bad[:25]:
        print("  %5d tile  %s" % (n, j))
