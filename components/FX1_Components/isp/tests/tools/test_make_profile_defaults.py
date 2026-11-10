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
    with tempfile.NamedTemporaryFile("w", suffix=".csrw", delete=False) as f:
        f.write(text)
    try:
        check(csrw_codec.writes(f.name, csrw_codec.CsrMap()) == list(w), "format_profile changes the stream")
    finally:
        os.remove(f.name)
    legacy = mp.format_profile(w, "legacy")
    check(legacy.splitlines()[1] == "BLC_CTRL 0x1" and "=" not in legacy, "legacy syntax")
    print("test_make_profile_defaults: %d failure(s)" % len(failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
