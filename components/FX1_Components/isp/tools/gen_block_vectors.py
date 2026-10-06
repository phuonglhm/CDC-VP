#!/usr/bin/env python3
"""Block-level vectors (HAS §10: every block verified on its own) for the
kernels whose corner cases cannot be reached through the full pipeline.
Written to tests/data/blocks/. Expected values come from the Python
reference only. --check verifies the committed files."""

import argparse
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "reference"))
from fx1_isp_ref import blocks, regs as regs_mod  # noqa: E402

OUT = os.path.join(HERE, "..", "tests", "data", "blocks")


def ee_gain_chain_cases(rng):
    lines = []
    edge = [0, 1, 0x3FFF, 0x4000, 0x4001, 0x7FFF, 0x8000, 0xA000, 0xC000]
    for gl in edge:
        for ga in edge:
            for gc in (0x4001, 0x6AAB, 0x8000):
                lines.append((gl, ga, gc, 0xC000))
    for _ in range(4000):
        g = rng.integers(0, 0x8001, size=3)
        lines.append((int(g[0]), int(g[1]), int(g[2]), int(rng.integers(0, 0xC001))))
    return "".join("%d %d %d %d %d\n" % (a, b, c, d, int(blocks.ee_gain_chain(a, b, c, d))) for a, b, c, d in lines)


def nr_variance_cases(rng):
    cases = []
    for n in (1, 2, 3, 4, 7, 8, 1000, 1001):
        half = n // 2
        cases.append((n, {0: half, 4: n - half}))          # cumulative hits n/2 exactly at bin 0 (even n)
        cases.append((n, {3: n}))
    cases.append((2, {172: 1, 173: 1}))
    cases.append((4, {173: 2, 255: 2}))
    cases.append((10, {255: 10}))
    cases.append((0, {}))
    for _ in range(60):
        n = int(rng.integers(1, 5000))
        bins = rng.integers(0, 256, size=n)
        hist = np.bincount(bins, minlength=256)
        cases.append((n, {int(b): int(c) for b, c in enumerate(hist) if c}))
    out = []
    for n, h in cases:
        hist = np.zeros(256, dtype=np.int64)
        for b, c in h.items():
            hist[b] = c
        items = " ".join("%d %d" % kv for kv in sorted(h.items()))
        out.append("%d %d %d %s\n" % (n, blocks.nr_variance(hist, n), len(h), items))
    return "".join(out)


def cnf_frame(rng, tc, ty, w=24, h=16):
    """Neighbours at exactly T_C / T_Y and one above, plus random pixels."""
    img = np.zeros((h, w, 3), dtype=np.int64)
    img[..., 0], img[..., 1], img[..., 2] = 120, 100, 150
    for y in range(h):
        for x in range(w):
            k = (x * 7 + y * 3) % 6
            du = tc // 2
            if k == 0:
                img[y, x] = (120, 100 + du, 150 + (tc - du))      # D_C == T_C
            elif k == 1:
                img[y, x] = (120, 100 + du + 1, 150 + (tc - du))  # D_C == T_C + 1
            elif k == 2:
                img[y, x] = (120 + ty, 100, 150)                  # D_Y == T_Y
            elif k == 3:
                img[y, x] = (120 + ty + 1, 100, 150)              # D_Y == T_Y + 1
    mask = rng.random((h, w)) < 0.2
    img[mask] = rng.integers(0, 256, size=(int(mask.sum()), 3))
    return np.clip(img, 0, 255)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    rng = np.random.default_rng(20260930)
    files = {"ee_gain_chain.txt": ee_gain_chain_cases(rng), "nr_variance.txt": nr_variance_cases(rng)}
    # CNF frames: "w h tc ty" then Y U V triplets, then expected U V pairs.
    cnf = []
    for tc, ty in ((0, 0), (40, 25), (510, 255), (511, 0), (7, 1)):
        img = cnf_frame(rng, tc, ty)
        out = blocks.cnf_core(img, min(tc, 511), ty)
        cnf.append("%d %d %d %d\n%s\n%s\n" % (img.shape[1], img.shape[0], tc, ty, " ".join(map(str, img.ravel())),
                                               " ".join(map(str, out.ravel()))))
    files["cnf_frames.txt"] = "".join(cnf)
    # 2DNR frames: "w h var_in var_out", Y, expected Y.
    nr = []
    for w, h, v in ((8, 6, 0), (16, 12, 30), (21, 9, 200), (6, 7, 5000), (40, 30, 65535), (1, 5, 10), (4, 2, 9)):
        y = rng.integers(0, 256, size=(h, w)).astype(np.int64)
        y[h // 2:, : w // 2] = 128 + rng.integers(-6, 7, size=(h - h // 2, w // 2))
        out, var = blocks.nr2d_core(y, v)
        nr.append("%d %d %d %d\n%s\n%s\n" % (w, h, v, var, " ".join(map(str, y.ravel())), " ".join(map(str, out.ravel()))))
    files["nr2d_frames.txt"] = "".join(nr)
    # EE frames: profile writes, Y, expected Y.
    ee = []
    for case in range(4):
        r = regs_mod.Registers()
        writes = [("EE_CTRL", 1), ("EE_ALPHA", int(rng.integers(0, 0x9000))), ("EE_BETA", int(rng.integers(0, 0x9000))),
                  ("EE_CLAMP", int(rng.integers(0, 1 << 16))), ("EE_FEATURE_EN", case % 8 if case else 7),
                  ("EE_RADIAL_CENTER", (int(rng.integers(0, 40)) << 16) | int(rng.integers(0, 60))),
                  ("EE_RADIAL_R2_TH0", 200), ("EE_RADIAL_R2_TH1", 900), ("EE_RADIAL_R2_TH2", 2500),
                  ("EE_RADIAL_GAIN_01", int(rng.integers(0, 1 << 32))), ("EE_RADIAL_GAIN_23", int(rng.integers(0, 1 << 32)))]
        for bank, n in ((0, 64), (1, 64), (2, 32), (3, 32)):
            writes += [("EE_LUT_CTRL", bank << 1), ("EE_LUT_ADDR", 0)]
            writes += [("EE_LUT_WDATA", int(v)) for v in rng.integers(0, 0x9000, size=n)]
        for n, v in writes:
            r.write(n, v)
        w, h = 30, 20
        y = rng.integers(0, 256, size=(h, w)).astype(np.int64)
        out = blocks.ee(y, r)
        ee.append("%d %d %d\n%s\n%s\n%s\n" % (w, h, len(writes), " ".join("%s %d" % kv for kv in writes),
                                               " ".join(map(str, y.ravel())), " ".join(map(str, out.ravel()))))
    files["ee_frames.txt"] = "".join(ee)
    os.makedirs(OUT, exist_ok=True)
    stale = []
    for fn, text in files.items():
        path = os.path.join(OUT, fn)
        if a.check:
            if not os.path.exists(path) or open(path).read() != text:
                stale.append(path)
        else:
            open(path, "w").write(text)
    if stale:
        sys.exit("gen_block_vectors.py: out of date: " + ", ".join(stale))
    print("%s %d block vector files" % ("checked" if a.check else "wrote", len(files)))


if __name__ == "__main__":
    main()
