#!/usr/bin/env python3
"""Check of the token-major output of GEMM_FUSED (extension register TMAJ_C0, 0x40000500) in tools/has/ref_program.py.

For every GEMM_FUSED instruction of a program directory, and a few values of c0, the instruction alone is computed twice by
ref_program.py (--first i --count 1 --write-ref): as written, and with "W 40000500 <c0>" added before its push. The two
images must be equal outside the output region; inside it, channels below c0 equal, channels c0.. the transpose.
usage: python3 tools/has/test_ref_tmaj.py <program dir> [<program dir> ...]   (exit 0 when every case passes)
"""
import os
import shutil
import subprocess
import sys
import tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.join(HERE, "ref_program.py")
sys.path.insert(0, HERE)
import ref_program as rp  # noqa: E402

# the child runs with the isolation of this process (-I on the Linux reference machine; none where numpy is a user package)
ISO = ["-I"] if sys.flags.isolated else []


def run_ref(d, first, out):
    p = subprocess.run([sys.executable] + ISO + [REF, d, "--first", str(first), "--count", "1", "--write-ref", out],
                       capture_output=True, text=True)
    return p.returncode


def with_tmaj(src, dst, push_line, c0):
    os.makedirs(dst, exist_ok=True)
    for f in ("dram_init.bin", "dram_golden.bin"):
        shutil.copy(os.path.join(src, f), os.path.join(dst, f))
    lines = open(os.path.join(src, "mmio.txt")).read().split("\n")
    lines.insert(push_line - 1, "W 40000500 %x" % c0)
    open(os.path.join(dst, "mmio.txt"), "w").write("\n".join(lines))


def main():
    bad = cases = 0
    tmp = tempfile.mkdtemp(prefix="tmaj_")
    for d in sys.argv[1:]:
        steps, _ = rp.read_program(os.path.join(d, "mmio.txt"), 0)
        for st in steps:
            if st.kind != "I" or st.opcode != 0x12 or st.reg.get("has_skip") or "IN_C" not in st.ext:
                continue
            x = st.ext
            cout, npos = rp.s32(x["OUT_C"]), rp.s32(x["OUT_H"]) * rp.s32(x["OUT_W"])
            base = os.path.join(tmp, "base.bin")
            if run_ref(d, st.index, base) not in (0, 1):
                continue
            a = np.fromfile(base, dtype=np.uint8)
            out = st.reg["out"]
            ct = rp.s32(x["TILE_COUT"])
            nt = -(-cout // ct)
            for c0 in sorted({0, ct * (nt // 2), ct * (nt - 1)}):
                cases += 1
                v = os.path.join(tmp, "v")
                shutil.rmtree(v, ignore_errors=True)
                with_tmaj(d, v, st.line, c0)
                img = os.path.join(tmp, "t.bin")
                rc = run_ref(v, st.index, img)
                b = np.fromfile(img, dtype=np.uint8)
                cm = a[out:out + cout * npos].reshape(cout, npos)
                exp = np.concatenate([cm[:c0].ravel(), cm[c0:].T.ravel()])
                outside = np.array_equal(np.delete(a, np.s_[out:out + cout * npos]), np.delete(b, np.s_[out:out + cout * npos]))
                ok = rc in (0, 1) and outside and np.array_equal(b[out:out + cout * npos], exp)
                bad += not ok
                print("%s [%d] cout %d pos %d c0 %d: %s" % (os.path.basename(d), st.index, cout, npos, c0, "PASS" if ok else "FAIL"))
    shutil.rmtree(tmp, ignore_errors=True)
    print("RESULT %d / %d PASS" % (cases - bad, cases))
    return 0 if cases and not bad else 1


if __name__ == "__main__":
    sys.exit(main())
