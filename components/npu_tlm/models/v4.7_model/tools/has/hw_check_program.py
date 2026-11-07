#!/usr/bin/env python3
"""Which instructions of a program the hardware (neo_lite RTL revision 1) can run, and why not. No SystemC, no model.

Reads a program directory (mmio.txt) or an mmio.txt file -- the register-write stream the compiler writes and the model replays
(tools/has/tb_has_npu_top) -- and prints one line per instruction in the format of the model's HAS_HW_CHECK=1:

    [hw-check] i=<n> op=<name> HW revision 1: OK | TRANSFORM <codes> | NO <codes>

    OK         the hardware runs the instruction as written
    TRANSFORM  only reasons the compiler removes (padding placed in DRAM, tensors cut to the tile grid, scale per channel)
    PATCH      an RTL fault or gap of the delivered hardware (class A) and nothing worse; a small RTL change removes it
    NO         a reason that only the hardware or the model can remove (activation table, bias / shift blocks, FA, LN, DMA ...)
The verdict is the worst of the codes (NO, PATCH, TRANSFORM, OK). The reason codes are those of the host-side tool
(COMPILER_GUIDE.md, support matrix): A-* = fault or gap of the delivered RTL, B-* = the model goes beyond the hardware, P-* = note only.
The rules are those of has/hw_capability.h (same codes, same order, same severity); the two print the same lines for a program, which is
how they are kept in step. Old-style m/k/n GEMM instructions: the tile grid is chosen by the model, so B-EDGE / B-INDIM are not given.

Usage: python3 tools/has/hw_check_program.py <program dir | mmio.txt> [--csv FILE] [--quiet]
  --quiet  summary only.   --csv  one row per instruction: index, op, verdict, codes.
Exit code 0 always when the file could be read (the verdicts are the output), 2 on a usage or file error.
"""
import argparse
import math
import os
import struct
import sys

# ---- register map and stream reader: the same as tools/has/ref_program.py, copied so that this tool needs Python 3 only (no numpy) ----
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


class Step:
    def __init__(self, kind, index):
        self.kind, self.index = kind, index
        self.opcode = 0
        self.reg, self.ext = {}, {}


def s32(v):
    v &= 0xFFFFFFFF
    return v - (1 << 32) if v & 0x80000000 else v


def f32(v):
    return struct.unpack("<f", struct.pack("<I", v & 0xFFFFFFFF))[0]


def float_to_fixed(v):
    """Real-valued scale -> (S, s) with S in [2^30, 2^31) and value = S * 2^-s; None when s leaves 0 .. 63."""
    if not (v > 0.0) or not math.isfinite(v):
        return None
    e = int(math.floor(math.log2(v)))
    S = int(math.floor(math.ldexp(v, 30 - e) + 0.5))
    if S >= (1 << 31):
        S >>= 1
        e += 1
    s = 30 - e
    return (S, s) if 0 <= s <= 63 else None


