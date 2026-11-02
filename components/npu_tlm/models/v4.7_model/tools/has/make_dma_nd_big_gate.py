#!/usr/bin/env python3
"""Check program for ELEM_WISE modes 6 (slice) / 7 (concat) on tensors larger than SRAM-A (79 KB): 64 x 64 x 64 int8."""
import os, sys
import numpy as np
B = 0x40000000
R = {"out": 0x408, "stride": 0x424, "a": 0x444, "b": 0x448, "mode": 0x454}
X = ["IN_C", "IN_H", "IN_W", "OUT_C", "OUT_H", "OUT_W", "TILE_COUT", "TILE_H", "TILE_W", "SCALE_ADDR", "SHIFT_ADDR",
     "LUT_ADDR", "ZP_OUT", "FLAGS", "ZP_A", "ZP_B", "ZP_O", "SA", "SHA", "SB", "SHB", "SO", "SHO", "POOL_K", "POOL_P",
     "POOL_MODE"]
out = sys.argv[1]
rng = np.random.default_rng(11)
init = bytearray(4 << 20); gold = bytearray(4 << 20); top = [0x1000]
def alloc(n):
    p = top[0]; top[0] = (p + n + 63) & ~63; return p
def put(img, p, a): b = np.ascontiguousarray(a, dtype=np.int8).tobytes(); img[p:p + len(b)] = b
def put_both(a): p = alloc(a.size); put(init, p, a); put(gold, p, a); return p
C = H = W = 64
x = rng.integers(-128, 128, size=(C, H, W), dtype=np.int8); xa = put_both(x)
prog = []
def sl(ax, p, k, s):
    idx = [slice(None)] * 3; idx[ax] = slice(p, p + k, s); g = x[tuple(idx)].copy(); o = alloc(g.size); put(gold, o, g)
    prog.append(("slice axis %d [%d, %d) step %d -> %s" % (ax, p, p + k, s, g.shape),
                 {"a": xa, "b": 0, "out": o, "mode": 6, "stride": s, "IN_C": C, "IN_H": H, "IN_W": W, "POOL_MODE": ax, "POOL_P": p, "POOL_K": k}))
sl(0, 8, 48, 1); sl(1, 4, 40, 1); sl(2, 1, 60, 3); sl(0, 0, 64, 2)
# concat A + B along C, H, W
for ax, k in ((0, 32), (1, 24), (2, 40)):
    ia = [slice(None)] * 3; ib = [slice(None)] * 3; ia[ax] = slice(0, k); ib[ax] = slice(k, 64)
    a = put_both(x[tuple(ia)].copy()); b = put_both(x[tuple(ib)].copy()); o = alloc(x.size); put(gold, o, x)
    prog.append(("concat A + B axis %d at %d" % (ax, k), {"a": a, "b": b, "out": o, "mode": 7, "stride": 1,
                 "OUT_C": C, "OUT_H": H, "OUT_W": W, "POOL_MODE": ax, "POOL_P": 0, "POOL_K": k}))
# single part into a pre-filled destination along each axis
for ax, p, k in ((0, 16, 40), (1, 16, 32), (2, 10, 30)):
    bg = rng.integers(-128, 128, size=(C, H, W), dtype=np.int8); o = put_both(bg)
    ip = [slice(None)] * 3; ip[ax] = slice(p, p + k); part = x[tuple(ip)].copy(); a = put_both(part)
    g = bg.copy(); g[tuple(ip)] = part; put(gold, o, g)
    prog.append(("concat one part axis %d at [%d, %d)" % (ax, p, p + k), {"a": a, "b": 0, "out": o, "mode": 7, "stride": 1,
                 "OUT_C": C, "OUT_H": H, "OUT_W": W, "POOL_MODE": ax, "POOL_P": p, "POOL_K": k}))
os.makedirs(out, exist_ok=True)
open(os.path.join(out, "dram_init.bin"), "wb").write(init[:top[0]])
open(os.path.join(out, "dram_golden.bin"), "wb").write(gold[:top[0]])
with open(os.path.join(out, "mmio.txt"), "w", newline="\n") as f:
    f.write("# make_dma_nd_big_gate.py: %d instructions\n" % len(prog))
    for i, (lab, regs) in enumerate(prog):
        f.write("# [%d] %s\n" % (i, lab))
        for n, v in regs.items():
            f.write("W %08x %x\n" % (B + (R[n] if n in R else 0x46C + 4 * X.index(n)), v & 0xFFFFFFFF))
        f.write("W 40000310 15\n")
print("wrote %s: %d instructions, %d bytes" % (out, len(prog), top[0]))
