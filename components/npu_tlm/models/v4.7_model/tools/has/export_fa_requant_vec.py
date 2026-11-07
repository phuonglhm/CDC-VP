#!/usr/bin/env python3
"""Vectors for the requant of the attention products (FUSED_ATTN phases 1 and 3) on the RTL GEMM_FUSED epilogue (skip activation).

One file per phase: the int32 sums as the core delivers them (zero-point corrections already inside, as has/gvu_fused_attn.h), cut in
beats of 32 lanes, and the expected outputs under two rules:
  A  the rule hardwired in the delivered RTL (gvu_pipe_controller.sv:368-370): round half away from zero, narrow to 16 bits by WRAP
  U  the rule of the model (Knobs defaults): round half up, narrow to 16 bits by SATURATE
Format (integers, one line each): "PH <phase> <M> <TS> <Z> <beats>", then per beat "B <mask> <d0..d31>", then per beat "A <e0..e31>" and
then per beat "U <e0..e31>"; masked lanes carry 0 in data and in the expected values.
Sets: realistic sums from random attention heads (tools/fe/fe_ref_gvu.random_heads, first rows of each phase) and directed sets with
sums over the whole int32 range and small shifts, where the two rules differ.
usage: python -P tools/has/export_fa_requant_vec.py <out dir> [seed] [rows per phase]
"""
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "fe"))
import numpy as np  # noqa: E402
import fe_ref_gvu as gv  # noqa: E402


def rshift_away(p, s):
    if s == 0:
        return p
    half = 1 << (s - 1)
    q = (abs(p) + half) >> s
    return -q if p < 0 else q


def rshift_up(p, s):
    return p if s == 0 else (p + (1 << (s - 1))) >> s


def wrap16(v):
    return ((v + 32768) & 0xFFFF) - 32768


def out_rule(x, M, ts, z, rule):
    if rule == "A":
        r = wrap16(rshift_away(x * M, ts))
        y = wrap16(r + z)
    else:
        r = max(-32768, min(32767, rshift_up(x * M, ts)))
        y = r + z
    return max(-128, min(127, y))


def write_phase(path, ph, M, ts, z, rows):
    cols = len(rows[0])
    per = (cols + 31) // 32
    lines = ["PH %d %d %d %d %d" % (ph, M, ts, z, len(rows) * per)]
    exp = {"A": [], "U": []}
    for r in rows:
        for b in range(per):
            seg = [int(v) for v in r[b * 32:(b + 1) * 32]]
            nv = len(seg)
            mask = sum(1 << l for l in range(nv))
            seg += [0] * (32 - nv)
            lines.append("B %d %s" % (mask, " ".join(map(str, seg))))
            for key, rule in (("A", "A"), ("U", "U")):
                exp[key].append([out_rule(v, M, ts, z, rule) if l < nv else 0 for l, v in enumerate(seg)])
    for key in ("A", "U"):
        for e in exp[key]:
            lines.append("%s %s" % (key, " ".join(map(str, e))))
    open(path, "w").write("\n".join(lines) + "\n")
    return len(rows) * per


def main():
    out = sys.argv[1]
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1840
    nrow = int(sys.argv[3]) if len(sys.argv) > 3 else 6
    os.makedirs(out, exist_ok=True)
    K, AK = gv.DEFAULT_KNOBS, gv.ATTN_KNOBS
    n = 0
    for h in gv.random_heads(seed)[:6]:
        Q, Kk, V = np.array(h["Q"], np.int64), np.array(h["K"], np.int64), np.array(h["V"], np.int64)
        D = Q.shape[1]
        r = gv.fused_attn(h, K, AK)
        qrow, kcol = Q.sum(1), Kk.sum(1)
        acc1 = Q @ Kk.T - h["Zk"] * qrow[:, None] - h["Zq"] * kcol[None, :] + D * h["Zq"] * h["Zk"]
        A = np.array(r["A"], np.int64)
        acc3 = A @ V + 128 * V.sum(0)[None, :]          # - Za * Vcol_n with Za = -128
        for ph, acc, M, ts, z in ((1, acc1, h["Mqk"], h["TSqk"], h["Zqk"]), (3, acc3, h["Mav"], h["TSav"], h["Zav"])):
            sel = acc[:nrow]
            b = write_phase(os.path.join(out, "fa_%s_p%d.txt" % (h["name"], ph)), ph, M, ts, z, sel.tolist())
            print("%s phase %d M %d TS %d Z %d: %d beats, |acc| max %d" % (h["name"], ph, M, ts, z, b, int(np.abs(sel).max())))
            n += 1
        if h["name"] == "r197":   # a phase longer than the 614 scale words of the sbank: 140 rows x 7 beats = 980 beats (vbank2 holds 1024)
            b = write_phase(os.path.join(out, "fa_long_p1.txt"), 1, h["Mqk"], h["TSqk"], h["Zqk"], acc1[:140].tolist())
            print("long phase 1: %d beats" % b)
            n += 1
    rng = random.Random(seed)
    for k in range(6):
        ts = [0, 3, 12, 20, 31, 45][k]
        M = rng.randint(1, (1 << 31) - 1)
        z = rng.randint(-128, 127)
        span = [1 << 8, 1 << 14, 1 << 20, 1 << 26, 1 << 31, 1 << 31][k]
        rows = [[max(-(1 << 31), min((1 << 31) - 1, int(rng.gauss(0, span / 3)))) for _ in range(197)] for _ in range(8)]
        b = write_phase(os.path.join(out, "fa_dir%d_p1.txt" % k), 1, M, ts, z, rows)
        print("directed %d: M %d TS %d Z %d span %d: %d beats" % (k, M, ts, z, span, b))
        n += 1
    print("export_fa_requant_vec: %d files in %s" % (n, out))


if __name__ == "__main__":
    main()
