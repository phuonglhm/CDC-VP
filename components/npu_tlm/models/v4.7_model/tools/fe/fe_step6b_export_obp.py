"""Step 6b: export OBP vector streams for tools/fe/sysc/tb_obp_tiles.cpp (real psm/obp_top.h, unmodified).

For selected convs of one image, every tile is laid out as the HAS defines (fe_hw_layout.py): contexts of
32 output positions, one vector per output channel, accumulator = PSUM preload (bias) + conv products.
Inputs, skip data and expected INT8 outputs come from T2 (fe_ref_int8.run_int8), so the testbench compares
the real Obp module against the golden.

Binary format (little endian):
  "OBPV" u32 n_convs
  per conv: u32 name_len, name, u8 silu, u8 has_skip, i8 lut[256], u32 n_tiles
    per tile: u32 nch, u32 scale[nch], u32 shift[nch], u32 n_ctx
      per ctx: u32 nrows; then per channel x: i32 acc[nrows], i8 skip[nrows], i8 expected[nrows]
Usage (repository root, FE_WORK set): python3 -u tools/fe/fe_step6b_export_obp.py [--config pc_p9999_rc] [--jobs stem,...]
"""
import argparse
import json
import os
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fe_common as fc  # noqa: E402
import fe_ref_int8 as fr  # noqa: E402
import fe_tile_plan as tp  # noqa: E402

import numpy as np  # noqa: E402

N = 32
DEFAULT_JOBS = "stem,dark2.c2f.b0.2,det.p3.box_out,det.p5.cls_out"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", default="pc_p9999_rc")
    ap.add_argument("--jobs", default=DEFAULT_JOBS)
    ap.add_argument("--image", type=int, default=0)
    args = ap.parse_args()
    t0 = time.time()
    with open(os.path.join(fc.FE_WORK, "step2", "graph_ir.json")) as f:
        ir = json.load(f)
    with open(os.path.join(fc.FE_WORK, "step6", "program.json")) as f:
        program = json.load(f)
    params = dict(np.load(os.path.join(fc.FE_WORK, "step3", args.config, "params.npz")))
    ops = {o["id"]: o for o in ir["ops"]}
    img = fc.list_coco8_images()[args.image]
    gold = fr.run_int8(ir, params, fc.letterbox_input(img))
    jobs = args.jobs.split(",")
    steps = {s["job"]: s for s in program["steps"] if s["kind"] == "conv"}
    missing = [j for j in jobs if j not in steps]
    if missing:
        raise SystemExit("unknown jobs: %s" % missing)

    out_dir = os.path.join(fc.FE_WORK, "step6b")
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, "obp_vectors.bin")
    summary = []
    with open(path, "wb") as f:
        f.write(b"OBPV" + struct.pack("<I", len(jobs)))
        for j in jobs:
            st, o = steps[j], ops[steps[j]["op_id"]]
            src = gold[st["input"]]
            skip = gold[st["skip"]] if st["skip"] else None
            expected = gold[st["output"]]
            lut = params.get(j + "/lut")
            silu = o["act"] == "silu"
            name = j.encode()
            f.write(struct.pack("<I", len(name)) + name + struct.pack("<BB", int(silu), int(skip is not None)))
            f.write((lut if silu else np.zeros(256, dtype=np.int8)).astype(np.int8).tobytes())
            f.write(struct.pack("<I", len(st["tiles"])))
            n_vec = n_ctx = 0
            for tl in st["tiles"]:
                c0, c1 = tl["c"]
                oy0, oy1 = tl["oy"]
                ox0, ox1 = tl["ox"]
                nch, npos = c1 - c0, (oy1 - oy0) * (ox1 - ox0)
                prod = fr.conv_psum(tp.padded_crop(src, tl["iy"], tl["ix"]), params[j + "/q_w"][c0:c1], o["stride"], [0, 0, 0, 0])
                acc = prod[0].reshape(nch, npos).T + params[j + "/q_b"][c0:c1].astype(np.int64)[None, :]
                if np.abs(acc).max() > (1 << 31) - 1:
                    raise SystemExit("%s: accumulator overflow" % j)
                sk = (skip[0, c0:c1, oy0:oy1, ox0:ox1].reshape(nch, npos).T if skip is not None
                      else np.zeros((npos, nch), dtype=np.int8))
                ex = expected[0, c0:c1, oy0:oy1, ox0:ox1].reshape(nch, npos).T
                f.write(struct.pack("<I", nch))
                f.write(params[j + "/scale"][c0:c1].astype("<u4").tobytes())
                f.write(params[j + "/shift"][c0:c1].astype("<u4").tobytes())
                wt = ox1 - ox0
                if "y_used" in tl:  # core mapping: y_used positions of one output row per context
                    yu = tl["y_used"]
                    ctxs = [slice(oy * wt + cx0, oy * wt + cx0 + yu) for oy in range(oy1 - oy0) for cx0 in range(0, wt, yu)]
                else:
                    ctxs = [slice(r0, min(r0 + N, npos)) for r0 in range(0, npos, N)]
                f.write(struct.pack("<I", len(ctxs)))
                for rows in ctxs:
                    f.write(struct.pack("<I", rows.stop - rows.start))
                    for x in range(nch):
                        f.write(acc[rows, x].astype("<i4").tobytes())
                        f.write(sk[rows, x].astype(np.int8).tobytes())
                        f.write(ex[rows, x].astype(np.int8).tobytes())
                        n_vec += 1
                    n_ctx += 1
            summary.append((j, len(st["tiles"]), n_ctx, n_vec, bool(silu), skip is not None))
    for s in summary:
        print("[step6b-export] %s: tiles %d, contexts %d, vectors %d, silu %s, skip %s" % s)
    print("[step6b-export] wrote %s (%.1f MB), image %s, %.1fs"
          % (path, os.path.getsize(path) / 1e6, os.path.basename(img), time.time() - t0))


if __name__ == "__main__":
    main()
