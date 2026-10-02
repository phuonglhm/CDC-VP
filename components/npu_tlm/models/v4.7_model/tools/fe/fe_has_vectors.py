"""Independent unit-test vectors for has/gvu_quant.h.

Written from has/HAS_IFACE.md §3-§4 and §8 only (not from the SystemC code). Arithmetic uses Python
ints (unbounded), every narrowing is explicit, as §1 requires.
Files (fe_work/has/vectors/):
  requant_<mode>.txt   # knobs ...  /  # x S s zp out sat16 clamp8
  dequant_<mode>.txt   # knobs ...  /  # x zp S s out
S is written as the raw 32-bit word the host writes (0 .. 2^32-1); the reader decodes it by scale_fmt.
Usage (repository root, FE_WORK set): python3 tools/fe/fe_has_vectors.py [--n 120000] [--seed 1]
"""
import argparse
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fe_common as fc  # noqa: E402

FLOOR, HALF_UP, HALF_AWAY, HALF_EVEN = 0, 1, 2, 3
SAT16, WRAP16, NONE = 0, 1, 2
SUB_BEFORE, ADD_AFTER = 0, 1
I32, U32 = 0, 1

# mode -> (round, narrow, deq_zp, scale_fmt); compat = pre-HAS int8 flow, has = HAS defaults, then one knob changed per variant
MODES = {
    "compat": (FLOOR, SAT16, SUB_BEFORE, U32),
    "has": (HALF_UP, SAT16, SUB_BEFORE, I32),
    "has_round2": (HALF_AWAY, SAT16, SUB_BEFORE, I32),
    "has_round3": (HALF_EVEN, SAT16, SUB_BEFORE, I32),
    "has_round0": (FLOOR, SAT16, SUB_BEFORE, I32),
    "has_narrow1": (HALF_UP, WRAP16, SUB_BEFORE, I32),
    "has_narrow2": (HALF_UP, NONE, SUB_BEFORE, I32),
    "has_deq1": (HALF_UP, SAT16, ADD_AFTER, I32),
}


def sat(v, lo, hi):
    return lo if v < lo else hi if v > hi else v


def wrap16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def rshift(p, s, mode):
    if s < 0 or s > 63:
        raise ValueError("shift %d outside [0, 63]" % s)
    if s == 0:
        return p
    half = 1 << (s - 1)
    if mode == FLOOR:
        return p >> s
    if mode == HALF_UP:
        return (p + half) >> s
    if mode == HALF_AWAY:
        return (1 if p >= 0 else -1) * ((abs(p) + half) >> s)
    if mode == HALF_EVEN:
        q = p >> s
        rem = p - (q << s)
        if rem > half or (rem == half and (q & 1)):
            q += 1
        return q
    raise ValueError(mode)


def decode_s(raw, fmt):
    return raw - (1 << 32) if fmt == I32 and raw >= (1 << 31) else raw


def requant(x, raw_s, s, zp, rnd, narrow, fmt):
    p = x * decode_s(raw_s, fmt)
    if abs(p) >= (1 << 63):
        raise OverflowError("OVF64 x=%d S=%d" % (x, raw_s))
    r = rshift(p, s, rnd)
    out16 = not (-32768 <= r <= 32767)
    if narrow == SAT16:
        n = sat(r, -32768, 32767)
    elif narrow == WRAP16:
        n = wrap16(r)
    else:
        n = r
    y = n + zp
    clamp = not (-128 <= y <= 127)
    return sat(y, -128, 127), int(out16 and narrow != NONE), int(clamp)


def dequant(x, zp, raw_s, s, rnd, order, fmt):
    S = decode_s(raw_s, fmt)
    if order == SUB_BEFORE:
        v = rshift((x - zp) * S, s, rnd)
    else:
        v = rshift(x * S, s, rnd) + zp
    return sat(v, -(1 << 31), (1 << 31) - 1)


I32_EDGE = [-(1 << 31), -(1 << 31) + 1, -65536, -32769, -32768, -129, -128, -1, 0, 1, 127, 128, 32767, 32768,
            65535, (1 << 31) - 2, (1 << 31) - 1]
