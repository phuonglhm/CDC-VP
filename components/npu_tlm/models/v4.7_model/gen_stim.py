#!/usr/bin/env python3
# Generate SystemC-consumable SAURIA stimuli (GoldenStimuli/initial_dram/gold_dram/tstcfg)
# for ONE conv/GeMM shape, by calling the BSC pipeline up to fh.generate_test_files
# (i.e. Conv2d_SAURIA minus the Verilator run). Output goes to <test_dir>/stimuli/.
#
# Usage: python3 gen_stim.py Bw Bh d s Cin Cw Ch Cout Xused Yused preload test_dir
# Example (1x1 MVM, Cin=64, out 8x1, Cout=16):
#   python3 gen_stim.py 1 1 1 1 64 8 1 16 16 8 1 ~/gen_test

import sys, os
SAURIA_PY = os.environ.get('SAURIA_PY', os.path.expanduser('~/Desktop/sauria/Python'))
sys.path.insert(0, SAURIA_PY)
os.chdir(SAURIA_PY)

import numpy as np

# Fixed seed so a captured demo case (stimuli/gold_dram) is reproducible on
# regeneration -- floating-point (FP16) accumulation is not associative, so
# different random draws can land on different last-bit rounding ties.
np.random.seed(117)
import src.hw_versions as hw
import src.sauria_lib as slib
import src.data_helper as dh
import src.config_helper as cfg
import src.execution_model as ex
import src.file_helper as fh

a = sys.argv
Bw, Bh, d, s = int(a[1]), int(a[2]), int(a[3]), int(a[4])
Cin, Cw, Ch, Cout = int(a[5]), int(a[6]), int(a[7]), int(a[8])
Xused, Yused = int(a[9]), int(a[10])
preload = bool(int(a[11]))
test_dir = os.path.expanduser(a[12]) if len(a) > 12 else os.path.expanduser('~/gen_test')

version = os.environ.get('SAURIA_VERSION', 'int8_8x16')
HOPTS = hw.get_params(version)

Aw = (1 + s * (Cw - 1)) + (1 + d * (Bw - 1)) - 1
Ah = (1 + s * (Ch - 1)) + (1 + d * (Bh - 1)) - 1
tensor_shapes = [[Cin, Ah, Aw], [Cout, Cin, Bh, Bw], [Cout, Ch, Cw]]
# single tile (no external tiling) by default
TILING_DICT = {'C_tile_shape': [Cout, Ch, Cw], 'tile_cin': Cin, 'X_used': Xused, 'Y_used': Yused}

print('[gen] version=%s shapes A=%s B=%s C=%s d=%d s=%d' %
      (version, tensor_shapes[0], tensor_shapes[1], tensor_shapes[2], d, s))

CONV_DICT = slib.get_conv_dict(tensor_shapes, TILING_DICT, HOPTS, d=d, s=s, preloads=preload)
print('[gen] CONV_DICT ok')

A, B, Cpre = dh.generate_tensors(CONV_DICT, HOPTS)
if not preload:
    Cpre[:] = 0
print('[gen] tensors ok  A%s B%s' % (np.shape(A), np.shape(B)))

sauria_regs, N_REGS = cfg.get_sauria_regs(CONV_DICT, HOPTS, silent=True)
print('[gen] sauria_regs ok  N_REGS=%d' % N_REGS)

_, _, _, loop_order = ex.get_tiling_loops(CONV_DICT)
print('[gen] loop_order=%d' % loop_order)

Cgold, _, _ = ex.get_ideal_results(A, B, Cpre, CONV_DICT, HOPTS, slib.get_sa_dict(HOPTS))
print('[gen] golden ok')

B_opt = dh.optimize_weight_tensor_shape(B, CONV_DICT)
DRAM, DRAMg, offsets = dh.assign_dram_values(A, B_opt, Cpre, Cgold, 0, CONV_DICT, HOPTS)
np.savetxt('/tmp/sauria_offsets.txt', np.array(offsets, dtype=np.int64), fmt='%d')

controller_args = cfg.get_controller_regs(CONV_DICT, sauria_regs, N_REGS, offsets, loop_order)

os.makedirs(os.path.join(test_dir, 'stimuli'), exist_ok=True)
fh.generate_test_files(DRAM, DRAMg, controller_args,
                       [offsets[0], offsets[2], offsets[3]], HOPTS, N_REGS, test_dir=test_dir)
print('[gen] WROTE stimuli ->', os.path.join(test_dir, 'stimuli'))
