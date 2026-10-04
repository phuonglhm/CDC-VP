"""SystemC output (DRAM snapshot) -> tools/fe/fe_eval.py decode() + detections() -> detection boxes.

Decode is not re-implemented: reads the 6 int8 head tensors from DRAM at the manifest.json addresses, dequantises
them with the config scales, then calls the unmodified fe_eval.decode() / fe_eval.detections() (DFL 16 bins,
anchors, strides 8/16/32, NMS conf .25 iou .7 -- as checked by fe_step5_quality.py).

DRAM source (any file with the dram_golden.bin layout and `dram_bytes` size):
  - fe_work/step6b/net/dram_golden.bin      : int8 golden (checks the pipeline without running SystemC)
  - <FE_SNAPSHOT_DIR>/dram_snapshot.bin     : snapshot after the last layer of a tb_fe_core_net run (SystemC output)
numpy only (no torch / ultralytics); the original image is only needed to map boxes back to it / draw them (PIL).

Usage:
  python3 tools/fe/fe_sysc_detect.py --dram fe_work/step6b/net/dram_golden.bin
  python3 tools/fe/fe_sysc_detect.py --dram <snap>/dram_snapshot.bin --compare-golden --json out.json --draw out.png
"""
import argparse
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fe_eval as fe  # noqa: E402  (numpy only)

FE_WORK = os.environ.get("FE_WORK", "")
if not FE_WORK:
    raise SystemExit("FE_WORK is not set: export FE_WORK=<work directory>")
HEADS_BOX = ["head_box0", "head_box1", "head_box2"]
HEADS_CLS = ["head_cls0", "head_cls1", "head_cls2"]
IMGSZ = 640
COCO80 = ["person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light",
          "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow",
          "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
          "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard",
          "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple",
          "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
          "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard", "cell phone",
          "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear",
          "hair drier", "toothbrush"]
# All 80 COCO names in ultralytics order: a missing name shifts every later class index by one.
assert len(COCO80) == 80 and COCO80[45] == "bowl" and COCO80[50] == "broccoli", "COCO80 must hold 80 names in ultralytics order"


def load_heads(dram, manifest, scales):
    """Read the 6 int8 head tensors from DRAM, dequantise each with its scale -> list of float32 [1,C,H,W]."""
    out = {}
    for name in HEADS_BOX + HEADS_CLS:
        t = manifest["tensors"][name]
        n = int(np.prod(t["shape"]))
        raw = dram[t["addr"]:t["addr"] + n].view(np.int8)
        out[name] = (raw.reshape(t["shape"]).astype(np.float32) * np.float32(scales[name]))
    return [out[n] for n in HEADS_BOX], [out[n] for n in HEADS_CLS]


def letterbox_params(w0, h0, imgsz=IMGSZ):
    """ultralytics LetterBox parameters (auto=False, centered): r = min(imgsz/h0, imgsz/w0), padding split evenly."""
    r = min(imgsz / h0, imgsz / w0)
    nw, nh = round(w0 * r), round(h0 * r)
    return r, (imgsz - nw) / 2.0, (imgsz - nh) / 2.0


