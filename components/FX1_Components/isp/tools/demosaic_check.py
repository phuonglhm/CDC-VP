#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Compare VP post-Demosaic captures with the Python reference, component by component.

    demosaic_check.py --capture-dir PREFIX.demosaic.d (--input F.isp16 ... | --input-list LIST)
                      --width W --height H --profile P.csrw [--frames N]
    demosaic_check.py --self-test

The reference (reference/fx1_isp_ref) runs the frames in the order
fx1_isp_run_raw feeds them (frame k uses input k mod number of inputs; one
legacy profile written once, temporal state carried) and keeps its RGB at the
Demosaic output, before CCM (blocks.run, stats["_tap_demosaic"]). The k-th
`frame_completed` capture of index.jsonl must equal reference frame k for
every R, G and B component; a mismatch is reported with frame, x, y and
channel. Every capture is also validated (demosaic_validate.py). The run's
frame count (--frames, default the number of inputs) is the expectation:
fewer completed captures, or none at all, fail.

--self-test: a flat Bayer field (one value per CFA colour) through the
reference for each pattern: every tapped pixel equals the post-DG value of
its colour (hand-checkable, independent of the Demosaic arithmetic).
Exit status 0 on success, 1 on a mismatch, 2 for usage errors.
"""

import argparse
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "reference"))
sys.path.insert(0, HERE)
sys.dont_write_bytecode = True  # no __pycache__ next to the tools
from fx1_isp_ref import blocks, regs as regs_mod  # noqa: E402
import demosaic_validate  # noqa: E402


def profile_writes(path):
    """Canonical writes of a profile in register or field syntax (ISP-UPD-008)."""
    return regs_mod.read_profile(path)


def reference_taps(inputs, w, h, writes, frames):
    r = regs_mod.Registers()
    for n, v in [("COMMON_FRAME_WIDTH", w), ("COMMON_FRAME_HEIGHT", h)] + writes:
        r.write(n, v)
    for k in range(frames):
        raw = np.fromfile(inputs[k % len(inputs)], dtype="<u2")
        if raw.size != w * h:
            raise SystemExit("%s: %d samples, expected %d" % (inputs[k % len(inputs)], raw.size, w * h))
        st = {}
        blocks.run_frame(raw.reshape(h, w).astype(np.int64), r, st)
        yield st["_tap_demosaic"]


def compare(cap, ref, frame, limit=10):
    """Mismatch lines (at most `limit`) and their total count."""
    if cap.shape != ref.shape:
        return ["frame %d: capture %s, reference %s" % (frame, "x".join(map(str, cap.shape)),
                                                       "x".join(map(str, ref.shape)))], 1
    diff = np.argwhere(cap != ref)
    lines = ["frame %d: x %d, y %d, %s: capture %d, reference %d" % (frame, x, y, "RGB"[c], cap[y, x, c], ref[y, x, c])
             for y, x, c in diff[:limit]]
    return lines, len(diff)


def read_input_list(path):
    """fx1_isp_run_raw's rule: one file per line, as written; empty lines and
    lines starting with '#' are skipped."""
    with open(path, encoding="utf-8", newline="") as f:
        lines = [l[:-1] if l.endswith("\n") else l for l in f]
    return [l for l in lines if l and not l.startswith("#")]


def check(a):
    inputs = list(a.input or [])
    if a.input_list:
        inputs += read_input_list(a.input_list)
    if not inputs:
        print("demosaic_check: no input", file=sys.stderr)
        return 2
    # Expected frames as fx1_isp_run_raw counts them, never from the captures.
    frames = len(inputs) if a.frames is None else a.frames
    if frames < 1:
        print("demosaic_check: --frames must be >= 1", file=sys.stderr)
        return 2
    index = os.path.join(a.capture_dir, "index.jsonl")
    try:
        entries = [json.loads(l) for l in open(index, encoding="utf-8") if l.strip()]
    except (OSError, ValueError) as e:
        print("demosaic_check: %s: %s" % (index, e))
        return 1
    done = [e for e in entries if e["status"] == "frame_completed"]
    bad = 0
    for e in entries:
        if e["status"] != "frame_completed":
            print("capture %d: %s (not compared)" % (e["ordinal"], e["status"]))
    if not done:
        print("no completed capture in %s: nothing compared" % index)
        return 1
    if len(done) != frames:
        print("%d completed capture(s), %d frame(s) expected" % (len(done), frames))
        bad += 1
    for k, ref in enumerate(reference_taps(inputs, a.width, a.height, profile_writes(a.profile), frames)):
        if k >= len(done):
            break
        problems = demosaic_validate.validate(done[k]["base"])
        for p in problems:
            print("capture %d: %s" % (done[k]["ordinal"], p))
        bad += len(problems)
        meta = json.load(open(done[k]["base"] + ".json", encoding="utf-8"))
        g = meta["geometry"]
        cap = np.fromfile(done[k]["base"] + ".rgb16", dtype="<u2").reshape(g["height"], g["width"], 3)
        lines, n = compare(cap.astype(np.int64), ref, k)
        for l in lines:
            print(l)
        if n:
            print("frame %d: %d mismatching component(s)" % (k, n))
        bad += n
        print("frame %d (capture %d, frame ID %d): %dx%d, %s" % (k, done[k]["ordinal"], meta["frame"]["frame_id"],
                                                            g["width"], g["height"], "MATCH" if not n else "MISMATCH"))
    return 1 if bad else 0


def self_test():
    """Flat field per pattern: every pixel = the post-DG value of its CFA colour."""
    w, h, bad = 34, 18, 0
    values = {0: 900, 1: 1800, 2: 1800, 3: 2700}  # R, Gr, Gb, B (Gr = Gb: G is flat)
    for bayer in range(4):
        sh, sv = bayer & 1, bayer >> 1
        y, x = np.mgrid[0:h, 0:w]
        ch = 2 * ((y + sv) & 1) + ((x + sh) & 1)  # RGGB phase from (sh, sv) on
        raw = np.vectorize(values.get)(ch).astype(np.int64)
        r = regs_mod.Registers()
        for n, v in [("COMMON_FRAME_WIDTH", w), ("COMMON_FRAME_HEIGHT", h), ("COMMON_BAYER", bayer),
                     ("WB_CTRL", 1), ("WB_GAIN_R", 0x1A0), ("WB_GAIN_B", 0x150), ("DG_CTRL", 1), ("DG_GAIN", 0x110)]:
            r.write(n, v)
        st = {}
        blocks.run_frame(raw, r, st)
        tap, dg = st["_tap_demosaic"], st["_tap_aec_dg"]  # post-DG Bayer of the active image
        want = [int(dg[0, 0]), int(dg[0, 1]), int(dg[1, 1])]  # R, G, B sites after the trim
        ok = tap.shape == ((h - sv) // 2 * 2, (w - sh) // 2 * 2, 3) and len(set(want)) == 3 and \
            all(np.all(tap[..., c] == want[c]) for c in range(3))
        print("self-test pattern %d: %s (R, G, B = %s)" % (bayer, "OK" if ok else "FAIL", want))
        bad += not ok
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--capture-dir", help="PREFIX.demosaic.d of fx1_isp_run_raw --dump-demosaic")
    ap.add_argument("--input", action="append", help="ISP 16-bit container frame (repeatable)")
    ap.add_argument("--input-list", help="one input per line, read as fx1_isp_run_raw reads it")
    ap.add_argument("--width", type=int)
    ap.add_argument("--height", type=int)
    ap.add_argument("--profile")
    ap.add_argument("--frames", type=int, help="frames of the run (default: the number of inputs, as fx1_isp_run_raw)")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args()
    if a.self_test:
        return self_test()
    if not (a.capture_dir and a.width and a.height and a.profile):
        ap.print_usage(sys.stderr)
        return 2
    return check(a)


if __name__ == "__main__":
    sys.exit(main())
