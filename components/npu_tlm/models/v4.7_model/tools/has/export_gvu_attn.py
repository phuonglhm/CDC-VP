"""Export the golden FUSED_ATTN parameters and exp tables (attn_knobs.json, attn_tables.json) for tools/has/tb_gvu_fused_attn.
Usage (repository root): python3 tools/has/export_gvu_attn.py [vectors dir] [out dir]   (default out: tools/has/vec_gvu)
Writes attn_params.txt ("name Zq Zk Zv Zqk Zav Mqk TSqk Mav TSav mask_e_zero av_zv_corr asym_zp") and attn_exp_<name>.txt.
"""
import json
import os
import sys

src = sys.argv[1] if len(sys.argv) > 1 else "fe_work/has/vectors/gvu"
out = sys.argv[2] if len(sys.argv) > 2 else "tools/has/vec_gvu"
os.makedirs(out, exist_ok=True)
meta = json.load(open(os.path.join(src, "attn_knobs.json")))
tabs = json.load(open(os.path.join(src, "attn_tables.json")))
ak = meta["attn_knobs"]
assert ak["mask_rule"] == "eq_neg128" and ak["inv_sqrt_d"] == "in_Mqk", ak
with open(os.path.join(out, "attn_params.txt"), "w") as f:
    for name, h in meta["heads"].items():
        f.write("%s %d %d %d %d %d %d %d %d %d %d %d %d\n" % (
            name, h["Zq"], h["Zk"], h["Zv"], h["Zqk"], h["Zav"], h["Mqk"], h["TSqk"], h["Mav"], h["TSav"],
            int(ak["mask_e"] == "zero"), int(ak["av_zv_corr"]), int(ak["asym_zp"])))
        with open(os.path.join(out, "attn_exp_%s.txt" % name), "w") as g:
            g.write("\n".join(str(int(v)) for v in tabs[name]) + "\n")
print("exported %d heads to %s" % (len(meta["heads"]), out))
