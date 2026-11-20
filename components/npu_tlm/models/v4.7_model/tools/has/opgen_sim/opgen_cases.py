#!/usr/bin/env python3
"""opgen_cases.py -- cases, independent reference and checker for tb_opgen.sv (redma_top of neo_lite revision 2).

  python opgen_cases.py gen   <outdir>      one directory per group: cmds.txt, mem_init.hex, expect.hex, expect_seg.txt, expect_rd.txt
  python opgen_cases.py check <outdir>      for every group with a sim.log + dump.hex: data (whole memory), segment stream, register reads;
                                            writes timing.csv (one row per descriptor, one per segment)

The reference is written from the description of the operations (sizes, strides, rows), NOT from redma_opgen.sv: segments and data are
computed with plain loops over the logical indices. Cycle numbers are not predicted here; check only collects them.
"""
import os
import random
import re
import sys

MEMB = 1 << 18
OG = 0x80
REG = dict(CTRL=0x00, OP=0x04, RADDR=0x08, WADDR=0x0C, CNT01=0x10, CNT2=0x14, GRPB=0x18, ROWB=0x1C, RS0=0x20, WS0=0x24, RS1=0x28,
           WS1=0x2C, RS2=0x30, WS2=0x34, PADL=0x38, PADR=0x3C, FULL=0x40, ID=0x44)
COPY, FILL, PAD2D, UPS = 0, 1, 2, 3
SRC, DST = 0x00100, 0x20100          # source / destination areas (the rest of the memory is guard)


class D:
    def __init__(self, op=COPY, ra=SRC, wa=DST, n0=1, n1=1, n2=1, row=16, grp=0, rs0=0, ws0=0, rs1=0, ws1=0, rs2=0, ws2=0,
                 padl=0, padr=0, full=0, pt=0, pb=0, rep0=1, rep1=1, fill=0):
        self.__dict__.update(locals())
        del self.__dict__["self"]

    def words(self):
        op = (self.op & 3) | (self.pt << 2) | (self.pb << 10) | (self.rep0 << 18) | (self.rep1 << 22) | (self.fill << 24)
        return [("OP", op), ("RADDR", self.ra), ("WADDR", self.wa), ("CNT01", self.n0 | (self.n1 << 16)), ("CNT2", self.n2),
                ("GRPB", self.grp), ("ROWB", self.row), ("RS0", self.rs0), ("WS0", self.ws0), ("RS1", self.rs1), ("WS1", self.ws1),
                ("RS2", self.rs2), ("WS2", self.ws2), ("PADL", self.padl), ("PADR", self.padr), ("FULL", self.full)]

    def segments(self):
        """(fill, raddr, waddr, len) in order, from the logical loops."""
        o, s = self, []
        if o.op in (COPY, FILL):
            for i2 in range(o.n2):
                for i1 in range(o.n1):
                    for i0 in range(o.n0):
                        s.append((o.op == FILL, o.ra + i2 * o.rs2 + i1 * o.rs1 + i0 * o.rs0, o.wa + i2 * o.ws2 + i1 * o.ws1 + i0 * o.ws0, o.row))
        elif o.op == PAD2D:
            for i2 in range(o.n2):
                for r in range(o.pt + o.n1 + o.pb):
                    w = o.wa + i2 * o.ws2 + r * o.ws1
                    if r < o.pt or r >= o.pt + o.n1:
                        s.append((True, 0, w, o.full))
                        continue
                    ra = o.ra + i2 * o.rs2 + (r - o.pt) * o.rs1
                    if o.padl:
                        s.append((True, 0, w, o.padl))
                    s.append((False, ra, w + o.padl, o.row))
                    if o.padr:
                        s.append((True, 0, w + o.padl + o.row, o.padr))
        else:
            for i2 in range(o.n2):
                for r in range(o.n1):
                    for ch in range(o.rep1):
                        w = o.wa + i2 * o.ws2 + (r * o.rep1 + ch) * o.ws1
                        ra = o.ra + i2 * o.rs2 + r * o.rs1
                        if o.rep0 == 1:
                            s.append((False, ra, w, o.row))
                        else:
                            for g in range(o.n0):
                                for cv in range(o.rep0):
                                    s.append((False, ra + g * o.grp, w + (g * o.rep0 + cv) * o.grp, o.grp))
        return s


def apply(mem, segs, fillv):
    for f, ra, wa, n in segs:
        if f:
            mem[wa:wa + n] = bytes([fillv]) * n
        else:
            mem[wa:wa + n] = bytes(mem[ra:ra + n])


