#!/usr/bin/env python3
"""RESIDUAL of the vector unit driven by the CPU (HAS_CPU_DIRECT=1, GVU block of has/has_cpu_direct.h) for tools/has/tb_has_npu_top.cpp.

Golden: the formula of gvu_top.sv in Python (per lane: s = a * Sa (+|-) b * Sb + bias, 64 bit; arithmetic shift right by sh; clamp to
[-256, 255]; + out_z; clamp to [-128, 127]). Rows of 32 int8 lanes: A in vbank0, B in vbank1 (port 2: four rows per word, byte-linear on the
DMA side), the result in vbank2 (RTL revision 2). The CPU: DMA A and B in, the registers of gvu_config_regs.sv (opcode, RESIDUAL_0..4), enable, polling
of the done bit, clear done + force buffer swap, DMA of vbank2 to DRAM, check ("K" line). Enable to done: 16 + N cycles (revision 2).

usage: mk_cpu_res.py <out dir> [--rows 64] [--sub 0]
"""
import os
import random
import sys

GVU, DMA, DRAM = 0x12000, 0x13000, 0x80000000
BUS_VBANK0, BUS_VBANK1, BUS_VBANK2 = 0x00100000, 0x00120000, 0x00140000
M64 = (1 << 64) - 1


def lane(a, b, sa, sb, bias, sh, z, sub):
    pa, pb = (a * sa) & M64, (b * sb) & M64
    s = ((pa - pb) if sub else (pa + pb)) + bias
    s &= M64
    if s >> 63: s -= 1 << 64
    x = max(-256, min(255, s >> sh))
    return max(-128, min(127, x + z))


def main():
    out = sys.argv[1]
    args = dict(zip(sys.argv[2::2], sys.argv[3::2]))
    n, sub = int(args.get("--rows", 64)), int(args.get("--sub", 0))
    rng = random.Random(1112)
    sa, sb, sh, z = 23456, -3141 if not sub else 4321, 16, -7
    bias = (1 << (sh - 1)) - 5 * sa
    a = [rng.randrange(-128, 128) for _ in range(32 * n)]
    b = [rng.randrange(-128, 128) for _ in range(32 * n)]
    res = [lane(a[i], b[i], sa, sb, bias, sh, z, sub) for i in range(32 * n)]
    tb = lambda v: bytes(x & 0xFF for x in v)
    oa, ob, oo = 0x1000, 0x1000 + 32 * n + 256, 0x1000 + 64 * n + 512
    size = oo + 32 * n + 0x1000
    init = bytearray(size); init[oa:oa + 32 * n] = tb(a); init[ob:ob + 32 * n] = tb(b)
    for i in range(32 * n): init[oo + i] = 0xA5
    gold = bytearray(init); gold[oo:oo + 32 * n] = tb(res)
    r7 = (sub << 30) | (sh << 24) | ((z & 0xFF) << 16) | n
    L = ["# RESIDUAL driven by the CPU: %d rows, Sa %d Sb %d bias %d shift %d z %d sub %d" % (n, sa, sb, bias, sh, z, sub), "W 0x%08x 0x3" % (DMA + 0x04)]

    def dma(r, w, k):
        L.extend(["W 0x%08x 0x%08x" % (DMA + 0x10, r), "W 0x%08x 0x%08x" % (DMA + 0x20, w), "W 0x%08x 0x%08x" % (DMA + 0x30, k),
                  "W 0x%08x 0x3" % DMA, "P 0x%08x 0x2 0x2" % (DMA + 0x0C), "W 0x%08x 0x3" % (DMA + 0x0C)])

    dma(DRAM + oa, BUS_VBANK0, 32 * n)
    dma(DRAM + ob, BUS_VBANK1, 32 * n)
    for off, v in ((0x04, 6), (0x1C, r7), (0x20, sa & 0xFFFFFFFF), (0x24, sb & 0xFFFFFFFF), (0x28, bias & 0xFFFFFFFF), (0x2C, (bias >> 32) & 0xFFFFFFFF), (0x00, 1)):
        L.append("W 0x%08x 0x%08x" % (GVU + off, v))
    L += ["P 0x%08x 0x2 0x2" % GVU, "W 0x%08x 0xc" % GVU]
    dma(BUS_VBANK2, DRAM + oo, 32 * n)
    L.append("K %x %d" % (oo, 32 * n))
    os.makedirs(out, exist_ok=True)
    open(os.path.join(out, "dram_init.bin"), "wb").write(init)
    open(os.path.join(out, "dram_golden.bin"), "wb").write(gold)
    open(os.path.join(out, "mmio.txt"), "w", newline="\n").write("\n".join(L) + "\n")
    print("rows %d bytes %d expected_unit_cycles %d" % (n, 32 * n, 16 + n))


if __name__ == "__main__":
    main()
