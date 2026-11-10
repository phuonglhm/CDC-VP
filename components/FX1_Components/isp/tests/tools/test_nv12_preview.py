#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""NV12 -> RGB preview converters (ISP-UPD-005): tools/nv12_preview.py (numpy) and
the SDK's stdlib platforms/fx1_soc/demo/isp_pack.py must decode the range and
standard the frame was produced with.

    test_nv12_preview.py OUT_DIR

  - full range: the neutral ramp Y 0..255 (U = V = 128) decodes to grey = Y,
    including 0/15/16/17/234/235/240/255;
  - limited range: Y 16 -> 0, Y 235 -> 255, below/above clip;
  - round trip through the Python reference's CSC (blocks.csc, MAS v1.07
    Table 6-92) for both standards and both ranges, on greys and the six
    primaries/secondaries at 12 bits: |decoded - RGB12 / 16| within the 8-bit
    quantisation of the forward path (limited 2, full 2), while the other
    range visibly differs (>= 10 levels);
  - the stdlib converter gives the same pixels as the numpy one for every
    case (one formula, one rounding);
  - review B/C/P/D R4: an independent exact oracle (the defining formula
    cross-multiplied, Kr/Kb as the standards' decimals, round half up on the
    exact rational): the numpy converter equals it on every (Y, Cb, Cr) of
    both standards and ranges, the stdlib converter on all 65536 (Cb, Cr) at
    16 Y values (exact .5 ties, which only BT.601 full range has, among
    them); the review's case BT.601 full
    Y 128 Cb 178 Cr 78 has G = 146.5 -> 147;
  - no default range: the numpy function needs both arguments, the CLI
    refuses to run without --meta or --std/--range, and --meta reads
    fx1_isp_run_raw's csc_std/csc_full_range.
"""

import os
import struct
import subprocess
import sys
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ISP = os.path.join(HERE, "..", "..")
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(ISP, "tools"))
sys.path.insert(0, os.path.join(ISP, "reference"))
sys.path.insert(0, os.path.join(ISP, "..", "..", "..", "platforms", "fx1_soc", "demo"))
import nv12_preview  # noqa: E402
import isp_pack  # noqa: E402
from fx1_isp_ref import blocks, regs as regs_mod  # noqa: E402

failures = []


def check(cond, what):
    if not cond:
        failures.append(what)
        print("FAIL: " + what)


def nv12_of(yuv):
    """[h, w, 3] Y, Cb, Cr (h, w even, chroma constant per 2x2) -> NV12 bytes."""
    h, w, _ = yuv.shape
    uv = np.empty((h // 2, w), dtype=np.uint8)
    uv[:, 0::2] = yuv[0::2, 0::2, 1]
    uv[:, 1::2] = yuv[0::2, 0::2, 2]
    return yuv[..., 0].astype(np.uint8).tobytes() + uv.tobytes()


def png_pixels(path, w, h):
    """Decode the stdlib converter's 8-bit RGB PNG (filter 0 rows)."""
    data = open(path, "rb").read()
    pos, idat = 8, b""
    while pos < len(data):
        n, tag = struct.unpack(">I4s", data[pos:pos + 8])
        if tag == b"IDAT":
            idat += data[pos + 8:pos + 8 + n]
        pos += 12 + n
    raw = zlib.decompress(idat)
    rows = [raw[r * (3 * w + 1) + 1:(r + 1) * (3 * w + 1)] for r in range(h)]
    return np.frombuffer(b"".join(rows), dtype=np.uint8).reshape(h, w, 3)


def both(nv12, w, h, std, full, tmp):
    a = nv12_preview.nv12_to_rgb(nv12, w, h, std, full)
    isp_pack.nv12_to_png(nv12, w, h, tmp, std, full)
    b = png_pixels(tmp, w, h)
    check(np.array_equal(a, b), "stdlib and numpy converters differ (std %d, full %s)" % (std, full))
    return a


KR_KB = {0: ("0.299", "0.114"), 1: ("0.2126", "0.0722")}  # the standards' decimals, as text


