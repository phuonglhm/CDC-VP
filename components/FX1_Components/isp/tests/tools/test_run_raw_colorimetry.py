#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""fx1_isp_run_raw records the output colorimetry of every frame from the SOF
snapshot (ISP-UPD-005, fx1_isp_tlm::debug_set_frame_output_hook).

    test_run_raw_colorimetry.py RUN_RAW VECTORS_DIR OUT_DIR

Vectors with a known datapath colorimetry, among them the trap: in legacy
mode TUNE_CSC_CTRL is bank storage, so a profile writing FULL_RANGE = 0
still produces full range (a CSR readback would say limited).
"""

import os
import shutil
import subprocess
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
import run_regression  # noqa: E402

CASES = {  # vector: (csc_std, csc_full_range) of every frame
    "tune_ignored_in_legacy_64x48": (0, 1),       # legacy, TUNE_CSC_CTRL 0x1: datapath keeps the reset
    "tune_csc_limited_look_bggr_64x48": (1, 0),   # profile mode, TUNE_CSC_CTRL 0x1: limited, BT.709
    "tune_wb_dms_csc_full_look_cnf_64x48": (0, 1),  # profile mode, TUNE_CSC_CTRL 0x3: full
    "full_bggr_66x34": (1, 1),                    # legacy, CSC_CTRL 0x1
    "mandatory_rggb_64x32": (0, 1),               # reset
}


def main():
    rr, vectors, out = sys.argv[1:4]
    out = os.path.join(out, "run_raw colorimetry")
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    fails = 0
    for v, (std, full) in CASES.items():
        d = os.path.join(vectors, v)
        w, h, _, _, frames = open(os.path.join(d, "meta.txt")).read().split()[:5]
        prefix = os.path.join(out, v)
        r = subprocess.run([rr, "--input", os.path.join(d, "input.bin"), "--width", w, "--height", h, "--profile",
                            os.path.join(d, "profile.csrw"), "--frames", frames, "--out", prefix],
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        meta = {}
        per_frame = []
        for line in open(prefix + ".txt"):
            f = line.split()
            if f and f[0].startswith("frame_colorimetry_"):
                per_frame.append((int(f[4]), int(f[6])))
            elif len(f) == 2:
                meta[f[0]] = f[1]
        got = (int(meta.get("csc_std", -1)), int(meta.get("csc_full_range", -1)))
        ok = r.returncode == 0 and got == (std, full) and len(per_frame) == int(frames) and \
            all(p == (std, full) for p in per_frame)
        # run_regression.py reads the same file (review Stop D: its parser broke on the per-frame lines).
        st = run_regression.read_status(prefix + ".txt")
        ok = ok and (int(st["csc_std"]), int(st["csc_full_range"])) == (std, full) and "nv12_sha256" in st
        print("%-4s %-34s last %s, per frame %s (expected %s)" % ("ok" if ok else "FAIL", v, got, per_frame, (std, full)))
        fails += not ok
    print("test_run_raw_colorimetry: %d failure(s)" % fails)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
