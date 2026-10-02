"""Roadmap R2 step 2: raw binaries of the core jobs for the C++ testbench tools/fe/sysc/tb_fe_core_tile.cpp.

Reads the job dirs written by tools/fe/fe_core_jobs.py (A.npy, B.npy, expect.npy, job.json) and adds, without
touching the existing files:
  A.bin           int8  [Cin, A_H, A_W]      host-padded input window
  B.bin           int8  [k, Cin, kh, kw]     tile weights (plain order; the testbench reorders through
                                             driver/libsauria_mem.h sauria_assemble_dram, like the runner does)
  bias.bin        int32 [k]                  PSUM preload value per channel (HAS 6.9: the bias)
  expect_psum.bin int32 [k, h, w]            bias[k] + sum(B * A), the accumulator the core must produce
  shape.txt       "kw kh d s Cin w h k X_used Y_used A_H A_W" (one line, what the testbench needs)
Usage (repository root, FE_WORK set): python3 -u tools/fe/fe_core_bins.py [--jobs fe_work/core/jobs.tsv]
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fe_common as fc  # noqa: E402

import numpy as np  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--jobs", default=os.path.join(fc.FE_WORK, "core", "jobs.tsv"))
    args = ap.parse_args()
    n, bad = 0, 0
    for line in open(args.jobs):
        line = line.rstrip("\n")
        if not line:
            continue
        name, gargs, tile, d = line.split("\t")
        with open(os.path.join(d, "job.json")) as f:
            meta = json.load(f)
        a = np.load(os.path.join(d, "A.npy"))
        b = np.load(os.path.join(d, "B.npy"))
        expect = np.load(os.path.join(d, "expect.npy")).astype(np.int64)
        bias = np.array(meta["bias"], dtype=np.int64)
        k, h, w = expect.shape
        if bias.size != k:
            bad += 1
            print("[core-bins] %s: bias %d != k %d" % (name, bias.size, k))
            continue
        psum = expect + bias[:, None, None]
        if psum.min() < -(1 << 31) or psum.max() > (1 << 31) - 1:
            bad += 1
            print("[core-bins] %s: accumulator exceeds INT32" % name)
            continue
        a.astype(np.int8).tofile(os.path.join(d, "A.bin"))
        b.astype(np.int8).tofile(os.path.join(d, "B.bin"))
        bias.astype("<i4").tofile(os.path.join(d, "bias.bin"))
        psum.astype("<i4").tofile(os.path.join(d, "expect_psum.bin"))
        ga = [int(x) for x in gargs.split()]
        with open(os.path.join(d, "shape.txt"), "w") as f:
            f.write("%d %d %d %d %d %d %d %d %d %d %d %d\n"
                    % (ga[0], ga[1], ga[2], ga[3], ga[4], ga[5], ga[6], ga[7], ga[8], ga[9], a.shape[1], a.shape[2]))
        n += 1
    print("[core-bins] wrote binaries for %d jobs, %d bad" % (n, bad))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
