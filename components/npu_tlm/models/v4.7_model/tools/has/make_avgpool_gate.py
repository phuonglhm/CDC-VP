#!/usr/bin/env python3
"""Instruction-path check of ELEM_WISE AVG_POOL (mode 5): a few pooling instructions with random int8 tensors, as a program
for tools/has/tb_has_npu_top (mmio.txt + dram_init.bin + dram_golden.bin, same format as fe_emit_insts.py).

Golden = the vector-unit drawing "Avg_Pool": per output, the k x k window is summed in a 16-bit accumulator (two's complement
wrap), multiplied by Avg_Scale (int32) and arithmetically right-shifted by Avg_Shift with round-half-up, then saturated to
int8 (the drawing shows no clamp; the model saturates and counts it). Tensors are C-order [c][h][w] int8, no padding.
Registers: A_ADDR input, OUT_ADDR output, MODE_PACK 5, STRIDE, extension IN_C/H/W, POOL_K, POOL_P 0, SO (Avg_Scale),
SHO (Avg_Shift); push 0x15.
Usage (repository root): python3 tools/has/make_avgpool_gate.py [out dir, default fe_work/has/avgpool_gate]
"""
import os
import sys

import numpy as np

out = sys.argv[1] if len(sys.argv) > 1 else "fe_work/has/avgpool_gate"
os.makedirs(out, exist_ok=True)
rng = np.random.default_rng(20260928)

# (C, H, W, k, s, Avg_Scale, Avg_Shift, lo, hi): int8 values in [lo, hi]; scale / 2^shift ~ 1 / k^2 unless stated
CASES = [
    (8, 20, 20, 2, 2, 16384, 16, -128, 127),     # 2x2 s2, exact 1/4
    (4, 13, 13, 3, 2, 7282, 16, -128, 127),      # 3x3 s2, ~1/9
    (3, 10, 10, 5, 1, 2621, 16, -128, 127),      # 5x5 s1, ~1/25
    (6, 7, 9, 3, 3, 7282, 16, -60, 60),          # non-square input, stride = k
    (2, 16, 16, 16, 16, 256, 16, -128, 127),     # global 16x16 (256 elements, fits int16)
    (2, 18, 18, 18, 18, 202, 16, 100, 127),      # 324 large values: the int16 accumulator wraps
    (5, 12, 12, 2, 2, 32768, 16, -128, 127),     # scale 1/2 of the sum: outputs saturate
    (4, 8, 8, 4, 4, -4096, 16, -128, 127),       # negative scale (SCALE_FMT signed)
]

A_ADDR, OUT_ADDR, STRIDE, MODE_PACK, PUSH_A = 0x40000444, 0x40000408, 0x40000424, 0x40000454, 0x40000310
EXT = 0x4000046C
X_IN_C, X_IN_H, X_IN_W, X_SO, X_SHO, X_POOL_K, X_POOL_P = 0, 1, 2, 21, 22, 23, 24

init, gold = bytearray(), bytearray()
mmio = ["# make_avgpool_gate.py: ELEM_WISE AVG_POOL instructions (mode 5)"]


def alloc(n):
    while len(init) % 64:
        init.append(0)
        gold.append(0)
    a = len(init)
    init.extend(bytes(n))
    gold.extend(bytes(n))
    return a


def w(addr, val):
    mmio.append("W %08x %x" % (addr, val & 0xFFFFFFFF))


def avgpool(x, k, s, scale, shift):
    c, h, wd = x.shape
    ho, wo = (h - k) // s + 1, (wd - k) // s + 1
    y = np.zeros((c, ho, wo), dtype=np.int8)
    for ci in range(c):
        for oy in range(ho):
            for ox in range(wo):
                acc = int(x[ci, oy * s:oy * s + k, ox * s:ox * s + k].astype(np.int64).sum())
                acc = ((acc + 0x8000) & 0xFFFF) - 0x8000               # 16-bit accumulator, two's complement
                p = acc * scale
                r = (p + (1 << (shift - 1))) >> shift if shift > 0 else p  # round half up (arithmetic shift)
                y[ci, oy, ox] = max(-128, min(127, r))
    return y


for i, (C, H, W, k, s, S, sh, lo, hi) in enumerate(CASES):
    x = rng.integers(lo, hi + 1, size=(C, H, W), dtype=np.int64).astype(np.int8)
    y = avgpool(x, k, s, S, sh)
    a = alloc(x.size)
    init[a:a + x.size] = x.tobytes()
    gold[a:a + x.size] = x.tobytes()
    o = alloc(y.size)
    gold[o:o + y.size] = y.tobytes()
    mmio.append("# [%d] ELEM_WISE AVG_POOL %dx%d s%d on [%d,%d,%d], Avg_Scale %d, Avg_Shift %d" % (i, k, k, s, C, H, W, S, sh))
    w(A_ADDR, a)
    w(OUT_ADDR, o)
    w(STRIDE, s)
    w(MODE_PACK, 5)
    for idx, v in ((X_IN_C, C), (X_IN_H, H), (X_IN_W, W), (X_POOL_K, k), (X_POOL_P, 0), (X_SO, S), (X_SHO, sh)):
        w(EXT + 4 * idx, v)
    w(PUSH_A, 0x15)

open(os.path.join(out, "dram_init.bin"), "wb").write(init)
open(os.path.join(out, "dram_golden.bin"), "wb").write(gold)
open(os.path.join(out, "mmio.txt"), "w").write("\n".join(mmio) + "\n")
print("wrote %s: %d instructions, DRAM %d B" % (out, len(CASES), len(init)))
