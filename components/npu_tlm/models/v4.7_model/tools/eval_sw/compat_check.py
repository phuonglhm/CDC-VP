#!/usr/bin/env python3
"""Validate `sw_eval --compat`: every cell of the 9 CSVs must MATCH `sauria_model` EXACTLY (sim/*.csv of each 1-tile job).

Compares: sw_eval's `layer` row (layer_name = job name) against the `1,core,GeMM` row in <jobs-dir>/<job>/sim/<csv>,
plus the `network` row. Compares strings after normalizing numbers (0.0 == 0). No tolerance: any difference is a MISMATCH.

usage: python compat_check.py --out out/compat --jobs-dir data/jobs
"""
import argparse
import csv
import os
import sys

CSVS = ["cycles.csv", "engine.csv", "dma.csv", "stall.csv", "utilization.csv", "bandwidth.csv", "memory.csv",
        "memory_wait.csv", "summary.csv"]


def norm(x):
    x = x.strip()
    try:
        f = float(x)
        return repr(int(f)) if f == int(f) else repr(f)
    except ValueError:
        return x


def rows(path):
    with open(path, newline="", encoding="utf-8") as fh:
        return list(csv.DictReader(fh))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--jobs-dir", required=True)
    a = ap.parse_args()
    cells = bad = 0
    per_metric = {}
    jobs = 0
    first = []
    for name in CSVS:
        mine = rows(os.path.join(a.out, name))
        for r in mine:
            if r["layer_id"] == "network":
                continue
            job = r["layer_name"]
            ref_p = os.path.join(a.jobs_dir, job, "sim", name)
            if not os.path.isfile(ref_p):
                print("!! missing", ref_p)
                bad += 1
                continue
            ref = [x for x in rows(ref_p) if x["layer_id"] != "network"][0]
            jobs += name == CSVS[0]
            for k, v in r.items():
                if k in ("layer_id", "layer_name", "op_type"):
                    continue
                cells += 1
                if norm(v) != norm(ref.get(k, "")):
                    bad += 1
                    per_metric.setdefault(k, []).append((job, v, ref.get(k)))
                    if len(first) < 12:
                        first.append((name, job, k, v, ref.get(k)))
    print("jobs compared: %d | cells compared: %d | MISMATCHES: %d" % (jobs, cells, bad))
    for k, v in sorted(per_metric.items()):
        print("  %-28s mismatched in %3d job(s), e.g. %s" % (k, len(v), v[0]))
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
