#!/usr/bin/env python3
"""Whole-network program for the CPU path (HAS_CPU_DIRECT=1, has/has_cpu_direct.h), written from the trace of the data flow controller of the model.

Step 1 (model, data flow controller): run the network program with the trace on:
    HAS_NEO_REV=2 HAS_CPU_DIRECT=1 HAS_CPU_DIRECT_NET=1 HAS_CPU_DIRECT_DUMP=<trace dir> tb_has_npu_top <program dir> [--first F --count N]
  <trace dir>/net.txt: the events of the controller in time order (layers, core calls with their 29 words and the hashes of the SRAM halves the core reads,
  the packets of the PSM stream, element-wise instructions and the register writes of their vector-unit runs); <trace dir>/ret.txt: the output region of
  every retired instruction and the host steps.
Step 2 (this tool): write <out dir>/mmio.txt, dram_init.bin, dram_golden.bin for the CPU:
    mk_cpu_net.py <trace dir> <program dir> <out dir> [--dram-base 0x80000000] [--check] [--no-prefetch] [--max-btt 65536]
Step 3: HAS_NEO_REV=2 HAS_CPU_DIRECT=1 tb_has_npu_top <out dir>   (RESULT: PASS, no "START without a soft reset" line)

The CPU program follows the rules for firmware that also runs on the FPGA (CPU_DIRECT_GUIDE.md, section 8):
  - a soft reset (SAURIA CONTROL bit 23, then polling of ready) before every START after the first;
  - every wait is a polling loop (DMA interrupt status, SAURIA interrupt status / ready, GVU CONTROL done);
  - a keep bit of CONTROL goes in the same write as the start bit;
  - GEMM_FUSED: the sbank holds the Out_Scale / Out_Shift of the k-th result packet since the enable (packet order of the trace), at most 614 packets
    and 1,024 vbank2 rows per enable: a layer is split into enables at tile boundaries; the real calls of an enable carry no oc_tile_last, and every
    enable is closed by a call of ONE context (1 channel, 32 positions, one packet) with oc_tile_last, which reads the halves the core already holds
    (keep A, B, C) and whose packet is not written back;
  - the activation table is reset (LUT reset register, bit 0) before a layer whose table differs from the one loaded;
  - SRAM halves: the input window, weight block and partial-sum preload go into the half the core does not use and the START flips it; a block the core
    already holds is kept (keep bit), a block already in the other half is flipped in without a DMA, the partial sums of an earlier Cin pass are kept;
    with prefetch (default) the blocks of call i + 1 are moved while call i runs, as the controller does;
  - data the controller derives from constants (the partial-sum preload = bias broadcast over the positions, the sbank images, the ReLU table, the
    padding frame of max pool, blocks of a non-zero padding value) is laid out once after the program image ("constant area"), as a compiler would.
--check: rebuild the bytes of the SRAM A / B / C halves of every call from the golden image and the constant area and compare their FNV-1a hashes with the
trace (the provenance of every byte the core reads). Output: one summary line (counts of register writes, polls, DMA commands, calls, enables).
"""
import os
import struct
import sys

SAURIA, GVU, DMA = 0x11000, 0x12000, 0x13000
REGION = [(0x200, 2), (0x400, 17), (0x600, 5), (0x800, 5)]
BUS_A, BUS_B, BUS_C = 0x00000000, 0x00040000, 0x00080000
BUS_V0, BUS_V1, BUS_V2, BUS_SB = 0x00100000, 0x00120000, 0x00140000, 0x00180000
SHIFT_BASE, MAX_BEATS, MAX_ROWS, OP_GEMM_FUSED = 2456, 614, 1024, 2
KEEP = {"A": 1 << 17, "B": 1 << 18, "C": 1 << 19}
SOFT_RESET, START = 0x00800000, 0x1


def fnv(b):
    h = 1469598103934665603
    for x in b:
        h ^= x
        h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


def kv(tokens):
    d = {}
    for t in tokens:
        if "=" in t:
            k, v = t.split("=", 1)
            d[k] = int(v, 16) if k in ("hA", "hB", "hC") else int(v)
    return d


# ------------------------------------------------------------------ trace
def read_trace(tdir):
    instrs, Z, bits = [], None, None
    cur = None
    for line in open(os.path.join(tdir, "net.txt")):
        t = line.split()
        if not t or t[0] == "#": continue
        k = t[0]
        if k == "B": bits = [int(x) for x in t[1:]]
        elif k == "Z": Z = [int(x, 16) for x in t[1:]]
        elif k == "I": cur = {"seq": int(t[1]), "op": int(t[2]), "mode": int(t[3]), "G": None, "E": None, "calls": [], "runs": [], "done": False}; instrs.append(cur)
        elif k == "D": cur["done"] = True
        elif k == "G": cur["G"] = kv(t[1:])
        elif k == "C": cur["calls"].append({"c": kv(t[1:]), "w": None, "pk": []})
        elif k == "w": cur["calls"][-1]["w"] = [int(x, 16) for x in t[1:]]
        elif k == "P": cur["calls"][-1]["pk"].append((int(t[1]), int(t[2], 16), int(t[3])))
        elif k == "E": cur["E"] = kv(t[1:])
        elif k == "r": cur["runs"].append({"kind": t[1], "p": kv(t[2:]), "g": []})
        elif k == "g": cur["runs"][-1]["g"].append((int(t[1], 16), int(t[2], 16)))
        else: raise SystemExit("unknown trace line: " + line)
    ret, hosts, first = [], [], None
    for line in open(os.path.join(tdir, "ret.txt")):
        t = line.split()
        if not t: continue
        if t[0] == "F": first = (int(t[1]), int(t[2]), int(t[3]))
        elif t[0] == "R": ret.append((int(t[1]), int(t[2]), int(t[3]), " ".join(t[4:])))
        elif t[0] == "H": hosts.append((int(t[1]), " ".join(t[2:])))
    return instrs, Z, bits, ret, hosts, first