def to_original(det, w0, h0):
    """Boxes in letterbox 640x640 coordinates -> original image coordinates (clipped to the image)."""
    r, dx, dy = letterbox_params(w0, h0)
    o = det.copy()
    o[:, [0, 2]] = ((o[:, [0, 2]] - dx) / r).clip(0, w0)
    o[:, [1, 3]] = ((o[:, [1, 3]] - dy) / r).clip(0, h0)
    return o


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dram", required=True, help="dram_golden.bin or dram_snapshot.bin")
    ap.add_argument("--net-dir", default=os.path.join(FE_WORK, "step6b", "net"))
    ap.add_argument("--image", default=None, help="original image (default: the name in the manifest, coco8 directory)")
    ap.add_argument("--compare-golden", action="store_true",
                    help="compare the boxes of this DRAM with those of dram_golden.bin (same config) -> P/R/IoU")
    ap.add_argument("--json", default=None)
    ap.add_argument("--draw", default=None, help="draw the boxes on the original image, save a PNG (needs PIL)")
    args = ap.parse_args()

    with open(os.path.join(args.net_dir, "manifest.json")) as f:
        manifest = json.load(f)
    cfg = manifest["config"]
    with open(os.path.join(FE_WORK, "step3", cfg, "params.json")) as f:
        scales = json.load(f)["tensor_scales"]

    dram = np.fromfile(args.dram, dtype=np.uint8)
    if dram.size != manifest["dram_bytes"]:
        raise SystemExit("DRAM size %d != manifest %d -- not a snapshot of this network"
                         % (dram.size, manifest["dram_bytes"]))

    box, cls = load_heads(dram, manifest, scales)
    pred = fe.decode(box, cls)
    dets = fe.detections(pred)
    print("[fe_sysc_detect] config=%s image=%s dram=%s" % (cfg, manifest["image"], args.dram))
    print("[fe_sysc_detect] %d boxes (conf>0.25, NMS iou 0.7)" % len(dets))

    result = {"config": cfg, "image": manifest["image"], "dram": args.dram, "n_det": int(len(dets)),
              "detections_letterbox640": dets.tolist()}

    w0 = h0 = None
    img_path = args.image
    if img_path is None:
        for sub in ("train", "val"):
            p = os.path.join(FE_WORK, "datasets", "coco8", "images", sub, manifest["image"])
            if os.path.exists(p):
                img_path = p
                break
    if img_path and os.path.exists(img_path):
        try:
            from PIL import Image
            with Image.open(img_path) as im:
                w0, h0 = im.size
        except Exception as e:  # noqa: BLE001
            print("[fe_sysc_detect] cannot read the image size (%s) -- boxes stay in network coordinates" % e)
    if w0:
        orig = to_original(dets, w0, h0) if len(dets) else dets
        result["detections_original"] = orig.tolist()
        result["original_size"] = [w0, h0]

    for k, d in enumerate(dets):
        shown = to_original(dets[k:k + 1], w0, h0)[0] if w0 else d
        print("  #%02d %-14s conf=%.3f  box=[%.1f, %.1f, %.1f, %.1f]%s"
              % (k, COCO80[int(d[5])], d[4], shown[0], shown[1], shown[2], shown[3],
                 "  (original image coordinates)" if w0 else "  (letterbox 640 coordinates)"))

    if args.compare_golden:
        gold_path = os.path.join(args.net_dir, "dram_golden.bin")
        gdram = np.fromfile(gold_path, dtype=np.uint8)
        gbox, gcls = load_heads(gdram, manifest, scales)
        gdets = fe.detections(fe.decode(gbox, gcls))
        m = fe.match(gdets, dets)
        p = m["tp"] / m["n_got"] if m["n_got"] else float("nan")
        r = m["tp"] / m["n_ref"] if m["n_ref"] else float("nan")
        # Compare the TENSOR regions only: the whole DRAM never equals the golden because the 4 staging buffers
        # (A/SKIP/PRE/OUT) keep leftovers of the last tile (all differing bytes lie outside the tensor regions).
        tmask = np.zeros(dram.size, dtype=bool)
        for t in manifest["tensors"].values():
            tmask[t["addr"]:t["addr"] + int(np.prod(t["shape"]))] = True
        neq = dram[:manifest["dram_bytes"]] != gdram
        n_in, n_out = int((neq & tmask).sum()), int((neq & ~tmask).sum())
        exact = int(n_in == 0)
        result["vs_golden"] = {"n_ref": m["n_ref"], "n_got": m["n_got"], "tp": m["tp"], "precision": p,
                               "recall": r, "iou_mean": float(np.mean(m["ious"])) if m["ious"] else None,
                               "dconf_max": float(np.max(m["dconf"])) if m["dconf"] else None,
                               "tensors_identical_to_golden": bool(exact),
                               "bytes_diff_in_tensors": n_in, "bytes_diff_outside_tensors_staging": n_out}
        print("[fe_sysc_detect] against the golden: ref=%d got=%d tp=%d P=%.3f R=%.3f IoU_mean=%s dconf_max=%s"
              % (m["n_ref"], m["n_got"], m["tp"], p, r,
                 "%.4f" % np.mean(m["ious"]) if m["ious"] else "-",
                 "%.4f" % np.max(m["dconf"]) if m["dconf"] else "-"))
        print("[fe_sysc_detect] mismatching bytes in the tensor region = %d (must be 0); outside it (staging) = %d" % (n_in, n_out))

    if args.json:
        with open(args.json, "w") as f:
            json.dump(result, f, indent=1)
        print("[fe_sysc_detect] wrote %s" % args.json)

    if args.draw and img_path and os.path.exists(img_path) and w0:
        from PIL import Image, ImageDraw
        im = Image.open(img_path).convert("RGB")
        dr = ImageDraw.Draw(im)
        for d in orig:
            dr.rectangle([d[0], d[1], d[2], d[3]], outline=(255, 0, 0), width=2)
            dr.text((d[0] + 2, d[1] + 2), "%s %.2f" % (COCO80[int(d[5])], d[4]), fill=(255, 255, 0))
        im.save(args.draw)
        print("[fe_sysc_detect] drew %s" % args.draw)


if __name__ == "__main__":
    main()
