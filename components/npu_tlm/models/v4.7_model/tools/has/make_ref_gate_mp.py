#!/usr/bin/env python3
"""Check program for the instruction forms that the network programs of this package do not use.

Writes a program directory (mmio.txt, dram_init.bin) with small instructions on random data:
  0  GEMM_FUSED 3 x 3 with extension registers but without SCALE_ADDR (one scale from the real-valued registers), ReLU
  1  GEMM_FUSED 1 x 1 in the residual form (HAS_SKIP = 1, SKIP_ADDR)
  2  GEMM_FUSED without extension registers (M positions x K inputs -> N outputs, default tiling, real-valued scale)
  3  ELEM_WISE ADD with real-valued scales (no integer parameters)
  4  ELEM_WISE ADD with integer parameters
  5  GEMM_FUSED 1 x 1 with a table activation, per-channel scales and a last channel range shorter than TILE_COUT
  6, 7  ELEM_WISE MAX_POOL 5 x 5 stride 1 padding 2, twice in a row (as the SPPF block)
  8  ELEM_WISE MAX_POOL 3 x 3 stride 1 padding 1
With --bus-base B a second directory <out>_bus is written whose address registers hold B + offset.
No reference image is written: tools/has/ref_program.py computes one (--write-ref), the model produces its own
(FE_SNAPSHOT_DIR of tools/has/tb_has_npu_top), and the two are compared.

Usage: python3 tools/has/make_ref_gate_mp.py <out dir> [--seed N] [--bus-base HEX]
"""
import argparse
import os
import struct

import numpy as np

B = 0x40000000
R = {"in": 0x400, "w": 0x404, "out": 0x408, "bias": 0x40C, "m": 0x410, "k": 0x414, "n": 0x418, "kh": 0x41C, "kw": 0x420,
     "stride": 0x424, "pad": 0x428, "act": 0x42C, "has_skip": 0x430, "skip": 0x434, "in_scale": 0x438, "w_scale": 0x43C,
     "out_scale": 0x440, "a": 0x444, "b": 0x448, "len": 0x450, "mode": 0x454, "scale_a": 0x458, "scale_b": 0x45C,
     "scale_o": 0x460, "a_len": 0x464, "b_len": 0x468}
X = ["IN_C", "IN_H", "IN_W", "OUT_C", "OUT_H", "OUT_W", "TILE_COUT", "TILE_H", "TILE_W", "SCALE_ADDR", "SHIFT_ADDR",
     "LUT_ADDR", "ZP_OUT", "FLAGS", "ZP_A", "ZP_B", "ZP_O", "SA", "SHA", "SB", "SHB", "SO", "SHO", "POOL_K", "POOL_P"]
ADDR = {"in", "w", "out", "bias", "skip", "a", "b", "SCALE_ADDR", "SHIFT_ADDR", "LUT_ADDR"}


