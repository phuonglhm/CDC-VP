"""Step 2: frontend reads the ONNX graph.

Checks (all must hold for PASS):
  A. 0 unrecognized nodes in the cone of the 6 head convs, all cut tensors reached.
  B. 83 conv ops (77 SiLU + 6 without act), each mapped to a sauria_model job name.
  C. Every conv matches the sauria_model job list (kernel, dilation, stride, Cin, out W/H, Cout),
     every job is covered, and pads are symmetric k//2.
  D. The IR executed with torch.nn.functional matches onnxruntime on the tapped tensors
     (layer00..21, head_*) with relative error < REL_TOL (proves the IR interprets the graph right).

Outputs: FE_WORK/step2/{graph_ir.json, conv_table.tsv, report.json}

Usage (repository root, FE_WORK set):  python3 tools/fe/fe_step2_graph_check.py [jobs.tsv]
"""
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fe_common as fc  # noqa: E402
import fe_graph as fg  # noqa: E402

import numpy as np  # noqa: E402
import onnx  # noqa: E402
import onnxruntime as ort  # noqa: E402

REL_TOL = 1e-4
DEFAULT_JOBS = os.path.expanduser("~/sauria_model/yolov8m_640_32x32_seeded.tsv")


def read_jobs(path):
    jobs = {}
    with open(path) as f:
        for line in f:
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            cols = line.rstrip("\n").split("\t")
            a = [int(v) for v in cols[1].split()]
            jobs[cols[0]] = {"Bw": a[0], "Bh": a[1], "d": a[2], "s": a[3], "Cin": a[4],
                             "Cw": a[5], "Ch": a[6], "Cout": a[7]}
    return jobs


def main():
    t0 = time.time()
    jobs_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_JOBS
    onnx_path = os.path.join(fc.FE_WORK, "onnx", "yolov8m_fp32_taps.onnx")
    out_dir = os.path.join(fc.FE_WORK, "step2")
    os.makedirs(out_dir, exist_ok=True)
    fails = []

    ir, stats, weights = fg.build_ir(onnx.load(onnx_path))
    ops = ir["ops"]
    counts = {}
    for o in ops:
        key = o["op"] + (":" + o["act"] if o["op"] == "conv" else "")
        counts[key] = counts.get(key, 0) + 1
    print("[step2] graph nodes %d, cone %d, folded %d, fused SiLU nodes %d, IR ops %d"
          % (stats["graph_nodes"], stats["cone_nodes"], stats["folded"], stats["fused_silu_nodes"], stats["ir_ops"]))
    print("[step2] IR op counts:", counts)

    # A
    if stats["unrecognized"] or stats["cut_not_reached"]:
        fails.append("A")
        for u in stats["unrecognized"][:20]:
            print("[step2] UNRECOGNIZED", u)
        print("[step2] cut not reached:", stats["cut_not_reached"])
    print("[step2] A unrecognized=%d cut_not_reached=%d"
          % (len(stats["unrecognized"]), len(stats["cut_not_reached"])))

    # B
    convs = [o for o in ops if o["op"] == "conv"]
    n_silu = sum(o["act"] == "silu" for o in convs)
    unmapped = [o["module"] for o in convs if not o.get("job")]
    if len(convs) != 83 or n_silu != 77 or unmapped:
        fails.append("B")
    print("[step2] B conv=%d (silu=%d, none=%d), unmapped=%s"
          % (len(convs), n_silu, len(convs) - n_silu, unmapped[:10]))

    # C
    jobs = read_jobs(jobs_path)
    mism, pad_bad = [], []
    rows = ["job\tmodule\tkh\tkw\tstride\tpad\tdil\tCin\tCout\tHin\tWin\tHout\tWout\tact\tweight"]
    for o in convs:
        _, cin, hin, win = o["in_shapes"][0]
        _, cout, hout, wout = o["out_shape"]
        mine = {"Bw": o["kw"], "Bh": o["kh"], "d": o["dilation"][0], "s": o["stride"][0], "Cin": cin,
                "Cw": wout, "Ch": hout, "Cout": cout}
        ref = jobs.get(o["job"])
        if ref is None or ref != mine or o["stride"][0] != o["stride"][1] or o["dilation"] != [1, 1]:
            mism.append((o["job"], mine, ref))
        if o["pad"] != [o["kh"] // 2, o["kw"] // 2, o["kh"] // 2, o["kw"] // 2]:
            pad_bad.append((o["job"], o["pad"]))
        rows.append("\t".join(str(v) for v in (o["job"], o["module"], o["kh"], o["kw"], o["stride"][0],
                                                 o["pad"][0], o["dilation"][0], cin, cout, hin, win, hout, wout,
                                                 o["act"], o["weight"])))
    uncovered = sorted(set(jobs) - {o["job"] for o in convs})
    if mism or pad_bad or uncovered:
        fails.append("C")
    print("[step2] C jobs in TSV=%d, matched=%d, mismatched=%d, uncovered jobs=%d, bad pads=%d"
          % (len(jobs), len(convs) - len(mism), len(mism), len(uncovered), len(pad_bad)))
    for m in mism[:10]:
        print("   MISMATCH", m)
    for u in uncovered[:10]:
        print("   UNCOVERED", u)
    with open(os.path.join(out_dir, "conv_table.tsv"), "w") as f:
        f.write("\n".join(rows) + "\n")

    # D
    img = fc.list_coco8_images()[0]
    x = fc.letterbox_input(img)
    ir_vals = fg.run_ir_float(ir, weights, x)
    so = ort.SessionOptions()
    so.intra_op_num_threads = 4
    sess = ort.InferenceSession(onnx_path, so, providers=["CPUExecutionProvider"])
    names = [o.name for o in sess.get_outputs()]
    ort_vals = dict(zip(names, sess.run(None, {"images": x})))
    compared, worst = 0, (0.0, None)
    d_fail = []
    for name in names:
        if name == "final":
            continue
        if name not in ir_vals:
            d_fail.append((name, "not an IR tensor"))
            continue
        ref, got = ort_vals[name], ir_vals[name]
        rel = float(np.abs(got - ref).max()) / (float(np.abs(ref).max()) + 1e-12)
        compared += 1
        worst = max(worst, (rel, name))
        if not rel < REL_TOL:
            d_fail.append((name, rel))
    if d_fail or compared != len(names) - 1:
        fails.append("D")
    print("[step2] D image %s: IR float vs onnxruntime on %d tapped tensors, worst rel %.3e (%s), fails=%s"
          % (os.path.basename(img), compared, worst[0], worst[1], d_fail[:5]))

    with open(os.path.join(out_dir, "graph_ir.json"), "w") as f:
        json.dump(ir, f, indent=1)
    report = {"onnx": onnx_path, "jobs": jobs_path, "stats": {k: v for k, v in stats.items()},
              "ir_op_counts": counts, "mismatch": mism, "uncovered": uncovered, "pad_bad": pad_bad,
              "D_worst_rel": worst, "D_fail": d_fail, "fails": fails, "elapsed_s": round(time.time() - t0, 1)}
    with open(os.path.join(out_dir, "report.json"), "w") as f:
        json.dump(report, f, indent=1, default=str)
    print("[step2] RESULT: %s (failed checks: %s), elapsed %.1fs"
          % ("PASS" if not fails else "FAIL", fails or "none", time.time() - t0))
    sys.exit(0 if not fails else 1)


if __name__ == "__main__":
    main()
