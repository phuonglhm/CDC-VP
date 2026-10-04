#!/usr/bin/env python3
"""Standalone SW-EVAL (MODE B), step 1/2: build the two flat tables that `sw_eval` (C++, no SystemC, no testbench) reads.

  shape_table.csv  -- one row per tile SHAPE: shape key + the raw [EVAL] fields of the one job measured for that shape
                      (cycles depend only on SHAPE: two tiles of the same shape measure identically) + ncontexts/mvm_k/out_elems/A_bytes/B_bytes.
  layer_tiles.csv  -- one row per TILE of the program: layer, tile index, shape_id (-1 = not in the table -> REPORTED, never silently dropped).
  layer_table.csv  -- one row per layer: cin, cout, K, h_in, w_in, DRAM footprint (estimate, does not deduplicate overlap across tiles).
  compat_tiles.csv -- (--compat) each shape job = one 1-tile layer, to cross-check every cell against `sauria_model`'s own 9 CSVs.

Source: `program.json` (the network's tile plan) + job directories `<conv>__tNNNN/{job.json, sim/run.log}` already MEASURED by `sauria_model`.
Runs NO simulation. Every number in shape_table.csv is read straight from the `[EVAL] k=v` lines of run.log (the source line is kept in `src_line`).

usage: python sw_eval_prep.py --program data/program.json --jobs-dir data/jobs --out-dir out/prep [--exclude-job NAME ...]
"""
import argparse
import csv
import json
import os
import re
import sys
from collections import OrderedDict

EVAL_INT = [
    "total_cycles", "processing_cycles", "core_exec_cycles", "transfer_cycles", "transfer_overhead_cycles",
    "internal_transfer_cycles", "mac_engine_cycles", "dma_engine_cycles", "dma_stall_cycles", "wait_input_cycles",
    "wait_output_cycles", "ddr_read_bytes", "ddr_write_bytes", "weight_bytes", "l2_to_l1_bytes", "l1_to_l2_bytes",
    "theory_min_cycles", "active_cycles", "idle_cycles", "dma_read_cycles", "dma_write_cycles", "dma_busy_cycles",
    "dma_idle_cycles", "dma_wait_cycles", "mac_active_cycles", "mac_idle_cycles", "pe_active_cycles", "pe_idle_cycles",
    "engine_active_cycles", "engine_idle_cycles", "bias_bytes", "l1_read_bytes", "l1_write_bytes", "l2_read_bytes",
    "l2_write_bytes", "l2_footprint_bytes", "ddr_footprint_control_bytes",
]
KEY_FIELDS = ["dc", "doy", "dox", "diy", "dix", "x_used", "y_used"]


def tile_key(conv, tl):
    return (conv, tl["c"][1] - tl["c"][0], tl["oy"][1] - tl["oy"][0], tl["ox"][1] - tl["ox"][0],
            tl["iy"][1] - tl["iy"][0], tl["ix"][1] - tl["ix"][0], tl["x_used"], tl["y_used"])


