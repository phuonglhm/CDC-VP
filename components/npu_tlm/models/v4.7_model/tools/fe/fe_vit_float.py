"""ViT-B/16 float reference in numpy, from the timm safetensors (FE_WORK/weights/).

Independent anchors (like onnxruntime for YOLOv8m):
  1. a second forward written with torch.nn.functional (F.layer_norm, F.scaled_dot_product_attention, F.gelu) on the same
     weights; every block output is compared with the numpy one (max abs diff, cosine);
  2. top-1 on a few labelled images (ImageNet indices: 281-285 = cats, 151-268 = dogs).
numpy forward: patch conv 16x16 s16 as a matmul, cls token + pos_embed, 12 x [x += proj(attn(LN1 x)); x += fc2(gelu(fc1(LN2 x)))],
LN eps 1e-6, exact erf GELU, head on the cls token. Preprocess (timm pretrained_cfg): bicubic resize of the short side to
224 / 0.9 = 248, center crop 224, (x/255 - 0.5) / 0.5.
Usage (repository root, FE_WORK set): python3 tools/fe/fe_vit_float.py [image ...]   (default: coco128 000000000443 + 2 coco8 images)
"""
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fe_common as fc  # noqa: E402

import numpy as np  # noqa: E402
from PIL import Image  # noqa: E402
from scipy.special import erf  # noqa: E402

WEIGHTS = os.path.join(fc.FE_WORK, "weights", "vit_b16_augreg2_in21k_ft_in1k.safetensors")
DEPTH, HEADS, DIM, EPS = 12, 12, 768, 1e-6
CATS, DOGS = range(281, 286), range(151, 269)


def load_safetensors(path):
    with open(path, "rb") as f:
        n = struct.unpack("<Q", f.read(8))[0]
        hdr = json.loads(f.read(n))
        blob = f.read()
    out = {}
    for k, v in hdr.items():
        if k == "__metadata__":
            continue
        if v["dtype"] != "F32":
            raise SystemExit("unexpected dtype %s for %s" % (v["dtype"], k))
        a, b = v["data_offsets"]
        out[k] = np.frombuffer(blob[a:b], dtype=np.float32).reshape(v["shape"])
    return out


def preprocess(path):
    im = Image.open(path).convert("RGB")
    s = 224 / 0.9
    w, h = im.size
    r = s / min(w, h)
    im = im.resize((max(1, round(w * r)), max(1, round(h * r))), Image.BICUBIC)
    w, h = im.size
    l, t = (w - 224) // 2, (h - 224) // 2
    im = im.crop((l, t, l + 224, t + 224))
    x = np.asarray(im, dtype=np.float32) / 255.0
    return ((x - 0.5) / 0.5).transpose(2, 0, 1)[None]  # [1, 3, 224, 224]


def layer_norm(x, g, b):
    m = x.mean(-1, keepdims=True)
    v = ((x - m) ** 2).mean(-1, keepdims=True)
    return (x - m) / np.sqrt(v + EPS) * g + b


def softmax(x):
    e = np.exp(x - x.max(-1, keepdims=True))
    return e / e.sum(-1, keepdims=True)


def forward_np(W, x):
    """Returns (logits [1000], list of block outputs [197, 768])."""
    p = x[0].reshape(3, 14, 16, 14, 16).transpose(1, 3, 0, 2, 4).reshape(196, 3 * 16 * 16)
    t = p @ W["patch_embed.proj.weight"].reshape(DIM, -1).T + W["patch_embed.proj.bias"]
    t = np.concatenate([W["cls_token"][0], t], 0) + W["pos_embed"][0]
    outs = []
    hd = DIM // HEADS
    for i in range(DEPTH):
        b = "blocks.%d." % i
        h = layer_norm(t, W[b + "norm1.weight"], W[b + "norm1.bias"])
        qkv = (h @ W[b + "attn.qkv.weight"].T + W[b + "attn.qkv.bias"]).reshape(197, 3, HEADS, hd).transpose(1, 2, 0, 3)
        a = softmax(qkv[0] @ qkv[1].transpose(0, 2, 1) / np.sqrt(hd)) @ qkv[2]
        t = t + a.transpose(1, 0, 2).reshape(197, DIM) @ W[b + "attn.proj.weight"].T + W[b + "attn.proj.bias"]
        h = layer_norm(t, W[b + "norm2.weight"], W[b + "norm2.bias"])
        h = h @ W[b + "mlp.fc1.weight"].T + W[b + "mlp.fc1.bias"]
        h = 0.5 * h * (1.0 + erf(h / np.sqrt(2.0)))
        t = t + h @ W[b + "mlp.fc2.weight"].T + W[b + "mlp.fc2.bias"]
        outs.append(t)
    t = layer_norm(t, W["norm.weight"], W["norm.bias"])
    return t[0] @ W["head.weight"].T + W["head.bias"], outs


