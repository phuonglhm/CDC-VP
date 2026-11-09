#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""tools/demosaic_select.py: the capture of a job's final frame (decisions §12).

    test_demosaic_select.py RUN_RAW VECTORS_DIR OUT_DIR

Three different inputs through fx1_isp_run_raw --dump-demosaic give three
distinct captures; on copies of their index the selection must:
  - take the last of the job's N frames (N = 1, 2, 3), validated, copied as a
    set, the summary appended;
  - fail when a frame of the job (the last or an earlier one) did not
    complete, or the index has fewer frames than the job: never substitute an
    earlier frame;
  - check --expect-sha256 and --expect-wb;
  - replace an existing destination set as a whole and, when that fails,
    leave no JSON;
  - refuse --frames < 1 and a malformed --expect-sha256 (exit 2).
"""

import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.join(HERE, "..", "..", "tools")
failures = []


def check(cond, what):
    if not cond:
        failures.append(what)
        print("FAIL: " + what)


def run(cmd):
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)


def select(d, frames, out, *extra):
    return run([sys.executable, os.path.join(TOOLS, "demosaic_select.py"), d, "--frames", str(frames), "--out", out]
               + list(extra))


def main():
    rr, vectors, root = sys.argv[1:4]
    root = os.path.join(root, "demosaic select")
    shutil.rmtree(root, ignore_errors=True)
    os.makedirs(root)
    names = ("mandatory_rggb_64x32", "blc_profile0_trims_noscale_64x32", "lsc_saturate_64x32")
    prof = os.path.join(vectors, names[0], "profile.csrw")
    run_prefix = os.path.join(root, "run")
    args = [rr, "--width", "64", "--height", "32", "--profile", prof, "--frames", "3", "--per-frame", "--out",
            run_prefix, "--dump-demosaic"]
    for n in names:
        args += ["--input", os.path.join(vectors, n, "input.bin")]
    r = run(args)
    check(r.returncode == 0, "run_raw: " + r.stdout[-300:])
    src = run_prefix + ".demosaic.d"
    entries = [json.loads(l) for l in open(os.path.join(src, "index.jsonl"))]
    data = [open(e["base"] + ".rgb16", "rb").read() for e in entries]
    check(len(set(data)) == 3, "the three captures are not distinct")
    meta = [json.load(open(e["base"] + ".json")) for e in entries]

    def index(tag, lines):
        d = os.path.join(root, tag)
        os.makedirs(d)
        open(os.path.join(d, "index.jsonl"), "w").write("".join(json.dumps(e) + "\n" for e in lines))
        return d

    # The last of the job's frames, whatever N; summary and set written.
    for n in (1, 2, 3):
        out = os.path.join(root, "sel_%d" % n)
        summary = out + ".summary.txt"
        open(summary, "w").write("NV12 lines\n")
        r = select(src, n, out, "--summary", summary, "--expect-sha256", meta[2]["sha256"])
        check(r.returncode == 0, "frames %d: %s" % (n, r.stdout))
        check(open(out + ".rgb16", "rb").read() == data[2], "frames %d: not the last frame" % n)
        for ext in (".png", ".json"):
            check(open(out + ext, "rb").read() == open(entries[2]["base"] + ext, "rb").read(),
                  "frames %d: %s is not the last frame's" % (n, ext))
        s = open(summary).read()
        check(s.startswith("NV12 lines\n") and ("frame %d of the job's %d: ordinal 3" % (n, n)) in s and
              "pre-CCM" in s and "MATCH" in s, "frames %d: summary %r" % (n, s))

    # A job frame that did not complete: no capture, no substitute.
    reset_last = index("reset_last", entries[:2] + [dict(entries[2], status="frame_reset", base="", sha256="")])
    reset_mid = index("reset_mid", [entries[0], dict(entries[1], status="frame_failed", base="", sha256=""),
                                    entries[2]])
    for tag, d, n, message in (("the final frame reset", reset_last, 2, "frame_reset"),
                               ("an earlier job frame failed", reset_mid, 3, "frame_failed"),
                               ("fewer frames than the job", index("short", entries[:2]), 3, "the job has 3")):
        out = os.path.join(root, "bad_" + os.path.basename(d))
        r = select(d, n, out)
        check(r.returncode == 1 and message in r.stdout and not os.path.exists(out + ".json")
              and not os.path.exists(out + ".rgb16"), "%s: exit %d, %s" % (tag, r.returncode, r.stdout))
    # The frames before the job are not the job's concern.
    r = select(reset_mid, 1, os.path.join(root, "after_failed"))
    check(r.returncode == 0, "job of 1 after a failed earlier frame: " + r.stdout)

    # --expect-sha256 / --expect-wb
    g = meta[2]["config"]["wb_gain"]
    wb = "%d,%d,%d" % (g["r"], g["gr"], g["b"])
    wrong_wb = "%d,%d,%d" % (g["r"] + 1, g["gr"], g["b"])
    for tag, extra, code, message in (("wrong SHA-256", ["--expect-sha256", meta[0]["sha256"]], 1, "expected"),
                                      ("right WB", ["--expect-wb", wb], 0, "frame 3 of"),
                                      ("wrong WB", ["--expect-wb", wrong_wb], 1, "WB gains"),
                                      ("--frames 0", ["--frames", "0"], 2, ""),
                                      ("malformed SHA-256", ["--expect-sha256", "abc"], 2, "")):
        out = os.path.join(root, "x_" + tag.replace(" ", "_"))
        r = select(src, 3, out, *extra)
        check(r.returncode == code and message in r.stdout,
              "%s: exit %d (expected %d): %s" % (tag, r.returncode, code, r.stdout))

    # An existing destination set is replaced whole; a failed replacement leaves no JSON.
    out = os.path.join(root, "replace")
    for e in (".rgb16", ".png", ".json"):
        shutil.copyfile(entries[0]["base"] + e, out + e)
    r = select(src, 3, out)
    check(r.returncode == 0 and open(out + ".rgb16", "rb").read() == data[2] and
          json.load(open(out + ".json"))["sha256"] == meta[2]["sha256"], "replacement: " + r.stdout)
    os.remove(out + ".png")
    os.makedirs(out + ".png")  # in the way of the second rename
    r = select(src, 3, out)
    check(r.returncode == 1 and "cannot write" in r.stdout and not os.path.exists(out + ".json") and
          not any(os.path.exists(out + e + ".part") for e in (".rgb16", ".png", ".json")),
          "failed replacement: exit %d, json %s: %s" % (r.returncode, os.path.exists(out + ".json"), r.stdout))

    print("test_demosaic_select: %d failure(s)" % len(failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
