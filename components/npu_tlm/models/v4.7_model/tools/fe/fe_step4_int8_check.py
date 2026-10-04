"""Step 4: T2 int8 reference + T3 independent checks.

For every config and every coco8 image, runs T2 (fe_ref_int8.run_int8) over the whole IR and checks:
  T3a  psum of every conv == onnxruntime ConvInteger on the same int8 input/weights (exact).
       The input is pre-padded on the host (uint8 = int8 + 128, zero point 128, pads=0), so the
       padding convention of ConvInteger cannot affect the result.
  T3b  requant + LUT recomputed with Python integers on SAMPLES random elements per conv (exact).
PASS = zero mismatches in T3a and T3b.
Saves the T2 int8 tensors of the first image as golden: FE_WORK/step4/<config>/golden_<image>.npz

Usage (repository root, FE_WORK set):  python3 tools/fe/fe_step4_int8_check.py [--config pc_max] [--images 8]
"""
import argparse
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fe_common as fc  # noqa: E402
import fe_ref_int8 as fr  # noqa: E402

import numpy as np  # noqa: E402
import onnx  # noqa: E402
import onnxruntime as ort  # noqa: E402
from onnx import TensorProto, helper, numpy_helper  # noqa: E402

SAMPLES = 2000


def convinteger_session(o, q_w):
    cout, cin, kh, kw = q_w.shape
    _, _, h, w = o["in_shapes"][0]
    pt, pl, pb, pr = o["pad"]
    x = helper.make_tensor_value_info("x", TensorProto.UINT8, [1, cin, h + pt + pb, w + pl + pr])
    y = helper.make_tensor_value_info("y", TensorProto.INT32, list(o["out_shape"]))
    # onnxruntime CPU has no ConvInteger kernel for int8 weights: shift both sides to uint8 with
    # zero point 128, which keeps (x_u - 128) * (w_u - 128) == q_in * q_w exactly.
    inits = [numpy_helper.from_array((q_w.astype(np.int16) + 128).astype(np.uint8), "w"),
             numpy_helper.from_array(np.array(128, dtype=np.uint8), "x_zp"),
             numpy_helper.from_array(np.array(128, dtype=np.uint8), "w_zp")]
    node = helper.make_node("ConvInteger", ["x", "w", "x_zp", "w_zp"], ["y"], kernel_shape=[kh, kw],
                            strides=list(o["stride"]), pads=[0, 0, 0, 0], dilations=list(o["dilation"]))
    graph = helper.make_graph([node], "ci_" + o["job"], [x], [y], initializer=inits)
    model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 13)])
    model.ir_version = 8
    onnx.checker.check_model(model)
    so = ort.SessionOptions()
    so.intra_op_num_threads = int(os.environ.get("FE_THREADS", "4"))
    return ort.InferenceSession(model.SerializeToString(), so, providers=["CPUExecutionProvider"])


def to_uint8_padded(q_in, pad):
    pt, pl, pb, pr = pad
    u = (q_in.astype(np.int16) + 128).astype(np.uint8)
    return np.pad(u, ((0, 0), (0, 0), (pt, pb), (pl, pr)), constant_values=128)


