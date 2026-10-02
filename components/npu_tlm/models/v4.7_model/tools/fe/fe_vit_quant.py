"""Int8 quantisation of ViT-B/16 laid out on the HAS drawing pipelines (GEMM_FUSED, FUSED_ATTN, SOFTMAX,
LAYERNORM, ELEM_WISE ADD, LUT direct), calibrated on coco128 images, evaluated against fe_vit_float.forward_np.

Quant points per block (every "q(...)" is an int8 tensor with a per-tensor scale; every GEMM is exact integer + requant
HALF_UP with S int32 in [2^30, 2^31) as in has/HAS_IFACE.md §4):
  x (residual stream, int8) --LN1--> q(ln1)                             LAYERNORM                  [float stand-in, R8-R10]
  q(ln1) @ Wqkv + b -> requant -> q(q), q(k), q(v)                      GEMM_FUSED (per-channel W)
  q(q) @ q(k)^T -> x 1/sqrt(d) folded into the requant -> q(s)          FUSED_ATTN phase 1 (H7: one rounding here)
  softmax(q(s)) -> q(p), scale 1/127                                    SOFTMAX                    [float stand-in, R5-R7]
  q(p) @ q(v) -> requant -> q(av)                                       FUSED_ATTN phase 3
  q(av) @ Wproj + b -> requant -> q(proj)                               GEMM_FUSED
  x = ELEM_WISE ADD(x, q(proj)) (dequant 2^14 grid, add, requant)        ELEM_WISE ADD (fe_ref_has.elem_add)
  LN2 -> q(ln2); q(ln2) @ W_fc1 + b -> requant -> q(h); GELU LUT int8->int8 -> q(g)      GEMM_FUSED + LUT direct (exact)
  q(g) @ W_fc2 + b -> requant -> q(mlp); x = ELEM_WISE ADD(x, q(mlp))
Stand-ins: LN and softmax are computed in float on dequantised int8 input and requantised; they become bit-exact once
HW answers R5-R10 (HAS_IFACE_RCE §3-§4). Patch embedding and head: int8 GEMMs; final LN: float stand-in.
Usage (repository root, FE_WORK set): python3 tools/fe/fe_vit_quant.py [--calib 16] [--eval 16]
"""
import argparse
import glob
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fe_common as fc  # noqa: E402
import fe_ref_has as fh  # noqa: E402
import fe_vit_float as vf  # noqa: E402

import numpy as np  # noqa: E402
from scipy.special import erf  # noqa: E402

K_HAS = fh.MODES["has"]
BIAS_SAT = [0]  # bias entries that do not fit int32 (tiny per-channel weight scale)
TAPS = ["ln1", "q", "k", "v", "s", "av", "proj", "ln2", "h", "g", "mlp", "x"]


def pctl(a, p=99.99):
    return float(np.percentile(np.abs(a), p))


def forward_taps(W, x):
    """Float forward that also returns every quant-point tensor per block."""
    p = x[0].reshape(3, 14, 16, 14, 16).transpose(1, 3, 0, 2, 4).reshape(196, 768)
    t = p @ W["patch_embed.proj.weight"].reshape(768, -1).T + W["patch_embed.proj.bias"]
    t = np.concatenate([W["cls_token"][0], t], 0) + W["pos_embed"][0]
    taps = {"patches": p, "x_in": t}
    for i in range(vf.DEPTH):
        b = "blocks.%d." % i
        d = {}
        d["ln1"] = vf.layer_norm(t, W[b + "norm1.weight"], W[b + "norm1.bias"])
        qkv = (d["ln1"] @ W[b + "attn.qkv.weight"].T + W[b + "attn.qkv.bias"]).reshape(197, 3, 12, 64).transpose(1, 2, 0, 3)
        d["q"], d["k"], d["v"] = qkv[0], qkv[1], qkv[2]
        d["s"] = d["q"] @ d["k"].transpose(0, 2, 1) / 8.0
        pr = vf.softmax(d["s"])
        d["av"] = (pr @ d["v"]).transpose(1, 0, 2).reshape(197, 768)
        d["proj"] = d["av"] @ W[b + "attn.proj.weight"].T + W[b + "attn.proj.bias"]
        t = t + d["proj"]
        d["ln2"] = vf.layer_norm(t, W[b + "norm2.weight"], W[b + "norm2.bias"])
        d["h"] = d["ln2"] @ W[b + "mlp.fc1.weight"].T + W[b + "mlp.fc1.bias"]
        d["g"] = 0.5 * d["h"] * (1 + erf(d["h"] / np.sqrt(2.0)))
        d["mlp"] = d["g"] @ W[b + "mlp.fc2.weight"].T + W[b + "mlp.fc2.bias"]
        t = t + d["mlp"]
        d["x"] = t
        taps[i] = d
    taps["lnf"] = vf.layer_norm(t, W["norm.weight"], W["norm.bias"])
    return taps


