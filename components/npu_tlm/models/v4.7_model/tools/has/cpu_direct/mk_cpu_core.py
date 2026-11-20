#!/usr/bin/env python3
"""CPU replay of one core run (HAS_CPU_DIRECT=1, SAURIA block of has/has_cpu_direct.h) for tools/has/tb_has_npu_top.cpp.

Input: the files that HAS_CPU_DIRECT_DUMP=<dir> writes for the first core run of the sequential schedule (sram_a.bin, sram_b.bin,
sram_c_pre.bin, sram_c_out.bin, core.txt with the 29 words). The CPU: one-dimensional DMA of A / B / C-preload from DRAM into the local
memories (host side), the 29 words, global and done interrupt enable, START (no keep: the select bits flip and the core takes the loaded
halves), polling of the interrupt status, clear, memory switch (the result half returns to the host side), DMA of SRAM C to DRAM, check
against SRAM C of the run of the data flow controller ("K" line).

usage: mk_cpu_core.py <dump dir> <out dir> [--dram-base 0x80000000]
"""
import os
import sys

SAURIA, DMA = 0x11000, 0x13000
REGION = [(0x200, 2), (0x400, 17), (0x600, 5), (0x800, 5)]   # CON, ACT, WEI, OUT words of config_regs.sv


def main():
    dump, out = sys.argv[1], sys.argv[2]
    dram_base = int(sys.argv[4], 0) if len(sys.argv) > 4 and sys.argv[3] == "--dram-base" else 0x80000000
    rd = lambda n: open(os.path.join(dump, n), "rb").read()
    a, b, pre, cout = rd("sram_a.bin"), rd("sram_b.bin"), rd("sram_c_pre.bin"), rd("sram_c_out.bin")
    words = [int(l.split()[1], 16) for l in open(os.path.join(dump, "core.txt")) if l.startswith("word")]
    assert len(words) == 29, len(words)
    al = lambda x: (x + 255) & ~255
    oa = 0x1000; ob = al(oa + len(a)); op = al(ob + len(b)); oo = al(op + len(pre)); size = al(oo + len(cout)) + 0x1000
    init = bytearray(size); init[oa:oa + len(a)] = a; init[ob:ob + len(b)] = b; init[op:op + len(pre)] = pre
    for i in range(len(cout)): init[oo + i] = 0xA5
    gold = bytearray(init); gold[oo:oo + len(cout)] = cout
    L = ["# CPU replay of one core run (has/has_cpu_direct.h): %d / %d / %d bytes in, %d bytes out" % (len(a), len(b), len(pre), len(cout)),
         "W 0x%08x 0x3" % (DMA + 0x04)]

    def dma(r, w, n):
        L.extend(["W 0x%08x 0x%08x" % (DMA + 0x10, r), "W 0x%08x 0x%08x" % (DMA + 0x20, w), "W 0x%08x 0x%08x" % (DMA + 0x30, n),
                  "W 0x%08x 0x3" % DMA, "P 0x%08x 0x2 0x2" % (DMA + 0x0C), "W 0x%08x 0x3" % (DMA + 0x0C)])

    dma(dram_base + oa, 0x00000, len(a))
    dma(dram_base + ob, 0x40000, len(b))
    dma(dram_base + op, 0x80000, len(pre))
    k = 0
    for base, n in REGION:
        for i in range(n):
            L.append("W 0x%08x 0x%08x" % (SAURIA + base + 4 * i, words[k])); k += 1
    L += ["W 0x%08x 0x1" % (SAURIA + 0x04), "W 0x%08x 0x1" % (SAURIA + 0x08),
          "# START (keep 0: SRAM A / B / C select bits flip)", "W 0x%08x 0x1" % SAURIA,
          "P 0x%08x 0x1 0x1" % (SAURIA + 0x0C), "W 0x%08x 0x1" % (SAURIA + 0x0C),
          "# memory switch: the result half back to the host side", "W 0x%08x 0x10000" % SAURIA]
    dma(0x80000, dram_base + oo, len(cout))
    L.append("K %x %d" % (oo, len(cout)))
    os.makedirs(out, exist_ok=True)
    open(os.path.join(out, "dram_init.bin"), "wb").write(init)
    open(os.path.join(out, "dram_golden.bin"), "wb").write(gold)
    open(os.path.join(out, "mmio.txt"), "w", newline="\n").write("\n".join(L) + "\n")
    cyc = [l.split()[1] for l in open(os.path.join(dump, "core.txt")) if l.startswith("cycles")]
    print("words 29 a %d b %d pre %d out %d dfc_core_cycles %s" % (len(a), len(b), len(pre), len(cout), cyc[0] if cyc else "?"))


if __name__ == "__main__":
    main()
