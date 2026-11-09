#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Validate a VP-only post-Demosaic capture (fx1_isp/fx1_isp_debug.h).

    demosaic_validate.py BASE [--expect-sha256 HEX] [--expect-status STATUS]

BASE is the files' common path without extension (BASE.rgb16, BASE.png,
BASE.json); a path ending in one of those extensions is accepted too. Python
standard library only (shipped with the FX1 SDK). Checks, independently of
the writer:
  - the JSON: schema, stage, layout (RGB, LE16, 12 bits, no header/padding),
    status (frame_completed unless --expect-status), geometry;
  - BASE.rgb16: exactly width * height * 6 bytes (a truncated or longer file
    fails), every component <= 4095, SHA-256 equal to the JSON (and to
    --expect-sha256);
  - BASE.png: signature, chunk CRCs, IHDR = width x height RGB8, the zlib
    stream, filter 0 on every scanline (as the writer stores them), and every
    byte = (12-bit component) >> 4.
Exit status 0 if every check passes, 1 otherwise, 2 for usage errors.
"""

import argparse
import array
import hashlib
import json
import os
import struct
import sys
import zlib

SCHEMA = "fx1-isp/demosaic-capture/1"


def base_of(path):
    for ext in (".rgb16", ".png", ".json"):
        if path.endswith(ext):
            return path[: -len(ext)]
    return path


def png_pixels(data, problems):
    """RGB8 scanlines of the capture writer's PNG: 8-bit truecolour, not interlaced, filter 0."""
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        problems.append("PNG: bad signature")
        return None, 0, 0
    at, idat, w, h, ended = 8, b"", 0, 0, False
    while at + 12 <= len(data) and not ended:
        n, kind = struct.unpack(">I4s", data[at:at + 8])
        body = data[at + 8:at + 8 + n]
        if len(body) != n or at + 12 + n > len(data):
            problems.append("PNG: truncated chunk %r" % kind)
            return None, 0, 0
        (crc,) = struct.unpack(">I", data[at + 8 + n:at + 12 + n])
        if zlib.crc32(kind + body) & 0xFFFFFFFF != crc:
            problems.append("PNG: CRC error in chunk %r" % kind)
            return None, 0, 0
        if kind == b"IHDR":
            w, h, depth, colour, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if (depth, colour, interlace) != (8, 2, 0):
                problems.append("PNG: not 8-bit non-interlaced RGB (depth %d, colour %d)" % (depth, colour))
                return None, 0, 0
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            ended = True
        at += 12 + n
    if not ended:
        problems.append("PNG: no IEND")
        return None, 0, 0
    try:
        raw = zlib.decompress(idat)
    except zlib.error as e:
        problems.append("PNG: zlib stream: %s" % e)
        return None, 0, 0
    row = 3 * w
    if len(raw) != h * (row + 1):
        problems.append("PNG: %d bytes of scanlines, expected %d" % (len(raw), h * (row + 1)))
        return None, 0, 0
    out = bytearray(h * row)
    for y in range(h):
        f = raw[y * (row + 1)]
        if f != 0:  # the writer stores every scanline unfiltered
            problems.append("PNG: filter %d in row %d (the capture writer uses filter 0 only)" % (f, y))
            return None, 0, 0
        out[y * row:(y + 1) * row] = raw[y * (row + 1) + 1:(y + 1) * (row + 1)]
    return out, w, h


