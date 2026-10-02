"""Golden-HAS: int8 reference of the IR executed with the HAS drawing semantics (has/HAS_IFACE.md §2-§6).

    conv:      psum = sum(q_in * q_w) + bias (bias = PSUM preload)                           (exact int)
               q_pre = Requant(psum, S, s, zp_out)  with knobs ROUND_MODE / REQ_NARROW / SCALE_FMT (§4)
               q_out = LUT[q_pre + 128] if SiLU else q_pre                                   (§5 direct)
    elem_add:  a' = Dequant(A), b' = Dequant(B), Sum = sat32(a' + b'), out = Requant(Sum)     (§6.2 ADD)
    elem_max:  max over KxK window, pad = -128                                               (§6.2 MAX)
    slice_ch / concat / upsample_nearest: copy (host, HAS §9.5)
Written from HAS_IFACE only; the conv sum reuses fe_ref_int8.conv_psum (exact integer im2col, not a HAS block).
Knob presets: MODES["compat"] reproduces the pre-HAS int8 reference (fe_ref_int8 with a _rc config) byte for byte; MODES["has"] is the default.
"""
import numpy as np

import fe_ref_int8 as fr

FLOOR, HALF_UP, HALF_AWAY, HALF_EVEN = 0, 1, 2, 3
SAT16, WRAP16, NONE = 0, 1, 2
SUB_BEFORE, ADD_AFTER = 0, 1
I32, U32 = 0, 1
MODES = {
    "compat": {"round": FLOOR, "narrow": SAT16, "deq_zp": SUB_BEFORE, "scale_fmt": U32},
    "has": {"round": HALF_UP, "narrow": SAT16, "deq_zp": SUB_BEFORE, "scale_fmt": I32},
}
ADD_GRID_SHIFT = 14  # HAS elem_add: a', b', Sum live on a grid of (out scale) / 2^14
INT63 = 1 << 63


def decode_s(raw, fmt):
    raw = np.asarray(raw, dtype=np.int64) & 0xFFFFFFFF
    return np.where(raw >= (1 << 31), raw - (1 << 32), raw) if fmt == I32 else raw


def _mul(x, S):
    """x * S in int64 with an explicit overflow check (HAS_IFACE §4 OVF64 must never happen)."""
    x = np.asarray(x, dtype=np.int64)
    S = np.asarray(S, dtype=np.int64)
    bound = np.abs(x.astype(np.float64)) * np.abs(S.astype(np.float64))
    if bound.size and bound.max() >= float(INT63) * 0.999:
        raise OverflowError("OVF64 in x*S")
    return x * S


def rshift(p, s, mode):
    p = np.asarray(p, dtype=np.int64)
    s = np.asarray(s, dtype=np.int64)
    if s.size and (s.min() < 0 or s.max() > 63):
        raise ValueError("shift outside [0, 63]")
    s1 = np.maximum(s, 1)
    half = np.where(s > 0, np.left_shift(np.int64(1), s1 - 1), 0)
    if mode == FLOOR:
        r = np.right_shift(p, s)
    elif mode == HALF_UP:
        r = np.right_shift(p + half, s)
    elif mode == HALF_AWAY:
        r = np.sign(p) * np.right_shift(np.abs(p) + half, s)
    elif mode == HALF_EVEN:
        q = np.right_shift(p, s)
        rem = p - np.left_shift(q, s)
        r = q + ((rem > half) | ((rem == half) & ((q & 1) == 1) & (s > 0)))
    else:
        raise ValueError(mode)
    return np.where(s == 0, p, r)


def requant(x, raw_S, s, zp, k, counters=None):
    r = rshift(_mul(x, decode_s(raw_S, k["scale_fmt"])), s, k["round"])
    over = (r < -32768) | (r > 32767)
    if k["narrow"] == SAT16:
        n = np.clip(r, -32768, 32767)
    elif k["narrow"] == WRAP16:
        n = ((r + 32768) & 0xFFFF) - 32768
    else:
        n = r
    y = n + np.int64(zp)
    if counters is not None:
        counters["n_sat16"] += int(over.sum()) if k["narrow"] != NONE else 0
        counters["n_clamp8"] += int(((y < -128) | (y > 127)).sum())
    return np.clip(y, -128, 127).astype(np.int8)


