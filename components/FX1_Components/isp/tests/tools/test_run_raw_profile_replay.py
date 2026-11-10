#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""fx1_isp_run_raw --profile-mode replays a legacy profile's TUNE extension
scalars into the bank after INIT, as fx1_isp_profile_from_legacy does
(ISP-UPD-009: INIT loads their reset values).

    test_run_raw_profile_replay.py RUN_RAW VECTORS_DIR OUT_DIR

- Every vector that runs its own profile transaction (TUNE_MODE, INIT, ...,
  PROFILE_ID, SEAL, APPLY), with those lines removed: a legacy profile with
  TUNE writes. Through --profile-mode it must give the vector's NV12. The two
  CH2 vectors (channels.txt) need CH1/CH2 buffers, which run_raw does not arm:
  test_driver_tlm runs all twelve.
- tune_ignored_in_legacy_64x48 (TUNE_CSC_CTRL LOOK_EN 1, FULL_RANGE 0): legacy
  mode keeps the reset (full range) and the vector's NV12; profile mode applies
  the profile's values (limited range, another NV12).
"""

import os
import shutil
import subprocess
import sys

TRANSACTION = ("TUNE_MODE", "TUNE_CMD", "TUNE_PROFILE_ID")


def run(rr, d, profile, prefix, profile_mode):
    w, h, _, _, frames = open(os.path.join(d, "meta.txt")).read().split()[:5]
    cmd = [rr, "--input", os.path.join(d, "input.bin"), "--width", w, "--height", h, "--profile", profile,
           "--frames", frames, "--out", prefix] + (["--profile-mode"] if profile_mode else [])
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
    meta = {}
    if os.path.exists(prefix + ".txt"):
        for line in open(prefix + ".txt"):
            f = line.split()
            if len(f) == 2:
                meta[f[0]] = f[1]
    nv12 = open(prefix + ".nv12", "rb").read() if os.path.exists(prefix + ".nv12") else b""
    return r.returncode, nv12, (int(meta.get("csc_std", -1)), int(meta.get("csc_full_range", -1))), r.stdout


def expected(d):
    return open(os.path.join(d, "expected_y.bin"), "rb").read() + open(os.path.join(d, "expected_uv.bin"), "rb").read()


def main():
    rr, vectors, out = sys.argv[1:4]
    out = os.path.join(out, "run_raw profile replay")
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    fails, stripped = 0, 0
    for v in sorted(os.listdir(vectors)):
        d = os.path.join(vectors, v)
        p = os.path.join(d, "profile.csrw")
        if not os.path.isfile(p) or any(os.path.exists(os.path.join(d, x)) for x in ("frames.csrw", "channels.txt")):
            continue
        lines = open(p, encoding="utf-8").read().splitlines(True)
        if not any(l.split()[:1] == ["TUNE_MODE"] for l in lines):
            continue
        kept = [l for l in lines if not (l.split() and l.split()[0] in TRANSACTION)]
        prof = os.path.join(out, v + ".legacy.csrw")
        open(prof, "w", encoding="utf-8").write("".join(kept))
        rc, nv12, _, log = run(rr, d, prof, os.path.join(out, v), True)
        ok = rc == 0 and nv12 == expected(d)
        print("%-4s %-40s transaction removed, --profile-mode: rc %d, NV12 %s" %
              ("ok" if ok else "FAIL", v, rc, "= vector" if nv12 == expected(d) else "differs"))
        if not ok:
            print(log[-2000:])
        fails += not ok
        stripped += 1
    if stripped != 10:
        print("FAIL: %d CH0-only vectors with their own transaction, 10 expected" % stripped)
        fails += 1
    d = os.path.join(vectors, "tune_ignored_in_legacy_64x48")
    p = os.path.join(d, "profile.csrw")
    rc, nv12, col, _ = run(rr, d, p, os.path.join(out, "ignored_legacy"), False)
    ok = rc == 0 and nv12 == expected(d) and col == (0, 1)
    print("%-4s tune_ignored_in_legacy legacy mode: reset kept, colorimetry %s, NV12 %s" %
          ("ok" if ok else "FAIL", col, "= vector" if nv12 == expected(d) else "differs"))
    fails += not ok
    rc, nv12p, col, log = run(rr, d, p, os.path.join(out, "ignored_profile"), True)
    ok = rc == 0 and col == (0, 0) and nv12p and nv12p != nv12
    print("%-4s tune_ignored_in_legacy profile mode: TUNE applied, colorimetry %s (limited), NV12 %s" %
          ("ok" if ok else "FAIL", col, "differs from legacy" if nv12p != nv12 else "= legacy"))
    if not ok:
        print(log[-2000:])
    fails += not ok
    print("test_run_raw_profile_replay: %d failure(s)" % fails)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