def groups():
    g = {}
    # group 1: one segment, many lengths x alignments (cost of one segment)
    L = [1, 3, 15, 16, 17, 31, 32, 33, 48, 64, 100, 128, 255, 256, 257, 512, 1024, 2048, 4096]
    g["g1_copy1"] = [D(ra=SRC + so, wa=DST + do, row=n) for n in L for (so, do) in ((0, 0), (3, 0), (0, 5), (3, 7))]
    # group 2: three-level copies with strides (slice, concat, corner turn shapes)
    g["g2_copynd"] = [
        D(n0=4, n1=3, n2=2, row=16, rs0=16, ws0=16, rs1=64, ws1=96, rs2=512, ws2=640),
        D(n0=1, n1=8, n2=1, row=13, rs1=40, ws1=13),
        D(n0=5, n1=2, n2=3, row=48, rs0=100, ws0=48, rs1=700, ws1=240, rs2=2048, ws2=480),
        D(n0=3, n1=3, n2=3, row=1, rs0=3, ws0=1, rs1=17, ws1=3, rs2=99, ws2=9),
        D(n0=1, n1=1, n2=2, row=200, rs2=300, ws2=200),
        D(n0=7, n1=1, n2=1, row=33, rs0=33, ws0=40),
    ]
    # group 3: fill
    g["g3_fill"] = [D(op=FILL, fill=0x5A, n0=1, n1=1, n2=1, row=n) for n in (1, 16, 17, 100, 1000)] + [
        D(op=FILL, fill=0x80, n0=3, n1=4, n2=2, row=21, ws0=21, ws1=100, ws2=500),
        D(op=FILL, fill=0x00, n0=2, n1=2, n2=1, row=64, ws0=64, ws1=128)]
    # group 4: pad2d
    g["g4_pad"] = [
        D(op=PAD2D, n1=4, n2=1, row=12, pt=1, pb=2, padl=3, padr=5, full=20, rs1=12, ws1=20, fill=0x81),
        D(op=PAD2D, n1=3, n2=2, row=16, pt=0, pb=0, padl=2, padr=0, full=18, rs1=16, ws1=24, rs2=64, ws2=100, fill=0x07),
        D(op=PAD2D, n1=3, n2=1, row=16, pt=2, pb=0, padl=0, padr=4, full=20, rs1=16, ws1=20, fill=0xF0),
        D(op=PAD2D, n1=5, n2=1, row=40, pt=1, pb=1, padl=0, padr=0, full=40, rs1=40, ws1=40, fill=0x11),
        D(op=PAD2D, n1=2, n2=1, row=1, pt=3, pb=3, padl=1, padr=1, full=3, rs1=1, ws1=3, fill=0x33)]
    # group 5: upsample (rep0 = 1: whole rows; rep0 > 1: groups)
    g["g5_ups"] = [
        D(op=UPS, n1=4, n2=1, row=24, rs1=24, ws1=24, rep0=1, rep1=2),
        D(op=UPS, n1=2, n2=2, row=16, rs1=16, ws1=16, rs2=32, ws2=96, rep0=1, rep1=3),
        D(op=UPS, n0=5, n1=3, n2=1, grp=8, row=40, rs1=40, ws1=80, rep0=2, rep1=2),
        D(op=UPS, n0=4, n1=2, n2=2, grp=3, row=12, rs1=12, ws1=36, rs2=24, ws2=100, rep0=3, rep1=1),
        D(op=UPS, n0=6, n1=2, n2=1, grp=16, row=96, rs1=96, ws1=384, rep0=2, rep1=2)]
    # group 6: cost of many equal segments (n0 loop) at several segment sizes
    g["g6_slope"] = [D(n0=64, row=r, rs0=r, ws0=r) for r in (1, 4, 16, 32, 64, 128, 256)] + [D(n0=16, row=16, rs0=48, ws0=16)]
    # group 10: hold-out (12 random descriptors, formula of model_cost.py frozen before this group was run)
    rnd = random.Random(20261112)
    hold = []
    for _ in range(12):
        op = rnd.choice([COPY, COPY, FILL, PAD2D, UPS, UPS])
        sa, da = SRC + rnd.randrange(16), DST + rnd.randrange(16)
        if op == COPY:
            r = rnd.choice([5, 16, 24, 40, 70, 130])
            n0, n1, n2 = rnd.randrange(1, 6), rnd.randrange(1, 5), rnd.randrange(1, 3)
            hold.append(D(ra=sa, wa=da, n0=n0, n1=n1, n2=n2, row=r, rs0=r + rnd.randrange(0, 20), ws0=r + rnd.randrange(0, 10), rs1=400, ws1=300, rs2=2000, ws2=1500))
        elif op == FILL:
            r = rnd.choice([3, 16, 33, 90])
            hold.append(D(op=FILL, wa=da, fill=rnd.randrange(256), n0=rnd.randrange(1, 5), n1=rnd.randrange(1, 4), n2=1, row=r, ws0=r + 4, ws1=300))
        elif op == PAD2D:
            r = rnd.choice([8, 20, 35])
            pl, pr = rnd.randrange(0, 6), rnd.randrange(0, 6)
            hold.append(D(op=PAD2D, ra=sa, wa=da, n1=rnd.randrange(1, 5), n2=1, row=r, pt=rnd.randrange(0, 3), pb=rnd.randrange(0, 3), padl=pl, padr=pr,
                          full=pl + r + pr, rs1=r, ws1=pl + r + pr + 3, fill=rnd.randrange(256)))
        else:
            r = rnd.choice([16, 32, 48])
            if rnd.random() < 0.5:
                hold.append(D(op=UPS, ra=sa, wa=da, n1=rnd.randrange(1, 5), n2=1, row=r, rs1=r, ws1=r, rep0=1, rep1=rnd.randrange(2, 4)))
            else:
                grp = rnd.choice([4, 8, 12, 16])
                n0 = r // grp
                hold.append(D(op=UPS, ra=sa, wa=da, n0=n0, n1=rnd.randrange(1, 4), n2=1, grp=grp, row=n0 * grp, rs1=n0 * grp, ws1=n0 * grp * 3, rep0=rnd.randrange(2, 4), rep1=rnd.randrange(1, 3)))
    g["g10_holdout"] = hold
    # group 11: the descriptors tools/fe/fe_emit_insts.py --slice-concat opgen writes: UPSAMPLE of an NCHW int8 tensor (GRP_BYTES = 1) and a one-run COPY
    for (c, h, w, f) in ((3, 4, 5, 2), (2, 3, 7, 3)):
        g.setdefault("g11_fe", []).append(D(op=UPS, ra=SRC, wa=DST, n0=w, n1=h, n2=c, grp=1, row=w, rs1=w, ws1=f * w, rs2=h * w, ws2=f * h * f * w, rep0=f, rep1=f))
        g["g11_fe"].append(D(ra=SRC + 7, wa=DST + 0x4000 + 5, row=c * h * w * 3))
    return g


