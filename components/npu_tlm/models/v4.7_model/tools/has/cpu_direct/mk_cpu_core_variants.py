#!/usr/bin/env python3
"""Two variants of the CPU program of mk_cpu_core.py (HAS_CPU_DIRECT=1): reads of the status registers of the SAURIA block, and a soft reset
(CONTROL bit 23) right after the START followed by a second run on the same memory halves (keep A / B / C).

usage: mk_cpu_core_variants.py <cpu_core dir> <status dir> <soft dir>
"""
import os
import shutil
import sys

STATUS = """# status reads (config_regs.sv): version, CONTROL (0xAC, ready, idle, done), counters, clear done, auto restart bit
P 0x0001101c 0xffffffff 0x00772020
P 0x00011000 0xff00000e 0xac00000e
P 0x00011014 0x0 0x0
P 0x00011018 0x0 0x0
W 0x00011000 0x2
P 0x00011000 0x2 0x0
W 0x00011000 0x80
P 0x00011000 0x80 0x80
W 0x00011000 0x0
P 0x00011000 0x80 0x0""".split("\n")
SOFT = """# soft reset right after the start (CONTROL bit 23), then the run again on the same halves (keep A / B / C)
W 0x00011000 0x800000
P 0x0001100c 0x1 0x1
W 0x0001100c 0x1
W 0x00011000 0x2
W 0x00011000 0x000e0001
P 0x0001100c 0x1 0x1
W 0x0001100c 0x1""".split("\n")


def main():
    src, st, so = sys.argv[1:4]
    lines = open(os.path.join(src, "mmio.txt")).read().split("\n")
    a, b = [], []
    j = 0
    while j < len(lines):
        a.append(lines[j]); b.append(lines[j])
        if lines[j] == "W 0x00011000 0x1" and j + 1 < len(lines) and lines[j + 1].startswith("P 0x0001100c"):
            b += SOFT   # the original poll and clear are replaced by those of the second run
            a += lines[j + 1:j + 3] + STATUS
            j += 3
            continue
        j += 1
    for d, text in ((st, a), (so, b)):
        os.makedirs(d, exist_ok=True)
        for f in ("dram_init.bin", "dram_golden.bin"): shutil.copy(os.path.join(src, f), os.path.join(d, f))
        open(os.path.join(d, "mmio.txt"), "w", newline="\n").write("\n".join(text))
    print("status and soft variants written")


if __name__ == "__main__":
    main()