def parse_h(text):
    d = {}
    for tok in text.split()[2:]:
        k, v = tok.split("=")
        d[k] = int(v, 0)
    return d


# ------------------------------------------------------------------ program writer
class Prog:
    def __init__(self, dram_base, max_btt):
        self.L, self.base, self.max_btt, self.used, self.use_opgen = [], dram_base, max_btt, [], True
        self.n = {"W": 0, "P": 0, "dma": 0, "dma_zero": 0, "dma_bytes": 0, "starts": 0, "soft": 0, "enables": 0, "close_calls": 0, "K": 0, "H": 0, "gvu_runs": 0}

    def c(self, s): self.L.append("# " + s)
    def w(self, a, v): self.L.append("W 0x%05x 0x%08x" % (a, v & 0xFFFFFFFF)); self.n["W"] += 1
    def p(self, a, m, v): self.L.append("P 0x%05x 0x%x 0x%x" % (a, m, v)); self.n["P"] += 1

    def dma(self, r, wa, n, zero=False):
        """one-dimensional DMA commands (split at max_btt); r / wa are bus addresses (DRAM = dram_base + offset)"""
        o = 0
        while o < n:
            k = min(self.max_btt, n - o)
            if not zero and r >= self.base: self.used.append((r + o - self.base, k))
            if wa >= self.base: self.used.append((wa + o - self.base, k))
            if not zero: self.w(DMA + 0x10, r + o)
            self.w(DMA + 0x20, wa + o)
            self.w(DMA + 0x30, k)
            self.w(DMA, 0x102 if zero else 0x3)
            self.p(DMA + 0x0C, 0x2, 0x2)
            self.w(DMA + 0x0C, 0x3)
            self.n["dma"] += 1; self.n["dma_bytes"] += k
            if zero: self.n["dma_zero"] += 1
            o += k

    def d(self, off): return self.base + off

    # tensor register file of the DMA (redma_opgen, RTL revision 2; has/has_opgen.h): DRAM to DRAM only in the model
    OG = {"CTRL": 0x00, "OP": 0x04, "RADDR": 0x08, "WADDR": 0x0C, "CNT01": 0x10, "CNT2": 0x14, "GRPB": 0x18, "ROWB": 0x1C, "RS0": 0x20, "WS0": 0x24,
          "RS1": 0x28, "WS1": 0x2C, "RS2": 0x30, "WS2": 0x34, "PADL": 0x38, "PADR": 0x3C, "FULL": 0x40}

    def opgen(self, op, raddr, waddr, n0=1, n1=1, n2=1, row=0, rs0=0, ws0=0, rs1=0, ws1=0, rs2=0, ws2=0, padl=0, padr=0, full=0, pt=0, pb=0, fill=0,
              used=()):
        """one descriptor (all its registers written), START, polling of DONE, clear; raddr / waddr DRAM offsets"""
        B = DMA + 0x80
        opw = (op & 3) | (pt & 0xFF) << 2 | (pb & 0xFF) << 10 | (fill & 0xFF) << 24
        for k, v in (("OP", opw), ("RADDR", self.base + raddr), ("WADDR", self.base + waddr), ("CNT01", (n0 & 0xFFFF) | (n1 & 0xFFFF) << 16), ("CNT2", n2 & 0xFFFF),
                     ("GRPB", 0), ("ROWB", row), ("RS0", rs0), ("WS0", ws0), ("RS1", rs1), ("WS1", ws1), ("RS2", rs2), ("WS2", ws2), ("PADL", padl),
                     ("PADR", padr), ("FULL", full)):
            self.w(B + self.OG[k], v)
        self.w(B, 0x1); self.p(B, 0x4, 0x4); self.w(B, 0x2)
        self.n["opgen"] = self.n.get("opgen", 0) + 1
        # whole spans of both sides (a compacted image must keep the strides of the descriptor)
        if op == 2: rsp, wsp = (0, max(0, n1 - 1) * rs1 + (n2 - 1) * rs2 + row), (0, (pt + n1 + pb - 1) * ws1 + (n2 - 1) * ws2 + full)
        else:
            cr = [i0 * rs0 + i1 * rs1 + i2 * rs2 for i0 in (0, n0 - 1) for i1 in (0, n1 - 1) for i2 in (0, n2 - 1)]
            cw = [i0 * ws0 + i1 * ws1 + i2 * ws2 for i0 in (0, n0 - 1) for i1 in (0, n1 - 1) for i2 in (0, n2 - 1)]
            rsp, wsp = (min(cr), max(cr) + row), (min(cw), max(cw) + row)
        self.used += [(raddr + rsp[0], rsp[1] - rsp[0]), (waddr + wsp[0], wsp[1] - wsp[0])]

    def copy_runs(self, runs):
        """DRAM to DRAM runs (src, dst, len) as COPY descriptors of up to three loops (compressed), or one-dimensional commands when --no-opgen"""
        runs = sorted(runs, key=lambda r: r[1])
        if not self.use_opgen:
            for s, d, n in runs: self.dma(self.d(s), self.d(d), n)
            return
        self.used += [(s, n) for s, _, n in runs] + [(d, n) for _, d, n in runs]
        for d in compress(runs):
            s0, d0, n, n0, rs0, ws0, n1, rs1, ws1, n2, rs2, ws2 = d
            self.opgen(0, s0, d0, n0, n1, n2, n, rs0, ws0, rs1, ws1, rs2, ws2)


