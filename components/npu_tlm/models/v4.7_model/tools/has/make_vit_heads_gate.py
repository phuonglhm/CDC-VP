#!/usr/bin/env python3
"""FUSED_ATTN program shaped as the attention of one ViT-B/16 encoder block: 12 heads, NQ = L = 197 tokens, D = 64, no mask,
Q / K / V / O channel-major [D][L] (FLAGS bit 1), as fe_vit_block.py emits them. Random float heads (no network weights), one seed
per head; golden O = fe_ref_gvu.fused_attn (integer reference in Python, shares no code with the model).
The cycles of a FUSED_ATTN depend on NQ, L, D and the mask only, so the program has the cycles of the attention of one ViT block.
usage: python3 tools/has/make_vit_heads_gate.py <destination dir> [--seed S] [--heads N] [--tokens L]
run:   tools/has/tb_has_npu_top <destination dir>
"""
import argparse
import os
import random
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "fe"))
sys.path.insert(0, HERE)
import fe_ref_gvu as gv  # noqa: E402
import ref_program as rp  # noqa: E402

import numpy as np  # noqa: E402

K, AK = gv.DEFAULT_KNOBS, gv.ATTN_KNOBS
EXT = {n: rp.BASE + rp.EXT_BASE + 4 * i for i, n in enumerate(rp.EXT)}


def cm(t):  # [L][C] -> [C][L] int8 bytes
    return np.ascontiguousarray(np.asarray(t).T).astype(np.int8).tobytes()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--seed", type=int, default=1840)
    ap.add_argument("--heads", type=int, default=12)
    ap.add_argument("--tokens", type=int, default=197)
    # exponential table [1, 0, 0, ...]: only the row maximum counts, so a row with one maximum has sigma = 1 and the reciprocal lookup
    # gives E = 0 (stage-3 shift E + q - P = -1, a left shift); rows with a tied maximum keep E > 0
    ap.add_argument("--exp-e0", action="store_true")
    a = ap.parse_args()
    if a.exp_e0:
        gv.exp_table = lambda sx, k: [1] + [0] * 255
    init, gold = bytearray(), bytearray()

    def alloc(di, dg=None):
        while len(init) % 64:
            init.append(0)
            gold.append(0)
        ad = len(init)
        init.extend(di)
        gold.extend(dg if dg is not None else di)
        return ad

    mm = ["# make_vit_heads_gate.py: %d FUSED_ATTN heads, NQ = L = %d, D 64, no mask, channel-major (seed %d)" % (a.heads, a.tokens, a.seed)]
    w = lambda ad, v: mm.append("W %08x %x" % (ad, int(v) & 0xFFFFFFFF))
    L, D = a.tokens, 64
    for hd in range(a.heads):
        rng = random.Random("%d/vit_head/%d" % (a.seed, hd))
        g = lambda n, d: [[rng.gauss(0, 1.0) for _ in range(d)] for _ in range(n)]
        h = gv.make_head("h%d" % hd, g(L, D), g(L, D), g(L, D), [[False] * L for _ in range(L)])
        r = gv.fused_attn(h, K, AK)
        flags = int(AK["asym_zp"]) | (int(AK["mask_e"] == "zero") << 1) | (int(AK["av_zv_corr"]) << 2)
        blk = struct.pack("<I5i", 0x314E5441, h["Zq"], h["Zk"], h["Zv"], h["Zqk"], h["Zav"])
        blk += struct.pack("<qi", h["Mqk"], h["TSqk"]) + struct.pack("<qi", h["Mav"], h["TSav"])
        blk += struct.pack("<II", flags, 0) + struct.pack("<256i", *r["exp"])
        pa, qa, ka, va = alloc(blk), alloc(cm(h["Q"])), alloc(cm(h["K"])), alloc(cm(h["V"]))
        oa = alloc(bytes(L * D), cm(r["O"]))
        mm.append("# [%d] FUSED_ATTN %s L %d D %d mask no, V channel-major" % (hd, h["name"], L, D))
        for ad, v in ((0x40000444, qa), (0x40000448, ka), (0x4000044C, va), (0x40000450, L), (0x40000458, D),
                      (0x40000408, oa), (EXT["ROWS"], L), (EXT["PARAM_ADDR"], pa), (EXT["FLAGS"], 2)):
            w(ad, v)
        w(0x40000310, 0x13)
        print("[vit-heads] %-4s L %3d D %d ovf16 %d rows with E = 0: %d" % (h["name"], L, D, r["ovf16"], sum(st[3] == 0 for st in r["stats"])))
    while len(init) % 4096:
        init.append(0)
        gold.append(0)
    os.makedirs(a.out, exist_ok=True)
    open(os.path.join(a.out, "dram_init.bin"), "wb").write(init)
    open(os.path.join(a.out, "dram_golden.bin"), "wb").write(gold)
    open(os.path.join(a.out, "mmio.txt"), "w").write("\n".join(mm) + "\n")
    print("[vit-heads] %d heads, DRAM %d B -> %s" % (a.heads, len(init), a.out))


if __name__ == "__main__":
    main()
