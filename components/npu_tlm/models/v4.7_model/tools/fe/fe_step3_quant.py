"""Step 3: calibration + integer parameters.

- Calibrates every IR tensor (and SiLU pre-activation) on the 8 coco8 images: max|x| and p99.99|x|.
- Builds equal-scale groups over slice/concat/add/maxpool/upsample.
- For each config computes per conv: q_w (int8), q_b (int32), scale (uint32), shift, SiLU LUT (int8[256]).
- Static checks per conv: psum and psum+bias fit int32, (psum+bias)*scale fits int64, scale/shift
  in range, multiplier representation error < M_ERR_TOL.

Outputs: FE_WORK/step3/calib_stats.json and FE_WORK/step3/<config>/{params.npz, params.json}

Usage (repository root, FE_WORK set):
  python3 tools/fe/fe_step3_quant.py                       # configs pc_max and pt_max
  python3 tools/fe/fe_step3_quant.py --wq per_channel --method p9999 --round-comp
"""
import argparse
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fe_common as fc  # noqa: E402
import fe_graph as fg  # noqa: E402
import fe_quant as fq  # noqa: E402

import numpy as np  # noqa: E402
import onnx  # noqa: E402

M_ERR_TOL = 1e-6


def config_name(wq, method, round_comp):
    return "%s_%s%s" % ({"per_channel": "pc", "per_tensor": "pt"}[wq], method, "_rc" if round_comp else "")


def run_config(ir, weights, stats, wq, method, round_comp, out_root):
    name = config_name(wq, method, round_comp)
    out_dir = os.path.join(out_root, name)
    os.makedirs(out_dir, exist_ok=True)
    scales, group_of = fq.tensor_scales(ir, stats, method)
    groups = {}
    for t, rep in group_of.items():
        groups.setdefault(rep, []).append(t)

    arrays, convs, fails = {}, [], []
    for o in ir["ops"]:
        if o["op"] != "conv":
            continue
        p, s = fq.quantize_conv(o, weights, scales, wq=wq, round_comp=round_comp)
        for k, v in p.items():
            arrays["%s/%s" % (o["job"], k)] = v
        convs.append(s)
        bad = [k for k in ("psum_ok", "biased_ok", "product_ok", "bias_ok") if not s[k]]
        if bad or not s["M_rel_err_max"] < M_ERR_TOL:
            fails.append((o["job"], bad, s["M_rel_err_max"]))

    np.savez(os.path.join(out_dir, "params.npz"), **arrays)
    doc = {
        "config": {"wq": wq, "method": method, "round_comp": round_comp, "input_scale": fq.INPUT_SCALE,
                   "qmax": fq.QMAX, "rounding": "offline round half away from zero; OBP requant floor"},
        "tensor_scales": scales,
        "groups": {rep: sorted(ms) for rep, ms in groups.items()},
        "convs": convs,
        "fails": fails,
    }
    with open(os.path.join(out_dir, "params.json"), "w") as f:
        json.dump(doc, f, indent=1)

    multi = sorted((len(ms) for ms in groups.values() if len(ms) > 1), reverse=True)
    print("[step3] config %s: groups=%d (multi-tensor %d, sizes %s)" % (name, len(groups), len(multi), multi))
    agg = lambda k, fn: fn(c[k] for c in convs)  # noqa: E731
    print("   shift %d..%d | scale %d..%d | M %.3e..%.3e | M rel err max %.2e"
          % (agg("shift_min", min), agg("shift_max", max), agg("scale_min", min), agg("scale_max", max),
             agg("M_min", min), agg("M_max", max), agg("M_rel_err_max", max)))
    print("   q_b |max| %d | psum bound max %d | biased bound max %d | product bound max %.3e (< 9.22e18)"
          % (agg("q_b_absmax", max), agg("psum_bound_max", max), agg("biased_bound_max", max),
             float(agg("product_bound_max", max))))
    print("   zero-weight channels %d | LUT saturated entries total %d | worst s_w max/min %.1f"
          % (agg("zero_weight_channels", sum), sum(c.get("lut_sat_entries", 0) for c in convs),
             max(c["s_w_max"] / c["s_w_min"] for c in convs)))
    print("   result %s%s" % ("PASS" if not fails else "FAIL", "" if not fails else " %s" % fails[:5]))
    return not fails


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wq", choices=("per_channel", "per_tensor"), action="append")
    ap.add_argument("--method", choices=("max", "p9999"), default="max")
    ap.add_argument("--round-comp", action="store_true")
    args = ap.parse_args()
    wqs = args.wq or ["per_channel", "per_tensor"]

    t0 = time.time()
    out_root = os.path.join(fc.FE_WORK, "step3")
    os.makedirs(out_root, exist_ok=True)
    ir, stats_build, weights = fg.build_ir(onnx.load(os.path.join(fc.FE_WORK, "onnx", "yolov8m_fp32_taps.onnx")))
    if stats_build["unrecognized"]:
        raise SystemExit("IR build has unrecognized nodes; rerun step 2")

    calib_path = os.path.join(out_root, "calib_stats.json")
    if os.path.exists(calib_path):
        with open(calib_path) as f:
            calib = json.load(f)
        print("[step3] reusing", calib_path)
    else:
        images = fc.list_coco8_images()
        inputs = [fc.letterbox_input(p) for p in images]
        stats = fq.calibrate(ir, weights, inputs, percentile=99.99)
        calib = {"images": [os.path.basename(p) for p in images], "percentile": 99.99, "stats": stats}
        with open(calib_path, "w") as f:
            json.dump(calib, f, indent=1)
        print("[step3] calibrated %d tensors on %d images (%.1fs)" % (len(stats), len(images), time.time() - t0))
    stats = calib["stats"]
    missing = [o["output"] for o in ir["ops"] if o["output"] not in stats]
    if missing:
        raise SystemExit("calib stats missing tensors: %s" % missing[:5])

    ok = True
    for wq in wqs:
        ok &= run_config(ir, weights, stats, wq, args.method, args.round_comp, out_root)
    print("[step3] RESULT: %s, elapsed %.1fs" % ("PASS" if ok else "FAIL", time.time() - t0))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