def compress(runs, cap=65535):
    """runs (s, d, n) in order -> descriptors (s0, d0, n, n0, rs0, ws0, n1, rs1, ws1, n2, rs2, ws2): run k of the descriptor is
    s0 + i0 rs0 + i1 rs1 + i2 rs2 (same for d), i0 fastest"""
    out, i, N = [], 0, len(runs)
    while i < N:
        s0, d0, n = runs[i]
        n0, rs0, ws0 = 1, 0, 0
        if i + 1 < N and runs[i + 1][2] == n:
            rs0, ws0 = runs[i + 1][0] - s0, runs[i + 1][1] - d0
            n0 = 2
            while i + n0 < N and n0 < cap and runs[i + n0][2] == n and runs[i + n0][0] - runs[i + n0 - 1][0] == rs0 and runs[i + n0][1] - runs[i + n0 - 1][1] == ws0: n0 += 1

        def rep(blk, start, cnt_cap):
            if start + blk >= N: return 1, 0, 0
            rs, ws = runs[start + blk][0] - runs[start][0], runs[start + blk][1] - runs[start][1]
            k = 1
            while k < cnt_cap and start + (k + 1) * blk <= N and all(
                    runs[start + k * blk + t][2] == runs[start + t][2] and runs[start + k * blk + t][0] - runs[start + t][0] == k * rs and
                    runs[start + k * blk + t][1] - runs[start + t][1] == k * ws for t in range(blk)):
                k += 1
            return k, rs, ws
        n1, rs1, ws1 = rep(n0, i, cap)
        n2, rs2, ws2 = rep(n0 * n1, i, cap) if n1 > 1 or True else (1, 0, 0)
        if n1 == 1: rs1 = ws1 = 0
        if n2 == 1: rs2 = ws2 = 0
        out.append((s0, d0, n, n0, rs0, ws0, n1, rs1, ws1, n2, rs2, ws2))
        i += n0 * n1 * n2
    return out


ADD_ROWS, OP_RESIDUAL = 128, 6


def rtl_add_map(E, round_half=True):
    """ELEM_WISE ADD parameters -> RESIDUAL registers (sa, sb, bias, sh, z) of the RTL, or None (port of rtl_add_map of has/gvu_top_stream.h,
    signed scales, round half up = the half in the bias)"""
    s32 = lambda v: v - (1 << 32) if v >= 1 << 31 else v
    SA, SB, SO, sA, sB, sO = s32(E["SA"]), s32(E["SB"]), s32(E["SO"]), E["sA"], E["sB"], E["sO"]
    fit = lambda v, b: -(1 << (b - 1)) <= v < (1 << (b - 1))
    if sA < 0 or sB < 0 or sO < 0 or sO > 63: return None
    if not (fit(E["zpA"], 8) and fit(E["zpB"], 8) and fit(E["zpO"], 8)) or SO < 0 or SO > 0x7FFFFFFF or not fit(SA, 32) or not fit(SB, 32): return None
    if 0 < sA < 31 and SA % (1 << sA) == 0: SA >>= sA; sA = 0
    if 0 < sB < 31 and SB % (1 << sB) == 0: SB >>= sB; sB = 0
    if sA or sB: return None
    if SO > 0:
        t = 0
        while t < sO and not (SO >> t) & 1: t += 1
        SO >>= t; sO -= t
    sa, sb = SA * SO, SB * SO
    if not (fit(sa, 32) and fit(sb, 32)): return None
    bias = ((1 << (sO - 1)) if (round_half and sO > 0) else 0) - (E["zpA"] * sa + E["zpB"] * sb)
    return sa, sb, bias & 0xFFFFFFFFFFFFFFFF, sO, E["zpO"]


def merge_runs(pairs):
    """pairs (src, dst) per byte, in order -> runs (src, dst, len) where both sides are contiguous"""
    runs = []
    for s, d in pairs:
        if runs and runs[-1][0] + runs[-1][2] == s and runs[-1][1] + runs[-1][2] == d: runs[-1][2] += 1
        else: runs.append([s, d, 1])
    return runs


def merge_seg(segs):
    """segments (src, dst, len) in order -> merged where both sides continue"""
    out = []
    for s, d, n in segs:
        if n <= 0: continue
        if out and out[-1][0] + out[-1][2] == s and out[-1][1] + out[-1][2] == d: out[-1][2] += n
        else: out.append([s, d, n])
    return out


