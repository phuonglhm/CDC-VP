# chk_maxpool_runs.py -- rebuilds vbank2 port 1 of every run of a max pool trace (tb_oracle_maxpool_runs.sv or the model's
# tb_gvu_top_maxpool_runs; lines "T n inpool v0en v0addr v2wen v2addr mask data", one "DONE n" per run) and compares run r with
# channels r*group .. of golden_output.txt ([C][OH][OW] int8, one value per line).
# Prints one line per run with a wrong or missing value, then a total line "GOLDEN runs .. values .. wrong .. not written ..".
# usage: python -I chk_maxpool_runs.py <trace> <golden_output.txt> <H> <W> <k> <s> <C> [group, default C]
import sys

def out_lanes(k, s):
    return 32 if s == 1 else ((32 - k) // s + 1 if k == s else (32 - k) // s + 2)

def main():
    trace, gold = sys.argv[1], sys.argv[2]
    h, w, k, s, c = map(int, sys.argv[3:8])
    group = int(sys.argv[8]) if len(sys.argv) > 8 else c
    oh, ow = (h - k) // s + 1, (w - k) // s + 1
    wt = (w + 31) // 32
    ol = out_lanes(k, s)
    g = [int(x) for x in open(gold).read().split()]
    runs, mem, dones, timeout = [], {}, [], False
    for line in open(trace):
        f = line.split()
        if not f:
            continue
        if f[0] == 'DONE':
            runs.append(mem); mem = {}; dones.append(int(f[1])); continue
        if f[0] == 'TIMEOUT':
            timeout = True; continue
        if f[0] != 'T' or len(f) < 9 or f[5] != '1':
            continue
        addr, mask, data = int(f[6]), int(f[7], 16), int(f[8], 16)
        row = mem.setdefault(addr, [None] * 32)
        for l in range(32):
            if mask >> l & 1:
                b = (data >> (8 * l)) & 0xFF
                row[l] = b - 256 if b > 127 else b
    nruns = (c + group - 1) // group
    tot_bad = tot_miss = tot_val = 0
    for r in range(nruns):
        c0 = r * group
        nc = min(group, c - c0)
        m = runs[r] if r < len(runs) else {}
        bad = miss = 0
        for cc in range(nc):
            for y in range(oh):
                for x in range(ow):
                    a = (cc * oh + y) * wt + x // ol
                    v = m.get(a, [None] * 32)[x % ol]
                    e = g[((c0 + cc) * oh + y) * ow + x]
                    if v is None:
                        miss += 1
                    elif v != e:
                        bad += 1
        tot_bad += bad; tot_miss += miss; tot_val += nc * oh * ow
        if bad or miss or r >= len(runs):
            print(f"  run {r + 1} channels {c0}..{c0 + nc - 1}: done {dones[r] if r < len(dones) else 'none'} wrong {bad} not written {miss}")
    last = dones[-1] if dones else 'none'
    print(f"GOLDEN runs {len(runs)} / {nruns} values {tot_val} wrong {tot_bad} not written {tot_miss} last done {last}{' TIMEOUT' if timeout else ''}")
    sys.exit(0 if (len(runs) == nruns and tot_bad == 0 and tot_miss == 0 and not timeout) else 1)

if __name__ == '__main__':
    main()
