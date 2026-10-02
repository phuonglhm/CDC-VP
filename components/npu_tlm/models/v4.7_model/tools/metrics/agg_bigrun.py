#!/usr/bin/env python3
"""Roll up the BIG-RUN OUTPUT (metrics_tiles.csv, written by the tb_fe_core_net_metrics.patch) into layer/network-level metrics.

Runs automatically, no manual work. The formulas are the ones of the verified SRAM-DSE results (never redefined here). Every number carries EXACTLY ONE label, MODE A (measured through the core port) or MODE B (filled in
from sauria_model's shape table) -- never mixed in one cell.

SAFETY GATE: if `busy` = 0 on every tile => the FSM histogram is DISABLED (rtl_ref_defaults.h:49-50) => the
run is BLANK. This script refuses with exit code 2 and states the cause plainly, instead of emitting an empty table.

usage:
  python agg_bigrun.py --csv metrics_tiles.csv --program program.json [--shapes per_shape.csv] [--out-dir DIR]
"""
import argparse
import csv
import json
import os
import sys
from collections import OrderedDict, defaultdict

F_GHZ = 0.8          # DmaTimingParams::freq_ghz
PES = 1024           # 32 x 32


def g(n):
    return "{:,}".format(int(round(n))).replace(",", " ")


def read_metrics(path):
    """Read a CSV that may come from an interrupted/resumed run: several `# meta` lines, a truncated tail line, duplicate tiles.
    Returns (last meta, list of metas per launch, header, deduplicated rows, stats on bad/duplicate lines)."""
    metas, rows, header = [], [], None
    bad = 0
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            line = line.rstrip("\n")
            if not line:
                continue
            if line.startswith("#"):
                m = {}
                for kv in line.lstrip("# ").split(",")[1:]:
                    if "=" in kv:
                        k, v = kv.split("=", 1)
                        m[k.strip()] = v.strip()
                metas.append(m)
                continue
            if header is None:
                header = line.split(",")
                continue
            parts = line.split(",")
            if len(parts) != len(header):       # a line truncated by a mid-run interruption
                bad += 1
                continue
            rows.append(dict(zip(header, parts)))
    seen, dedup = {}, 0
    for r in rows:                              # a resumed run may re-simulate an old tile: keep the LAST copy, don't double-count
        k = (r["step"], r["tile"])
        if k in seen:
            dedup += 1
        seen[k] = r
    rows = list(seen.values())
    meta = metas[-1] if metas else {}
    return meta, metas, header, rows, {"bad_lines": bad, "duplicate_tiles": dedup}


