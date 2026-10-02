"""T2: int8 reference that executes the IR with the step-3 parameters, matching the current OBP.

    conv:     psum  = sum(q_in * q_w) over the zero-padded window          (int, exact)
              q_pre = sat8(((psum + q_b) * scale) >> shift)                 (int64, arithmetic shift)
              q_out = lut[q_pre + 128]  if SiLU  else  q_pre
    slice_ch / concat / upsample_nearest: copy
    add:      sat8(a + b)                                                   (OBP stage 4 arithmetic)
    maxpool:  max over window, padding = -128

The conv sum is computed with float64 BLAS on im2col columns. That is exact: every partial sum is an
integer with |value| <= 128 * 127 * K < 2^53, so no rounding can occur in any summation order.
"""
import numpy as np
from numpy.lib.stride_tricks import sliding_window_view

import fe_quant as fq

EXACT_LIMIT = float(1 << 53)


def quantize_input(x, s0=fq.INPUT_SCALE):
    return fq.sat8(fq.round_half_away(x / s0)).astype(np.int8)


def conv_psum(q_in, q_w, stride, pad):
    """q_in int8 [1,C,H,W], q_w int8 [Cout,C,kh,kw], pad [top,left,bottom,right] -> int64 [1,Cout,Ho,Wo]."""
    cout, cin, kh, kw = q_w.shape
    pt, pl, pb, pr = pad
    sh, sw = stride
    x = np.pad(q_in[0].astype(np.float64), ((0, 0), (pt, pb), (pl, pr)))
    win = sliding_window_view(x, (kh, kw), axis=(1, 2))[:, ::sh, ::sw]
    _, ho, wo, _, _ = win.shape
    cols = np.ascontiguousarray(win.transpose(1, 2, 0, 3, 4)).reshape(ho * wo, cin * kh * kw)
    w = q_w.reshape(cout, -1).astype(np.float64)
    out = cols @ w.T
    if np.abs(out).max() >= EXACT_LIMIT:
        raise OverflowError("conv sum exceeds exact float64 range")
    return np.rint(out).astype(np.int64).T.reshape(1, cout, ho, wo)


def requant(psum, q_b, scale, shift, round_mode=0):
    """round_mode 0 = floor (current OBP); 1 = HALF_UP (x + 2^(shift-1)) >> shift (HAS_IFACE §3, shift >= 1)."""
    b = q_b.astype(np.int64)[None, :, None, None]
    s = scale.astype(np.int64)[None, :, None, None]
    h = shift.astype(np.int64)[None, :, None, None]
    p = (psum + b) * s
    if round_mode == 1:
        p = p + np.left_shift(np.int64(1), h - 1)
    elif round_mode != 0:
        raise ValueError("round_mode %r not supported here" % round_mode)
    return fq.sat8(np.right_shift(p, h)).astype(np.int8)


def maxpool_int8(x, kernel, stride, pad):
    kh, kw = kernel
    pt, pl, pb, pr = pad
    xp = np.pad(x[0], ((0, 0), (pt, pb), (pl, pr)), constant_values=-128)
    win = sliding_window_view(xp, (kh, kw), axis=(1, 2))[:, ::stride[0], ::stride[1]]
    return win.max(axis=(-2, -1))[None].astype(np.int8)


def run_int8(ir, params, x, keep_psum=False):
    """Returns dict tensor -> int8 array; with keep_psum also '<output>@psum' (int64) and
    '<output>@qpre' (int8, before the LUT) per conv."""
    t = {ir["graph_input"]: quantize_input(x)}
    for o in ir["ops"]:
        op = o["op"]
        a = [t[i] for i in o["inputs"]]
        if op == "conv":
            j = o["job"]
            psum = conv_psum(a[0], params[j + "/q_w"], o["stride"], o["pad"])
            if keep_psum:
                t[o["output"] + "@psum"] = psum
            rm = int(params[j + "/round_mode"]) if j + "/round_mode" in params else 0
            y = requant(psum, params[j + "/q_b"], params[j + "/scale"], params[j + "/shift"], rm)
            if keep_psum:
                t[o["output"] + "@qpre"] = y
            if o["act"] == "silu":
                y = params[j + "/lut"][y.astype(np.int16) + 128]
        elif op == "slice_ch":
            y = a[0][:, o["start"]:o["end"]]
        elif op == "concat":
            y = np.concatenate(a, axis=1)
        elif op == "add":
            y = fq.sat8(a[0].astype(np.int16) + a[1].astype(np.int16)).astype(np.int8)
        elif op == "maxpool":
            y = maxpool_int8(a[0], o["kernel"], o["stride"], o["pad"])
        elif op == "upsample_nearest":
            f = o["factor"]
            y = a[0].repeat(f, axis=2).repeat(f, axis=3)
        else:
            raise ValueError(op)
        if list(y.shape) != o["out_shape"] or y.dtype != np.int8:
            raise RuntimeError("op %d %s: got %s %s, want %s int8" % (o["id"], op, list(y.shape), y.dtype, o["out_shape"]))
        t[o["output"]] = np.ascontiguousarray(y)
    return t
