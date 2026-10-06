#!/usr/bin/env python3
"""Convert an FX1 ISP NV12 dump (active bytes, Y then UV) to a PNG preview.

The inverse transform matches the CSC standard of the run (CSC_CTRL.std):
limited-range YCbCr (Y 16..235, C 16..240), BT.601 or BT.709, chroma
upsampled by nearest neighbour (the Output Formatter keeps the top-left
sample of each 2x2 block). A preview is a visual check only; it is not a
pass/fail criterion (plan §6.2).
"""

import argparse

import numpy as np
from PIL import Image

KR_KB = {0: (0.299, 0.114), 1: (0.2126, 0.0722)}


def nv12_to_rgb(buf, w, h, std=0):
    """NV12 bytes (Y then interleaved UV) -> uint8 RGB [h, w, 3]."""
    buf = np.frombuffer(buf, dtype=np.uint8) if isinstance(buf, (bytes, bytearray)) else buf
    if buf.size != w * h * 3 // 2:
        raise ValueError("size %d != %d" % (buf.size, w * h * 3 // 2))
    y = buf[: w * h].reshape(h, w).astype(np.float64)
    uv = buf[w * h:].reshape(h // 2, w)
    u = np.repeat(np.repeat(uv[:, 0::2], 2, 0), 2, 1).astype(np.float64)
    v = np.repeat(np.repeat(uv[:, 1::2], 2, 0), 2, 1).astype(np.float64)
    kr, kb = KR_KB[std]
    yn = (y - 16) / 219.0
    pb = (u - 128) / 224.0
    pr = (v - 128) / 224.0
    r = yn + 2 * (1 - kr) * pr
    b = yn + 2 * (1 - kb) * pb
    g = (yn - kr * r - kb * b) / (1 - kr - kb)
    return np.clip(np.stack([r, g, b], -1) * 255 + 0.5, 0, 255).astype(np.uint8)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--nv12", required=True)
    ap.add_argument("--width", type=int, required=True)
    ap.add_argument("--height", type=int, required=True)
    ap.add_argument("--std", type=int, default=0, choices=(0, 1))
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    try:
        rgb = nv12_to_rgb(np.fromfile(a.nv12, dtype=np.uint8), a.width, a.height, a.std)
    except ValueError as e:
        raise SystemExit(str(e))
    Image.fromarray(rgb).save(a.out)
    print("wrote", a.out)


if __name__ == "__main__":
    main()
