#!/usr/bin/env python3
"""Program with token-major GEMM_FUSED outputs (X_TMAJ_C0, 0x40000500) for the model run with HAS_OUT_TRANSPOSE=1.

Takes a program directory, writes "W 40000500 <c0>" before the push of every GEMM_FUSED that has extension registers and no
HAS_SKIP (c0 = the channel tile in the middle of the output, a multiple of TILE_COUT), and writes dram_golden.bin with
tools/has/ref_program.py over the whole program (later instructions read the token-major bytes as they are, in both).
usage: python3 tools/has/make_tmaj_gate.py <source dir> <destination dir>
"""
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ref_program as rp  # noqa: E402

# the child runs with the isolation of this process (-I on the Linux reference machine; none where numpy is a user package)
ISO = ["-I"] if sys.flags.isolated else []


def main():
    src, dst = sys.argv[1], sys.argv[2]
    os.makedirs(dst, exist_ok=True)
    shutil.copy(os.path.join(src, "dram_init.bin"), os.path.join(dst, "dram_init.bin"))
    steps, _ = rp.read_program(os.path.join(src, "mmio.txt"), 0)
    lines = open(os.path.join(src, "mmio.txt")).read().split("\n")
    ins = {}
    for st in steps:
        if st.kind == "I" and st.opcode == 0x12 and "IN_C" in st.ext and not st.reg.get("has_skip"):
            cout, ct = rp.s32(st.ext["OUT_C"]), rp.s32(st.ext["TILE_COUT"])
            ins[st.line] = ct * ((-(-cout // ct)) // 2)
    out = []
    for i, ln in enumerate(lines, 1):
        if i in ins:
            out.append("W 40000500 %x" % ins[i])
        out.append(ln)
    out[0:0] = ["# make_tmaj_gate.py: %d GEMM_FUSED with X_TMAJ_C0 (run with HAS_OUT_TRANSPOSE=1)" % len(ins)]
    open(os.path.join(dst, "mmio.txt"), "w").write("\n".join(out))
    shutil.copy(os.path.join(src, "dram_init.bin"), os.path.join(dst, "dram_golden.bin"))
    rc = subprocess.run([sys.executable] + ISO + [os.path.join(HERE, "ref_program.py"), dst, "--write-ref",
                         os.path.join(dst, "dram_golden.bin")], capture_output=True, text=True)
    print("%s: %d instructions token-major, ref_program rc %d: %s" % (dst, len(ins), rc.returncode,
                                                                       (rc.stdout.strip().splitlines() or [""])[-1][:120]))
    return 0 if rc.returncode in (0, 1) else 1


if __name__ == "__main__":
    sys.exit(main())
