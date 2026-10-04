"""Frontend tiles as SAURIA core jobs.

Each selected frontend tile becomes one single-tile job of the sauria_model flow (gen_stim_tiled_real.py + tb_iso) whose
"layer" IS the tile, because that generator only accepts layers split evenly into tiles:
  A = host-padded input window [Cin, A_H, A_W] int8  (NPU_REAL_A)
  B = q_w[c0:c1]               [k, Cin, kh, kw] int8 (NPU_REAL_B)
  output [k, h, w]; w_til = w, h_til = h, k_til = k, c_til = Cin; X_used / Y_used from the tile.
expect[k, y, x] = sum(B * A) (int64) is computed here with a sliding-window einsum and cross-checked against
fe_ref_int8.conv_psum. The bias is a PSUM preload value (HAS 6.9); generator preload mode 1 (random C) exercises the
same preload path, mode 2 loads zeros.

Selection per conv: the first tile (top-left padding) plus the first tile of every distinct (k, h, w) shape;
--all-tiles takes every tile.
Writes FE_WORK/core/jobs/<name>/{A.npy, B.npy, expect.npy, job.json} and FE_WORK/core/jobs.tsv
(name, generator args "Bw Bh d s Cin Cw Ch Cout Xused Yused preload", tile "w h k c", job dir).
Usage (repository root, FE_WORK set): python3 -u tools/fe/fe_core_jobs.py [--config pc_p9999_rc] [--image 0] [--convs a,b] [--preload 1]
"""
import argparse
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fe_common as fc  # noqa: E402
import fe_ref_int8 as fr  # noqa: E402
import fe_tile_plan as tp  # noqa: E402

import numpy as np  # noqa: E402


def select_tiles(tiles, all_tiles):
    if all_tiles:
        return list(range(len(tiles)))
    picked, seen = [0], set()
    for i, tl in enumerate(tiles):
        key = (tl["c"][1] - tl["c"][0], tl["oy"][1] - tl["oy"][0], tl["ox"][1] - tl["ox"][0])
        if key not in seen:
            seen.add(key)
            if i not in picked:
                picked.append(i)
    return picked


def window_psum(a, b, stride):
    """a int8 [Cin, AH, AW], b int8 [k, Cin, kh, kw] -> int64 [k, h, w] (correlation, no padding)."""
    _, kh, kw = b.shape[1:]
    win = np.lib.stride_tricks.sliding_window_view(a.astype(np.int64), (kh, kw), axis=(1, 2))[:, ::stride, ::stride]
    return np.einsum("kcij,cyxij->kyx", b.astype(np.int64), win)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", default="pc_p9999_rc")
    ap.add_argument("--image", type=int, default=0)
    ap.add_argument("--convs", default="", help="comma-separated conv jobs (default: all 83)")
    ap.add_argument("--all-tiles", action="store_true")
    ap.add_argument("--preload", type=int, default=1, choices=(0, 1, 2))
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
    want = [c for c in args.convs.split(",") if c]

    root = os.path.join(fc.FE_WORK, "core")
    jobs_dir = os.path.join(root, "jobs")
    os.makedirs(jobs_dir, exist_ok=True)
    lines, n_bad = [], 0
    for st in program["steps"]:
        if st["kind"] != "conv" or (want and st["job"] not in want):
            continue
        o = ops[st["op_id"]]
        j = st["job"]
        kh, kw = o["kh"], o["kw"]
        sy, sx = o["stride"]
        if kh != kw or sy != sx or o.get("dilation", [1, 1]) != [1, 1]:
            raise SystemExit("%s: non-square kernel/stride or dilation not supported by the generator CLI" % j)
        src = gold[st["input"]]
        q_w = params[j + "/q_w"].astype(np.int8)
        for i in select_tiles(st["tiles"], args.all_tiles):
            tl = st["tiles"][i]
            c0, c1 = tl["c"]
            k, h, w = c1 - c0, tl["oy"][1] - tl["oy"][0], tl["ox"][1] - tl["ox"][0]
            a = tp.padded_crop(src, tl["iy"], tl["ix"])[0]
            b = np.ascontiguousarray(q_w[c0:c1])
            if a.shape[1] != (h - 1) * sy + kh or a.shape[2] != (w - 1) * sx + kw:
                raise SystemExit("%s tile %d: window %s does not match output %dx%d" % (j, i, a.shape, h, w))
            expect = window_psum(a, b, sy)
            ref = fr.conv_psum(a[None], b, o["stride"], [0, 0, 0, 0])[0].astype(np.int64)
            if expect.shape != (k, h, w) or not np.array_equal(expect, ref):
                n_bad += 1
                print("[core-jobs] MISMATCH expect vs fe_ref_int8: %s tile %d" % (j, i))
            name = "%s__t%04d" % (j, i)
            d = os.path.join(jobs_dir, name)
            os.makedirs(d, exist_ok=True)
            np.save(os.path.join(d, "A.npy"), a)
            np.save(os.path.join(d, "B.npy"), b)
            np.save(os.path.join(d, "expect.npy"), expect)
            gen_args = [kw, kh, 1, sy, o["cin"], w, h, k, tl["x_used"], tl["y_used"], args.preload]
            tile = [w, h, k, o["cin"]]
            meta = {"conv": j, "tile_index": i, "tile": tl, "gen_args": gen_args, "gen_tile": tile,
                    "bias": params[j + "/q_b"][c0:c1].astype(np.int64).tolist(), "config": args.config,
                    "image": os.path.basename(img), "A_bytes": int(a.size), "B_bytes": int(b.size),
                    "C_psums": k * h * w, "W_t*Cout_t": w * k}
            with open(os.path.join(d, "job.json"), "w") as f:
                json.dump(meta, f, indent=1)
            lines.append("%s\t%s\t%s\t%s" % (name, " ".join(map(str, gen_args)), " ".join(map(str, tile)), d))
    with open(os.path.join(root, "jobs.tsv"), "w") as f:
        f.write("\n".join(lines) + "\n")
    print("[core-jobs] %d jobs (%s tiles), expect cross-check mismatches %d, image %s, %.1fs -> %s"
          % (len(lines), "all" if args.all_tiles else "representative", n_bad, os.path.basename(img),
             time.time() - t0, os.path.join(root, "jobs.tsv")))
    sys.exit(1 if n_bad else 0)


if __name__ == "__main__":
    main()
