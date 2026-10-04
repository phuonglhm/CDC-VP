"""Instruction-path check of FUSED_ATTN / LAYERNORM: turn the golden unit vectors into an instruction stream for tools/has/tb_has_npu_top, so FUSED_ATTN /
LAYERNORM run the way a CPU drives them (MMIO -> compat decoder -> DFC -> GVU) and every output is compared with the golden byte by byte.
Inputs: $FE_WORK/has/vectors/gvu/{attn_<name>,ln_<case>}.txt + tools/has/vec_gvu (export_gvu_attn.py / export_gvu_ln.py output).
Output dir (default fe_work/has/rce_gate, relative to the repository root): dram_init.bin, dram_golden.bin, mmio.txt (same format as fe_emit_insts.py).
Parameter blocks: has/gvu_rce_params.h (ATN1 / LNP1). Usage (repository root): python3 tools/has/make_rce_gate.py [vec] [tables] [out]
"""
import os
import struct
import sys

vec = sys.argv[1] if len(sys.argv) > 1 else "fe_work/has/vectors/gvu"
tab = sys.argv[2] if len(sys.argv) > 2 else "tools/has/vec_gvu"
out = sys.argv[3] if len(sys.argv) > 3 else "fe_work/has/rce_gate"
CM = os.environ.get("RCE_CM") == "1"   # FLAGS bit 1 CHANNEL_MAJOR: operands / outputs stored [C][tokens]
os.makedirs(out, exist_ok=True)

init, gold = bytearray(), bytearray()
mmio = ["# make_rce_gate.py: FUSED_ATTN heads + LAYERNORM cases from the GVU unit vectors"]


def alloc(data_init, data_gold=None):
    """Append a 64-byte aligned region; returns its address."""
    while len(init) % 64:
        init.append(0); gold.append(0)
    a = len(init)
    init.extend(data_init)
    gold.extend(data_gold if data_gold is not None else data_init)
    return a


def i8(rows, cm=False):
    if cm:
        rows = [list(c) for c in zip(*rows)]
    return bytes((v & 0xFF) for r in rows for v in r)


def sections(path):
    s = {}
    for line in open(path):
        if not line.strip() or line.startswith("#"):
            continue
        t = line.split()
        s.setdefault(t[0], []).append([int(v) for v in t[1:]])
    return s


def w(addr, val):
    mmio.append("W %x %x" % (addr, val & 0xFFFFFFFF))


n = 0
for line in open(os.path.join(tab, "attn_params.txt")):
    t = line.split()
    name, (Zq, Zk, Zv, Zqk, Zav, Mqk, TSqk, Mav, TSav, mz, zc, az) = t[0], [int(v) for v in t[1:]]
    exp = [int(v) for v in open(os.path.join(tab, "attn_exp_%s.txt" % name)).read().split()]
    assert len(exp) == 256, name
    blk = struct.pack("<I5i", 0x314E5441, Zq, Zk, Zv, Zqk, Zav) + struct.pack("<qi", Mqk, TSqk) + struct.pack("<qi", Mav, TSav)
    blk += struct.pack("<II", az | (mz << 1) | (zc << 2), 0) + struct.pack("<256i", *exp)
    assert len(blk) == 4 * 270
    s = sections(os.path.join(vec, "attn_%s.txt" % name))
    L, D, NQ = len(s["K"]), len(s["Q"][0]), len(s["Q"])
    pa, qa, ka, va, ma = alloc(blk), alloc(i8(s["Q"], CM)), alloc(i8(s["K"], CM)), alloc(i8(s["V"], CM)), alloc(i8(s["M"]))
    oa = alloc(bytes(NQ * D), i8(s["O"], CM))
    mmio.append("# [%d] FUSED_ATTN %s NQ %d L %d D %d" % (n, name, NQ, L, D))
    for a, v in ((0x40000444, qa), (0x40000448, ka), (0x4000044C, va), (0x40000450, L), (0x40000458, D), (0x40000408, oa),
                 (0x400004DC, NQ), (0x400004E0, pa), (0x400004E4, ma)):
        w(a, v)
    if CM:
        w(0x400004A0, 2)
    w(0x40000310, 0x13)
    n += 1

for name in open(os.path.join(tab, "ln_cases.txt")).read().split():
    pf = open(os.path.join(tab, "ln_params_%s.txt" % name)).read().splitlines()
    scal = [int(v) for v in pf[0].split()]
    gamma, beta, m0, ts = ([int(v) for v in l.split()] for l in pf[1:5])
    H, out16 = scal[0], scal[18] != 0
    blk = struct.pack("<Ii", 0x31504E4C, H) + struct.pack("<19q", *scal)
    blk += struct.pack("<%di" % H, *gamma) + struct.pack("<%dq" % H, *beta) + struct.pack("<%dq" % H, *m0) + struct.pack("<%di" % H, *ts)
    s = sections(os.path.join(vec, "ln_%s.txt" % name))
    R = len(s["X"])
    Y = [list(c) for c in zip(*s["Y"])] if CM else s["Y"]
    yb = b"".join(struct.pack("<%dh" % len(r), *r) for r in Y) if out16 else i8(Y)
    pa, xa = alloc(blk), alloc(i8(s["X"], CM))
    ya = alloc(bytes(len(yb)), yb)
    mmio.append("# [%d] LAYERNORM %s rows %d H %d out %s" % (n, name, R, H, "int16" if out16 else "int8"))
    for a, v in ((0x40000400, xa), (0x40000408, ya), (0x40000450, H), (0x400004DC, R), (0x400004E0, pa)):
        w(a, v)
    if CM:
        w(0x400004A0, 2)
    w(0x40000310, 0x14)
    n += 1

while len(init) % 4096:
    init.append(0); gold.append(0)
open(os.path.join(out, "dram_init.bin"), "wb").write(init)
open(os.path.join(out, "dram_golden.bin"), "wb").write(gold)
open(os.path.join(out, "mmio.txt"), "w").write("\n".join(mmio) + "\n")
print("RCE check: %d instructions, DRAM %d B -> %s" % (n, len(init), out))
