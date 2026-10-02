"""Step 6b: export the whole YOLOv8m program for tools/fe/sysc/tb_fe_network.cpp.

Writes FE_WORK/step6b/net/:
  dram_init.bin   DRAM image: staging buffers, every IR tensor region (only the quantized input filled),
                  per-conv weights [Cout, Cin, kh, kw] int8, LUT int8[256], scale u32[Cout], shift u32[Cout],
                  bias (PSUM preload value) i32[Cout]
  dram_golden.bin same layout with every tensor region filled from T2 (fe_ref_int8.run_int8)
  prog.bin        tensor table + program steps (see write_prog)
  program_snapshot.json exact program.json bytes used to write prog.bin; SHA-256 in manifest
  manifest.json   addresses, sizes, config, image
Staging buffers (fixed addresses at the start of DRAM):
  STAGE_A    input window [Cin, A_H, A_W] int8           -> IFmap SRAM bank 2 (DMA CH1)
  STAGE_SKIP skip values, C-order [k, h, w] int8         -> IFmap SRAM bank 3 (DMA CH2 read; HAS leaves skip memory open)
  STAGE_PRE  PSUM preload, C-order [k, h, w] int32 (bias) -> PSUM SRAM bank 4 (DMA CH3), 32 elements per word
  STAGE_OUT  PSUM SRAM bank 4 words after the OBP        <- DMA write channel
Usage (repository root, FE_WORK set): python3 -u tools/fe/fe_step6b_export_net.py [--config pc_p9999_rc] [--image 0]
                   [--image-path <any .jpg>] [--out-dir <dir, default FE_WORK/step6b/net>]
"""
import argparse
import hashlib
import json
import os
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fe_common as fc  # noqa: E402
import fe_ref_int8 as fr  # noqa: E402

import numpy as np  # noqa: E402

STAGE_A_BYTES = 79 * 1024
STAGE_SKIP_BYTES = 79 * 1024
STAGE_PRE_BYTES = 96 * 1024
STAGE_OUT_BYTES = 96 * 1024
NONE = 0xFFFFFFFF
KIND = {"conv": 0, "slice_ch": 1, "concat": 2, "maxpool": 3, "upsample_nearest": 4}