def dequant(x, zp, raw_S, s, k):
    S = decode_s(raw_S, k["scale_fmt"])
    x = np.asarray(x, dtype=np.int64)
    if k["deq_zp"] == SUB_BEFORE:
        v = rshift(_mul(x - zp, S), s, k["round"])
    else:
        v = rshift(_mul(x, S), s, k["round"]) + zp
    return np.clip(v, -(1 << 31), (1 << 31) - 1)


def elem_add_params(mode, sa, sb, so):
    """Per-step ELEM_WISE ADD parameters (HAS_IFACE §7 field names). compat: unit scales (= sat8(A + B))."""
    if mode == "compat":
        return {"zpA": 0, "zpB": 0, "zpO": 0, "SA": 1, "sA": 0, "SB": 1, "sB": 0, "SO": 1, "sO": 0}
    g = 1 << ADD_GRID_SHIFT
    SA, SB = int(round(sa / so * g)), int(round(sb / so * g))
    if not (0 < SA < (1 << 31) and 0 < SB < (1 << 31)):
        raise SystemExit("elem_add scale ratio out of int32 range: %g %g %g" % (sa, sb, so))
    return {"zpA": 0, "zpB": 0, "zpO": 0, "SA": SA, "sA": 0, "SB": SB, "sB": 0, "SO": 1, "sO": ADD_GRID_SHIFT}


def elem_add(a, b, p, k, counters=None):
    a1 = dequant(a, p["zpA"], p["SA"], p["sA"], k)
    b1 = dequant(b, p["zpB"], p["SB"], p["sB"], k)
    s = np.clip(a1 + b1, -(1 << 31), (1 << 31) - 1)
    return requant(s, p["SO"], p["sO"], p["zpO"], k, counters)


def elem_max(x, kk, st, pad):
    return fr.maxpool_int8(x, (kk, kk), (st, st), (pad, pad, pad, pad))


def run_has(ir, params, x, mode="has", knobs=None, add_params=None, counters=None):
    """Returns tensor -> int8. add_params: {out tensor: elem_add params}; conv outputs feeding a fused
    residual are produced under the conv_output_elided tensor name by the caller's IR (the IR 'add' op
    is executed here as elem_add)."""
    k = dict(MODES[mode], **(knobs or {}))
    add_params = add_params or {}
    t = {ir["graph_input"]: fr.quantize_input(x)}
    for o in ir["ops"]:
        op = o["op"]
        a = [t[i] for i in o["inputs"]]
        if op == "conv":
            j = o["job"]
            psum = fr.conv_psum(a[0], params[j + "/q_w"], o["stride"], o["pad"])
            cout = psum.shape[1]
            zp = int(params[j + "/zp_out"]) if j + "/zp_out" in params else 0
            S = params[j + "/scale"].astype(np.int64).reshape(1, cout, 1, 1)
            sh = params[j + "/shift"].astype(np.int64).reshape(1, cout, 1, 1)
            y = requant(psum + params[j + "/q_b"].astype(np.int64).reshape(1, cout, 1, 1), S, sh, zp, k, counters)
            if o["act"] == "silu":
                y = params[j + "/lut"][y.astype(np.int16) + 128]
        elif op == "add":
            p = add_params.get(o["output"]) or elem_add_params("compat", 1, 1, 1)
            y = elem_add(a[0], a[1], p, k, counters)
        elif op == "maxpool":
            if o["kernel"][0] != o["kernel"][1] or o["stride"][0] != o["stride"][1] or len(set(o["pad"])) != 1:
                raise ValueError("maxpool not square: %s" % o)
            y = elem_max(a[0], o["kernel"][0], o["stride"][0], o["pad"][0])
        elif op == "slice_ch":
            y = a[0][:, o["start"]:o["end"]]
        elif op == "concat":
            y = np.concatenate(a, axis=1)
        elif op == "upsample_nearest":
            f = o["factor"]
            y = a[0].repeat(f, axis=2).repeat(f, axis=3)
        else:
            raise ValueError(op)
        y = np.asarray(y, dtype=np.int8)
        if list(y.shape) != o["out_shape"]:
            raise RuntimeError("op %d %s: got %s, want %s" % (o["id"], op, list(y.shape), o["out_shape"]))
        t[o["output"]] = np.ascontiguousarray(y)
    return t