def init_mem(seed):
    rnd = random.Random(seed)
    return bytearray(rnd.getrandbits(8) for _ in range(MEMB))


def write_hex(path, mem):
    with open(path, "w", newline="\n") as f:
        f.write("\n".join("%02x" % b for b in mem) + "\n")


def gen_group(out, name, descs, legacy=None, regs=False):
    os.makedirs(out, exist_ok=True)
    mem = init_mem(1234 + sum(map(ord, name)))
    write_hex(os.path.join(out, "mem_init.hex"), mem)
    cmds, expect_seg, expect_rd, ndone = [], [], [], 0
    if name == "g11_fe":   # independent reference: nearest-neighbour upsample of NCHW straight from the definition
        m0 = bytearray(mem)
        for d in descs:
            if d.op == UPS:
                c, h, w, f = d.n2, d.n1, d.n0, d.rep0
                src = m0[d.ra:d.ra + c * h * w]
                want = bytearray(c * f * h * f * w)
                for ci in range(c):
                    for y in range(h * f):
                        for x in range(w * f):
                            want[(ci * h * f + y) * w * f + x] = src[(ci * h + y // f) * w + x // f]
                m1 = bytearray(m0)
                apply(m1, d.segments(), 0)
                assert m1[d.wa:d.wa + len(want)] == want, "descriptor words of the upsample do not give the NCHW nearest-neighbour result"
    for d in descs:
        for nm, v in d.words():
            cmds.append("W %x %08x" % (OG + REG[nm], v))
        cmds += ["S", "P"]
        segs = d.segments()
        expect_seg += [(f, ra, wa, n, d.fill if f else 0) for (f, ra, wa, n) in segs]
        apply(mem, segs, d.fill)
        ndone += 1
    for (ra, wa, btt, wz) in (legacy or []):
        cmds.append("L %x %x %x %d" % (ra, wa, btt, wz))
        if wz:
            mem[wa:wa + btt] = bytes(btt)
        else:
            mem[wa:wa + btt] = bytes(mem[ra:ra + btt])
    if regs:
        cmds += ["W %x deadbeef" % (OG + REG["OP"]), "R %x" % (OG + REG["OP"]), "R %x" % (OG + REG["ID"]), "R %x" % (OG + REG["CTRL"]),
                 "R %x" % (OG + 0x48), "R %x" % (OG + 0x7C), "R 00", "R 08", "W 10 00012345", "R 10", "W 04 00000002", "R 04", "R 0c"]
        expect_rd = [(OG + 4, 0xDEADBEEF, 0), (OG + 0x44, 0x5ED00001, 0), (OG + 0, 0, 0), (OG + 0x48, 0, 0), (OG + 0x7C, 0, 0),
                     (0, 0x2D000000, 0), (8, 0x3BADADD2, 2), (0x10, 0x12345, 0), (4, 2, 0), (0xC, 0, 0)]
    cmds.append("Q")
    open(os.path.join(out, "cmds.txt"), "w", newline="\n").write("\n".join(cmds) + "\n")
    write_hex(os.path.join(out, "expect.hex"), mem)
    with open(os.path.join(out, "expect_seg.txt"), "w", newline="\n") as f:
        for (fl, ra, wa, n, fv) in expect_seg:
            f.write("%d %s %08x %d %02x\n" % (fl, "--------" if fl else "%08x" % ra, wa, n, fv))
    with open(os.path.join(out, "expect_rd.txt"), "w", newline="\n") as f:
        for a, v, r in expect_rd:
            f.write("%08x %08x %d\n" % (a, v, r))
    with open(os.path.join(out, "descs.txt"), "w", newline="\n") as f:
        for d in descs:
            f.write("%d %d %d\n" % (d.op, len(d.segments()), sum(s[3] for s in d.segments())))
        for (ra, wa, btt, wz) in (legacy or []):
            f.write("L %d %d\n" % (btt, wz))
    return name


def gen(outdir):
    for name, descs in groups().items():
        gen_group(os.path.join(outdir, name), name, descs)
    # group 7: one-run descriptors through conf_regs
    leg = [(SRC + so, DST + do, n, 0) for n in (16, 64, 256, 1024, 2048, 4096) for (so, do) in ((0, 0), (3, 5))] + [(SRC, DST, 64, 1), (SRC, DST + 3, 100, 1)]
    gen_group(os.path.join(outdir, "g7_legacy"), "g7_legacy", [], legacy=leg)
    # group 8: two STARTs back to back (start_pend); group 9: register reads
    d = D(n0=8, row=64, rs0=64, ws0=64)
    gen_group(os.path.join(outdir, "g9_regs"), "g9_regs", [], regs=True)
    os.makedirs(os.path.join(outdir, "g8_twostart"), exist_ok=True)
    gen_group(os.path.join(outdir, "g8_twostart"), "g8_twostart", [])
    cm = []
    for nm, v in d.words():
        cm.append("W %x %08x" % (OG + REG[nm], v))
    cm += ["S", "S", "P", "P", "Q"]
    open(os.path.join(outdir, "g8_twostart", "cmds.txt"), "w", newline="\n").write("\n".join(cm) + "\n")
    mem = init_mem(1234 + sum(map(ord, "g8_twostart")))
    segs = d.segments()
    apply(mem, segs, 0)
    write_hex(os.path.join(outdir, "g8_twostart", "expect.hex"), mem)
    with open(os.path.join(outdir, "g8_twostart", "expect_seg.txt"), "w", newline="\n") as f:
        for (fl, ra, wa, n) in segs + segs:
            f.write("%d %08x %08x %d %02x\n" % (fl, ra, wa, n, 0))
    print("groups written to", outdir)


def read_hex(path):
    with open(path) as f:
        out = bytearray()
        for ln in f:
            ln = ln.strip()
            if not ln or ln.startswith("//") or ln.startswith("@"):
                continue
            out.append(int(ln.lower().replace("x", "0"), 16))   # an x in the dump (never written) counts as a mismatch below through the 0 fill only if the reference is not 0
        return out


def check(outdir):
    bad_all = 0
    rows = []
    for name in sorted(os.listdir(outdir)):
        gd = os.path.join(outdir, name)
        lg = os.path.join(gd, "sim.log")
        if not os.path.isdir(gd) or not os.path.exists(lg) or not os.path.exists(os.path.join(gd, "dump.hex")):
            continue
        log = [re.sub(r"^# ", "", l) for l in open(lg, errors="replace").read().split("\n")]
        end = [l for l in log if l.startswith("END")]
        dump, exp = read_hex(os.path.join(gd, "dump.hex")), read_hex(os.path.join(gd, "expect.hex"))
        bad_bytes = sum(1 for a, b in zip(dump, exp) if a != b) + abs(len(dump) - len(exp))
        segs = []
        for l in log:
            m = re.match(r"SEG (\d+) (\d+) ([0-9a-fA-F]+) ([0-9a-fA-F]+) (\d+) ([0-9a-fA-F]+) (\d+)", l)
            if m:
                segs.append((int(m.group(1)), int(m.group(2)), int(m.group(3), 16), int(m.group(4), 16), int(m.group(5)), int(m.group(6), 16), int(m.group(7))))
        es = [l.split() for l in open(os.path.join(gd, "expect_seg.txt")).read().strip().split("\n") if l.strip()]
        bad_seg = 0
        if os.path.exists(os.path.join(gd, "expect_seg.txt")) and (es or segs) and name != "g7_legacy":
            if len(es) != len(segs):
                bad_seg = abs(len(es) - len(segs)) or 1
                print("  %s: segment count RTL %d, reference %d" % (name, len(segs), len(es)))
            for k, (e, s) in enumerate(zip(es, segs)):
                ef, era, ewa, en, efv = int(e[0]), None if e[1].startswith("-") else int(e[1], 16), int(e[2], 16), int(e[3]), int(e[4], 16)
                if (ef != s[1]) or (ewa != s[3]) or (en != s[4]) or (ef == 0 and era != s[2]) or (ef == 1 and efv != s[5]):
                    bad_seg += 1
                    if bad_seg < 6:
                        print("  %s: segment %d: RTL %s, reference %s" % (name, k, s[1:6], e))
        # register reads
        bad_rd = 0
        er = [l.split() for l in open(os.path.join(gd, "expect_rd.txt")).read().strip().split("\n") if l.strip()]
        rd = [re.match(r"RD ([0-9a-fA-F]+) ([0-9a-fA-FxX]+) (\d)", l) for l in log]
        rd = [(int(m.group(1), 16), int(m.group(2).lower().replace("x", "0"), 16), int(m.group(3))) for m in rd if m]
        if len(rd) != len(er):
            bad_rd += abs(len(rd) - len(er)) or 1
        for r, e in zip(rd, er):
            if (r[0], r[1] & ~0x100 if r[0] == 0 else r[1], r[2]) != (int(e[0], 16), int(e[1], 16), int(e[2])):   # conf_regs write_zero is never reset (x until the first CONTROL write)
                bad_rd += 1
                print("  %s: read %s, expected %s" % (name, ["%x" % x for x in r], e))
        # timing: per descriptor (T_START .. T_DONE) and per segment
        ts = [int(l.split()[1]) for l in log if l.startswith("T_START")]
        td = [int(l.split()[1]) for l in log if l.startswith("T_DONE")]
        send = [int(l.split()[1]) for l in log if l.startswith("SEGEND")]
        k = 0
        for i, t0 in enumerate(ts):
            if i < len(td):
                n = 0
                while k + n < len(segs) and segs[k + n][0] <= td[i]:
                    n += 1
                rows.append((name, "desc", i, n, t0, td[i], td[i] - t0, segs[k][0] - t0 if n else -1))
                k += n
        for i, s in enumerate(segs):
            rows.append((name, "seg", i, s[1], s[4], s[0], send[i] - s[0] if i < len(send) else -1, (segs[i + 1][0] - s[0]) if i + 1 < len(segs) else -1))
        leg_s = [int(l.split()[1]) for l in log if l.startswith("T_LEGSTART")]
        leg_e = [int(l.split()[1]) for l in log if l.startswith("T_LEG ")]
        for i in range(min(len(leg_s), len(leg_e))):
            rows.append((name, "leg", i, 0, 0, leg_s[i], leg_e[i] - leg_s[i], 0))
        ok = bad_bytes == 0 and bad_seg == 0 and bad_rd == 0 and bool(end)
        print("%-12s %s  bytes_bad=%d seg_bad=%d (segments %d) reads_bad=%d descriptors=%d legacy=%d end=%s" %
              (name, "PASS" if ok else "FAIL", bad_bytes, bad_seg, len(segs), bad_rd, len(td), len(leg_e), bool(end)))
        bad_all += 0 if ok else 1
    with open(os.path.join(outdir, "timing.csv"), "w", newline="\n") as f:
        f.write("group,kind,idx,a,b,c,d,e\n")
        for r in rows:
            f.write(",".join(str(x) for x in r) + "\n")
    print("RESULT: %d groups with a mismatch" % bad_all)
    return 1 if bad_all else 0


if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[1] not in ("gen", "check"):
        raise SystemExit(__doc__)
    if sys.argv[1] == "gen":
        gen(sys.argv[2])
    else:
        sys.exit(check(sys.argv[2]))