def align(n, a=16):
    return (n + a - 1) // a * a


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", default="pc_p9999_rc")
    ap.add_argument("--image", type=int, default=0)
    # --image-path / --out-dir: run any image without overwriting the default net directory (defaults unchanged).
    ap.add_argument("--image-path", default=None, help="path of any image (instead of --image)")
    ap.add_argument("--out-dir", default=None, help="output directory (default FE_WORK/step6b/net)")
    args = ap.parse_args()
    t0 = time.time()
    with open(os.path.join(fc.FE_WORK, "step2", "graph_ir.json")) as f:
        ir = json.load(f)
    program_path = os.path.join(fc.FE_WORK, "step6", "program.json")
    with open(program_path, "rb") as f:
        program_blob = f.read()
    program_sha256 = hashlib.sha256(program_blob).hexdigest()
    program = json.loads(program_blob)
    params = dict(np.load(os.path.join(fc.FE_WORK, "step3", args.config, "params.npz")))
    ops = {o["id"]: o for o in ir["ops"]}
    img = args.image_path if args.image_path else fc.list_coco8_images()[args.image]
    if not os.path.isfile(img):
        raise SystemExit("image not found: %s" % img)
    gold = fr.run_int8(ir, params, fc.letterbox_input(img))

    addr = 0
    stage = {}
    for name, size in (("A", STAGE_A_BYTES), ("SKIP", STAGE_SKIP_BYTES), ("PRE", STAGE_PRE_BYTES), ("OUT", STAGE_OUT_BYTES)):
        stage[name] = addr
        addr = align(addr + size)

    # tensors: graph input + every step output + every step input/skip (all are IR tensors)
    tnames = [ir["graph_input"]]
    for st in program["steps"]:
        for t in ([st["input"], st["skip"], st["output"]] if st["kind"] == "conv" else st["inputs"] + [st["output"]]):
            if t and t not in tnames:
                tnames.append(t)
    shapes = {ir["graph_input"]: ir["input_shape"]}
    for o in ir["ops"]:
        shapes[o["output"]] = o["out_shape"]
    tid, taddr = {}, {}
    for t in tnames:
        _, c, h, w = shapes[t]
        tid[t] = len(tid)
        taddr[t] = addr
        addr = align(addr + c * h * w)

    conv_params = {}
    for st in program["steps"]:
        if st["kind"] != "conv":
            continue
        j = st["job"]
        rec = {}
        for key, arr in (("w", params[j + "/q_w"].astype(np.int8)),
                         ("lut", params.get(j + "/lut", np.zeros(256, dtype=np.int8)).astype(np.int8)),
                         ("scale", params[j + "/scale"].astype("<u4")), ("shift", params[j + "/shift"].astype("<u4")),
                         ("bias", params[j + "/q_b"].astype("<i4"))):
            rec[key] = (addr, arr)
            addr = align(addr + arr.nbytes)
        conv_params[j] = rec
    total = addr

    out_dir = args.out_dir or os.path.join(fc.FE_WORK, "step6b", "net")
    os.makedirs(out_dir, exist_ok=True)
    dram = np.zeros(total, dtype=np.uint8)
    for j, rec in conv_params.items():
        for key, (a, arr) in rec.items():
            dram[a:a + arr.nbytes] = np.frombuffer(arr.tobytes(), dtype=np.uint8)
    inp = ir["graph_input"]
    dram[taddr[inp]:taddr[inp] + gold[inp].size] = gold[inp].reshape(-1).view(np.uint8)
    dram.tofile(os.path.join(out_dir, "dram_init.bin"))
    for t in tnames:
        v = gold[t].reshape(-1).view(np.uint8)
        dram[taddr[t]:taddr[t] + v.size] = v
    dram.tofile(os.path.join(out_dir, "dram_golden.bin"))

    with open(os.path.join(out_dir, "prog.bin"), "wb") as f:
        f.write(b"FENP")
        f.write(struct.pack("<4I", stage["A"], stage["SKIP"], stage["PRE"], stage["OUT"]))
        f.write(struct.pack("<I", len(tnames)))
        for t in tnames:
            _, c, h, w = shapes[t]
            f.write(struct.pack("<4I", taddr[t], c, h, w))
        f.write(struct.pack("<I", len(program["steps"])))
        for st in program["steps"]:
            o = ops[st["op_id"]]
            if st["kind"] == "conv":
                rec = conv_params[st["job"]]
                f.write(struct.pack("<B", KIND["conv"]))
                f.write(struct.pack("<3I", tid[st["input"]], tid[st["output"]], tid[st["skip"]] if st["skip"] else NONE))
                f.write(struct.pack("<B", int(o["act"] == "silu")))
                f.write(struct.pack("<6I", o["cin"], o["cout"], o["kh"], o["kw"], o["stride"][0], o["stride"][1]))
                f.write(struct.pack("<5I", rec["w"][0], rec["lut"][0], rec["scale"][0], rec["shift"][0], rec["bias"][0]))
                f.write(struct.pack("<I", len(st["tiles"])))
                for tl in st["tiles"]:
                    if "y_used" not in tl or tl["x_used"] != tl["c"][1] - tl["c"][0]:
                        raise SystemExit("program is not core-mapped (rerun fe_step6a_tile_check.py)")
                    f.write(struct.pack("<11i", *tl["c"], *tl["oy"], *tl["ox"], *tl["iy"], *tl["ix"], tl["y_used"]))
            elif o["op"] == "slice_ch":
                f.write(struct.pack("<B4I", KIND["slice_ch"], tid[o["inputs"][0]], tid[o["output"]], o["start"], o["end"]))
            elif o["op"] == "concat":
                f.write(struct.pack("<B2I", KIND["concat"], tid[o["output"]], len(o["inputs"])))
                f.write(struct.pack("<%dI" % len(o["inputs"]), *[tid[i] for i in o["inputs"]]))
            elif o["op"] == "maxpool":
                if o["kernel"][0] != o["kernel"][1] or o["stride"][0] != o["stride"][1] or len(set(o["pad"])) != 1:
                    raise SystemExit("maxpool attributes not square/symmetric: %s" % o)
                f.write(struct.pack("<B5I", KIND["maxpool"], tid[o["inputs"][0]], tid[o["output"]], o["kernel"][0], o["stride"][0], o["pad"][0]))
            elif o["op"] == "upsample_nearest":
                f.write(struct.pack("<B3I", KIND["upsample_nearest"], tid[o["inputs"][0]], tid[o["output"]], o["factor"]))
            else:
                raise SystemExit("unsupported host op %s" % o["op"])

    with open(os.path.join(out_dir, "program_snapshot.json"), "wb") as f:
        f.write(program_blob)
    manifest = {"config": args.config, "image": os.path.basename(img), "dram_bytes": total, "stage": stage,
                "program_sha256": program_sha256,
                "tensors": {t: {"id": tid[t], "addr": taddr[t], "shape": shapes[t]} for t in tnames},
                "cut_outputs": ir["cut_outputs"], "steps": len(program["steps"])}
    with open(os.path.join(out_dir, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=1)
    print("[step6b-net-export] tensors %d, steps %d, DRAM %.1f MB, image %s, %.1fs"
          % (len(tnames), len(program["steps"]), total / 1e6, os.path.basename(img), time.time() - t0))


if __name__ == "__main__":
    main()
