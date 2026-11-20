#!/usr/bin/env python3
"""Several layers in a row driven by the CPU (HAS_CPU_DIRECT=1) for tools/has/tb_has_npu_top.cpp: the data of a layer comes from the result that the
CPU itself wrote to DRAM in the layer before, as firmware for a whole network does.

  gate <out dir> [--seed 7]
      Program of the data flow controller of the model with two GEMM_FUSED layers (1 x 1 convolutions on a row of 32 positions): layer 0 takes 24
      input channels to 64, layer 1 takes the 64 channels of layer 0 to 64, each layer with its own weights, bias, scales, shifts and activation
      table. Run it with HAS_CPU_DIRECT_DUMP=<dump dir>: the model writes both layers (gemm_L0, gemm_L1) and their results.
  cpu <dump dir> <out dir>
      The CPU program of the chain:
        layer 0  GEMM_FUSED (2 core calls), input, weights and partial-sum preload from DRAM, result rows of vbank2 to DRAM (out0);
        layer 1  GEMM_FUSED (2 core calls), INPUT = out0 as the CPU wrote it (64 rows of 32 positions = the channel-major layout that SRAM A takes), its own
                 activation table after a reset of the table, its own scales and shifts in the sbank, a soft reset before every START after the first;
                 result to DRAM (out1);
        RESIDUAL out0 + out1 (formula of the RTL, revision 2) to DRAM (res).
      Golden: out0 and out1 are the results of the data flow controller of the model for the same layers (beats of the dump), res is the formula of
      gvu_top.sv in Python on those two. Three checks ("K" lines).

usage: mk_cpu_chain.py gate <out dir> [--seed 7]
       mk_cpu_chain.py cpu <dump dir> <out dir> [--dram-base 0x80000000]
"""
import os
import random
import struct
import sys

# data flow controller program (instruction registers of the host window, as mk_gemm_gate.py)
B = 0x40000000
R = {"in": 0x400, "w": 0x404, "out": 0x408, "bias": 0x40C, "kh": 0x41C, "kw": 0x420, "stride": 0x424, "pad": 0x428, "act": 0x42C, "has_skip": 0x430}
X = ["IN_C", "IN_H", "IN_W", "OUT_C", "OUT_H", "OUT_W", "TILE_COUT", "TILE_H", "TILE_W", "SCALE_ADDR", "SHIFT_ADDR", "LUT_ADDR", "ZP_OUT", "FLAGS"]
W_POS, CIN0, C = 32, 24, 64

# CPU-direct blocks (has/has_cpu_direct.h)
SAURIA, GVU, DMA = 0x11000, 0x12000, 0x13000
REGION = [(0x200, 2), (0x400, 17), (0x600, 5), (0x800, 5)]
BUS_A, BUS_B, BUS_C = 0x00000000, 0x00040000, 0x00080000
BUS_VBANK0, BUS_VBANK1, BUS_VBANK2, BUS_SBANK = 0x00100000, 0x00120000, 0x00140000, 0x00180000
SHIFT_BASE, MAX_BEATS, OP_GEMM_FUSED, OP_RESIDUAL = 2456, 614, 2, 6
M64 = (1 << 64) - 1


