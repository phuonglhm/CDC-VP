#!/usr/bin/env python3
"""Check program of DMA_TENSOR (opcode 0x16) COPY: contiguous DRAM block copies between and after other instructions.

Writes a program directory (mmio.txt, dram_init.bin) on random data:
  * a concat built by COPY from two GEMM_FUSED outputs (read after write GEMM -> COPY) and read by a GEMM_FUSED
    (read after write COPY -> GEMM), an ELEM_WISE ADD copied to an unaligned address and read by a MAX_POOL
    (COPY -> ELEM_WISE), a chain of two copies (COPY -> COPY), a copy over the input of an earlier GEMM_FUSED that a later
    GEMM_FUSED reads (write after read);
  * copies of 1 .. 200,000 bytes around the chunk and burst sizes, with unaligned sources and destinations.
COPY registers: A_ADDR source, OUT_ADDR destination, LEN bytes, MODE_PACK 0; push 0x16 (the model needs HAS_DMA_TENSOR=1).
No reference image is written: tools/has/ref_program.py computes one (--write-ref), the model produces its own
(FE_SNAPSHOT_DIR of tools/has/tb_has_npu_top), and the two are compared. The copies whose source is untouched input data
are also listed with their expected bytes in copy_expect.npz; --check IMAGE compares an image with that list (a third
reference that needs neither the model nor ref_program.py).

Usage: python3 tools/has/make_dma_tensor_gate.py <out dir> [--seed N]
       python3 tools/has/make_dma_tensor_gate.py <out dir> --check <image>
"""
import argparse
import os
import sys

import numpy as np

B = 0x40000000
R = {"in": 0x400, "w": 0x404, "out": 0x408, "bias": 0x40C, "kh": 0x41C, "kw": 0x420, "stride": 0x424, "pad": 0x428,
     "act": 0x42C, "has_skip": 0x430, "a": 0x444, "b": 0x448, "len": 0x450, "mode": 0x454, "a_len": 0x464, "b_len": 0x468}
X = ["IN_C", "IN_H", "IN_W", "OUT_C", "OUT_H", "OUT_W", "TILE_COUT", "TILE_H", "TILE_W", "SCALE_ADDR", "SHIFT_ADDR",
     "LUT_ADDR", "ZP_OUT", "FLAGS", "ZP_A", "ZP_B", "ZP_O", "SA", "SHA", "SB", "SHB", "SO", "SHO", "POOL_K", "POOL_P"]
IMG = 1536 * 1024
SIZES = [1, 15, 16, 17, 127, 128, 129, 1023, 1024, 1025, 4097, 151296, 200000]


