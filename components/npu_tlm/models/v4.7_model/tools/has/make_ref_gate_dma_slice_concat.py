#!/usr/bin/env python3
"""Gate program exercising HasDma 4-D Slice/Concat/Tile descriptors on HasNpuTop.

Generates a program directory (mmio.txt, dram_init.bin, dram_golden.bin) with 27 instructions across 3 blocks:
  Block 1 [0..9]  Multi-tile 3x3 GEMM_FUSED + C/H/W DMA_SLICE (contiguous & strided) + MAX_POOL + C/H/W DMA_CONCAT + ADD
  Block 2 [10..17] YOLOv8m C2f Block: cv1 (1x1 GEMM_FUSED) -> DMA_SLICE split (branch0, branch1) ->
                   Bottleneck (3x3 GEMM_FUSED + ADD shortcut) -> 3-way DMA_CONCAT ([48,16,16]) ->
                   cv2 (1x1 GEMM_FUSED with TILE_CIN=32 2-pass Cin split -> [32,16,16])
  Block 3 [18..26] YOLOv8m SPPF Block + AvgPool: sppf_cv1 (1x1 GEMM_FUSED -> [16,16,16]) ->
                   3x chained MAX_POOL 5x5 s1 p2 (y1, y2, y3) -> 4-way DMA_CONCAT ([64,16,16]) ->
                   sppf_cv2 (1x1 GEMM_FUSED with TILE_CIN=32 2-pass Cin split -> [32,16,16]) ->
                   ELEM_WISE AVG_POOL 2x2 s2 (-> [32,8,8])
"""
import argparse
import os

import numpy as np

B = 0x40000000
R = {"in": 0x400, "w": 0x404, "out": 0x408, "bias": 0x40C, "m": 0x410, "k": 0x414, "n": 0x418, "kh": 0x41C, "kw": 0x420,
     "stride": 0x424, "pad": 0x428, "act": 0x42C, "has_skip": 0x430, "skip": 0x434, "in_scale": 0x438, "w_scale": 0x43C,
     "out_scale": 0x440, "a": 0x444, "b": 0x448, "len": 0x450, "mode": 0x454, "scale_a": 0x458, "scale_b": 0x45C,
     "scale_o": 0x460, "a_len": 0x464, "b_len": 0x468}
X = ["IN_C", "IN_H", "IN_W", "OUT_C", "OUT_H", "OUT_W", "TILE_COUT", "TILE_H", "TILE_W", "SCALE_ADDR", "SHIFT_ADDR",
     "LUT_ADDR", "ZP_OUT", "FLAGS", "ZP_A", "ZP_B", "ZP_O", "SA", "SHA", "SB", "SHB", "SO", "SHO", "POOL_K", "POOL_P",
     "POOL_MODE", "TILE_CIN", "Y_USED", "ROWS", "PARAM_ADDR", "MASK_ADDR", "PAD_VALUE", "PAD_T", "PAD_L", "PAD_B", "PAD_R",
     "DILATION"]