def validate(base, expect_sha=None, expect_status="frame_completed"):
    """List of problems; empty if the capture is valid."""
    problems = []
    try:
        meta = json.load(open(base + ".json", encoding="utf-8"))
    except (OSError, ValueError) as e:
        return ["JSON: %s" % e]
    g, lay = meta.get("geometry", {}), meta.get("layout", {})
    if meta.get("schema") != SCHEMA:
        problems.append("JSON: schema %r, expected %r" % (meta.get("schema"), SCHEMA))
    if meta.get("stage") != "post_demosaic_pre_ccm":
        problems.append("JSON: stage %r" % meta.get("stage"))
    if meta.get("status") != expect_status:
        problems.append("JSON: status %r, expected %r" % (meta.get("status"), expect_status))
    want_layout = {"order": "RGB", "interleaved": True, "component": "uint16_le", "significant_bits": 12,
                   "header_bytes": 0, "row_padding_bytes": 0}
    for k, v in want_layout.items():
        if lay.get(k) != v:
            problems.append("JSON: layout.%s = %r, expected %r" % (k, lay.get(k), v))
    w, h = g.get("width", 0), g.get("height", 0)
    if not (isinstance(w, int) and isinstance(h, int) and w > 0 and h > 0):
        return problems + ["JSON: bad geometry %r x %r" % (w, h)]
    if lay.get("row_bytes") != 6 * w or lay.get("bytes") != 6 * w * h:
        problems.append("JSON: row_bytes/bytes %r/%r do not match %dx%d" % (lay.get("row_bytes"), lay.get("bytes"), w, h))
    try:
        data = open(base + ".rgb16", "rb").read()
    except OSError as e:
        return problems + ["RGB16: %s" % e]
    if len(data) != 6 * w * h:
        return problems + ["RGB16: %d bytes, expected %d (%dx%d x 3 components x 2 bytes): truncated or padded"
                           % (len(data), 6 * w * h, w, h)]
    sha = hashlib.sha256(data).hexdigest()
    if sha != meta.get("sha256"):
        problems.append("RGB16: SHA-256 %s, JSON has %s" % (sha, meta.get("sha256")))
    if expect_sha and sha != expect_sha.lower():
        problems.append("RGB16: SHA-256 %s, expected %s" % (sha, expect_sha.lower()))
    comp = array.array("H")
    comp.frombytes(data)
    if sys.byteorder != "little":
        comp.byteswap()
    big = max(comp)
    if big > 4095:
        i = next(i for i, v in enumerate(comp) if v > 4095)
        problems.append("RGB16: component %d > 4095 at x %d, y %d, %s" % (big, (i // 3) % w, i // (3 * w), "RGB"[i % 3]))
    try:
        px, pw, ph = png_pixels(open(base + ".png", "rb").read(), problems)
    except OSError as e:
        return problems + ["PNG: %s" % e]
    if px is not None:
        if (pw, ph) != (w, h):
            problems.append("PNG: %dx%d, JSON %dx%d" % (pw, ph, w, h))
        else:
            bad = [i for i in range(len(comp)) if px[i] != comp[i] >> 4]
            if bad:
                i = bad[0]
                problems.append("PNG: %d bytes differ from v >> 4, first at x %d, y %d, %s (%d, expected %d)"
                                % (len(bad), (i // 3) % w, i // (3 * w), "RGB"[i % 3], px[i], comp[i] >> 4))
    return problems


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("base", help="capture path without extension (or BASE.rgb16 / .png / .json)")
    ap.add_argument("--expect-sha256", help="expected SHA-256 of BASE.rgb16")
    ap.add_argument("--expect-status", default="frame_completed")
    a = ap.parse_args()
    if a.expect_sha256 and not (len(a.expect_sha256) == 64 and all(c in "0123456789abcdefABCDEF" for c in a.expect_sha256)):
        print("demosaic_validate: --expect-sha256 needs 64 hex digits", file=sys.stderr)
        return 2
    base = base_of(a.base)
    problems = validate(base, a.expect_sha256, a.expect_status)
    for p in problems:
        print("%s: %s" % (base, p))
    if problems:
        return 1
    meta = json.load(open(base + ".json", encoding="utf-8"))
    g, f = meta["geometry"], meta["frame"]
    print("%s: OK, %dx%d RGB12 in LE16 (post-Demosaic, pre-CCM), frame ordinal %d (epoch %d, sequence %d, frame ID %d), "
          "sha256 %s" % (os.path.basename(base), g["width"], g["height"], f["ordinal"], f["epoch"], f["sequence"],
                         f["frame_id"], meta["sha256"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