def check(out, image):
    e = np.load(os.path.join(out, "copy_expect.npz"))
    with open(image, "rb") as f:
        img = np.frombuffer(f.read(), dtype=np.uint8)
    bad = 0
    for addr, n, i in zip(e["addr"], e["n"], range(len(e["addr"]))):
        want = e["data_%d" % i]
        got = img[int(addr):int(addr) + int(n)]
        d = int(np.count_nonzero(got != want)) if got.size == want.size else int(n)
        bad += d
        print("[copy_expect] 0x%06x %7d B different %d %s" % (addr, n, d, "PASS" if d == 0 else "FAIL"))
    print("[copy_expect] RESULT: %s (%d regions, %d bytes different)" % ("PASS" if bad == 0 else "FAIL", len(e["addr"]), bad))
    return 0 if bad == 0 else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("out")
    ap.add_argument("--seed", type=int, default=25)
    ap.add_argument("--check", help="compare this image with copy_expect.npz of <out>")
    a = ap.parse_args()
    if a.check:
        return check(a.out, a.check)
    rng = np.random.default_rng(a.seed)
    img = bytearray(IMG)
    top = [0x1000]

    def alloc(n, skew=0):
        p = top[0] + skew
        top[0] = (p + n + 63) & ~63
        return p

    def put8(arr, skew=0):
        arr = np.asarray(arr, dtype=np.int8).ravel()
        p = alloc(arr.size, skew)
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

    prog = []      # (label, {register or extension name: value}, opcode)
    expect = []    # (addr, bytes) of copies whose source is input data no instruction writes before the copy

    def gemm(label, x, cin, h, w, cout, k, pad, out, act=0):
        prog.append((label, {
            "in": x, "w": weights(cout, cin, k, k, 16), "out": out, "bias": put32(rng.integers(-1500, 1501, size=cout)),
            "kh": k, "kw": k, "stride": 1, "pad": pad, "act": act, "has_skip": 0,
            "IN_C": cin, "IN_H": h, "IN_W": w, "OUT_C": cout, "OUT_H": h, "OUT_W": w, "TILE_COUT": 16, "TILE_H": h, "TILE_W": w,
            "SCALE_ADDR": put32(rng.integers(1 << 29, 1 << 30, size=cout)), "SHIFT_ADDR": put32(rng.integers(37, 40, size=cout)),
            "ZP_OUT": 0, "FLAGS": 0}, 0x12))

    def copy(label, src, dst, n):
        prog.append((label, {"a": src, "out": dst, "len": n, "mode": 0}, 0x16))

    cin, h, w, c = 8, 12, 12, 16
    n = c * h * w
    x0 = put8(rng.integers(-100, 101, size=cin * h * w))
    x0_data = bytes(img[x0:x0 + cin * h * w])
    o0, o1 = alloc(n), alloc(n)
    gemm("GEMM_FUSED 3x3 -> concat input 0", x0, cin, h, w, c, 3, 1, o0, act=1)
    gemm("GEMM_FUSED 1x1 -> concat input 1", x0, cin, h, w, c, 1, 0, o1)
    cat = alloc(2 * n)
    copy("COPY GEMM output 0 -> concat cell 0 (read after write GEMM -> COPY)", o0, cat, n)
    copy("COPY GEMM output 1 -> concat cell 1", o1, cat + n, n)
    o4 = alloc(n)
    gemm("GEMM_FUSED 1x1 on the 32-channel concat (read after write COPY -> GEMM)", cat, 2 * c, h, w, c, 1, 0, o4)
    o5 = alloc(n)
    prog.append(("ELEM_WISE ADD of the concat GEMM and GEMM output 0", {
        "a": o4, "b": o0, "out": o5, "len": n, "a_len": n, "b_len": n, "mode": 0,
        "ZP_A": 0, "ZP_B": 0, "ZP_O": 0, "SA": 1 << 30, "SHA": 14, "SB": 1 << 30, "SHB": 14, "SO": 1 << 30, "SHO": 46}, 0x15))
    u = alloc(n, skew=3)
    copy("COPY ADD output -> unaligned address (ADD -> COPY)", o5, u, n)
    o7 = alloc(n)
    prog.append(("ELEM_WISE MAX_POOL 3x3 s1 p1 on the copy (COPY -> ELEM_WISE)", {
        "a": u, "out": o7, "stride": 1, "mode": 1, "IN_C": c, "IN_H": h, "IN_W": w, "POOL_K": 3, "POOL_P": 1}, 0x15))
    x2 = alloc(cin * h * w, skew=7)
    x3 = alloc(cin * h * w)
    copy("COPY input tensor -> X2", x0, x2, cin * h * w)
    copy("COPY X2 -> X3 (read after write COPY -> COPY)", x2, x3, cin * h * w)
    expect += [(x2, x0_data), (x3, x0_data)]
    z = put8(rng.integers(-100, 101, size=cin * h * w))
    copy("COPY new data over the input of the first GEMM (write after read)", z, x0, cin * h * w)
    expect.append((x0, bytes(img[z:z + cin * h * w])))
    o11 = alloc(n)
    gemm("GEMM_FUSED 3x3 on the overwritten input", x0, cin, h, w, c, 3, 1, o11)
    for i, s in enumerate(SIZES):
        src = put8(rng.integers(-128, 128, size=s), skew=(5 * i) % 16)
        dst = alloc(s, skew=(11 * i) % 16)
        copy("COPY %d bytes, source offset %d, destination offset %d mod 64" % (s, src % 64, dst % 64), src, dst, s)
        expect.append((dst, bytes(img[src:src + s])))

    assert top[0] <= len(img), top[0]
    os.makedirs(a.out, exist_ok=True)
    with open(os.path.join(a.out, "dram_init.bin"), "wb") as f:
        f.write(img)
    np.savez(os.path.join(a.out, "copy_expect.npz"), addr=np.array([e[0] for e in expect]), n=np.array([len(e[1]) for e in expect]),
             **{"data_%d" % i: np.frombuffer(e[1], dtype=np.uint8) for i, e in enumerate(expect)})
    with open(os.path.join(a.out, "mmio.txt"), "w", newline="\n") as f:
        ncopy = sum(1 for p in prog if p[2] == 0x16)
        f.write("# make_dma_tensor_gate.py: %d instructions (%d DMA_TENSOR COPY), image %d B\n" % (len(prog), ncopy, len(img)))
        for i, (label, regs, op) in enumerate(prog):
            f.write("# [%d] %s\n" % (i, label))
            for name, v in regs.items():
                addr = B + (R[name] if name in R else 0x46C + 4 * X.index(name))
                f.write("W %08x %x\n" % (addr, v & 0xFFFFFFFF))
            f.write("W 40000310 %x\n" % op)
    print("wrote %s: %d instructions (%d COPY), %d bytes used of %d" % (a.out, len(prog), ncopy, top[0], len(img)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