def read_program(path):
    """Steps of a program: one "I" step per push of an instruction (registers and extension registers as written until then)."""
    reg = {n: 0 for n in REG.values()}
    reg.update(REG_DEFAULT)
    for n in ("in_scale", "w_scale", "out_scale", "r458", "r45c", "scale_o"):
        reg[n] = FLOAT_ONE
    ext, steps, n_instr = {}, [], 0
    with open(path, "r", errors="replace") as f:
        for line in f:
            tok = line.split()
            if not tok or tok[0] != "W" or len(tok) < 3:
                continue
            a, v = int(tok[1], 16) - BASE, int(tok[2], 16)
            if EXT_BASE <= a < EXT_END and (a - EXT_BASE) % 4 == 0:
                ext[EXT[(a - EXT_BASE) // 4]] = v
            elif a in REG:
                name = REG[a]
                if name == "mode":
                    v = (v >> 24) & 0xFF if v & 0xFFFF0000 else v
                reg[name] = v
            elif a == PUSH_A:
                st = Step("I", n_instr)
                st.opcode, st.reg, st.ext = v & 0xFF, dict(reg), dict(ext)
                steps.append(st)
                n_instr += 1
                ext = {}
    return steps

# ---- reason codes: those of the host-side tool (COMPILER_GUIDE.md, support matrix); order and severity as in has/hw_capability.h ----
CODES = ["A-H1", "A-H3", "A-H4",
         "B-ACTLUT", "B-REQ3", "B-FSCALE", "B-PAD", "B-INDIM", "B-EDGE", "B-PADTAIL", "B-CINORD", "B-HALO", "B-STRIDE", "B-TMAJ",
         "B-ADDMAP", "B-ADDTILE", "B-POOLPAD", "B-POOLFIT", "B-POOLK", "B-AVG", "B-FA", "B-LN", "B-DMA", "B-FIELD",
         "P-SPLIT", "P-H2", "P-POOLDENSE"]
# A = RTL fault / gap, T = removed by the compiler, N = removed only by the hardware or the model, I = note only
SEVERITY = "AAA" "NN" "TTTTT" "NN" "T" "N" "TT" "TT" "N" "NNNN" "T" "III"
assert len(CODES) == len(SEVERITY)
C = {n: i for i, n in enumerate(CODES)}
NOT_CHECKED = "[hw-check] not checked: contiguity of the tile in DRAM"


def cdiv(a, b):
    return -(-a // b)


def residual_map(zpA, zpB, zpO, SA, SB, SO, sA, sB, sO, round_mode=1, sub_before=True):
    """Port of rtl_ref_gvu::ResidualHw::map (has/rtl_ref_gvu_residual_hw.h): can the model's ADD be written as one RESIDUAL."""
    if not sub_before or round_mode not in (0, 1) or sO < 0 or sO > 63 or sA < 0 or sB < 0:
        return False
    if SA < 0 or SB < 0 or SO < 0 or SO > 0x7FFFFFFF or SA > 0x7FFFFFFF or SB > 0x7FFFFFFF:
        return False
    if 0 < sA < 31 and (SA & ((1 << sA) - 1)) == 0:
        SA >>= sA
        sA = 0
    if 0 < sB < 31 and (SB & ((1 << sB) - 1)) == 0:
        SB >>= sB
        sB = 0
    if sA != 0 or sB != 0:
        return False
    if SO > 0:
        t = 0
        while t < sO and (SO & (1 << t)) == 0:
            t += 1
        SO >>= t
        sO -= t
    if SA > 4000000 or SB > 4000000:
        return False
    if zpO < -128 or zpO > 127:
        return False
    sa, sb = SA * SO, SB * SO
    if sa > 0x7FFFFFFF or sb > 0x7FFFFFFF:
        return False
    if SO > 0 and (SA > 0x7FFFFFFF // SO or SB > 0x7FFFFFFF // SO):
        return False
    return True


def valid_scale(bits):
    v = f32(bits)
    return v if (v > 0.00001 and v < 10000.0) else 1.0


def add_params(st):
    """AddQ of an ELEM_WISE ADD as the model derives it (has_mmio_compat.h push_elem); None when the model refuses the instruction."""
    e, r = st.ext, st.reg
    if "SA" in e:
        return (s32(e.get("ZP_A", 0)), s32(e.get("ZP_B", 0)), s32(e.get("ZP_O", 0)), e["SA"] & 0xFFFFFFFF, s32(e.get("SHA", 0)),
                e.get("SB", 0) & 0xFFFFFFFF, s32(e.get("SHB", 0)), e.get("SO", 0) & 0xFFFFFFFF, s32(e.get("SHO", 0)))
    sa, sb, so = valid_scale(r["r458"]), valid_scale(r["r45c"]), valid_scale(r["scale_o"])
    a, b = float_to_fixed(sa / so * 65536.0), float_to_fixed(sb / so * 65536.0)
    if a is None or b is None:
        return None
    return (0, 0, 0, a[0], a[1], b[0], b[1], 1 << 30, 46)


def add_codes(n, p):
    """ADD of n elements with parameters p = (zpA, zpB, zpO, SA, sA, SB, sB, SO, sO)."""
    zpA, zpB, zpO, SA, sA, SB, sB, SO, sO = p
    codes = set()
    if n % 32:
        codes.add(C["B-ADDTILE"])
    # the words give back zpO (int8), sO (6 bit), SA, SB; zpA, zpB, sA, sB must be 0 and SO 1
    if not residual_map(zpA, zpB, zpO, SA, SB, SO, sA, sB, sO) and (
            zpA != 0 or zpB != 0 or sA != 0 or sB != 0 or SO != 1 or zpO < -128 or zpO > 127 or sO < 0 or sO > 63):
        codes.add(C["B-ADDMAP"])
    return codes


def pool_codes(h, w, k, s, p, pool_mode, codes):
    hp, wp = h + 2 * p, w + 2 * p
    nw = cdiv(wp, 32)
    if k < 2 or k > 5:
        codes.add(C["B-POOLK"])
    if s < 0 or s > 7:
        codes.add(C["B-POOLFIT"])
    if pool_mode != 0:
        codes.add(C["B-POOLFIT"])
    if wp != w:
        codes.add(C["B-POOLPAD"])
    n8 = nw & 0xFF
    h_back = (((nw * hp) & 0x3FF) // n8) if n8 else 0
    if h_back != h:
        codes.add(C["B-POOLPAD"] if p > 0 else C["B-POOLFIT"])
    if p > 0:
        codes.add(C["B-POOLPAD"])


def check(st):
    """List of (op name, codes) for one instruction step (two entries for an HAS_SKIP split); None when the model would not run it."""
    e, r = st.ext, st.reg
    op = st.opcode
    codes = set()
    if op == 0x12:
        has_skip = r["has_skip"] != 0
        codes.add(C["A-H1"])
        kh, kw, s = r["kh"], r["kw"], r["stride"]
        pad_val, dil = 0, 1
        if "IN_C" in e:
            in_c, in_h, in_w = s32(e["IN_C"]), s32(e.get("IN_H", 0)), s32(e.get("IN_W", 0))
            cout, oh, ow = s32(e.get("OUT_C", 0)), s32(e.get("OUT_H", 0)), s32(e.get("OUT_W", 0))
            if any(k in e for k in ("PAD_T", "PAD_L", "PAD_B", "PAD_R")):
                pads = [s32(e.get(k, 0)) for k in ("PAD_T", "PAD_L", "PAD_B", "PAD_R")]
            else:
                pads = [s32(r["pad"])] * 4
            pad_val = ((e["PAD_VALUE"] & 0xFF) ^ 0x80) - 0x80 if "PAD_VALUE" in e else 0
            dil = s32(e["DILATION"]) if s32(e.get("DILATION", 0)) != 0 else 1
            if kh == 1 and kw == 1:
                dil = 1
            cout_t, h_t, w_t = s32(e.get("TILE_COUT", 0)), s32(e.get("TILE_H", 0)), s32(e.get("TILE_W", 0))
            if cout_t <= 0 or h_t <= 0 or w_t <= 0:
                return None
        else:
            # old-style Linear / MatMul: M positions x K inputs -> N outputs, a 1x1 convolution on a 1 x M image; the model picks the tile
            in_c, in_h, in_w = r["k"], 1, r["m"]
            cout, oh, ow = r["n"], 1, r["m"]
            kh = kw = s = 1
            pads = [0, 0, 0, 0]
            h_t = 1
            cout_t = min(32, cout, 82944 // max(in_c, 1))
            if cout_t <= 0:
                return None
            w_t = min(ow, 80896 // max(in_c, 1), 24576 // cout_t)
            if w_t >= 32:
                w_t -= w_t % 32
            if w_t <= 0:
                return None
        cin_t = s32(e["TILE_CIN"]) if s32(e.get("TILE_CIN", 0)) > 0 else in_c
        flags = e.get("FLAGS", 0)
        pad_tail = bool(flags & 1)
        # normalized as the host-side tool does: tile sizes clamped to the tensor (the channel tile not for PAD_TAIL)
        cout_t = cout_t if pad_tail else min(cout_t, cout)
        h_t, w_t = min(h_t, oh), min(w_t, ow)
        if cout_t > 0 and h_t > 0 and w_t > 0 and cout > 0 and oh > 0 and ow > 0:
            if cout % cout_t or oh % h_t or ow % w_t:
                codes.add(C["B-EDGE"])
            Cw, Ch = cdiv(ow, w_t) * w_t, cdiv(oh, h_t) * h_t
            if in_w != (Cw - 1) * s + 1 + (kw - 1) * dil or in_h != (Ch - 1) * s + 1 + (kh - 1) * dil:
                codes.add(C["B-INDIM"])
        if any(p != 0 for p in pads) or pad_val != 0:
            codes.add(C["B-PAD"])
        if flags != 0:
            codes.add(C["B-PADTAIL"])
        if kw == 1 and kh > 1 and dil > 1:
            codes.add(C["B-FIELD"])
        if 0 < cin_t < in_c:
            codes.add(C["B-CINORD"])
        if "TMAJ_C0" in e:
            codes.add(C["B-TMAJ"])
        if "SCALE_ADDR" in e:
            codes.add(C["A-H4"])
        else:
            m = f32(r["in_scale"]) * f32(r["w_scale"]) / f32(r["out_scale"])
            fx = float_to_fixed(m)
            if fx is None:
                return None
            if fx != (1, 0):
                codes.add(C["B-FSCALE"])
        n_skip = cout * oh * ow
        if r["act"] != 0 or e.get("LUT_ADDR", 0) != 0:
            codes.add(C["B-ACTLUT"])
        if r["act"] in (2, 3):
            codes.add(C["P-H2"])
        if r["bias"] != 0 or e.get("SHIFT_ADDR", 0) != 0:
            codes.add(C["B-REQ3"])
        out = []
        if has_skip:
            codes.add(C["P-SPLIT"])
            out.append(("GEMM_FUSED", codes))
            unit = (0, 0, 0, 1 << 30, 14, 1 << 30, 14, 1 << 30, 46)   # unit_add(): exact sat8(A + B)
            out.append(("ADD", add_codes(n_skip, unit)))
        else:
            out.append(("GEMM_FUSED", codes))
        return out
    if op == 0x15:
        mode = r["mode"]
        if mode == 0:
            p = add_params(st)
            if p is None:
                return None
            return [("ADD", add_codes(r["len"], p))]
        if mode in (1, 5):
            h, w, k, pp = s32(e.get("IN_H", 0)), s32(e.get("IN_W", 0)), s32(e.get("POOL_K", 0)), s32(e.get("POOL_P", 0))
            s, pm = r["stride"], s32(e.get("POOL_MODE", 0))
            pool_codes(h, w, k, s, pp, pm, codes)
            if r["b"] != 0:   # B_ADDR left over from an earlier instruction: the words do not give it back
                codes.add(C["B-POOLFIT"])
            if mode == 5:
                codes.add(C["A-H3"])
                if e.get("SO", 0) != 0 or s32(e.get("SHO", 0)) != 0:
                    codes.add(C["B-AVG"])
            elif s in (2, 4) and k != s:
                codes.add(C["P-POOLDENSE"])
            return [("MAX_POOL" if mode == 1 else "AVG_POOL", codes)]
        if mode in (6, 7, 8, 9):
            codes.add(C["B-DMA"])
            return [({6: "SLICE", 7: "CONCAT", 8: "UPSAMPLE", 9: "RESHAPE"}[mode], codes)]
        return None
    if op == 0x16:
        codes.add(C["B-DMA"])
        return [("DMA_TENSOR", codes)]
    if op == 0x13:
        codes.update((C["A-H3"], C["B-FA"], C["P-H2"]))
        return [("FUSED_ATTN", codes)]
    if op == 0x14:
        codes.update((C["A-H3"], C["B-LN"]))
        return [("LAYERNORM", codes)]
    return None


def verdict(codes):
    sev = {SEVERITY[c] for c in codes}
    return "NO" if "N" in sev else ("PATCH" if "A" in sev else ("TRANSFORM" if "T" in sev else "OK"))


def main():
    ap = argparse.ArgumentParser(description="which instructions of a program the hardware (neo_lite RTL revision 1) can run")
    ap.add_argument("path", help="program directory or mmio.txt")
    ap.add_argument("--csv")
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()
    path = a.path
    if os.path.isdir(path):
        path = os.path.join(path, "mmio.txt")
    if path.endswith(".bin") or not os.path.isfile(path):
        print("hw_check_program: need a program directory or its mmio.txt (got %s)" % a.path, file=sys.stderr)
        return 2
    steps = read_program(path)
    rows, counts, per_code = [], {"OK": 0, "TRANSFORM": 0, "PATCH": 0, "NO": 0}, [0] * len(CODES)
    for st in steps:
        r = check(st)
        if r is None:
            continue
        for op, codes in r:
            v = verdict(codes)
            ordered = sorted(codes)
            rows.append((st.index, op, v, ordered))
            counts[v] += 1
            for c in ordered:
                per_code[c] += 1
            if not a.quiet:
                print("[hw-check] i=%d op=%s HW revision 1: %s%s" % (st.index, op, v, (" " + ",".join(CODES[c] for c in ordered)) if ordered else ""))
    n = sum(counts.values())
    print("[hw-check] summary: %d instructions, OK %d, TRANSFORM %d, PATCH %d, NO %d" % (n, counts["OK"], counts["TRANSFORM"], counts["PATCH"], counts["NO"]))
    for c in range(len(CODES)):
        if per_code[c]:
            print("[hw-check]   %-10s %s %d" % (CODES[c], SEVERITY[c], per_code[c]))
    print(NOT_CHECKED)
    if a.csv:
        with open(a.csv, "w") as f:
            f.write("index,op,verdict,codes\n")
            for i, op, v, cs in rows:
                f.write("%d,%s,%s,%s\n" % (i, op, v, ";".join(CODES[c] for c in cs)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
