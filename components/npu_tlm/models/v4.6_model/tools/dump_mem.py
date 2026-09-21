#!/usr/bin/env python3
# Dump raw tensors + the SAURIA initial DRAM image for a shape, so the C packer
# (libsauria_mem) can be checked bit-exact.
# Usage: SAURIA_PY=... python3 tools/dump_mem.py "Bw Bh d s Cin Cw Ch Cout Xused Yused preload" VERSION
import sys, os
SAURIA_PY = os.environ.get('SAURIA_PY', os.path.expanduser('~/Desktop/sauria/Python'))
sys.path.insert(0, SAURIA_PY); os.chdir(SAURIA_PY)
import numpy as np
import src.hw_versions as hw
import src.sauria_lib as slib
import src.data_helper as dh

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

A, B, Cpre = dh.generate_tensors(CONV, H)
if not preload:
    Cpre[:] = 0
# Golden C output is not needed for the input-DRAM check; pass zeros of C shape.
Cgold = np.zeros_like(Cpre)
B_opt = dh.optimize_weight_tensor_shape(B, CONV)
DRAM, DRAMg, offs = dh.assign_dram_values(A, B_opt, Cpre, Cgold, 0, CONV, H)

_FP = (H['OP_TYPE'] == 1)
def flat(x):
    a = np.asarray(x).flatten()
    if _FP:
        return " ".join(repr(float(v)) for v in a)   # exact half values as floats
    return " ".join(str(int(v)) for v in a)

print("VERSION", version)
print("DIMS", A.shape[0], A.shape[1], A.shape[2],   # A_c A_h A_w
      Cout, Cin, Bh, Bw,                             # C_out C_in B_h B_w
      Cpre.shape[0], Cpre.shape[1], Cpre.shape[2],   # C_c C_h C_w
      CONV['c_til'], CONV['k_til'])                  # c_til k_til
print("OFFS", offs[0], offs[1], offs[2])
print("A", A.size, flat(A))
print("B", B.size, flat(B))     # original [C_out,C_in,B_h,B_w]
print("C", Cpre.size, flat(Cpre))
print("DRAM", DRAM.size, " ".join("%02x" % int(v) for v in DRAM))
