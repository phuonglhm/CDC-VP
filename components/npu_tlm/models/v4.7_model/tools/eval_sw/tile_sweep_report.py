#!/usr/bin/env python3
"""Render the M7-ME report from `tile_sweep.py`'s results (two runs: with extrapolation X and --no-x). Read-only. MODE B PREDICTION.

usage: python tile_sweep_report.py --with-x out/tile_sweep --no-x out/tile_sweep_nox --shapes out/prep/shape_table.csv --graph-ir data/graph_ir.json --out out/tile_sweep/tile_sweep_report.md
"""
import argparse
import csv
import collections
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tile_model as tm  # noqa: E402


def g(r, k):
    v = r.get(k)
    return int(float(v)) if v not in (None, "") else None


def fm(x):
    return "{:,}".format(int(round(x))).replace(",", " ")


def load(d):
    rows = list(csv.DictReader(open(os.path.join(d, "sweep_by_conv.csv"), encoding="utf-8")))
    j = json.load(open(os.path.join(d, "sweep.json"), encoding="utf-8"))
    return rows, j


def best_key(r, keys):
    best = None
    for k in keys:
        v = g(r, k + "_total")
        if v is not None and (best is None or v < best[0]):
            best = (v, k)
    return best


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--with-x", required=True)
    ap.add_argument("--no-x", required=True)
    ap.add_argument("--shapes", required=True)
    ap.add_argument("--graph-ir", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    RX, JX = load(a.with_x)
    RN, JN = load(a.no_x)
    model = tm.ShapeModel(a.shapes, a.graph_ir)
    O = []
    w = O.append
    w("# M7-ME -- Per-layer tile-split sweep, scored by PREDICTED CYCLES (MODE B)\n")
    w("**Every number in this document is a PREDICTION from the calibrated model (MODE B), not a measurement.** Once a choice is made it must be verified with a real run. "
      "Unit is **cycles**, not converted to time (simulation time is not proportional to cycles -- Frontend: 2.2x cycles corresponded to 3.6x wall-clock time).\n")
    w("Scope: 83 core conv layers; excludes OBP, OBP config load, host ops, inter-layer DMA. Reference frame = **sequential cycle sum** (compute + DMA exposure, no prefetch), "
      "the same frame as sw_eval (249,616,773); the model gives %s for the current tile plan (differs because it must predict the 10 `det.p3.box_out` tiles that have no PASSing shape).\n"
      % fm(next(iter(JX["levers"].values()))))

    # ---- 1. model + validation ----
    w("## 1. Reliability of the cost model (`tile_model.py`)\n")
    rep = model.error_report()
    by = collections.defaultdict(list)
    for c, nch, ht, wt, fl, p, b in rep:
        if p is not None:
            by[fl].append(abs(p - b) / b)
    w("- **Calibrated on 149 sauria_model shapes** (same source as sw_eval): in cell `C` (kernel x x_used already has several points): %d shapes, mean error **%.2f%%**, max **%.1f%%** (IN-SAMPLE -- the model has only ~6 parameters, this is not an independent check)."
      % (len(by["C"]), 100 * sum(by["C"]) / len(by["C"]), 100 * max(by["C"])))
    w("- **3 shapes MEASURED on sauria_model** (predictions pre-registered in `s2_prediction.md`, never edited afterward): exec matched 3/3 (17,548/17,547, 2,571/2,571, 1,997/1,995). "
      "busy: 3x3/s1 x=32 y=32 measured **17,560** (pre-registered 23,259: +32.5%% TOO CONSERVATIVE); 1x1 x=32 y=32 measured **2,584** (pre-registered 4,885: +89%% TOO CONSERVATIVE); "
      "3x3/s1 x=32 y=16 small ht measured **2,008** (pre-registered 2,005: -0.1%%). => at x=32,y=32 the pipeline stall is ~0 for large K (max(0,225-K) per context); "
      "the model has been UPDATED accordingly (%s shapes in cell `C`, mean error %.2f%%, max %.1f%%)." % (len(by["C"]), 100 * sum(by["C"]) / len(by["C"]), 100 * max(by["C"])))
    xs = [(c, nch, ht, wt, (p - b) / b) for c, nch, ht, wt, fl, p, b in rep if fl == "X" and p is not None]
    w("- **Remaining `X` = EXTRAPOLATED cells**: only 3x3 STRIDE 2 at x=32,y=32 (%d shapes, K=432, model off by %+.1f%%) -- uses the 0.33*K coefficient at every K (may be too conservative for large K). "
      "Reported as **two bands**: `--no-x` (excludes any candidate touching an X cell) and with-X." % (len(xs), 100 * max(e for *_, e in xs)))
    w("- DMA: a regression over 149 shapes; load error <= 5.1%, store error <= 20.5% (on small tiles). Weights are assumed reloaded on EVERY tile (matches the reference frame), no cross-tile weight reuse is modeled.")
    w("- **Not yet modeled**: per-tile config-load/OBP cost, the tile-count increase (see the `tiles` column), and the physical cause of the 'pipeline stall' (a feeder-throughput limit -- sauria_model notes 'A0 not closed'); "
      "the model only describes it per calibrated cell (kernel, x_used, y_used). Outside a calibrated cell -> no prediction.\n")

    # ---- 2. levers ----
    w("## 2. Levers -- network totals (cycles, sequential frame)\n")
    w("| # | Option | With-X (extrapolated) | vs. current | Calibrated only (--no-x) | vs. current |\n|---|---|---:|---:|---:|---:|")
    keys = list(JX["levers"].keys())
    base = JX["levers"][keys[0]]
    baseN = JN["levers"][keys[0]]
    for k in keys:
        vx, vn = JX["levers"][k], JN["levers"][k]
        w("| %s | %s | %s | %+.1f%% | %s | %+.1f%% |" % (k.split(".")[0], k.split(". ", 1)[1], fm(vx), 100 * (vx - base) / base, fm(vn), 100 * (vn - baseN) / baseN))
    w("")

    # ---- 3. what's blocking ----
    w("## 3. Which constraint is blocking (per layer, scored by cycles, no prefetch, to isolate the tiling effect alone)\n")
    cnt = collections.Counter()
    wt_cyc = collections.Counter()
    lim = {}
    for r in RN:
        base_l = g(r, "cur_total_seq")
        a_seq = g(r, "a_seq_total") or base_l
        gains = {"OBJECTIVE FUNCTION (picks the fewest tiles)": base_l - a_seq}
        for tag, k in (("BANK B (weight)", "b_seq"), ("BANK A (act + skip)", "c_seq"), ("BANK C (psum)", "e_seq")):
            v = g(r, k + "_total")
            if v is not None:
                gains[tag] = a_seq - v
        v = g(r, "a_ideal_seq_total")
        if v is not None:
            gains["GEOMETRY (multi-row span, wo<32)"] = a_seq - v
        tag, gain = max(gains.items(), key=lambda kv: kv[1])
        if gain < 0.02 * base_l:
            tag = "unclear / already optimal (inherent to the layer)"
        lim[r["job"]] = (tag, gain, gains)
        cnt[tag] += 1
        wt_cyc[tag] += base_l
    w("| Main blocking constraint | # layers | current cycles of those layers | share |\n|---|---:|---:|---:|")
    tot = sum(wt_cyc.values())
    for k, v in wt_cyc.most_common():
        w("| %s | %d | %s | %.1f%% |" % (k, cnt[k], fm(v), 100 * v / tot))
    w("\n(Gain = the ADDITIONAL cycles saved by relaxing that constraint, on top of cfg a's objective-function change; the constraint with the largest gain wins, and must be >= 2% of the layer's cycles. This table only uses the --no-x band.)\n")

    # ---- 4. per-layer table ----
    w("## 4. Per-layer table (top 15 most expensive layers + the 3 'Y=4/32' layers named in the ticket). --no-x band; with-X in parentheses\n")
    w("Column 'proposed' = cfg a with prefetch (**no hardware change**, just the objective function + enabling prefetch); 'best' = the best of a/b/c/d (with/without prefetch).\n")
    w("| Layer | k/s | Cout | wo | current X/Y | tiles | current cycles | proposed X/Y (cfg a+prefetch) | predicted cycles | saving | best (cfg) | saving | blocked by |\n|---|---|---:|---:|---|---:|---:|---|---:|---:|---|---:|---|")
    RXd = {r["job"]: r for r in RX}
    named = ["dark5.conv", "neck.d5.conv", "det.p5.cls1"]
    order = sorted(RN, key=lambda r: -g(r, "cur_total_seq"))
    show = order[:15] + [r for r in order[15:] if r["job"] in named]
    allk = ["a_seq", "a_pf", "b_seq", "b_pf", "c_seq", "c_pf", "d_seq", "d_pf"]
    for r in show:
        base_l = g(r, "cur_total_seq")
        ap_ = g(r, "a_pf_total")
        bk = best_key(r, allk)
        rx = RXd[r["job"]]
        bkx = best_key(rx, allk)
        w("| %s | %sx%s/s%s | %s | %s | %s/%s | %s | %s | %s | %s | %.0f%% (%.0f%%) | %s (%s) | %.0f%% (%.0f%%) | %s |" % (
            r["job"], r["kh"], r["kh"], r["sh"], r["cout"], r["wo"], r["cur_xu"], r["cur_yu"], r["cur_tiles"], fm(base_l),
            r.get("a_pf_xy", "-"), fm(ap_) if ap_ else "-",
            100 * (ap_ - base_l) / base_l if ap_ else 0, 100 * ((g(rx, "a_pf_total") or base_l) - base_l) / base_l,
            (bk[1] if bk else "-"), (r.get(bk[1] + "_xy", "-") if bk else "-") + " " + (r.get(bk[1] + "_shape", "") if bk else ""),
            100 * (bk[0] - base_l) / base_l if bk else 0, 100 * (bkx[0] - base_l) / base_l if bkx else 0, lim[r["job"]][0]))
    w("")
    w("Full 83-layer CSV: `tools/eval_sw/out/tile_sweep/sweep_by_conv.csv` (every config: shape cout_t/wt/ht, X/Y, cycles, calibration status).\n")

    # ---- 5. the three most expensive layers ----
    w("## 5. The three 'Y=4/32' layers (dark5.conv, neck.d5.conv, det.p5.cls1) -- geometry, not SRAM\n")
    tot3 = 0
    for j in named:
        r = [x for x in RN if x["job"] == j][0]
        rx = RXd[j]
        tot3 += g(r, "cur_total_seq")
        w("- **%s** (wo=%s): currently %s X/Y, %s cycles. Lever 1 alone (just enable prefetch): %s; cfg a objective change: %s (X/Y %s, %s); "
          "cfg b (2 weight banks -> **X=32**): %s (X/Y %s); span-ideal + cfg b: %s (X/Y %s, uses an X cell)."
          % (j, r["wo"], "%s/%s" % (r["cur_xu"], r["cur_yu"]), fm(g(r, "cur_total_seq")), fm(g(r, "cur_total_pf")),
             fm(g(r, "a_seq_total")), r.get("a_seq_xy"), r.get("a_seq_shape"),
             fm(g(r, "b_pf_total")), r.get("b_pf_xy"), fm(g(rx, "b_ideal_pf_total") or 0), rx.get("b_ideal_pf_xy")))
    w("\nThese three layers make up **%.1f%%** of current cycles (%s / %s). "
      "Main finding: **X_used=16 (cout_t=16) is a BIGGER cause than Y=4** -- `kh*kw*Cin*32` = 110,592 B > 81 KB forces the planner down to cout_t=16, "
      "and at x_used=16 (23 measured shapes, all the same) every context costs **2K cycles** (pipeline stall = K) versus **1K** at x_used=32 (3x3: 77 measured shapes all measured 0). "
      "Using both weight banks (cfg b) brings X back to 32 and removes the stall; this is the single biggest lever for these three layers. See Section 6 for whether a single weight tile can actually span both banks on real hardware." % (100 * tot3 / base, fm(tot3), fm(base)))
    w("The multi-row-span option only adds value once cfg b is already applied AND the context mapping is CHANGED (currently one context = one row's worth of positions, `fe_tile_plan.py:74`) -- not checked against hardware.\n")

    # ---- 6. S9-FE addendum: can a weight tile span bank 0 + bank 1? ----
    w("## 6. Can a 110 KB weight tile span bank 0 + bank 1? -- NO on the evidence in this repo, and here is cfg b's marginal value on top of a+c\n")
    w("**Question asked**: cfg a + cfg c are being implemented now; cfg b (doubling the weight-bank capacity by giving up its ping-pong pairing) is on hold pending this answer.\n")
    w("**Answer: NO, not without new hardware** -- based on the SystemC reference model (no Verilog RTL or a written HAS chapter on SRAM-B addressing exists in this repo; if the hardware team has since added a decoder, this model would not show it):\n")
    w("- `sram/rtl_ref_sram_top.h:438`: the array-side weight read is `addr_b = i_sramb_addr.read() % ROWS_B` -- the address **wraps modulo the single buffer's row count**; it never continues into the other buffer.\n")
    w("- Buffer choice is `i_select`'s bit 1 (`sel[1]`, `rtl_ref_sram_top.h:118-119,206-209,326-329`), a discrete control signal driven by the context-switch/double-buffer logic (which tile uses buffer 0 vs. 1) -- it is **not derived from the weight address**, so software cannot 'flip' it mid-tile by driving a wider address.\n")
    w("- This is a straight, unmodified port of `sauria_model`'s own reference Sram (`docs/sauria_model_reference/sram/sram_top.h`, header comment: \"Double-Buffered SRAM System (A, B, C)\") -- the two buffers there are two independently-addressed macros picked by a select line, the standard way real double-buffered SRAM pairs work; there is no address-range decoder in `rtl_ref_wei_feeder.h`/`rtl_ref_main_controller.h` that would route addresses past one macro's depth into the other.\n")
    w("- **A separate, unrelated file** -- v4.5's own NATIVE `sram/sram_top.h` (dual-lane, 8-bank layout) -- DOES implement address-range crossing on its HOST WRITE path (`SRAMB_BANK_1_OFFSET`, `sram/sram_top.h:247,261`), but its own comments contradict themselves about what bank0/bank1 even mean there (\"ping-pong buffers\" in the section header vs. \"Lane A weights\"/\"Lane B weights\" on the array declarations) -- this is a DIFFERENT architecture (per-lane SRAM, not per-context double buffering) and not the core port used by this tile sweep, so it does not change the answer above, but the internal contradiction is itself worth flagging to HAS.\n")
    w("- **Not checked**: no Verilog RTL and no written HAS section on SRAM-B addressing were found in this repo to confirm the SystemC model against a datasheet-level source; if one exists elsewhere, that should be checked before treating this as final.\n")
    w("\n**cfg b's marginal value on top of an a+c baseline** (not on top of the unmodified current plan): a+c alone (cfg b unavailable) predicts **%s** cycles (%+.1f%% vs. current); enabling cfg b on top of that predicts **%s** (%+.1f%% vs. current) -- "
      "cfg b adds a further **%.1f%%** reduction relative to the a+c total (%s cycles), and it only changes the outcome for **6 of 83 layers** (dark5.conv, neck.d5.conv, det.p4.box1, det.p5.box1, det.p4.cls1, det.p5.cls1 -- 65-71%% further reduction each on top of their a+c value). "
      "See `tools/eval_sw/offcore_estimate.py --plan ac` vs. `--plan abc` for the reproducible numbers.\n" % ("109,764,457", -56.1, "87,944,305", -64.8, 19.9, "109,764,457"))

    open(a.out, "w", encoding="utf-8").write("\n".join(O) + "\n")
    print("ok", a.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
