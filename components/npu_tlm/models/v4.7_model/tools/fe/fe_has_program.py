"""Lower the core-path program (FE_WORK/step6/program.json) to the HAS program (has/HAS_IFACE.md §7).

  conv with skip  -> conv (skip = None, output = its conv_output_elided tensor, i.e. "T@pre")
                     + elem_add {a: T@pre, b: skip, out: T, zpA, zpB, zpO, SA, sA, SB, sB, SO, sO}
  host maxpool    -> elem_max {in, out, k, s, p}   (pad value -128)
  slice/concat/upsample stay host steps. Conv tiles are copied unchanged; the SHA-256 of the conv tile list
  is checked equal to the source program's.
Writes FE_WORK/has/program_has_<mode>.json for mode in (compat, has): only the elem_add parameters differ.
Usage (repository root, FE_WORK set): python3 tools/fe/fe_has_program.py [--config pc_p9999_rc] [--has-config pc_p9999_has]
"""
import argparse
import copy
import hashlib
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fe_common as fc  # noqa: E402
import fe_ref_has as fh  # noqa: E402


def conv_tiles_sha(program):
    blob = json.dumps([st["tiles"] for st in program["steps"] if st["kind"] == "conv"], sort_keys=True)
    return hashlib.sha256(blob.encode()).hexdigest()


def lower(src, ir, scales, mode):
    ops = {o["id"]: o for o in ir["ops"]}
    steps = []
    n_add = n_max = 0
    for st in src["steps"]:
        if st["kind"] == "conv" and st["skip"]:
            pre = st["conv_output_elided"]
            if not pre:
                raise SystemExit("%s: fused residual without conv_output_elided" % st["job"])
            c = copy.deepcopy(st)
            c["skip"] = None
            c["output"] = pre
            c["fused_residual_lowered_to"] = "elem_add"
            steps.append(c)
            p = fh.elem_add_params(mode, scales[pre], scales[st["skip"]], scales[st["output"]])
            steps.append(dict({"kind": "elem_add", "from_job": st["job"], "a": pre, "b": st["skip"],
                               "out": st["output"]}, **p))
            n_add += 1
        elif st["kind"] == "host" and st["op"] == "maxpool":
            o = ops[st["op_id"]]
            if o["kernel"][0] != o["kernel"][1] or o["stride"][0] != o["stride"][1] or len(set(o["pad"])) != 1:
                raise SystemExit("maxpool not square/symmetric: %s" % o)
            steps.append({"kind": "elem_max", "op_id": st["op_id"], "in": st["inputs"][0], "out": st["output"],
                          "k": o["kernel"][0], "s": o["stride"][0], "p": o["pad"][0], "pad_value": -128})
            n_max += 1
        else:
            steps.append(copy.deepcopy(st))
    return steps, n_add, n_max


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", default="pc_p9999_rc", help="compat parameter config")
    ap.add_argument("--has-config", default="pc_p9999_has")
    args = ap.parse_args()
    ppath = os.path.join(fc.FE_WORK, "step6", "program.json")
    with open(ppath, "rb") as f:
        blob = f.read()
    src = json.loads(blob)
    with open(os.path.join(fc.FE_WORK, "step2", "graph_ir.json")) as f:
        ir = json.load(f)
    out_dir = os.path.join(fc.FE_WORK, "has")
    os.makedirs(out_dir, exist_ok=True)
    for mode, cfg in (("compat", args.config), ("has", args.has_config)):
        with open(os.path.join(fc.FE_WORK, "step3", cfg, "params.json")) as f:
            scales = json.load(f)["tensor_scales"]
        steps, n_add, n_max = lower(src, ir, scales, mode)
        prog = {k: v for k, v in src.items() if k != "steps"}
        prog.update({"steps": steps, "has_mode": mode, "config": cfg, "knobs": fh.MODES[mode],
                     "source_program_sha256": hashlib.sha256(blob).hexdigest()})
        if conv_tiles_sha(prog) != conv_tiles_sha(src):
            raise SystemExit("conv tiles changed")
        path = os.path.join(out_dir, "program_has_%s.json" % mode)
        with open(path, "w") as f:
            json.dump(prog, f)
        kinds = {}
        for st in steps:
            kk = st["kind"] if st["kind"] != "host" else st["op"]
            kinds[kk] = kinds.get(kk, 0) + 1
        print("[has-prog] %s: %d steps %s, elem_add %d, elem_max %d, conv tile sha %s (= source)"
              % (os.path.basename(path), len(steps), kinds, n_add, n_max, conv_tiles_sha(prog)[:12]))


if __name__ == "__main__":
    main()
