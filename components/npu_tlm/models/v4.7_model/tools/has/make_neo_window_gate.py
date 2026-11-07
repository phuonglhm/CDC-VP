#!/usr/bin/env python3
"""Check program for HAS_HOST=neo (NEO_REGISTER_FILE.md): max pools 3x3 s1, 3x3 s2, 5x5 s1 on [40,32,32] (the shapes of the MAXPOOL test of
tb_neo_top.sv on a map the model's scratchpad takes) and an ADD whose scales lie in the int32 range of the model's ADD (the numbers of the hardware
RESIDUAL test, sa = 2^30, are outside it and are refused as B-ADDMAP).
usage: python3 make_neo_window_gate.py <out dir>   -> mmio.txt, dram_init.bin, dram_golden.bin (golden = init: the gate compares two runs, not a reference)"""
import os, random, sys

out = sys.argv[1]
os.makedirs(out, exist_ok=True)
B = 0x40000000
R = dict(IN=0x400, OUT=0x408, STRIDE=0x424, A=0x444, Bb=0x448, LEN=0x450, MODE=0x454, ALEN=0x464, BLEN=0x468)
EXT = 0x46C
X = {n: i for i, n in enumerate(["IN_C", "IN_H", "IN_W", "OUT_C", "OUT_H", "OUT_W", "TILE_COUT", "TILE_H", "TILE_W", "SCALE_ADDR", "SHIFT_ADDR", "LUT_ADDR",
                                 "ZP_OUT", "FLAGS", "ZP_A", "ZP_B", "ZP_O", "SA", "SHA", "SB", "SHB", "SO", "SHO", "POOL_K", "POOL_P", "POOL_MODE"])}
lines = []


def w(a, v):
    lines.append("W %x %x" % (a, v & 0xFFFFFFFF))


def ext(n, v):
    w(B + EXT + 4 * X[n], v)


def pool(src, dst, c, h, wd, k, s):
    w(B + R["A"], src); w(B + R["OUT"], dst); w(B + R["MODE"], 1); w(B + R["STRIDE"], s)
    ext("IN_C", c); ext("IN_H", h); ext("IN_W", wd); ext("POOL_K", k); ext("POOL_P", 0); ext("POOL_MODE", 0)
    w(B + 0x310, 0x15)


C, H, W = 40, 32, 32
src = 0x10000
random.seed(94)
img = bytearray(0x100000)
for i in range(C * H * W):
    img[src + i] = random.randrange(256)
n_add = 25600
a_op, b_op, o_add = 0x50000, 0x58000, 0xF0000
for i in range(n_add):
    img[a_op + i] = random.randrange(256)
    img[b_op + i] = random.randrange(256)
lines.append("# [0] MAX_POOL 3x3 s1")
pool(src, 0x60000, C, H, W, 3, 1)
lines.append("# [1] ADD (scales inside the model int32 range)")
w(B + R["A"], a_op); w(B + R["Bb"], b_op); w(B + R["OUT"], o_add); w(B + R["LEN"], n_add); w(B + R["MODE"], 0); w(B + R["ALEN"], n_add); w(B + R["BLEN"], n_add)
for n, v in (("ZP_A", -3), ("ZP_B", 7), ("ZP_O", -5), ("SA", 40000), ("SHA", 0), ("SB", 33000), ("SHB", 0), ("SO", 1), ("SHO", 16)):
    ext(n, v)
w(B + 0x310, 0x15)
lines.append("# [2] MAX_POOL 3x3 s2")
pool(src, 0xA0000, C, H, W, 3, 2)
lines.append("# [3] MAX_POOL 5x5 s1")
pool(src, 0xB0000, C, H, W, 5, 1)
open(os.path.join(out, "mmio.txt"), "w").write("\n".join(lines) + "\n")
open(os.path.join(out, "dram_init.bin"), "wb").write(bytes(img))
open(os.path.join(out, "dram_golden.bin"), "wb").write(bytes(img))
print("window program: %d lines, image %d B" % (len(lines), len(img)))
