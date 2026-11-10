#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""tools/make_profile.py defaults of the consolidated update (ISP-UPD-001/003/004/007).

    test_make_profile_defaults.py

  - aec_grid: 16 x 16 zones of ceil(D/16) when legal, otherwise N = ceil(D/Z);
    (N - 1) * Z < D <= N * Z for every D in 32..3840 (MAS v1.07 §6.21.3.1);
    2686 x 1518 -> 16 x 16 of 168 x 95, 34 -> 12 zones of 3;
  - awb_grid: 16 x 16, at most min(64, W/2) x min(32, H/2);
  - preset 'full' on 2686 x 1518 active (2688 x 1520 BGGR): LSC 17 x 13 into
    residents 1 and 2 (1768 coefficients, one mesh), profile 1 selected;
    CCM offsets 0 explicit; AWB_CTRL 16 x 16; AEC 16 x 16 of 168 x 95, clip
    0..4095, UE 272 / OE 3744; the other presets keep their grids/thresholds.
"""

import os
import sys

import numpy as np

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
import make_profile as mp  # noqa: E402

failures = []


def check(cond, what):
    if not cond:
        failures.append(what)
        print("FAIL: " + what)


def last(writes, name):
    v = [x for n, x in writes if n == name]
    return v[-1] if v else None


# ISP-UPD-009: the tuning team's list (2026-11-10), as the Tuning v1.07 names them.
TUNING_ITEMS = """TUNE_DEMOSAIC_MARGIN.GREEN TUNE_DEMOSAIC_MARGIN.RB CNF_CTRL.en CNF_CHROMA_TH.chroma_th
CNF_LUMA_TH.luma_th TUNE_CNF_BLEND.VALUE TUNE_CSC_CTRL.LOOK_EN TUNE_CSC_CTRL.FULL_RANGE TUNE_CSC_CONTRAST.VALUE
TUNE_CSC_BRIGHTNESS.VALUE TUNE_CSC_UU.VALUE TUNE_CSC_UV.VALUE TUNE_CSC_VU.VALUE TUNE_CSC_VV.VALUE TUNE_NR_CTRL.MODE
TUNE_NR_CTRL.MEDIAN_EN TUNE_NR_VARIANCE.VALUE TUNE_NR_SCALE.VALUE TUNE_NR_LIMITS.MIN TUNE_NR_LIMITS.MAX
TUNE_NR_LL_SCALE.VALUE TUNE_NR_LH_SCALE.VALUE TUNE_NR_HL_SCALE.VALUE TUNE_NR_HH_SCALE.VALUE TUNE_NR_BLEND.VALUE
TUNE_NR_SEED.VALUE NR.variance_curve TUNE_BPC_CTRL.COLOR_THRESH_EN TUNE_BPC_CTRL.FACTORY_EN TUNE_BPC_G_THRESH.K
TUNE_BPC_G_THRESH.FLOOR TUNE_BPC_RB_THRESH.K TUNE_BPC_RB_THRESH.FLOOR BPC.factory_coordinates""".split()


def tune_block(raw, csrw_codec, tempfile):
    """ISP-UPD-009: the TUNE block (tune=True) of every preset."""
    import re
    cmap = csrw_codec.CsrMap()
    hdr = open(cmap.header, encoding="utf-8").read()
    reset = lambda r: int(re.search(r"#define FX1_ISP_%s_RESET (0x[0-9A-F]+)u" % r, hdr).group(1), 16)
    tw = mp.tune_writes(cnf=True)
    for n, v in tw:
        want = cmap.regs[n].field("BPC_CONFIRM").mask if n == "TUNE_TABLE_CMD" else reset(n)
        check(v == want, "TUNE block %s = %#x, CSR reset %#x" % (n, v, want))
    check(len(TUNING_ITEMS) == 34, "34 tuning items")
    names = {n for n, _ in tw}
    for item in TUNING_ITEMS:
        r, f = item.split(".")
        if r in ("NR", "BPC"):
            continue
        check(r in names and cmap.regs[r].field(f) is not None, "tuning item %s in the TUNE block" % item)
    notes = mp.TUNE_NOTES
    curve_names = {n for n, _ in mp.nr_curve_writes([0] * 17)}
    for n in notes:
        check((n[0] if isinstance(n, tuple) else n) in names | curve_names, "TUNE_NOTES anchor %s is a block register" % (n,))
    # The commented templates: NR curve 17 x (index, data) + NR_CONFIRM, a BPC
    # point; every template line is a valid profile line once uncommented.
    tmpl = [c for c in mp.NR_TEMPLATE + sum(notes.values(), []) if re.match(r"TUNE_[A-Z_]+ ", c)]
    nr = [c for c in tmpl if c.startswith("TUNE_NR_LUT_")]
    check(nr == sum((["TUNE_NR_LUT_ADDR VALUE=%d" % k, "TUNE_NR_LUT_DATA 0x0"] for k in range(17)), []),
          "NR.variance_curve template: 17 knots with explicit index")
    check("TUNE_TABLE_CMD NR_CONFIRM=1" in tmpl and "TUNE_BPC_DATA X=0 Y=0" in tmpl, "NR_CONFIRM / BPC point template")
    for c in tmpl:
        try:
            csrw_codec.parse_line(cmap, c, "template")
        except csrw_codec.CsrwError as e:
            check(False, "template %r: %s" % (c, e))
    # BPC.factory_coordinates: the empty list, confirmed after COUNT.
    order = [n for n, _ in tw]
    check(order[-2:] == ["TUNE_BPC_COUNT", "TUNE_TABLE_CMD"] and dict(tw)["TUNE_BPC_COUNT"] == 0,
          "empty factory list: COUNT 0 then BPC_CONFIRM")
    # ISP-UPD-010: a loaded NR curve: 17 explicit-index pairs and NR_CONFIRM after
    # TUNE_NR_SEED, its note instead of the template; the BPC template stays
    # before BPC_CONFIRM; values outside 0..65535 or not 17 are refused.
    curve = [16 * k for k in range(17)]
    cw = mp.tune_writes(nr_curve=curve)
    i = [n for n, _ in cw].index("TUNE_NR_SEED") + 1
    check(cw[i:i + 35] == sum(([("TUNE_NR_LUT_ADDR", k), ("TUNE_NR_LUT_DATA", v)] for k, v in enumerate(curve)), [])
          + [("TUNE_TABLE_CMD", 2)] and dict(cw)["TUNE_NR_CTRL"] == 0x4, "NR curve loaded after the seed, MODE 0")
    ct = mp.format_profile(cw)
    check("# TUNE_NR_LUT_ADDR VALUE=0\n" not in ct and "# Loaded and confirmed; used with TUNE_NR_CTRL MODE=2 only.\n" in ct
          and "\nTUNE_NR_LUT_DATA 0x100\nTUNE_TABLE_CMD NR_CONFIRM=1\n" in ct, "loaded curve: note, no template")
    check(ct.index("# TUNE_BPC_DATA X=0 Y=0") < ct.index("TUNE_TABLE_CMD BPC_CONFIRM=1")
          and ct.index("TUNE_BPC_COUNT VALUE=0") < ct.index("# TUNE_BPC_DATA X=0 Y=0"), "BPC template before BPC_CONFIRM")
    for bad in ([0] * 16, [0] * 16 + [65536], [0] * 16 + [-1]):
        try:
            mp.nr_curve_writes(bad)
            check(False, "NR curve %r accepted" % (bad[-2:],))
        except ValueError:
            pass
    shared = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "samples", "profiles",
                          "shared_bggr_2688x1520_full.csrw")
    sw = csrw_codec.writes(shared, cmap)
    data = [v for n, v in sw if n == "TUNE_NR_LUT_DATA"]
    check(len(data) == 17 and ("TUNE_TABLE_CMD", 2) in sw and dict(sw)["TUNE_NR_CTRL"] == 0x4,
          "shared profile: the tuning NR curve loaded, MODE 0")
    for preset in mp.PRESETS:
        base, _ = mp.profile_writes(raw, 2688, 1520, "BGGR", preset)
        full, _ = mp.profile_writes(raw, 2688, 1520, "BGGR", preset, tune=True)
        has_cnf = any(n == "CNF_CTRL" for n, _ in base)
        check(not any(n.startswith("TUNE_") for n, _ in base), "%s: no TUNE write by default" % preset)
        check(full[:len(base)] == base and full[len(base):] == mp.tune_writes(cnf=not has_cnf),
              "%s: the TUNE block appended (CNF controls only if the preset has none)" % preset)
        text = mp.format_profile(full)
        check(text.startswith(mp.format_profile(base)), "%s: text before the block unchanged" % preset)
        check(text.count("# ---- TUNE extension controls") == 1 and text.count("\n# CNF thresholds") == (0 if has_cnf else 1),
              "%s: block comments once" % preset)
        with tempfile.NamedTemporaryFile("w", suffix=".csrw", delete=False) as f:
            f.write(text)
        try:
            check(csrw_codec.writes(f.name, cmap) == list(full), "%s: TUNE block text changes the stream" % preset)
        finally:
            os.remove(f.name)


def main():
    check(mp.aec_grid(2686, 1518) == (16, 16, 168, 95), "aec_grid 2686x1518 %r" % (mp.aec_grid(2686, 1518),))
    check(mp.aec_grid(34, 34) == (12, 12, 3, 3), "aec_grid 34x34 %r" % (mp.aec_grid(34, 34),))
    check(mp.aec_grid(3840, 2160) == (16, 16, 240, 135), "aec_grid 4K %r" % (mp.aec_grid(3840, 2160),))
    for d in range(32, 3841):
        n, _, z, _ = mp.aec_grid(d, 32)
        if not ((n - 1) * z < d <= n * z and 1 <= n <= 16):
            check(False, "aec_grid D=%d: N=%d Z=%d illegal" % (d, n, z))
            break
        if d >= 256 and n != 16:  # 16 zones exist from here on
            check(False, "aec_grid D=%d: %d zones, 16 expected" % (d, n))
            break
    check(mp.awb_grid(2686, 1518) == (16, 16), "awb_grid default")
    check(mp.awb_grid(20, 12) == (10, 6), "awb_grid clamp to W/2, H/2")

    raw = np.full((1520, 2688), 800, dtype=np.int64)
    w, _ = mp.profile_writes(raw, 2688, 1520, "BGGR", "full")
    coef = [v for n, v in w if n == "LSC_COEF_DATA"]
    loads = [v for n, v in w if n == "LSC_LOAD_CTRL"]
    check(last(w, "LSC_MESH_NODES") == (17 | (13 << 8)) and sum(n == "LSC_MESH_NODES" for n, _ in w) == 1,
          "one LSC_MESH_NODES 0x0D11")
    check(len(coef) == 2 * 884 and coef[:884] == coef[884:] == mp.lsc_mesh(17, 13, 1.6), "LSC 2 x 884 equal")
    check(loads == [0x101, 0x201, 0x102, 0x202], "LSC loads into 1 then 2: %r" % loads)
    check(last(w, "LSC_PROFILE_SEL") == 1, "LSC profile 1 selected")
    check(all(last(w, "CCM_OFS_" + c) == 0 for c in "RGB"), "CCM offsets 0 explicit")
    check(last(w, "AWB_CTRL") == 1 | (16 << 1) | (16 << 8), "AWB_CTRL 16x16: %r" % last(w, "AWB_CTRL"))
    check(last(w, "AEC_ZONE_CFG") == 16 | (16 << 8) and last(w, "AEC_ZONE_SIZE") == 168 | (95 << 16), "AEC 16x16 of 168x95")
    check(last(w, "AEC_SAMPLE_CLIP") == 0 | (4095 << 16) and last(w, "AEC_THRESH") == 272 | (3744 << 16),
          "AEC clip 0..4095, UE 272 / OE 3744")
    v, _ = mp.profile_writes(raw, 2688, 1520, "BGGR", "full_vga_709")
    check(last(v, "AWB_CTRL") == 1 | (64 << 1) | (32 << 8) and last(v, "AEC_THRESH") == 400 | (3600 << 16),
          "full_vga_709 keeps AWB 64x32, UE 400 / OE 3600")
    check(sum(n == "LSC_COEF_DATA" for n, _ in v) == 1768, "full_vga_709 LSC 17x13 into 1 and 2")
    q, _ = mp.profile_writes(raw, 2688, 1520, "BGGR", "gtm_manual_qhd")
    check(last(q, "AEC_THRESH") == 256 | (3840 << 16) and last(q, "AWB_CTRL") == 1 | (16 << 1) | (8 << 8),
          "gtm_manual_qhd keeps UE 256 / OE 3840, AWB 16x8")
    # ISP-UPD-008: the profile text make_profile writes (field syntax) is the same stream.
    import csrw_codec
    import tempfile
    text = mp.format_profile(w)
    check("BLC_GAIN_SEL gain_level=0 range_scale_en=1\n" in text and "AEC_CTRL en=1 commit=1\n" in text and
          "GAMMA_CTRL en=0 lut_load_confirm=1\n" in text, "format_profile is not in field syntax")
    check("CSC_CTRL std=0\n" in text and "BLC_CTRL en=1\n" in text and "LSC_COEF_DATA 0x" in text
          and "\nCSC_CTRL 0x" not in text, "one-field registers named, data ports raw")
    with tempfile.NamedTemporaryFile("w", suffix=".csrw", delete=False) as f:
        f.write(text)
    try:
        check(csrw_codec.writes(f.name, csrw_codec.CsrMap()) == list(w), "format_profile changes the stream")
    finally:
        os.remove(f.name)
    legacy = mp.format_profile(w, "legacy")
    check(legacy.splitlines()[1] == "BLC_CTRL 0x1" and "=" not in legacy, "legacy syntax")
    tune_block(raw, csrw_codec, tempfile)
    print("test_make_profile_defaults: %d failure(s)" % len(failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