def key_of(conv, tl):
    return (conv, tl["c"][1] - tl["c"][0], tl["oy"][1] - tl["oy"][0], tl["ox"][1] - tl["ox"][0],
            tl["iy"][1] - tl["iy"][0], tl["ix"][1] - tl["ix"][0], tl["x_used"], tl["y_used"])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--csv", required=True)
    ap.add_argument("--program", required=True)
    ap.add_argument("--shapes", default=None, help="per_shape.csv (sauria_model): used to cross-check and fill in MODE B")
    ap.add_argument("--fill", action="store_true", help="fill in unmeasured tiles from the shape table (MODE B, sauria_model source)")
    ap.add_argument("--out-dir", default=None)
    args = ap.parse_args()

    meta, metas, header, rows, rstat = read_metrics(args.csv)
    prog = json.load(open(args.program))
    steps = prog["steps"]
    O = sys.stdout
    problems = []
    if len({m.get("fifo") for m in metas}) > 1:
        problems.append("FIFO depth CHANGED between launches: %s -- cannot be rolled up together" % sorted({m.get("fifo") for m in metas}))
    if rstat["bad_lines"]:
        problems.append("%d truncated/corrupt CSV line(s) (run interrupted mid-write) were skipped" % rstat["bad_lines"])
    if rstat["duplicate_tiles"]:
        problems.append("%d tile(s) appeared more than once (resumed run) -- kept the last copy, NOT double-counted" % rstat["duplicate_tiles"])

    # ---------------------------------------------------------------- 0. safety gate
    if not rows:
        O.write("!! The CSV has no tile rows -- this run has not written anything yet.\n")
        return 2
    n = len(rows)
    busy_total = sum(int(r["busy"]) for r in rows)
    if busy_total == 0:
        O.write("!! `busy` = 0 on ALL %d tiles => the FSM histogram is DISABLED (rtl_ref_defaults.h:49-50, unconditional #define).\n"
                "!! This run did NOT record core busy. Apply `rtl_ref_defaults_perf.patch` and re-run. ABORTING, no table produced.\n" % n)
        return 2

    # ---------------------------------------------------------------- 1. attach conv name + shape from program.json
    tiles = []
    for r in rows:
        si, ti = int(r["step"]), int(r["tile"])
        st = steps[si] if 0 <= si < len(steps) else None
        if st is None or st.get("kind") != "conv" or ti >= len(st["tiles"]):
            problems.append("step %s tile %s does not match program.json" % (r["step"], r["tile"]))
            continue
        tl = st["tiles"][ti]
        shape_ok = (int(r["nch"]) == tl["c"][1] - tl["c"][0] and int(r["ht"]) == tl["oy"][1] - tl["oy"][0]
                    and int(r["wt"]) == tl["ox"][1] - tl["ox"][0] and int(r["yu"]) == tl["y_used"])
        if not shape_ok:
            problems.append("step %s tile %s: CSV shape != program.json" % (r["step"], r["tile"]))
        rec = {k: (int(v) if v.lstrip("-").isdigit() else v) for k, v in r.items()}
        rec["conv"] = st["job"]
        rec["key"] = key_of(st["job"], tl)
        tiles.append(rec)

    # ---------------------------------------------------------------- 2. automatic per-tile self-consistency checks
    viol = defaultdict(list)
    for t in tiles:
        tag = "%s#%d" % (t["conv"], t["tile"])
        if not (t["exec"] <= t["busy"] <= t["ticks_all"]):
            viol["exec<=busy<=ticks"].append(tag)
        if t["pe_cycles"] != t["exec"] * PES:
            viol["pe_cycles==exec*1024"].append(tag)
        if t["c_wr_bytes"] != 4 * t["nch"] * t["ht"] * t["wt"]:
            viol["c_wr_bytes==4*elems"].append(tag)
        if t["mac_nz"] > t["macs_theory"]:
            viol["mac_nz<=macs_theory"].append(tag)
        sst = sum(t["st%02d" % q] for q in range(1, 25))
        if sst != t["busy"]:
            viol["sum(states)==busy"].append(tag)
        if t["ok"] != 1:
            viol["tile ok (core did not deadlock/timeout)"].append(tag)

    # ---------------------------------------------------------------- 3. roll up MODE A per conv
    per_conv = OrderedDict()
    for t in tiles:
        c = per_conv.setdefault(t["conv"], defaultdict(int))
        c["tiles"] += 1
        for k in ("busy", "exec", "macs_theory", "mac_nz", "a_rd_bytes", "b_rd_bytes", "c_rd_bytes", "c_wr_bytes", "ticks_all"):
            c[k] += t[k]

    def derived(busy, execc, macs):
        return {
            "stall": busy - execc,
            "pe_util": macs / (busy * PES) if busy else 0.0,
            "core_gops": 2.0 * macs * F_GHZ / busy if busy else 0.0,   # 2*MAC / (busy / f)
        }

    A = defaultdict(int)
    for c in per_conv.values():
        for k, v in c.items():
            A[k] += v
    dA = derived(A["busy"], A["exec"], A["macs_theory"])

    # ---------------------------------------------------------------- 4. compare + fill in MODE B from the shape table (sauria_model)
    shapes, cmp_rows, fill = {}, [], None
    if args.shapes:
        for r in csv.DictReader(open(args.shapes, encoding="utf-8")):
            k = (r["conv"], int(r["dc"]), int(r["doy"]), int(r["dox"]), int(r["diy"]), int(r["dix"]),
                 int(r["x_used"]), int(r["y_used"]))
            shapes[k] = r
        for t in tiles:
            ref = shapes.get(t["key"])
            if ref is None:
                continue
            rb, re_ = int(float(ref["processing_cycles_per_tile"])), int(float(ref["core_exec_cycles_per_tile"]))
            cmp_rows.append((t["conv"], t["tile"], t["busy"], rb, t["exec"], re_))
    if args.fill and shapes:
        seen = {(t["step"], t["tile"]) for t in tiles}
        B = defaultdict(int)
        miss = 0
        for si, st in enumerate(steps):
            if st.get("kind") != "conv":
                continue
            for ti, tl in enumerate(st["tiles"]):
                if (si, ti) in seen:
                    continue
                ref = shapes.get(key_of(st["job"], tl))
                if ref is None:
                    miss += 1
                    continue
                B["tiles"] += 1
                B["busy"] += int(float(ref["processing_cycles_per_tile"]))
                B["exec"] += int(float(ref["core_exec_cycles_per_tile"]))
        fill = dict(B, unmatched=miss)

    total_tiles = sum(len(s["tiles"]) for s in steps if s.get("kind") == "conv")

    # ---------------------------------------------------------------- 5. report
    O.write("# Big-run report -- automatic rollup (agg_bigrun.py)\n\n")
    O.write("Source: `%s` | counting window: **%s** | this run's FIFO depth: **%s** | SRAM backdoor: %s\n\n"
            % (args.csv, meta.get("window", "?"), meta.get("fifo", "?"), meta.get("backdoor", "?")))
    O.write("## 1. Coverage\n\n")
    O.write("- Tiles that ran through the core port: **%d / %d** (%.2f%%) -- **MODE A (MEASURED)**\n" % (len(tiles), total_tiles, 100.0 * len(tiles) / total_tiles))
    O.write("- Conv layers with a measured tile: %d (%s)\n" % (len(per_conv), ", ".join(per_conv.keys())[:120]))
    if fill:
        O.write("- Remaining tiles filled in from the shape table: %d (no match in the table: %d) -- **MODE B (from `sauria_model`, NOT the port)**\n"
                % (fill["tiles"], fill["unmatched"]))
    O.write("\n## 2. Network-level metrics -- MODE A (MEASURED through the core port)\n\n")
    O.write("| Metric | Value | Formula / source |\n|---|---:|---|\n")
    O.write("| Core busy | **%s** | sum of FSM states minus IDLE (`ctrl_state_cycles`) |\n" % g(A["busy"]))
    O.write("| Exec cycles | %s | `exec_cycles` (sa_array) |\n" % g(A["exec"]))
    O.write("| Pipeline stall cycles | %s | busy - exec |\n" % g(dA["stall"]))
    O.write("| Total MACs (theoretical) | %s | sum of nch*ht*wt*K (verified_results §2) |\n" % g(A["macs_theory"]))
    O.write("| PE utilization | **%.2f%%** | Total MACs / (busy x 1024) |\n" % (100.0 * dA["pe_util"]))
    O.write("| Core GOPS @0.8 GHz | %.1f | 2*MAC / (busy / f) |\n" % dA["core_gops"])
    O.write("| MACs with both operands != 0 | %s (%.1f%% of theoretical) | `mac_ops` -- **DIFFERENT** from the PE-utilization definition, reference only |\n"
            % (g(A["mac_nz"]), 100.0 * A["mac_nz"] / A["macs_theory"] if A["macs_theory"] else 0))
    O.write("| SRAM-A reads / SRAM-B reads | %s B / %s B | `srama/sramb_read_bytes`, window = one tile call |\n" % (g(A["a_rd_bytes"]), g(A["b_rd_bytes"])))
    O.write("| SRAM-C writes / reads | %s B / %s B | `sramc_write/read_bytes` |\n" % (g(A["c_wr_bytes"]), g(A["c_rd_bytes"])))
    if fill:
        tb, te = A["busy"] + fill["busy"], A["exec"] + fill["exec"]
        O.write("\n## 3. A + B combined (full network) -- SEPARATE labels, cells never blended\n\n")
        O.write("| | Core busy | Share |\n|---|---:|---:|\n")
        O.write("| MODE A (measured, port) | %s | %.1f%% |\n" % (g(A["busy"]), 100.0 * A["busy"] / tb))
        O.write("| MODE B (filled in from `sauria_model`) | %s | %.1f%% |\n" % (g(fill["busy"]), 100.0 * fill["busy"] / tb))
        O.write("| **Total** | **%s** | 100%% |\n" % g(tb))
        O.write("\n**Bottom line (plan §3.5)**: %.1f%% of core busy is MEASURED through the port, %.1f%% is filled in from `sauria_model`'s shape table.\n"
                % (100.0 * A["busy"] / tb, 100.0 * fill["busy"] / tb))
    O.write("\n## 4. FSM state distribution (verified_results §6)\n\n| State | Cycles | % of busy |\n|---|---:|---:|\n")
    names = ["START_FLAGS", "ARRAY_PREP", "ARRAY_FILL", "FIRST_SHIFT", "START_COMP", "DRAIN_FEED", "ARRAY_FLUSH", "WAIT_CSWITCH",
             "WAIT_CSWITCH_STALL", "WAIT_OBUF", "WAIT_OBUF_STALL", "SCND_SHIFT", "SCND_SHIFT_STALL", "ALL_BUSY_SHIFT", "ALL_BUSY",
             "ARRAY_BUSY", "OBUF_BUSY_SHIFT", "FORCE_STALL", "OBUF_BUSY", "ARRAY_CSWITCH", "ARRAY_CSWITCH_STALL", "LAST_SHIFT",
             "LAST_WAIT", "DONE"]
    for q in range(1, 25):
        v = sum(t["st%02d" % q] for t in tiles)
        if v:
            O.write("| s%02d %s | %s | %.1f%% |\n" % (q, names[q - 1], g(v), 100.0 * v / A["busy"]))
    O.write("\n## 5. Automatic per-tile self-consistency checks\n\n")
    for name in ("exec<=busy<=ticks", "pe_cycles==exec*1024", "c_wr_bytes==4*elems", "mac_nz<=macs_theory", "sum(states)==busy",
                 "tile ok (core did not deadlock/timeout)"):
        v = viol.get(name, [])
        O.write("- %s: **%s**%s\n" % (name, "PASS" if not v else "MISMATCH on %d tile(s)" % len(v), "" if not v else " -- e.g. %s" % ", ".join(v[:3])))
    for p in problems:
        O.write("- !! %s\n" % p)
    if cmp_rows:
        eq_b = sum(1 for c in cmp_rows if c[2] == c[3])
        eq_e = sum(1 for c in cmp_rows if c[4] == c[5])
        O.write("\n## 6. Comparison against sauria_model's shape table (PRELIMINARY -- not the formal Tier 2 yet)\n\n")
        O.write("Tiles whose shape is in the table: %d. busy matches exactly: **%d/%d**, exec matches exactly: **%d/%d**. "
                "Port FIFO depth = %s; sauria_model's table ran the default FIFO (assumed 5/4, **not checked in this run**).\n\n"
                % (len(cmp_rows), eq_b, len(cmp_rows), eq_e, len(cmp_rows), meta.get("fifo", "?")))
        O.write("| conv | tile | busy (port) | busy (sauria) | exec (port) | exec (sauria) |\n|---|---:|---:|---:|---:|---:|\n")
        for c in cmp_rows[:20]:
            O.write("| %s | %d | %s | %s | %s | %s |\n" % (c[0], c[1], g(c[2]), g(c[3]), g(c[4]), g(c[5])))
    O.write("\n## 7. Not in this CSV (must be combined from other sources)\n\n"
            "- Total cycles / DMA exposure / transfer: the `apply_burst_dma` model (`e2e_compose.py`) -- MODE B.\n"
            "- OBP: `vector + 6*context` (context count, checked against 2 mappings) or `tb_fe_network`'s own counter -- see `e2e3_compose_table.py`.\n"
            "- Host ops: range [0 .. 3,785,875] (`host_ops_cycles.py`) -- an estimate.\n"
            "- 6 feeder counters (`act/wei_feed/stall`, `*_l1_read_words`): **always 0** (inherited from sauria_model) -- not used in this report.\n")

    if args.out_dir:
        od = args.out_dir
        os.makedirs(od, exist_ok=True)
        with open(os.path.join(od, "bigrun_layers.csv"), "w", newline="") as fh:
            w = csv.writer(fh)
            w.writerow(["conv", "tiles_measured", "busy", "exec", "stall", "macs_theory", "mac_nz", "pe_util_pct", "core_gops"])
            for cv, c in per_conv.items():
                d = derived(c["busy"], c["exec"], c["macs_theory"])
                w.writerow([cv, c["tiles"], c["busy"], c["exec"], d["stall"], c["macs_theory"], c["mac_nz"],
                            "%.2f" % (100 * d["pe_util"]), "%.1f" % d["core_gops"]])
        json.dump({"meta": meta, "tiles_measured": len(tiles), "tiles_total": total_tiles, "mode_A": dict(A), "derived_A": dA,
                   "mode_B_fill": fill, "violations": {k: len(v) for k, v in viol.items()}, "problems": problems},
                  open(os.path.join(od, "bigrun_totals.json"), "w"), indent=1)
    return 1 if (viol or problems) else 0


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    sys.exit(main())
