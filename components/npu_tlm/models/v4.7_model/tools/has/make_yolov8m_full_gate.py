#!/usr/bin/env python3
"""Full YOLOv8m instruction stream generator for HasNpuTop with 4-D HasDma Slice/Concat.

Generates a complete YOLOv8m program directory (mmio.txt, dram_init.bin, dram_golden.bin) covering all 117 graph steps
of the YOLOv8m architecture (tools/eval_sw/data/program.json):
  - 83 GEMM_FUSED (opcode 0x12) convolutions across Backbone (stem, dark2..dark5, sppf), Neck (p4, p3, d4, d5), and
    Detection Head (det.p3, det.p4, det.p5 box & cls branches), using SiLU LUT activation (act=2) on the 77 internal
    convolutions and linear activation (act=0) on the 6 detection output convolutions.
  - 12 ELEM_WISE ADD (opcode 0x15, mode 0) residual shortcut connections in dark2.c2f, dark3.c2f, dark4.c2f, dark5.c2f.
  - 16 ELEM_WISE DMA_SLICE (opcode 0x15, mode 6) channel-split operations across all 8 C2f blocks (splitting cv1 output
    along axis C into lower half [0..C/2) and upper half [C/2..C) via HasDma 4-D gather descriptors).
  - 13 multi-branch Concat stages lowered to 35 ELEM_WISE DMA_CONCAT (opcode 0x15, mode 7) instructions (4-way concats
    in dark2.c2f, dark5.c2f, sppf, neck.p4.c2f, neck.p3.c2f, neck.d4.c2f, neck.d5.c2f; 6-way concats in dark3.c2f and
    dark4.c2f; 2-way FPN/PAN concats in neck.p4, neck.p3, neck.d4, neck.d5 via HasDma 4-D scatter descriptors).
  - 3 ELEM_WISE MAX_POOL (opcode 0x15, mode 1) 5x5 stride-1 pad-2 operations in SPPF.
  - 2 host upsample2x steps in the FPN neck (P5 -> P4 and P4 -> P3).

Total stream: 149 NPU instructions + 2 host upsample2x steps = 151 steps.

Usage:
  python3 tools/has/make_yolov8m_full_gate.py <out_dir> [--ch-div 8] [--hw-div 10] [--seed 42]
  python3 tools/has/make_yolov8m_full_gate.py <out_dir> --ch-div 1 --hw-div 1 --section sppf_neck
"""
import argparse
import math
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

CAP_A, CAP_B, CAP_C = 80896, 82944, 24576


def make_silu_lut():
    """256-entry int8 SiLU lookup table indexed by q + 128 for q in -128..127."""
    q = np.arange(-128, 128, dtype=np.float64)
    x = q * 0.05
    silu = x / (1.0 + np.exp(-x))
    return np.clip(np.round(silu / 0.05), -128, 127).astype(np.int8)


