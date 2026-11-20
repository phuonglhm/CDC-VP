#!/usr/bin/env python3
"""Test program of the one-dimensional DMA driven by the CPU (HAS_CPU_DIRECT=1, has/has_cpu_direct.h) for tools/has/tb_has_npu_top.cpp.

For every local memory (SRAM A / B / C of SAURIA, vbank0 / 1 / 2 and sbank of the GVU scratchpad) the CPU copies a DRAM block into the memory and
back to another DRAM block, then checks it against the golden image ("K" line). Also: unaligned addresses, write-zero, the earlier address of the
register file (0x40000A00), and one command with an address outside every memory (it must not change anything; one line on stderr).
Each command: RADDR, WADDR, BTT, CONTROL (reader + writer), poll the interrupt status until both bits are set, clear it.

usage: mk_cpu_dma.py <out dir> [--dram-base 0x80000000] [--page 0x13000]
writes <out dir>/dram_init.bin, dram_golden.bin, mmio.txt, expect.txt (number of commands, bytes, model cycles of the commands)
"""
import os
import random
import sys

GVU = 0x00100000
MEMS = [  # name, bus base, bytes used (inside one buffer of the bank)
    ("srama", 0x00000000, 3000), ("sramb", 0x00040000, 2048), ("sramc", 0x00080000, 4096),
    ("vbank0", GVU + 0x00000, 25600), ("vbank1", GVU + 0x20000, 4096), ("vbank2", GVU + 0x40000, 8192), ("sbank", GVU + 0x80000, 3072),
]


def main():
    out = sys.argv[1]
    dram_base, page = 0x80000000, 0x13000
    a = sys.argv[2:]
    for i in range(0, len(a), 2):
        if a[i] == "--dram-base": dram_base = int(a[i + 1], 0)
        elif a[i] == "--page": page = int(a[i + 1], 0)
    os.makedirs(out, exist_ok=True)
    rng = random.Random(7919)
    size = 0x40000
    init = bytearray(rng.randrange(256) for _ in range(size))
    gold = bytearray(init)
    lines = ["# CPU-direct one-dimensional DMA test (has/has_cpu_direct.h)", "W 0x%08x 0x3" % (page + 0x04)]
    cmds = []

    def cmd(raddr, waddr, n, zero=False, base=page, poll=True):
        lines.append("W 0x%08x 0x%08x" % (base + 0x10, raddr))
        lines.append("W 0x%08x 0x%08x" % (base + 0x20, waddr))
        lines.append("W 0x%08x 0x%08x" % (base + 0x30, n))
        lines.append("W 0x%08x 0x%08x" % (base + 0x00, 0x102 if zero else 0x3))
        if poll:
            lines.append("P 0x%08x 0x2 0x2" % (base + 0x0C))   # writer done
        lines.append("W 0x%08x 0x3" % (base + 0x0C))
        cmds.append(n)

    src = 0x1000
    dst = 0x20000
    for name, bus, n in MEMS:
        lines.append("# %s: DRAM 0x%x -> 0x%08x -> DRAM 0x%x, %d bytes" % (name, src, bus, dst, n))
        cmd(dram_base + src, bus, n)
        cmd(bus, dram_base + dst, n)
        gold[dst:dst + n] = init[src:src + n]
        lines.append("K %x %d" % (dst, n))
        src += n + 7
        dst += n + 5
    # unaligned on both sides, through SRAM A at an odd offset
    lines.append("# unaligned through SRAM A")
    cmd(dram_base + 0x333, 0x00000000 + 0x11, 777)
    cmd(0x00000000 + 0x11, dram_base + dst + 3, 777)
    gold[dst + 3:dst + 3 + 777] = init[0x333:0x333 + 777]
    lines.append("K %x %d" % (dst + 3, 777))
    dst += 800
    # write-zero
    lines.append("# write-zero")
    cmd(0, dram_base + dst, 300, zero=True)
    gold[dst:dst + 300] = bytes(300)
    lines.append("K %x %d" % (dst, 300))
    dst += 304
    # the earlier address of the same register file
    lines.append("# register file at 0x40000A00")
    cmd(dram_base + 0x2000, dram_base + dst, 500, base=0x40000A00)
    gold[dst:dst + 500] = init[0x2000:0x2000 + 500]
    lines.append("K %x %d" % (dst, 500))
    dst += 504
    # an address outside every memory (between the GVU space and DRAM): nothing written, the status still rises
    lines.append("# outside every memory: refused")
    cmd(dram_base + 0x3000, 0x00300000, 64)
    lines.append("K %x %d" % (0, size))   # the whole image unchanged except the regions above
    with open(os.path.join(out, "dram_init.bin"), "wb") as f: f.write(init)
    with open(os.path.join(out, "dram_golden.bin"), "wb") as f: f.write(gold)
    with open(os.path.join(out, "mmio.txt"), "w", newline="\n") as f: f.write("\n".join(lines) + "\n")
    cyc = sum(6 + (n + 15) // 16 + 1 + 3 for n in cmds)
    good = sum(cmds[:-1])
    with open(os.path.join(out, "expect.txt"), "w", newline="\n") as f:
        f.write("commands %d bytes_moved %d bad_commands 1 command_cycles %d\n" % (len(cmds), good, cyc))
    print("commands %d bytes_moved %d bad_commands 1 command_cycles %d" % (len(cmds), good, cyc))


if __name__ == "__main__":
    main()
