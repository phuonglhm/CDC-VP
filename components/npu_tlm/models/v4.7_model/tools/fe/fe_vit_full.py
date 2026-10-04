"""ViT-B/16 end to end through the DFC, golden chained block to block (block N eats the INTEGER output of N-1).
Instruction stream (channel-major [C][L], L = 197, no host step):
  embed  GEMM 0x12 768 -> 768 on the patch matrix [768 = c*256 + ky*16 + kx][197] (16x16 s16 conv = non-overlapping im2col,
         i.e. only a layout of the input image; column 0 = zeros = the CLS slot, so the output lands directly as [768][197])
         ADD 0x15 e + pos' -> x0 (pos' column 0 = CLS + pos0 - requant(bias) contribution, so x0 matches the float CLS token)
  12 x   the 20-instruction encoder block of fe_vit_block.Emitter.block
  head   LAYERNORM 0x14 (all 197 rows) + GEMM 0x12 768 -> 1000 (cout padded to 1024, PAD_TAIL) over all tokens; logits = token 0
Golden: fe_vit_block.block_golden per block; quality vs fe_vit_quant.forward_taps (float) per block + logits cos + top-1.
Output fe_work/has/vit_full/: dram_init.bin, dram_golden.bin, mmio.txt, summary.json.
Usage (repository root, FE_WORK set): python3 tools/fe/fe_vit_full.py [--blocks 12] [--image 16]
Run: tools/has/tb_has_npu_top $FE_WORK/has/vit_full --c-bcast   (recommended profile + bias broadcast, as measured)
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
import fe_vit_block as vb  # noqa: E402
import fe_vit_float as vf  # noqa: E402
import fe_vit_quant as vq  # noqa: E402

import numpy as np  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--blocks", type=int, default=12)
    ap.add_argument("--image", type=int, default=16, help="coco128 index (16 = first after the calibration images)")
    args = ap.parse_args()
    t0 = time.time()
    log = lambda m: print("[vit-full %5.0fs] %s" % (time.time() - t0, m), flush=True)  # noqa: E731
    out = os.path.join(fc.FE_WORK, "has", "vit_full")
    W = vf.load_safetensors(vf.WEIGHTS)
    with open(os.path.join(fc.FE_WORK, "has", "vit", "vit_scales.json")) as f:
        sc = json.load(f)
    imgs = sorted(glob.glob(os.path.join(fc.FE_WORK, "datasets", "coco128", "images", "train2017", "*.jpg")))
    img = imgs[args.image]
    x = vf.preprocess(img)
    tp = vq.forward_taps(W, x)
    em = vb.Emitter("fe_vit_full.py: ViT-B/16, %d blocks, image %s, channel-major [C][197]" % (args.blocks, os.path.basename(img)))
    S = {"image": os.path.basename(img), "blocks": args.blocks, "cos_block_y": [], "gemm_raised": {}, "psum_max": 0}

    # ---- patch embedding + CLS / pos (0x12 + 0x15) ----
    p = x[0].reshape(3, 14, 16, 14, 16).transpose(1, 3, 0, 2, 4).reshape(196, 768)
    pq = np.concatenate([np.zeros((1, 768), np.int64), vq.q8(p, sc["patches"])], 0)  # row 0 = CLS slot
    sxi = sc["x_in"]
    G = {}
    e = vb.gemm_into(G, S, "embed", pq, sc["patches"], W["patch_embed.proj.weight"].reshape(768, -1),
                     W["patch_embed.proj.bias"], sxi)
    pos_f = W["pos_embed"][0].astype(np.float64).copy()
    pos_f[0] += W["cls_token"][0, 0] - e[0] * sxi  # CLS: e[0] = requant(bias) is not zero, fold it out
    pos = vq.q8(pos_f, sxi)
    pa = fh.elem_add_params("has", sxi, sxi, sxi)
    xq = fh.elem_add(e, pos, pa, vq.K_HAS).astype(np.int64)
    pqa, posa = em.data(pq), em.data(pos)
    ea, xa = em.act(e), em.act(xq)
    em.gemm("embed 16x16 s16 (im2col input)", pqa, ea, G, "embed", 197)
    em.add("x0 = embed + pos", ea, posa, xa, pa, xq.size)
    S["cos_x0"] = vf.cos(xq * sxi, tp["x_in"])
    log("embed: cos x0 %.4f, %d instructions" % (S["cos_x0"], em.n))

    # ---- encoder blocks, chained on the integer stream ----
    sx = sxi
    for bi in range(args.blocks):
        s = sc[str(bi)]
        Gb, Pb = vb.block_golden(W, bi, xq, sx, s, log=log)
        xa = em.block(Gb, Pb, xa)
        xq, sx = Gb["y"], s["x"]
        c = vf.cos(xq * sx, tp[bi]["x"])
        S["cos_block_y"].append(c)
        for k, v in Pb.items():
            if k.startswith("gemm_"):
                S["gemm_raised"]["b%d_%s" % (bi, k[5:])] = v["raised_channels"]
                S["psum_max"] = max(S["psum_max"], v["psum_max"])
        log("block %d: cos y %.4f, %d instructions, DRAM %.1f MB" % (bi, c, em.n, len(em.init) / 2 ** 20))
        del Gb, Pb

    # ---- head: final LN (all rows) + GEMM 768 -> 1000 over all tokens, logits = token 0 ----
    if args.blocks == 12:
        lnq, lnp, _ = vb.ln(xq, sx, W["norm.weight"], W["norm.bias"], sc["lnf"])
        logit_f = tp["lnf"][0] @ W["head.weight"].T + W["head.bias"]
        s_log = float(np.abs(tp["lnf"] @ W["head.weight"].T + W["head.bias"]).max()) / 127.0  # this image, all tokens
        G = {}
        lg = vb.gemm_into(G, S, "head", lnq, sc["lnf"], W["head.weight"], W["head.bias"], s_log)
        lna, la = em.act(lnq), em.act(lg)
        em.lnorm("final ln", xa, lna, lnp, 197)
        em.gemm("head", lna, la, G, "head", 197)
        li = lg[0]
        S.update(s_logit=s_log, cos_lnf=vf.cos(lnq * sc["lnf"], tp["lnf"]), cos_logits=vf.cos(li * s_log, logit_f),
                 top1_int=int(np.argmax(li)), top1_float=int(np.argmax(logit_f)),
                 top1_int_ties=int((li == li.max()).sum()), top5_float_has_int_top1=bool(int(np.argmax(li)) in
                                                                                         np.argsort(logit_f)[-5:].tolist()))
        log("head: cos logits %.4f, top-1 int %d (ties %d) float %d" % (S["cos_logits"], S["top1_int"], S["top1_int_ties"],
                                                                         S["top1_float"]))
    size = em.write(out)
    S.update(instructions=em.n, dram_bytes=size, seconds=round(time.time() - t0))
    with open(os.path.join(out, "summary.json"), "w") as f:
        json.dump(S, f, indent=1, default=lambda o: o.tolist() if hasattr(o, "tolist") else float(o))
    log("done: %d instructions, DRAM %d B -> %s" % (em.n, size, out))


if __name__ == "__main__":
    main()
