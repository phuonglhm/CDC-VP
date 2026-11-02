#!/usr/bin/env python3
"""Independent reference for a program of extended instructions.

Reads a program directory (mmio.txt + dram_init.bin, the format tools/has/tb_has_npu_top replays and
SAURIA_CORE_RTL_RECORD writes), computes the output of every instruction with plain integer arithmetic in numpy, and
compares it with an image under test: dram_golden.bin of the directory, a RAM dump, or the image a recording produced.

It shares no code with the model. A convolution is one sum over the whole tensor: the tiling registers are used only to
find the weights in the image (they are stored per output-channel range) and to report tiles the hardware cannot hold.
So it tells a compiler apart from the model: when the model and this reference agree on a program, the program means
what its registers say; when they disagree, the report names the first instruction and element.

Covered: GEMM_FUSED (0x12) with integer scales or real-valued scales, the residual form (HAS_SKIP), ELEM_WISE (0x15)
ADD, MAX_POOL and AVG_POOL, DMA_TENSOR (0x16) COPY, and the host step "H upsample2x". FUSED_ATTN (0x13) and LAYERNORM (0x14) are not computed:
their output is copied from the image under test and reported as "not checked".

Arithmetic (default configuration of the model):
  GEMM_FUSED   psum = bias[c] + sum(in * w), int32;  q = clamp8(sat16(round_half_up(psum * S[c] >> s[c])) + zp_out);
               ACT_TYPE 0: q, 1: max(q, 0), 2 / 3: table[q + 128]
               padding: PAD on all four sides, or PAD_T / PAD_L / PAD_B / PAD_R when any of them is written (the
               unwritten ones are then 0); the padded positions hold PAD_VALUE (int8, 0 when not written);
               kernel taps DILATION apart (1 when not written or 0)
               TMAJ_C0 written (0x40000500, value c0 < OUT_C): channels c0 .. OUT_C-1 are stored token-major in their own
               region, position p of channel c at OUT_ADDR + c0 * OH * OW + p * (OUT_C - c0) + (c - c0); channels below c0
               stay [C][H][W]; c0 a multiple of TILE_COUT; not with HAS_SKIP
  ADD          a' = sat32(round((A - zpA) * SA >> sA)), b' likewise, out = requant(sat32(a' + b'), SO, sO, zpO)
  MAX_POOL     window k x k, stride, padding value -128
  AVG_POOL     sum of the window as int16 (wraps), out = clamp8(round(sum * S >> s)), no padding
  COPY         MODE_PACK 0: LEN bytes from A_ADDR to OUT_ADDR; refused when LEN is 0, a region passes 2^32 or the two
               regions overlap (the model runs it only with HAS_DMA_TENSOR=1)
Tensors are int8 [C][H][W]; scales are signed 32-bit words.

Usage:
  python3 tools/has/ref_program.py <program dir> [--image FILE] [--first F] [--count N] [--no-resync]
                                   [--write-ref FILE] [--csv FILE] [--addr-base HEX]
With --first F the state before instruction F is taken from the image under test; this is only right for an image
that holds every tensor of the run (dram_golden.bin, a recording), not for a program that reuses memory.
Exit code 0 when every checked instruction matches, 1 otherwise (also when nothing was checked), 2 on a usage or file
error.
"""
import argparse
import math
import os
import struct
import sys

import numpy as np

# ---- register map (offsets from 0x40000000) -----------------------------------------------------------------------
BASE = 0x40000000
PUSH_A = 0x310
REG = {0x400: "in", 0x404: "w", 0x408: "out", 0x40C: "bias", 0x410: "m", 0x414: "k", 0x418: "n", 0x41C: "kh", 0x420: "kw",
       0x424: "stride", 0x428: "pad", 0x42C: "act", 0x430: "has_skip", 0x434: "skip", 0x438: "in_scale", 0x43C: "w_scale",
       0x440: "out_scale", 0x444: "a", 0x448: "b", 0x44C: "v", 0x450: "len", 0x454: "mode", 0x458: "r458", 0x45C: "r45c",
       0x460: "scale_o", 0x464: "a_len", 0x468: "b_len"}
REG_DEFAULT = {"m": 32, "k": 32, "n": 32, "kh": 1, "kw": 1, "stride": 1}
FLOAT_ONE = 0x3F800000
EXT_BASE = 0x46C
EXT = ["IN_C", "IN_H", "IN_W", "OUT_C", "OUT_H", "OUT_W", "TILE_COUT", "TILE_H", "TILE_W", "SCALE_ADDR", "SHIFT_ADDR",
       "LUT_ADDR", "ZP_OUT", "FLAGS", "ZP_A", "ZP_B", "ZP_O", "SA", "SHA", "SB", "SHB", "SO", "SHO", "POOL_K", "POOL_P",
       "POOL_MODE", "TILE_CIN", "Y_USED", "ROWS", "PARAM_ADDR", "MASK_ADDR", "PAD_VALUE", "PAD_T", "PAD_L", "PAD_B", "PAD_R",
       "DILATION", "TMAJ_C0"]