def oracle(y, u, v, std, full):
    """Exact decoder from the definition: Y' = (Y - yoff) / yspan, P = (C - 128) / cspan,
    R = Y' + 2(1-Kr)Pr, B = Y' + 2(1-Kb)Pb, G = (Y' - Kr R - Kb B) / Kg; x255, round
    half up on the exact rational, clip. All quantities x E = yspan cspan K
    (integers, int64 arrays), G additionally x Kg K."""
    from fractions import Fraction
    kr, kb = (Fraction(t) for t in KR_KB[std])
    k = max(kr.denominator, kb.denominator)
    krn, kbn = int(kr * k), int(kb * k)
    yoff, yspan, cspan = (0, 255, 255) if full else (16, 219, 224)
    y, u, v = (np.asarray(a, dtype=np.int64) for a in (y, u, v))
    ye = (y - yoff) * cspan * k                       # Y' * E
    re_ = ye + 2 * (k - krn) * (v - 128) * yspan      # R * E
    be = ye + 2 * (k - kbn) * (u - 128) * yspan       # B * E
    ge = k * ye - krn * re_ - kbn * be                # G * E * Kg * K  (G = (K Y' - krn R - kbn B) / kgn)
    e = yspan * cspan * k
    kgn = k - krn - kbn

    def level(n, den):  # floor(255 n / den + 1/2), clipped
        return np.clip((2 * 255 * n + den) // (2 * den), 0, 255)

    return np.stack([level(re_, e), level(ge, e * kgn), level(be, e)], -1), \
        np.stack([(2 * 255 * re_) % (2 * e) == e, (2 * 255 * ge) % (2 * e * kgn) == e * kgn,
                  (2 * 255 * be) % (2 * e) == e], -1)


def chroma_frame(ys):
    """512 x 512 NV12: chroma block (j, i) = (Cb i, Cr j); its four pixels take ys[0..3]."""
    yp = np.empty((512, 512), dtype=np.uint8)
    yp[0::2, 0::2], yp[0::2, 1::2], yp[1::2, 0::2], yp[1::2, 1::2] = ys
    uv = np.empty((256, 512), dtype=np.uint8)
    uv[:, 0::2] = np.arange(256, dtype=np.uint8)[None, :]
    uv[:, 1::2] = np.arange(256, dtype=np.uint8)[:, None]
    u = np.repeat(np.repeat(uv[:, 0::2], 2, 0), 2, 1)
    v = np.repeat(np.repeat(uv[:, 1::2], 2, 0), 2, 1)
    return yp.tobytes() + uv.tobytes(), yp, u, v


def main():
    out = os.path.join(sys.argv[1], "nv12 preview")
    os.makedirs(out, exist_ok=True)
    tmp = os.path.join(out, "t.png")

    # Neutral ramp, 256 columns x 2 rows.
    y = np.tile(np.arange(256, dtype=np.uint8), (2, 1))
    yuv = np.stack([y, np.full_like(y, 128), np.full_like(y, 128)], -1)
    for std in (0, 1):
        g = both(nv12_of(yuv), 256, 2, std, True, tmp)
        check(np.array_equal(g[0, :, 0], np.arange(256)) and np.all(g[..., 0] == g[..., 1]) and
              np.all(g[..., 1] == g[..., 2]), "full range ramp is not grey = Y (std %d)" % std)
        lim = both(nv12_of(yuv), 256, 2, std, False, tmp)[0, :, 0]
        check(lim[16] == 0 and lim[235] == 255 and lim[15] == 0 and lim[240] == 255 and lim[126] == 128,
              "limited Y 16/235/15/240/126 -> %r (std %d)" % ([int(lim[k]) for k in (16, 235, 15, 240, 126)], std))
        check(all(int(g[0, k, 0]) == k for k in (0, 15, 16, 17, 234, 235, 240, 255)), "full range landmarks")

    # Round trip through the reference CSC.
    levels = [0, 64, 1024, 2048, 3072, 4095]
    cols = [(v, v, v) for v in levels] + [(4095, 0, 0), (0, 4095, 0), (0, 0, 4095),
                                          (0, 4095, 4095), (4095, 0, 4095), (4095, 4095, 0), (2800, 1200, 600)]
    rgb12 = np.repeat(np.repeat(np.array(cols, dtype=np.int64)[None, :, :], 2, 0), 2, 1)  # 2 x 2n, 2x2 blocks
    for std in (0, 1):
        for full in (0, 1):
            r = regs_mod.Registers()
            r.write("TUNE_MODE", 1)                     # the extension control is live in profile mode
            r.write("CSC_CTRL", std)
            r.write("TUNE_CSC_CTRL", full << 1)
            yuv = blocks.csc(rgb12, r)
            dec = both(nv12_of(yuv), rgb12.shape[1], 2, std, bool(full), tmp).astype(np.int64)
            err = np.abs(dec - ((rgb12 + 8) >> 4).clip(0, 255)).max()
            check(err <= 2, "round trip std %d full %d: max error %d" % (std, full, err))
            # Decoded with the other range: a visibly wrong picture (the bug the issue reports).
            wrong = nv12_preview.nv12_to_rgb(nv12_of(yuv), rgb12.shape[1], 2, std, not full).astype(np.int64)
            check(np.abs(wrong - dec).max() >= 10, "the wrong range is not distinguishable (std %d full %d)" % (std, full))

    # Review B/C/P/D R4: exact rounding, the same pixels from both converters.
    want, _ = oracle([128], [178], [78], 0, True)
    got = nv12_preview.nv12_to_rgb(nv12_of(np.array([[[128, 178, 78]] * 2] * 2, dtype=np.uint8)), 2, 2, 0, True)
    check(int(want[0, 1]) == 147 and int(got[0, 0, 1]) == 147, "BT.601 full Y128 Cb178 Cr78: G oracle %d numpy %d"
          % (int(want[0, 1]), int(got[0, 0, 1])))
    ties = {}
    for std in (0, 1):
        for full in (False, True):
            bad = 0
            for base in range(0, 256, 4):        # every Y: 64 frames of four
                nv, yp, u, v = chroma_frame((base, base + 1, base + 2, base + 3))
                want, tie = oracle(yp, u, v, std, full)
                bad += int(np.count_nonzero(nv12_preview.nv12_to_rgb(nv, 512, 512, std, full) != want))
                ties[(std, full)] = ties.get((std, full), 0) + int(np.count_nonzero(tie))
            check(bad == 0, "numpy converter != exact oracle in %d components (std %d, full %s)" % (bad, std, full))
            for ys in ((0, 16, 128, 255), (17, 127, 129, 235), (64, 96, 160, 200), (1, 15, 236, 254)):
                nv, yp, u, v = chroma_frame(ys)
                want, tie = oracle(yp, u, v, std, full)
                isp_pack.nv12_to_png(nv, 512, 512, tmp, std, full)
                got = png_pixels(tmp, 512, 512)
                check(np.array_equal(got, want), "stdlib converter != exact oracle in %d components (std %d, full %s, Y %r)"
                      % (np.count_nonzero(got != want), std, full, ys))
    print("exact .5 ties checked (std, full): %r" % ties)
    # Exact .5 ties exist for BT.601 full range only (the other denominators never
    # give a half); they must be among the cases checked.
    check(ties[(0, True)] > 0, "no exact .5 tie exercised: %r" % ties)

    # No default range.
    try:
        nv12_preview.nv12_to_rgb(nv12_of(yuv), rgb12.shape[1], 2, 0)
        check(False, "nv12_to_rgb accepted no range")
    except TypeError:
        pass
    nv = os.path.join(out, "f.nv12")
    open(nv, "wb").write(nv12_of(yuv))
    tool = [sys.executable, "-B", os.path.join(ISP, "tools", "nv12_preview.py"), "--nv12", nv, "--width",
            str(rgb12.shape[1]), "--height", "2", "--out", tmp]
    r = subprocess.run(tool, capture_output=True, text=True)
    check(r.returncode != 0 and "no colorimetry" in r.stderr + r.stdout, "CLI without colorimetry: %r" % r.stderr)
    meta = os.path.join(out, "f.txt")
    open(meta, "w").write("width 26\nheight 2\ncsc_std 1\ncsc_full_range 1\nframes 1\n")
    r = subprocess.run(tool + ["--meta", meta], capture_output=True, text=True)
    check(r.returncode == 0 and "BT.709, full range" in r.stdout, "CLI --meta: %r" % (r.stdout + r.stderr))
    open(meta, "w").write("width 26\nheight 2\ncsc_std 1\nframes 1\n")
    r = subprocess.run(tool + ["--meta", meta], capture_output=True, text=True)
    check(r.returncode != 0, "CLI --meta without csc_full_range accepted")
    r = subprocess.run(tool + ["--meta", meta, "--range", "limited"], capture_output=True, text=True)
    check(r.returncode == 0 and "BT.709, limited range" in r.stdout, "CLI --meta + --range: %r" % r.stdout)

    print("test_nv12_preview: %d failure(s)" % len(failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
