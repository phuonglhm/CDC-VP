#!/usr/bin/env python3
"""One GEMM_FUSED layer for the data flow controller of the model (program directory: mmio.txt, dram_init.bin), made for the CPU replay
of mk_cpu_gemm.py: 1 x 1 convolution on a row of W positions, cin input channels, cout output channels in tiles of 32 (one core call per tile),
per-channel scale and shift, table activation. W = 32 gives one context per call (the case the RTL program can use), W = 64 two contexts per call.
The model runs the program itself; HAS_CPU_DIRECT_DUMP then writes the layer for mk_cpu_gemm.py.

usage: mk_gemm_gate.py <out dir> [--w 32] [--cin 24] [--cout 96] [--seed 5]
"""
import argparse
import os

import random
import struct

B = 0x40000000
R = {"in": 0x400, "w": 0x404, "out": 0x408, "bias": 0x40C, "kh": 0x41C, "kw": 0x420, "stride": 0x424, "pad": 0x428, "act": 0x42C, "has_skip": 0x430}
X = ["IN_C", "IN_H", "IN_W", "OUT_C", "OUT_H", "OUT_W", "TILE_COUT", "TILE_H", "TILE_W", "SCALE_ADDR", "SHIFT_ADDR", "LUT_ADDR", "ZP_OUT", "FLAGS"]
ADDR = {"in", "w", "out", "bias", "SCALE_ADDR", "SHIFT_ADDR", "LUT_ADDR"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--w", type=int, default=32)
    ap.add_argument("--cin", type=int, default=24)
    ap.add_argument("--cout", type=int, default=96)
    ap.add_argument("--seed", type=int, default=5)
    a = ap.parse_args()
    rng = random.Random(a.seed)
    ri = lambda lo, hi, n: [rng.randint(lo, hi) for _ in range(n)]
    img = bytearray(256 * 1024)
    top = [0x1000]

    def alloc(n):
        p = top[0]
        top[0] = (p + n + 63) & ~63
        return p

    def put(vals, dt):
        raw = b"".join(struct.pack("<i", v) for v in vals) if dt == "i4" else bytes(v & 0xFF for v in vals)
        p = alloc(len(raw))
        img[p:p + len(raw)] = raw
        return p

    cin, cout, w = a.cin, a.cout, a.w
    x0 = put(ri(-100, 100, cin * w), "i1")
    K = cin
    pw = alloc(cout * K)
    for c0 in range(0, cout, 32):
        nch = min(32, cout - c0)
        blk = bytes(v & 0xFF for v in ri(-20, 20, cin * nch))
        img[pw + c0 * K:pw + c0 * K + len(blk)] = blk
    out = alloc(cout * w)
    regs = {"in": x0, "w": pw, "out": out, "bias": put(ri(-800, 800, cout), "i4"), "kh": 1, "kw": 1, "stride": 1, "pad": 0, "act": 2, "has_skip": 0,
            "IN_C": cin, "IN_H": 1, "IN_W": w, "OUT_C": cout, "OUT_H": 1, "OUT_W": w, "TILE_COUT": 32, "TILE_H": 1, "TILE_W": w,
            "SCALE_ADDR": put(ri(1 << 29, 1 << 30, cout), "i4"), "SHIFT_ADDR": put(ri(38, 40, cout), "i4"),
            "LUT_ADDR": put([max(-128, min(127, int(round(x * 0.9 - 5)))) for x in range(-128, 128)], "i1"), "ZP_OUT": 3, "FLAGS": 0}
    os.makedirs(a.out, exist_ok=True)
    open(os.path.join(a.out, "dram_init.bin"), "wb").write(img)
    with open(os.path.join(a.out, "mmio.txt"), "w", newline="\n") as f:
        f.write("# mk_gemm_gate.py: GEMM_FUSED 1x1, W %d, cin %d, cout %d\n# [0] GEMM_FUSED\n" % (w, cin, cout))
        for name, v in regs.items():
            addr = B + (R[name] if name in R else 0x46C + 4 * X.index(name))
            f.write("W %08x %x\n" % (addr, v & 0xFFFFFFFF))
        f.write("W 40000310 12\n")
    open(os.path.join(a.out, "dram_golden.bin"), "wb").write(img)   # no reference image: the check of the run fails, the dump is what is wanted
    print("wrote", a.out)


if __name__ == "__main__":
    main()
