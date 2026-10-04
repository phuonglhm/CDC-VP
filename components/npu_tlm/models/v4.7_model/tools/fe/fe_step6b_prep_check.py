"""Step 6b preparation check: HAS-ordered execution (fe_hw_layout) reproduces T2 exactly.

Checks on the chosen config and the 6a program:
  - bias through PSUM preload (not added in the OBP), contexts of 32 positions x <= 32 channels,
    one OBP vector per channel with the vector counter reset per context, residual after the LUT;
  - every accumulator fits INT32;
  - every materialized tensor equals T2.
Outputs: FE_WORK/step6/report_6b_prep.json
Usage (repository root, FE_WORK set): python3 -u tools/fe/fe_step6b_prep_check.py [--config pc_p9999_rc] [--images 2]
"""
import argparse
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fe_common as fc  # noqa: E402
import fe_hw_layout as hw  # noqa: E402
import fe_ref_int8 as fr  # noqa: E402

import numpy as np  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", default="pc_p9999_rc")
    ap.add_argument("--images", type=int, default=2)
    args = ap.parse_args()
    t0 = time.time()
    with open(os.path.join(fc.FE_WORK, "step2", "graph_ir.json")) as f:
        ir = json.load(f)
    with open(os.path.join(fc.FE_WORK, "step6", "program.json")) as f:
        program = json.load(f)
    if program.get("config") != args.config:
        print("[step6b-prep] note: program built with config %s, checking %s (tiling is config-independent)"
              % (program.get("config"), args.config))
    params = dict(np.load(os.path.join(fc.FE_WORK, "step3", args.config, "params.npz")))
    fails, compared = [], 0
    stats = {"acc_absmax": 0, "contexts": 0, "partial_contexts": 0, "tiles": 0, "tiles_lt32_channels": 0}
    for img in fc.list_coco8_images()[: args.images]:
        ti = time.time()
        x = fc.letterbox_input(img)
        ref = fr.run_int8(ir, params, x)
        got = hw.run_program_hw(ir, program, params, x, stats)
        for name, v in got.items():
            if name == ir["graph_input"]:
                continue
            compared += 1
            if v.shape != ref[name].shape or not np.array_equal(v, ref[name]):
                fails.append((os.path.basename(img), name))
        print("[step6b-prep] %s: compared %d, fails so far %d, %.1fs" % (os.path.basename(img), len(got) - 1, len(fails), time.time() - ti))
    per_img = {k: (v // max(1, args.images) if k != "acc_absmax" else v) for k, v in stats.items()}
    report = {"config": args.config, "compared": compared, "fails": fails, "stats_per_image": per_img,
              "int32_headroom_bits": float(np.log2((1 << 31) / max(1, stats["acc_absmax"]))),
              "elapsed_s": round(time.time() - t0, 1)}
    with open(os.path.join(fc.FE_WORK, "step6", "report_6b_prep.json"), "w") as f:
        json.dump(report, f, indent=1)
    print("[step6b-prep] per image: tiles %d (with <32 channels %d), contexts %d (partial %d); |acc| max %d "
          "(%.1f bits headroom)" % (per_img["tiles"], per_img["tiles_lt32_channels"], per_img["contexts"],
                                    per_img["partial_contexts"], stats["acc_absmax"], report["int32_headroom_bits"]))
    print("[step6b-prep] RESULT: %s (%d comparisons, %d fails), elapsed %.1fs"
          % ("PASS" if not fails else "FAIL", compared, len(fails), time.time() - t0))
    sys.exit(0 if not fails else 1)


if __name__ == "__main__":
    main()
