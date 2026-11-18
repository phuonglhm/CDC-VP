#!/usr/bin/env python3
"""fx1_fw_npu_oracle_xcheck: the firmware's C oracle against the drop's numpy reference.

  npu_oracle_xcheck.py <generator> <ref_program.py> <work dir> [generator args...]

Runs the generator as `<generator> [args...] <work dir>` (npu_case_gen: the
firmware test cases; npu_job_gen --xcheck <fixture>: a job fixture; both are
the firmware builders + C oracle compiled for the host), then
tools/has/ref_program.py of the SAURIA v4.7 drop on the same program with the C
oracle's image as the image under test: every instruction must match (exit 0),
and the whole reference image must equal the C oracle's byte for byte.
"""
import os
import subprocess
import sys


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    gen, ref, work = sys.argv[1:4]
    os.makedirs(work, exist_ok=True)
    subprocess.run([gen, *sys.argv[4:], work], check=True)
    oracle = os.path.join(work, "c_oracle.bin")
    out = os.path.join(work, "ref_image.bin")
    r = subprocess.run([sys.executable, ref, work, "--image", oracle, "--write-ref", out])
    with open(oracle, "rb") as f:
        a = f.read()
    with open(out, "rb") as f:
        b = f.read()
    diff = sum(1 for x, y in zip(a, b) if x != y) + abs(len(a) - len(b))
    print("[xcheck] ref_program exit %d; whole image: %d of %d bytes differ" % (r.returncode, diff, len(a)))
    ok = r.returncode == 0 and diff == 0
    print("RESULT %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
