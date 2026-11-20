#!/usr/bin/env python3
"""Gate for the residual add (ELEM_WISE ADD) on revision 2 (HAS_NEO_REV=2): the descriptor form of the model (zero points, scales, shifts) with the
parameters of the whole networks and of the hardware test of tb_neo_top.sv, the golden computed here by integer arithmetic straight from the descriptor
(Requant((a - zpA) * SA + (b - zpB) * SB, SO, sO, zpO), round half up, clamp to int8), independent of the model.
usage: python3 make_g2_add_gate.py <out dir>   -> mmio.txt, dram_init.bin, dram_golden.bin"""
import os, random, sys

out = sys.argv[1]
os.makedirs(out, exist_ok=True)
B = 0x40000000
R = dict(OUT=0x408, A=0x444, Bb=0x448, LEN=0x450, MODE=0x454, ALEN=0x464, BLEN=0x468)
EXT = 0x46C
X = {n: i for i, n in enumerate(["IN_C", "IN_H", "IN_W", "OUT_C", "OUT_H", "OUT_W", "TILE_COUT", "TILE_H", "TILE_W", "SCALE_ADDR", "SHIFT_ADDR", "LUT_ADDR",
                                 "ZP_OUT", "FLAGS", "ZP_A", "ZP_B", "ZP_O", "SA", "SHA", "SB", "SHB", "SO", "SHO", "POOL_K", "POOL_P", "POOL_MODE"])}
lines = []


def w(a, v):
    lines.append("W %x %x" % (a, v & 0xFFFFFFFF))


def ext(n, v):
    w(B + EXT + 4 * X[n], v)


def s8(v):
    return v - 256 if v >= 128 else v


# name, n, zpA, zpB, zpO, SA, SB, SO, sO
cases = [
    ("net (YOLO, 2^14 / 2^14)", 151296, 0, 0, 0, 16384, 16384, 1, 14),
    ("ViT b0 x1", 151296, 0, 0, 0, 17705, 10593, 1, 14),
    ("ViT b4 y", 151296, 0, 0, 0, 8970, 9180, 1, 14),
    ("ViT b9 x1", 151296, 0, 0, 0, 16543, 1069, 1, 14),
    ("tail (n not a multiple of 32), net scales", 100003, 0, 0, 0, 16384, 16384, 1, 14),
    ("HW test tb_neo_top (16x160x160)", 409600, -125, -125, -125, 1073741824, 1055666676, 1, 30),
]
random.seed(2410)
img = bytearray(0x400000)
gold = bytearray(0x400000)
cur = 0x1000
for k, (name, n, za, zb, zo, SA, SB, SO, sO) in enumerate(cases):
    a_op, b_op, o_op = cur, cur + ((n + 0xFFF) & ~0xFFF), cur + 2 * ((n + 0xFFF) & ~0xFFF)
    cur = o_op + ((n + 0xFFF) & ~0xFFF)
    assert cur <= len(img), (name, hex(cur))
    for i in range(n):
        img[a_op + i] = random.randrange(256)
        img[b_op + i] = random.randrange(256)
    lines.append("# [%d] ADD %s, %d elements" % (k, name, n))
    w(B + R["A"], a_op); w(B + R["Bb"], b_op); w(B + R["OUT"], o_op); w(B + R["LEN"], n); w(B + R["MODE"], 0); w(B + R["ALEN"], n); w(B + R["BLEN"], n)
    for nm, v in (("ZP_A", za), ("ZP_B", zb), ("ZP_O", zo), ("SA", SA), ("SHA", 0), ("SB", SB), ("SHB", 0), ("SO", SO), ("SHO", sO)):
        ext(nm, v)
    w(B + 0x310, 0x15)
    half = (1 << (sO - 1)) if sO > 0 else 0
    for i in range(n):
        a, b = s8(img[a_op + i]), s8(img[b_op + i])
        acc = (a - za) * SA + (b - zb) * SB
        y = ((acc * SO + half) >> sO) + zo
        gold[o_op + i] = (max(-128, min(127, y))) & 0xFF
# the golden image: the inputs unchanged, the outputs as computed
res = bytearray(img)
cur = 0x1000
for (name, n, *_r) in cases:
    sz = (n + 0xFFF) & ~0xFFF
    o_op = cur + 2 * sz
    res[o_op:o_op + n] = gold[o_op:o_op + n]
    cur = o_op + sz
open(os.path.join(out, "mmio.txt"), "w").write("\n".join(lines) + "\n")
open(os.path.join(out, "dram_init.bin"), "wb").write(bytes(img))
open(os.path.join(out, "dram_golden.bin"), "wb").write(bytes(res))
print("g2 add gate: %d cases, %d lines, image %d B" % (len(cases), len(lines), len(img)))
