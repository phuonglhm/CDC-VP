#!/usr/bin/env python3
"""Build a deterministic *test* profile (CSR write list) for a real RAW frame.

This is NOT a sensor calibration (plan §5.2 item 4, DEC-05): every value comes
from a stated formula so that runs are reproducible and explainable. Presets
(DEC-34):

  basic           BLC profile 0: pedestal = --pedestal (12-bit scale), no trims,
                  range scale = round(4095 / (4095 - pedestal) * 2^16);
                  WB grey world on the central 50 % of the frame after BLC:
                  gain_c = round(256 * mean(G) / mean(c)), c in {R, B}, gain_g = 256;
                  Gamma preset 0 (BT.709 OETF, CSR #192 / HAS p94); CSC BT.601.
  full            every block: basic + LSC (synthetic radial 17x9 mesh, gain
                  1.0 -> 1.6), BPC static, DG (gain 1.0625), CCM (mild saturation,
                  committed), GTM auto, 2DNR, EE, CNF (committed), Resizer FHD,
                  AEC 32x24 zones of 84x64, AWB 32x16, AF.
  full_vga_709    full with Resizer VGA (decimation path), CSC BT.709, Gamma 2.2,
                  LSC profile 1 at strength 0.75, DG gain 0.9375 (attenuation),
                  AEC 16x12 of 168x128 with clipping, AWB at the maximum 64x32 grid.
  gtm_manual_qhd  basic + GTM manual LUT (software S-curve), Resizer qHD, 2DNR,
                  EE with radial gains and non-unity tables, BPC static, AEC with
                  an illegal grid (global + histogram only), AWB global only, AF.

Input: an ISP-contract container file from raw_fixture.py (bits [11:0]).
"""

import argparse
import math

import numpy as np

BAYER = {"RGGB": 0, "GRBG": 1, "GBRG": 2, "BGGR": 3}
PRESETS = ("basic", "full", "full_vga_709", "gtm_manual_qhd")
# Frames per run: presets with temporal state (GTM curve, 2DNR variance) run twice.
FRAMES = {"basic": 1, "full": 2, "full_vga_709": 2, "gtm_manual_qhd": 2}


def gamma_curve(preset):
    """Software-built Gamma curves of CSR #192 / HAS p94-95: 0 = BT.709 OETF, 3 = power 1/2.2."""
    def f(v):
        if preset == 0:
            return 4.5 * v if v < 0.018 else 1.099 * v ** 0.45 - 0.099
        return v ** (1.0 / 2.2)
    return [min(4095, max(0, math.floor(4095 * f(i / 255) + 0.5))) for i in range(256)]


def q39(v):
    return int(round(v * 512)) & 0xFFF


def lsc_load(nx, ny, dest, peak):
    """Synthetic radial mesh, UQ3.18, 1.0 at the centre to `peak` at the corners."""
    w = [("LSC_MESH_NODES", nx | (ny << 8)), ("LSC_LOAD_CTRL", dest | 0x100)]
    for j in range(ny):
        for i in range(nx):
            r2 = ((i / (nx - 1) - 0.5) ** 2 + (j / (ny - 1) - 0.5) ** 2) * 2
            for c in range(4):
                w.append(("LSC_COEF_DATA", int(round((1.0 + (peak - 1.0) * r2 + 0.02 * c) * (1 << 18)))))
    w.append(("LSC_LOAD_CTRL", dest | 0x200))
    return w


