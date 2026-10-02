"""Quantization parameters matching the current OBP.

All activations are symmetric int8 (zero point 0). One conv:
    psum  = conv2d_int32(pad0(q_in), q_w)
    q_pre = sat8(((psum + q_b) * scale) >> shift)          # OBP stage 1-2, arithmetic shift = floor
    q_out = lut[q_pre + 128]  (SiLU)  or  q_pre  (no act)  # OBP stage 3
Dataflow ops (slice/concat/add/maxpool/upsample) require equal scales on their tensors; those
tensors are grouped with union-find and the group scale is max(|x|) over the group / 127.
"""
import math

import numpy as np

import fe_graph as fg

QMAX = 127
INPUT_SCALE = 1.0 / 127.0          # letterboxed image in [0, 1]
SCALE_MAX = (1 << 32) - 1          # OBP scale is uint32
SHIFT_MAX = 62                     # keep int64 product and shift well-defined
INT32_MAX = (1 << 31) - 1


def round_half_away(x):
    """Offline rounding used for q_w, q_b, scale and LUT (fixed here so results are reproducible)."""
    x = np.asarray(x, dtype=np.float64)
    return np.sign(x) * np.floor(np.abs(x) + 0.5)


def sat8(x):
    return np.clip(x, -128, 127)


# --------------------------------------------------------------------------- calibration
def calibrate(ir, weights, inputs, percentile=99.99):
    """max|x| and the given percentile of |x| for every IR tensor (+ SiLU pre-activations),
    each aggregated as the max over the calibration images."""
    stats = {}
    for x in inputs:
        vals = fg.run_ir_float(ir, weights, x, keep_preact=True)
        for name, v in vals.items():
            a = np.abs(v).reshape(-1)
            k = min(a.size - 1, int(math.ceil(a.size * percentile / 100.0)) - 1)
            p = float(np.partition(a, k)[k])
            s = stats.setdefault(name, {"max": 0.0, "p": 0.0})
            s["max"] = max(s["max"], float(a.max()))
            s["p"] = max(s["p"], p)
    return stats


# --------------------------------------------------------------------------- scale groups
def scale_groups(ir):
    parent = {}

    def find(a):
        parent.setdefault(a, a)
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[rb] = ra

    for o in ir["ops"]:
        find(o["output"])
        for i in o["inputs"]:
            find(i)
        if o["op"] != "conv":
            for i in o["inputs"]:
                union(i, o["output"])
    groups = {}
    for t in parent:
        groups.setdefault(find(t), []).append(t)
    return {t: sorted(groups[find(t)]) for t in parent}


def tensor_scales(ir, stats, method="max"):
    """Scale of every IR tensor (post-activation) and of every SiLU pre-activation."""
    key = {"max": "max", "p9999": "p", "mse": "mse"}.get(method, "p")
    groups = scale_groups(ir)
    inp = ir["graph_input"]
    if len(groups[inp]) != 1:
        raise fg.FrontendError("graph input shares a scale group: %s" % groups[inp])
    scales, group_of = {inp: INPUT_SCALE}, {}
    for t, members in groups.items():
        if t == inp:
            continue
        rng = max(stats[m][key] for m in members)
        if rng <= 0:
            raise fg.FrontendError("tensor group %s has zero range" % members)
        scales[t] = rng / QMAX
        group_of[t] = members[0]
    for o in ir["ops"]:
        if o["op"] == "conv" and o["act"] == "silu":
            pre = o["output"] + fg.PREACT_SUFFIX
            scales[pre] = stats[pre][key] / QMAX
    return scales, group_of


# --------------------------------------------------------------------------- conv params
def scale_shift(m):
    """Largest shift <= SHIFT_MAX with scale = round(m * 2^shift) <= SCALE_MAX."""
    if not m > 0:
        raise fg.FrontendError("non-positive multiplier %r" % m)
    shift = min(SHIFT_MAX, int(math.floor(math.log2(SCALE_MAX / m))))
    while shift >= 0:
        scale = int(round_half_away(m * (2.0 ** shift)))
        if 1 <= scale <= SCALE_MAX:
            return scale, shift
        shift -= 1
    raise fg.FrontendError("multiplier %r not representable" % m)