def py_requant_lut(psum, q_b, scale, shift, lut, rng):
    """Python-int recomputation on random elements. Returns (#qpre mismatches, #lut mismatches) closure."""
    _, cout, ho, wo = psum.shape
    idx = [(int(rng.integers(cout)), int(rng.integers(ho)), int(rng.integers(wo))) for _ in range(SAMPLES)]
    exp_pre, exp_out = {}, {}
    for c, i, j in idx:
        v = ((int(psum[0, c, i, j]) + int(q_b[c])) * int(scale[c])) >> int(shift[c])  # Python >> floors
        v = max(-128, min(127, v))
        exp_pre[(c, i, j)] = v
        exp_out[(c, i, j)] = int(lut[v + 128]) if lut is not None else v
    return exp_pre, exp_out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", action="append")
    ap.add_argument("--images", type=int, default=8)
    args = ap.parse_args()
    configs = args.config or ["pc_max", "pt_max"]
    t0 = time.time()
    with open(os.path.join(fc.FE_WORK, "step2", "graph_ir.json")) as f:
        ir = json.load(f)
    convs = [o for o in ir["ops"] if o["op"] == "conv"]
    images = fc.list_coco8_images()[: args.images]
    rng = np.random.default_rng(20260917)
    ok_all = True

    for cfg in configs:
        out_dir = os.path.join(fc.FE_WORK, "step4", cfg)
        os.makedirs(out_dir, exist_ok=True)
        params = dict(np.load(os.path.join(fc.FE_WORK, "step3", cfg, "params.npz")))
        sessions = {o["job"]: convinteger_session(o, params[o["job"] + "/q_w"]) for o in convs}
        t3a_bad, t3b_bad, sat_pre, n_pre = [], [], 0, 0
        timing = []
        for k, img in enumerate(images):
            ti = time.time()
            vals = fr.run_int8(ir, params, fc.letterbox_input(img), keep_psum=True)
            t_t2 = time.time() - ti
            for o in convs:
                j = o["job"]
                src = vals[o["inputs"][0]]
                psum = vals[o["output"] + "@psum"]
                ref = sessions[j].run(None, {"x": to_uint8_padded(src, o["pad"])})[0].astype(np.int64)
                if ref.shape != psum.shape or not np.array_equal(ref, psum):
                    diff = -1 if ref.shape != psum.shape else int((ref != psum).sum())
                    t3a_bad.append((os.path.basename(img), j, diff))
                qpre = vals[o["output"] + "@qpre"]
                qout = vals[o["output"]]
                sat_pre += int(((qpre == 127) | (qpre == -128)).sum())
                n_pre += qpre.size
                lut = params.get(j + "/lut")
                exp_pre, exp_out = py_requant_lut(psum, params[j + "/q_b"], params[j + "/scale"],
                                                  params[j + "/shift"], lut, rng)
                bad = sum(int(qpre[0, c, i, jj]) != v for (c, i, jj), v in exp_pre.items())
                bad += sum(int(qout[0, c, i, jj]) != v for (c, i, jj), v in exp_out.items())
                if bad:
                    t3b_bad.append((os.path.basename(img), j, bad))
            timing.append((os.path.basename(img), round(t_t2, 1), round(time.time() - ti, 1)))
            print("[step4] %s image %d/%d %s: T2 %.1fs, total %.1fs, T3a fails so far %d, T3b fails so far %d"
                  % (cfg, k + 1, len(images), os.path.basename(img), t_t2, time.time() - ti, len(t3a_bad), len(t3b_bad)))
            if k == 0:
                golden = {name: v for name, v in vals.items() if not name.endswith("@psum")}
                gpath = os.path.join(out_dir, "golden_%s.npz" % os.path.splitext(os.path.basename(img))[0])
                np.savez_compressed(gpath, **golden)
        ok = not t3a_bad and not t3b_bad
        ok_all &= ok
        rep = {"config": cfg, "images": [os.path.basename(p) for p in images], "convs": len(convs),
               "T3a_psum_exact_fail": t3a_bad, "T3b_requant_lut_fail": t3b_bad, "samples_per_conv": SAMPLES,
               "qpre_saturation_frac": sat_pre / max(1, n_pre), "timing": timing, "pass": ok}
        with open(os.path.join(out_dir, "report.json"), "w") as f:
            json.dump(rep, f, indent=1)
        print("[step4] %s: T3a psum exact %d/%d conv-images, T3b requant/LUT samples fails %d, q_pre saturation %.4f%% -> %s"
              % (cfg, len(convs) * len(images) - len(t3a_bad), len(convs) * len(images), len(t3b_bad),
                 100.0 * sat_pre / max(1, n_pre), "PASS" if ok else "FAIL"))
    print("[step4] RESULT: %s, elapsed %.1fs" % ("PASS" if ok_all else "FAIL", time.time() - t0))
    sys.exit(0 if ok_all else 1)


if __name__ == "__main__":
    main()