def gate(out, seed):
    rng = random.Random(seed)
    ri = lambda lo, hi, n: [rng.randint(lo, hi) for _ in range(n)]
    img = bytearray(256 * 1024)
    top = [0x1000]

    def alloc(n):
        p = top[0]
        top[0] = (p + n + 63) & ~63
        return p

    def put(vals, dt):
        raw = b"".join(struct.pack("<i", v) for v in vals) if dt == "i4" else bytes(v & 0xFF for v in vals)
        p = alloc(len(raw))
        img[p:p + len(raw)] = raw
        return p

    x0 = put(ri(-100, 100, CIN0 * W_POS), "i1")
    lines = ["# mk_cpu_chain.py gate: two GEMM_FUSED 1x1 layers, W %d, %d -> %d -> %d channels" % (W_POS, CIN0, C, C)]
    src = x0
    for k, (cin, tab) in enumerate(((CIN0, (0.9, -5)), (C, (0.7, 4)))):
        pw = alloc(C * cin)
        for c0 in range(0, C, 32):
            blk = bytes(v & 0xFF for v in ri(-20, 20, cin * 32))
            img[pw + c0 * cin:pw + c0 * cin + len(blk)] = blk
        dst = alloc(C * W_POS)
        regs = {"in": src, "w": pw, "out": dst, "bias": put(ri(-800, 800, C), "i4"), "kh": 1, "kw": 1, "stride": 1, "pad": 0, "act": 2, "has_skip": 0,
                "IN_C": cin, "IN_H": 1, "IN_W": W_POS, "OUT_C": C, "OUT_H": 1, "OUT_W": W_POS, "TILE_COUT": 32, "TILE_H": 1, "TILE_W": W_POS,
                "SCALE_ADDR": put(ri(1 << 29, 1 << 30, C), "i4"), "SHIFT_ADDR": put(ri(38, 40, C), "i4"),
                "LUT_ADDR": put([max(-128, min(127, int(round(x * tab[0] + tab[1])))) for x in range(-128, 128)], "i1"), "ZP_OUT": 3 - 5 * k, "FLAGS": 0}
        lines.append("# [%d] GEMM_FUSED" % k)
        for name, v in regs.items():
            lines.append("W %08x %x" % (B + (R[name] if name in R else 0x46C + 4 * X.index(name)), v & 0xFFFFFFFF))
        lines.append("W 40000310 12")
        src = dst
    os.makedirs(out, exist_ok=True)
    open(os.path.join(out, "dram_init.bin"), "wb").write(img)
    open(os.path.join(out, "dram_golden.bin"), "wb").write(img)   # no reference image here: the dump of the run is what is wanted
    open(os.path.join(out, "mmio.txt"), "w", newline="\n").write("\n".join(lines) + "\n")
    print("wrote", out)


def read_layer(d):
    kv, calls = {}, []
    for l in open(os.path.join(d, "layer.txt")):
        t = l.split()
        if not t: continue
        if t[0] == "call": calls.append({t[i]: int(t[i + 1]) for i in range(2, len(t) - 1, 2)} | {"i": int(t[1])})
        else: kv[t[0]] = int(t[1])
    return kv, calls


def load_layer(d):
    kv, calls = read_layer(d)
    rd = lambda n: open(os.path.join(d, n), "rb").read()
    if kv.get("unfit") or kv.get("bypassed"): raise SystemExit("%s: not expressible by the CPU path" % d)
    beats = [l.split() for l in open(os.path.join(d, "beats.txt")) if l.strip()]
    nb = len(beats)
    if nb != kv["beats"] or nb != kv["scale_entries"] or nb > MAX_BEATS: raise SystemExit("%s: beats %d" % (d, nb))
    if len(calls) != kv["calls"] or not calls[-1]["oc_tile_last"] or any(c["oc_tile_last"] for c in calls[:-1]) or calls[-1]["ncontexts"] != 1:
        raise SystemExit("%s: oc_tile_last in the last call only, one context in the last call" % d)
    if any(int(b[2], 16) != 0xFFFFFFFF for b in beats): raise SystemExit("%s: every beat must have 32 lanes" % d)
    res = bytes(int(b[3 + l], 16) for b in beats for l in range(32))
    cd = []
    for c in calls:
        blobs = [rd("call%03d_%s.bin" % (c["i"], k)) for k in "abc"]
        words = [int(l.split()[1], 16) for l in open(os.path.join(d, "call%03d_words.txt" % c["i"])) if l.startswith("word")]
        assert len(words) == 29
        cd.append((blobs, words))
    sb = rd("sbank.bin")
    return {"kv": kv, "nb": nb, "res": res, "calls": cd, "lut": rd("lut.bin") if kv["layer_act"] else bytes(256),
            "scale": sb[0:4 * nb], "shift": sb[SHIFT_BASE:SHIFT_BASE + nb]}


