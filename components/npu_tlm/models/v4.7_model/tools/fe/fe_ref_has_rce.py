"""Golden-HAS for the RCE blocks, written from has/HAS_IFACE_RCE.md only (not from has/gvu_lut.h).

Done: §2 LUT indirect (recip / rsqrt), bit-exact integer model + vector files for an independent SystemC check.
Not yet: §3 SOFTMAX tier 3, §4 LAYERNORM, §5 FUSED_ATTN -- they wait for the answers R5-R10 (a guessed shift would only
give two sides agreeing on a guess).

Reading of §2 used here (Frontend's interpretation, to be confirmed in HAS_IFACE_RCE §6):
  E = 31 - clz(x); y = (x << (31 - E)) mod 2^32; i = y[30:23]; frac = y[22:15]
  recip: table T[0..256],             entries a = T[i],        b = T[i + 1]
  rsqrt: table T[p*257 + i], p = E&1  entries a = T[p*257 + i], b = T[p*257 + i + 1]   (the 9-bit index {E[0], y[30:23]}
         selects the half p; the 257 stride keeps i + 1 = 256 inside the same half)
  r = a + (((b - a) * frac) >> 8)      (arithmetic shift = floor, also for negative b - a)
  E_out = E (recip) | E >> 1 (rsqrt);  value = r * 2^-(Q + E_out)
  x = 0 is not modelled (R4 open).

Usage (repository root, FE_WORK set): python3 tools/fe/fe_ref_has_rce.py [--n 200000]  -> fe_work/has/vectors/lut_indirect_{recip,rsqrt}.txt
"""
import argparse
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fe_common as fc  # noqa: E402

Q = 14


def rnd(v):
    return int(math.floor(v + 0.5))


def table(func, q=Q):
    if func == "recip":
        return [rnd(2.0 ** q / (1.0 + i / 256.0)) for i in range(257)]
    if func == "rsqrt":
        return [rnd(2.0 ** q / math.sqrt((1.0 + i / 256.0) * 2.0 ** p)) for p in (0, 1) for i in range(257)]
    raise ValueError(func)


def lut_indirect(x, func, T):
    """Returns (r, E_out) for integer x in [1, 2^32)."""
    if not 0 < x < (1 << 32):
        raise ValueError("x outside [1, 2^32): %d" % x)
    E = x.bit_length() - 1
    y = (x << (31 - E)) & 0xFFFFFFFF
    i = (y >> 23) & 0xFF
    frac = (y >> 15) & 0xFF
    base = 0 if func == "recip" else (E & 1) * 257
    a, b = T[base + i], T[base + i + 1]
    d = b - a
    if not -256 <= d <= 255:
        raise OverflowError("table delta %d does not fit int9" % d)
    prod = d * frac
    if not -(1 << 16) <= prod < (1 << 16):
        raise OverflowError("product %d does not fit int17" % prod)
    r = a + (prod >> 8)
    return r, (E if func == "recip" else E >> 1)


def value(r, e_out, q=Q):
    return r * 2.0 ** -(q + e_out)


def cases(rng, n):
    xs = list(range(1, 4097))
    for k in range(32):
        for d in (-1, 0, 1):
            v = (1 << k) + d
            if 0 < v < (1 << 32):
                xs.append(v)
    xs += [(1 << 32) - 1, (1 << 31) - 1, 0x80008000, 0x7F800000, 0x00FF8001]
    while len(xs) < n:
        xs.append(max(1, int(2.0 ** rng.uniform(0, 32)) & 0xFFFFFFFF))
    return xs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, default=200000)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--out-dir", default=os.path.join(fc.FE_WORK, "has", "vectors"))
    args = ap.parse_args()
    os.makedirs(args.out_dir, exist_ok=True)
    for func in ("recip", "rsqrt"):
        T = table(func)
        rng = random.Random("%d/%s" % (args.seed, func))
        xs = cases(rng, args.n)
        worst, dmax, pmax = 0.0, 0, 0
        path = os.path.join(args.out_dir, "lut_indirect_%s.txt" % func)
        with open(path, "w") as f:
            f.write("# knobs func=%s q=%d\n# x r E\n" % (func, Q))
            for x in xs:
                r, e = lut_indirect(x, func, T)
                exact = 1.0 / x if func == "recip" else 1.0 / math.sqrt(x)
                worst = max(worst, abs(value(r, e) - exact) / exact)
                f.write("%d %d %d\n" % (x, r, e))
        base_n = 257 if func == "recip" else 514
        for h in range(0, base_n, 257):
            for i in range(256):
                d = T[h + i + 1] - T[h + i]
                dmax = max(dmax, abs(d))
                pmax = max(pmax, abs(d) * 255)
        print("[rce] %-24s %d lines, max rel err %.3e, max |table delta| %d, max |product| %d"
              % (os.path.basename(path), len(xs), worst, dmax, pmax))


if __name__ == "__main__":
    main()
