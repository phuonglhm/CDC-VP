"""Export the golden LAYERNORM parameters (ln_knobs.json) for tools/has/tb_gvu_layernorm.
Usage (repository root): python3 tools/has/export_gvu_ln.py [vectors dir] [out dir]   (default out: tools/has/vec_gvu)
Writes ln_params_<case>.txt: line 1 scalars, then gamma / beta / M0_mul / TS_mul lines (per channel).
"""
import json
import os
import sys

src = sys.argv[1] if len(sys.argv) > 1 else "fe_work/has/vectors/gvu"
out = sys.argv[2] if len(sys.argv) > 2 else "tools/has/vec_gvu"
os.makedirs(out, exist_ok=True)
meta = json.load(open(os.path.join(src, "ln_knobs.json")))
lk = meta["ln_knobs"]
names = []
for name, c in meta["cases"].items():
    p = c["params"]
    H = p["H"]
    per = lambda v: v if isinstance(v, list) else [v] * H
    scal = [H, p["Pre_Shift"], p["M0_var"], p["TS_var"], p["Z_var"], p["M0_7"], p["TS_7"], p["E_bias"], p["Z_7"],
            p["M0_div"], p["TS_div"], p["Z_div"], p["Z_mul"], p["Z_out"],
            int(lk["r8_floor"] == "z_out"), lk["r8_z_out"], int(lk["r9_split"] == "exact"), lk["r9_lsb_bits"],
            int(lk["out_fmt"] == "int16")]
    with open(os.path.join(out, "ln_params_%s.txt" % name), "w") as f:
        f.write(" ".join(str(int(v)) for v in scal) + "\n")
        for key in ("gamma_q", "beta_q", "M0_mul", "TS_mul"):
            f.write(" ".join(str(int(v)) for v in per(p[key])) + "\n")
    names.append(name)
with open(os.path.join(out, "ln_cases.txt"), "w") as f:
    f.write("\n".join(names) + "\n")
print("exported %d LN cases to %s (out_fmt %s)" % (len(names), out, lk["out_fmt"]))
