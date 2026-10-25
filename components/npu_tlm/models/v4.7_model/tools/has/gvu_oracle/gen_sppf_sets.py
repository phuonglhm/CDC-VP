# gen_sppf_sets.py -- writes the SPPF max pool vector sets of YOLOv8m in the layout of the hardware sets target/test/maxpool
# (input.txt [C][H][W], golden_output.txt [C][OH][OW], int8, one value per line) plus params.txt per set.
# The RTL has no padding: input.txt holds the tensor as the data flow controller puts it into vbank0, already padded by 2
# with -128 (24 x 24 for the 20 x 20 SPPF tensor). Sets:
#   mp1, mp2, mp3 : the three chained 5x5 s1 p2 max pools of the YOLOv8m SPPF block (instructions 50, 51, 52 of the program),
#                   288 channels, inputs and reference outputs taken from the model's reference memory image (frontend reference,
#                   independent of the vector unit); the script checks that each reference equals a plain max pool of the
#                   padded input and that mp2 / mp3 take the previous output.
#   rand          : the same shape, seed 2026: even channels uniform int8 in [-128, 127]; odd channels uniform in [-128, -100]
#                   (every window negative, so a 0 read from an unwritten lane or a wrong padding shows); channel 1 all -128
#                   (every output -128); reference = plain max pool.
# usage: python -I gen_sppf_sets.py <dump dir with mp50/51/52_{in,out}.txt> <output dir>
import os, random, sys

C, H, W, K, S, P = 288, 20, 20, 5, 1, 2
HP, WP = H + 2 * P, W + 2 * P
OH, OW = (HP - K) // S + 1, (WP - K) // S + 1
GROUP = 14            # channels per run on the instruction path of the model (21 runs: 20 x 14 + 8)
LANES = 32

def pad(x):
    o = [-128] * (C * HP * WP)
    for c in range(C):
        for y in range(H):
            for xx in range(W):
                o[(c * HP + y + P) * WP + xx + P] = x[(c * H + y) * W + xx]
    return o

def maxpool(xp):
    o = []
    for c in range(C):
        for y in range(OH):
            for xx in range(OW):
                o.append(max(xp[(c * HP + y * S + dy) * WP + xx * S + dx] for dy in range(K) for dx in range(K)))
    return o

def regs(n):
    wt = (WP + LANES - 1) // LANES
    r7 = (K - 2) | S << 2 | wt << 8 | n << 16 | OH << 24
    r8 = (wt * S) | (wt * HP) << 10
    return wt, r7, r8

def write_set(d, name, xp, gold, note):
    os.makedirs(d, exist_ok=True)
    open(os.path.join(d, 'input.txt'), 'w').write('\n'.join(map(str, xp)) + '\n')
    open(os.path.join(d, 'golden_output.txt'), 'w').write('\n'.join(map(str, gold)) + '\n')
    wt, _, r8 = regs(GROUP)
    lines = [f'set sppf/k5_p2/{name}: {note}',
             f'input.txt  [C][H][W] = [{C}][{HP}][{WP}] int8 (tensor {H} x {W} padded by {P} with -128 on every side)',
             f'golden_output.txt [C][OH][OW] = [{C}][{OH}][{OW}] int8',
             f'window {K} x {K} (POOLING mode {K - 2}), stride {S}, W tiles {wt}, OH {OH}',
             f'vbank0 layout: row c * {HP * wt} + h * {wt} + w_tile, lane = w mod {LANES}; vbank2 port 1: row c * {OH * wt} + oh * {wt} + ow / {LANES}',
             f'channels per run: {GROUP} as on the instruction path of the model ({-(-C // GROUP)} runs, the last one {C - (C // GROUP) * GROUP or GROUP}); '
             f'the largest that fits: min(800 / ({HP} * {wt}), 1024 / ({OH} * {wt})) = {min(800 // (HP * wt), 1024 // (OH * wt))}',
             'register values of one run (Reg 7 POOLING_0, Reg 8 POOLING_1), then Reg 0 = (7 << 3) | 1 = 0x39; after done Reg 0 = 0x4:']
    for n in sorted({GROUP, C - (C // GROUP) * GROUP or GROUP, min(800 // (HP * wt), 1024 // (OH * wt))}):
        _, r7, r8 = regs(n)
        lines.append(f'  C = {n:3d}: Reg 7 = 0x{r7:08X}  (mode {K - 2}, stride {S}, W tiles {wt}, C {n}, OH {OH}),  Reg 8 = 0x{r8:08X}  (h move {wt * S}, c move {wt * HP})')
    open(os.path.join(d, 'params.txt'), 'w').write('\n'.join(lines) + '\n')

def main():
    dump, out = sys.argv[1], sys.argv[2]
    prev = None
    for k, i in enumerate((50, 51, 52)):
        x = [int(v) for v in open(os.path.join(dump, f'mp{i}_in.txt')).read().split()]
        g = [int(v) for v in open(os.path.join(dump, f'mp{i}_out.txt')).read().split()]
        assert len(x) == C * H * W and len(g) == C * OH * OW
        if prev is not None:
            assert x == prev, f'mp{i}: input is not the previous output'
        xp = pad(x)
        assert maxpool(xp) == g, f'mp{i}: reference differs from a plain max pool'
        write_set(os.path.join(out, f'mp{k + 1}'), f'mp{k + 1}', xp, g,
                  f'YOLOv8m SPPF max pool {k + 1} of 3 (program instruction {i}), real activations, reference = model reference image = plain max pool')
        print(f'mp{k + 1}: instruction {i}, {len(xp)} input values, {len(g)} outputs, reference = plain max pool: yes, input min {min(x)} max {max(x)}')
        prev = g
    rng = random.Random(2026)
    x = []
    for c in range(C):
        lo, hi = (-128, 127) if c % 2 == 0 else (-128, -100)
        x += [-128] * (H * W) if c == 1 else [rng.randint(lo, hi) for _ in range(H * W)]
    xp = pad(x)
    g = maxpool(xp)
    write_set(os.path.join(out, 'rand'), 'rand', xp, g, 'SPPF shape, seed 2026: even channels uniform in [-128, 127], odd channels in [-128, -100], channel 1 all -128; reference = plain max pool')
    print(f'rand: {len(xp)} input values, {len(g)} outputs, -128 in input {x.count(-128)}, outputs equal to -128 {g.count(-128)}')

if __name__ == '__main__':
    main()
