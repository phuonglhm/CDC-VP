#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Select the post-Demosaic capture of a job's final frame (VP-only debug tap).

    demosaic_select.py DIR --frames N --out BASE [--expect-sha256 HEX]
                       [--expect-wb R,G,B] [--summary FILE]

DIR is the capture directory of fx1_soc --dump-isp-demosaic (or of
fx1_isp_run_raw --dump-demosaic): frames/<key>.* and index.jsonl, one line per
frame end in the order the frames ended. The job's N frames are the last N
lines; every one must be frame_completed, otherwise the selection fails (a
failed final frame is never replaced by an earlier one). The last of them is
validated (demosaic_validate.py) and copied to BASE.rgb16 / .png / .json as a
set: the three files are staged first, the JSON (the set's commit marker) is
moved last. --expect-sha256 checks BASE.rgb16; --expect-wb checks the WB gains
of the frame's SOF configuration (UQ4.8, e.g. the render frame of `isp run
auto`); --summary appends the result to a summary file. Python standard
library only. Exit status 0 on success, 1 on a failed check, 2 for usage
errors.
"""

import argparse
import errno
import json
import os
import shutil
import sys

sys.dont_write_bytecode = True  # no __pycache__ in the SDK tree (its SHA256SUMS list every file)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import demosaic_validate  # noqa: E402


def remove_files(paths):
    """Best effort, files only: every path is tried even if one fails."""
    for p in paths:
        try:
            if os.path.isfile(p):
                os.remove(p)
        except OSError:
            pass


def publish(src, dst):
    """Copy the set src.* to dst.* (stage, withdraw the old JSON, JSON last)."""
    ext = (".rgb16", ".png", ".json")
    try:
        for e in ext:
            shutil.copyfile(src + e, dst + e + ".part")
    except OSError:
        remove_files(dst + e + ".part" for e in ext)
        raise
    try:
        os.remove(dst + ".json")
    except OSError as e:
        if e.errno != errno.ENOENT:
            raise
    try:
        for e in ext:
            os.replace(dst + e + ".part", dst + e)
    except OSError:
        remove_files(p for e in ext for p in (dst + e + ".part", dst + e))
        raise


def select(a):
    """(problems, chosen entry, its metadata)."""
    index = os.path.join(a.dir, "index.jsonl")
    try:
        entries = [json.loads(l) for l in open(index, encoding="utf-8") if l.strip()]
    except (OSError, ValueError) as e:
        return ["%s: %s" % (index, e)], None, None
    if len(entries) < a.frames:
        return ["%d frame(s) in %s, the job has %d" % (len(entries), index, a.frames)], None, None
    job = entries[-a.frames:]
    bad = [e for e in job if e["status"] != "frame_completed"]
    if bad:
        return ["job frame (capture %d, epoch %d, sequence %d) is %s: no capture of the job's final frame"
                % (e["ordinal"], e["epoch"], e["sequence"], e["status"]) for e in bad], None, None
    chosen = job[-1]
    problems = demosaic_validate.validate(chosen["base"], a.expect_sha256)
    meta = None
    if not problems:
        meta = json.load(open(chosen["base"] + ".json", encoding="utf-8"))
        if a.expect_wb:
            g = meta["config"]["wb_gain"]
            got = (g["r"], g["gr"], g["b"])
            if got != tuple(a.expect_wb):
                problems.append("WB gains R, G, B %s, expected %s" % (got, tuple(a.expect_wb)))
    return problems, chosen, meta


def wb_triplet(text):
    v = [int(x, 0) for x in text.split(",")]
    if len(v) != 3:
        raise argparse.ArgumentTypeError("R,G,B")
    return v


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("dir", help="capture directory (frames/, index.jsonl)")
    ap.add_argument("--frames", type=int, required=True, help="frames of the job (the last N index lines)")
    ap.add_argument("--out", required=True, help="base path of the selected set (BASE.rgb16 / .png / .json)")
    ap.add_argument("--expect-sha256")
    ap.add_argument("--expect-wb", type=wb_triplet, help="R,G,B WB gains of the frame (UQ4.8)")
    ap.add_argument("--summary", help="append the result to this file")
    a = ap.parse_args()
    if a.frames < 1:
        print("demosaic_select: --frames must be >= 1", file=sys.stderr)
        return 2
    if a.expect_sha256 and not (len(a.expect_sha256) == 64 and
                                all(c in "0123456789abcdefABCDEF" for c in a.expect_sha256)):
        print("demosaic_select: --expect-sha256 needs 64 hex digits", file=sys.stderr)
        return 2
    problems, chosen, meta = select(a)
    if not problems:
        try:
            publish(chosen["base"], a.out)
        except OSError as e:
            problems.append("cannot write %s.*: %s" % (a.out, e))
    for p in problems:
        print("demosaic_select: " + p)
    if problems:
        lines = ["demosaic       FAILED (%s)" % problems[0]]
    else:
        g, f = meta["geometry"], meta["frame"]
        lines = [
            "demosaic       %s.rgb16  %dx%d RGB12 in LE16 (R, G, B; bits [11:0]), sha256 %s" % (
                a.out, g["width"], g["height"], meta["sha256"]),
            "               VP debug tap: post-Demosaic, pre-CCM (BLC, LSC, BPC, WB, DG, Demosaic applied);"
            " not an ISP output",
            "               frame %d of the job's %d: ordinal %d, epoch %d, sequence %d, frame ID %d, profile ID %d" % (
                a.frames, a.frames, f["ordinal"], f["epoch"], f["sequence"], f["frame_id"], f["profile_id"]),
            "               preview %s.png (v8 = v12 >> 4, no WB/gamma/tone); metadata %s.json; per frame %s" % (
                a.out, a.out, os.path.join(a.dir, "frames")),
        ]
        if a.expect_sha256:
            lines.append("               MATCH: the stand-alone ISP model's capture of this frame")
    if a.summary:
        with open(a.summary, "a", encoding="utf-8") as s:
            s.write("\n".join(lines) + "\n")
    print("\n".join(lines))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