def q8(x, s):
    return np.clip(np.floor(x / s + 0.5), -127, 127).astype(np.int64)


def qw(w):
    """Per-output-channel symmetric int8 weights: w [out, in] -> (q int64, scale [out])."""
    s = np.maximum(np.abs(w).max(1), 1e-12) / 127.0
    return np.clip(np.floor(w / s[:, None] + 0.5), -127, 127).astype(np.int64), s


def req_params(m):
    """Real multiplier m > 0 -> (S int32 in [2^30, 2^31), s) with m ~= S / 2^s."""
    m = np.atleast_1d(np.asarray(m, dtype=np.float64))
    e = np.floor(np.log2(m)).astype(np.int64)
    s = 30 - e
    S = np.floor(m * 2.0 ** s + 0.5).astype(np.int64)
    over = S >= (1 << 31)
    S = np.where(over, S >> 1, S)
    s = np.where(over, s - 1, s)
    if s.min() < 0 or s.max() > 63:
        raise SystemExit("requant shift out of range")
    return S, s


def requant(acc, m):
    S, s = req_params(m)
    return fh.requant(acc, S, s, 0, K_HAS).astype(np.int64)


def gemm(xq, sx, W, bias, s_out):
    wq, sw = qw(W)
    acc = xq @ wq.T  # exact: |sum| <= 127*127*3072 < 2^53 via int64
    if bias is not None:
        qb = np.floor(bias.astype(np.float64) / (sx * sw) + 0.5)
        BIAS_SAT[0] += int((np.abs(qb) > (1 << 31) - 1).sum())
        acc = acc + np.clip(qb, -(1 << 31), (1 << 31) - 1).astype(np.int64)  # bias = int32 PSUM preload
    acc = np.clip(acc, -(1 << 31), (1 << 31) - 1)  # PSUM is int32
    return requant(acc, sx * sw / s_out)


def add(aq, sa, bq, sb, so):
    p = fh.elem_add_params("has", sa, sb, so)
    return fh.elem_add(aq, bq, p, K_HAS).astype(np.int64)


