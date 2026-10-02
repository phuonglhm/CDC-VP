"""Step 5: int8 (T2) quality vs FP32, per config.

Per image:
  - FP32 reference: IR float tensors (fe_graph.run_ir_float) and onnxruntime 'final'.
  - Decode check: numpy decode of the onnxruntime head tensors must reproduce the model's 'final'
    (relative error < DECODE_TOL), otherwise the script aborts — detection metrics would be untrusted.
  - Per config: T2 int8 run, dequantize every IR tensor with its scale, cosine similarity vs float;
    SQNR on the 6 head tensors; detections (conf 0.25, NMS IoU 0.7) matched to FP32 detections
    (same class, IoU >= 0.5).
NOTE: calibration used the same 8 coco8 images (in-sample); 8 images cannot measure mAP. The numbers
compare configs against FP32, they are not an accuracy claim.

Usage (repository root, FE_WORK set):  python3 -u tools/fe/fe_step5_quality.py [--config pc_max ...] [--images 8]
"""
import argparse
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fe_common as fc  # noqa: E402
import fe_eval as fe  # noqa: E402
import fe_graph as fg  # noqa: E402
import fe_ref_int8 as fr  # noqa: E402

import numpy as np  # noqa: E402
import onnx  # noqa: E402
import onnxruntime as ort  # noqa: E402

DECODE_TOL = 1e-4
DEFAULT_CONFIGS = ["pc_max", "pc_p9999", "pc_max_rc", "pc_p9999_rc", "pt_max", "pt_p9999"]
HEADS_BOX = ["head_box0", "head_box1", "head_box2"]
HEADS_CLS = ["head_cls0", "head_cls1", "head_cls2"]


