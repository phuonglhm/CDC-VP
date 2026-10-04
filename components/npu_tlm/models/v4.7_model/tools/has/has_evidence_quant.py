"""Evidence for HW questions H1 (rounding mode) and H2 (int64 -> int16 narrowing before clamp) on real YOLOv8m data.

This is a MEASUREMENT script, not a golden: it runs the v4.5 int8 chain once
(tools/fe/fe_ref_int8.run_int8, config pc_p9999_rc) to get the real conv accumulators of every layer, then applies each requant
variant of the QUANTIZE drawing to those same accumulators and counts what changes. Arithmetic follows has/HAS_IFACE.md §3-§4.

HAS-format parameters are DERIVED here from pc_p9999 (no rounding compensation, because the HW rounds itself):
  Out_Scale int32 = scale_u32 >> 1   (scale_u32 in [2^31, 2^32) -> [2^30, 2^31))
  Out_Shift       = shift - 1
When the official HAS parameter set (pc_p9999_has, tools/fe/fe_make_has_config.py) is present it is used instead.

Usage (repository root):  python3 tools/has/has_evidence_quant.py [--image <path>] [--out tools/has/out]
"""
import argparse
import json
import os
import sys

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools", "fe"))
import fe_common as fc  # noqa: E402
import fe_ref_int8 as fr  # noqa: E402

MODES = {"FLOOR": 0, "HALF_UP": 1, "HALF_AWAY": 2, "HALF_EVEN": 3}


