#!/usr/bin/env python3
"""Roll up NETWORK-level metrics from TILE-level measurements, keyed by tile shape.

Rules (keep them when editing this file):
  1. NO hard-coded numbers. Every number in the output traces back to `file:line` of a file on disk. `--expect-*` only
     CHECKS, it never changes a number.
  2. No full trace of all 6 924 tiles. Basis: the 6 924 tiles have only 150 shapes, and three tiles of the same shape at
     different positions / with different data all measure 18 689 cycles (cycles depend only on shape). So: measure one
     tile per shape and multiply by the tile count.
  3. A tile that does NOT match a job is REPORTED, never silently dropped. Tiles in --exclude (default `det.p3.box_out`,
     10 tiles) are separated and reported, not added to the total.
  4. The per-tile sum is SEQUENTIAL: no DMA overlap between tiles is assumed. Off-core operations (OBP, OBP configuration
     load, network-level DMA, host ops) are not part of this file.

The DEFAULT data source is `sauria_model` (`fe_work/core/jobs/<job>/sim/run.log`, `[EVAL]` lines). The ported (v4.5) path,
once it exports CSVs, is read by this same script through another --jobs-dir and must match before it is used.

Run (read-only):
  python3 tools/metrics/agg_network_metrics.py --root <v4.5_model checkout>       --out-dir /tmp/metrics_out       --expect-busy 209651588 --expect-total 249616773
"""

import argparse
import csv
import json
import os
import sys
from collections import OrderedDict

# [EVAL] fields summed per tile (linear: value per tile x tile count).
# This list deliberately holds ADDITIVE fields only. Ratio fields (transfer_overhead_percent,
# engine_utilization) are NOT summed -- they are recomputed from the totals.
SUM_FIELDS = [
    "total_cycles",
    "processing_cycles",
    "core_exec_cycles",
    "transfer_cycles",
    "transfer_overhead_cycles",
    "internal_transfer_cycles",
    "mac_engine_cycles",
    "dma_engine_cycles",
    "dma_stall_cycles",
    "wait_input_cycles",
    "wait_output_cycles",
    "ddr_read_bytes",
    "ddr_write_bytes",
    "weight_bytes",
    "l2_to_l1_bytes",
    "l1_to_l2_bytes",
    "theory_min_cycles",
    "active_cycles",
    "idle_cycles",
    "pe_active_cycles",
    "pe_idle_cycles",
    "bias_bytes",
    "l1_read_bytes",
    "l1_write_bytes",
    "l2_read_bytes",
    "l2_write_bytes",
]

# Shape key: (sizes c/oy/ox/iy/ix) + x_used + y_used, within ONE conv.
KEY_FIELDS = ("dc", "doy", "dox", "diy", "dix", "x_used", "y_used")


def is_pass(status):
    """STATUS in run.log looks like ` STATUS    : PASS  (0 / 20480 mismatches, timeouts=0)`."""
    return bool(status) and "PASS" in status.split(":", 1)[-1]


def tile_key(conv, tl):
    return (
        conv,
        tl["c"][1] - tl["c"][0],
        tl["oy"][1] - tl["oy"][0],
        tl["ox"][1] - tl["ox"][0],
        tl["iy"][1] - tl["iy"][0],
        tl["ix"][1] - tl["ix"][0],
        tl["x_used"],
        tl["y_used"],
    )


def parse_run_log(path):
    """Read `[EVAL] k=v` + STATUS from run.log. Returns (values, linenos, status).

    linenos[k] = 1-based line number that produced value k (for traceability).
    """
    values = OrderedDict()
    linenos = {}
    status = None
    with open(path, "r", errors="replace") as fh:
        for i, line in enumerate(fh, 1):
            line = line.rstrip("\n")
            if line.startswith("[EVAL] "):
                body = line[len("[EVAL] "):]
                if "=" not in body:
                    continue
                k, v = body.split("=", 1)
                if k == "dma_model":
                    values[k] = v
                    linenos[k] = i
                    continue
                try:
                    values[k] = int(v)
                except ValueError:
                    try:
                        values[k] = float(v)
                    except ValueError:
                        values[k] = v
                linenos[k] = i
            elif " STATUS " in line and ":" in line:
                status = line.strip()
                linenos["STATUS"] = i
    return values, linenos, status