def choose_tiling(cin, ih, iw, cout, oh, ow, kh, kw, stride):
    """Selects hardware-valid (cout_t, cin_t, h_t, w_t) satisfying CAP_A, CAP_B, CAP_C, and SWEEP<=256."""
    cout_t = min(32, cout)
    divs = [d for d in range(cin, 0, -1) if cin % d == 0 and cout_t * d * kh * kw <= CAP_B]
    for cin_t in divs:
        w_candidates = [w for w in (min(ow, 64), min(ow, 32), min(ow, 16), min(ow, 8), min(ow, 4), min(ow, 2), 1) if w > 0]
        h_candidates = [h for h in range(min(oh, 32), 0, -1)]
        for w_t in w_candidates:
            aw = (min(w_t, ow) - 1) * stride + kw
            widths = {min(w_t, ow), ow - (-(-ow // w_t) - 1) * w_t}
            if max(1 + (math.gcd(wt, 32) - 1) * stride + kw + 1 - (kw % 2) + 32 for wt in widths) > 256:
                continue
            for h_t in h_candidates:
                ah = (min(h_t, oh) - 1) * stride + kh
                if cin_t * ah * aw <= CAP_A and cout_t * min(h_t, oh) * min(w_t, ow) <= CAP_C:
                    return cout_t, cin_t, h_t, w_t
    raise RuntimeError(f"Cannot tile cin={cin} ih={ih} iw={iw} cout={cout} oh={oh} ow={ow} k={kh}x{kw} s={stride}")


def conv_golden(x_chw, w_blocks, bias, scales, shifts, zp_out, cout_t, kh, kw, stride, pad, act, lut_arr):
    """Bit-exact GEMM_FUSED golden matching HasObp and ref_program.py using fast matrix multiplication."""
    cin, ih, iw = x_chw.shape
    cout = bias.size
    oh = (ih + 2 * pad - kh) // stride + 1
    ow = (iw + 2 * pad - kw) // stride + 1
    W = np.zeros((cout, cin, kh, kw), dtype=np.float64)
    for idx, c0 in enumerate(range(0, cout, cout_t)):
        nch = min(cout_t, cout - c0)
        W[c0:c0 + nch] = np.transpose(w_blocks[idx].astype(np.float64), (3, 0, 1, 2))
    xp = np.pad(x_chw.astype(np.float64), ((0, 0), (pad, pad), (pad, pad)), mode="constant", constant_values=0.0) if pad > 0 else x_chw.astype(np.float64)
    acc = np.zeros((cout, oh * ow), dtype=np.float64)
    for ky in range(kh):
        for kx in range(kw):
            win = xp[:, ky:ky + (oh - 1) * stride + 1:stride, kx:kx + (ow - 1) * stride + 1:stride].reshape(cin, oh * ow)
            acc += W[:, :, ky, kx] @ win
    psum = acc.astype(np.int64) + bias.astype(np.int64)[:, None]
    S = scales.astype(np.int64)[:, None]
    sh = shifts.astype(np.int64)[:, None]
    prod = psum * S
    half = np.where(sh > 0, np.left_shift(np.int64(1), np.maximum(sh, 1) - 1), 0)
    rq = np.right_shift(prod + half, sh)
    q = np.clip(np.clip(rq, -32768, 32767) + int(zp_out), -128, 127).astype(np.int8)
    if act == 1:
        q = np.maximum(q, 0).astype(np.int8)
    elif act in (2, 3):
        q = lut_arr[q.astype(np.int16) + 128]
    return q.reshape(cout, oh, ow)


def maxpool_golden(x_chw, k=5, p=2, s=1):
    c, h, w = x_chw.shape
    ho, wo = (h + 2 * p - k) // s + 1, (w + 2 * p - k) // s + 1
    xp = np.pad(x_chw, ((0, 0), (p, p), (p, p)), mode="constant", constant_values=-128)
    out = np.full((c, ho, wo), -128, dtype=np.int8)
    for ky in range(k):
        for kx in range(k):
            out = np.maximum(out, xp[:, ky:ky + (ho - 1) * s + 1:s, kx:kx + (wo - 1) * s + 1:s])
    return out


def add_golden(a_chw, b_chw, zpA, zpB, zpO, SA, sA, SB, sB, SO, sO):
    def rshift_rhu(v, sh):
        return (v + (1 << (sh - 1))) >> sh if sh > 0 else v

    a32 = np.clip(rshift_rhu((a_chw.astype(np.int64) - zpA) * SA, sA), -2147483648, 2147483647)
    b32 = np.clip(rshift_rhu((b_chw.astype(np.int64) - zpB) * SB, sB), -2147483648, 2147483647)
    s32 = np.clip(a32 + b32, -2147483648, 2147483647)
    rq = np.clip(rshift_rhu(s32 * SO, sO), -32768, 32767) + zpO
    return np.clip(rq, -128, 127).astype(np.int8)


def sc(c, ch_div):
    """Scales a channel count by ch_div while keeping it even (for C2f 2-way channel splits)."""
    if ch_div <= 1:
        return c
    v = max(2, c // ch_div)
    return v + (v & 1)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("out", help="Output directory for mmio.txt, dram_init.bin, dram_golden.bin")
    ap.add_argument("--ch-div", type=int, default=8, help="Channel divisor relative to full YOLOv8m (default: 8; use 1 for full channels)")
    ap.add_argument("--hw-div", type=int, default=10, help="Spatial divisor relative to 640x640 (default: 10 -> 64x64 input; use 1 for 640x640)")
    ap.add_argument("--section", choices=["all", "backbone_stem_d2", "sppf_neck_p4", "det_heads"], default="all",
                    help="Which section of YOLOv8m to emit (default: all 149 instructions + 2 host steps)")
    ap.add_argument("--seed", type=int, default=42)
    a = ap.parse_args()

    rng = np.random.default_rng(a.seed)
    chunks_init = []
    chunks_gold = []
    top = [0x1000]

    def ensure_cap(end_off):
        cur = len(chunks_init)
        if end_off > cur:
            ext = max(end_off - cur, 1024 * 1024)
            chunks_init.extend(b"\x00" * ext)
            chunks_gold.extend(b"\x00" * ext)

    chunks_init = bytearray(1024 * 1024)
    chunks_gold = bytearray(1024 * 1024)

    def alloc(n):
        p = top[0]
        top[0] = (p + n + 63) & ~63
        ensure_cap(top[0])
        return p

    def put_init(arr):
        b = np.asarray(arr).tobytes()
        p = alloc(len(b))
        chunks_init[p:p + len(b)] = b
        chunks_gold[p:p + len(b)] = b
        return p

    def put_gold(p, arr):
        b = np.asarray(arr).tobytes()
        chunks_gold[p:p + len(b)] = b

    silu_lut = make_silu_lut()
    silu_lut_addr = put_init(silu_lut)

    events = []   # ("I", label, regs, opcode) or ("H", text)
    counts = {"gemm": 0, "add": 0, "slice": 0, "concat_stages": 0, "concat_instrs": 0, "maxpool": 0, "upsample": 0}

    zpA, zpB, zpO = 0, 0, 0
    SA, sA, SB, sB, SO, sO = 16384, 14, 16384, 14, 16384, 14

    def emit_conv(label, x_T, cout, kh, kw, stride, pad, act=2, zp_out=0):
        x_addr, x_arr = x_T
        cin, ih, iw = x_arr.shape
        oh = (ih + 2 * pad - kh) // stride + 1
        ow = (iw + 2 * pad - kw) // stride + 1
        cout_t, cin_t, h_t, w_t = choose_tiling(cin, ih, iw, cout, oh, ow, kh, kw, stride)
        K = cin * kh * kw
        w_addr = alloc(cout * K)
        w_blocks = []
        for c0 in range(0, cout, cout_t):
            nch = min(cout_t, cout - c0)
            blk = rng.integers(-12, 13, size=(cin, kh, kw, nch), dtype=np.int8)
            w_blocks.append(blk)
            off = w_addr + c0 * K
            b = blk.tobytes()
            chunks_init[off:off + len(b)] = b
            chunks_gold[off:off + len(b)] = b
        bias_arr = rng.integers(-400, 401, size=cout, dtype="<i4")
        scales_arr = rng.integers(1 << 27, 1 << 28, size=cout, dtype="<i4")
        shifts_arr = rng.integers(36, 39, size=cout, dtype="<i4")
        bias_addr = put_init(bias_arr)
        scale_addr = put_init(scales_arr)
        shift_addr = put_init(shifts_arr)
        y_gold = conv_golden(x_arr, w_blocks, bias_arr, scales_arr, shifts_arr, zp_out, cout_t, kh, kw, stride, pad, act, silu_lut)
        y_addr = alloc(y_gold.size)
        put_gold(y_addr, y_gold)
        regs = {
            "in": x_addr, "w": w_addr, "out": y_addr, "bias": bias_addr,
            "kh": kh, "kw": kw, "stride": stride, "pad": pad, "act": act, "has_skip": 0,
            "IN_C": cin, "IN_H": ih, "IN_W": iw, "OUT_C": cout, "OUT_H": oh, "OUT_W": ow,
            "TILE_COUT": cout_t, "TILE_H": h_t, "TILE_W": w_t,
            "SCALE_ADDR": scale_addr, "SHIFT_ADDR": shift_addr, "ZP_OUT": zp_out & 0xFFFFFFFF, "FLAGS": 0,
        }
        if act in (2, 3):
            regs["LUT_ADDR"] = silu_lut_addr
        if cin_t < cin:
            regs["TILE_CIN"] = cin_t
        events.append(("I", f"{label} GEMM_FUSED {kh}x{kw} s{stride} [{cin},{ih},{iw}]->[{cout},{oh},{ow}]", regs, 0x12))
        counts["gemm"] += 1
        return (y_addr, y_gold)

    def emit_add(label, a_T, b_T):
        (a_addr, a_arr), (b_addr, b_arr) = a_T, b_T
        c, h, w = a_arr.shape
        y_gold = add_golden(a_arr, b_arr, zpA, zpB, zpO, SA, sA, SB, sB, SO, sO)
        y_addr = alloc(y_gold.size)
        put_gold(y_addr, y_gold)
        events.append(("I", f"{label} ELEM_WISE ADD [{c},{h},{w}]", {
            "a": a_addr, "b": b_addr, "out": y_addr, "mode": 0, "stride": 1,
            "len": y_gold.size, "a_len": y_gold.size, "b_len": y_gold.size,
            "IN_C": c, "IN_H": h, "IN_W": w, "OUT_C": c, "OUT_H": h, "OUT_W": w,
            "ZP_A": zpA & 0xFFFFFFFF, "ZP_B": zpB & 0xFFFFFFFF, "ZP_O": zpO & 0xFFFFFFFF,
            "SA": SA, "SHA": sA, "SB": SB, "SHB": sB, "SO": SO, "SHO": sO}, 0x15))
        counts["add"] += 1
        return (y_addr, y_gold)

    def emit_slice_ch(label, x_T, c_start, c_len):
        x_addr, x_arr = x_T
        c, h, w = x_arr.shape
        y_gold = x_arr[c_start:c_start + c_len, :, :].copy()
        y_addr = alloc(y_gold.size)
        put_gold(y_addr, y_gold)
        events.append(("I", f"{label} ELEM_WISE DMA_SLICE c=[{c_start}..{c_start + c_len}) [{c},{h},{w}]->[{c_len},{h},{w}]", {
            "a": x_addr, "b": 0, "out": y_addr, "mode": 6, "stride": 1,
            "IN_C": c, "IN_H": h, "IN_W": w, "POOL_MODE": 0, "POOL_P": c_start, "POOL_K": c_len}, 0x15))
        counts["slice"] += 1
        return (y_addr, y_gold)

    def emit_concat_ch(label, tensors):
        """Lowers an N-way channel concat (N >= 2) into HasDma mode-7 DMA_CONCAT instruction(s)."""
        assert len(tensors) >= 2
        counts["concat_stages"] += 1
        y_gold = np.concatenate([t[1] for t in tensors], axis=0)
        tot_c, h, w = y_gold.shape
        y_addr = alloc(y_gold.size)
        put_gold(y_addr, y_gold)

        t0_addr, t0_arr = tensors[0]
        t1_addr, t1_arr = tensors[1]
        c0, c1 = t0_arr.shape[0], t1_arr.shape[0]
        c01 = c0 + c1
        events.append(("I", f"{label} ELEM_WISE DMA_CONCAT (part 0+1/{len(tensors)}) c=[0..{c01}) -> [{tot_c},{h},{w}]", {
            "a": t0_addr, "b": t1_addr, "out": y_addr, "mode": 7, "stride": 1,
            "IN_C": c01, "IN_H": h, "IN_W": w, "OUT_C": c01, "OUT_H": h, "OUT_W": w,
            "POOL_MODE": 0, "POOL_P": 0, "POOL_K": c0}, 0x15))
        counts["concat_instrs"] += 1
        cur_c = c01
        for idx in range(2, len(tensors)):
            ti_addr, ti_arr = tensors[idx]
            ci = ti_arr.shape[0]
            events.append(("I", f"{label} ELEM_WISE DMA_CONCAT (part {idx}/{len(tensors)}) c=[{cur_c}..{cur_c + ci}) -> [{tot_c},{h},{w}]", {
                "a": ti_addr, "b": 0, "out": y_addr, "mode": 7, "stride": 1,
                "IN_C": tot_c, "IN_H": h, "IN_W": w, "OUT_C": tot_c, "OUT_H": h, "OUT_W": w,
                "POOL_MODE": 0, "POOL_P": cur_c, "POOL_K": ci}, 0x15))
            counts["concat_instrs"] += 1
            cur_c += ci
        return (y_addr, y_gold)

    def emit_maxpool(label, x_T, k=5, p=2, s=1):
        x_addr, x_arr = x_T
        c, h, w = x_arr.shape
        y_gold = maxpool_golden(x_arr, k=k, p=p, s=s)
        y_addr = alloc(y_gold.size)
        put_gold(y_addr, y_gold)
        events.append(("I", f"{label} ELEM_WISE MAX_POOL {k}x{k} s{s} p{p} [{c},{h},{w}]", {
            "a": x_addr, "b": 0, "out": y_addr, "mode": 1, "stride": s,
            "IN_C": c, "IN_H": h, "IN_W": w, "POOL_K": k, "POOL_P": p}, 0x15))
        counts["maxpool"] += 1
        return (y_addr, y_gold)

    def emit_upsample2x(label, x_T):
        x_addr, x_arr = x_T
        c, h, w = x_arr.shape
        y_gold = x_arr.repeat(2, axis=1).repeat(2, axis=2)
        y_addr = alloc(y_gold.size)
        put_gold(y_addr, y_gold)
        events.append(("H", f"H upsample2x in=0x{x_addr:x} out=0x{y_addr:x} c={c} h={h} w={w}"))
        counts["upsample"] += 1
        return (y_addr, y_gold)

    def emit_c2f(prefix, x_T, cout, n_bottlenecks, shortcut):
        """YOLOv8m C2f block: cv1 -> 2x DMA_SLICE -> n_bottlenecks x (cv1 -> cv2 [+ ADD]) -> (n+2)-way DMA_CONCAT -> cv2."""
        cv1_T = emit_conv(f"{prefix}.cv1", x_T, cout, 1, 1, 1, 0, act=2)
        half_c = cout // 2
        b0_T = emit_slice_ch(f"{prefix}.slice0", cv1_T, 0, half_c)
        b1_T = emit_slice_ch(f"{prefix}.slice1", cv1_T, half_c, half_c)
        branches = [b0_T, b1_T]
        cur_T = b1_T
        for m in range(n_bottlenecks):
            m_cv1 = emit_conv(f"{prefix}.m{m}.cv1", cur_T, half_c, 3, 3, 1, 1, act=2)
            m_cv2 = emit_conv(f"{prefix}.m{m}.cv2", m_cv1, half_c, 3, 3, 1, 1, act=2)
            if shortcut:
                cur_T = emit_add(f"{prefix}.m{m}.add", m_cv2, cur_T)
            else:
                cur_T = m_cv2
            branches.append(cur_T)
        cat_T = emit_concat_ch(f"{prefix}.concat", branches)
        out_T = emit_conv(f"{prefix}.cv2", cat_T, cout, 1, 1, 1, 0, act=2)
        return out_T

    def emit_sppf(prefix, x_T, cout):
        """YOLOv8m SPPF block: cv1 -> 3x MAX_POOL 5x5 -> 4-way DMA_CONCAT -> cv2."""
        mid_c = cout // 2
        cv1_T = emit_conv(f"{prefix}.cv1", x_T, mid_c, 1, 1, 1, 0, act=2)
        y1_T = emit_maxpool(f"{prefix}.m1", cv1_T, k=5, p=2, s=1)
        y2_T = emit_maxpool(f"{prefix}.m2", y1_T, k=5, p=2, s=1)
        y3_T = emit_maxpool(f"{prefix}.m3", y2_T, k=5, p=2, s=1)
        cat_T = emit_concat_ch(f"{prefix}.concat", [cv1_T, y1_T, y2_T, y3_T])
        out_T = emit_conv(f"{prefix}.cv2", cat_T, cout, 1, 1, 1, 0, act=2)
        return out_T

    def emit_det_scale(prefix, x_T, c_box, c_cls, c_nc):
        """YOLOv8m detection head for one feature scale (box1->box2->box_out and cls1->cls2->cls_out)."""
        b1 = emit_conv(f"{prefix}.box1", x_T, c_box, 3, 3, 1, 1, act=2)
        b2 = emit_conv(f"{prefix}.box2", b1, c_box, 3, 3, 1, 1, act=2)
        b_out = emit_conv(f"{prefix}.box_out", b2, c_box, 1, 1, 1, 0, act=0)
        k1 = emit_conv(f"{prefix}.cls1", x_T, c_cls, 3, 3, 1, 1, act=2)
        k2 = emit_conv(f"{prefix}.cls2", k1, c_cls, 3, 3, 1, 1, act=2)
        k_out = emit_conv(f"{prefix}.cls_out", k2, c_nc, 1, 1, 1, 0, act=0)
        return b_out, k_out

    def make_input(c, h, w):
        arr = rng.integers(-90, 91, size=(c, h, w), dtype=np.int8)
        return (put_init(arr), arr)

    # YOLOv8m channel widths (scaled by --ch-div)
    c1 = sc(48, a.ch_div)
    c2 = sc(96, a.ch_div)
    c3 = sc(192, a.ch_div)
    c4 = sc(384, a.ch_div)
    c5 = sc(576, a.ch_div)
    c_box = sc(64, a.ch_div)
    c_cls = sc(192, a.ch_div)
    c_nc = sc(80, a.ch_div)

    H0 = max(32, 640 // a.hw_div)
    W0 = H0

    if a.section == "backbone_stem_d2":
        images = make_input(3, H0, W0)
        layer00 = emit_conv("stem", images, c1, 3, 3, 2, 1, act=2)
        layer01 = emit_conv("dark2.conv", layer00, c2, 3, 3, 2, 1, act=2)
        _ = emit_c2f("dark2.c2f", layer01, c2, n_bottlenecks=2, shortcut=True)
    elif a.section == "sppf_neck_p4":
        p5_h, p5_w = H0 // 32, W0 // 32
        layer06 = make_input(c4, p5_h * 2, p5_w * 2)
        layer08 = make_input(c5, p5_h, p5_w)
        layer09 = emit_sppf("sppf", layer08, c5)
        layer10 = emit_upsample2x("neck.p4.upsample", layer09)
        layer11 = emit_concat_ch("neck.p4.cat", [layer10, layer06])
        _ = emit_c2f("neck.p4.c2f", layer11, c4, n_bottlenecks=2, shortcut=False)
    elif a.section == "det_heads":
        p3_h, p3_w = H0 // 8, W0 // 8
        n3 = make_input(c3, p3_h, p3_w)
        n4 = make_input(c4, p3_h // 2, p3_w // 2)
        n5 = make_input(c5, p3_h // 4, p3_w // 4)
        emit_det_scale("det.p3", n3, c_box, c_cls, c_nc)
        emit_det_scale("det.p4", n4, c_box, c_cls, c_nc)
        emit_det_scale("det.p5", n5, c_box, c_cls, c_nc)
    else:
        # Full 117-step YOLOv8m network (Backbone + SPPF + FPN/PAN Neck + 3-Scale Detection Head)
        images = make_input(3, H0, W0)
        # Backbone
        layer00 = emit_conv("stem", images, c1, 3, 3, 2, 1, act=2)
        layer01 = emit_conv("dark2.conv", layer00, c2, 3, 3, 2, 1, act=2)
        layer02 = emit_c2f("dark2.c2f", layer01, c2, n_bottlenecks=2, shortcut=True)
        layer03 = emit_conv("dark3.conv", layer02, c3, 3, 3, 2, 1, act=2)
        layer04 = emit_c2f("dark3.c2f", layer03, c3, n_bottlenecks=4, shortcut=True)   # P3
        layer05 = emit_conv("dark4.conv", layer04, c4, 3, 3, 2, 1, act=2)
        layer06 = emit_c2f("dark4.c2f", layer05, c4, n_bottlenecks=4, shortcut=True)   # P4
        layer07 = emit_conv("dark5.conv", layer06, c5, 3, 3, 2, 1, act=2)
        layer08 = emit_c2f("dark5.c2f", layer07, c5, n_bottlenecks=2, shortcut=True)
        layer09 = emit_sppf("sppf", layer08, c5)                                       # P5
        # Neck (FPN top-down + PAN bottom-up)
        layer10 = emit_upsample2x("neck.p4.upsample", layer09)
        layer11 = emit_concat_ch("neck.p4.cat", [layer10, layer06])
        layer12 = emit_c2f("neck.p4.c2f", layer11, c4, n_bottlenecks=2, shortcut=False)
        layer13 = emit_upsample2x("neck.p3.upsample", layer12)
        layer14 = emit_concat_ch("neck.p3.cat", [layer13, layer04])
        layer15 = emit_c2f("neck.p3.c2f", layer14, c3, n_bottlenecks=2, shortcut=False)  # N3
        layer16 = emit_conv("neck.d4.conv", layer15, c3, 3, 3, 2, 1, act=2)
        layer17 = emit_concat_ch("neck.d4.cat", [layer16, layer12])
        layer18 = emit_c2f("neck.d4.c2f", layer17, c4, n_bottlenecks=2, shortcut=False)  # N4
        layer19 = emit_conv("neck.d5.conv", layer18, c4, 3, 3, 2, 1, act=2)
        layer20 = emit_concat_ch("neck.d5.cat", [layer19, layer09])
        layer21 = emit_c2f("neck.d5.c2f", layer20, c5, n_bottlenecks=2, shortcut=False)  # N5
        # Detection Head (P3, P4, P5)
        emit_det_scale("det.p3", layer15, c_box, c_cls, c_nc)
        emit_det_scale("det.p4", layer18, c_box, c_cls, c_nc)
        emit_det_scale("det.p5", layer21, c_box, c_cls, c_nc)

    final_size = (top[0] + 4095) & ~4095
    ensure_cap(final_size)
    os.makedirs(a.out, exist_ok=True)
    with open(os.path.join(a.out, "dram_init.bin"), "wb") as f:
        f.write(chunks_init[:final_size])
    with open(os.path.join(a.out, "dram_golden.bin"), "wb") as f:
        f.write(chunks_gold[:final_size])
    n_ins = sum(1 for e in events if e[0] == "I")
    with open(os.path.join(a.out, "mmio.txt"), "w", newline="\n") as f:
        f.write(f"# make_yolov8m_full_gate.py (section={a.section}, ch_div={a.ch_div}, hw_div={a.hw_div}): "
                f"{n_ins} instructions + {counts['upsample']} host steps\n")
        ins_idx = 0
        for ev in events:
            if ev[0] == "H":
                f.write(ev[1] + "\n")
                continue
            _, label, regs, op = ev
            f.write(f"# [{ins_idx}] {label}\n")
            for name, v in regs.items():
                addr = B + (R[name] if name in R else 0x46C + 4 * X.index(name))
                f.write("W %08x %x\n" % (addr, v & 0xFFFFFFFF))
            f.write("W 40000310 %x\n" % op)
            ins_idx += 1

    print(f"wrote {a.out} (section={a.section}, ch_div={a.ch_div}, hw_div={a.hw_div}): "
          f"{n_ins} NPU instructions ({counts['gemm']} GEMM_FUSED, {counts['add']} ADD, "
          f"{counts['slice']} DMA_SLICE, {counts['concat_instrs']} DMA_CONCAT [{counts['concat_stages']} concat stages], "
          f"{counts['maxpool']} MAX_POOL) + {counts['upsample']} host upsample2x steps, DRAM {final_size} B")


if __name__ == "__main__":
    main()
