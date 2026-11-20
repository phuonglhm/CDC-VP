#!/usr/bin/env python3
"""Small programs whose GEMM_FUSED tiles are full 32 x 32 (32 output channels, Y_USED 32 positions per context),
2 / 3 / 4 contexts per tile, for the vertical scan through HasNpuTop. Uses tools/has/make_pad_gate.py unchanged (its case list is replaced).
usage: python make_vgate.py <tree dir> <out dir> [zerobias | nonsq]      (then tools/has/make_tmaj_gate.py <out dir> <tmaj out dir>)
zerobias: the bias values are zero (preload 0). nonsq: one layer whose tile is NOT square (16 channels, Cin 64 in 2 passes), the tile type the vertical
scan refuses on the clocked path."""
import os
import sys

tree, out = sys.argv[1], sys.argv[2]
sys.path.insert(0, os.path.join(tree, "tools", "has"))
import make_pad_gate as m  # noqa: E402

# (name, cin, h, w, cout, kh, kw, stride, pads, pad value, tile (cout, th, tw), extra regs)
m.CASES = [
    ("1x1 32 channels, 2 contexts per tile (32 x 1 x 64), 2 tiles", 8, 2, 64, 32, 1, 1, 1, 0, None, (32, 1, 64), {"Y_USED": 32}),
    ("1x1 32 channels, 3 contexts per tile (32 x 1 x 96), 2 tiles", 8, 2, 96, 32, 1, 1, 1, 0, None, (32, 1, 96), {"Y_USED": 32}),
    ("1x1 32 channels, 4 contexts per tile (32 x 1 x 128), 2 tiles", 8, 2, 128, 32, 1, 1, 1, 0, None, (32, 1, 128), {"Y_USED": 32}),
    ("3x3 32 channels, 4 contexts per tile (32 x 1 x 128), 2 tiles", 4, 4, 130, 32, 3, 3, 1, 0, None, (32, 1, 128), {"Y_USED": 32}),
]
if len(sys.argv) > 3 and sys.argv[3] == "nonsq":
    m.CASES = [("1x1 Cin 64 split in 2 (TILE_CIN 32), 16 channels, non-square tile (16 x 1 x 32)", 64, 2, 32, 16, 1, 1, 1, 0, None, (16, 1, 32), {"Y_USED": 32, "TILE_CIN": 32})]
if len(sys.argv) > 3 and sys.argv[3] == "zerobias":
    # same random stream, the bias values (the only draw in [-2000, 2000]) are zeroed: the preload of the core is 0
    _real = m.np.random.default_rng

    class _Z:
        def __init__(self, g):
            self.g = g

        def integers(self, lo, hi=None, size=None, dtype=None):
            v = self.g.integers(lo, hi, size=size, **({} if dtype is None else {"dtype": dtype}))
            return v * 0 if (lo, hi) == (-2000, 2001) else v

    m.np.random.default_rng = lambda seed=None: _Z(_real(seed))
sys.argv = ["make_pad_gate.py", out, "--set", "fixed"]
m.main()
