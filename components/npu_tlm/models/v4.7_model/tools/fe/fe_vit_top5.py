"""ViT-B/16 output of an NPU run (DRAM snapshot) -> top-5 ImageNet classes, compared with the golden and the float model.

The logits are read from the DRAM image at the output address of the last instruction ("GEMM_FUSED head" of mmio.txt,
register 0x40000408 = OUT address), layout channel-major [cout][tokens]; the logits are token 0 (the CLS token), int8,
dequantised with `s_logit` of summary.json. The golden (dram_golden.bin) is read the same way; the float reference is
fe_vit_float.forward_np on the same preprocessed image (about a minute on one core; --no-float skips it).
Class names: torchvision's ImageNet list, read as text from its source file (torchvision is not imported, so a
torchvision build that does not match the installed torch is no obstacle); without torchvision the classes are numbered.

Usage (repository root, FE_WORK = the work dir of the program):
  python3 tools/fe/fe_vit_top5.py --net-dir $FE_WORK/has/vit_full --dram <run>/snap/dram_snapshot.bin --draw out.png [--json out.json]
"""
import argparse
import ast
import glob
import importlib.util
import json
import os
import re
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fe_common as fc  # noqa: E402

REG_OUT = 0x40000408


def imagenet_names():
    spec = importlib.util.find_spec("torchvision")
    if spec and spec.submodule_search_locations:
        p = os.path.join(spec.submodule_search_locations[0], "models", "_meta.py")
        if os.path.exists(p):
            src = open(p).read()
            m = re.search(r"_IMAGENET_CATEGORIES\s*=\s*(\[.*?\])", src, re.S)
            if m:
                names = ast.literal_eval(m.group(1))
                if len(names) == 1000:
                    return names
    return ["class %d" % i for i in range(1000)]


def head_layout(mmio):
    """(out address, cout, tokens) of the last GEMM_FUSED block of mmio.txt."""
    blocks, cur = [], None
    for line in open(mmio):
        if line.startswith("#"):
            cur = {"title": line[1:].strip(), "w": {}}
            blocks.append(cur)
        elif line.startswith("W ") and cur is not None:
            _, a, v = line.split()
            cur["w"][int(a, 16)] = int(v, 16)
    head = [b for b in blocks if "GEMM_FUSED" in b["title"]][-1]
    m = re.search(r"cout (\d+) tokens (\d+)", head["title"])
    return head["w"][REG_OUT], int(m.group(1)), int(m.group(2)), head["title"]


def logits_at(dram, addr, cout, tokens):
    return dram[addr:addr + cout * tokens].view(np.int8).reshape(cout, tokens)[:, 0].astype(np.int64)


def top5(v):
    return [int(i) for i in np.argsort(-v, kind="stable")[:5]]


def softmax(v):
    e = np.exp(v - v.max())
    return e / e.sum()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--net-dir", required=True, help="program dir of the run (mmio.txt, dram_golden.bin, summary.json)")
    ap.add_argument("--dram", required=True, help="dram_snapshot.bin of the run (or dram_golden.bin)")
    ap.add_argument("--image", default=None, help="source image (default: summary.json name under FE_WORK/datasets)")
    ap.add_argument("--no-float", action="store_true", help="skip the float model")
    ap.add_argument("--draw", default=None)
    ap.add_argument("--json", default=None)
    args = ap.parse_args()

    summ = json.load(open(os.path.join(args.net_dir, "summary.json")))
    addr, cout, tokens, title = head_layout(os.path.join(args.net_dir, "mmio.txt"))
    snap = np.fromfile(args.dram, dtype=np.uint8)
    gold = np.fromfile(os.path.join(args.net_dir, "dram_golden.bin"), dtype=np.uint8)
    li_n, li_g = logits_at(snap, addr, cout, tokens), logits_at(gold, addr, cout, tokens)
    same = bool((snap[addr:addr + cout * tokens] == gold[addr:addr + cout * tokens]).all())
    s_log = float(summ["s_logit"])
    names = imagenet_names()
    img = args.image or next(iter(glob.glob(os.path.join(fc.FE_WORK, "datasets", "*", "images", "*", summ["image"]))), None)

    res = {"image": summ["image"], "head": title, "out_addr": hex(addr), "npu_logits_equal_golden": same,
           "npu_top5": top5(li_n), "golden_top5": top5(li_g), "npu_top1_ties": int((li_n == li_n.max()).sum())}
    pn = softmax(li_n * s_log)
    print("[fe_vit_top5] image %s, %s at %s" % (summ["image"], title, hex(addr)))
    print("[fe_vit_top5] NPU logits == golden: %s" % ("yes" if same else "NO"))
    print("[fe_vit_top5] NPU (int8) top-5:")
    for k in res["npu_top5"]:
        print("   %4d %-28s logit %4d  p %.3f" % (k, names[k], li_n[k], pn[k]))
    pf = None
    if not args.no_float and img:
        import fe_vit_float as vf
        lf, _ = vf.forward_np(vf.load_safetensors(vf.WEIGHTS), vf.preprocess(img))
        pf = softmax(lf.astype(np.float64))
        res["float_top5"] = top5(lf)
        print("[fe_vit_top5] float top-5:")
        for k in res["float_top5"]:
            print("   %4d %-28s p %.3f" % (k, names[k], pf[k]))
    if args.json:
        json.dump(res, open(args.json, "w"), indent=1)

    if args.draw and img:
        from PIL import Image, ImageDraw, ImageFont
        src = Image.open(img).convert("RGB")
        h = 420
        src = src.resize((max(1, round(src.width * h / src.height)), h))
        pw = 560
        canvas = Image.new("RGB", (src.width + pw, h), (255, 255, 255))
        canvas.paste(src, (0, 0))
        d = ImageDraw.Draw(canvas)
        try:
            f = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 15)
            fb = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 16)
        except OSError:
            f = fb = ImageFont.load_default()
        x, y = src.width + 16, 12
        d.text((x, y), "ViT-B/16 on the NPU model  (%s)" % summ["image"], fill=(0, 0, 0), font=fb)
        y += 28
        d.text((x, y), "NPU output = golden: %s" % ("yes, bit-exact" if same else "NO"), fill=(0, 110, 0) if same else (200, 0, 0), font=f)
        y += 30

        def bars(title, ids, probs, color, y):
            d.text((x, y), title, fill=(0, 0, 0), font=fb)
            y += 24
            for k in ids:
                w = int(300 * probs[k])
                d.rectangle([x, y + 2, x + max(w, 1), y + 16], fill=color)
                d.text((x + 308, y), "%.2f  %s" % (probs[k], names[k][:26]), fill=(0, 0, 0), font=f)
                y += 22
            return y + 12
        y = bars("NPU int8 logits: top-5", res["npu_top5"], pn, (200, 60, 40), y)
        if pf is not None:
            y = bars("Float model: top-5", res["float_top5"], pf, (60, 90, 200), y)
        canvas.save(args.draw)
        print("[fe_vit_top5] drew %s" % args.draw)


if __name__ == "__main__":
    main()
