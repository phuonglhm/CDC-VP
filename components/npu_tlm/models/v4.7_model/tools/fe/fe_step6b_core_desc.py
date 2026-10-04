"""Roadmap R2 preparation: SauriaLayerDesc of every tile shape in FE_WORK/step6/program.json.

Mapping (driver/libsauria_cfg.h): B_w = kw, B_h = kh, d = 1, s = stride, c_til = Cin (never split),
k_til = tile output channels, h_til/w_til = tile output height/width, X_used/Y_used from the tile,
preload_en = 1 (bias is the PSUM preload, HAS 6.9). Writes FE_WORK/step6/core_desc.tsv for
tools/fe/sysc/check_core_fields.cpp. Usage (repository root, FE_WORK set): python3 tools/fe/fe_step6b_core_desc.py
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fe_common as fc  # noqa: E402


def main():
    with open(os.path.join(fc.FE_WORK, "step2", "graph_ir.json")) as f:
        ops = {o["id"]: o for o in json.load(f)["ops"]}
    with open(os.path.join(fc.FE_WORK, "step6", "program.json")) as f:
        program = json.load(f)
    shapes = {}
    for st in program["steps"]:
        if st["kind"] != "conv":
            continue
        o = ops[st["op_id"]]
        for tl in st["tiles"]:
            key = (o["kw"], o["kh"], o["dilation"][0], o["stride"][0], o["cin"], tl["c"][1] - tl["c"][0],
                   tl["oy"][1] - tl["oy"][0], tl["ox"][1] - tl["ox"][0], tl["x_used"], tl["y_used"], 1)
            if key not in shapes:
                shapes[key] = [0, st["job"]]
            shapes[key][0] += 1
    path = os.path.join(fc.FE_WORK, "step6", "core_desc.tsv")
    with open(path, "w") as f:
        f.write("# B_w B_h d s c_til k_til h_til w_til X_used Y_used preload_en count job\n")
        for key, (count, job) in sorted(shapes.items()):
            f.write("%s %d %s\n" % (" ".join(str(v) for v in key), count, job))
    print("[core-desc] %d unique tile shapes, %d tiles -> %s" % (len(shapes), sum(v[0] for v in shapes.values()), path))


if __name__ == "__main__":
    main()