EXT_END = EXT_BASE + 4 * len(EXT)
ADDR_REGS = ("in", "w", "out", "bias", "skip", "a", "b", "v")
ADDR_EXT = ("SCALE_ADDR", "SHIFT_ADDR", "LUT_ADDR", "PARAM_ADDR", "MASK_ADDR")

# capacities of one tile in the hardware (bytes / int32 elements)
CAP_A, CAP_B, CAP_C = 80896, 82944, 24576


class Refused(Exception):
    """The model refuses this instruction at the push (error name as in the STATUS register)."""


def s32(v):
    v &= 0xFFFFFFFF
    return v - (1 << 32) if v & 0x80000000 else v


def f32(v):
    return struct.unpack("<f", struct.pack("<I", v & 0xFFFFFFFF))[0]


# ---- arithmetic -----------------------------------------------------------------------------------------------------
def round_shift(p, s):
    """Round half up: (p + 2^(s-1)) >> s, s in 0 .. 63, element-wise on int64 (Python integers when too wide)."""
    p = np.asarray(p, dtype=np.int64)
    s = np.broadcast_to(np.asarray(s, dtype=np.int64), p.shape)
    if s.size and (s.min() < 0 or s.max() > 63):
        raise ValueError("shift outside 0 .. 63")
    if p.size and (np.abs(p.astype(np.float64)).max() >= 2.0 ** 62 or s.max() >= 62):
        flat = [(int(a) + ((1 << (int(b) - 1)) if b else 0)) >> int(b) for a, b in zip(p.ravel().tolist(), s.ravel().tolist())]
        return np.array([max(min(v, (1 << 62)), -(1 << 62)) for v in flat], dtype=np.int64).reshape(p.shape)
    half = np.where(s > 0, np.left_shift(np.int64(1), np.maximum(s, 1) - 1), 0)
    return np.right_shift(p + half, s)


def mul(x, S):
    x = np.asarray(x, dtype=np.int64)
    S = np.broadcast_to(np.asarray(S, dtype=np.int64), x.shape)
    if x.size and float(np.abs(x).max()) * float(np.abs(S).max()) >= 2.0 ** 62:
        # exact products with Python integers; the caller's round_shift takes the slow path too
        return np.array([int(a) * int(b) for a, b in zip(x.ravel().tolist(), S.ravel().tolist())], dtype=object).reshape(x.shape)
    return x * S


def round_shift_any(p, s):
    if p.dtype == object:
        s = np.broadcast_to(np.asarray(s, dtype=np.int64), p.shape)
        flat = [(int(a) + ((1 << (int(b) - 1)) if b else 0)) >> int(b) for a, b in zip(p.ravel().tolist(), s.ravel().tolist())]
        return np.array([max(min(v, 1 << 40), -(1 << 40)) for v in flat], dtype=np.int64).reshape(p.shape)
    return round_shift(p, s)


def requant(x, S, s, zp):
    r = round_shift_any(mul(x, S), s)
    return np.clip(np.clip(r, -32768, 32767) + int(zp), -128, 127).astype(np.int8)


def dequant(x, zp, S, s):
    r = round_shift_any(mul(np.asarray(x, dtype=np.int64) - int(zp), S), s)
    return np.clip(r, -(1 << 31), (1 << 31) - 1)


def float_to_fixed(v):
    """Real-valued scale -> (S, s) with S in [2^30, 2^31), value = S * 2^-s; None when s leaves 0 .. 63."""
    if not (v > 0.0) or not math.isfinite(v):
        return None
    e = int(math.floor(math.log2(v)))
    S = int(math.floor(math.ldexp(v, 30 - e) + 0.5))
    if S >= (1 << 31):
        S >>= 1
        e += 1
    s = 30 - e
    return (S, s) if 0 <= s <= 63 else None


# ---- program ---------------------------------------------------------------------------------------------------------
class Step:
    def __init__(self, kind, index, label, line):
        self.kind, self.index, self.label, self.line = kind, index, label, line
        self.opcode = 0
        self.reg, self.ext = {}, {}
        self.text = ""


