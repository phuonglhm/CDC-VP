#!/usr/bin/env python3
"""Per-layer sweep of tile splits, scored by PREDICTED cycles (not by tile count). MODE B PREDICTION -- not a measurement.

Sweeps and proposes only. Does not modify `tools/fe/fe_tile_plan.py`; no VM; no simulation.
Cost model = `tile_model.py` (calibrated on 149 sauria_model shapes; same source as sw_eval) + an `apply_burst_dma`-style DMA schedule.

Sweep space per conv: cout_t in {32..1}, wt in {wide + powers of two} (the real constraints of plan_conv_core), ht in {largest
feasible, and a few smaller values}.
Four constraint configurations (bank configurations):
  a  current : A<=79K, B<=81K, C<=24576 words; skip occupies the second act bank
  b  B x2    : B<=162K (both weight banks)
  c  A x2    : A<=158K, skip moved to scratch (skip <= C <= 24 KB always fits)
  d  b + c
Prefetch (double buffer) per configuration (hardware: banks 0/1 weight, banks 2/3 ifmap, banks 4/5 psum):
  A can be prefetched <=> the second act bank is not used for anything else: configurations a, b and layers WITHOUT skip
  B can be prefetched <=> a second weight bank is free: configurations a, c
  C (psum), write: always ping-pong (banks 4/5)
The sequential sum ("no prefetch") is the reference frame (sw_eval: 249 616 773); "with prefetch" is reported as well.
Multi-row spans (wo<32): `ideal` (P = ht*wo positions/tile, no waste) and `flat` (stride 1: P = ht*(wo+kw-1), kw-1 dummy
columns per row).
  WARNING: spans assume a context does NOT cross rows -- currently "one context = Y_used positions of ONE row"
  (fe_tile_plan.py); spanning rows is a mapping change not yet checked against hardware / configuration.
"""
import argparse
import csv
import json
import math
import os
import sys
from collections import Counter, OrderedDict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tile_model as tm  # noqa: E402

KB = 1024
POWS = (32, 16, 8, 4, 2, 1)
BANK_A0, BANK_B0, BANK_C0 = 79 * KB, 81 * KB, (96 * KB) // 4
CFG = OrderedDict([
    ("a", dict(A=BANK_A0, B=BANK_B0, A_pp=True, B_pp=True, skip_bank=True)),
    ("b", dict(A=BANK_A0, B=2 * BANK_B0, A_pp=True, B_pp=False, skip_bank=True)),
    ("c", dict(A=2 * BANK_A0, B=BANK_B0, A_pp=False, B_pp=True, skip_bank=False)),
    ("d", dict(A=2 * BANK_A0, B=2 * BANK_B0, A_pp=False, B_pp=False, skip_bank=False)),
    # e: only for attribution (C x2 = both psum banks, no C ping-pong) -- not one of the four configurations
    ("e", dict(A=BANK_A0, B=BANK_B0, C=2 * BANK_C0, A_pp=True, B_pp=True, skip_bank=True)),
])
NO_X = False   # --no-x: drop every candidate with an EXTRAPOLATED tile (flag X) -> calibrated region only