S_EDGE = [0, 1, 2, (1 << 30) - 1, 1 << 30, (1 << 31) - 1, 1 << 31, (1 << 31) + 1, (1 << 32) - 2, (1 << 32) - 1]
SH_EDGE = [0, 1, 31, 62, 63]


def req_cases(rng, n, zero_zp):
    out = []
    zps = [0] if zero_zp else [0, 0, -32768, -1, 1, 127, 32767]
    for x in I32_EDGE:
        for S in S_EDGE:
            for s in SH_EDGE:
                out.append((x, S, s, rng.choice(zps)))
    # exact halves: p = k*2^s + 2^(s-1) with S = 1, both signs, and one below/above
    for s in range(1, 31):
        for k in (-3, -2, -1, 0, 1, 2, 3, rng.randrange(-100, 100)):
            base = k * (1 << s) + (1 << (s - 1))
            for d in (-1, 0, 1):
                x = base + d
                if -(1 << 31) <= x < (1 << 31):
                    out.append((x, 1, s, 0))
    # realistic YOLOv8m-like: S in [2^30, 2^31) or [2^31, 2^32), s in [36, 45], psum+bias moderate
    while len(out) < n:
        kind = rng.random()
        if kind < 0.4:
            x = rng.randrange(-(1 << 22), 1 << 22)
            S = rng.randrange(1 << 30, 1 << 32)
            s = rng.randrange(36, 46)
        elif kind < 0.7:
            x = rng.randrange(-(1 << 31), 1 << 31)
            S = rng.randrange(0, 1 << 32)
            s = rng.randrange(0, 64)
        else:
            x = rng.randrange(-(1 << 16), 1 << 16)
            S = rng.randrange(0, 1 << 32)
            s = rng.randrange(28, 50)
        out.append((x, S, s, 0 if zero_zp or rng.random() < 0.5 else rng.randrange(-32768, 32768)))
    return out


def deq_cases(rng, n, zero_zp):
    out = []
    zps = [0] if zero_zp else [0, -32768, -128, -1, 1, 127, 32767]
    for x in (-128, -127, -1, 0, 1, 127):
        for S in S_EDGE:
            for s in SH_EDGE:
                for zp in zps:
                    out.append((x, zp, S, s))
    while len(out) < n:
        out.append((rng.randrange(-128, 128), 0 if zero_zp else rng.randrange(-32768, 32768),
                    rng.randrange(0, 1 << 32), rng.randrange(0, 64)))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, default=120000)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--out-dir", default=os.path.join(fc.FE_WORK, "has", "vectors"))
    args = ap.parse_args()
    os.makedirs(args.out_dir, exist_ok=True)
    for mode, (rnd, narrow, order, fmt) in MODES.items():
        zero_zp = mode == "compat"
        knobs = "# knobs round=%d narrow=%d deq_zp=%d scale_fmt=%d\n" % (rnd, narrow, order, fmt)
        rng = random.Random("%d/%s/req" % (args.seed, mode))
        cnt = [0, 0, 0]
        path = os.path.join(args.out_dir, "requant_%s.txt" % mode)
        with open(path, "w") as f:
            f.write(knobs + "# x S s zp out sat16 clamp8\n")
            for x, S, s, zp in req_cases(rng, args.n, zero_zp):
                o, f16, f8 = requant(x, S, s, zp, rnd, narrow, fmt)
                cnt[0] += 1
                cnt[1] += f16
                cnt[2] += f8
                f.write("%d %d %d %d %d %d %d\n" % (x, S, s, zp, o, f16, f8))
        print("[vectors] %-26s %d lines, sat16 %d, clamp8 %d" % (os.path.basename(path), cnt[0], cnt[1], cnt[2]))
        rng = random.Random("%d/%s/deq" % (args.seed, mode))
        path = os.path.join(args.out_dir, "dequant_%s.txt" % mode)
        with open(path, "w") as f:
            f.write(knobs + "# x zp S s out\n")
            cases = deq_cases(rng, args.n, zero_zp)
            for x, zp, S, s in cases:
                f.write("%d %d %d %d %d\n" % (x, zp, S, s, dequant(x, zp, S, s, rnd, order, fmt)))
        print("[vectors] %-26s %d lines" % (os.path.basename(path), len(cases)))


if __name__ == "__main__":
    main()