def read_program(path, addr_base):
    reg = {n: 0 for n in REG.values()}
    reg.update(REG_DEFAULT)
    for n in ("in_scale", "w_scale", "out_scale", "r458", "r45c", "scale_o"):
        reg[n] = FLOAT_ONE
    ext, steps, label, n_instr = {}, [], "", 0
    with open(path, "r", errors="replace") as f:
        for ln, line in enumerate(f, 1):
            line = line.strip()
            if line.startswith("# ["):
                label = line[2:]
                continue
            if not line or line[0] == "#":
                continue
            if line[0] == "H":
                st = Step("H", n_instr, line, ln)
                st.text = line
                steps.append(st)
                continue
            tok = line.split()
            if tok[0] != "W" or len(tok) < 3:
                continue
            a, v = int(tok[1], 16) - BASE, int(tok[2], 16)
            if EXT_BASE <= a < EXT_END and (a - EXT_BASE) % 4 == 0:
                name = EXT[(a - EXT_BASE) // 4]
                ext[name] = (v - addr_base) & 0xFFFFFFFF if name in ADDR_EXT and v else v
            elif a in REG:
                name = REG[a]
                if name == "mode":
                    v = (v >> 24) & 0xFF if v & 0xFFFF0000 else v
                reg[name] = (v - addr_base) & 0xFFFFFFFF if name in ADDR_REGS and v else v
            elif a == PUSH_A:
                st = Step("I", n_instr, label, ln)
                st.opcode, st.reg, st.ext = v & 0xFF, dict(reg), dict(ext)
                steps.append(st)
                n_instr += 1
                ext = {}
                label = ""
    return steps, n_instr


class Image:
    def __init__(self, data):
        self.b = np.frombuffer(bytearray(data), dtype=np.uint8).copy()

    def need(self, addr, n, what):
        if addr < 0 or addr + n > self.b.size:
            raise Refused("address: %s at 0x%X + %d bytes is outside the image of %d bytes" % (what, addr, n, self.b.size))

    def i8(self, addr, n, what):
        self.need(addr, n, what)
        return self.b[addr:addr + n].view(np.int8)

    def i32(self, addr, n, what):
        self.need(addr, 4 * n, what)
        return self.b[addr:addr + 4 * n].copy().view("<i4").astype(np.int64)

    def put(self, addr, arr, what):
        a = np.ascontiguousarray(arr, dtype=np.int8).ravel().view(np.uint8)
        self.need(addr, a.size, what)
        self.b[addr:addr + a.size] = a


# ---- instructions ----------------------------------------------------------------------------------------------------
def gemm(st, img, notes):
    r, x = st.reg, st.ext
    if "IN_C" in x:
        cin, ih, iw = s32(x["IN_C"]), s32(x.get("IN_H", 0)), s32(x.get("IN_W", 0))
        cout, oh, ow = s32(x.get("OUT_C", 0)), s32(x.get("OUT_H", 0)), s32(x.get("OUT_W", 0))
        kh, kw, stride, pad = r["kh"], r["kw"], r["stride"], r["pad"]
        if any(k in x for k in ("PAD_T", "PAD_L", "PAD_B", "PAD_R")):
            pt, pl, pb, pr = (s32(x.get(k, 0)) for k in ("PAD_T", "PAD_L", "PAD_B", "PAD_R"))
        else:
            pt = pl = pb = pr = s32(pad)
        pad_val = ((x["PAD_VALUE"] & 0xFF) ^ 0x80) - 0x80 if "PAD_VALUE" in x else 0
        dil = s32(x.get("DILATION", 0)) or 1
        cout_t, h_t, w_t = s32(x.get("TILE_COUT", 0)), s32(x.get("TILE_H", 0)), s32(x.get("TILE_W", 0))
        if cout_t <= 0 or h_t <= 0 or w_t <= 0:
            raise Refused("NO_TILING")
    else:
        # Linear / MatMul given as M positions x K inputs -> N outputs: a 1 x 1 layer on a 1 x M image, default tiling
        cin, ih, iw, cout, oh, ow = r["k"], 1, r["m"], r["n"], 1, r["m"]
        kh = kw = stride = dil = 1
        pt = pl = pb = pr = pad_val = 0
        cout_t = min(32, cout, CAP_B // max(cin, 1))
        if cout_t <= 0:
            raise Refused("NO_TILING")
        w_t = min(ow, CAP_A // max(cin, 1), 24576 // cout_t)
        if w_t >= 32:
            w_t -= w_t % 32
        h_t = 1
        if w_t <= 0:
            raise Refused("NO_TILING")
        notes.append("no extension registers: default tiling %d channels x %d positions" % (cout_t, w_t))
        if not x:
            notes.append("no extension register at all: a system wrapper sends this instruction to the functional decoder, "
                         "whose result is not the one computed here")
    ekh, ekw = (kh - 1) * dil + 1, (kw - 1) * dil + 1   # effective kernel: taps dil apart
    if (min(cin, ih, iw, cout, oh, ow, kh, kw, stride, dil) <= 0 or min(pt, pl, pb, pr) < 0 or (kw - 1) * dil > 63
            or (oh - 1) * stride - pt + ekh > ih + pb or (ow - 1) * stride - pl + ekw > iw + pr):
        raise Refused("BAD_GEOM")
    cin_t = s32(x.get("TILE_CIN", 0))
    cin_t = cin_t if cin_t > 0 else cin
    y_used = s32(x.get("Y_USED", 0))
    if y_used < 0 or y_used > 32 or cin_t > cin or cin % cin_t:
        raise Refused("BAD_GEOM")
    act = r["act"]
    if act > 3:
        raise Refused("UNSUPPORTED_MODE")
    if act in (2, 3) and "LUT_ADDR" not in x:
        raise Refused("NEED_LUT")
    zp, flags = s32(x.get("ZP_OUT", 0)), x.get("FLAGS", 0)
    pad_tail = bool(flags & 1)
    tmaj_c0 = s32(x["TMAJ_C0"]) if "TMAJ_C0" in x else None
    if tmaj_c0 is not None and (not 0 <= tmaj_c0 < cout or tmaj_c0 % cout_t):
        raise Refused("BAD_GEOM")
    if tmaj_c0 is not None and r["has_skip"]:
        raise Refused("UNSUPPORTED_MODE")
    # one row sweep of the IFMAP feeder must stay within the 8-bit position counters of its lanes:
    # xlim = 1 + (Y_used - 1) x stride + effective kernel width + 1 - (width mod 2) + 32 <= 256 for every tile width
    def y_used_of(wt):
        if pad_tail:
            return y_used if 0 < y_used <= 32 else 32
        return y_used if (0 < y_used <= 32 and wt % y_used == 0) else math.gcd(wt, 32)
    widths = {min(w_t, ow), ow - (-(-ow // w_t) - 1) * w_t}
    if max(1 + (y_used_of(wt) - 1) * stride + ekw + 1 - ekw % 2 + 32 for wt in widths) > 256:
        raise Refused("SWEEP")
    K = cin * kh * kw

    if "SCALE_ADDR" in x:
        S = img.i32(x["SCALE_ADDR"], cout, "scale array")
        sh = img.i32(x.get("SHIFT_ADDR", 0), cout, "shift array")
        if "SHIFT_ADDR" not in x:
            notes.append("SCALE_ADDR without SHIFT_ADDR: the shifts are read at offset 0 of the image")
    else:
        fx = float_to_fixed(f32(r["in_scale"]) * f32(r["w_scale"]) / f32(r["out_scale"])) if f32(r["out_scale"]) != 0.0 else None
        if fx is None:
            raise Refused("SCALE_RANGE")
        S, sh = np.full(cout, fx[0], dtype=np.int64), np.full(cout, fx[1], dtype=np.int64)
        notes.append("no SCALE_ADDR: one scale converted from the real-valued registers (S = %d, shift %d); an approximation" % fx)
    if sh.min() < 0 or sh.max() > 63:
        notes.append("a shift is outside 0 .. 63 (range %d .. %d)" % (sh.min(), sh.max()))
        sh = np.clip(sh, 0, 63)
    if r["bias"] == 0:
        notes.append("BIAS_ADDR = 0: the first bytes of the image are used as bias")
    bias = img.i32(r["bias"], cout, "bias array")

    # weights: one block per output-channel range of TILE_COUT channels, laid out [c][kh][kw][k of the range]
    W = np.zeros((cout, cin, kh, kw), dtype=np.float64)
    for c0 in range(0, cout, cout_t):
        nch = cout_t if pad_tail else min(cout_t, cout - c0)
        take = min(nch, cout - c0)
        blk = img.i8(r["w"] + c0 * K, nch * K, "weights of channels %d.." % c0).reshape(cin, kh, kw, nch)
        W[c0:c0 + take] = np.transpose(blk[..., :take], (3, 0, 1, 2))
        if pad_tail and take < nch and np.any(blk[..., take:]):
            notes.append("PAD_TAIL: the padding channels of the last range hold non-zero weights (they are computed and dropped)")
    if cout_t * (cin_t * kh * kw) > CAP_B:
        notes.append("weights of one tile need %d bytes, above %d: the hardware drops the tile, no output is written"
                     % (cout_t * cin_t * kh * kw, CAP_B))
    aw, ah = (min(w_t, ow) - 1) * stride + ekw, (min(h_t, oh) - 1) * stride + ekh
    if cin_t * ah * aw > CAP_A:
        notes.append("input window of one tile needs %d bytes, above %d" % (cin_t * ah * aw, CAP_A))
    if cout_t * min(h_t, oh) * min(w_t, ow) > CAP_C:
        notes.append("output of one tile needs %d elements, above %d" % (cout_t * min(h_t, oh) * min(w_t, ow), CAP_C))

    X = img.i8(r["in"], cin * ih * iw, "input tensor").reshape(cin, ih, iw).astype(np.float64)
    Xp = np.full((cin, ih + pt + pb + ekh, iw + pl + pr + ekw), float(pad_val), dtype=np.float64)   # room below / right
    Xp[:, pt:pt + ih, pl:pl + iw] = X
    acc = np.zeros((cout, oh * ow), dtype=np.float64)   # exact: |sum| < 2^53
    for i in range(kh):
        for j in range(kw):
            y0, x0 = i * dil, j * dil
            win = Xp[:, y0:y0 + (oh - 1) * stride + 1:stride, x0:x0 + (ow - 1) * stride + 1:stride].reshape(cin, oh * ow)
            acc += W[:, :, i, j] @ win
    psum = acc.astype(np.int64) + bias[:, None]
    if psum.size and (psum.max() > 2147483647 or psum.min() < -2147483648):
        notes.append("a partial sum leaves int32 (range %d .. %d)" % (psum.min(), psum.max()))
    q = requant(psum, S[:, None], sh[:, None], zp)
    if act == 1:
        q = np.maximum(q, 0).astype(np.int8)
    elif act in (2, 3):
        q = img.i8(x["LUT_ADDR"], 256, "activation table")[q.astype(np.int16) + 128]
    out_addr, n = r["out"], cout * oh * ow
    if tmaj_c0 is None:
        img.put(out_addr, q, "output tensor")
    else:
        # channels c0.. token-major [OH*OW][OUT_C - c0] in the region they would take channel-major
        q = q.reshape(cout, oh * ow)
        img.put(out_addr, q[:tmaj_c0], "output tensor")
        img.put(out_addr + tmaj_c0 * oh * ow, q[tmaj_c0:].T, "output tensor (token-major)")
        notes.append("TMAJ_C0 %d: channels %d..%d token-major [%d][%d]" % (tmaj_c0, tmaj_c0, cout - 1, oh * ow, cout - tmaj_c0))
    regions = [(out_addr, n)]
    if r["has_skip"]:
        # residual form: the convolution, then out = sat8(out + skip) in place
        b = img.i8(r["skip"], n, "skip tensor").astype(np.int64)
        img.put(out_addr, np.clip(q.ravel().astype(np.int64) + b, -128, 127), "output tensor")
        notes.append("HAS_SKIP: convolution followed by a saturating add of the skip tensor")
    return regions, "[%d][%d][%d]" % (cout, oh, ow)


def elem(st, img, notes):
    r, x = st.reg, st.ext
    mode = r["mode"]
    if mode == 0:
        n = r["len"]
        if (r["a_len"] or n) != n or (r["b_len"] or n) != n:
            raise Refused("BROADCAST")
        if n == 0:
            raise Refused("BAD_GEOM")
        if "SA" in x:
            zpA, zpB, zpO = s32(x.get("ZP_A", 0)), s32(x.get("ZP_B", 0)), s32(x.get("ZP_O", 0))
            SA, SB, SO = s32(x["SA"]), s32(x.get("SB", 0)), s32(x.get("SO", 0))
            sA, sB, sO = s32(x.get("SHA", 0)), s32(x.get("SHB", 0)), s32(x.get("SHO", 0))
        else:
            def valid(v):
                v = f32(v)
                return float(np.float32(v)) if 0.00001 < v < 10000.0 else 1.0
            sa, sb, so = valid(r["r458"]), valid(r["r45c"]), valid(r["scale_o"])
            fa, fb = float_to_fixed(sa / so * 65536.0), float_to_fixed(sb / so * 65536.0)
            if fa is None or fb is None:
                raise Refused("SCALE_RANGE")
            zpA = zpB = zpO = 0
            (SA, sA), (SB, sB), SO, sO = fa, fb, 1 << 30, 46
            notes.append("no integer ADD parameters: scales converted from the real-valued registers; an approximation")
            if not x:
                notes.append("no extension register at all: a system wrapper sends this instruction to the functional decoder, "
                         "whose result is not the one computed here")
        for s_ in (sA, sB, sO):
            if s_ < 0 or s_ > 63:
                raise Refused("address: a shift of the ADD is outside 0 .. 63")
        a = img.i8(r["a"], n, "operand A")
        b = img.i8(r["b"], n, "operand B")
        s = np.clip(dequant(a, zpA, SA, sA) + dequant(b, zpB, SB, sB), -(1 << 31), (1 << 31) - 1)
        img.put(r["out"], requant(s, SO, sO, zpO), "output tensor")
        return [(r["out"], n)], "[%d]" % n
    if mode in (1, 5):
        if "IN_C" not in x or "POOL_K" not in x or (mode == 5 and "SO" not in x):
            raise Refused("BAD_GEOM")
        c, h, w = s32(x["IN_C"]), s32(x.get("IN_H", 0)), s32(x.get("IN_W", 0))
        k, p, s_ = s32(x["POOL_K"]), s32(x.get("POOL_P", 0)), r["stride"]
        if min(c, h, w, k, s_) <= 0 or p < 0 or h + 2 * p < k or w + 2 * p < k or (mode == 5 and p != 0):
            raise Refused("BAD_GEOM")
        ho, wo = (h + 2 * p - k) // s_ + 1, (w + 2 * p - k) // s_ + 1
        X = img.i8(r["a"], c * h * w, "input tensor").reshape(c, h, w).astype(np.int64)
        if mode == 1:
            Xp = np.full((c, h + 2 * p, w + 2 * p), -128, dtype=np.int64)
            Xp[:, p:p + h, p:p + w] = X
            out = np.full((c, ho, wo), -128, dtype=np.int64)
            for i in range(k):
                for j in range(k):
                    out = np.maximum(out, Xp[:, i:i + (ho - 1) * s_ + 1:s_, j:j + (wo - 1) * s_ + 1:s_])
        else:
            sh = s32(x.get("SHO", 0))
            if sh < 0 or sh > 63:
                raise Refused("BAD_GEOM")
            acc = np.zeros((c, ho, wo), dtype=np.int64)
            for i in range(k):
                for j in range(k):
                    acc += X[:, i:i + (ho - 1) * s_ + 1:s_, j:j + (wo - 1) * s_ + 1:s_]
            wrapped = ((acc + 32768) & 0xFFFF) - 32768
            if np.any(wrapped != acc):
                notes.append("AVG_POOL: a window sum leaves int16 and wraps")
            out = np.clip(round_shift_any(mul(wrapped, s32(x["SO"])), sh), -128, 127)
        img.put(r["out"], out, "output tensor")
        return [(r["out"], c * ho * wo)], "[%d][%d][%d]" % (c, ho, wo)
    if mode == 6:   # DMA_SLICE (HAS_DMA_ND=1)
        c, h, w = s32(x.get("IN_C", 0)), s32(x.get("IN_H", 0)), s32(x.get("IN_W", 0))
        ax, start, length = s32(x.get("POOL_MODE", 0)), s32(x.get("POOL_P", 0)), s32(x.get("POOL_K", 0))
        s_ = max(1, s32(r.get("stride", 1)))
        dim = (c, h, w)[ax] if 0 <= ax <= 2 else 0
        if min(c, h, w, length) <= 0 or not (0 <= ax <= 2) or start < 0 or start + length > dim:
            raise Refused("BAD_GEOM")
        X = img.i8(r["a"], c * h * w, "input tensor").reshape(c, h, w)
        if ax == 0:
            out = X[start:start + length:s_, :, :]
        elif ax == 1:
            out = X[:, start:start + length:s_, :]
        else:
            out = X[:, :, start:start + length:s_]
        img.put(r["out"], out, "output tensor")
        return [(r["out"], out.size)], "[%d][%d][%d]" % out.shape
    if mode == 7:   # DMA_CONCAT (HAS_DMA_ND=1): A at POOL_P (length POOL_K) along the axis; with B: A at 0, B after it to the end
        oc = s32(x.get("OUT_C", 0)) or s32(x.get("IN_C", 0))
        oh = s32(x.get("OUT_H", 0)) or s32(x.get("IN_H", 0))
        ow = s32(x.get("OUT_W", 0)) or s32(x.get("IN_W", 0))
        ax, start, len_a = s32(x.get("POOL_MODE", 0)), s32(x.get("POOL_P", 0)), s32(x.get("POOL_K", 0))
        dim = (oc, oh, ow)[ax] if 0 <= ax <= 2 else 0
        has_b = r.get("b", 0) != 0
        if min(oc, oh, ow, len_a) <= 0 or not (0 <= ax <= 2) or start < 0 or start + len_a > dim or                 (has_b and (start != 0 or len_a >= dim)):
            raise Refused("BAD_GEOM")
        parts = [(r["a"], start, len_a, "operand A")] + ([(r["b"], len_a, dim - len_a, "operand B")] if has_b else [])
        T = img.i8(r["out"], oc * oh * ow, "output tensor").copy().reshape(oc, oh, ow)
        for addr, off, ln, what in parts:
            sh = [oc, oh, ow]
            sh[ax] = ln
            idx = [slice(None)] * 3
            idx[ax] = slice(off, off + ln)
            T[tuple(idx)] = img.i8(addr, int(np.prod(sh)), what).reshape(sh)
        if not has_b and ax == 0:
            dst = r["out"] + start * oh * ow
            img.put(dst, T[start:start + len_a], "output tensor slice")
            return [(dst, len_a * oh * ow)], "[%d][%d][%d]" % (len_a, oh, ow)
        img.put(r["out"], T, "output tensor")
        return [(r["out"], T.size)], "[%d][%d][%d]" % T.shape
    raise Refused("UNSUPPORTED_MODE")


def dma_copy(st, img):
    r = st.reg
    if r["mode"] != 0:
        raise Refused("UNSUPPORTED_MODE")
    s, d, n = r["a"], r["out"], r["len"]
    if n == 0 or s + n > 1 << 32 or d + n > 1 << 32 or (s < d + n and d < s + n):
        raise Refused("BAD_GEOM")
    img.put(d, img.i8(s, n, "copy source").copy(), "copy destination")
    return [(d, n)], "[%d] B" % n


def not_computed(st):
    """Output region of an instruction this tool does not compute (copied from the image under test)."""
    r, x = st.reg, st.ext
    if "PARAM_ADDR" not in x:
        raise Refused("BAD_GEOM")
    if st.opcode == 0x13:
        rows = s32(x.get("ROWS", 0)) or r["len"]
        if rows <= 0 or r["len"] <= 0 or r["r458"] <= 0:
            raise Refused("BAD_GEOM")
        return [(r["out"], rows * r["r458"])]
    rows = s32(x.get("ROWS", 0))
    if rows <= 0 or r["len"] <= 0:
        raise Refused("BAD_GEOM")
    return [(r["out"], 2 * rows * r["len"])]   # int8 or int16 rows: the larger of the two is copied


def host_step(st, img):
    kv = {}
    tok = st.text.split()
    if len(tok) < 2 or tok[1] != "upsample2x":
        raise Refused("unknown host step: %s" % st.text)
    for t in tok[2:]:
        if "=" in t:
            a, b = t.split("=", 1)
            kv[a] = int(b, 0)
    c, h, w = kv.get("c", 0), kv.get("h", 0), kv.get("w", 0)
    if not (c and h and w):
        raise Refused("unknown host step: %s" % st.text)
    X = img.i8(kv.get("in", 0), c * h * w, "input of the host step").reshape(c, h, w)
    img.put(kv.get("out", 0), X.repeat(2, axis=1).repeat(2, axis=2), "output of the host step")
    return [(kv.get("out", 0), 4 * c * h * w)], "[%d][%d][%d]" % (c, 2 * h, 2 * w)


def output_region(st):
    """Output region of a step without computing it (steps outside the checked window)."""
    r, x = st.reg, st.ext
    if st.kind == "H":
        kv = dict(t.split("=", 1) for t in st.text.split()[2:] if "=" in t)
        return [(int(kv.get("out", "0"), 0), 4 * int(kv.get("c", "0"), 0) * int(kv.get("h", "0"), 0) * int(kv.get("w", "0"), 0))]
    if st.opcode == 0x12:
        if "IN_C" in x:
            return [(r["out"], max(0, s32(x.get("OUT_C", 0)) * s32(x.get("OUT_H", 0)) * s32(x.get("OUT_W", 0))))]
        return [(r["out"], r["n"] * r["m"])]
    if st.opcode == 0x15:
        if r["mode"] == 0:
            return [(r["out"], r["len"])]
        if r["mode"] == 6:
            c, h, w = s32(x.get("IN_C", 0)), s32(x.get("IN_H", 0)), s32(x.get("IN_W", 0))
            ax, k, s_ = s32(x.get("POOL_MODE", 0)), s32(x.get("POOL_K", 0)), max(1, s32(r.get("stride", 1)))
            sl = (k + s_ - 1) // s_
            if ax == 0:
                c = sl
            elif ax == 1:
                h = sl
            else:
                w = sl
            return [(r["out"], max(0, c * h * w))]
        if r["mode"] == 7:
            oc = s32(x.get("OUT_C", 0)) or s32(x.get("IN_C", 0))
            oh = s32(x.get("OUT_H", 0)) or s32(x.get("IN_H", 0))
            ow = s32(x.get("OUT_W", 0)) or s32(x.get("IN_W", 0))
            if r.get("b", 0) == 0 and s32(x.get("POOL_MODE", 0)) == 0:
                return [(r["out"] + s32(x.get("POOL_P", 0)) * oh * ow, max(0, s32(x.get("POOL_K", 0)) * oh * ow))]
            return [(r["out"], max(0, oc * oh * ow))]
        k, p_, s_ = s32(x.get("POOL_K", 0)), s32(x.get("POOL_P", 0)), r["stride"]
        c, h, w = s32(x.get("IN_C", 0)), s32(x.get("IN_H", 0)), s32(x.get("IN_W", 0))
        if min(c, h, w, k, s_) <= 0 or h + 2 * p_ < k or w + 2 * p_ < k:
            return []
        return [(r["out"], c * ((h + 2 * p_ - k) // s_ + 1) * ((w + 2 * p_ - k) // s_ + 1))]
    if st.opcode == 0x16:
        return [(r["out"], r["len"])]
    if st.opcode in (0x13, 0x14):
        try:
            return not_computed(st)
        except Refused:
            return []
    return []


OPNAME = {0x05: "SET_NSPLIT", 0x12: "GEMM_FUSED", 0x13: "FUSED_ATTN", 0x14: "LAYERNORM", 0x15: "ELEM_WISE", 0x16: "DMA_TENSOR"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("dir", help="program directory with mmio.txt and dram_init.bin")
    ap.add_argument("--image", help="image under test (default: <dir>/dram_golden.bin)")
    ap.add_argument("--first", type=int, default=0, help="first instruction to check (the ones before it are taken from the image under test)")
    ap.add_argument("--count", type=int, default=-1, help="number of instructions to check")
    ap.add_argument("--no-resync", action="store_true",
                    help="keep the reference output after a mismatch (default: continue from the output of the image under test, "
                         "so that every instruction is judged on its own)")
    ap.add_argument("--write-ref", help="write the reference image to this file (implies --no-resync; with --first, or with "
                                        "instructions that are not computed, the file also holds bytes of the image under test)")
    ap.add_argument("--csv", help="write one line per step to this file")
    ap.add_argument("--addr-base", type=lambda v: int(v, 0), default=0,
                    help="value subtracted from every address register (when the program holds bus addresses, not image offsets)")
    a = ap.parse_args()
    if a.write_ref:
        a.no_resync = True   # the file must hold the reference alone, never bytes of the image under test
    try:
        steps, n_instr = read_program(os.path.join(a.dir, "mmio.txt"), a.addr_base)
        with open(os.path.join(a.dir, "dram_init.bin"), "rb") as f:
            ref = Image(f.read())
        ipath = a.image or os.path.join(a.dir, "dram_golden.bin")
        with open(ipath, "rb") as f:
            test = np.frombuffer(f.read(), dtype=np.uint8)
    except OSError as e:
        print("[ref] %s" % e)
        return 2
    last = n_instr if a.count < 0 else min(n_instr, a.first + a.count)
    print("[ref] %s: %d instructions, %d host steps, image %d bytes; image under test %s (%d bytes); checking instructions %d .. %d"
          % (a.dir, n_instr, sum(1 for s in steps if s.kind == "H"), ref.b.size, ipath, test.size, a.first, last - 1))
    if test.size < ref.b.size:
        print("[ref] the image under test is shorter than the program image; missing bytes count as different")
    rows, tot = [], {"checked": 0, "match": 0, "mismatch": 0, "skipped": 0, "refused": 0, "elements": 0, "bad": 0}
    first_bad = None

    def region_diff(addr, n):
        got = np.zeros(n, dtype=np.uint8)
        m = max(0, min(n, test.size - addr))
        if m > 0:
            got[:m] = test[addr:addr + m]
        want = ref.b[addr:addr + n]
        d = np.nonzero(got != want)[0]
        if m < n:
            d = np.union1d(d, np.arange(m, n))
        return got, want, d

    for st in steps:
        active = a.first <= st.index < last
        name = "HOST" if st.kind == "H" else OPNAME.get(st.opcode, "0x%02X" % st.opcode)
        notes, regions, shape, status = [], [], "", ""
        if st.index >= last:
            break
        if not active:
            # before the window: the state of the image under test is taken as it is
            for addr, n in output_region(st):
                m = max(0, min(n, test.size - addr, ref.b.size - addr))
                if addr >= 0 and m > 0:
                    ref.b[addr:addr + m] = test[addr:addr + m]
            continue
        try:
            if st.kind == "H":
                regions, shape = host_step(st, ref)
            elif st.opcode == 0x12:
                regions, shape = gemm(st, ref, notes)
            elif st.opcode == 0x15:
                regions, shape = elem(st, ref, notes)
            elif st.opcode == 0x16:
                regions, shape = dma_copy(st, ref)
            elif st.opcode in (0x13, 0x14):
                regions = not_computed(st)
                status = "NOT CHECKED"
            elif st.opcode == 0x05:
                status = "IGNORED"
            else:
                raise Refused("UNSUPPORTED_OP")
        except Refused as e:
            status = "REFUSED" if not str(e).startswith(("address", "unknown")) else "ERROR"
            notes.insert(0, str(e))
        n_el = sum(n for _, n in regions)
        bad, where = 0, ""
        if status in ("REFUSED", "ERROR"):
            if active:
                tot["refused"] += 1
        elif status == "NOT CHECKED" or not active:
            for addr, n in regions:   # take the output of the image under test
                m = max(0, min(n, test.size - addr, ref.b.size - addr))
                if m > 0:
                    ref.b[addr:addr + m] = test[addr:addr + m]
            if active and status:
                tot["skipped"] += 1
            if not active:
                status = "before the window" if st.index < a.first else "after the window"
        elif status == "IGNORED":
            pass
        else:
            for addr, n in regions:
                got, want, d = region_diff(addr, n)
                bad += int(d.size)
                if d.size and not where:
                    e = int(d[0])
                    where = "first at element %d (address 0x%X): reference %d, image %d; largest difference %d" % (
                        e, addr + e, int(want.view(np.int8)[e]), int(got.view(np.int8)[e]),
                        int(np.abs(want.view(np.int8).astype(np.int16) - got.view(np.int8).astype(np.int16)).max()))
                if d.size and not a.no_resync:
                    m = max(0, min(n, test.size - addr))
                    ref.b[addr:addr + m] = test[addr:addr + m]
            status = "MATCH" if bad == 0 else "MISMATCH"
            tot["checked"] += 1
            tot["match" if bad == 0 else "mismatch"] += 1
            tot["elements"] += n_el
            tot["bad"] += bad
            if bad and first_bad is None:
                first_bad = st
        if active or status in ("REFUSED", "ERROR"):
            tag = "host step before instruction %d" % st.index if st.kind == "H" else "instruction %d" % st.index
            print("[REF] %-34s %-10s %-18s elements %-9d different %-8d %-11s # %s" % (tag, name, shape, n_el, bad, status, st.label))
            if where:
                print("[REF]     %s" % where)
            for n in notes:
                print("[REF]     note: %s" % n)
        rows.append((st.index, st.kind, name, shape, n_el, bad, status, "; ".join(notes + ([where] if where else [])), st.label))
    verdict = "NOTHING CHECKED" if tot["checked"] == 0 and tot["refused"] == 0 else (
        "MATCH" if tot["mismatch"] == 0 and tot["refused"] == 0 else "MISMATCH")
    print("[ref] RESULT: %s (checked %d: match %d, mismatch %d; not checked %d; refused or in error %d; elements %d, different %d)"
          % (verdict, tot["checked"], tot["match"],
             tot["mismatch"], tot["skipped"], tot["refused"], tot["elements"], tot["bad"]))
    if first_bad is not None:
        print("[ref] first mismatch: %s %d, line %d of mmio.txt # %s"
              % ("host step before instruction" if first_bad.kind == "H" else "instruction", first_bad.index, first_bad.line, first_bad.label))
    if a.csv:
        with open(a.csv, "w") as f:
            f.write("instruction,kind,operation,shape,elements,different,status,notes,label\n")
            for r_ in rows:
                f.write(",".join('"%s"' % str(v).replace('"', "'") if i >= 7 else str(v) for i, v in enumerate(r_)) + "\n")
    if a.write_ref:
        with open(a.write_ref, "wb") as f:
            f.write(ref.b.tobytes())
    return 0 if tot["mismatch"] == 0 and tot["refused"] == 0 and tot["checked"] > 0 else 1


if __name__ == "__main__":
    sys.exit(main())