def rshift(p, s, mode):
    """Round-shift of int64 array p by per-element/channel shift s (HAS_IFACE §3)."""
    s = s.astype(np.int64)
    half = np.where(s > 0, np.left_shift(np.int64(1), np.maximum(s - 1, 0)), 0)
    if mode == "FLOOR":
        return np.right_shift(p, s)
    if mode == "HALF_UP":
        return np.right_shift(p + half, s)
    if mode == "HALF_AWAY":
        q = np.right_shift(np.abs(p) + half, s)
        return np.where(p < 0, -q, q)
    q = np.right_shift(p, s)                       # HALF_EVEN
    rem = p - np.left_shift(q, s)
    return q + ((rem > half) | ((rem == half) & (q & 1 == 1))).astype(np.int64)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", default=None, help="image path (default: coco8 image 0)")
    ap.add_argument("--out", default=os.path.join(ROOT, "tools", "has", "out"))
    args = ap.parse_args()
    img = args.image or fc.list_coco8_images()[0]
    with open(os.path.join(fc.FE_WORK, "step2", "graph_ir.json")) as f:
        ir = json.load(f)
    p_rc = dict(np.load(os.path.join(fc.FE_WORK, "step3", "pc_p9999_rc", "params.npz")))
    # Official HAS-format config (S' = ceil(S/2) int32, s' = s - 1) when present, else derive from pc_p9999
    has_npz = os.path.join(fc.FE_WORK, "step3", "pc_p9999_has", "params.npz")
    official = os.path.exists(has_npz)
    p_no = dict(np.load(has_npz if official else os.path.join(fc.FE_WORK, "step3", "pc_p9999", "params.npz")))
    print("HAS parameters:", "OFFICIAL pc_p9999_has" if official else "DERIVED from pc_p9999")
    t = fr.run_int8(ir, p_rc, fc.letterbox_input(img), keep_psum=True)

    rows, tot = [], {"elems": 0, "ovf64": 0, "gt16": 0, "wrap_changes": 0, "m3_vs_has": 0, "m3_vs_has_lut": 0}
    for m in MODES:
        tot["diff_" + m] = 0
    for o in ir["ops"]:
        if o["op"] != "conv":
            continue
        j = o["job"]
        psum = t[o["output"] + "@psum"]
        ch = lambda a: a.astype(np.int64)[None, :, None, None]  # noqa: E731
        # v4.5 reference output (floor, u32, rc bias) must equal the chain's own @qpre -- self-check of this script
        acc_rc = psum + ch(p_rc[j + "/q_b"])
        m3 = np.clip(np.right_shift(acc_rc * ch(p_rc[j + "/scale"]), ch(p_rc[j + "/shift"])), -128, 127)
        assert np.array_equal(m3.astype(np.int8), t[o["output"] + "@qpre"]), j
        # HAS-format (derived) accumulator: bias WITHOUT rounding compensation, S int32, s = shift - 1
        acc = psum + ch(p_no[j + "/q_b"])
        if official:
            S = ch(p_no[j + "/scale_i32"])
            s = np.broadcast_to(ch(p_no[j + "/shift_i8"]), acc.shape)
        else:
            S = ch(p_no[j + "/scale"].astype(np.uint64) >> 1)
            s = np.broadcast_to(ch(p_no[j + "/shift"]) - 1, acc.shape)
        prod = acc * S
        ovf64 = int((np.abs(acc).astype(np.float64) * S.astype(np.float64) >= 2.0 ** 63).sum())
        outs = {}
        gt16 = 0
        for mname in MODES:
            r = rshift(prod, s, mname)
            if mname == "HALF_UP":
                gt16 = int((np.abs(r) > 32767).sum())
                wrap = ((r + 32768) % 65536) - 32768
                wrap_changes = int((np.clip(wrap, -128, 127) != np.clip(r, -128, 127)).sum())
            outs[mname] = np.clip(r, -128, 127)
        lut = p_no.get(j + "/lut")
        has = outs["HALF_UP"]
        n = int(acc.size)
        row = {"layer": j, "elems": n, "max_abs_r": int(np.abs(rshift(prod, s, "HALF_UP")).max()), "gt16": gt16,
               "wrap_changes": wrap_changes, "ovf64": ovf64,
               "m3_vs_has": int((m3 != has).sum())}
        if o["act"] == "silu" and lut is not None:
            L = lut.astype(np.int64)
            row["m3_vs_has_lut"] = int((L[m3 + 128] != L[has + 128]).sum())
        else:
            row["m3_vs_has_lut"] = row["m3_vs_has"]
        for mname in MODES:
            row["diff_" + mname] = int((outs[mname] != has).sum())
        rows.append(row)
        for k in tot:
            tot[k] += row.get(k, 0)

    os.makedirs(args.out, exist_ok=True)
    name = os.path.splitext(os.path.basename(img))[0]
    md = os.path.join(args.out, "evidence_quant_%s.md" % name)
    with open(md, "w") as f:
        f.write("# Evidence H1/H2 on real YOLOv8m accumulators -- image %s\n\n" % os.path.basename(img))
        f.write("HAS parameters: %s. Per-layer effect on the same accumulators (not propagated through the network).\n\n"
                % ("OFFICIAL fe_work/step3/pc_p9999_has (scale_i32, shift_i8)" if official
                   else "DERIVED from pc_p9999 (S = scale>>1, s = shift-1)"))
        f.write("| Metric | Value |\n|---|---:|\n")
        f.write("| Conv layers / output elements | %d / %s |\n" % (len(rows), format(tot["elems"], ",")))
        f.write("| **H2**: elements with abs(r) > 32767 before narrowing (HALF_UP) | %s |\n" % format(tot["gt16"], ","))
        f.write("| **H2**: int8 outputs that change if int16 narrowing WRAPS instead of saturating | %s |\n" % format(tot["wrap_changes"], ","))
        f.write("| int64 product overflow (abs(acc)*S >= 2^63) | %s |\n" % format(tot["ovf64"], ","))
        for mname in MODES:
            f.write("| **H1**: int8 outputs differing %s vs HALF_UP | %s (%.3f %%) |\n"
                    % (mname, format(tot["diff_" + mname], ","), 100.0 * tot["diff_" + mname] / tot["elems"]))
        f.write("| v4.5 (floor + rc bias, u32) vs HAS (HALF_UP, i32), before LUT | %s (%.3f %%) |\n"
                % (format(tot["m3_vs_has"], ","), 100.0 * tot["m3_vs_has"] / tot["elems"]))
        f.write("| same, after the SiLU LUT | %s (%.3f %%) |\n\n"
                % (format(tot["m3_vs_has_lut"], ","), 100.0 * tot["m3_vs_has_lut"] / tot["elems"]))
        f.write("| Layer | Elements | max abs(r) | >int16 | wrap changes | FLOOR diff | HALF_AWAY diff | HALF_EVEN diff | v4.5 vs HAS |\n")
        f.write("|---|---:|---:|---:|---:|---:|---:|---:|---:|\n")
        for r in rows:
            f.write("| %s | %s | %s | %d | %d | %d | %d | %d | %d |\n" % (
                r["layer"], format(r["elems"], ","), format(r["max_abs_r"], ","), r["gt16"], r["wrap_changes"],
                r["diff_FLOOR"], r["diff_HALF_AWAY"], r["diff_HALF_EVEN"], r["m3_vs_has"]))
    print("wrote", md)
    print(json.dumps(tot))


if __name__ == "__main__":
    main()
