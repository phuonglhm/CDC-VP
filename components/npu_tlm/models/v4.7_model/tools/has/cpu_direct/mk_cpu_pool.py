#!/usr/bin/env python3
"""CPU replay of one max pool run of the vector unit (HAS_CPU_DIRECT=1, GVU block of has/has_cpu_direct.h) for tools/has/tb_has_npu_top.cpp.

Input: the files that HAS_CPU_DIRECT_DUMP=<dir> writes for the first max pool run by registers of the data flow controller (pool.txt with the
register writes the controller issued, vbank0_in.bin, vbank2_out.bin). The CPU: one-dimensional DMA of the input rows into vbank0 (the buffer
the unit does not use), the same register writes (the last one sets the enable bit), polling of the done bit of CONTROL (the unit has no
interrupt to the CPU), a CONTROL write that clears done and forces the buffer swap (the output buffer goes to the DMA side), DMA of the output
rows of vbank2 to DRAM, check against the run of the data flow controller ("K" line).

usage: mk_cpu_pool.py <dump dir> <out dir> [--dram-base 0x80000000]
"""
import os
import sys

GVU, DMA = 0x12000, 0x13000
BUS_VBANK0, BUS_VBANK2 = 0x00100000, 0x00140000


def main():
    dump, out = sys.argv[1], sys.argv[2]
    dram_base = int(sys.argv[4], 0) if len(sys.argv) > 4 and sys.argv[3] == "--dram-base" else 0x80000000
    txt = open(os.path.join(dump, "pool.txt")).read().split("\n")
    writes = [(int(l.split()[1], 16), int(l.split()[2], 16)) for l in txt if l.startswith("write")]
    kv = {}
    for l in txt:
        t = l.split()
        if t and t[0] in ("rows_in", "rows_out", "cycles"): kv[t[0]] = int(t[1])
    vin = open(os.path.join(dump, "vbank0_in.bin"), "rb").read()
    vout = open(os.path.join(dump, "vbank2_out.bin"), "rb").read()[:kv["rows_out"] * 32]
    assert len(vin) == kv["rows_in"] * 32 and len(vout) == kv["rows_out"] * 32
    al = lambda x: (x + 255) & ~255
    oi = 0x1000; oo = al(oi + len(vin)); size = al(oo + len(vout)) + 0x1000
    init = bytearray(size); init[oi:oi + len(vin)] = vin
    for i in range(len(vout)): init[oo + i] = 0xA5
    gold = bytearray(init); gold[oo:oo + len(vout)] = vout
    L = ["# CPU replay of one max pool run (has/has_cpu_direct.h): %d input rows, %d output rows, %d register writes" % (kv["rows_in"], kv["rows_out"], len(writes)),
         "W 0x%08x 0x3" % (DMA + 0x04)]

    def dma(r, w, n):
        L.extend(["W 0x%08x 0x%08x" % (DMA + 0x10, r), "W 0x%08x 0x%08x" % (DMA + 0x20, w), "W 0x%08x 0x%08x" % (DMA + 0x30, n),
                  "W 0x%08x 0x3" % DMA, "P 0x%08x 0x2 0x2" % (DMA + 0x0C), "W 0x%08x 0x3" % (DMA + 0x0C)])

    dma(dram_base + oi, BUS_VBANK0, len(vin))
    L.append("# the register writes of the data flow controller (the last one sets the enable bit)")
    for a, v in writes:
        L.append("W 0x%08x 0x%08x" % (GVU + a, v))
    L += ["P 0x%08x 0x2 0x2" % GVU, "# clear done + force buffer swap: the output buffer goes to the DMA side", "W 0x%08x 0xc" % GVU]
    dma(BUS_VBANK2, dram_base + oo, len(vout))
    if "--valid" in sys.argv:   # only the lanes that hold outputs (ow < Wo): the others keep what the buffer held before
        g = {}
        for l in txt:
            t = l.split()
            if t and t[0] == "kk": g = dict(zip(t[0::2], map(int, t[1::2])))
        kk, st, c, ho, wo = g["kk"], g["s"], g["c"], g["Ho"], g["Wo"]
        wt = (g["W"] + 31) // 32
        ol = 32 if st == 1 else ((32 - kk) // st + 1 if kk == st else (32 - kk) // st + 2)
        wpr, lanes = ((wt + st - 1) // st, 32) if st in (2, 4) else (wt, ol)
        for cc in range(c):
            for oh in range(ho):
                for w in range(wpr):
                    n = min(lanes, wo - w * lanes)
                    if n > 0: L.append("K %x %d" % (oo + ((cc * ho + oh) * wpr + w) * 32, n))
    else:
        L.append("K %x %d" % (oo, len(vout)))
    os.makedirs(out, exist_ok=True)
    open(os.path.join(out, "dram_init.bin"), "wb").write(init)
    open(os.path.join(out, "dram_golden.bin"), "wb").write(gold)
    open(os.path.join(out, "mmio.txt"), "w", newline="\n").write("\n".join(L) + "\n")
    print("writes %d rows_in %d rows_out %d dfc_unit_cycles %s high_addr %d" % (len(writes), kv["rows_in"], kv["rows_out"], kv.get("cycles", "?"),
                                                                              sum(1 for a, _ in writes if a >= 0x38)))


if __name__ == "__main__":
    main()