def op_label(o):
    return o["job"] if o["op"] == "conv" else "%s#%d" % (o["op"], o["id"])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", action="append")
    ap.add_argument("--images", type=int, default=8)
    ap.add_argument("--split", choices=("all", "train", "val"), default="all")
    ap.add_argument("--report", default="report.json")
    args = ap.parse_args()
    configs = args.config or DEFAULT_CONFIGS
    t0 = time.time()
    out_dir = os.path.join(fc.FE_WORK, "step5")
    os.makedirs(out_dir, exist_ok=True)

    onnx_path = os.path.join(fc.FE_WORK, "onnx", "yolov8m_fp32_taps.onnx")
    ir, bstats, weights = fg.build_ir(onnx.load(onnx_path))
    if bstats["unrecognized"]:
        raise SystemExit("IR build has unrecognized nodes")
    label = {o["output"]: op_label(o) for o in ir["ops"]}
    cfg_data = {}
    for cfg in configs:
        d = os.path.join(fc.FE_WORK, "step3", cfg)
        if not os.path.exists(os.path.join(d, "params.npz")):
            raise SystemExit("missing step3 config %s (run fe_step3_quant.py for it first)" % cfg)
        with open(os.path.join(d, "params.json")) as f:
            scales = json.load(f)["tensor_scales"]
        cfg_data[cfg] = (dict(np.load(os.path.join(d, "params.npz"))), scales)

    so = ort.SessionOptions()
    so.intra_op_num_threads = int(os.environ.get("FE_THREADS", "4"))
    sess = ort.InferenceSession(onnx_path, so, providers=["CPUExecutionProvider"])
    names = [o.name for o in sess.get_outputs()]

    acc = {c: {"tensor_min_cos": {}, "head_sqnr": [], "head_cos": [], "tp": 0, "n_ref": 0, "n_got": 0,
               "ious": [], "dconf": []} for c in configs}
    decode_err_max = 0.0
    images = fc.list_coco8_images(args.split)[: args.images]
    for k, img in enumerate(images):
        ti = time.time()
        x = fc.letterbox_input(img)
        ov = dict(zip(names, sess.run(None, {"images": x})))
        dec = fe.decode([ov[n] for n in HEADS_BOX], [ov[n] for n in HEADS_CLS])
        final = ov["final"][0].astype(np.float64)
        err = float(np.abs(dec - final).max() / (np.abs(final).max() + 1e-12))
        decode_err_max = max(decode_err_max, err)
        if not err < DECODE_TOL:
            raise SystemExit("decode check FAILED on %s: rel err %.3e" % (img, err))

        fv = fg.run_ir_float(ir, weights, x)
        det_fp = fe.detections(fe.decode([fv[n] for n in HEADS_BOX], [fv[n] for n in HEADS_CLS]))
        for cfg in configs:
            params, scales = cfg_data[cfg]
            qv = fr.run_int8(ir, params, x)
            a = acc[cfg]
            deq = {}
            for o in ir["ops"]:
                t = o["output"]
                d = qv[t].astype(np.float32) * np.float32(scales[t])
                c = fe.cosine(fv[t], d)
                a["tensor_min_cos"][t] = min(a["tensor_min_cos"].get(t, 1.0), c)
                if t in HEADS_BOX or t in HEADS_CLS:
                    deq[t] = d
                    a["head_sqnr"].append(fe.sqnr_db(fv[t], d))
                    a["head_cos"].append(c)
            det_q = fe.detections(fe.decode([deq[n] for n in HEADS_BOX], [deq[n] for n in HEADS_CLS]))
            m = fe.match(det_fp, det_q)
            for key in ("tp", "n_ref", "n_got"):
                a[key] += m[key]
            a["ious"] += m["ious"]
            a["dconf"] += m["dconf"]
        print("[step5] image %d/%d %s: decode rel err %.2e, FP32 dets %d, %.1fs"
              % (k + 1, len(images), os.path.basename(img), err, len(det_fp), time.time() - ti))

    rows, report = [], {"images": [os.path.basename(p) for p in images], "decode_err_max": decode_err_max,
                        "split": args.split, "configs": {}}
    for cfg in configs:
        a = acc[cfg]
        tcos = a["tensor_min_cos"]
        worst = sorted(tcos.items(), key=lambda kv: kv[1])[:5]
        p = a["tp"] / a["n_got"] if a["n_got"] else float("nan")
        r = a["tp"] / a["n_ref"] if a["n_ref"] else float("nan")
        f1 = 2 * p * r / (p + r) if p + r > 0 else 0.0
        summary = {
            "tensor_cos_min": min(tcos.values()), "tensor_cos_median_of_min": float(np.median(list(tcos.values()))),
            "worst5": [(label[t], c) for t, c in worst],
            "head_cos_min": min(a["head_cos"]), "head_sqnr_min_db": min(a["head_sqnr"]),
            "head_sqnr_mean_db": float(np.mean(a["head_sqnr"])),
            "det_fp32": a["n_ref"], "det_int8": a["n_got"], "det_tp": a["tp"], "precision": p, "recall": r, "f1": f1,
            "iou_mean": float(np.mean(a["ious"])) if a["ious"] else None,
            "dconf_mean": float(np.mean(a["dconf"])) if a["dconf"] else None,
            "dconf_max": float(np.max(a["dconf"])) if a["dconf"] else None,
        }
        report["configs"][cfg] = summary
        rows.append(summary)
        print("[step5] %-12s cos min %.4f (median %.4f) | head cos min %.4f, SQNR min/mean %.1f/%.1f dB | "
              "dets fp32 %d int8 %d tp %d P %.3f R %.3f F1 %.3f | IoU %.3f dconf mean/max %.3f/%.3f"
              % (cfg, summary["tensor_cos_min"], summary["tensor_cos_median_of_min"], summary["head_cos_min"],
                 summary["head_sqnr_min_db"], summary["head_sqnr_mean_db"], a["n_ref"], a["n_got"], a["tp"],
                 p, r, f1, summary["iou_mean"] or 0, summary["dconf_mean"] or 0, summary["dconf_max"] or 0))
        print("               worst tensors:", ", ".join("%s=%.4f" % (n, c) for n, c in summary["worst5"]))
    report["elapsed_s"] = round(time.time() - t0, 1)
    with open(os.path.join(out_dir, args.report), "w") as f:
        json.dump(report, f, indent=1)
    print("[step5] done, decode rel err max %.2e, elapsed %.1fs" % (decode_err_max, time.time() - t0))


if __name__ == "__main__":
    main()
