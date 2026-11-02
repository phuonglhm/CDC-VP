# ref_avgpool.py -- independent reference of AVG_POOL as the GvuTop datapath will compute it, written from the MAS drawing
# "Avg_Pool" and the RTL round-shift unit (FUs/gvu_vroundshift.sv: floor shift + increment rule per mode), not from the C++ model.
#   sum16 = window sum, int16 two's-complement wrap; p = sum16 * Avg_Scale (int32 x int16, exact);
#   y = narrow8(roundshift(p, Avg_Shift, mode)), narrow8 = saturate (default) or wrap (RTL as delivered).
# Usage: ref_avgpool.py vectors <dir with avgpool/{tile,vec}>  |  ref_avgpool.py gate <gate dir written by make_avgpool_gate.py>
import sys, os
MODES = ['FLOOR', 'CEIL', 'TRUNC', 'AWAY', 'NEAR_UP', 'NEAR_AWAY', 'NEAR_EVEN']

def wrap(v, bits):
    m = 1 << bits; v &= m - 1
    return v - m if v >> (bits - 1) else v

def roundshift(p, sh, mode):
    if sh == 0: return p
    q = p >> sh; rem = p - (q << sh); half = 1 << (sh - 1); neg = p < 0
    inc = {'FLOOR': False, 'CEIL': rem != 0, 'TRUNC': neg and rem != 0, 'AWAY': (not neg) and rem != 0,
           'NEAR_UP': rem >= half, 'NEAR_AWAY': rem > half or (rem == half and not neg),
           'NEAR_EVEN': rem > half or (rem == half and (q & 1) == 1)}[mode]
    return q + (1 if inc else 0)

def narrow8(v, sat=True):
    return max(-128, min(127, v)) if sat else wrap(v, 8)

def avgpool(x, C, H, W, k, s, S, sh, mode='NEAR_UP', sat=True):
    Ho, Wo = (H - k) // s + 1, (W - k) // s + 1
    out, sums, ovf = [], [], 0
    for c in range(C):
        for y in range(Ho):
            for xx in range(Wo):
                t = sum(x[(c * H + y * s + i) * W + xx * s + j] for i in range(k) for j in range(k))
                s16 = wrap(t, 16); ovf += s16 != t
                sums.append(s16); out.append(narrow8(roundshift(s16 * S, sh, mode), sat))
    return out, sums, ovf

if __name__ == '__main__':
    what, d = sys.argv[1], sys.argv[2]
    if what == 'vectors':
        rd = lambda p: [int(v) for v in open(p).read().split()]
        for m, (C, H, W, k, S) in {'tile': (3, 80, 80, 5, 2621), 'vec': (40, 20, 20, 3, 7282)}.items():
            x = rd(f'{d}/avgpool/{m}/input.txt'); g = rd(f'{d}/avgpool/{m}/golden_output.txt'); gs = rd(f'{d}/avgpool/{m}/golden_sum.txt')
            for mode in MODES:
                o, s16, ovf = avgpool(x, C, H, W, k, 1, S, 16, mode)
                print(f'A0 vectors {m} mode {mode}: outputs {len(o)} / golden {len(g)}, out mismatches {sum(a != b for a, b in zip(o, g))}, '
                      f'sum mismatches {sum(a != b for a, b in zip(s16, gs))}, int16 wraps {ovf}')
    else:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        init = open(f'{d}/dram_init.bin', 'rb').read(); gold = open(f'{d}/dram_golden.bin', 'rb').read()
        regs, cases = {}, []
        for line in open(f'{d}/mmio.txt'):
            p = line.split()
            if not p or p[0] != 'W': continue
            a, v = int(p[1], 16), int(p[2], 16); regs[a] = v
            if a == 0x40000310:
                E = 0x4000046C
                cases.append(dict(A=regs[0x40000444], O=regs[0x40000408], s=regs[0x40000424], C=regs[E], H=regs[E + 4], W=regs[E + 8],
                                  k=regs[E + 4 * 23], S=wrap(regs[E + 4 * 21], 32), sh=regs[E + 4 * 22]))
        tot = 0
        for i, c in enumerate(cases):
            n = c['C'] * c['H'] * c['W']
            x = [wrap(b, 8) for b in init[c['A']:c['A'] + n]]
            o, _, ovf = avgpool(x, c['C'], c['H'], c['W'], c['k'], c['s'], c['S'], c['sh'], 'NEAR_UP')
            g = [wrap(b, 8) for b in gold[c['O']:c['O'] + len(o)]]
            bad = sum(a != b for a, b in zip(o, g)); tot += bad
            sat = sum(1 for v in o if v in (-128, 127))
            ow, _, _ = avgpool(x, c['C'], c['H'], c['W'], c['k'], c['s'], c['S'], c['sh'], 'NEAR_AWAY', sat=False)
            print(f'A0 gate case {i}: k {c["k"]} s {c["s"]} [{c["C"]},{c["H"]},{c["W"]}] S {c["S"]} sh {c["sh"]}: outputs {len(o)}, mismatches {bad}, '
                  f'int16 wraps {ovf}, values at +-limit {sat}; as RTL (near-away, wrap) differs at {sum(a != b for a, b in zip(ow, g))}')
        print(f'A0 gate: {len(cases)} cases, total mismatches {tot}')
