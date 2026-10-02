"""Step 6a check: the tile program reproduces T2 exactly.

- Builds the tile program (fe_tile_plan.build_program) from FE_WORK/step2/graph_ir.json.
- Runs it with numpy (padded input crops, per-tile psum on the Cout slice, per-lane OBP params,
  residual fused into the conv) and compares every materialized tensor with T2 (fe_ref_int8.run_int8)
  for the chosen config. Host ops reuse the T2 implementations, so this check targets tiling,
  host padding and residual fusion, not the host ops themselves.
PASS = all compared tensors identical on every image, and every tile within bank limits.

Outputs: FE_WORK/step6/{program.json, conv_tiles.tsv, report_6a.json}
Usage (repository root, FE_WORK set): python3 -u tools/fe/fe_step6a_tile_check.py [--config pc_p9999_rc] [--images 2]
"""
import argparse
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "eval_sw"))
import fe_common as fc  # noqa: E402
import fe_ref_int8 as fr  # noqa: E402
import fe_tile_plan as tp  # noqa: E402
import tile_model as tm  # noqa: E402  -- predicted-cycles objective (calibrated tile cost model)

import numpy as np  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", default="pc_p9999_rc")
    ap.add_argument("--images", type=int, default=2)
    args = ap.parse_args()
    t0 = time.time()
    out_dir = os.path.join(fc.FE_WORK, "step6")
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(fc.FE_WORK, "step2", "graph_ir.json")) as f:
        ir = json.load(f)
    params = dict(np.load(os.path.join(fc.FE_WORK, "step3", args.config, "params.npz")))

    # Predicted-cycles objective. shape_table.csv is the calibrated 149-shape cost table
    # (tools/eval_sw/out/prep/shape_table.csv, same one tile_sweep.py reads) -- graph_ir here is the
    # SAME FE_WORK/step2/graph_ir.json loaded above (ShapeModel re-reads it from its own path arg).
    repo_root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
    shape_csv = os.path.join(repo_root, "tools", "eval_sw", "out", "prep", "shape_table.csv")
    graph_ir_path = os.path.join(fc.FE_WORK, "step2", "graph_ir.json")
    model = tm.ShapeModel(shape_csv, graph_ir_path) if os.path.exists(shape_csv) else None
    if model is None:
        print("[step6a] WARN: %s not found -- falling back to tile-count objective (S9-FE model disabled)" % shape_csv)
    program = tp.build_program(ir, model=model)
    program["config"] = args.config
    convs = [s for s in program["steps"] if s["kind"] == "conv"]
    hosts = [s for s in program["steps"] if s["kind"] == "host"]
    host_counts = {}
    for s in hosts:
        host_counts[s["op"]] = host_counts.get(s["op"], 0) + 1
    n_tiles = sum(s["n_tiles"] for s in convs)
    rows = ["job\tcout_t\th_t\tw_t\tn_tiles\tA_bytes\tB_bytes\tC_psums\tskip"]
    for s in convs:
        u = s["usage"]
        rows.append("\t".join(str(v) for v in (s["job"], s["cout_t"], s["h_t"], s["w_t"], s["n_tiles"],
                                                 u["A"], u["B"], u["C"], "yes" if s["skip"] else "")))
    with open(os.path.join(out_dir, "conv_tiles.tsv"), "w") as f:
        f.write("\n".join(rows) + "\n")
    with open(os.path.join(out_dir, "program.json"), "w") as f:
        json.dump(program, f)
    print("[step6a] steps %d: conv %d (fused residual %d), host %s; tiles total %d; cout_t %d..%d; "
          "max usage A %d/%d B %d/%d C %d/%d"
          % (len(program["steps"]), len(convs), program["fused_residuals"], host_counts, n_tiles,
             min(s["cout_t"] for s in convs), max(s["cout_t"] for s in convs),
             max(s["usage"]["A"] for s in convs), tp.BANK_A_BYTES, max(s["usage"]["B"] for s in convs),
             tp.BANK_B_BYTES, max(s["usage"]["C"] for s in convs), tp.BANK_C_PSUMS))
    most = sorted(convs, key=lambda s: -s["n_tiles"])[:5]
    print("[step6a] most tiles:", ", ".join("%s=%d (%dx%dx%d)" % (s["job"], s["n_tiles"], s["h_t"], s["w_t"], s["cout_t"]) for s in most))

    elided = {s["conv_output_elided"] for s in convs if s["conv_output_elided"]}
    fails, compared = [], 0
    for img in fc.list_coco8_images()[: args.images]:
        ti = time.time()
        x = fc.letterbox_input(img)
        ref = fr.run_int8(ir, params, x)
        got = tp.run_program(ir, program, params, x)
        missing = [o["output"] for o in ir["ops"] if o["output"] not in got and o["output"] not in elided]
        if missing:
            fails.append((os.path.basename(img), "missing", missing[:5]))
        for name, v in got.items():
            if name == ir["graph_input"]:
                continue
            compared += 1
            if v.shape != ref[name].shape or not np.array_equal(v, ref[name]):
                fails.append((os.path.basename(img), name, int((v != ref[name]).sum()) if v.shape == ref[name].shape else -1))
        print("[step6a] %s: tensors compared %d, fails so far %d, %.1fs"
              % (os.path.basename(img), len(got) - 1, len(fails), time.time() - ti))

    report = {"config": args.config, "steps": len(program["steps"]), "convs": len(convs),
              "fused_residuals": program["fused_residuals"], "host_ops": host_counts, "tiles": n_tiles,
              "elided_conv_outputs": len(elided), "compared": compared, "fails": fails,
              "elapsed_s": round(time.time() - t0, 1)}
    with open(os.path.join(out_dir, "report_6a.json"), "w") as f:
        json.dump(report, f, indent=1)
    print("[step6a] RESULT: %s (%d tensor comparisons, %d fails), elapsed %.1fs"
          % ("PASS" if not fails else "FAIL", compared, len(fails), time.time() - t0))
    sys.exit(0 if not fails else 1)


if __name__ == "__main__":
    main()
