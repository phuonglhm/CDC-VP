#!/usr/bin/env python3
"""RAW fixture adapter for the FX1 ISP (plan task P07).

Converts a source RAW file into the ISP input contract of HAS Table 6-88:
one little-endian 16-bit container per sample, the sample in bits [11:0]
(RAW10 MSB-aligned as {raw[9:0], 2'b00}), bits [15:12] zero. The conversion
happens here, outside the IP; the IDMA never shifts. Source files are never
modified.

The source layout is always given explicitly (never guessed from the data):

  le16_raw10_msb   RAW10 in bits [15:6] of a LE uint16 (the "Image Source" set)
  le16_raw10_has   RAW10 already in bits [11:2] (HAS contract), passed through
  le16_raw12_has   RAW12 in bits [11:0] (HAS contract), passed through

Each conversion checks that the bits the format declares unused are zero, and
writes a manifest with checksums of both files.

Usage:
  raw_fixture.py convert --in SRC --format le16_raw10_msb --width 2688 --height 1520 \
      --bayer BGGR --out OUT.isp16 [--src-stride BYTES] [--out-stride BYTES]
  raw_fixture.py verify-inventory --inventory ISP_RAW_INVENTORY.json --root DIR
"""

import argparse
import hashlib
import json
import os
import sys

import numpy as np

FORMATS = {
    # name: (bits that must be zero in the source word, right shift to the ISP container)
    "le16_raw10_msb": (0x003F, 4),
    "le16_raw10_has": (0xF003, 0),
    "le16_raw12_has": (0xF000, 0),
}
BAYER_CODES = {"RGGB": 0, "GRBG": 1, "GBRG": 2, "BGGR": 3}  # COMMON_BAYER, DEC-11


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def load_source(path, fmt, width, height, src_stride=None):
    """Returns (container_samples[h, w] uint16 in ISP scale, stats)."""
    if fmt not in FORMATS:
        raise ValueError("unknown format %r (choose one of %s)" % (fmt, ", ".join(FORMATS)))
    stride = src_stride or 2 * width
    if stride < 2 * width or stride % 2:
        raise ValueError("source stride %d invalid for width %d" % (stride, width))
    size = os.path.getsize(path)
    if size != stride * height:
        raise ValueError("%s: %d bytes, expected %d (stride %d x height %d)" % (path, size, stride * height, stride, height))
    words = np.fromfile(path, dtype="<u2").reshape(height, stride // 2)[:, :width]
    must_be_zero, shift = FORMATS[fmt]
    bad = int(np.count_nonzero(words & must_be_zero))
    if bad:
        raise ValueError("%s: %d samples have bits 0x%04X set, which format %s declares zero"
                         % (path, bad, must_be_zero, fmt))
    out = (words >> shift).astype(np.uint16)
    if int(out.max(initial=0)) > 0x0FFF:
        raise AssertionError("conversion produced a value above 12 bits")
    return out, {"min": int(out.min()), "max": int(out.max()), "mean": float(out.mean())}


def write_container(samples, path, out_stride=None):
    height, width = samples.shape
    stride = out_stride or 2 * width
    if stride < 2 * width or stride % 2:
        raise ValueError("output stride %d invalid for width %d" % (stride, width))
    buf = np.zeros((height, stride // 2), dtype="<u2")
    buf[:, :width] = samples
    buf.tofile(path)
    return stride


def cmd_convert(a):
    samples, stats = load_source(a.inp, a.format, a.width, a.height, a.src_stride)
    stride = write_container(samples, a.out, a.out_stride)
    bayer = a.bayer.upper()
    manifest = {
        "tool": "tools/raw_fixture.py",
        "source": {"path": os.path.abspath(a.inp), "sha256": sha256_file(a.inp), "format": a.format,
                   "stride": a.src_stride or 2 * a.width},
        "conversion": "container = word >> %d; bits 0x%04X checked zero" % (FORMATS[a.format][1], FORMATS[a.format][0]),
        "output": {"path": os.path.abspath(a.out), "sha256": sha256_file(a.out), "stride": stride,
                   "layout": "LE uint16, sample in bits [11:0] (HAS Table 6-88)"},
        "width": a.width, "height": a.height,
        "bayer": bayer, "common_bayer_code": BAYER_CODES[bayer],
        "stats_12bit": stats,
    }
    with open(a.out + ".json", "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
    print("wrote %s (+ .json): %dx%d %s, 12-bit range %d..%d" % (a.out, a.width, a.height, bayer, stats["min"], stats["max"]))


def cmd_verify_inventory(a):
    inv = json.load(open(a.inventory, encoding="utf-8"))
    decl = inv["declared_by_user"]
    ok = 0
    for entry in inv["files"]:
        path = os.path.join(a.root, entry["path"])
        digest = sha256_file(path)
        if digest != entry["sha256"]:
            sys.exit("CHECKSUM MISMATCH: %s" % path)
        _, stats = load_source(path, "le16_raw10_msb", decl["width"], decl["height"])
        if stats["min"] != entry["has12_min"] or stats["max"] != entry["has12_max"]:
            sys.exit("RANGE MISMATCH: %s: %r vs inventory %d..%d" % (path, stats, entry["has12_min"], entry["has12_max"]))
        ok += 1
    print("verify-inventory: %d files match checksums and 12-bit ranges" % ok)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("convert")
    c.add_argument("--in", dest="inp", required=True)
    c.add_argument("--format", required=True, choices=sorted(FORMATS))
    c.add_argument("--width", type=int, required=True)
    c.add_argument("--height", type=int, required=True)
    c.add_argument("--bayer", required=True, choices=sorted(BAYER_CODES) + [k.lower() for k in BAYER_CODES])
    c.add_argument("--out", required=True)
    c.add_argument("--src-stride", type=int)
    c.add_argument("--out-stride", type=int)
    c.set_defaults(func=cmd_convert)
    v = sub.add_parser("verify-inventory")
    v.add_argument("--inventory", required=True)
    v.add_argument("--root", required=True)
    v.set_defaults(func=cmd_verify_inventory)
    a = ap.parse_args()
    a.func(a)


if __name__ == "__main__":
    main()