def quantize_conv(o, weights, scales, wq="per_channel", round_comp=False):
    w = weights[o["weight"]].astype(np.float64)
    b = weights[o["bias"]].astype(np.float64) if o["bias"] else np.zeros(o["cout"])
    cout = w.shape[0]
    if wq == "per_channel":
        wmax = np.abs(w).reshape(cout, -1).max(axis=1)
    elif wq == "per_tensor":
        wmax = np.full(cout, np.abs(w).max())
    else:
        raise ValueError(wq)
    zero_ch = int((wmax == 0).sum())
    s_w = np.where(wmax > 0, wmax / QMAX, 1.0)
    q_w = np.clip(round_half_away(w / s_w[:, None, None, None]), -QMAX, QMAX).astype(np.int8)

    s_in = scales[o["inputs"][0]]
    s_acc = s_in * s_w
    q_b = round_half_away(b / s_acc)
    if np.abs(q_b).max() > INT32_MAX:
        raise fg.FrontendError("%s: bias overflows int32 (max %d)" % (o["job"], np.abs(q_b).max()))
    q_b = q_b.astype(np.int64)

    s_out = scales[o["output"]]
    s_pre = scales[o["output"] + fg.PREACT_SUFFIX] if o["act"] == "silu" else s_out
    m = s_acc / s_pre
    ss = [scale_shift(float(mc)) for mc in m]
    scale = np.array([s for s, _ in ss], dtype=np.int64)
    shift = np.array([h for _, h in ss], dtype=np.int64)
    m_err = np.abs(scale / (2.0 ** shift) - m) / m

    comp = np.zeros(cout, dtype=np.int64)
    if round_comp:
        # floor((p + b + d) * scale >> shift) ~= round(...) with d = 2^(shift-1) / scale
        comp = round_half_away((2.0 ** (shift - 1)) / scale).astype(np.int64)
    q_b_total = q_b + comp

    psum_bound = 128 * np.abs(q_w.astype(np.int64)).reshape(cout, -1).sum(axis=1)
    biased_bound = psum_bound + np.abs(q_b_total)
    product_bound = biased_bound.astype(object) * scale.astype(object)
    checks = {
        "psum_bound_max": int(psum_bound.max()),
        "biased_bound_max": int(biased_bound.max()),
        "product_bound_max": int(max(product_bound)),
        "psum_ok": bool(psum_bound.max() <= INT32_MAX),
        "biased_ok": bool(biased_bound.max() <= INT32_MAX),
        "product_ok": bool(max(product_bound) < (1 << 63)),
        "bias_ok": bool(np.abs(q_b_total).max() <= INT32_MAX),
    }

    p = {"q_w": q_w, "q_b": q_b_total.astype(np.int32), "scale": scale.astype(np.uint32),
         "shift": shift.astype(np.uint32)}
    if o["act"] == "silu":
        q = np.arange(-128, 128, dtype=np.float64) * s_pre
        p["lut"] = sat8(round_half_away(q * (1.0 / (1.0 + np.exp(-q))) / s_out)).astype(np.int8)
    summary = {
        "job": o["job"], "module": o["module"], "act": o["act"], "s_in": s_in, "s_pre": s_pre, "s_out": s_out,
        "s_w_min": float(s_w.min()), "s_w_max": float(s_w.max()), "zero_weight_channels": zero_ch,
        "M_min": float(m.min()), "M_max": float(m.max()), "M_rel_err_max": float(m_err.max()),
        "shift_min": int(shift.min()), "shift_max": int(shift.max()),
        "scale_min": int(scale.min()), "scale_max": int(scale.max()),
        "q_b_absmax": int(np.abs(q_b_total).max()), "round_comp_max": int(comp.max()), **checks,
    }
    if o["act"] == "silu":
        lut = p["lut"].astype(np.int64)
        summary["lut_min"], summary["lut_max"] = int(lut.min()), int(lut.max())
        summary["lut_sat_entries"] = int(((lut == 127) | (lut == -128)).sum())
    return p, summary