def fbits(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("out")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--bus-base", type=lambda v: int(v, 0), default=0)
    a = ap.parse_args()
    rng = np.random.default_rng(a.seed)
    img = bytearray(256 * 1024)
    top = [0x1000]

    def alloc(n):
        p = top[0]
        top[0] = (p + n + 63) & ~63
        return p

    def put8(arr):
        arr = np.asarray(arr, dtype=np.int8).ravel()
        p = alloc(arr.size)
        img[p:p + arr.size] = arr.tobytes()
        return p

    def put32(arr):
        arr = np.asarray(arr, dtype="<i4").ravel()
        p = alloc(4 * arr.size)
        img[p:p + 4 * arr.size] = arr.tobytes()
        return p

    def weights(cout, cin, kh, kw, cout_t):
        """Random weights stored per output-channel range, each range laid out [c][kh][kw][k]."""
        K = cin * kh * kw
        p = alloc(cout * K)
        for c0 in range(0, cout, cout_t):
            nch = min(cout_t, cout - c0)
            blk = rng.integers(-20, 21, size=(cin, kh, kw, nch), dtype=np.int8)
            img[p + c0 * K:p + c0 * K + blk.size] = blk.tobytes()
        return p

    prog = []   # (label, {register or extension name: value}, opcode)

    # 0: 3 x 3, real-valued scale, ReLU
    cin, h, w, cout = 8, 12, 12, 16
    x0 = put8(rng.integers(-100, 101, size=cin * h * w))
    o0 = alloc(cout * h * w)
    prog.append(("GEMM_FUSED 3x3 real-valued scale ReLU", {
        "in": x0, "w": weights(cout, cin, 3, 3, 16), "out": o0, "bias": put32(rng.integers(-2000, 2001, size=cout)),
        "kh": 3, "kw": 3, "stride": 1, "pad": 1, "act": 1, "has_skip": 0,
        "in_scale": fbits(0.02), "w_scale": fbits(0.01), "out_scale": fbits(0.05),
        "IN_C": cin, "IN_H": h, "IN_W": w, "OUT_C": cout, "OUT_H": h, "OUT_W": w, "TILE_COUT": 16, "TILE_H": 12, "TILE_W": 12,
        "ZP_OUT": 3, "FLAGS": 0}, 0x12))

    # 1: 1 x 1, residual form
    skip = put8(rng.integers(-90, 91, size=cout * h * w))
    o1 = alloc(cout * h * w)
    prog.append(("GEMM_FUSED 1x1 HAS_SKIP", {
        "in": o0, "w": weights(cout, cout, 1, 1, 16), "out": o1, "bias": put32(rng.integers(-500, 501, size=cout)),
        "kh": 1, "kw": 1, "stride": 1, "pad": 0, "act": 0, "has_skip": 1, "skip": skip,
        "IN_C": cout, "IN_H": h, "IN_W": w, "OUT_C": cout, "OUT_H": h, "OUT_W": w, "TILE_COUT": 16, "TILE_H": 12, "TILE_W": 12,
        "SCALE_ADDR": put32(rng.integers(1 << 29, 1 << 30, size=cout)), "SHIFT_ADDR": put32(rng.integers(37, 40, size=cout)),
        "ZP_OUT": -2 & 0xFFFFFFFF, "FLAGS": 0}, 0x12))

    # 2: no extension registers: M x K -> N
    M, K, N = 64, 24, 40
    x2 = put8(rng.integers(-100, 101, size=K * M))
    o2 = alloc(N * M)
    prog.append(("GEMM_FUSED M 64 K 24 N 40 without extension registers", {
        "in": x2, "w": weights(N, K, 1, 1, 32), "out": o2, "bias": put32(rng.integers(-1000, 1001, size=N)),
        "m": M, "k": K, "n": N, "kh": 1, "kw": 1, "stride": 1, "pad": 0, "act": 0, "has_skip": 0,
        "in_scale": fbits(0.03), "w_scale": fbits(0.004), "out_scale": fbits(0.02)}, 0x12))

    # 3: ADD, real-valued scales
    n = cout * h * w
    o3 = alloc(n)
    prog.append(("ELEM_WISE ADD real-valued scales", {
        "a": o0, "b": skip, "out": o3, "len": n, "a_len": n, "b_len": n, "mode": 0,
        "scale_a": fbits(0.02), "scale_b": fbits(0.05), "scale_o": fbits(0.04)}, 0x15))

    # 4: ADD, integer parameters
    o4 = alloc(n)
    prog.append(("ELEM_WISE ADD integer parameters", {
        "a": o1, "b": o3, "out": o4, "len": n, "a_len": n, "b_len": n, "mode": 0,
        "ZP_A": 2, "ZP_B": -1 & 0xFFFFFFFF, "ZP_O": 4, "SA": 11000, "SHA": 0, "SB": 23000, "SHB": 1, "SO": 1, "SHO": 14}, 0x15))

    # 5: 1 x 1, table activation, last channel range shorter than TILE_COUT
    cout5 = 40
    lut = put8(np.clip(np.round(np.arange(-128, 128) * 0.9 - 5), -128, 127))
    o5 = alloc(cout5 * h * w)
    prog.append(("GEMM_FUSED 1x1 table activation cout 40 TILE_COUT 32", {
        "in": o4, "w": weights(cout5, cout, 1, 1, 32), "out": o5, "bias": put32(rng.integers(-800, 801, size=cout5)),
        "kh": 1, "kw": 1, "stride": 1, "pad": 0, "act": 2, "has_skip": 0,
        "IN_C": cout, "IN_H": h, "IN_W": w, "OUT_C": cout5, "OUT_H": h, "OUT_W": w, "TILE_COUT": 32, "TILE_H": 12, "TILE_W": 12,
        "SCALE_ADDR": put32(rng.integers(1 << 29, 1 << 30, size=cout5)), "SHIFT_ADDR": put32(rng.integers(38, 41, size=cout5)),
        "LUT_ADDR": lut, "ZP_OUT": 0, "FLAGS": 0}, 0x12))

    # 6, 7: MAX_POOL 5 x 5 s1 p2 twice in a row (as SPPF), 8: MAX_POOL 3 x 3 s1 p1 (check variant)
    o6 = alloc(n)
    prog.append(("ELEM_WISE MAX_POOL 5x5 s1 p2", {"a": o4, "out": o6, "mode": 1, "stride": 1, "IN_C": cout, "IN_H": h, "IN_W": w, "POOL_K": 5, "POOL_P": 2}, 0x15))
    o7 = alloc(n)
    prog.append(("ELEM_WISE MAX_POOL 5x5 s1 p2 again", {"a": o6, "out": o7, "mode": 1, "stride": 1, "IN_C": cout, "IN_H": h, "IN_W": w, "POOL_K": 5, "POOL_P": 2}, 0x15))
    o8 = alloc(cout5 * h * w)
    prog.append(("ELEM_WISE MAX_POOL 3x3 s1 p1", {"a": o5, "out": o8, "mode": 1, "stride": 1, "IN_C": cout5, "IN_H": h, "IN_W": w, "POOL_K": 3, "POOL_P": 1}, 0x15))

    assert top[0] <= len(img)
    for d, base in ((a.out, 0),) + (((a.out.rstrip("/\\") + "_bus", a.bus_base),) if a.bus_base else ()):
        os.makedirs(d, exist_ok=True)
        with open(os.path.join(d, "dram_init.bin"), "wb") as f:
            f.write(img)
        with open(os.path.join(d, "mmio.txt"), "w", newline="\n") as f:
            f.write("# make_ref_gate.py: %d instructions, image %d B, address base 0x%x\n" % (len(prog), len(img), base))
            for i, (label, regs, op) in enumerate(prog):
                f.write("# [%d] %s\n" % (i, label))
                for name, v in regs.items():
                    addr = B + (R[name] if name in R else 0x46C + 4 * X.index(name))
                    f.write("W %08x %x\n" % (addr, (v + base) & 0xFFFFFFFF if name in ADDR else v & 0xFFFFFFFF))
                f.write("W 40000310 %x\n" % op)
    print("wrote %s: %d instructions, %d bytes used of %d" % (a.out, len(prog), top[0], len(img)))


if __name__ == "__main__":
    main()
