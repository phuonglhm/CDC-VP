#!/usr/bin/env python3
"""[S4-ME] Build the TABLE FOR THE USER from a smoke run: which columns are MEASURED vs. FILLED-IN, a per-layer table to spot poorly tiled layers, and an UNCERTAIN section.

Reads: metrics_tiles.csv (MODE A, through the core port), <out>/nine_csv/{coverage.csv,parameters.json}, out/sensitivity.json,
     data/program.json + data/graph_ir.json, out/prep/layer_tiles.csv, (optional) out/tile_sweep_nox/sweep_by_conv.csv (MODE B prediction).
Runs no simulation. Every number carries one label: MEASURED (MODE A) or PREDICTED/FILLED-IN (MODE B).

usage: python smoke_table.py --csv metrics_tiles.csv --nine out/smoke/nine_csv --out out/smoke/smoke_table.md
"""
import argparse
import csv
import json
import os
import sys
from collections import Counter, OrderedDict, defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "metrics"))
from agg_bigrun import read_metrics  # noqa: E402

PES = 1024


def fm(x):
    return "{:,}".format(int(round(x))).replace(",", " ")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", required=True)
    ap.add_argument("--nine", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--prep", default=os.path.join(HERE, "out", "prep"))
    ap.add_argument("--program", default=os.path.join(HERE, "data", "program.json"))
    ap.add_argument("--graph-ir", default=os.path.join(HERE, "data", "graph_ir.json"))
    ap.add_argument("--sens", default=os.path.join(HERE, "out", "sensitivity.json"))
    ap.add_argument("--sweep", default=os.path.join(HERE, "out", "tile_sweep_nox", "sweep_by_conv.csv"))
    a = ap.parse_args()

    meta, metas, header, rows, rstat = read_metrics(a.csv)
    prog = json.load(open(a.program))
    ops = {o["job"]: o for o in json.load(open(a.graph_ir))["ops"] if o["op"] == "conv"}
    sens = json.load(open(a.sens))
    par = json.load(open(os.path.join(a.nine, "parameters.json")))
    cov = {r["layer_name"]: r for r in csv.DictReader(open(os.path.join(a.nine, "coverage.csv"), encoding="utf-8"))}
    sweep = {}
    if os.path.isfile(a.sweep):
        sweep = {r["job"]: r for r in csv.DictReader(open(a.sweep, encoding="utf-8"))}
    steps = prog["steps"]

    # ---- group by layer ----
    by = OrderedDict()
    bad_ok = 0
    for r in rows:
        si, ti = int(r["step"]), int(r["tile"])
        st = steps[si]
        if st.get("kind") != "conv":
            continue
        if int(r["ok"]) != 1:
            bad_ok += 1
        c = by.setdefault(st["job"], defaultdict(int))
        c["tiles"] += 1
        for k in ("busy", "exec", "mac_nz", "macs_theory", "n_ctx", "ticks_all"):
            c[k] += int(r[k])
        c["st_sum_ok"] += int(sum(int(r["st%02d" % q]) for q in range(1, 25)) == int(r["busy"]))
    zero_busy = sum(1 for r in rows if int(r["busy"]) == 0)

    O = []
    w = O.append
    w("# Smoke-test table -- for the user to decide: launch the 45-hour run now, or fix `fe_tile_plan.py` first\n")
    w("**How to read this table**: every number carries ONE label. **MEASURED** = through the REAL core port (MODE A, `metrics_tiles.csv`). **FILLED-IN / PREDICTED** = from sauria_model's shape table (MODE B) or the tile-sweep model -- "
      "NOT measured in this run. Columns that are uncertain are called out plainly in Section 4.\n")
    tot_tiles = sum(int(r["tiles_with_shape"]) for r in cov.values())
    tot_meas = sum(int(r["tiles_measured_core_port"]) for r in cov.values())
    w("## 1. What this run actually measured (coverage)\n")
    w("- CSV: `%s` | counting window: `%s` | FIFO depth: **%s** | tile rows: %d (truncated lines %d, duplicate tiles %d) | tiles with `ok!=1`: %d | tiles with `busy=0`: %d%s"
      % (a.csv, meta.get("window", "?"), meta.get("fifo", "?"), len(rows), rstat["bad_lines"], rstat["duplicate_tiles"], bad_ok, zero_busy,
         "  **!! busy=0: the FSM histogram is disabled, this table is NOT usable**" if zero_busy else ""))
    w("- Tiles measured through the core port (MODE A): **%d / %d** tiles with a shape have a real measurement (%.2f%%); **%d layers** have at least one measured tile out of 83." % (
        tot_meas, tot_tiles, 100.0 * tot_meas / tot_tiles if tot_tiles else 0, sum(1 for r in cov.values() if int(r["tiles_measured_core_port"]) > 0)))
    full = [n for n, r in cov.items() if r["mode"] == "A"]
    part = [n for n, r in cov.items() if r["mode"] == "A+B"]
    w("- FULLY measured layers (MODE A): %d (%s) | PARTIALLY measured (A+B): %d (%s) | not measured (B): %d\n" % (
        len(full), ", ".join(full[:8]), len(part), ", ".join(part[:8]), sum(1 for r in cov.values() if r["mode"] == "B")))

    dsum = sum(int(r[k]) for r in cov.values() for k in ("abs_dbusy_up", "abs_dbusy_down", "abs_dexec_up", "abs_dexec_down"))
    w("- **Port vs. `sauria_model`'s shape table (same shape)**: over %d measured tiles, sum(|busy measured - busy table|) + sum(|exec measured - exec table|) = **%s** cycles => %s (port FIFO depth %s)."
      % (tot_meas, fm(dsum), "EXACT MATCH, CYCLE FOR CYCLE (busy and exec)" if dsum == 0 else "**MISMATCH -- see the 'vs. table' column**", meta.get("fifo", "?")))
    cs = os.path.join(os.path.dirname(a.csv), "coverage_shapes.csv")
    if os.path.isfile(cs):
        rs = list(csv.DictReader(open(cs, encoding="utf-8")))
        w("- Shapes in this run: %d shapes SPECIFIC to these layers, %d/%d with at least one tile through the core (`coverage_shapes.csv`) -- **not comparable to 66 or 150**." % (
            len(rs), sum(1 for r in rs if int(r["tiles_via_core"]) > 0), len(rs)))
    w("")
    w("## 2. The 60 metrics: which columns are MEASURED, which are FILLED-IN\n")
    d, dv, ins = [c for c in sens["sensitive"]], None, sens["insensitive"]
    direct = ["processing_cycles", "active_cycles", "mac_engine_cycles", "mac_active_cycles", "l2_to_l1_bytes", "l1_to_l2_bytes", "l1_read_bytes", "l1_write_bytes"]
    derived = [c for c in d if c not in direct]
    w("Determined by perturbation testing (multiply every tile's busy/exec/bytes by 1.1x and see which columns move; `out/sensitivity.json`), not guessed:\n")
    w("| Group | # columns | Columns | Status |\n|---|---:|---|---|")
    w("| **Directly measured** (tile ran through the core port) | %d | `%s` | MODE A on measured tiles, MODE B on the rest |" % (len(direct), "`, `".join(direct)))
    w("| **Derived** from measured columns + the DMA model | %d | `%s` | mixed (A+B) by construction |" % (len(derived), "`, `".join(derived)))
    w("| **Independent of this run's measurements** | %d | (DMA, DDR bytes, weights, footprints, host, stall, peak...) | **pure MODE B** |" % len(ins))
    w("\n=> **%d/60 columns respond to this run's measurements**, %d/60 do not. Coverage is only %.2f%% of tiles right now, so these 21 columns are still mostly MODE B until the full run (S6).\n" % (len(d), len(ins), 100.0 * tot_meas / tot_tiles if tot_tiles else 0))

    # ---- per-layer table ----
    w("## 3. Per-layer table of MEASURED layers -- spotting poorly tiled layers\n")
    w("Columns: **X/Y** = `x_used/y_used` from the tile plan (a geometric bottleneck if <32) | **n_ctx/tile** | **exec/busy** (low = the array is waiting, pipeline-stalled) | "
      "**PE util** = theoretical MACs/(busy*1024) | **mac_nz/MAC** = fraction of MACs with both operands nonzero (a DATA characteristic, not a cycle count) | "
      "**vs. shape table** = measured busy compared with the FILLED-IN (MODE B) value for the same tiles | **PREDICTED saving** = the tile-sweep model, cfg a + prefetch (MODE B **PREDICTION**).\n")
    w("| Layer | k/s | cin->cout | tiles measured/total | X/Y | n_ctx/tile | busy/tile (MEASURED) | exec/busy (MEASURED) | PE util (MEASURED) | mac_nz/MAC (MEASURED) | vs. shape table | PREDICTED saving | Tiling concern |\n|---|---|---|---:|---|---:|---:|---:|---:|---:|---:|---:|---|")
    for job, c in by.items():
        st = next(s for s in steps if s.get("kind") == "conv" and s["job"] == job)
        o = ops[job]
        n = c["tiles"]
        xs = Counter(t["x_used"] for t in st["tiles"]).most_common(1)[0][0]
        ys = Counter(t["y_used"] for t in st["tiles"]).most_common(1)[0][0]
        ex_b = c["exec"] / c["busy"] if c["busy"] else 0
        pe = c["macs_theory"] / (c["busy"] * PES) if c["busy"] else 0
        nz = c["mac_nz"] / c["macs_theory"] if c["macs_theory"] else 0
        cv = cov.get(job, {})
        dev = "-"
        if cv:
            up, dn = int(cv["abs_dbusy_up"]), int(cv["abs_dbusy_down"])
            dev = "%s / %s" % (fm(up), fm(dn)) if (up or dn) else "0"
        sv = "-"
        if job in sweep and sweep[job].get("a_pf_total"):
            base = float(sweep[job]["cur_total_seq"])
            sv = "%.0f%%" % (100.0 * (float(sweep[job]["a_pf_total"]) - base) / base)
        flags = []
        if xs < 32:
            flags.append("X=%d<32" % xs)
        if ys < 16:
            flags.append("Y=%d<16" % ys)
        if ex_b < 0.6:
            flags.append("exec/busy=%.2f" % ex_b)
        if pe < 0.15:
            flags.append("low PE util")
        w("| %s | %dx%d/s%d | %d->%d | %d/%d | %d/%d | %.1f | %s | %.2f | %.1f%% | %.1f%% | %s | %s | %s |" % (
            job, o["kh"], o["kw"], o["stride"][0], o["cin"], o["cout"], n, len(st["tiles"]), xs, ys, c["n_ctx"] / n, fm(c["busy"] / n),
            ex_b, 100 * pe, 100 * nz, dev, sv, "; ".join(flags) or "none"))
    w("\n*`vs. shape table`* = the absolute (excess / shortfall) of measured busy versus the shape table, summed over the measured tiles. **0 means an exact cycle-for-cycle match** (expected: cycles depend only on shape, E1).\n")

    # ---- uncertain ----
    w("## 4. UNCERTAIN -- stated plainly\n")
    sram_txt = "could not be computed (prep files missing)"
    try:
        sh = {int(r["shape_id"]): r for r in csv.DictReader(open(os.path.join(a.prep, "shape_table.csv"), encoding="utf-8"))}
        lt = {(int(r["step"]), int(r["tile"])): int(r["shape_id"]) for r in csv.DictReader(open(os.path.join(a.prep, "layer_tiles.csv"), encoding="utf-8"))}
        rat, dab, dwr = [], [], []
        for r in rows:
            sid = lt.get((int(r["step"]), int(r["tile"])), -1)
            if sid < 0:
                continue
            ref = int(sh[sid]["l2_to_l1_bytes"])
            rd = int(r["a_rd_bytes"]) + int(r["b_rd_bytes"])
            rat.append(rd / ref)
            dab.append(rd - ref)
            dwr.append(int(r["c_wr_bytes"]) - int(sh[sid]["l1_to_l2_bytes"]))
        sram_txt = ("over %d measured tiles: port's A+B reads / sauria's = %.3f .. %.3f (mean %.3f), absolute gap %d .. %d B/tile; C writes gap %d .. %d B (0 = exact match)"
                    % (len(rat), min(rat), max(rat), sum(rat) / len(rat), min(dab), max(dab), min(dwr), max(dwr)))
    except Exception as ex:  # noqa: BLE001
        sram_txt = "could not be computed (%s)" % ex
    for t in [
        "**SRAM bytes (`l2_to_l1/l1_to_l2/l1_read/l1_write`)**: the port's counting window is ONE tile CALL, not start-to-done, so it differs from `sauria_model`'s in-tile window: %s. Cycles MATCH, bytes do NOT -- neither window has been validated as correct." % sram_txt,
        "**DMA/analytic group** (transfer, dma_*, wait_*, total_cycles...): computed from the burst model summing tiles ONE AT A TIME (n=1), sequentially => an **UPPER BOUND**; the ideal lower bound (prefetch always possible) is in `project_metrics.csv`. The real DMA schedule has not been measured.",
        "**Scope**: 83 core conv layers. Excludes OBP (%s cycles -- this part IS counted), OBP config load, host ops, inter-layer DMA. `ips` is core-only." % "4,217,000",
        "**`mac_nz/MAC`** depends on the INPUT DATA (image 000000000009.jpg) -- do not extrapolate to other images; do NOT use it as PE utilization.",
        "**DRAM footprint** is an ESTIMATE (input = cin*Hmax*Wmax; trailing padding may be over-counted). **L1 = 4,096 B** is not verified. **Scratch** = not modeled.",
        "**The 'PREDICTED saving' column** is a MODE B prediction (a model calibrated on 149 measured shapes + 3 extra measured shapes), NOT a measurement; per-tile off-core cost is in `out/offcore_estimate.txt` (OBP reproduces the measured number exactly; the 256-word/tile CSR figure is an ASSUMPTION).",
        "This run's FIFO depth = **%s** (the `# meta` line); sauria_model's shape table ran with the default FIFO -- whether the two match has not been checked in this run." % meta.get("fifo", "?"),
    ]:
        w("- " + t)
    w("\n## 5. Accompanying files\n")
    w("- `%s/*.csv` (9 CSVs) + `parameters.json` (`mode`=%s, `tile_counts`, per-metric label) + `coverage.csv` (per layer: A / A+B / B)" % (a.nine, par.get("mode")))
    open(a.out, "w", encoding="utf-8").write("\n".join(O) + "\n")
    print("ok", a.out)
    return 0


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    sys.exit(main())
