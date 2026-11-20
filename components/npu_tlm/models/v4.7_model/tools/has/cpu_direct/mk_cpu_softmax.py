#!/usr/bin/env python3
"""Softmax of the vector unit driven by the CPU (HAS_CPU_DIRECT=1: GVU and DMA blocks of has/has_cpu_direct.h) for tools/has/tb_has_npu_top.cpp, and the
same program for the neo_top bench of the RTL.

Input: the files that HAS_CPU_DIRECT_DUMP=<dir> writes for a softmax group of the data flow controller on the call-style connection (HAS_GVU_REGS=0
HAS_GVU_ELEM_CLK=0): smx<k>_params.txt (len, scale, shift, zp), smx<k>_tables.bin (the reciprocal table in rows 0..3 and the exponential table in rows 4..5 of
vbank1, 128 bytes a row), smx<k>_in.bin (len words of 32 int8: lane j = row j, word p = token p), smx<k>_out.bin (the words of vbank0 after the run).
The golden is the result of the unit on the data flow controller path of the model itself.

The CPU program: DMA the tables into vbank1 and the scores into vbank0 (before the enable: the buffers of the scratchpad flip at the enable), the registers
SOFTMAX_0 (token length, tiles 1), SOFTMAX_1 (scale), SOFTMAX_2 (shift, zero point), OPCODE, the enable bit of CONTROL last; polling of the done bit, write
0xC (clear done, force the buffer swap), DMA vbank0 to DRAM (the probabilities are written in place), check ("K" line).

usage: mk_cpu_softmax.py <dump dir> <k> <out dir> [--name NAME]
  out dir: dram_init.bin, dram_golden.bin, mmio.txt (model), rtl/NAME.prog, rtl/NAME_dram.hex, rtl/NAME_gold.hex (RTL bench)
"""
import os
import sys

GVU, DMA = 0x12000, 0x13000
BUS_VBANK0, BUS_VBANK1 = 0x00100000, 0x00120000
OP_SOFTMAX = 3
NEO, DRAM_RTL, DRAM_MODEL = 0x80000000, 0x40000000, 0x80000000


def rtl_addr(a):
    if 0x11000 <= a < 0x15000: return NEO + (((a >> 12) & 0xF) << 16) + (a & 0xFFF)
    raise SystemExit("address 0x%x has no block of neo_top" % a)


def main():
    d, k, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    opt = dict(zip(sys.argv[4::2], sys.argv[5::2]))
    name = opt.get("--name", os.path.basename(os.path.normpath(out)))
    pre = os.path.join(d, "smx%d_" % k)
    prm = {l.split()[0]: int(l.split()[1]) for l in open(pre + "params.txt") if l.strip()}
    tab, xin, res = (open(pre + n, "rb").read() for n in ("tables.bin", "in.bin", "out.bin"))
    n = prm["len"]
    assert len(tab) == 768 and len(xin) == 32 * n == len(res), (len(tab), len(xin), len(res))
    al = lambda x: (x + 255) & ~255
    o_tab = 0x1000; o_in = al(o_tab + len(tab)); o_out = al(o_in + len(xin)); size = al(o_out + len(res)) + 0x1000
    init = bytearray(size); init[o_tab:o_tab + len(tab)] = tab; init[o_in:o_in + len(xin)] = xin
    for i in range(len(res)): init[o_out + i] = 0xA5
    gold = bytearray(init); gold[o_out:o_out + len(res)] = res
    dram = 0x80000000
    L = ["# Softmax driven by the CPU: %d tokens, scale %d shift %d zp %d" % (n, prm["scale"], prm["shift"], prm["zp"]), "W 0x%08x 0x3" % (DMA + 0x04)]

    def dma(r, w, c):
        L.extend(["W 0x%08x 0x%08x" % (DMA + 0x10, r), "W 0x%08x 0x%08x" % (DMA + 0x20, w), "W 0x%08x 0x%08x" % (DMA + 0x30, c),
                  "W 0x%08x 0x3" % DMA, "P 0x%08x 0x2 0x2" % (DMA + 0x0C), "W 0x%08x 0x3" % (DMA + 0x0C)])

    dma(dram + o_tab, BUS_VBANK1, len(tab))
    dma(dram + o_in, BUS_VBANK0, len(xin))
    L += ["W 0x%08x 0x%08x" % (GVU + 0x14, prm["scale"] & 0xFFFFFFFF),
          "W 0x%08x 0x%08x" % (GVU + 0x18, (prm["shift"] & 0xFF) | ((prm["zp"] & 0xFF) << 8)),
          "W 0x%08x 0x%08x" % (GVU + 0x10, (n & 0xFFFF) | (1 << 16)),
          "W 0x%08x 0x%08x" % (GVU + 0x04, OP_SOFTMAX), "W 0x%08x 0x1" % GVU,
          "P 0x%08x 0x2 0x2" % GVU, "W 0x%08x 0xc" % GVU]
    dma(BUS_VBANK0, dram + o_out, len(res))
    L.append("K %x %d" % (o_out, len(res)))
    os.makedirs(out, exist_ok=True)
    open(os.path.join(out, "dram_init.bin"), "wb").write(init)
    open(os.path.join(out, "dram_golden.bin"), "wb").write(gold)
    open(os.path.join(out, "mmio.txt"), "w", newline="\n").write("\n".join(L) + "\n")
    os.makedirs(os.path.join(out, "rtl"), exist_ok=True)
    P = []
    for l in L:
        t = l.split()
        if not t or t[0] == "#": continue
        if t[0] == "W":
            a, v = int(t[1], 16), int(t[2], 16)
            if (a & 0xFF) in (0x10, 0x20) and 0x13000 <= a < 0x14000 and v >= DRAM_MODEL: v = DRAM_RTL + (v - DRAM_MODEL)
            P.append("W %08x %08x" % (rtl_addr(a), v))
        elif t[0] == "P": P.append("P %08x %08x %08x" % (rtl_addr(int(t[1], 16)), int(t[2], 16), int(t[3], 16)))
        elif t[0] == "K": P.append("K %08x %d" % (int(t[1], 16), int(t[2])))
    P.append("E 0 0")
    base = os.path.join(out, "rtl", name)
    open(base + ".prog", "w", newline="\n").write("\n".join(P) + "\n")
    for src, dst in ((init, "_dram.hex"), (gold, "_gold.hex")):
        open(base + dst, "w", newline="\n").write("@0\n" + "\n".join("%02x" % x for x in src) + "\n")
    print("tokens %d out_bytes %d | RTL program %s.prog, %d lines" % (n, len(res), base, len(P)))


if __name__ == "__main__":
    main()
