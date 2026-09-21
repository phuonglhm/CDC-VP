#!/usr/bin/env python3
# Dump SAURIA controller_args (ground truth) for a shape, plus the resolved tile
# descriptor, so the C encoder (libsauria_cfg) can be checked bit-exact.
# Usage: SAURIA_PY=... python3 tools/dump_cfg.py "Bw Bh d s Cin Cw Ch Cout Xused Yused preload" VERSION
import sys, os
SAURIA_PY = os.environ.get('SAURIA_PY', os.path.expanduser('~/Desktop/sauria/Python'))
sys.path.insert(0, SAURIA_PY); os.chdir(SAURIA_PY)
import numpy as np
import src.hw_versions as hw
import src.sauria_lib as slib
import src.config_helper as cfg
import src.execution_model as ex

a = sys.argv[1].split()
Bw, Bh, d, s = int(a[0]), int(a[1]), int(a[2]), int(a[3])
Cin, Cw, Ch, Cout = int(a[4]), int(a[5]), int(a[6]), int(a[7])
Xused, Yused = int(a[8]), int(a[9])
preload = bool(int(a[10]))
version = sys.argv[2]
H = hw.get_params(version)

Aw = (1 + s*(Cw-1)) + (1 + d*(Bw-1)) - 1
Ah = (1 + s*(Ch-1)) + (1 + d*(Bh-1)) - 1
tensor_shapes = [[Cin, Ah, Aw], [Cout, Cin, Bh, Bw], [Cout, Ch, Cw]]
TILING = {'C_tile_shape': [Cout, Ch, Cw], 'tile_cin': Cin, 'X_used': Xused, 'Y_used': Yused}
CONV = slib.get_conv_dict(tensor_shapes, TILING, H, d=d, s=s, preloads=preload)

sauria_regs, N_REGS = cfg.get_sauria_regs(CONV, H, silent=True)
_, _, _, loop_order = ex.get_tiling_loops(CONV)
# args[22..] (core config) does not depend on DRAM offsets; use dummies.
args = cfg.get_controller_regs(CONV, sauria_regs, N_REGS, [0, 0, 0, 0], loop_order, silent=True)

print("VERSION", version)
print("DESC", Bw, Bh, d, s, CONV['c_til'], CONV['k_til'], CONV['h_til'], CONV['w_til'],
      Xused, Yused, int(preload))
print("FULL", CONV['C_w'], CONV['C_h'], CONV['C_c'], CONV['A_c'])
print("START 22")
print("ARGS", len(args), " ".join("%08x" % (int(x) & 0xFFFFFFFF) for x in args))