def parse_log(path):
    vals, lines, status, wl = {}, {}, None, None
    with open(path, errors="replace") as fh:
        for i, line in enumerate(fh, 1):
            line = line.rstrip("\n")
            if line.startswith("[EVAL] ") and "=" in line:
                k, v = line[7:].split("=", 1)
                if k in EVAL_INT:
                    vals[k] = int(v)
                    lines[k] = i
            elif " STATUS " in line and ":" in line:
                status = line.split(":", 1)[1].strip()
            elif " WORKLOAD " in line:
                wl = line
    if wl is None:
        raise SystemExit("run.log has no WORKLOAD line: %s" % path)
    m = re.search(r"n_tiles=(\d+) ncontexts=(\d+) mvm_k=(\d+) \| (\d+) output elements", wl)
    if not m:
        raise SystemExit("could not parse WORKLOAD: %r" % wl)
    return vals, lines, status, dict(n_tiles=int(m.group(1)), ncontexts=int(m.group(2)),
                                     mvm_k=int(m.group(3)), out_elems=int(m.group(4)))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--program", required=True)
    ap.add_argument("--jobs-dir", required=True)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--exclude-job", action="append", default=[])
    ap.add_argument("--compat", action="store_true", help="also write compat_tiles.csv (each job = one 1-tile layer)")
    a = ap.parse_args()
    os.makedirs(a.out_dir, exist_ok=True)
    prog = json.load(open(a.program))

    # ---- 1. job -> shape key ----
    by_key = OrderedDict()
    skipped = []
    jobs = []
    for name in sorted(os.listdir(a.jobs_dir)):
        jf = os.path.join(a.jobs_dir, name, "job.json")
        rl = os.path.join(a.jobs_dir, name, "sim", "run.log")
        if not (os.path.isfile(jf) and os.path.isfile(rl)):
            continue
        meta = json.load(open(jf))
        vals, lines, status, wl = parse_log(rl)
        ok = status is not None and status.startswith("PASS")
        rec = dict(name=name, conv=meta["conv"], tile=meta["tile"], vals=vals, lines=lines, wl=wl,
                   A_bytes=int(meta.get("A_bytes", 0)), B_bytes=int(meta.get("B_bytes", 0)), status=status, ok=ok,
                   key=tile_key(meta["conv"], meta["tile"]))
        jobs.append(rec)
        if not ok or name in a.exclude_job:
            skipped.append((name, status or "no STATUS line"))
            continue
        missing = [f for f in EVAL_INT if f not in vals]
        if missing:
            raise SystemExit("job %s is missing [EVAL] field(s): %s" % (name, missing))
        by_key.setdefault(rec["key"], rec)     # keep the first job (stable name order), as agg_network_metrics.py

    shape_id = {}
    with open(os.path.join(a.out_dir, "shape_table.csv"), "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["shape_id", "conv"] + KEY_FIELDS + ["job", "ncontexts", "mvm_k", "out_elems", "A_bytes", "B_bytes",
                                                     "src_line_first_eval"] + EVAL_INT)
        for i, (k, r) in enumerate(by_key.items()):
            shape_id[k] = i
            w.writerow([i, k[0]] + list(k[1:]) + [r["name"], r["wl"]["ncontexts"], r["wl"]["mvm_k"], r["wl"]["out_elems"],
                                                  r["A_bytes"], r["B_bytes"], min(r["lines"].values())]
                       + [r["vals"][f] for f in EVAL_INT])

    # ---- 2. program: every tile of every conv -> shape_id ----
    n_tiles, unmatched, layers = 0, OrderedDict(), []
    with open(os.path.join(a.out_dir, "layer_tiles.csv"), "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["layer_id", "layer_name", "op_id", "tile", "shape_id", "n_contexts_step", "step"])
        lid = 0
        for si, st in enumerate(prog["steps"]):
            if st.get("kind") != "conv":
                continue
            lid += 1
            layers.append((lid, st["job"], si, len(st["tiles"])))
            for ti, tl in enumerate(st["tiles"]):
                k = tile_key(st["job"], tl)
                sid = shape_id.get(k, -1)
                if sid < 0:
                    unmatched[k] = unmatched.get(k, 0) + 1
                w.writerow([lid, st["job"], st["op_id"], ti, sid, st.get("n_contexts", ""), si])
                n_tiles += 1

    # ---- 3. per-LAYER DRAM footprint (estimate): input = cin*Hmax*Wmax from the tile plan, weight = cout*K ----
    #   cin from the job's A_bytes (= diy*dix*cin, divisibility checked); Hmax/Wmax = largest upper edge of the input windows (clipped at 0,
    #   negative padding not counted). Trailing padding (if any) is still counted => a slight upper bound. Replaces the "sum of per-tile
    #   images" (which double-counts halo and weights repeated per spatial tile).
    with open(os.path.join(a.out_dir, "layer_table.csv"), "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["layer_id", "layer_name", "cin", "cout", "K", "h_in", "w_in", "data_bytes", "weight_bytes"])
        lid = 0
        for st in prog["steps"]:
            if st.get("kind") != "conv":
                continue
            lid += 1
            cin = K = None
            for tl in st["tiles"]:
                r = by_key.get(tile_key(st["job"], tl))
                if r is None:
                    continue
                area = (tl["iy"][1] - tl["iy"][0]) * (tl["ix"][1] - tl["ix"][0])
                if area <= 0 or r["A_bytes"] % area:
                    raise SystemExit("A_bytes %d does not divide evenly by input area %d (%s)" % (r["A_bytes"], area, r["name"]))
                cin, K = r["A_bytes"] // area, r["wl"]["mvm_k"]
                break
            if cin is None:
                w.writerow([lid, st["job"], "", "", "", "", "", "", ""])
                continue
            cout = max(tl["c"][1] for tl in st["tiles"])
            h = max(max(tl["iy"][1], 0) for tl in st["tiles"])
            wd = max(max(tl["ix"][1], 0) for tl in st["tiles"])
            w.writerow([lid, st["job"], cin, cout, K, h, wd, cin * h * wd, cout * K])

    if a.compat:
        with open(os.path.join(a.out_dir, "compat_tiles.csv"), "w", newline="") as fh:
            w = csv.writer(fh)
            w.writerow(["layer_id", "layer_name", "op_id", "tile", "shape_id", "n_contexts_step", "step"])
            for i, (k, r) in enumerate(by_key.items()):
                w.writerow([i + 1, r["name"], 0, 0, shape_id[k], r["wl"]["ncontexts"], -1])

    with open(os.path.join(a.out_dir, "prep_report.txt"), "w", encoding="utf-8") as fh:
        fh.write("prep (MODE B) -- does NOT run any simulation; reads sauria_model's `run.log`\n")
        fh.write("jobs read: %d | usable PASSing shapes: %d | jobs excluded: %d\n" % (len(jobs), len(by_key), len(skipped)))
        for n, s in skipped:
            fh.write("  EXCLUDED %s : %s\n" % (n, s))
        fh.write("conv layers: %d | tiles in program: %d | tiles with no shape in the table: %d\n"
                 % (len(layers), n_tiles, sum(unmatched.values())))
        for k, n in unmatched.items():
            fh.write("  NO MATCH %s x%d\n" % (k, n))
    sys.stdout.write(open(os.path.join(a.out_dir, "prep_report.txt"), encoding="utf-8").read())
    return 0


if __name__ == "__main__":
    sys.exit(main())
