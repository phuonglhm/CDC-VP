#!/usr/bin/env python3
"""Cost model per TILE SHAPE (MODE B, prediction) -- used to sweep tile splits.

For ANY tile (conv, nch, ht, wt), including shapes never measured, returns the layer cycles ("core busy"), exec, and DMA cycles.
Calibrated entirely on the 149 shapes measured with sauria_model (shape_table.csv). No VM, no simulation.

Structure of the measured data (checked on 149 shapes, see tile_model_fit.txt):
  exec  = n_ctx * max(K, KMIN_E) + E(x_used)              KMIN_E ~ 102 (context switch + psum drain not hidden when K is small)
  busy  = exec + n_ctx * S(cell, K) + d                   d ~ 8..11
  S     = pipeline-stall cycles per context -- depends on (kh==1?, x_used, y_used==32?):
            x=32, KxK (kh>1) : 0                         (71/71 stride-1 shapes with y<32; stride 2 with y<32 also 0)
            x=32, 1x1,  y<32 : max(K, 170)               (K=64 -> ~170, K=192/384 -> K)
            x=16, any kernel : max(K, 170)               (K=192, 432, 3456 all ~ K)
            x=32, y=32 (1x1 and 3x3 s1): max(0, 225-K)   (3 measured shapes; an upper bound before that was too conservative)
            x=32, y=32, 3x3 stride 2  : 0.33*K (2 shapes, K=432) -> still EXTRAPOLATED, flag X
            other x (8,4,2,1): no data -> flag N (no prediction; used only when the shape was measured)
  DMA   : ld = 0.03642 * bytes_read + 49.8 ; st = 0.03616 * bytes_written + 32.8   (regression on 149 shapes: load error <= 5.1 %,
          store <= 20.5 % (small tiles))
          bytes_read = A + B + C(read-back int32) ; A = iy*ix*cin ; B = K*nch ; C = 4*ht*wt*nch ; bytes_written = 4*ht*wt*nch
          (matches 137 600 = 79 488 + 55 296 + 2 816)
Returned flag: 'M' = measured shape (measured numbers used), 'C' = prediction inside the calibrated region,
'X' = extrapolation where data is thin, 'N' = no prediction possible.
"""
import csv
import json
import math
import os

SA_DIM = 32
KMIN_E = 102
E_EXEC = {32: 267, 16: 219}
D_BUSY = 10
K_STALL_MIN = 170
Y32_STALL_C = 225
LD_A, LD_B = 0.03642, 49.8
ST_A, ST_B = 0.03616, 32.8


def active_divisor(d, hw=SA_DIM):
    for a in range(min(d, hw), 0, -1):
        if d % a == 0 and hw % a == 0:
            return a
    return 1


def chunk_contexts(total, step):
    return sum(math.ceil(min(step, total - s) / active_divisor(min(step, total - s))) for s in range(0, total, step))


class ShapeModel:
    def __init__(self, shape_csv, graph_ir):
        g = json.load(open(graph_ir))
        self.ops = {o["job"]: o for o in g["ops"] if o["op"] == "conv"}
        self.meas = {}
        for r in csv.DictReader(open(shape_csv)):
            self.meas[(r["conv"], int(r["dc"]), int(r["doy"]), int(r["dox"]))] = dict(
                busy=int(r["processing_cycles"]), exe=int(r["core_exec_cycles"]), ld=int(r["wait_input_cycles"]),
                st=int(r["wait_output_cycles"]), n=int(r["ncontexts"]), K=int(r["mvm_k"]), xu=int(r["x_used"]), yu=int(r["y_used"]))

    def stall_ctx(self, kh, sh, xu, yu, K):
        """(stall cycles per context, flag)"""
        if K < 64:
            return None, "N"   # K<64: only the stem (x=8) -- no data beyond the measured shapes
        if xu == 32:
            if yu == 32:
                # measured shapes at x=32, y=32: stall = max(0, 225-K) per context
                # (K=96 -> ~129, K=192 -> ~33, K=1152 and K=1728 -> 0). 3x3 stride 2 (2 shapes, K=432) measures 0.33*K -> kept, still EXTRAPOLATED (X).
                if kh > 1 and sh > 1:
                    return 0.33 * K, "X"
                return float(max(0, Y32_STALL_C - K)), "C"
            if kh > 1:
                return 0.0, "C"
            return max(K, K_STALL_MIN), "C"
        if xu == 16:
            return max(K, K_STALL_MIN), "C"
        return None, "N"

    def dma(self, cin, K, nch, ht, wt, iy, ix):
        a = iy * ix * cin
        b = K * nch
        c = 4 * ht * wt * nch
        return LD_A * (a + b + c) + LD_B, ST_A * c + ST_B, a, b, c

    def tile(self, conv, nch, ht, wt, n_ctx_override=None, y_used_override=None):
        """One tile with its full shape (nch channels, ht rows, wt columns)."""
        o = self.ops[conv]
        cin, kh, kw = o["cin"], o["kh"], o["kw"]
        sh, sw = o["stride"]
        K = cin * kh * kw
        xu = active_divisor(nch)
        yu = y_used_override or active_divisor(wt)
        n = n_ctx_override if n_ctx_override is not None else ht * math.ceil(wt / yu) * math.ceil(nch / xu)
        iy = (ht - 1) * sh + kh
        ix = (wt - 1) * sw + kw
        ld, st, a, b, c = self.dma(cin, K, nch, ht, wt, iy, ix)
        m = self.meas.get((conv, nch, ht, wt)) if n_ctx_override is None else None
        if m is not None and m["n"] == n:
            return dict(busy=m["busy"], exe=m["exe"], ld=ld, st=st, ld_meas=m["ld"], st_meas=m["st"], n=n, K=K, xu=xu, yu=yu,
                        flag="M", A=a, B=b, C=c)
        s, flag = self.stall_ctx(kh, sh, xu, yu, K)
        if s is None:
            return dict(busy=None, exe=None, ld=ld, st=st, n=n, K=K, xu=xu, yu=yu, flag="N", A=a, B=b, C=c)
        exe = n * max(K, KMIN_E) + E_EXEC.get(xu, 267)
        busy = exe + n * s + D_BUSY
        return dict(busy=busy, exe=exe, ld=ld, st=st, n=n, K=K, xu=xu, yu=yu, flag=flag, A=a, B=b, C=c)

    def error_report(self):
        """Error of the PURE model (no measured numbers used) on the 149 measured shapes."""
        out = []
        for (conv, nch, ht, wt), m in self.meas.items():
            o = self.ops[conv]
            K = o["cin"] * o["kh"] * o["kw"]
            xu, yu = m["xu"], m["yu"]
            s, flag = self.stall_ctx(o["kh"], o["stride"][0], xu, yu, K)
            if s is None:
                out.append((conv, nch, ht, wt, flag, None, m["busy"]))
                continue
            exe = m["n"] * max(K, KMIN_E) + E_EXEC.get(xu, 267)
            out.append((conv, nch, ht, wt, flag, exe + m["n"] * s + D_BUSY, m["busy"]))
        return out
