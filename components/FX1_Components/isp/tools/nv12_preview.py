#!/usr/bin/env python3
"""Convert an FX1 ISP NV12 dump (active bytes, Y then UV) to a PNG preview.

The inverse transform follows the colorimetry of the frame (ISP-UPD-005):
CSC_CTRL.std (BT.601 / BT.709) and TUNE_CSC_CTRL.FULL_RANGE as the pipeline
used them, i.e. the datapath value at the frame's SOF:
  limited  Y 16..235, Cb/Cr 16..240:  Y' = (Y - 16) / 219, Pb/Pr = (C - 128) / 224
  full     Y, Cb/Cr 0..255:           Y' = Y / 255,        Pb/Pr = (C - 128) / 255
then R = Y' + 2(1-Kr)Pr, B = Y' + 2(1-Kb)Pb, G = (Y' - Kr R - Kb B) / Kg, x255,
round half up, clip 0..255. Kr, Kb are the exact decimals of the standards
(BT.601 0.299/0.114, BT.709 0.2126/0.0722) and the arithmetic is exact
(integers over one common denominator, then floor((2n + d) / 2d)): an exact
.5 rounds up, whatever the order of the operations, and the SDK's stdlib
converter (isp_pack.nv12_to_png) gives the same pixels (review B/C/P/D R4:
float64 put 146.5 at 146 here, 147 there). Chroma is upsampled by nearest neighbour (the
Output Formatter keeps the top-left sample of each 2x2 block). The
colorimetry comes from fx1_isp_run_raw's PREFIX.txt (--meta, csc_std and
csc_full_range of the last frame's SOF) or from --std and --range; there is
no default: a guessed range shows a full-range frame with crushed blacks and
clipped whites, or the reverse. A preview is a visual check only; it is not
a pass/fail criterion (plan §6.2).
"""

import argparse

import numpy as np
from PIL import Image

# Kr, Kb numerators over K (CSC_CTRL.std 0 BT.601, 1 BT.709).
KR_KB = {0: (299, 114, 1000), 1: (2126, 722, 10000)}


def rgb_tables(std, full_range):
    """Exact integer LUTs: 2 x 255 x value x D (+ D + 2048 D on the Y table, so
    one floor division by 2D gives the half-up rounded level + 1024) for
    Y, R(Cr), B(Cb), G(Cr), G(Cb); returns (D2 = 2D, tables)."""
    krn, kbn, k = KR_KB[std]
    kgn = k - krn - kbn
    yoff, yspan, cspan = (0, 255, 255) if full_range else (16, 219, 224)
    d = yspan * cspan * k * kgn
    y = [2 * 255 * (v - yoff) * cspan * k * kgn + d + 2048 * d for v in range(256)]
    r_v = [2 * 255 * 2 * (k - krn) * (v - 128) * yspan * kgn for v in range(256)]
    b_u = [2 * 255 * 2 * (k - kbn) * (u - 128) * yspan * kgn for u in range(256)]
    g_v = [-2 * 255 * 2 * krn * (k - krn) * (v - 128) * yspan for v in range(256)]
    g_u = [-2 * 255 * 2 * kbn * (k - kbn) * (u - 128) * yspan for u in range(256)]
    return 2 * d, (y, r_v, b_u, g_v, g_u)


def read_colorimetry(meta_path):
    """(std, full_range) from fx1_isp_run_raw's PREFIX.txt; None for a value the
    file does not give (an older fx1_isp_run_raw wrote no csc_full_range)."""
    vals = {}
    for line in open(meta_path, encoding="utf-8"):
        f = line.split()
        if len(f) == 2 and f[0] in ("csc_std", "csc_full_range"):
            vals[f[0]] = int(f[1])
    return vals.get("csc_std"), (bool(vals["csc_full_range"]) if "csc_full_range" in vals else None)


def nv12_to_rgb(buf, w, h, std, full_range):
    """NV12 bytes (Y then interleaved UV) -> uint8 RGB [h, w, 3]. std 0 BT.601,
    1 BT.709; full_range: Y/C 0..255, else 16..235 / 16..240. No defaults."""
    buf = np.frombuffer(buf, dtype=np.uint8) if isinstance(buf, (bytes, bytearray)) else buf
    if buf.size != w * h * 3 // 2:
        raise ValueError("size %d != %d" % (buf.size, w * h * 3 // 2))
    y = buf[: w * h].reshape(h, w)
    uv = buf[w * h:].reshape(h // 2, w)
    u = np.repeat(np.repeat(uv[:, 0::2], 2, 0), 2, 1)
    v = np.repeat(np.repeat(uv[:, 1::2], 2, 0), 2, 1)
    d2, tables = rgb_tables(std, full_range)
    ty, tr, tb, tgv, tgu = (np.array(t, dtype=np.int64) for t in tables)  # |n| < 2^56: exact in int64
    yy = ty[y]
    rgb = np.stack([yy + tr[v], yy + tgv[v] + tgu[u], yy + tb[u]], -1) // d2 - 1024
    return np.clip(rgb, 0, 255).astype(np.uint8)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--nv12", required=True)
    ap.add_argument("--width", type=int, required=True)
    ap.add_argument("--height", type=int, required=True)
    ap.add_argument("--meta", help="fx1_isp_run_raw PREFIX.txt: csc_std and csc_full_range of the frame")
    ap.add_argument("--std", type=int, choices=(0, 1), help="CSC_CTRL.std: 0 BT.601, 1 BT.709 (overrides --meta)")
    ap.add_argument("--range", choices=("full", "limited"), help="TUNE_CSC_CTRL.FULL_RANGE (overrides --meta)")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    try:
        std, full = read_colorimetry(a.meta) if a.meta else (None, None)
        std = a.std if a.std is not None else std
        full = (a.range == "full") if a.range else full
        if std is None or full is None:
            raise ValueError("no colorimetry%s: give --meta PREFIX.txt, or --std and --range"
                             % (" in %s (an older fx1_isp_run_raw?)" % a.meta if a.meta else ""))
        rgb = nv12_to_rgb(np.fromfile(a.nv12, dtype=np.uint8), a.width, a.height, std, full)
    except (OSError, ValueError) as e:
        raise SystemExit("nv12_preview: %s" % e)
    Image.fromarray(rgb).save(a.out)
    print("wrote %s (%s, %s range)" % (a.out, ("BT.601", "BT.709")[std], "full" if full else "limited"))


if __name__ == "__main__":
    main()