# ------------------------------------------------------------------ generator
class Gen:
    def __init__(self, tdir, pdir, odir, dram_base, check, prefetch, max_btt, sub=None):
        self.instrs, self.Z, bits, self.ret, self.hosts, self.first = read_trace(tdir)
        if sub is not None:
            # --instr a:b: instructions a .. b - 1 of the trace (its order), the DRAM image starts from the golden with only their outputs overwritten
            a, b = sub
            self.hosts = [(k - a, t) for k, t in self.hosts if a < k <= b]
            self.instrs, self.ret = self.instrs[a:b], self.ret[a:b]
            self.first = (1, 0, self.first[2])
        self.check, self.prefetch, self.odir = check, prefetch, odir
        init = open(os.path.join(pdir, "dram_init.bin"), "rb").read()
        gold = open(os.path.join(pdir, "dram_golden.bin"), "rb").read()
        first, last, nbytes = self.first
        if first == 0: base = bytearray(init)
        else:
            base = bytearray(gold)   # tb_has_npu_top --first: golden with the outputs of the window overwritten
            for (_, out, nb, _) in self.ret: base[out:out + nb] = b"\xA5" * nb
            for (_, txt) in self.hosts:
                h = parse_h(txt); base[h["out"]:h["out"] + h["c"] * 4 * h["h"] * h["w"]] = b"\xA5" * (h["c"] * 4 * h["h"] * h["w"])
        self.init, self.gold = base, bytearray(gold)
        self.const = bytearray()
        self.stg = (len(base) + 4095) & ~4095          # staging area of the CPU program (window build, write-back, max pool frames): 256 KiB
        self.cbase = self.stg + 0x40000
        self.cache = {}
        self.P = Prog(dram_base, max_btt)
        self.lut_loaded = None
        self.halves = {"A": [None, None], "B": [None, None], "C": [None, None]}   # [npu key, host key]
        self.starts = 0
        self.stats = {"calls": 0, "check_bad": 0, "check_calls": 0, "packets": 0, "wb_runs": 0, "a_runs": 0, "layers": 0, "elem_runs": 0}
        self.bit_otl = bits[2] if bits else None   # stream_en, cin_tile_last, oc_tile_last: bit numbers of the packed words (has_cpu_direct.h)
        if len(self.ret) != len(self.instrs): raise SystemExit("ret.txt has %d retired instructions, the trace %d" % (len(self.ret), len(self.instrs)))

    # constant area: content-addressed blocks after the program image
    def put_const(self, data, align=64):
        key = bytes(data)
        if key in self.cache: return self.cache[key]
        off = (len(self.const) + align - 1) & ~(align - 1)
        self.const += b"\x00" * (off - len(self.const)) + key
        self.cache[key] = self.cbase + off
        return self.cbase + off

    def rd(self, off, n): return bytes(self.gold[off:off + n]) if off < len(self.gold) else self.const_rd(off, n)
    def const_rd(self, off, n): return bytes(self.const[off - self.cbase:off - self.cbase + n])
    def u32(self, off): return struct.unpack_from("<I", self.gold, off)[0]

    # ---------------------------------------------------------------- GEMM_FUSED
    def a_segments(self, G, c):
        """SRAM A window of a call: (dram offset, sram offset, bytes) in-bounds runs, merged; and whether padding is present"""
        H, W, cin_t, part = G["in_h"], G["in_w"], c["cin_t"], c["part"]
        iy0, iy1, ix0, ix1 = c["iy0"], c["iy1"], c["ix0"], c["ix1"]
        ah, aw = iy1 - iy0, ix1 - ix0
        y0, y1, x0, x1 = max(0, iy0), min(H, iy1), max(0, ix0), min(W, ix1)
        segs = []
        for ci in range(cin_t):
            ch = part * cin_t + ci
            for y in range(y0, y1):
                segs.append((G["in"] + (ch * H + y) * W + x0, (ci * ah + (y - iy0)) * aw + (x0 - ix0), x1 - x0))
        padded = not (iy0 >= 0 and ix0 >= 0 and iy1 <= H and ix1 <= W)
        return merge_seg(segs), padded, cin_t * ah * aw

    def a_bytes(self, G, c):
        segs, padded, n = self.a_segments(G, c)
        b = bytearray([G["pad"] & 0xFF]) * n if padded else bytearray(n)
        for s, d, k in segs: b[d:d + k] = self.gold[s:s + k]
        return bytes(b)

    def c_image(self, G, c):
        nch, npos = c["xu"], (c["oy1"] - c["oy0"]) * (c["ox1"] - c["ox0"])
        nC = c["nC"]
        img = bytearray(nC)
        for x in range(nch):
            b = self.u32(G["bias"] + 4 * (c["c0"] + x)) if c["c0"] + x < G["cout"] else 0
            bb = struct.pack("<I", b)
            for p in range(npos):
                e = x * npos + p
                if 4 * e + 4 <= nC: img[4 * e:4 * e + 4] = bb
        return bytes(img)

    def b_src(self, G, c):
        K, Kt = G["K"], c["cin_t"] * G["kh"] * G["kw"]
        return G["w"] + c["c0"] * K + c["part"] * c["xu"] * Kt, c["xu"] * Kt

    def load_block(self, which, G, c):
        """DMA of the block of `which` for call c into the host half"""
        P = self.P
        if which == "A":
            segs, padded, n = self.a_segments(G, c)
            H, W = G["in_h"], G["in_w"]
            iy0, iy1, ix0, ix1 = c["iy0"], c["iy1"], c["ix0"], c["ix1"]
            y0, y1, x0, x1 = max(0, iy0), min(H, iy1), max(0, ix0), min(W, ix1)
            pads = (y0 - iy0, iy1 - y1, x0 - ix0, ix1 - x1)
            if P.use_opgen and len(segs) > 1 and y1 > y0 and x1 > x0 and max(pads) <= 255 and c["cin_t"] <= 65535:
                # the window with its padding built by one PAD2D descriptor in the staging area, then one command into the host half of SRAM A
                aw, ah = ix1 - ix0, iy1 - iy0
                src = G["in"] + (c["part"] * c["cin_t"] * H + y0) * W + x0
                P.used += [(s, k) for s, _, k in segs] + [(self.stg, n)]
                P.opgen(2, src, self.stg, 1, y1 - y0, c["cin_t"], x1 - x0, rs1=W, ws1=aw, rs2=H * W, ws2=ah * aw, padl=pads[2], padr=pads[3], full=aw,
                        pt=pads[0], pb=pads[1], fill=G["pad"] & 0xFF)
                P.dma(P.d(self.stg), BUS_A, n)
                self.stats["a_pad2d"] = self.stats.get("a_pad2d", 0) + 1
                self.stats["a_runs"] += len(segs)
                return
            if padded:
                if G["pad"] == 0: P.dma(0, BUS_A, n, zero=True)
                else: P.dma(P.d(self.put_const(bytes([G["pad"] & 0xFF]) * n)), BUS_A, n)
            for s, d, k in segs: P.dma(P.d(s), BUS_A + d, k)
            self.stats["a_runs"] += len(segs)
        elif which == "B":
            src, n = self.b_src(G, c)
            P.dma(P.d(src), BUS_B, n)
        else:
            img = self.c_image(G, c)
            P.dma(P.d(self.put_const(img)), BUS_C, len(img))

    def keys(self, G, c):
        a = ("A", G["seq"], c["part"], c["iy0"], c["iy1"], c["ix0"], c["ix1"])
        b = ("B", G["seq"], c["c0"], c["part"])
        cc = None if c["part"] > 0 else ("C", G["seq"], c["c0"], c["oy0"], c["oy1"], c["ox0"], c["ox1"], c["nC"])
        return {"A": a, "B": b, "C": cc}

    def plan_call(self, G, c):
        """decide keep / flip / load for each block; returns (keep bits, blocks to load into the host half before the START)"""
        need, keep, loads = self.keys(G, c), 0, []
        for x in "ABC":
            npu, host = self.halves[x]
            k = need[x]
            if x == "C" and k is None: keep |= KEEP["C"]; continue        # later Cin pass: accumulate on the partial sums the core holds
            if npu == k: keep |= KEEP[x]; continue
            if host != k: loads.append(x); self.halves[x][1] = k
        return keep, loads

    def flip(self, keep):
        for x in "ABC":
            if not keep & KEEP[x]: self.halves[x] = [self.halves[x][1], self.halves[x][0]]

    def start(self, words, keep, note):
        P = self.P
        q = 0
        for base, n in REGION:
            for j in range(n):
                P.w(SAURIA + base + 4 * j, words[q]); q += 1
        P.p(SAURIA, 0x8, 0x8)
        if self.starts:
            P.w(SAURIA, SOFT_RESET); P.p(SAURIA, 0x8, 0x8); P.n["soft"] += 1
        P.w(SAURIA, START | keep)
        P.n["starts"] += 1
        self.starts += 1

    def wait_core(self):
        self.P.p(SAURIA + 0x0C, 0x1, 0x1); self.P.w(SAURIA + 0x0C, 0x1)

    def gemm(self, ins):
        G, P = ins["G"], self.P
        if G is None or not ins["calls"]: raise SystemExit("instruction %d: GEMM_FUSED without layer / calls in the trace" % ins["seq"])
        for k in ("stream", "zpfit", "overlap", "inline"):
            if not G[k]: raise SystemExit("instruction %d: %s = 0: not expressible by the CPU path" % (ins["seq"], k))
        if G["feeder_pad"] or G["tmaj"] >= 0: raise SystemExit("instruction %d: feeder padding / token-major write-back not supported" % ins["seq"])
        self.stats["layers"] += 1
        calls = ins["calls"]
        for cl in calls:
            c = cl["c"]
            if c["pad"] or c["vert"]: raise SystemExit("instruction %d: a call with feeder padding / vertical scan" % ins["seq"])
            if (len(cl["pk"]) > 0) != bool(c["cin_last"]): raise SystemExit("instruction %d: packets only in the last Cin pass" % ins["seq"])
        if not calls[-1]["c"]["call_last"]: raise SystemExit("instruction %d: last call without call_last" % ins["seq"])
        # enables: tiles (Cin passes up to the one with packets) packed greedily, at most MAX_BEATS - 1 packets + the closing packet per enable
        tiles, cur = [], []
        for cl in calls:
            cur.append(cl)
            if cl["c"]["cin_last"]: tiles.append(cur); cur = []
        if cur: raise SystemExit("instruction %d: the trace ends inside the Cin passes of a tile" % ins["seq"])
        chunks, cur, npk = [], [], 0
        for t in tiles:
            n = sum(len(cl["pk"]) for cl in t)
            if n + 1 > MAX_BEATS: raise SystemExit("instruction %d: one tile has %d packets (> %d)" % (ins["seq"], n, MAX_BEATS - 1))
            if cur and npk + n + 1 > MAX_BEATS: chunks.append(cur); cur, npk = [], 0
            cur += t; npk += n
        if cur: chunks.append(cur)
        # activation table
        act = G["act"]
        lut = None
        if act == 1: lut = bytes(max(x - 128, 0) for x in range(256))
        elif act != 0: lut = self.rd(G["lut"], 256)
        P.c("---- instruction %d GEMM_FUSED: %d core calls, %d packets, %d enables" % (ins["seq"], len(calls), sum(len(c["pk"]) for c in calls), len(chunks)))
        if lut is not None and self.lut_loaded is not None and self.lut_loaded != lut:
            P.w(GVU + 0x08, 0x1)          # another activation table: reset the table unit first
        if lut is not None: self.lut_loaded = lut
        lut_dram = self.put_const(lut) if lut is not None else None
        flat = [cl for ch in chunks for cl in ch]
        # plan of the halves for every call (sequential), the loads of call i go before the START of call i or, prefetched, after the START of call i - 1
        plan = []
        for cl in flat:
            keep, loads = self.plan_call(G, cl["c"])
            plan.append((keep, loads))
            self.flip(keep)
        first_loads_done = False
        idx = 0
        zp = G["zp"]
        for ch in chunks:
            # sbank image of the enable: entry k = k-th packet; the closing packet gets scale 0, shift 0
            scales, shifts = bytearray(), bytearray()
            for cl in ch:
                c = cl["c"]
                for (_, _, chn) in cl["pk"]:
                    real = c["c0"] + chn < G["cout"]
                    S = self.u32(G["scale"] + 4 * (c["c0"] + chn)) if (G["per_channel"] and real) else G["bS"] & 0xFFFFFFFF
                    s = self.u32(G["shift"] + 4 * (c["c0"] + chn)) if (G["per_channel"] and real) else G["bs"]
                    scales += struct.pack("<I", S); shifts.append(s & 0xFF)
            scales += struct.pack("<I", 0); shifts.append(0)
            npk = len(shifts)
            P.n["enables"] += 1
            if lut is not None: P.dma(P.d(lut_dram), BUS_V1, 256)
            P.dma(P.d(self.put_const(bytes(scales))), BUS_SB, len(scales))
            P.dma(P.d(self.put_const(bytes(shifts))), BUS_SB + SHIFT_BASE, len(shifts))
            P.w(GVU + 0x0C, ((zp & 0xFF) << 8) | (0 if lut is not None else 1))
            P.w(GVU + 0x04, OP_GEMM_FUSED)
            P.w(GVU, 0x1)
            rows = []   # vbank2 row -> (call, packet)
            for j, cl in enumerate(ch):
                c = cl["c"]
                keep, loads = plan[idx]
                if not self.prefetch or idx == 0 or not first_loads_done:
                    for x in loads: self.load_block(x, G, c)
                first_loads_done = True
                words = list(cl["w"])
                words[self.bit_otl // 32] &= ~(1 << (self.bit_otl % 32))   # the closing call ends the enable
                P.c("call %d: c0 %d, rows %d..%d, cols %d..%d, part %d, keep 0x%x, %d packets" % (c["s"], c["c0"], c["oy0"], c["oy1"], c["ox0"], c["ox1"], c["part"], keep, len(cl["pk"])))
                self.start(words, keep, "")
                self.stats["calls"] += 1
                if self.check: self.check_call(G, c)
                # prefetch of the next call of this layer while the core runs
                if self.prefetch and idx + 1 < len(flat):
                    for x in plan[idx + 1][1]: self.load_block(x, G, flat[idx + 1]["c"])
                self.wait_core()
                for k in range(len(cl["pk"])): rows.append((cl, k))
                idx += 1
            # closing call: one context, one packet, keeps every half
            P.c("closing call of the enable (one context, oc_tile_last)")
            self.start(self.Z, KEEP["A"] | KEEP["B"] | KEEP["C"], "")
            self.wait_core()
            P.n["close_calls"] += 1
            P.p(GVU, 0x2, 0x2); P.w(GVU, 0xC)
            if len(rows) + 1 > MAX_ROWS: raise SystemExit("vbank2 rows %d" % len(rows))
            self.writeback(G, rows)
            self.stats["packets"] += len(rows)
        if not self.prefetch: pass

    def writeback(self, G, rows):
        P = self.P
        OH, OW, cout, out = G["oh"], G["ow"], G["cout"], G["out"]
        segs = []
        for r, (cl, k) in enumerate(rows):
            c = cl["c"]
            addr, mask, chn = cl["pk"][k]
            wt = c["ox1"] - c["ox0"]; npos = (c["oy1"] - c["oy0"]) * wt
            e0 = (addr % 1536) * 32
            for l in range(32):
                if not (mask >> l) & 1: continue
                p = e0 + l - chn * npos
                oy, ox, cc = c["oy0"] + p // wt, c["ox0"] + p % wt, c["c0"] + chn
                if cc >= cout or ox >= OW: continue
                segs.append((32 * r + l, out + (cc * OH + oy) * OW + ox, 1))
        runs = merge_seg(segs)
        self.stats["wb_runs"] += len(runs)
        self.v2_out(runs, 32 * len(rows))

    def v2_out(self, runs, nbytes):
        """runs (vbank2 byte, dram offset, len): with opgen, vbank2 to the staging area in one command, then COPY descriptors to the tensor"""
        P = self.P
        if not P.use_opgen or len(runs) <= 2:
            for s, d, n in runs: P.dma(BUS_V2 + s, P.d(d), n)
            return
        P.dma(BUS_V2, P.d(self.stg), nbytes)
        P.copy_runs([(self.stg + s, d, n) for s, d, n in runs])

    def check_call(self, G, c):
        self.stats["check_calls"] += 1
        a = self.a_bytes(G, c)
        src, n = self.b_src(G, c)
        b = self.rd(src, n)
        bad = []
        if len(a) == c["nA"] and fnv(a) != c["hA"]: bad.append("A")
        if n == c["nB"] and fnv(b) != c["hB"]: bad.append("B")
        if c["part"] == 0 and fnv(self.c_image(G, c)) != c["hC"]: bad.append("C")
        if len(a) != c["nA"] or n != c["nB"]: bad.append("size")
        if bad:
            self.stats["check_bad"] += 1
            if self.stats["check_bad"] <= 10: print("check: call s=%d of layer %d differs in %s" % (c["s"], G["seq"], ",".join(bad)))

    # ---------------------------------------------------------------- element-wise
    def elem(self, ins):
        E, P = ins["E"], self.P
        mode = E["mode"]
        if mode == 7:
            h, w = E["h"], E["w"]
            if E["axis"] != 0: raise SystemExit("instruction %d: concat on axis %d not supported" % (ins["seq"], E["axis"]))
            P.c("---- instruction %d DMA_CONCAT (DRAM to DRAM)" % ins["seq"])
            if E["b"] != 0:
                na = E["k"] * h * w
                P.dma(P.d(E["a"]), P.d(E["out"]), na)
                P.dma(P.d(E["b"]), P.d(E["out"] + na), max(1, E["c"] - E["k"]) * h * w)
            else:
                P.dma(P.d(E["a"]), P.d(E["out"] + E["p"] * h * w), E["k"] * h * w)
            return
        if mode == 0:
            hw = rtl_add_map(E)
            if hw is not None and not self.cpu_add:
                # RESIDUAL of the vector unit (RTL revision 2): out = clamp8(clamp[-256, 255]((a sa + b sb + bias) >> sh) + z), the half of the
                # rounding in the bias (gvu_top_stream.h rtl_add_map, round half up); runs of ADD_ROWS rows of 32 lanes
                sa, sb, bias, sh, z = hw
                n = E["n"]; rows_total = (n + 31) // 32
                P.c("---- instruction %d ADD as RESIDUAL of the vector unit: sa %d sb %d bias %d sh %d z %d, %d rows" % (ins["seq"], sa, sb, bias, sh, z, rows_total))
                for r0 in range(0, rows_total, ADD_ROWS):
                    rows = min(ADD_ROWS, rows_total - r0)
                    o = r0 * 32; nb = min(rows * 32, n - o)
                    P.dma(P.d(E["a"] + o), BUS_V0, nb)
                    P.dma(P.d(E["b"] + o), BUS_V1, nb)
                    r7 = (sh << 24) | ((z & 0xFF) << 16) | rows
                    self.gvu_run([(0x04, OP_RESIDUAL), (0x1C, r7), (0x20, sa & 0xFFFFFFFF), (0x24, sb & 0xFFFFFFFF), (0x28, bias & 0xFFFFFFFF),
                                  (0x2C, (bias >> 32) & 0xFFFFFFFF), (0x00, 1)])
                    P.dma(BUS_V2, P.d(E["out"] + o), nb)
                    self.stats["elem_runs"] += 1
                return
            if True:
                # the two-stage add of the model (dequant A, dequant B, saturating sum, requant) is not the one-rounding formula of the RESIDUAL registers
                # of the RTL for these parameters: the CPU computes it (host step of tb_has_npu_top, same formula and knobs)
                P.c("---- instruction %d ADD computed by the CPU (parameters not expressible by the RESIDUAL registers)" % ins["seq"])
                P.L.append("H add2 a=0x%x b=0x%x out=0x%x n=%d zpA=%d zpB=%d zpO=%d SA=%d SB=%d SO=%d sA=%d sB=%d sO=%d"
                           % (E["a"], E["b"], E["out"], E["n"], E["zpA"], E["zpB"], E["zpO"], E["SA"], E["SB"], E["SO"], E["sA"], E["sB"], E["sO"]))
                P.n["H"] += 1; self.stats["cpu_add"] = self.stats.get("cpu_add", 0) + 1
                return
            P.c("---- instruction %d ADD (RESIDUAL of the vector unit), %d runs" % (ins["seq"], len(ins["runs"])))
            for r in ins["runs"]:
                q = r["p"]
                o = q["r0"] * 32; nb = min(q["rows"] * 32, q["n"] - o)
                P.dma(P.d(E["a"] + o), BUS_V0, nb)
                P.dma(P.d(E["b"] + o), BUS_V1, nb)
                self.gvu_run(r["g"])
                P.dma(BUS_V2, P.d(E["out"] + o), nb)
                self.stats["elem_runs"] += 1
            return
        if mode == 1:
            P.c("---- instruction %d MAX_POOL (vector unit), %d runs" % (ins["seq"], len(ins["runs"])))
            # the model hands max pool to the unit in groups of channels (HasElemwise::maxpool: as many as its scratchpad holds), each a job of runs
            # whose c0 counts inside the group: a run with c0 = 0 opens the next group, its first channel = the channels of the groups before
            gbase, gprev, nchan = 0, None, 0
            for r in ins["runs"]:
                if r["kind"] != "pool": raise SystemExit("instruction %d: pool run kind %s" % (ins["seq"], r["kind"]))
                q = r["p"]
                if q["c0"] == 0 and gprev is not None: gbase += gprev
                gprev = q["g"]
                c0, nc, H, W, p, Hp, Wt, Ho, Wo, s, kk, OL = (q[k] for k in ("c0", "nc", "H", "W", "p", "Hp", "Wt", "Ho", "Wo", "s", "kk", "OL"))
                c0 += gbase
                nchan += nc
                if c0 + nc > E["c"]: raise SystemExit("instruction %d: pool runs pass the %d channels" % (ins["seq"], E["c"]))
                Wp = W + 2 * p
                if P.use_opgen and Wt * 32 - W - p <= 255 and p <= 255:
                    # padded input rows (-128 around, and in the lanes after the padded row, which no valid output reads) by one PAD2D descriptor
                    nb = nc * Hp * Wt * 32
                    P.used += [(E["a"] + c0 * H * W, nc * H * W), (self.stg, nb)]
                    P.opgen(2, E["a"] + c0 * H * W, self.stg, 1, H, nc, W, rs1=W, ws1=Wt * 32, rs2=H * W, ws2=Hp * Wt * 32, padl=p, padr=Wt * 32 - W - p,
                            full=Wt * 32, pt=p, pb=p, fill=0x80)
                    P.dma(P.d(self.stg), BUS_V0, nb)
                    self.gvu_run(r["g"])
                    wpr, lanes = ((Wt + s - 1) // s, 32) if s in (2, 4) else (Wt, OL)
                    orun = merge_seg([((((cc * Ho + oh) * wpr + ow // lanes) * 32 + ow % lanes), E["out"] + ((c0 + cc) * Ho + oh) * Wo + ow, 1)
                                      for cc in range(nc) for oh in range(Ho) for ow in range(Wo)])
                    self.v2_out(orun, (max(s_ + n_ for s_, _, n_ in orun) + 31) & ~31)
                    self.stats["elem_runs"] += 1
                    continue
                frame = bytearray(nc * Hp * Wt * 32)
                for cc in range(nc):
                    for y in range(Hp):
                        row = (cc * Hp + y) * Wt * 32
                        for x in range(Wp):
                            if y < p or y >= H + p or x < p or x >= W + p: frame[row + x] = 0x80
                P.dma(P.d(self.put_const(bytes(frame))), BUS_V0, len(frame))
                segs = [(E["a"] + ((c0 + cc) * H + y) * W, ((cc * Hp + y + p) * Wt) * 32 + p, W) for cc in range(nc) for y in range(H)]
                for sa, da, n in merge_seg(segs): P.dma(P.d(sa), BUS_V0 + da, n)
                self.gvu_run(r["g"])
                wpr, lanes = ((Wt + s - 1) // s, 32) if s in (2, 4) else (Wt, OL)
                osegs = []
                for cc in range(nc):
                    for oh in range(Ho):
                        for ow in range(Wo):
                            row = (cc * Ho + oh) * wpr + ow // lanes
                            osegs.append((row * 32 + ow % lanes, E["out"] + ((c0 + cc) * Ho + oh) * Wo + ow, 1))
                orun = merge_seg(osegs)
                self.v2_out(orun, (max(s + n for s, _, n in orun) + 31) & ~31)
                self.stats["elem_runs"] += 1
            return
        raise SystemExit("instruction %d: ELEM_WISE mode %d not supported" % (ins["seq"], mode))

    def gvu_run(self, g):
        P = self.P
        for off, v in g:
            if off < 0x38: P.w(GVU + off, v)        # the 14 registers of the RTL; the proposal registers of the model (AGU_ND) do not exist there
        P.p(GVU, 0x2, 0x2); P.w(GVU, 0xC)
        P.n["gvu_runs"] += 1

    # ---------------------------------------------------------------- program
    def run(self):
        P = self.P
        P.c("whole-network program for the CPU path, written by mk_cpu_net.py from the trace of the data flow controller")
        P.w(DMA + 0x04, 0x3)
        P.w(SAURIA + 0x04, 0x1); P.w(SAURIA + 0x08, 0x1)
        byseq = {}
        hosts = list(self.hosts)
        nret = 0
        retq = list(self.ret)
        for ins in self.instrs:
            if not ins["done"]: raise SystemExit("instruction %d not finished in the trace" % ins["seq"])
            if ins["op"] == 0x12: self.gemm(ins)
            elif ins["op"] == 0x15: self.elem(ins)
            else: raise SystemExit("instruction %d: opcode 0x%x not supported" % (ins["seq"], ins["op"]))
            # the output check of the instruction, then the host steps that follow it
            if retq:
                idx, out, nb, label = retq.pop(0)
                P.L.append("K %x %d" % (out, nb)); P.n["K"] += 1
                self.gold_region(out, nb)
                nret += 1
            while hosts and hosts[0][0] <= nret:
                P.L.append(hosts.pop(0)[1]); P.n["H"] += 1
        while hosts: P.L.append(hosts.pop(0)[1]); P.n["H"] += 1
        # images
        init = bytearray(self.init) + b"\x00" * (self.cbase - len(self.init)) + self.const
        gold = bytearray(self.gold) + b"\x00" * (self.cbase - len(self.gold)) + self.const
        if self.compact: init, gold = self.compact_images(init, gold)
        os.makedirs(self.odir, exist_ok=True)
        open(os.path.join(self.odir, "dram_init.bin"), "wb").write(init)
        open(os.path.join(self.odir, "dram_golden.bin"), "wb").write(gold)
        open(os.path.join(self.odir, "mmio.txt"), "w", newline="\n").write("\n".join(P.L) + "\n")
        n = P.n
        print("instructions %d layers %d calls %d enables %d closing_calls %d packets %d starts %d soft_resets %d | mmio_writes %d polls %d dma_cmds %d (zero %d) "
              "dma_bytes %d opgen %d a_runs %d wb_runs %d gvu_runs %d K %d H %d | const_bytes %d dram_bytes %d | check calls %d bad %d"
              % (len(self.instrs), self.stats["layers"], self.stats["calls"], n["enables"], n["close_calls"], self.stats["packets"], n["starts"], n["soft"],
                 n["W"], n["P"], n["dma"], n["dma_zero"], n["dma_bytes"], n.get("opgen", 0), self.stats["a_runs"], self.stats["wb_runs"], n["gvu_runs"], n["K"], n["H"],
                 len(self.const), len(init), self.stats["check_calls"], self.stats["check_bad"]))

    def gold_region(self, out, nb): pass

    # --compact: only the DRAM regions the program touches, packed one after the other from offset 0 (for a bench with a small memory); the
    # addresses of the DMA commands, K lines and host steps are rewritten
    def compact_images(self, init, gold):
        P = self.P
        used = []
        for line in P.L:
            t = line.split()
            if t[0] == "K": used.append((int(t[1], 16), int(t[2])))
            elif t[0] == "H" and t[1] == "add2":
                h = parse_h(line); used += [(h["a"], h["n"]), (h["b"], h["n"]), (h["out"], h["n"])]
            elif t[0] == "H":
                h = parse_h(line); n = h["c"] * h["h"] * h["w"]
                used += [(h["in"], n), (h["out"], 4 * n)]
        used += P.used
        used.sort()
        rng = []
        for o, n in used:
            o0, o1 = o & ~63, (o + n + 63) & ~63
            if rng and o0 <= rng[-1][1]: rng[-1][1] = max(rng[-1][1], o1)
            else: rng.append([o0, o1])
        starts, new, top = [r[0] for r in rng], [], 0
        for a, b in rng: new.append(top); top += b - a
        import bisect

        def m(o):
            i = bisect.bisect_right(starts, o) - 1
            if i < 0 or o >= rng[i][1]: raise SystemExit("compact: offset 0x%x outside the used regions" % o)
            return new[i] + o - rng[i][0]
        L = []
        for line in P.L:
            t = line.split()
            if t[0] == "W" and int(t[1], 16) in (DMA + 0x10, DMA + 0x20, DMA + 0x88, DMA + 0x8C) and int(t[2], 16) >= P.base:
                line = "W %s 0x%08x" % (t[1], P.base + m(int(t[2], 16) - P.base))
            elif t[0] == "K": line = "K %x %s" % (m(int(t[1], 16)), t[2])
            elif t[0] == "H" and t[1] == "add2":
                h = parse_h(line)
                for k in ("a", "b", "out"): h[k] = m(h[k])
                line = "H add2 " + " ".join("%s=%s" % (k, ("0x%x" % h[k]) if k in ("a", "b", "out") else h[k]) for k in
                                            ("a", "b", "out", "n", "zpA", "zpB", "zpO", "SA", "SB", "SO", "sA", "sB", "sO"))
            elif t[0] == "H":
                h = parse_h(line)
                line = "H upsample2x in=0x%x out=0x%x c=%d h=%d w=%d" % (m(h["in"]), m(h["out"]), h["c"], h["h"], h["w"])
            L.append(line)
        P.L = L
        ci, cg = bytearray(top), bytearray(top)
        for (a, b), o in zip(rng, new):
            ci[o:o + b - a] = bytes(init[a:b]).ljust(b - a, b"\0"); cg[o:o + b - a] = bytes(gold[a:b]).ljust(b - a, b"\0")   # the last range may pass the image end
        print("compact: %d regions, %d bytes (from %d)" % (len(rng), top, len(init)))
        return ci, cg


def main():
    a = sys.argv[1:]
    if len(a) < 3: raise SystemExit(__doc__)
    opt = a[3:]
    base = int(opt[opt.index("--dram-base") + 1], 0) if "--dram-base" in opt else 0x80000000
    mb = int(opt[opt.index("--max-btt") + 1], 0) if "--max-btt" in opt else 65536
    sub = tuple(int(x) for x in opt[opt.index("--instr") + 1].split(":")) if "--instr" in opt else None
    g = Gen(a[0], a[1], a[2], base, "--check" in opt, "--no-prefetch" not in opt, mb, sub)
    g.compact = "--compact" in opt
    g.P.use_opgen = "--no-opgen" not in opt
    g.cpu_add = "--cpu-add" in opt   # every ADD by the CPU (host step H add2) instead of the RESIDUAL of the vector unit
    g.run()


if __name__ == "__main__":
    main()
