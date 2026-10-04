"""Step 1: T0 torch FP32 vs T1 onnxruntime FP32.

- Exports the fused YOLOv8m (TappedYolo wrapper) to FE_WORK/onnx/yolov8m_fp32_taps.onnx
  (legacy TorchScript exporter, opset 13). Graph outputs: layer00..layer21, head_box0..2,
  head_cls0..2, final.
- Compares, per coco8 image, every graph output plus every ultralytics Conv (conv+SiLU) output
  between torch and onnxruntime. Conv outputs are located in ONNX by their scope name, and the
  script fails if any Conv module cannot be located (no silent skipping).
- Pass criterion: relative error max|ort - torch| / max|torch| < REL_TOL for every tensor.

Usage (repository root, FE_WORK set):  python3 tools/fe/fe_step1_export_check.py
"""
import hashlib
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fe_common as fc  # noqa: E402  (sets YOLO_* env before ultralytics import)

import numpy as np  # noqa: E402
import onnx  # noqa: E402
import onnxruntime as ort  # noqa: E402
import torch  # noqa: E402

REL_TOL = 1e-4
OPSET = 13


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def module_name_from_scope(node_name, suffix):
    """'/det/model.22/cv2.0/cv2.0.0/act/Mul' -> 'model.22.cv2.0.0' (suffix='/act/Mul')."""
    if not node_name.endswith(suffix):
        return None
    segs = [s for s in node_name[: -len(suffix)].split("/") if s]
    if segs and segs[0] == "det":
        segs = segs[1:]
    # A child of a container can be scoped with its qualified name ('cv2.0' -> 'cv2.0.0'):
    # then it replaces the parent segment instead of being appended.
    full = []
    for seg in segs:
        if full and seg.startswith(full[-1] + "."):
            full[-1] = seg
        else:
            full.append(seg)
    return ".".join(full)


