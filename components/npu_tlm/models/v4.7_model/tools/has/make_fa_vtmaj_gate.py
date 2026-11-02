#!/usr/bin/env python3
"""FUSED_ATTN program with V read token-major (FLAGS bit 2, row pitch IN_C) next to the same heads with V channel-major.

Heads: tools/fe/fe_ref_gvu.random_heads (no network weights). Every head is pushed twice: Q, K channel-major [D][L] (FLAGS
bit 1) with V channel-major [D][L], then with V token-major in a wider tensor (row i of V at V_ADDR + i * PITCH, V_ADDR at
column OFFSET of the row, IN_C = PITCH; FLAGS bits 1 and 2), as a head of a GEMM_FUSED output written with X_TMAJ_C0 is.
Golden O of both pushes = fe_ref_gvu.fused_attn (integer reference in Python, shares no code with the model), channel-major.
usage: python3 tools/has/make_fa_vtmaj_gate.py <destination dir> [--seed S] [--pitch P] [--offset C] [--heads N]
run:   tools/has/tb_has_npu_top <destination dir>   (every push must PASS)
"""
import argparse
import os
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


class Image:
    def __init__(self):
        self.init, self.gold = bytearray(), bytearray()

    def alloc(self, di, dg=None):
        while len(self.init) % 64:
            self.init.append(0)
            self.gold.append(0)
        a = len(self.init)
        self.init.extend(di)
        self.gold.extend(dg if dg is not None else di)
        return a


def cm(t):  # [L][C] -> [C][L] int8 bytes
    return np.ascontiguousarray(np.asarray(t).T).astype(np.int8).tobytes()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--seed", type=int, default=1662)
    ap.add_argument("--pitch", type=int, default=96)
    ap.add_argument("--offset", type=int, default=16)
    ap.add_argument("--heads", type=int, default=6)
    a = ap.parse_args()
    img, mm, n = Image(), ["# make_fa_vtmaj_gate.py: FUSED_ATTN, V channel-major then token-major (pitch %d, offset %d)"
                           % (a.pitch, a.offset)], 0
    w = lambda ad, v: mm.append("W %08x %x" % (ad, int(v) & 0xFFFFFFFF))
    for h in gv.random_heads(a.seed)[:a.heads]:
        r = gv.fused_attn(h, K, AK)
        L, D = len(h["Q"]), len(h["Q"][0])
        flags = int(AK["asym_zp"]) | (int(AK["mask_e"] == "zero") << 1) | (int(AK["av_zv_corr"]) << 2)
        blk = struct.pack("<I5i", 0x314E5441, h["Zq"], h["Zk"], h["Zv"], h["Zqk"], h["Zav"])
        blk += struct.pack("<qi", h["Mqk"], h["TSqk"]) + struct.pack("<qi", h["Mav"], h["TSav"])
        blk += struct.pack("<II", flags, 0) + struct.pack("<256i", *r["exp"])
        pa, qa, ka = img.alloc(blk), img.alloc(cm(h["Q"])), img.alloc(cm(h["K"]))
        masked = any(v == -128 for row in h["mask"] for v in row)
        ma = img.alloc(np.asarray(h["mask"]).astype(np.int8).tobytes()) if masked else 0
        vt = np.zeros((L, a.pitch), np.int8)
        vt[:, a.offset:a.offset + D] = np.asarray(h["V"])
        for tm in (False, True):
            va = img.alloc(vt.tobytes()) + a.offset if tm else img.alloc(cm(h["V"]))
            oa = img.alloc(bytes(L * D), cm(r["O"]))
            mm.append("# [%d] FUSED_ATTN %s L %d D %d mask %s, V %s" % (n, h["name"], L, D, "yes" if masked else "no",
                                                                     "token-major" if tm else "channel-major"))
            for ad, v in ((0x40000444, qa), (0x40000448, ka), (0x4000044C, va), (0x40000450, L), (0x40000458, D),
                          (0x40000408, oa), (EXT["ROWS"], L), (EXT["PARAM_ADDR"], pa), (EXT["FLAGS"], 2 | (4 if tm else 0))):
                w(ad, v)
            if masked:
                w(EXT["MASK_ADDR"], ma)
            if tm:
                w(EXT["IN_C"], a.pitch)
            w(0x40000310, 0x13)
            n += 1
        print("[fa-vtmaj] %-18s L %3d D %d mask %s ovf16 %d" % (h["name"], L, D, "yes" if masked else "no", r["ovf16"]))
    while len(img.init) % 4096:
        img.init.append(0)
        img.gold.append(0)
    os.makedirs(a.out, exist_ok=True)
    open(os.path.join(a.out, "dram_init.bin"), "wb").write(img.init)
    open(os.path.join(a.out, "dram_golden.bin"), "wb").write(img.gold)
    open(os.path.join(a.out, "mmio.txt"), "w").write("\n".join(mm) + "\n")
    print("[fa-vtmaj] %d pushes, DRAM %d B -> %s" % (n, len(img.init), a.out))


if __name__ == "__main__":
    main()
