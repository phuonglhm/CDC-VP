#!/usr/bin/env python3
"""Input-depth migration check (V107-2, MAS v1.07 §6.6.3.2, Table 6-18).

A synthetic source in the "Image Source" layout (RAW10 in bits [15:6] of a LE
uint16) goes through tools/raw_fixture.py twice: native RAW10 right-aligned
(DEPTH 1) and normalized RAW12 (DEPTH 0). tools/make_profile.py builds a
profile for each, and the Python reference processes both. The check fails
unless:
  - the two containers differ, the two profiles differ only by the
    COMMON_INPUT_FORMAT write, and the NV12 outputs and statistics are equal;
  - normalized data declared RAW10 (a double shift) changes the output.
No confidential data is used; everything is written to a temporary directory.
"""

import os
import subprocess
import sys
import tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "reference"))
from fx1_isp_ref import blocks, regs as regs_mod  # noqa: E402

W, H, BAYER = 96, 64, "BGGR"


def run(cmd):
    subprocess.run([sys.executable] + cmd, check=True, stdout=subprocess.DEVNULL)


def profile(path):
    writes = []
    for line in open(path, encoding="utf-8"):
        if line.strip() and not line.startswith("#"):
            n, v = line.split()
            writes.append((n, int(v, 16)))
    return writes


def process(container, writes):
    r = regs_mod.Registers()
    for n, v in [("COMMON_FRAME_WIDTH", W), ("COMMON_FRAME_HEIGHT", H)] + writes:
        r.write(n, v)
    stats = {}
    y, uv = blocks.run_frame(container.astype(np.int64), r, stats)
    return y.tobytes() + uv.tobytes(), sorted((k, v) for k, v in stats.items() if not k.startswith("_"))


def main():
    rng = np.random.default_rng(20261028)
    raw10 = rng.integers(16, 1024, size=(H, W), dtype=np.uint16)
    bad = []
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "src.raw")
        (raw10 << 6).astype("<u2").tofile(src)
        out = {}
        for depth in ("raw12", "raw10"):
            iso = os.path.join(d, depth + ".isp16")
            prof = os.path.join(d, depth + ".csrw")
            run([os.path.join(HERE, "raw_fixture.py"), "convert", "--in", src, "--format", "le16_raw10_msb",
                 "--width", str(W), "--height", str(H), "--bayer", BAYER, "--out", iso, "--out-depth", depth])
            run([os.path.join(HERE, "make_profile.py"), "--input", iso, "--width", str(W), "--height", str(H),
                 "--bayer", BAYER, "--preset", "full", "--out", prof, "--depth", depth])
            c = np.fromfile(iso, dtype="<u2").reshape(H, W)
            out[depth] = (c, profile(prof))
        c12, p12 = out["raw12"]
        c10, p10 = out["raw10"]
        if np.array_equal(c12, c10):
            bad.append("the RAW10 and RAW12 containers are identical")
        if int(c10.max()) > 0x3FF or not np.array_equal(c10.astype(np.int64) << 2, c12):
            bad.append("the RAW10 container is not the native right-aligned form of the RAW12 one")
        if p10 != [("COMMON_INPUT_FORMAT", 1)] + p12:
            bad.append("the profiles differ beyond the COMMON_INPUT_FORMAT write")
        r12, r10 = process(c12, p12), process(c10, p10)
        if r12[0] != r10[0]:
            bad.append("NV12 output differs between RAW12 and RAW10 input")
        if r12[1] != r10[1]:
            bad.append("statistics differ between RAW12 and RAW10 input")
        double = process(c12, [("COMMON_INPUT_FORMAT", 1)] + p12)
        if double[0] == r12[0]:
            bad.append("normalized data declared RAW10 (double shift) is not detected")
    for b in bad:
        print("FAIL", b, file=sys.stderr)
    print("check_input_depth: %d failure(s)" % len(bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