def ht_for(wt, cout_t, cin, kh, kw, sh, sw, ho, skip, cfg):
    a_w = (wt - 1) * sw + kw
    d = a_w * cin
    ht_a = 0 if (d <= 0 or cfg["A"] // d < kh) else (cfg["A"] // d - kh) // sh + 1
    ht_c = cfg.get("C", BANK_C0) // (wt * cout_t)
    ht = min(ho, ht_a, ht_c)
    if skip and cfg["skip_bank"]:
        ht = min(ht, cfg["A"] // (wt * cout_t))
    return max(ht, 0)


def splits(total, step):
    out, s = [], 0
    while s < total:
        out.append(min(step, total - s))
        s += step
    return out


def build_seq(cout, ho, wo, cout_t, ht, wt):
    seq = []
    for cn in splits(cout, cout_t):
        for hh in splits(ho, ht):
            for ww in splits(wo, wt):
                seq.append((cn, hh, ww))
    return seq


_TILE_CACHE = {}


def eval_seq(model, conv, seq, span, skip, cfg):
    """Return a dict of totals (sequential / with prefetch), or None if some tile cannot be predicted."""
    o = model.ops[conv]
    K = o["cin"] * o["kh"] * o["kw"]
    cache, tiles, flags = _TILE_CACHE.setdefault((conv, span), {}), [], Counter()
    for (nch, hh, ww) in seq:
        key = (nch, hh, ww)
        t = cache.get(key)
        if t is None:
            if span:
                kw = o["kw"]
                P = hh * (ww + kw - 1 if (span == "flat") else ww)
                xu = tm.active_divisor(nch)
                n = math.ceil(nch / xu) * tm.chunk_contexts(P, 32)
                yu = 32 if P >= 32 else tm.active_divisor(P)
                t = model.tile(conv, nch, hh, ww, n_ctx_override=n, y_used_override=yu)
            else:
                t = model.tile(conv, nch, hh, ww)
            cache[key] = t
        if t["busy"] is None or (NO_X and t["flag"] == "X"):
            return None
        tiles.append(t)
        flags[t["flag"]] += 1
    n = len(tiles)
    busy = [t["busy"] for t in tiles]
    ldA = [tm.LD_A * t["A"] + tm.LD_B for t in tiles]
    ldB = [tm.LD_A * t["B"] for t in tiles]
    ldC = [tm.LD_A * t["C"] for t in tiles]
    st = [t["st"] for t in tiles]
    ld = [ldA[i] + ldB[i] + ldC[i] for i in range(n)]
    a_hide = cfg["A_pp"] and not skip
    b_hide = cfg["B_pp"]
    seq_total = sum(busy) + sum(ld) + sum(st)
    hid = [(ldA[i] if a_hide else 0.0) + (ldB[i] if b_hide else 0.0) + ldC[i] for i in range(n)]
    non = [ld[i] - hid[i] for i in range(n)]
    tot = ld[0] + st[n - 1] + sum(busy)
    for t in range(n):
        nxt = hid[t + 1] if t + 1 < n else 0.0
        prv = st[t - 1] if t >= 1 else 0.0
        tot += max(0.0, nxt + prv - busy[t]) + (non[t + 1] if t + 1 < n else 0.0)
    return dict(seq_total=seq_total, pf_total=tot, busy=sum(busy), n_tiles=n, flags=dict(flags),
                dma_seq=sum(ld) + sum(st), pe_util=None)


def candidates(model, job, st, cfg, span):
    o = model.ops[job]
    cin, cout = o["cin"], o["cout"]
    kh, kw = o["kh"], o["kw"]
    sh, sw = o["stride"]
    ho, wo = o["out_shape"][2], o["out_shape"][3]
    skip = st.get("skip") is not None
    K = cin * kh * kw
    wide = [32 * m for m in range(math.ceil(wo / 32), 1, -1)]
    out = []
    for cout_t in POWS:
        if K * cout_t > cfg["B"]:
            continue
        if cout_t < 16 and cout >= 16:
            continue   # x_used<16 has no calibration data (N) and is always worse when Cout>=16
        if span:
            wts = [wo]
        else:
            wts = [w for w in wide + list(POWS) if not (w > max(wo, 1) and w != 1) and (w >= 4 or wo < 4)]
        for wt in wts:
            if span and wo >= 32:
                continue
            htm = ht_for(wt, cout_t, cin, kh, kw, sh, sw, ho, skip, cfg)
            if htm < 1:
                continue
            m0 = math.ceil(ho / htm)
            hts = sorted({math.ceil(ho / m) for m in (m0, m0 + 1, m0 + 2, 2 * m0, 4 * m0, 8 * m0) if m <= ho and math.ceil(ho / m) <= htm}
                         | {htm}, reverse=True)
            for ht in hts:
                out.append((cout_t, wt, ht))
    return out


def best_both(model, job, st, cfgname, span):
    """Return (best by sequential sum, best with prefetch) in ONE sweep."""
    cfg = CFG[cfgname]
    o = model.ops[job]
    cout, ho, wo = o["cout"], o["out_shape"][2], o["out_shape"][3]
    skip = st.get("skip") is not None
    bs = bp = None
    for (cout_t, wt, ht) in candidates(model, job, st, cfg, span):
        seq = build_seq(cout, ho, wo, cout_t, ht, wt)
        r = eval_seq(model, job, seq, span, skip, cfg)
        if r is None:
            continue
        r.update(cout_t=cout_t, wt=wt, ht=ht)
        if bs is None or r["seq_total"] < bs["seq_total"]:
            bs = r
        if bp is None or r["pf_total"] < bp["pf_total"]:
            bp = r
    return bs, bp


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--shapes", required=True)
    ap.add_argument("--graph-ir", required=True)
    ap.add_argument("--program", required=True)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--no-x", action="store_true", help="loai ung vien co tile ngoai suy (X)")
    a = ap.parse_args()
    global NO_X
    NO_X = a.no_x
    model = tm.ShapeModel(a.shapes, a.graph_ir)
    prog = json.load(open(a.program))
    steps = OrderedDict((s["job"], s) for s in prog["steps"] if s.get("kind") == "conv")
    os.makedirs(a.out_dir, exist_ok=True)

    rows, tot = [], Counter()
    cur_flags = Counter()
    for job, st in steps.items():
        o = model.ops[job]
        cout, ho, wo = o["cout"], o["out_shape"][2], o["out_shape"][3]
        skip = st.get("skip") is not None
        seq = [(t["c"][1] - t["c"][0], t["oy"][1] - t["oy"][0], t["ox"][1] - t["ox"][0]) for t in st["tiles"]]
        base = eval_seq(model, job, seq, None, skip, CFG["a"])
        if base is None:
            continue
        cur_flags.update(base["flags"])
        row = OrderedDict(job=job, cin=o["cin"], kh=o["kh"], sh=o["stride"][0], cout=cout, ho=ho, wo=wo, skip=int(skip),
                          cur_cout_t=st["cout_t"], cur_wt=st["w_t"], cur_ht=st["h_t"], cur_xu=tm.active_divisor(st["cout_t"]),
                          cur_yu=tm.active_divisor(st["w_t"]), cur_tiles=len(seq), cur_busy=base["busy"],
                          cur_total_seq=round(base["seq_total"]), cur_total_pf=round(base["pf_total"]))
        for cfgname in CFG:
            for span in (None, "ideal", "flat"):
                if cfgname == "e" and span:
                    continue
                if span == "flat" and o["stride"][0] != 1:
                    continue
                if span and wo >= 32:
                    continue
                bs, bp = best_both(model, job, st, cfgname, span)
                for mode, b in (("seq", bs), ("pf", bp)):
                    k = "%s%s_%s" % (cfgname, ("_" + span) if span else "", mode)
                    if b is None:
                        row[k + "_total"] = None
                        continue
                    row[k + "_total"] = round(b["seq_total"] if mode == "seq" else b["pf_total"])
                    row[k + "_shape"] = "%d/%d/%d" % (b["cout_t"], b["wt"], b["ht"])
                    row[k + "_xy"] = "%d/%d" % (tm.active_divisor(b["cout_t"]), tm.active_divisor(b["wt"]) if not span else 32)
                    row[k + "_flags"] = "".join(sorted(k2 for k2 in b["flags"]))
                    row[k + "_busy"] = round(b["busy"])
        rows.append(row)
        tot["cur_busy"] += row["cur_busy"]
        tot["cur_seq"] += row["cur_total_seq"]
        tot["cur_pf"] += row["cur_total_pf"]

    # ---- group by unit (each conv picks its best option WITHIN that unit) ----
    def col(keyprefix):
        s, n = 0, 0
        for r in rows:
            v = r.get(keyprefix + "_total")
            base = r["cur_total_seq"]
            s += v if v is not None else base
            n += v is not None
        return s

    def best_of(keys, r, base_key):
        vals = [r.get(k + "_total") for k in keys if r.get(k + "_total") is not None]
        return min(vals) if vals else r[base_key]

    levers = OrderedDict()
    levers["0. CURRENT (today's tile split, no prefetch) -- reference frame"] = sum(r["cur_total_seq"] for r in rows)
    levers["1. Only enable PREFETCH (tiles unchanged; hardware: bank 1 / bank 3)"] = sum(r["cur_total_pf"] for r in rows)
    levers["2. Change the OBJECTIVE FUNCTION to cycles (cfg a), no prefetch"] = sum(best_of(["a_seq"], r, "cur_total_seq") for r in rows)
    levers["3. Cycle objective + prefetch (cfg a)"] = sum(best_of(["a_pf"], r, "cur_total_seq") for r in rows)
    levers["4. cfg b (B x2, uses both weight banks), sequential"] = sum(best_of(["b_seq"], r, "cur_total_seq") for r in rows)
    levers["5. cfg b, with prefetch (A prefetch only)"] = sum(best_of(["b_pf"], r, "cur_total_seq") for r in rows)
    levers["6. cfg c (A x2, skip -> scratch), sequential"] = sum(best_of(["c_seq"], r, "cur_total_seq") for r in rows)
    levers["7. cfg c, with prefetch (B prefetch only)"] = sum(best_of(["c_pf"], r, "cur_total_seq") for r in rows)
    levers["8. cfg d (b + c), sequential"] = sum(best_of(["d_seq"], r, "cur_total_seq") for r in rows)
    levers["9. Each conv picks the BEST of cfg a/b/c/d + prefetch/sequential (no span)"] = sum(
        best_of(["a_seq", "a_pf", "b_seq", "b_pf", "c_seq", "c_pf", "d_seq", "d_pf"], r, "cur_total_seq") for r in rows)
    levers["10. Same as 9 + SPAN ideal (changes the context mapping -- NOT checked against hardware)"] = sum(
        best_of(["a_seq", "a_pf", "b_seq", "b_pf", "c_seq", "c_pf", "d_seq", "d_pf",
                 "a_ideal_seq", "a_ideal_pf", "b_ideal_seq", "b_ideal_pf", "c_ideal_seq", "c_ideal_pf", "d_ideal_seq", "d_ideal_pf"], r, "cur_total_seq")
        for r in rows)
    levers["11. Same as 9 + SPAN flat (stride 1, counts the extra padding columns)"] = sum(
        best_of(["a_seq", "a_pf", "b_seq", "b_pf", "c_seq", "c_pf", "d_seq", "d_pf",
                 "a_flat_seq", "a_flat_pf", "b_flat_seq", "b_flat_pf", "c_flat_seq", "c_flat_pf", "d_flat_seq", "d_flat_pf"], r, "cur_total_seq")
        for r in rows)

    with open(os.path.join(a.out_dir, "sweep_by_conv.csv"), "w", newline="", encoding="utf-8") as fh:
        keys = []
        for r in rows:
            for k in r:
                if k not in keys:
                    keys.append(k)
        w = csv.DictWriter(fh, fieldnames=keys)
        w.writeheader()
        for r in rows:
            w.writerow(r)
    json.dump(dict(levers=levers, cur_busy_model=tot["cur_busy"], cur_flags=dict(cur_flags), n_conv=len(rows)),
              open(os.path.join(a.out_dir, "sweep.json"), "w", encoding="utf-8"), indent=1, ensure_ascii=False)
    base = levers[next(iter(levers))]
    print("conv: %d | busy (model, current tile plan): %s | cells: %s" % (len(rows), "{:,}".format(tot["cur_busy"]), dict(cur_flags)))
    for k, v in levers.items():
        print("%-88s %14s  %+6.1f %%" % (k, "{:,}".format(int(v)), 100.0 * (v - base) / base))
    return 0


if __name__ == "__main__":
    sys.exit(main())