def lane(a, b, sa, sb, bias, sh, z):
    s = (((a * sa) & M64) + ((b * sb) & M64) + bias) & M64
    if s >> 63: s -= 1 << 64
    return max(-128, min(127, max(-256, min(255, s >> sh)) + z))


def cpu(dump, out, dram_base):
    L0, L1 = load_layer(os.path.join(dump, "gemm_L0")), load_layer(os.path.join(dump, "gemm_L1"))
    n_in1 = len(L0["res"])
    for blobs, _ in L1["calls"]:   # layer 1 reads what layer 0 wrote: same bytes, nothing else in SRAM A
        if blobs[0][:n_in1] != L0["res"] or any(blobs[0][n_in1:]):
            raise SystemExit("SRAM A of layer 1 is not the result of layer 0 in rows of 32 positions")
    al = lambda x: (x + 255) & ~255
    off, ptr = {}, 0x1000

    def put(key, size):
        nonlocal ptr
        off[key] = ptr; ptr = al(ptr + size)

    for k, Ly in enumerate((L0, L1)):
        put("lut%d" % k, 256); put("scale%d" % k, 4 * Ly["nb"]); put("shift%d" % k, Ly["nb"])
        for i, (blobs, _) in enumerate(Ly["calls"]):
            for j, kk in enumerate("abc"):
                if k == 1 and kk == "a": continue   # the input of layer 1 is out0
                put("L%dc%d%s" % (k, i, kk), len(blobs[j]))
    put("out0", n_in1); put("out1", len(L1["res"])); put("res", n_in1)
    init = bytearray(ptr + 0x1000)
    for k, Ly in enumerate((L0, L1)):
        init[off["lut%d" % k]:off["lut%d" % k] + 256] = Ly["lut"]
        init[off["scale%d" % k]:off["scale%d" % k] + 4 * Ly["nb"]] = Ly["scale"]
        init[off["shift%d" % k]:off["shift%d" % k] + Ly["nb"]] = Ly["shift"]
        for i, (blobs, _) in enumerate(Ly["calls"]):
            for j, kk in enumerate("abc"):
                key = "L%dc%d%s" % (k, i, kk)
                if key in off: init[off[key]:off[key] + len(blobs[j])] = blobs[j]
    for key in ("out0", "out1", "res"):
        for i in range(off[key], off[key] + n_in1): init[i] = 0xA5
    # RESIDUAL out0 + out1 (signed int8 rows), parameters chosen so that both operands count
    sa, sb, sh, z = 20000, 15000, 16, 2
    bias = (1 << (sh - 1))
    s8 = lambda v: v - 256 if v > 127 else v
    res = bytes(lane(s8(a), s8(b), sa, sb, bias, sh, z) & 0xFF for a, b in zip(L0["res"], L1["res"]))
    gold = bytearray(init)
    gold[off["out0"]:off["out0"] + n_in1] = L0["res"]
    gold[off["out1"]:off["out1"] + n_in1] = L1["res"]
    gold[off["res"]:off["res"] + n_in1] = res

    D = lambda key: dram_base + off[key]
    P = ["# chain driven by the CPU: GEMM_FUSED %d -> %d channels, GEMM_FUSED on its result, RESIDUAL of the two results" % (CIN0, C),
         "W 0x%08x 0x3" % (DMA + 0x04), "W 0x%08x 0x1" % (SAURIA + 0x04), "W 0x%08x 0x1" % (SAURIA + 0x08)]

    def dma(r, w, n):
        P.extend(["W 0x%08x 0x%08x" % (DMA + 0x10, r), "W 0x%08x 0x%08x" % (DMA + 0x20, w), "W 0x%08x 0x%08x" % (DMA + 0x30, n),
                  "W 0x%08x 0x3" % DMA, "P 0x%08x 0x2 0x2" % (DMA + 0x0C), "W 0x%08x 0x3" % (DMA + 0x0C)])

    starts = 0
    for k, Ly in enumerate((L0, L1)):
        P.append("# ---- layer %d: %d core calls, %d beats" % (k, len(Ly["calls"]), Ly["nb"]))
        if k: P.append("W 0x%08x 0x1" % (GVU + 0x08))   # another activation table: reset the table unit first
        dma(D("lut%d" % k), BUS_VBANK1, 256)
        dma(D("scale%d" % k), BUS_SBANK, 4 * Ly["nb"])
        dma(D("shift%d" % k), BUS_SBANK + SHIFT_BASE, Ly["nb"])
        zp, act = Ly["kv"]["layer_zp"], Ly["kv"]["layer_act"]
        P += ["W 0x%08x 0x%08x" % (GVU + 0x0C, ((zp & 0xFF) << 8) | (0 if act else 1)), "W 0x%08x 0x%08x" % (GVU + 0x04, OP_GEMM_FUSED), "W 0x%08x 0x1" % GVU]
        for i, (blobs, words) in enumerate(Ly["calls"]):
            P.append("# layer %d, core call %d" % (k, i))
            dma(D("out0") if k else D("L0c%da" % i), BUS_A, n_in1 if k else len(blobs[0]))
            dma(D("L%dc%db" % (k, i)), BUS_B, len(blobs[1]))
            dma(D("L%dc%dc" % (k, i)), BUS_C, len(blobs[2]))
            q = 0
            for base, n in REGION:
                for j in range(n):
                    P.append("W 0x%08x 0x%08x" % (SAURIA + base + 4 * j, words[q])); q += 1
            P.append("P 0x%08x 0x8 0x8" % SAURIA)
            if starts: P += ["W 0x%08x 0x800000" % SAURIA, "P 0x%08x 0x8 0x8" % SAURIA]
            P += ["W 0x%08x 0x1" % SAURIA, "P 0x%08x 0x1 0x1" % (SAURIA + 0x0C), "W 0x%08x 0x1" % (SAURIA + 0x0C)]
            starts += 1
        P += ["P 0x%08x 0x2 0x2" % GVU, "W 0x%08x 0xc" % GVU]
        dma(BUS_VBANK2, D("out%d" % k), n_in1)
    rows = n_in1 // 32
    P.append("# ---- RESIDUAL out0 + out1, %d rows" % rows)
    dma(D("out0"), BUS_VBANK0, n_in1)
    dma(D("out1"), BUS_VBANK1, n_in1)
    r7 = (sh << 24) | ((z & 0xFF) << 16) | rows
    for o, v in ((0x04, OP_RESIDUAL), (0x1C, r7), (0x20, sa), (0x24, sb), (0x28, bias & 0xFFFFFFFF), (0x2C, (bias >> 32) & 0xFFFFFFFF), (0x00, 1)):
        P.append("W 0x%08x 0x%08x" % (GVU + o, v))
    P += ["P 0x%08x 0x2 0x2" % GVU, "W 0x%08x 0xc" % GVU]
    dma(BUS_VBANK2, D("res"), n_in1)
    P += ["K %x %d" % (off[key], n_in1) for key in ("out0", "out1", "res")]
    os.makedirs(out, exist_ok=True)
    open(os.path.join(out, "dram_init.bin"), "wb").write(init)
    open(os.path.join(out, "dram_golden.bin"), "wb").write(gold)
    open(os.path.join(out, "mmio.txt"), "w", newline="\n").write("\n".join(P) + "\n")
    print("layers 2 starts %d beats %d+%d residual_rows %d bytes_checked %d mmio_lines %d"
          % (starts, L0["nb"], L1["nb"], rows, 3 * n_in1, sum(1 for l in P if l[0] in "WP")))


def main():
    a = sys.argv[1:]
    if len(a) >= 2 and a[0] == "gate":
        gate(a[1], int(a[3]) if len(a) > 3 and a[2] == "--seed" else 7)
    elif len(a) >= 3 and a[0] == "cpu":
        cpu(a[1], a[2], int(a[4], 0) if len(a) > 4 and a[3] == "--dram-base" else 0x80000000)
    else:
        raise SystemExit(__doc__)


if __name__ == "__main__":
    main()
