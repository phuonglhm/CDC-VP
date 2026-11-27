#!/usr/bin/env python3
"""Expected AXI4-Stream beats of the CSI-2 RX sample sessions (direct CSI->ISP, D0).

Independent of the model: the pixels follow the frozen TPG definition
(spec_decisions C35 / D11, MAS v1.4 TPG colour bars) and the harness's own
sensor pattern; the beat packing follows the RAW16 stream format of the
contract (docs/DIRECT_CSI_ISP_CONTRACT.md §3, §5): 4 samples per 64-bit beat,
sample n in bits [16n+15:16n], little endian, the line's last beat carrying
TKEEP = (1 << 2*samples) - 1.

  gen_expected.py [--out DIR]     writes one CSV per session into DIR
  gen_expected.py --check DIR     exit 1 if DIR differs from what this script writes

CSV columns: line,beat,tdata,tkeep,tlast,tuser,tid (tdata hex, 16 digits).
Only the payload beats of a valid frame; abort beats and stalls are not part of
the expected payload (they are checked by the harness itself).
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

# TPG colour bars (MAS v1.4 Table 6-25, C35): band index -> (R, G, B) on/off.
# band = number of thresholds (k+1)*base, k = 0..6, that the coordinate reaches;
# R on for bands 0,1,4,5 (!bit1), G on for bands 0..3 (!bit2), B on for even
# bands (!bit0). CFA RGGB: even row/even col = R, odd row/odd col = B, else G.


def tpg_pixel(x, y, width, bits):
    """Vertical bars (TPG_CONTROL.pattern = 0): band from the column."""
    base = (width & ~3) >> 3
    band = sum(1 for k in range(7) if x >= (k + 1) * base)
    r_on, g_on, b_on = not (band >> 1 & 1), not (band >> 2 & 1), not (band & 1)
    odd_col, odd_row = x & 1, y & 1
    on = r_on if (not odd_col and not odd_row) else b_on if (odd_col and odd_row) else g_on
    v = 0xFFF if on else 0
    return v >> 2 if bits == 10 else v


def sensor_pixel(x, y, bits, frame=1):
    """The harness BFM's sensor pattern (same formula as the VP camera sensor)."""
    return (x * 37 + y * 101 + frame * 53 + 5) & ((1 << bits) - 1)


def beats(width, height, pixel):
    rows = []
    for y in range(height):
        line = [pixel(x, y) for x in range(width)]
        nb = (width + 3) // 4
        for b in range(nb):
            chunk = line[4 * b:4 * b + 4]
            data = 0
            for n, s in enumerate(chunk):
                data |= (s & 0xFFFF) << (16 * n)
            keep = (1 << (2 * len(chunk))) - 1
            last = int(b == nb - 1)
            user = int(y == 0 and b == 0)  # TUSER[0] = SOF on the first beat
            rows.append((y, b, "%016x" % data, "0x%02x" % keep, last, user, 0))
    return rows


SESSIONS = {
    # TPG width & ~3 is used by the generator: 32 -> 32 (no tail by construction)
    "tpg_raw12_32x8": (32, 8, lambda x, y: tpg_pixel(x, y, 32, 12)),
    "tpg_raw10_32x8": (32, 8, lambda x, y: tpg_pixel(x, y, 32, 10)),
    # Sensor BFM through the D-PHY: RAW12 34 wide -> a 2-sample tail beat (TKEEP 0x0f)
    "bfm_raw12_34x8": (34, 8, lambda x, y: sensor_pixel(x, y, 12)),
    "bfm_raw10_32x8": (32, 8, lambda x, y: sensor_pixel(x, y, 10)),
}


def render(name):
    w, h, px = SESSIONS[name]
    out = ["line,beat,tdata,tkeep,tlast,tuser,tid"]
    out += ["%d,%d,%s,%s,%d,%d,%d" % r for r in beats(w, h, px)]
    return "\n".join(out) + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", default=os.path.join(HERE, "expected"))
    ap.add_argument("--check", metavar="DIR")
    a = ap.parse_args()
    d = a.check or a.out
    bad = 0
    for name in SESSIONS:
        p = os.path.join(d, name + ".csv")
        text = render(name)
        if a.check:
            if not os.path.exists(p) or open(p).read() != text:
                print("out of date:", p)
                bad = 1
        else:
            os.makedirs(d, exist_ok=True)
            open(p, "w").write(text)
            print("wrote", p)
    return bad


if __name__ == "__main__":
    sys.exit(main())
