"""Export the HAS program (fe_has_program.py) for tools/fe/sysc/tb_has_net.cpp.

Same DRAM layout and staging buffers as fe_step6b_export_net.py (which is left unchanged); differences:
  - program: FE_WORK/has/program_has_<mode>.json; golden: fe_ref_has.run_has(mode)
    (compat golden is also checked byte-equal to the int8 reference fe_ref_int8.run_int8 before anything is written)
  - prog.bin magic "FEHP", followed by the knobs <4I> round narrow deq_zp scale_fmt, then as FENP.
    conv skip is always NONE (residuals are elem_add steps). New step records (little endian):
      KIND 5 elem_add: <B3I3i6I>  a_tid b_tid out_tid  zpA zpB zpO  SA sA SB sB SO sO   (S = raw 32-bit word)
      KIND 6 elem_max: <B5I>      in_tid out_tid k s p   (pad value -128)
    KIND 1 slice_ch, 2 concat, 4 upsample_nearest unchanged; KIND 3 (host maxpool) no longer emitted.
  - scale/shift arrays in DRAM come from the mode's config (has: S' int32 in [2^30, 2^31), shift' = shift - 1).
Output only under FE_WORK/has/ (default FE_WORK/has/net_<mode>); FE_WORK/step6b/* is refused.
Usage (repository root, FE_WORK set): python3 -u tools/fe/fe_has_export_net.py --mode compat|has [--image 0 | --image-path p] [--out-dir d]
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
import fe_ref_has as fh  # noqa: E402
import fe_ref_int8 as fr  # noqa: E402

import numpy as np  # noqa: E402

STAGE_A_BYTES = 79 * 1024
STAGE_SKIP_BYTES = 79 * 1024
STAGE_PRE_BYTES = 96 * 1024
STAGE_OUT_BYTES = 96 * 1024
NONE = 0xFFFFFFFF
KIND = {"conv": 0, "slice_ch": 1, "concat": 2, "upsample_nearest": 4, "elem_add": 5, "elem_max": 6}
ADD_FIELDS = ("zpA", "zpB", "zpO", "SA", "sA", "SB", "sB", "SO", "sO")


def align(n, a=16):
    return (n + a - 1) // a * a


def step_io(st):
    if st["kind"] == "conv":
        return [st["input"], st["output"]]
    if st["kind"] == "elem_add":
        return [st["a"], st["b"], st["out"]]
    if st["kind"] == "elem_max":
        return [st["in"], st["out"]]
    return st["inputs"] + [st["output"]]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mode", choices=("compat", "has"), required=True)
    ap.add_argument("--image", type=int, default=0)
    ap.add_argument("--image-path", default=None)
    ap.add_argument("--out-dir", default=None)
    args = ap.parse_args()
    t0 = time.time()
    out_dir = os.path.abspath(args.out_dir or os.path.join(fc.FE_WORK, "has", "net_" + args.mode))
    if not out_dir.startswith(os.path.abspath(os.path.join(fc.FE_WORK, "has")) + os.sep):
        raise SystemExit("refusing out-dir outside FE_WORK/has: %s" % out_dir)

    with open(os.path.join(fc.FE_WORK, "step2", "graph_ir.json")) as f:
        ir = json.load(f)
    program_path = os.path.join(fc.FE_WORK, "has", "program_has_%s.json" % args.mode)
    with open(program_path, "rb") as f:
        program_blob = f.read()
    program = json.loads(program_blob)
    cfg = program["config"]
    params = dict(np.load(os.path.join(fc.FE_WORK, "step3", cfg, "params.npz")))
    knobs = program["knobs"]
    ops = {o["id"]: o for o in ir["ops"]}
    img = args.image_path if args.image_path else fc.list_coco8_images()[args.image]
    if not os.path.isfile(img):
        raise SystemExit("image not found: %s" % img)
    x = fc.letterbox_input(img)
    add_params = {st["out"]: {k: st[k] for k in ADD_FIELDS} for st in program["steps"] if st["kind"] == "elem_add"}
    counters = {"n_sat16": 0, "n_clamp8": 0}
    gold = fh.run_has(ir, params, x, args.mode, knobs, add_params, counters)
    if args.mode == "compat":
        m3 = fr.run_int8(ir, params, x)
        bad = [t for t in m3 if not np.array_equal(m3[t], gold[t])]
        if bad:
            raise SystemExit("compat golden differs from the int8 reference (fe_ref_int8) on %d tensors, first %s" % (len(bad), bad[0]))

    addr = 0
    stage = {}
    for name, size in (("A", STAGE_A_BYTES), ("SKIP", STAGE_SKIP_BYTES), ("PRE", STAGE_PRE_BYTES), ("OUT", STAGE_OUT_BYTES)):
        stage[name] = addr
        addr = align(addr + size)
    tnames = [ir["graph_input"]]
    for st in program["steps"]:
        for t in step_io(st):
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

    os.makedirs(out_dir, exist_ok=True)
    dram = np.zeros(total, dtype=np.uint8)
    for rec in conv_params.values():
        for a, arr in rec.values():
            dram[a:a + arr.nbytes] = np.frombuffer(arr.tobytes(), dtype=np.uint8)
    inp = ir["graph_input"]
    dram[taddr[inp]:taddr[inp] + gold[inp].size] = gold[inp].reshape(-1).view(np.uint8)
    dram.tofile(os.path.join(out_dir, "dram_init.bin"))
    for t in tnames:
        v = gold[t].reshape(-1).view(np.uint8)
        dram[taddr[t]:taddr[t] + v.size] = v
    dram.tofile(os.path.join(out_dir, "dram_golden.bin"))

    with open(os.path.join(out_dir, "prog.bin"), "wb") as f:
        f.write(b"FEHP")
        f.write(struct.pack("<4I", knobs["round"], knobs["narrow"], knobs["deq_zp"], knobs["scale_fmt"]))
        f.write(struct.pack("<4I", stage["A"], stage["SKIP"], stage["PRE"], stage["OUT"]))
        f.write(struct.pack("<I", len(tnames)))
        for t in tnames:
            _, c, h, w = shapes[t]
            f.write(struct.pack("<4I", taddr[t], c, h, w))
        f.write(struct.pack("<I", len(program["steps"])))
        for st in program["steps"]:
            k = st["kind"]
            if k == "conv":
                o = ops[st["op_id"]]
                rec = conv_params[st["job"]]
                f.write(struct.pack("<B", KIND["conv"]))
                f.write(struct.pack("<3I", tid[st["input"]], tid[st["output"]], NONE))
                f.write(struct.pack("<B", int(o["act"] == "silu")))
                f.write(struct.pack("<6I", o["cin"], o["cout"], o["kh"], o["kw"], o["stride"][0], o["stride"][1]))
                f.write(struct.pack("<5I", rec["w"][0], rec["lut"][0], rec["scale"][0], rec["shift"][0], rec["bias"][0]))
                f.write(struct.pack("<I", len(st["tiles"])))
                for tl in st["tiles"]:
                    if "y_used" not in tl or tl["x_used"] != tl["c"][1] - tl["c"][0]:
                        raise SystemExit("program is not core-mapped")
                    f.write(struct.pack("<11i", *tl["c"], *tl["oy"], *tl["ox"], *tl["iy"], *tl["ix"], tl["y_used"]))
            elif k == "elem_add":
                f.write(struct.pack("<B3I3i6I", KIND["elem_add"], tid[st["a"]], tid[st["b"]], tid[st["out"]],
                                    st["zpA"], st["zpB"], st["zpO"], st["SA"] & 0xFFFFFFFF, st["sA"],
                                    st["SB"] & 0xFFFFFFFF, st["sB"], st["SO"] & 0xFFFFFFFF, st["sO"]))
            elif k == "elem_max":
                f.write(struct.pack("<B5I", KIND["elem_max"], tid[st["in"]], tid[st["out"]], st["k"], st["s"], st["p"]))
            else:
                o = ops[st["op_id"]]
                if o["op"] == "slice_ch":
                    f.write(struct.pack("<B4I", KIND["slice_ch"], tid[o["inputs"][0]], tid[o["output"]], o["start"], o["end"]))
                elif o["op"] == "concat":
                    f.write(struct.pack("<B2I", KIND["concat"], tid[o["output"]], len(o["inputs"])))
                    f.write(struct.pack("<%dI" % len(o["inputs"]), *[tid[i] for i in o["inputs"]]))
                elif o["op"] == "upsample_nearest":
                    f.write(struct.pack("<B3I", KIND["upsample_nearest"], tid[o["inputs"][0]], tid[o["output"]], o["factor"]))
                else:
                    raise SystemExit("unsupported host op %s" % o["op"])

    with open(os.path.join(out_dir, "program_snapshot.json"), "wb") as f:
        f.write(program_blob)
    manifest = {"mode": args.mode, "config": cfg, "knobs": knobs, "image": os.path.basename(img), "dram_bytes": total,
                "stage": stage, "program_sha256": hashlib.sha256(program_blob).hexdigest(),
                "golden_counters": counters,
                "tensors": {t: {"id": tid[t], "addr": taddr[t], "shape": shapes[t]} for t in tnames},
                "cut_outputs": ir["cut_outputs"], "steps": len(program["steps"])}
    with open(os.path.join(out_dir, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=1)
    print("[has-net-export] %s: tensors %d, steps %d, DRAM %.1f MB, image %s, sat16 %d clamp8 %d, %.1fs"
          % (args.mode, len(tnames), len(program["steps"]), total / 1e6, os.path.basename(img),
             counters["n_sat16"], counters["n_clamp8"], time.time() - t0))


if __name__ == "__main__":
    main()
