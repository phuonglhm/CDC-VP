"""Requant parameters in the HAS drawing format.

Derived from pc_p9999 (no rounding compensation in the bias, since the HAS requant rounds itself):
  Out_Scale int32 in [2^30, 2^31): S' = ceil(S / 2), shift' = shift - 1   (source S is in [2^31, 2^32))
  Out_Shift int8, zp_out = 0, round_mode = 1 (HALF_UP, HAS_IFACE §3).
S' = ceil(S/2) changes M = S/2^shift by < 2^-31 relative (0 when S is even); S' is clamped to 2^31 - 1.
Keys written: <job>/scale (= S', uint32, so fe_ref_int8 uses it), <job>/shift (= shift'),
<job>/round_mode, and the HAS_IFACE §7 keys <job>/scale_i32, <job>/shift_i8, <job>/zp_out.
Output: FE_WORK/step3/<source>_has/  (default source pc_p9999 -> pc_p9999_has)
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fe_common as fc  # noqa: E402

import numpy as np  # noqa: E402


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "pc_p9999"
    sdir = os.path.join(fc.FE_WORK, "step3", src)
    ddir = os.path.join(fc.FE_WORK, "step3", src + "_has")
    with open(os.path.join(sdir, "params.json")) as f:
        doc = json.load(f)
    if doc["config"].get("round_comp"):
        raise SystemExit("%s has rounding compensation in the bias; HAS config must start from a non-_rc config" % src)
    p = dict(np.load(os.path.join(sdir, "params.npz")))
    jobs = sorted({k.split("/")[0] for k in p})
    n_ch = n_odd = n_clamp = 0
    rel_err = []
    shifts = []
    for j in jobs:
        s = p[j + "/scale"].astype(np.int64)
        h = p[j + "/shift"].astype(np.int64)
        if s.min() < (1 << 31) or s.max() >= (1 << 32):
            raise SystemExit("%s: scale outside [2^31, 2^32), renormalisation rule does not apply" % j)
        s2 = (s + 1) >> 1
        n_clamp += int((s2 > (1 << 31) - 1).sum())
        s2 = np.minimum(s2, (1 << 31) - 1)
        h2 = h - 1
        if h2.min() < 1 or h2.max() > 63:
            raise SystemExit("%s: shift' outside [1, 63]" % j)
        n_ch += s.size
        n_odd += int((s & 1).sum())
        rel_err.append(np.abs(s2 * 2.0 - s) / s)
        shifts.append(h2)
        p[j + "/scale"] = s2.astype(np.uint32)
        p[j + "/shift"] = h2.astype(p[j + "/shift"].dtype)
        p[j + "/round_mode"] = np.int64(1)
        p[j + "/scale_i32"] = s2.astype(np.int32)
        p[j + "/shift_i8"] = h2.astype(np.int8)
        p[j + "/zp_out"] = np.int16(0)
    os.makedirs(ddir, exist_ok=True)
    np.savez(os.path.join(ddir, "params.npz"), **p)
    doc["config"] = dict(doc["config"], derived_from=src, scale_format="I32 [2^30, 2^31)",
                         rounding="HALF_UP in requant (HAS_IFACE §3), no bias compensation", zp_out=0)
    with open(os.path.join(ddir, "params.json"), "w") as f:
        json.dump(doc, f, indent=1)
    e = np.concatenate(rel_err)
    hs = np.concatenate(shifts)
    print("[has_cfg] %s -> %s: jobs %d, channels %d, odd S %d, clamped %d, M rel err max %.3e, shift' [%d, %d]"
          % (src, os.path.basename(ddir), len(jobs), n_ch, n_odd, n_clamp, e.max(), hs.min(), hs.max()))


if __name__ == "__main__":
    main()