def forward_torch(W, x):
    import torch
    import torch.nn.functional as F
    T = {k: torch.from_numpy(v.copy()) for k, v in W.items()}
    with torch.no_grad():
        t = F.conv2d(torch.from_numpy(x), T["patch_embed.proj.weight"], T["patch_embed.proj.bias"], stride=16)
        t = t.flatten(2).transpose(1, 2)
        t = torch.cat([T["cls_token"], t], 1) + T["pos_embed"]
        outs = []
        for i in range(DEPTH):
            b = "blocks.%d." % i
            h = F.layer_norm(t, (DIM,), T[b + "norm1.weight"], T[b + "norm1.bias"], EPS)
            q, k, v = F.linear(h, T[b + "attn.qkv.weight"], T[b + "attn.qkv.bias"]).reshape(1, 197, 3, HEADS, -1).permute(2, 0, 3, 1, 4)
            a = F.scaled_dot_product_attention(q, k, v).transpose(1, 2).reshape(1, 197, DIM)
            t = t + F.linear(a, T[b + "attn.proj.weight"], T[b + "attn.proj.bias"])
            h = F.layer_norm(t, (DIM,), T[b + "norm2.weight"], T[b + "norm2.bias"], EPS)
            t = t + F.linear(F.gelu(F.linear(h, T[b + "mlp.fc1.weight"], T[b + "mlp.fc1.bias"])),
                             T[b + "mlp.fc2.weight"], T[b + "mlp.fc2.bias"])
            outs.append(t[0].numpy())
        t = F.layer_norm(t, (DIM,), T["norm.weight"], T["norm.bias"], EPS)
        return F.linear(t[:, 0], T["head.weight"], T["head.bias"])[0].numpy(), outs


def cos(a, b):
    a, b = a.ravel().astype(np.float64), b.ravel().astype(np.float64)
    return float(a @ b / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-30))


def main():
    try:
        import torch
        torch.set_num_threads(1)
    except ImportError:
        torch = None
    W = load_safetensors(WEIGHTS)
    imgs = sys.argv[1:] or [os.path.join(fc.FE_WORK, "datasets", "coco128", "images", "train2017", "000000000443.jpg")] + \
        fc.list_coco8_images()[:2]
    worst_cos, worst_abs = 1.0, 0.0
    for path in imgs:
        x = preprocess(path)
        logits, outs = forward_np(W, x)
        top = np.argsort(-logits)[:5]
        pr = softmax(logits.astype(np.float64))
        tag = "cat" if top[0] in CATS else "dog" if top[0] in DOGS else "-"
        line = "[vit] %s top5 %s (p %.3f, top1 %s)" % (os.path.basename(path), list(map(int, top)), pr[top[0]], tag)
        if torch is not None:
            lt, ot = forward_torch(W, x)
            c = min(cos(a, b) for a, b in zip(outs, ot))
            d = max(float(np.abs(a - b).max()) for a, b in zip(outs, ot))
            worst_cos, worst_abs = min(worst_cos, c), max(worst_abs, d)
            line += " | vs torch: block cos min %.7f, max|diff| %.2e, logits max|diff| %.2e, top1 same %s" % (
                c, d, float(np.abs(logits - lt).max()), int(np.argmax(lt)) == int(top[0]))
        print(line)
    if torch is not None:
        print("[vit] numpy vs torch over all images: block cos min %.7f, max|diff| %.2e" % (worst_cos, worst_abs))


if __name__ == "__main__":
    main()