def conv_golden(x_chw, w_blocks, bias, scales, shifts, zp_out, cout_t, kh=3, kw=3, pad=1, act=1):
    cin, h, w = x_chw.shape
    cout = bias.size
    xp = np.pad(x_chw.astype(np.int32), ((0, 0), (pad, pad), (pad, pad)), mode="constant", constant_values=0) if pad > 0 else x_chw.astype(np.int32)
    out = np.zeros((cout, h, w), dtype=np.int8)
    for c0 in range(0, cout, cout_t):
        nch = min(cout_t, cout - c0)
        wb = w_blocks[c0 // cout_t].astype(np.int32)  # [cin, kh, kw, nch]
        for oc in range(nch):
            c = c0 + oc
            acc = np.full((h, w), int(bias[c]), dtype=np.int64)
            for ic in range(cin):
                for ky in range(kh):
                    for kx in range(kw):
                        acc += xp[ic, ky:ky + h, kx:kx + w] * int(wb[ic, ky, kx, oc])
            S = int(scales[c])
            sh = int(shifts[c])
            prod = acc * S
            rq = (prod + (1 << (sh - 1))) >> sh if sh > 0 else prod
            rq = np.clip(rq, -32768, 32767) + int(zp_out)
            if act == 1:
                rq = np.maximum(rq, 0)
            out[c] = np.clip(rq, -128, 127).astype(np.int8)
    return out


def maxpool_golden(x_chw, k=5, p=2, s=1):
    c, h, w = x_chw.shape
    xp = np.pad(x_chw, ((0, 0), (p, p), (p, p)), mode="constant", constant_values=-128)
    out = np.full((c, h, w), -128, dtype=np.int8)
    for y in range(h):
        for x in range(w):
            out[:, y, x] = xp[:, y * s:y * s + k, x * s:x * s + k].max(axis=(1, 2))
    return out


def avgpool_golden(x_chw, k=2, s=2, scale=16384, shift=16):
    c, h, w = x_chw.shape
    ho, wo = (h - k) // s + 1, (w - k) // s + 1
    out = np.zeros((c, ho, wo), dtype=np.int8)
    for ci in range(c):
        for oy in range(ho):
            for ox in range(wo):
                acc = int(x_chw[ci, oy * s:oy * s + k, ox * s:ox * s + k].astype(np.int64).sum())
                acc = ((acc + 0x8000) & 0xFFFF) - 0x8000
                p = acc * scale
                r = (p + (1 << (shift - 1))) >> shift if shift > 0 else p
                out[ci, oy, ox] = max(-128, min(127, r))
    return out


def add_golden(a_chw, b_chw, zpA, zpB, zpO, SA, sA, SB, sB, SO, sO):
    def rshift_rhu(v, sh):
        return (v + (1 << (sh - 1))) >> sh if sh > 0 else v

    a32 = np.clip(rshift_rhu((a_chw.astype(np.int64) - zpA) * SA, sA), -2147483648, 2147483647)
    b32 = np.clip(rshift_rhu((b_chw.astype(np.int64) - zpB) * SB, sB), -2147483648, 2147483647)
    s32 = np.clip(a32 + b32, -2147483648, 2147483647)
    rq = np.clip(rshift_rhu(s32 * SO, sO), -32768, 32767) + zpO
    return np.clip(rq, -128, 127).astype(np.int8)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("out")
    ap.add_argument("--seed", type=int, default=7)
    a = ap.parse_args()
    rng = np.random.default_rng(a.seed)
    img_init = bytearray(512 * 1024)
    img_gold = bytearray(512 * 1024)
    top = [0x1000]

    def alloc(n):
        p = top[0]
        top[0] = (p + n + 63) & ~63
        return p

    def put_init(arr):
        b = np.asarray(arr).tobytes()
        p = alloc(len(b))
        img_init[p:p + len(b)] = b
        img_gold[p:p + len(b)] = b
        return p

    def put_gold(p, arr):
        b = np.asarray(arr).tobytes()
        img_gold[p:p + len(b)] = b

    def build_conv(label, x_addr, x_arr, cout, kh, kw, pad, cout_t, tile_h, tile_w, act=1, zp_out=2, tile_cin=0):
        cin, h, w = x_arr.shape
        w_blocks = []
        K = cin * kh * kw
        w_addr = alloc(cout * K)
        for c0 in range(0, cout, cout_t):
            nch = min(cout_t, cout - c0)
            blk = rng.integers(-16, 17, size=(cin, kh, kw, nch), dtype=np.int8)
            w_blocks.append(blk)
            off = w_addr + c0 * K
            img_init[off:off + blk.size] = blk.tobytes()
            img_gold[off:off + blk.size] = blk.tobytes()
        bias_arr = rng.integers(-500, 501, size=cout, dtype="<i4")
        scales_arr = rng.integers(1 << 28, 1 << 29, size=cout, dtype="<i4")
        shifts_arr = rng.integers(36, 39, size=cout, dtype="<i4")
        bias_addr = put_init(bias_arr)
        scale_addr = put_init(scales_arr)
        shift_addr = put_init(shifts_arr)
        y_gold = conv_golden(x_arr, w_blocks, bias_arr, scales_arr, shifts_arr, zp_out, cout_t, kh=kh, kw=kw, pad=pad, act=act)
        y_addr = alloc(y_gold.size)
        put_gold(y_addr, y_gold)
        regs = {
            "in": x_addr, "w": w_addr, "out": y_addr, "bias": bias_addr,
            "kh": kh, "kw": kw, "stride": 1, "pad": pad, "act": act, "has_skip": 0,
            "IN_C": cin, "IN_H": h, "IN_W": w, "OUT_C": cout, "OUT_H": h, "OUT_W": w,
            "TILE_COUT": cout_t, "TILE_H": tile_h, "TILE_W": tile_w,
            "SCALE_ADDR": scale_addr, "SHIFT_ADDR": shift_addr, "ZP_OUT": zp_out, "FLAGS": 0,
        }
        if tile_cin > 0:
            regs["TILE_CIN"] = tile_cin
        return (label, regs, 0x12), y_addr, y_gold

    prog = []

    # =========================================================================
    # Block 1 [0..9]: Multi-axis (C, H, W) Slice / Concat + MaxPool + Residual
    # =========================================================================
    cin, h, w = 8, 16, 16
    x0_arr = rng.integers(-80, 81, size=(cin, h, w), dtype=np.int8)
    x0 = put_init(x0_arr)

    # [0] GEMM_FUSED 3x3 tiled 16x8x8 (8 tiles)
    ins0, o0, o0_gold = build_conv("GEMM_FUSED 3x3 cout=32 H=16 W=16 tiled 16x8x8 (8 tiles)", x0, x0_arr, 32, 3, 3, 1, 16, 8, 8)
    prog.append(ins0)

    # [1] DMA_SLICE branch 0 along C: c=0..16
    s0_gold = o0_gold[0:16, :, :].copy()
    s0 = alloc(s0_gold.size)
    put_gold(s0, s0_gold)
    prog.append(("ELEM_WISE DMA_SLICE branch 0 along C [c=0..16, H=16, W=16]", {
        "a": o0, "b": 0, "out": s0, "mode": 6, "stride": 1,
        "IN_C": 32, "IN_H": 16, "IN_W": 16, "POOL_MODE": 0, "POOL_P": 0, "POOL_K": 16}, 0x15))

    # [2] DMA_SLICE branch 1 along C: c=16..32
    s1_gold = o0_gold[16:32, :, :].copy()
    s1 = alloc(s1_gold.size)
    put_gold(s1, s1_gold)
    prog.append(("ELEM_WISE DMA_SLICE branch 1 along C [c=16..32, H=16, W=16]", {
        "a": o0, "b": 0, "out": s1, "mode": 6, "stride": 1,
        "IN_C": 32, "IN_H": 16, "IN_W": 16, "POOL_MODE": 0, "POOL_P": 16, "POOL_K": 16}, 0x15))

    # [3] MAX_POOL 5x5 s1 p2 on branch 1
    mp1_gold = maxpool_golden(s1_gold, k=5, p=2, s=1)
    mp1 = alloc(mp1_gold.size)
    put_gold(mp1, mp1_gold)
    prog.append(("ELEM_WISE MAX_POOL 5x5 s1 p2 on branch 1 [16, 16, 16]", {
        "a": s1, "b": 0, "out": mp1, "mode": 1, "stride": 1,
        "IN_C": 16, "IN_H": 16, "IN_W": 16, "POOL_K": 5, "POOL_P": 2}, 0x15))

    # [4] DMA_CONCAT along C: branch 0 (a) + mp1 (b) -> cat_out [32, 16, 16]
    cat_gold = np.concatenate([s0_gold, mp1_gold], axis=0)
    cat_out = alloc(cat_gold.size)
    put_gold(cat_out, cat_gold)
    prog.append(("ELEM_WISE DMA_CONCAT along C: branch 0 (a) + pooled branch 1 (b) -> [32, 16, 16]", {
        "a": s0, "b": mp1, "out": cat_out, "mode": 7, "stride": 1,
        "IN_C": 32, "IN_H": 16, "IN_W": 16, "OUT_C": 32, "OUT_H": 16, "OUT_W": 16,
        "POOL_MODE": 0, "POOL_P": 0, "POOL_K": 16}, 0x15))

    # [5] DMA_SLICE along H (axis=1): rows y=4..12 -> [32, 8, 16]
    sh_gold = cat_gold[:, 4:12, :].copy()
    sh_out = alloc(sh_gold.size)
    put_gold(sh_out, sh_gold)
    prog.append(("ELEM_WISE DMA_SLICE along H: rows [y=4..12] -> [32, 8, 16]", {
        "a": cat_out, "b": 0, "out": sh_out, "mode": 6, "stride": 1,
        "IN_C": 32, "IN_H": 16, "IN_W": 16, "POOL_MODE": 1, "POOL_P": 4, "POOL_K": 8}, 0x15))

    # [6] DMA_CONCAT along H (axis=1): sh_out [32, 8, 16] + sh_out [32, 8, 16] -> [32, 16, 16]
    cat_h_gold = np.concatenate([sh_gold, sh_gold], axis=1)
    cat_h_out = alloc(cat_h_gold.size)
    put_gold(cat_h_out, cat_h_gold)
    prog.append(("ELEM_WISE DMA_CONCAT along H: [32, 8, 16] + [32, 8, 16] -> [32, 16, 16]", {
        "a": sh_out, "b": sh_out, "out": cat_h_out, "mode": 7, "stride": 1,
        "IN_C": 32, "IN_H": 16, "IN_W": 16, "OUT_C": 32, "OUT_H": 16, "OUT_W": 16,
        "POOL_MODE": 1, "POOL_P": 0, "POOL_K": 8}, 0x15))

    # [7] DMA_SLICE along W (axis=2) with stride 2: even cols x=0..16:2 -> [32, 16, 8]
    sw_even_gold = cat_gold[:, :, 0:16:2].copy()
    sw_even_out = alloc(sw_even_gold.size)
    put_gold(sw_even_out, sw_even_gold)
    prog.append(("ELEM_WISE DMA_SLICE along W with stride 2: cols [x=0..16:2] -> [32, 16, 8]", {
        "a": cat_out, "b": 0, "out": sw_even_out, "mode": 6, "stride": 2,
        "IN_C": 32, "IN_H": 16, "IN_W": 16, "POOL_MODE": 2, "POOL_P": 0, "POOL_K": 16}, 0x15))

    # [8] DMA_CONCAT along W (axis=2): sw_even_out [32, 16, 8] + sw_even_out [32, 16, 8] -> [32, 16, 16]
    cat_w_gold = np.concatenate([sw_even_gold, sw_even_gold], axis=2)
    cat_w_out = alloc(cat_w_gold.size)
    put_gold(cat_w_out, cat_w_gold)
    prog.append(("ELEM_WISE DMA_CONCAT along W: [32, 16, 8] + [32, 16, 8] -> [32, 16, 16]", {
        "a": sw_even_out, "b": sw_even_out, "out": cat_w_out, "mode": 7, "stride": 1,
        "IN_C": 32, "IN_H": 16, "IN_W": 16, "OUT_C": 32, "OUT_H": 16, "OUT_W": 16,
        "POOL_MODE": 2, "POOL_P": 0, "POOL_K": 8}, 0x15))

    # [9] ELEM_WISE ADD (combined cat_gold + o0_gold)
    zpA, zpB, zpO = 1, -2, 3
    SA, sA, SB, sB, SO, sO = 16384, 14, 16384, 14, 16384, 15
    add_gold = add_golden(cat_gold, o0_gold, zpA, zpB, zpO, SA, sA, SB, sB, SO, sO)
    add_out = alloc(add_gold.size)
    put_gold(add_out, add_gold)
    prog.append(("ELEM_WISE ADD concatenated [32, 16, 16] + conv0 [32, 16, 16]", {
        "a": cat_out, "b": o0, "out": add_out, "mode": 0, "stride": 1,
        "len": add_gold.size, "a_len": add_gold.size, "b_len": add_gold.size,
        "IN_C": 32, "IN_H": 16, "IN_W": 16, "OUT_C": 32, "OUT_H": 16, "OUT_W": 16,
        "ZP_A": zpA & 0xFFFFFFFF, "ZP_B": zpB & 0xFFFFFFFF, "ZP_O": zpO & 0xFFFFFFFF,
        "SA": SA, "SHA": sA, "SB": SB, "SHB": sB, "SO": SO, "SHO": sO}, 0x15))

    # =========================================================================
    # Block 2 [10..17]: YOLOv8m C2f Block (split + bottleneck + 3-way concat + cv2)
    # =========================================================================
    # [10] C2f cv1: 1x1 GEMM_FUSED [32, 16, 16] -> [32, 16, 16] (tiled 16x8x8 = 8 tiles)
    ins_cv1, c2f_cv1, c2f_cv1_gold = build_conv("C2f cv1: GEMM_FUSED 1x1 [32,16,16] -> [32,16,16] (8 tiles)", add_out, add_gold, 32, 1, 1, 0, 16, 8, 8)
    prog.append(ins_cv1)

    # [11] C2f split branch 0: DMA_SLICE c=0..16 -> [16, 16, 16]
    c2f_b0_gold = c2f_cv1_gold[0:16, :, :].copy()
    c2f_b0 = alloc(c2f_b0_gold.size)
    put_gold(c2f_b0, c2f_b0_gold)
    prog.append(("C2f split branch 0: ELEM_WISE DMA_SLICE c=0..16 -> [16,16,16]", {
        "a": c2f_cv1, "b": 0, "out": c2f_b0, "mode": 6, "stride": 1,
        "IN_C": 32, "IN_H": 16, "IN_W": 16, "POOL_MODE": 0, "POOL_P": 0, "POOL_K": 16}, 0x15))

    # [12] C2f split branch 1: DMA_SLICE c=16..32 -> [16, 16, 16]
    c2f_b1_gold = c2f_cv1_gold[16:32, :, :].copy()
    c2f_b1 = alloc(c2f_b1_gold.size)
    put_gold(c2f_b1, c2f_b1_gold)
    prog.append(("C2f split branch 1: ELEM_WISE DMA_SLICE c=16..32 -> [16,16,16]", {
        "a": c2f_cv1, "b": 0, "out": c2f_b1, "mode": 6, "stride": 1,
        "IN_C": 32, "IN_H": 16, "IN_W": 16, "POOL_MODE": 0, "POOL_P": 16, "POOL_K": 16}, 0x15))

    # [13] C2f bottleneck conv: 3x3 GEMM_FUSED [16, 16, 16] -> [16, 16, 16] (tiled 16x8x8 = 4 tiles)
    ins_bn, bn_conv, bn_conv_gold = build_conv("C2f bottleneck: GEMM_FUSED 3x3 [16,16,16] -> [16,16,16] (4 tiles)", c2f_b1, c2f_b1_gold, 16, 3, 3, 1, 16, 8, 8)
    prog.append(ins_bn)

    # [14] C2f bottleneck shortcut ADD: bn_conv + c2f_b1 -> bneck1 [16, 16, 16]
    bneck1_gold = add_golden(bn_conv_gold, c2f_b1_gold, zpA, zpB, zpO, SA, sA, SB, sB, SO, sO)
    bneck1 = alloc(bneck1_gold.size)
    put_gold(bneck1, bneck1_gold)
    prog.append(("C2f bottleneck shortcut: ELEM_WISE ADD [16,16,16]", {
        "a": bn_conv, "b": c2f_b1, "out": bneck1, "mode": 0, "stride": 1,
        "len": bneck1_gold.size, "a_len": bneck1_gold.size, "b_len": bneck1_gold.size,
        "IN_C": 16, "IN_H": 16, "IN_W": 16, "OUT_C": 16, "OUT_H": 16, "OUT_W": 16,
        "ZP_A": zpA & 0xFFFFFFFF, "ZP_B": zpB & 0xFFFFFFFF, "ZP_O": zpO & 0xFFFFFFFF,
        "SA": SA, "SHA": sA, "SB": SB, "SHB": sB, "SO": SO, "SHO": sO}, 0x15))

    # [15..16] C2f 3-way DMA_CONCAT along C: [c2f_b0, bneck1, bn_conv] -> c2f_cat [48, 16, 16]
    c2f_cat_gold = np.concatenate([c2f_b0_gold, bneck1_gold, bn_conv_gold], axis=0)
    c2f_cat = alloc(c2f_cat_gold.size)
    put_gold(c2f_cat, c2f_cat_gold)
    prog.append(("C2f 3-way DMA_CONCAT (part 0+1): branch0 [c=0..16] + bneck1 [c=16..32] -> [48,16,16]", {
        "a": c2f_b0, "b": bneck1, "out": c2f_cat, "mode": 7, "stride": 1,
        "IN_C": 32, "IN_H": 16, "IN_W": 16, "OUT_C": 32, "OUT_H": 16, "OUT_W": 16,
        "POOL_MODE": 0, "POOL_P": 0, "POOL_K": 16}, 0x15))
    prog.append(("C2f 3-way DMA_CONCAT (part 2): bn_conv [c=32..48] -> [48,16,16]", {
        "a": bn_conv, "b": 0, "out": c2f_cat, "mode": 7, "stride": 1,
        "IN_C": 48, "IN_H": 16, "IN_W": 16, "OUT_C": 48, "OUT_H": 16, "OUT_W": 16,
        "POOL_MODE": 0, "POOL_P": 32, "POOL_K": 16}, 0x15))

    # [17] C2f cv2: 1x1 GEMM_FUSED [48, 16, 16] -> [32, 16, 16] with TILE_CIN=24 (2-pass Cin split, 8 tiles)
    ins_cv2, c2f_out, c2f_out_gold = build_conv("C2f cv2: GEMM_FUSED 1x1 [48,16,16] -> [32,16,16] TILE_CIN=24 (8 tiles, 16 passes)",
                                                c2f_cat, c2f_cat_gold, 32, 1, 1, 0, 16, 8, 8, tile_cin=24)
    prog.append(ins_cv2)

    # =========================================================================
    # Block 3 [18..26]: YOLOv8m SPPF Block (cv1 + 3x MaxPool + 4-way concat + cv2 + AvgPool)
    # =========================================================================
    # [18] SPPF cv1: 1x1 GEMM_FUSED [32, 16, 16] -> [16, 16, 16] (4 tiles)
    ins_sppf1, sppf_x, sppf_x_gold = build_conv("SPPF cv1: GEMM_FUSED 1x1 [32,16,16] -> [16,16,16] (4 tiles)", c2f_out, c2f_out_gold, 16, 1, 1, 0, 16, 8, 8)
    prog.append(ins_sppf1)

    # [19] SPPF MAX_POOL #1 5x5 s1 p2
    sppf_y1_gold = maxpool_golden(sppf_x_gold, k=5, p=2, s=1)
    sppf_y1 = alloc(sppf_y1_gold.size)
    put_gold(sppf_y1, sppf_y1_gold)
    prog.append(("SPPF MAX_POOL #1 5x5 s1 p2 [16,16,16]", {
        "a": sppf_x, "b": 0, "out": sppf_y1, "mode": 1, "stride": 1,
        "IN_C": 16, "IN_H": 16, "IN_W": 16, "POOL_K": 5, "POOL_P": 2}, 0x15))

    # [20] SPPF MAX_POOL #2 5x5 s1 p2
    sppf_y2_gold = maxpool_golden(sppf_y1_gold, k=5, p=2, s=1)
    sppf_y2 = alloc(sppf_y2_gold.size)
    put_gold(sppf_y2, sppf_y2_gold)
    prog.append(("SPPF MAX_POOL #2 5x5 s1 p2 [16,16,16]", {
        "a": sppf_y1, "b": 0, "out": sppf_y2, "mode": 1, "stride": 1,
        "IN_C": 16, "IN_H": 16, "IN_W": 16, "POOL_K": 5, "POOL_P": 2}, 0x15))

    # [21] SPPF MAX_POOL #3 5x5 s1 p2
    sppf_y3_gold = maxpool_golden(sppf_y2_gold, k=5, p=2, s=1)
    sppf_y3 = alloc(sppf_y3_gold.size)
    put_gold(sppf_y3, sppf_y3_gold)
    prog.append(("SPPF MAX_POOL #3 5x5 s1 p2 [16,16,16]", {
        "a": sppf_y2, "b": 0, "out": sppf_y3, "mode": 1, "stride": 1,
        "IN_C": 16, "IN_H": 16, "IN_W": 16, "POOL_K": 5, "POOL_P": 2}, 0x15))

    # [22..24] SPPF 4-way DMA_CONCAT along C: [sppf_x, sppf_y1, sppf_y2, sppf_y3] -> sppf_cat [64, 16, 16]
    sppf_cat_gold = np.concatenate([sppf_x_gold, sppf_y1_gold, sppf_y2_gold, sppf_y3_gold], axis=0)
    sppf_cat = alloc(sppf_cat_gold.size)
    put_gold(sppf_cat, sppf_cat_gold)
    prog.append(("SPPF 4-way DMA_CONCAT (part 0+1): sppf_x [c=0..16] + sppf_y1 [c=16..32] -> [64,16,16]", {
        "a": sppf_x, "b": sppf_y1, "out": sppf_cat, "mode": 7, "stride": 1,
        "IN_C": 32, "IN_H": 16, "IN_W": 16, "OUT_C": 32, "OUT_H": 16, "OUT_W": 16,
        "POOL_MODE": 0, "POOL_P": 0, "POOL_K": 16}, 0x15))
    prog.append(("SPPF 4-way DMA_CONCAT (part 2): sppf_y2 [c=32..48] -> [64,16,16]", {
        "a": sppf_y2, "b": 0, "out": sppf_cat, "mode": 7, "stride": 1,
        "IN_C": 64, "IN_H": 16, "IN_W": 16, "OUT_C": 64, "OUT_H": 16, "OUT_W": 16,
        "POOL_MODE": 0, "POOL_P": 32, "POOL_K": 16}, 0x15))
    prog.append(("SPPF 4-way DMA_CONCAT (part 3): sppf_y3 [c=48..64] -> [64,16,16]", {
        "a": sppf_y3, "b": 0, "out": sppf_cat, "mode": 7, "stride": 1,
        "IN_C": 64, "IN_H": 16, "IN_W": 16, "OUT_C": 64, "OUT_H": 16, "OUT_W": 16,
        "POOL_MODE": 0, "POOL_P": 48, "POOL_K": 16}, 0x15))

    # [25] SPPF cv2: 1x1 GEMM_FUSED [64, 16, 16] -> [32, 16, 16] with TILE_CIN=32 (2-pass Cin split, 8 tiles)
    ins_sppf2, sppf_out, sppf_out_gold = build_conv("SPPF cv2: GEMM_FUSED 1x1 [64,16,16] -> [32,16,16] TILE_CIN=32 (8 tiles, 16 passes)",
                                                    sppf_cat, sppf_cat_gold, 32, 1, 1, 0, 16, 8, 8, tile_cin=32)
    prog.append(ins_sppf2)

    # [26] ELEM_WISE AVG_POOL 2x2 s2 on sppf_out [32, 16, 16] -> [32, 8, 8]
    avg_gold = avgpool_golden(sppf_out_gold, k=2, s=2, scale=16384, shift=16)
    avg_out = alloc(avg_gold.size)
    put_gold(avg_out, avg_gold)
    prog.append(("ELEM_WISE AVG_POOL 2x2 s2 on [32,16,16] -> [32,8,8]", {
        "a": sppf_out, "b": 0, "out": avg_out, "mode": 5, "stride": 2,
        "IN_C": 32, "IN_H": 16, "IN_W": 16, "POOL_K": 2, "POOL_P": 0, "SO": 16384, "SHO": 16}, 0x15))

    os.makedirs(a.out, exist_ok=True)
    with open(os.path.join(a.out, "dram_init.bin"), "wb") as f:
        f.write(img_init)
    with open(os.path.join(a.out, "dram_golden.bin"), "wb") as f:
        f.write(img_gold)
    with open(os.path.join(a.out, "mmio.txt"), "w", newline="\n") as f:
        f.write("# make_ref_gate_dma_slice_concat.py: %d instructions\n" % len(prog))
        for i, (label, regs, op) in enumerate(prog):
            f.write("# [%d] %s\n" % (i, label))
            for name, v in regs.items():
                addr = B + (R[name] if name in R else 0x46C + 4 * X.index(name))
                f.write("W %08x %x\n" % (addr, v & 0xFFFFFFFF))
            f.write("W 40000310 %x\n" % op)
    print("wrote %s: %d instructions, %d bytes used of %d" % (a.out, len(prog), top[0], len(img_init)))


if __name__ == "__main__":
    main()

