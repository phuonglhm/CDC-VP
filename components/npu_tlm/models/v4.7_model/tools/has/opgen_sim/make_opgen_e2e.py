#!/usr/bin/env python3
"""make_opgen_e2e.py <outdir> -- a small program for tools/has/tb_has_npu_top (knob ON, end to end through HasNpuTop):
dram_init.bin, dram_golden.bin, mmio.txt. One DMA_TENSOR COPY instruction (so the bench has an instruction to retire and the DFC is busy once), then one
descriptor of every operation of the DMA tensor register file through the host window: P (idle), registers, CTRL = 1, P (done), CTRL = 2, K (check the
written region against the golden DRAM). Golden = segments of the independent reference of opgen_cases.py applied to the initial image.
Run: HAS_DMA_OPGEN=1 HAS_NEO_REV=2 tb_has_npu_top <outdir>   (the bench prints one [OPGEN] line per descriptor and PASS / FAIL)."""
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import opgen_cases as oc  # noqa: E402

BASE = 0x40000A00 + 0x80
CTRL = BASE + oc.REG["CTRL"]
PUSH_A, A_ADDR, OUT_ADDR, LEN, MODE_PACK = 0x40000310, 0x40000444, 0x40000408, 0x40000450, 0x40000454


def main(out):
    os.makedirs(out, exist_ok=True)
    rnd = random.Random(7)
    size = 0x50000
    init = bytearray(rnd.getrandbits(8) for _ in range(size))
    gold = bytearray(init)
    lines = ["# make_opgen_e2e.py: one DMA_TENSOR COPY and the descriptors of the DMA tensor register file"]
    # instruction 0
    src, dst, n = 0x40000, 0x44000, 4096
    lines.append("# [0] DMA_TENSOR COPY %d bytes" % n)
    for a, v in ((A_ADDR, src), (OUT_ADDR, dst), (LEN, n), (MODE_PACK, 0), (PUSH_A, 0x16)):
        lines.append("W %08x %x" % (a, v))
    gold[dst:dst + n] = gold[src:src + n]
    descs = [
        oc.D(n0=4, n1=3, n2=2, row=16, rs0=16, ws0=16, rs1=64, ws1=96, rs2=512, ws2=640),
        oc.D(n0=1, n1=1, n2=1, ra=oc.SRC + 3, wa=oc.DST + 0x2000 + 5, row=1000),
        oc.D(op=oc.FILL, fill=0x5A, n0=3, n1=2, n2=1, row=21, ws0=21, ws1=100, wa=oc.DST + 0x4000),
        oc.D(op=oc.PAD2D, n1=4, n2=1, row=12, pt=1, pb=2, padl=3, padr=5, full=20, rs1=12, ws1=20, fill=0x81, wa=oc.DST + 0x6000),
        oc.D(op=oc.UPS, n1=4, n2=1, row=24, rs1=24, ws1=24, rep0=1, rep1=2, wa=oc.DST + 0x8000),
        oc.D(op=oc.UPS, n0=5, n1=3, n2=1, grp=8, row=40, rs1=40, ws1=80, rep0=2, rep1=2, wa=oc.DST + 0xA000),
        # NCHW nearest-neighbour upsample x2 of [3, 4, 5] (the words tools/fe/fe_emit_insts.py writes)
        oc.D(op=oc.UPS, n0=5, n1=4, n2=3, grp=1, row=5, rs1=5, ws1=10, rs2=20, ws2=80, rep0=2, rep1=2, wa=oc.DST + 0xC000),
    ]
    for k, d in enumerate(descs):
        lines.append("# opgen descriptor %d op %d" % (k, d.op))
        lines.append("P %08x 2 0" % CTRL)
        for nm, v in d.words():
            lines.append("W %08x %x" % (BASE + oc.REG[nm], v))
        lines.append("W %08x 1" % CTRL)
        lines.append("P %08x 4 4" % CTRL)
        lines.append("W %08x 2" % CTRL)
        segs = d.segments()
        oc.apply(gold, segs, d.fill)
        lo = min(s[2] for s in segs)
        hi = max(s[2] + s[3] for s in segs)
        lines.append("K %x %d" % (lo, hi - lo))
    open(os.path.join(out, "mmio.txt"), "w", newline="\n").write("\n".join(lines) + "\n")
    open(os.path.join(out, "dram_init.bin"), "wb").write(init)
    open(os.path.join(out, "dram_golden.bin"), "wb").write(gold)
    print("program written to", out, "(%d descriptors)" % len(descs))


if __name__ == "__main__":
    main(sys.argv[1])