def forward_int8(W, x, sc):
    """Returns (logits float, per-block residual stream dequantised)."""
    p = x[0].reshape(3, 14, 16, 14, 16).transpose(1, 3, 0, 2, 4).reshape(196, 768)
    pq = q8(p, sc["patches"])
    e = gemm(pq, sc["patches"], W["patch_embed.proj.weight"].reshape(768, -1), W["patch_embed.proj.bias"], sc["x_in"])
    pos = q8(np.concatenate([W["cls_token"][0], np.zeros((196, 768), np.float32)], 0) + W["pos_embed"][0], sc["x_in"])
    e = np.concatenate([np.zeros((1, 768), np.int64), e], 0)
    xq, sx = add(e, sc["x_in"], pos, sc["x_in"], sc["x_in"]), sc["x_in"]
    outs = []
    for i in range(vf.DEPTH):
        b, s = "blocks.%d." % i, sc[i]
        ln1 = q8(vf.layer_norm(xq * sx, W[b + "norm1.weight"], W[b + "norm1.bias"]), s["ln1"])  # stand-in
        wqkv, bqkv = W[b + "attn.qkv.weight"], W[b + "attn.qkv.bias"]
        qh = [gemm(ln1, s["ln1"], wqkv[j * 768:(j + 1) * 768], bqkv[j * 768:(j + 1) * 768], s[n])
              .reshape(197, 12, 64).transpose(1, 0, 2) for j, n in enumerate(("q", "k", "v"))]
        sq = requant(qh[0] @ qh[1].transpose(0, 2, 1), s["q"] * s["k"] / 8.0 / s["s"])
        pq = q8(vf.softmax(sq * s["s"]), 1.0 / 127)  # stand-in
        av = requant(pq @ qh[2], (1.0 / 127) * s["v"] / s["av"]).transpose(1, 0, 2).reshape(197, 768)
        pr = gemm(av, s["av"], W[b + "attn.proj.weight"], W[b + "attn.proj.bias"], s["proj"])
        x1 = add(xq, sx, pr, s["proj"], s["x1"])
        ln2 = q8(vf.layer_norm(x1 * s["x1"], W[b + "norm2.weight"], W[b + "norm2.bias"]), s["ln2"])  # stand-in
        h = gemm(ln2, s["ln2"], W[b + "mlp.fc1.weight"], W[b + "mlp.fc1.bias"], s["h"])
        lv = np.arange(-128, 128) * s["h"]
        lut = np.clip(np.floor(0.5 * lv * (1 + erf(lv / np.sqrt(2.0))) / s["g"] + 0.5), -128, 127).astype(np.int64)
        g = lut[h + 128]  # LUT direct, exact
        m = gemm(g, s["g"], W[b + "mlp.fc2.weight"], W[b + "mlp.fc2.bias"], s["mlp"])
        xq, sx = add(x1, s["x1"], m, s["mlp"], s["x"]), s["x"]
        outs.append(xq * sx)
    lnf = q8(vf.layer_norm(xq * sx, W["norm.weight"], W["norm.bias"]), sc["lnf"])  # stand-in
    hq, sh = qw(W["head.weight"])
    logits = (lnf[0] @ hq.T) * sc["lnf"] * sh + W["head.bias"]
    return logits, outs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--calib", type=int, default=16)
    ap.add_argument("--eval", type=int, default=16)
    args = ap.parse_args()
    t0 = time.time()
    W = vf.load_safetensors(vf.WEIGHTS)
    imgs = sorted(glob.glob(os.path.join(fc.FE_WORK, "datasets", "coco128", "images", "train2017", "*.jpg")))
    calib, evals = imgs[: args.calib], imgs[args.calib: args.calib + args.eval]
    acc = {}
    for path in calib:
        tp = forward_taps(W, vf.preprocess(path))
        for key in ("patches", "x_in", "lnf"):
            acc.setdefault(key, []).append(pctl(tp[key]))
        for i in range(vf.DEPTH):
            for n in ("ln1", "q", "k", "v", "s", "av", "proj", "ln2", "h", "g", "mlp", "x"):
                acc.setdefault((i, n), []).append(pctl(tp[i][n]))
            x1 = tp[i]["x"] - tp[i]["mlp"]
            acc.setdefault((i, "x1"), []).append(pctl(x1))
    sc = {k: float(np.max(v)) / 127.0 for k, v in acc.items() if not isinstance(k, tuple)}
    for i in range(vf.DEPTH):
        sc[i] = {n: float(np.max(acc[(i, n)])) / 127.0 for n in TAPS + ["x1"] if (i, n) in acc}
    print("[vit-q] calibrated on %d coco128 images, %.0fs" % (len(calib), time.time() - t0))

    agree, cmin, rows = 0, [1.0] * vf.DEPTH, []
    for path in evals:
        x = vf.preprocess(path)
        lf, of = vf.forward_np(W, x)
        lq, oq = forward_int8(W, x, sc)
        for i in range(vf.DEPTH):
            cmin[i] = min(cmin[i], vf.cos(of[i], oq[i]))
        agree += int(np.argmax(lf) == np.argmax(lq))
        rows.append((os.path.basename(path), int(np.argmax(lf)), int(np.argmax(lq)), vf.cos(lf, lq)))
    out = os.path.join(fc.FE_WORK, "has", "vit")
    os.makedirs(out, exist_ok=True)
    rep = {"calib": [os.path.basename(p) for p in calib], "eval": rows, "top1_agree": agree, "n_eval": len(evals),
           "block_cos_min": cmin, "logit_cos_min": min(r[3] for r in rows),
           "stand_ins": ["LN1", "LN2", "final LN", "softmax"], "requant": "HALF_UP, S int32 [2^30,2^31)"}
    with open(os.path.join(out, "vit_quant_report.json"), "w") as f:
        json.dump(rep, f, indent=1)
    with open(os.path.join(out, "vit_scales.json"), "w") as f:
        json.dump({str(k): v for k, v in sc.items()}, f, indent=1)
    print("[vit-q] top-1 int8 == float: %d/%d, logit cos min %.4f" % (agree, len(evals), rep["logit_cos_min"]))
    print("[vit-q] residual-stream cos min per block: " + " ".join("%.4f" % c for c in cmin))
    print("[vit-q] bias entries saturated to int32 (all eval GEMM calls): %d" % BIAS_SAT[0])
    print("[vit-q] done %.0fs -> %s" % (time.time() - t0, out))


if __name__ == "__main__":
    main()