def ccm_writes(m):
    names = ["CRR", "CRG", "CRB", "CGR", "CGG", "CGB", "CBR", "CBG", "CBB"]
    return [("CCM_" + n, q39(m[i // 3][i % 3])) for i, n in enumerate(names)] + [("CCM_CTRL", 0x5)]


def aec(nx, ny, zw, zh, mn=0, mx=4095, ue=256, oe=3840):
    return [("AEC_ZONE_CFG", nx | (ny << 8)), ("AEC_ZONE_SIZE", zw | (zh << 16)),
            ("AEC_SAMPLE_CLIP", mn | (mx << 16)), ("AEC_THRESH", ue | (oe << 16)), ("AEC_CTRL", 3)]


def awb(nx, ny, under, sat):
    return [("AWB_UNDEREXPOSED_LIMIT", under), ("AWB_SATURATION_LIMIT", sat),
            ("AWB_CTRL", 1 | (nx << 1) | (ny << 8))]


def resizer(scale):
    return [("RESIZER_CTRL", 0x1 | 0x4 | (scale << 3))]


def gamma(preset):
    return [("GAMMA_LUT_ADDR", 0)] + [("GAMMA_LUT_DATA", v) for v in gamma_curve(preset)] + [("GAMMA_CTRL", 1)]


def grey_world(raw12, width, height, bayer, pedestal):
    code = BAYER[bayer]
    sh, sv = code & 1, code >> 1
    p = raw12.reshape(height, width).astype(np.int64)[sv:, sh:]
    p = p[: p.shape[0] // 2 * 2, : p.shape[1] // 2 * 2]      # RGGB phase, as after the Input Formatter
    d = np.maximum(p - pedestal, 0)
    h, w = d.shape
    even = lambda v: v // 2 * 2                                   # keep the RGGB phase
    c = d[even(h // 4): even(3 * h // 4), even(w // 4): even(3 * w // 4)]
    r, g, b = c[0::2, 0::2].mean(), (c[0::2, 1::2].mean() + c[1::2, 0::2].mean()) / 2, c[1::2, 1::2].mean()
    gain = lambda m: min(0xFFF, max(1, round(256 * g / max(m, 1e-9))))
    return gain(r), gain(b), (r, g, b)


def profile_writes(raw12, width, height, bayer, preset="basic", pedestal=200):
    """Returns (writes, note) for one preset. raw12: container samples, bits [11:0]."""
    if preset not in PRESETS:
        raise ValueError("unknown preset %r" % preset)
    gr, gb, means = grey_world(raw12, width, height, bayer, pedestal)
    w = [("COMMON_BAYER", BAYER[bayer]),
         ("BLC_CTRL", 1), ("BLC_GAIN_SEL", 0x100),        # profile 0, range scale on
         ("BLC_OFS_G0_DFT", pedestal),
         ("BLC_SCALE_G0", round(4095 / (4095 - pedestal) * 65536)),
         ("WB_CTRL", 1), ("WB_GAIN_R", gr), ("WB_GAIN_G", 0x100), ("WB_GAIN_B", gb)]
    ids = [("AEC_CONTEXT_ID", 0xA1), ("AWB_CONTEXT_ID", 0xA2), ("AF_CONTEXT_ID", 0xA3)]
    if preset == "basic":
        w += gamma(0) + [("CSC_CTRL", 0)]
    elif preset in ("full", "full_vga_709"):
        vga = preset == "full_vga_709"
        w += lsc_load(17, 9, 1 if vga else 0, 1.6)
        w += [("LSC_PROFILE_SEL", 1 if vga else 0), ("LSC_STRENGTH", 0xC000 if vga else 0x10000), ("LSC_CTRL", 1)]
        w += [("BPC_MODE", 0), ("BPC_THRESH", (77 << 16) | 402), ("BPC_CTRL", 1)]
        w += [("DG_CTRL", 1), ("DG_GAIN", 0x0F0 if vga else 0x110)]   # UQ4.8
        w += ccm_writes([[1.4, -0.3, -0.1], [-0.2, 1.4, -0.2], [-0.1, -0.4, 1.5]])
        w += gamma(3 if vga else 0) + [("CSC_CTRL", 1 if vga else 0)]
        w += [("GTM_KEY", 0xBB), ("GTM_LWHITE", 0x0180), ("GTM_ROI_LOG2", 11), ("GTM_CTRL", 1)]
        w += [("NR_2D_CTRL", 1)]
        w += [("EE_ALPHA", 0x6000), ("EE_BETA", 0x4000), ("EE_CLAMP", (40 << 8) | 30), ("EE_CTRL", 1)]
        w += [("CNF_CHROMA_TH", 30), ("CNF_LUMA_TH", 20), ("CNF_CTRL", 0x5)]
        w += resizer(14 if vga else 5)
        if vga:
            w += aec(16, 12, 168, 128, mn=64, mx=3968, ue=400, oe=3600) + awb(64, 32, 32, 3900)
        else:
            w += aec(32, 24, 84, 64) + awb(32, 16, 16, 4000)
        w += [("AF_CTRL", 1)] + ids
    else:  # gtm_manual_qhd
        w += gamma(0) + [("CSC_CTRL", 0)]
        w += [("BPC_MODE", 0), ("BPC_THRESH", (60 << 16) | 380), ("BPC_CTRL", 1)]
        s_curve = [min(0xFFFF, max(0, int(round(256 * (1.0 + 0.6 * math.sin(math.pi * i / 64)))))) for i in range(65)]
        w += [("GTM_CTRL", 2), ("GTM_LUT_ADDR", 0)] + [("GTM_LUT_DATA", v) for v in s_curve] + [("GTM_CTRL", 3)]
        w += [("NR_2D_CTRL", 1)]
        for bank, n in ((0, 64), (1, 64), (2, 32), (3, 32)):
            w += [("EE_LUT_CTRL", bank << 1), ("EE_LUT_ADDR", 0)]
            w += [("EE_LUT_WDATA", 0x6000 + ((i * 977 + bank * 311) % 0x3000)) for i in range(n)]
        # Radial centre in doubled coordinates of the cropped frame (W-1, H-1 = centre);
        # thresholds at 10 / 35 / 70 % of the corner radius squared.
        sh, sv = BAYER[bayer] & 1, BAYER[bayer] >> 1
        aw, ah = (width - sh) // 2 * 2, (height - sv) // 2 * 2
        r2max = (aw - 1) ** 2 + (ah - 1) ** 2
        w += [("EE_ALPHA", 0x7000), ("EE_BETA", 0x5000), ("EE_CLAMP", (40 << 8) | 30), ("EE_FEATURE_EN", 7),
              ("EE_RADIAL_CENTER", ((ah - 1) << 16) | (aw - 1)), ("EE_RADIAL_R2_TH0", r2max // 10),
              ("EE_RADIAL_R2_TH1", r2max * 35 // 100), ("EE_RADIAL_R2_TH2", r2max * 70 // 100),
              ("EE_RADIAL_GAIN_01", (0x9000 << 16) | 0x8000), ("EE_RADIAL_GAIN_23", (0xC000 << 16) | 0xA000),
              ("EE_CTRL", 1)]
        w += resizer(7)
        w += aec(0, 4, 84, 64) + awb(0, 0, 16, 4000) + [("AF_CTRL", 1)] + ids
    note = "grey world means after BLC: R %.1f G %.1f B %.1f" % means
    return w, note


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--input", required=True)
    ap.add_argument("--width", type=int, required=True)
    ap.add_argument("--height", type=int, required=True)
    ap.add_argument("--bayer", required=True, choices=sorted(BAYER))
    ap.add_argument("--preset", default="basic", choices=PRESETS)
    ap.add_argument("--pedestal", type=int, default=200)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    raw = np.fromfile(a.input, dtype="<u2").astype(np.int64) & 0xFFF
    writes, note = profile_writes(raw, a.width, a.height, a.bayer, a.preset, a.pedestal)
    with open(a.out, "w", encoding="utf-8") as f:
        f.write("# test profile '%s' (not a calibration), tools/make_profile.py\n# %s\n" % (a.preset, note))
        for n, v in writes:
            f.write("%s 0x%X\n" % (n, v))
    print("wrote %s (%s, %d writes): %s" % (a.out, a.preset, len(writes), note))


if __name__ == "__main__":
    main()