def main():
    torch.set_num_threads(4)
    out_dir = os.path.join(fc.FE_WORK, "step1")
    onnx_dir = os.path.join(fc.FE_WORK, "onnx")
    os.makedirs(out_dir, exist_ok=True)
    os.makedirs(onnx_dir, exist_ok=True)
    onnx_path = os.path.join(onnx_dir, "yolov8m_fp32_taps.onnx")
    t0 = time.time()

    det = fc.load_fused_model()
    wrapper = fc.TappedYolo(det).eval()
    tap_names = wrapper.tap_names()
    images = fc.list_coco8_images()
    print("[step1] images:", len(images), "taps:", len(tap_names))

    from ultralytics.nn.modules.conv import Conv as UConv

    conv_modules = {n: m for n, m in det.named_modules() if isinstance(m, UConv)}
    print("[step1] ultralytics Conv modules:", len(conv_modules))

    # ---------------- export (T1 graph) ----------------
    sample = torch.from_numpy(fc.letterbox_input(images[0]))
    with torch.no_grad():
        torch.onnx.export(
            wrapper, sample, onnx_path, opset_version=OPSET, dynamo=False,
            input_names=["images"], output_names=tap_names, do_constant_folding=True,
        )
    model = onnx.load(onnx_path)
    onnx.checker.check_model(model)
    op_counts = {}
    for n in model.graph.node:
        op_counts[n.op_type] = op_counts.get(n.op_type, 0) + 1
    print("[step1] exported", onnx_path, "nodes", len(model.graph.node), "ops", op_counts)

    # ---------------- locate Conv outputs in ONNX ----------------
    graph_outputs = {o.name for o in model.graph.output}
    # TappedYolo calls the Detect head convs directly (d.cv2[i], d.cv3[i]), so their ONNX scope
    # starts at 'cv2.N' / 'cv3.N' instead of 'model.<detect_idx>.cv2.N'.
    head_prefix = "model.%d." % (len(det.model) - 1)
    conv_tensor = {}
    dup = []
    for node in model.graph.node:
        if node.op_type != "Mul":
            continue
        name = module_name_from_scope(node.name, "/act/Mul")
        if name is None:
            continue
        if name not in conv_modules and (head_prefix + name) in conv_modules:
            name = head_prefix + name
        if name in conv_tensor:
            dup.append(name)
        conv_tensor[name] = node.output[0]
    missing = sorted(set(conv_modules) - set(conv_tensor))
    extra = sorted(set(conv_tensor) - set(conv_modules))
    print("[step1] Conv located in ONNX: %d/%d, missing=%d, extra=%d, dup=%d"
          % (len(set(conv_modules) & set(conv_tensor)), len(conv_modules), len(missing), len(extra), len(dup)))
    if missing or extra or dup:
        mul_names = [n.name for n in model.graph.node if n.op_type == "Mul"][:12]
        print("  missing:", missing[:10], "\n  extra:", extra[:10], "\n  dup:", dup[:10],
              "\n  sample Mul node names:", mul_names)
        sys.exit(2)

    aug = onnx.load(onnx_path)
    for name in sorted(conv_modules):
        tname = conv_tensor[name]
        if tname not in graph_outputs:
            aug.graph.output.append(onnx.helper.make_empty_tensor_value_info(tname))
    so = ort.SessionOptions()
    so.intra_op_num_threads = 4
    sess = ort.InferenceSession(aug.SerializeToString(), so, providers=["CPUExecutionProvider"])
    ort_out_names = [o.name for o in sess.get_outputs()]

    # ---------------- run & compare ----------------
    hooks_store = {}

    def make_hook(nm):
        def hook(_m, _i, out):
            hooks_store[nm] = out.detach().numpy().copy()
        return hook

    handles = [m.register_forward_hook(make_hook(n)) for n, m in conv_modules.items()]
    worst = {}  # tensor key -> dict(rel, abs, image)
    orig_vs_tapped = 0.0
    for img_path in images:
        x = fc.letterbox_input(img_path)
        hooks_store.clear()
        with torch.no_grad():
            taps = wrapper(torch.from_numpy(x))
            conv_ref = dict(hooks_store)
            orig = det(torch.from_numpy(x))
        orig_final = orig[0] if isinstance(orig, (tuple, list)) else orig
        orig_vs_tapped = max(orig_vs_tapped, float((orig_final - taps[-1]).abs().max()))
        ort_vals = dict(zip(ort_out_names, sess.run(None, {"images": x})))

        pairs = [(tn, taps[i].numpy(), ort_vals[tn]) for i, tn in enumerate(tap_names)]
        pairs += [("conv:" + n, conv_ref[n], ort_vals[conv_tensor[n]]) for n in sorted(conv_modules)]
        for key, ref, got in pairs:
            if ref.shape != got.shape:
                print("[step1] SHAPE MISMATCH", key, ref.shape, got.shape)
                sys.exit(3)
            a = float(np.abs(got - ref).max())
            r = a / (float(np.abs(ref).max()) + 1e-12)
            w = worst.get(key)
            if w is None or r > w["rel"]:
                worst[key] = {"rel": r, "abs": a, "image": os.path.basename(img_path), "shape": list(ref.shape)}
    for h in handles:
        h.remove()

    fails = {k: v for k, v in worst.items() if not v["rel"] < REL_TOL}
    top = sorted(worst.items(), key=lambda kv: -kv[1]["rel"])[:10]
    report = {
        "onnx_path": onnx_path, "onnx_sha256": sha256(onnx_path), "opset": OPSET,
        "torch": torch.__version__, "onnxruntime": ort.__version__, "onnx": onnx.__version__,
        "images": [os.path.basename(p) for p in images], "rel_tol": REL_TOL,
        "n_tensors_compared": len(worst), "n_fail": len(fails),
        "tapped_vs_original_final_maxabs": orig_vs_tapped,
        "op_counts": op_counts, "worst10": top, "all": worst,
        "elapsed_s": round(time.time() - t0, 1),
    }
    with open(os.path.join(out_dir, "report.json"), "w") as f:
        json.dump(report, f, indent=1)

    print("[step1] tapped wrapper vs original model final: max abs diff = %.3g" % orig_vs_tapped)
    print("[step1] tensors compared per image: %d over %d images" % (len(worst), len(images)))
    print("[step1] worst 10 (rel, abs, image, shape):")
    for k, v in top:
        print("   %-34s rel=%.3e abs=%.3e %s %s" % (k, v["rel"], v["abs"], v["image"], v["shape"]))
    print("[step1] RESULT: %s (%d/%d tensors rel < %.0e), elapsed %.1fs"
          % ("PASS" if not fails else "FAIL", len(worst) - len(fails), len(worst), REL_TOL, time.time() - t0))
    sys.exit(0 if not fails else 1)


if __name__ == "__main__":
    main()
