"""Quality track (parallel to step 6): hold-out calibration and MSE clipping.

Calibrates on the 4 coco8 'train' images only, then writes two candidate configs next to the step-3 ones:
  step3/pc_p9999_rc_ho   per-channel weights, p99.99 activation range, rounding compensation
  step3/pc_mse_rc_ho     same, activation range = clip value minimizing the estimated int8 MSE
Evaluate them on the 4 'val' images with:
  python3 -u tools/fe/fe_step5_quality.py --split val --report report_holdout.json \
      --config pc_p9999_rc --config pc_p9999_rc_ho --config pc_mse_rc_ho
The chosen golden config (pc_p9999_rc, calibrated on all 8 images) is not modified.

MSE clip per tensor: histogram of |x| (HIST_BINS bins over [0, max]) aggregated over the calibration images;
for candidate clip c the error is sum(n * (v - min(round(v / s), 127) * s)^2) with s = c / 127.
A scale group takes the largest clip of its members (the group must represent all of them).
"""
import json
import os
import shutil
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fe_common as fc  # noqa: E402
import fe_graph as fg  # noqa: E402
import fe_quant as fq  # noqa: E402
import fe_step3_quant as s3  # noqa: E402

import numpy as np  # noqa: E402
import onnx  # noqa: E402

HIST_BINS = 2048
CANDIDATES = np.linspace(0.05, 1.0, 96)


def mse_clip(hist, vmax):
    edges = np.linspace(0.0, vmax, HIST_BINS + 1)
    v = 0.5 * (edges[:-1] + edges[1:])
    best_c, best_e = vmax, None
    for f in CANDIDATES:
        c = f * vmax
        s = c / fq.QMAX
        q = np.minimum(np.floor(v / s + 0.5), fq.QMAX) * s
        e = float((hist * (v - q) ** 2).sum())
        if best_e is None or e < best_e:
            best_c, best_e = c, e
    return best_c


def main():
    t0 = time.time()
    ir, bstats, weights = fg.build_ir(onnx.load(os.path.join(fc.FE_WORK, "onnx", "yolov8m_fp32_taps.onnx")))
    if bstats["unrecognized"]:
        raise SystemExit("IR build has unrecognized nodes")
    train = fc.list_coco8_images("train")
    inputs = [fc.letterbox_input(p) for p in train]
    stats = fq.calibrate(ir, weights, inputs, percentile=99.99)
    hists = {n: np.zeros(HIST_BINS) for n in stats}
    for x in inputs:
        vals = fg.run_ir_float(ir, weights, x, keep_preact=True)
        for n, v in vals.items():
            if stats[n]["max"] > 0:
                h, _ = np.histogram(np.abs(v).reshape(-1), bins=HIST_BINS, range=(0.0, stats[n]["max"]))
                hists[n] += h
    for n, s in stats.items():
        s["mse"] = mse_clip(hists[n], s["max"]) if s["max"] > 0 else 0.0
    ratio = np.array([s["mse"] / s["max"] for s in stats.values() if s["max"] > 0])
    print("[holdout] calibrated %d tensors on %d train images; mse clip / max: median %.3f min %.3f"
          % (len(stats), len(train), float(np.median(ratio)), float(ratio.min())))
    with open(os.path.join(fc.FE_WORK, "step3", "calib_stats_train_ho.json"), "w") as f:
        json.dump({"images": [os.path.basename(p) for p in train], "stats": stats}, f, indent=1)

    tmp_root = os.path.join(fc.FE_WORK, "step3_ho_tmp")
    os.makedirs(tmp_root, exist_ok=True)
    ok = True
    for method in ("p9999", "mse"):
        ok &= s3.run_config(ir, weights, stats, "per_channel", method, True, tmp_root)
        name = s3.config_name("per_channel", method, True)
        dst = os.path.join(fc.FE_WORK, "step3", name + "_ho")
        if os.path.exists(dst):
            shutil.rmtree(dst)
        shutil.move(os.path.join(tmp_root, name), dst)
        print("[holdout] wrote", dst)
    shutil.rmtree(tmp_root, ignore_errors=True)
    print("[holdout] %s, elapsed %.1fs" % ("PASS" if ok else "FAIL (overflow checks)", time.time() - t0))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