def load_jobs(jobs_dir):
    """Read every job directory: job.json + sim/run.log. Returns a list of dicts."""
    jobs = []
    for name in sorted(os.listdir(jobs_dir)):
        jdir = os.path.join(jobs_dir, name)
        jf = os.path.join(jdir, "job.json")
        if not os.path.isfile(jf):
            continue
        with open(jf) as fh:
            meta = json.load(fh)
        rl = os.path.join(jdir, "sim", "run.log")
        rec = {
            "name": name,
            "dir": jdir,
            "conv": meta.get("conv"),
            "tile": meta.get("tile"),
            "run_log": rl if os.path.isfile(rl) else None,
            "values": None,
            "linenos": None,
            "status": None,
        }
        if rec["run_log"]:
            rec["values"], rec["linenos"], rec["status"] = parse_run_log(rl)
        jobs.append(rec)
    return jobs


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=".",
                    help="root of the tree that contains fe_work/ (default: current directory = repo root)")
    ap.add_argument("--program", default=None,
                    help="default <root>/fe_work/step6/program.json")
    ap.add_argument("--jobs-dir", default=None,
                    help="default <root>/fe_work/core/jobs")
    ap.add_argument("--out-dir", default=None, help="directory for CSV/JSON (not set = print only)")
    ap.add_argument("--exclude-job", action="append", default=[],
                    help="job name to set aside, kept out of the total (repeatable)")
    ap.add_argument("--keep-nonpass", action="store_true",
                    help="also sum shapes whose STATUS is not PASS (DEFAULT: set aside, not summed)")
    ap.add_argument("--expect-busy", type=int, default=None,
                    help="check: total processing_cycles (core busy) must equal this number")
    ap.add_argument("--expect-total", type=int, default=None,
                    help="check: total total_cycles must equal this number")
    args = ap.parse_args()

    root = os.path.expanduser(args.root)
    program = args.program or os.path.join(root, "fe_work", "step6", "program.json")
    jobs_dir = args.jobs_dir or os.path.join(root, "fe_work", "core", "jobs")
    excluded_jobs = set(args.exclude_job)

    with open(program) as fh:
        prog = json.load(fh)
    jobs = load_jobs(jobs_dir)

    # --- map shape key -> measured job -------------------------------------------
    by_key = {}
    dup_conflicts = []
    jobs_no_log = []
    for rec in jobs:
        if not rec["tile"] or not rec["conv"]:
            continue
        k = tile_key(rec["conv"], rec["tile"])
        if rec["values"] is None:
            jobs_no_log.append(rec["name"])
            continue
        if k in by_key:
            a, b = by_key[k], rec
            diff = [f for f in SUM_FIELDS
                    if a["values"].get(f) != b["values"].get(f)]
            if diff:
                dup_conflicts.append((k, a["name"], b["name"], diff))
            continue  # keep the first job (stable sort order)
        by_key[k] = rec

    # --- walk the tile plan -----------------------------------------------------
    per_shape = OrderedDict()   # key -> dict
    per_conv = OrderedDict()    # conv -> dict
    unmatched = []              # (conv, key, n_tiles)
    excluded = []               # (conv, key, n_tiles, job, reason, per_tile values)
    n_tiles_total = 0
    n_steps_conv = 0
    non_conv_steps = []

    for st in prog["steps"]:
        if st.get("kind") != "conv":
            non_conv_steps.append((st.get("kind"), st.get("job")))
            continue
        n_steps_conv += 1
        conv = st["job"]
        counts = OrderedDict()
        for tl in st["tiles"]:
            k = tile_key(conv, tl)
            counts[k] = counts.get(k, 0) + 1
            n_tiles_total += 1
        for k, n in counts.items():
            rec = by_key.get(k)
            if rec is None:
                unmatched.append((conv, k, n))
                continue
            reason = None
            if rec["name"] in excluded_jobs:
                reason = "--exclude-job"
            elif not args.keep_nonpass and not is_pass(rec["status"]):
                reason = "STATUS not PASS: %s" % (rec["status"] or "no STATUS line")
            if reason:
                excluded.append((conv, k, n, rec["name"], reason, rec["values"]))
                continue
            row = per_shape.setdefault(k, {
                "conv": conv, "key": k, "job": rec["name"], "n_tiles": 0,
                "per_tile": rec["values"], "run_log": rec["run_log"],
                "linenos": rec["linenos"], "status": rec["status"],
            })
            row["n_tiles"] += n
            cv = per_conv.setdefault(conv, {"n_tiles": 0, "n_shapes": 0,
                                            "sums": {f: 0 for f in SUM_FIELDS}})
            cv["n_tiles"] += n
            for f in SUM_FIELDS:
                v = rec["values"].get(f)
                if isinstance(v, int):
                    cv["sums"][f] += v * n

    for conv, cv in per_conv.items():
        cv["n_shapes"] = sum(1 for r in per_shape.values() if r["conv"] == conv)

    totals = {f: 0 for f in SUM_FIELDS}
    for r in per_shape.values():
        for f in SUM_FIELDS:
            v = r["per_tile"].get(f)
            if isinstance(v, int):
                totals[f] += v * r["n_tiles"]

    n_tiles_counted = sum(r["n_tiles"] for r in per_shape.values())
    n_tiles_excluded = sum(n for _, _, n, _, _, _ in excluded)
    n_tiles_unmatched = sum(n for _, _, n in unmatched)

    # --- report -----------------------------------------------------------------
    out = sys.stdout
    out.write("=== SOURCES ===\n")
    out.write("program : %s\n" % program)
    out.write("jobs    : %s (%d job directories, %d with run.log)\n"
              % (jobs_dir, len(jobs), sum(1 for j in jobs if j["values"])))
    out.write("fields summed per tile: %d ([EVAL] in each job's run.log)\n" % len(SUM_FIELDS))
    out.write("\n=== COVERAGE ===\n")
    out.write("convs in program.json   : %d (non-conv steps: %d)\n"
              % (n_steps_conv, len(non_conv_steps)))
    out.write("tiles in program.json   : %d\n" % n_tiles_total)
    out.write("  tiles summed          : %d (%d shapes)\n" % (n_tiles_counted, len(per_shape)))
    out.write("  tiles set aside       : %d\n" % n_tiles_excluded)
    for conv, k, n, job, reason, vals in excluded:
        out.write("    - %s n_tiles=%d job=%s  reason: %s\n" % (conv, n, job, reason))
        out.write("      would add processing=%s / total=%s -- NOT used\n"
                  % ("{:,}".format(vals.get("processing_cycles", 0) * n).replace(",", " "),
                     "{:,}".format(vals.get("total_cycles", 0) * n).replace(",", " ")))
    out.write("  tiles without a job   : %d\n" % n_tiles_unmatched)
    if unmatched:
        for conv, k, n in unmatched[:20]:
            out.write("    ! %s n_tiles=%d key=%s\n" % (conv, n, k[1:]))
        if len(unmatched) > 20:
            out.write("    ... %d more lines\n" % (len(unmatched) - 20))
    if dup_conflicts:
        out.write("  ! %d keys have >1 job AND the measurements DIFFER (they should match):\n"
                  % len(dup_conflicts))
        for k, a, b, diff in dup_conflicts[:10]:
            out.write("    %s vs %s differ in: %s\n" % (a, b, ",".join(diff)))
    if jobs_no_log:
        out.write("  (skipped %d jobs without sim/run.log: %s)\n"
                  % (len(jobs_no_log), ",".join(jobs_no_log[:5])))

    out.write("\n=== NETWORK TOTALS (sequential sum over tiles, off-core ops NOT included) ===\n")
    for f in SUM_FIELDS:
        out.write("%-28s %s\n" % (f, "{:,}".format(totals[f]).replace(",", " ")))

    rc = 0
    if args.expect_busy is not None:
        got = totals["processing_cycles"]
        ok = got == args.expect_busy
        out.write("\nCHECK core busy (processing_cycles): %s  got=%d expect=%d  diff=%+d\n"
                  % ("MATCH" if ok else "MISMATCH", got, args.expect_busy,
                     got - args.expect_busy))
        rc |= 0 if ok else 1
    if args.expect_total is not None:
        got = totals["total_cycles"]
        ok = got == args.expect_total
        out.write("CHECK total_cycles                 : %s  got=%d expect=%d  diff=%+d\n"
                  % ("MATCH" if ok else "MISMATCH", got, args.expect_total,
                     got - args.expect_total))
        rc |= 0 if ok else 2

    if args.out_dir:
        od = os.path.expanduser(args.out_dir)
        os.makedirs(od, exist_ok=True)

        with open(os.path.join(od, "per_shape.csv"), "w", newline="") as fh:
            w = csv.writer(fh)
            w.writerow(["conv"] + list(KEY_FIELDS) + ["job", "n_tiles", "status", "run_log"]
                       + [f + "_per_tile" for f in SUM_FIELDS]
                       + [f + "_total" for f in SUM_FIELDS])
            for r in sorted(per_shape.values(),
                            key=lambda x: -(x["per_tile"].get("processing_cycles", 0) * x["n_tiles"])):
                pt = [r["per_tile"].get(f, "") for f in SUM_FIELDS]
                tot = [(r["per_tile"].get(f, 0) * r["n_tiles"])
                       if isinstance(r["per_tile"].get(f), int) else "" for f in SUM_FIELDS]
                w.writerow([r["conv"]] + list(r["key"][1:])
                           + [r["job"], r["n_tiles"], r["status"] or "", r["run_log"] or ""]
                           + pt + tot)

        with open(os.path.join(od, "per_conv.csv"), "w", newline="") as fh:
            w = csv.writer(fh)
            w.writerow(["conv", "n_shapes", "n_tiles"] + SUM_FIELDS)
            for conv, cv in sorted(per_conv.items(),
                                   key=lambda kv: -kv[1]["sums"]["processing_cycles"]):
                w.writerow([conv, cv["n_shapes"], cv["n_tiles"]]
                           + [cv["sums"][f] for f in SUM_FIELDS])

        prov = {}
        for r in per_shape.values():
            prov[r["job"]] = {
                "run_log": r["run_log"],
                "lines": {f: r["linenos"].get(f) for f in ("processing_cycles", "total_cycles",
                                                           "core_exec_cycles", "transfer_cycles")},
                "status": r["status"],
            }
        doc = OrderedDict()
        doc["source"] = {"program": program, "jobs_dir": jobs_dir,
                         "measured_by": "sauria_model (run.log [EVAL])"}
        doc["coverage"] = {"convs": n_steps_conv, "tiles_in_program": n_tiles_total,
                           "tiles_counted": n_tiles_counted, "shapes_counted": len(per_shape),
                           "tiles_excluded": n_tiles_excluded,
                           "excluded_jobs_manual": sorted(excluded_jobs),
                           "tiles_unmatched": n_tiles_unmatched,
                           "non_conv_steps": len(non_conv_steps)}
        doc["totals"] = totals
        doc["excluded"] = [{"conv": c, "job": j, "n_tiles": n, "reason": r,
                            "processing_cycles_if_counted": v.get("processing_cycles", 0) * n,
                            "total_cycles_if_counted": v.get("total_cycles", 0) * n}
                           for c, k, n, j, r, v in excluded]
        doc["composition_rule"] = ("per-shape characterization x tile count, sequential sum; "
                                   "excludes OBP / OBP configuration load / network-level DMA / host ops")
        doc["provenance"] = prov
        with open(os.path.join(od, "network_totals.json"), "w") as fh:
            json.dump(doc, fh, indent=1)
        out.write("\nWritten: %s/{per_shape.csv,per_conv.csv,network_totals.json}\n" % od)

    return rc


if __name__ == "__main__":
    sys.exit(main())
