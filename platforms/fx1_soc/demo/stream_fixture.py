#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Synthetic RAW sequences for the ISP stream (`isp-stream`) tests; stdlib only.

  stream_fixture.py DIR [--case NAME ...] [--width 256 --height 144]

Writes, for every case, DIR/NAME/ with three frames A, B, C of one synthetic
scene at three exposures (dark, mid, bright with clipped highlights), the
profile NAME.csrw and manifest.json listing A, B, C. The pixel values are an
integer formula (no stored data) quantized to RAW10, so one scene gives the
same ISP input whether it is fed as RAW12 normalized or as RAW10 native: the
cases bggr_raw10 and bggr_raw12 must produce identical NV12.

Cases (name: Bayer, source_format, input_depth):
  rggb_raw12  RGGB  le16_raw12_has  raw12
  grbg_raw10  GRBG  le16_raw10_msb  raw10 (native, COMMON_INPUT_FORMAT DEPTH=1)
  gbrg_raw12  GBRG  le16_raw10_msb  raw12 (normalized)
  bggr_raw10  BGGR  le16_raw10_has  raw10
  bggr_raw12  BGGR  le16_raw10_has  raw12

The profile is small and fixed: Bayer, depth, WB enabled with fixed gains, GTM in
automatic mode (key 0xBB, Lwhite 0x180, ROI log2 11, as the isp_stream FW
fixture), so every frame's tone curve depends on the frames before it.
"""
import argparse
import json
import os
import struct
import sys

CASES = {
    "rggb_raw12": ("RGGB", "le16_raw12_has", "raw12"),
    "grbg_raw10": ("GRBG", "le16_raw10_msb", "raw10"),
    "gbrg_raw12": ("GBRG", "le16_raw10_msb", "raw12"),
    "bggr_raw10": ("BGGR", "le16_raw10_has", "raw10"),
    "bggr_raw12": ("BGGR", "le16_raw10_has", "raw12"),
}
BAYER = {"RGGB": 0, "GRBG": 1, "GBRG": 2, "BGGR": 3}
EXPOSURE = {"A": 3, "B": 8, "C": 20}    # gain in 1/8: dark, mid, bright (clips)
CHANNEL = {"R": 9, "G": 8, "B": 6}       # scene colour per channel, in 1/8


def channel(bayer, x, y):
    """Colour of the CFA site (x, y) for a 2x2 pattern string such as RGGB."""
    return bayer[(y % 2) * 2 + (x % 2)]


def raw10(bayer, frame, x, y, w, h):
    """RAW10 value of the scene at (x, y): gradient, bars, a patch grid, exposure."""
    base = 40 + (x * 300) // w + (y * 200) // h                  # smooth gradient
    if (x // 16) % 4 == 0:                                         # vertical bars: edges
        base += 120
    if ((x // 24) + (y // 24)) % 2 and y > h // 2:                 # checker patches: texture
        base += 60
    v = base * EXPOSURE[frame] * CHANNEL[channel(bayer, x, y)] // 64
    return min(v + ((x * 7 + y * 13) % 5), 1023)                   # fixed dither, clip at 10 bits


def encode(v10, fmt):
    if fmt == "le16_raw10_msb":
        return v10 << 6
    if fmt == "le16_raw10_has":
        return v10 << 2
    if fmt == "le16_raw12_has":
        return v10 << 2                                            # RAW12 scale, RAW10-quantized
    raise ValueError(fmt)


def profile_text(name, bayer, depth):
    lines = ["# isp-stream synthetic fixture %s (platforms/fx1_soc/demo/stream_fixture.py)" % name,
             "COMMON_BAYER pattern=%d" % BAYER[bayer]]
    if depth == "raw10":
        lines.append("COMMON_INPUT_FORMAT DEPTH=1")
    lines += ["WB_CTRL en=1", "WB_GAIN_R gain_r=0x1A0", "WB_GAIN_G gain_g=0x100", "WB_GAIN_B gain_b=0x1C0",
              "GTM_KEY 0xBB", "GTM_LWHITE 0x180", "GTM_ROI_LOG2 11", "GTM_CTRL 1"]
    return "\n".join(lines) + "\n"


def write_case(root, name, w, h):
    bayer, fmt, depth = CASES[name]
    d = os.path.join(root, name)
    os.makedirs(d, exist_ok=True)
    files = []
    for f in "ABC":
        data = b"".join(struct.pack("<%dH" % w, *[encode(raw10(bayer, f, x, y, w, h), fmt) for x in range(w)])
                        for y in range(h))
        p = "frame %s.raw" % f                                     # a space: paths are quoted everywhere
        with open(os.path.join(d, p), "wb") as out:
            out.write(data)
        files.append(p)
    with open(os.path.join(d, name + ".csrw"), "w") as out:
        out.write(profile_text(name, bayer, depth))
    manifest = {"version": 1, "width": w, "height": h, "bayer": bayer, "source_format": fmt,
                "source_stride": 2 * w, "input_depth": depth, "frames": files}
    with open(os.path.join(d, "manifest.json"), "w") as out:
        json.dump(manifest, out, indent=2)
        out.write("\n")
    return d


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("dir")
    ap.add_argument("--case", action="append", choices=sorted(CASES))
    ap.add_argument("--width", type=int, default=256)
    ap.add_argument("--height", type=int, default=144)
    a = ap.parse_args(argv)
    for name in a.case or sorted(CASES):
        print(write_case(a.dir, name, a.width, a.height))
    return 0


if __name__ == "__main__":
    sys.exit(main())
