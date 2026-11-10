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
  full            every block: basic + LSC (synthetic radial 17x13 mesh, gain
                  1.0 -> 1.6, loaded into residents 1 and 2, profile 1 selected;
                  the firmware fills resident 0, ISP-UPD-001), BPC static, DG
                  (gain 1.0625), CCM (mild saturation, offsets 0 explicit), GTM
                  auto, 2DNR, EE, CNF (committed), Resizer FHD, AEC 16x16 zones
                  covering the active image (aec_grid; UE 272, OE 3744), AWB
                  16x16 (ISP-UPD-003/004), AF.
  full_vga_709    full with Resizer VGA (decimation path), CSC BT.709, Gamma 2.2,
                  LSC profile 1 at strength 0.75, DG gain 0.9375 (attenuation),
                  AEC 16x12 of 168x128 with clipping (UE 400, OE 3600), AWB at
                  the maximum 64x32 grid.
  gtm_manual_qhd  basic + GTM manual LUT (software S-curve), Resizer qHD, 2DNR,
                  EE with radial gains and non-unity tables, BPC static, AEC 8x6
                  zones sized to cover the active image, AWB 16x8, AF. (Before
                  V107-10 an illegal AEC/AWB grid, dropped under v1.07; the
                  illegal and reset grids are covered by the dedicated vectors
                  stats_illegal_grids_64x32 and stats_reset_grids_af_edges_130x90.)

Every preset can end with the TUNE block (tune=True; the command line's
default, --no-tune omits it; ISP-UPD-009): the TUNE extension controls the
tuning team adjusts -- TUNE_DEMOSAIC_MARGIN, TUNE_CNF_BLEND (and the CNF
controls where the preset has none), TUNE_CSC_*, TUNE_NR_*, TUNE_BPC_* and the
empty factory bad-pixel list -- at their CSR reset values (the Tuning v1.07
startup values), so the output is that of the preset without them, with a
commented template for the NR variance curve. The library default is
tune=False: the vectors, fixtures and the RAW regression built from presets
stay as they were.

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


def lsc_mesh(nx, ny, peak):
    """Synthetic radial mesh, UQ4.18 (ISP-UPD-001), 1.0 at the centre to `peak` at the
    corners, +0.02 per channel R, Gr, Gb, B; nodes x then y. A function of the
    normalised node position, so a 17x13 mesh samples the same surface as 17x9."""
    m = []
    for j in range(ny):
        for i in range(nx):
            r2 = ((i / (nx - 1) - 0.5) ** 2 + (j / (ny - 1) - 0.5) ** 2) * 2
            for c in range(4):
                m.append(int(round((1.0 + (peak - 1.0) * r2 + 0.02 * c) * (1 << 18))))
    return m


def lsc_load(nx, ny, dests, peak):
    """One mesh loaded into each resident of `dests` (ISP-UPD-001: the same mesh
    duplicated, each resident its own storage). LSC_MESH_NODES is written once:
    any write of it invalidates every resident."""
    mesh = lsc_mesh(nx, ny, peak)
    w = [("LSC_MESH_NODES", nx | (ny << 8))]
    for dest in dests:
        w += [("LSC_LOAD_CTRL", dest | 0x100)] + [("LSC_COEF_DATA", v) for v in mesh] + [("LSC_LOAD_CTRL", dest | 0x200)]
    return w


def ccm_writes(m, offsets=(0, 0, 0)):
    """The 12 CCM parameters explicitly: 9 coefficients (Q3.9) and the R, G, B
    offsets in output pixel levels (signed 12 bit, ISP-UPD-007; default 0, owner
    2026-11-09), then the enable."""
    names = ["CRR", "CRG", "CRB", "CGR", "CGG", "CGB", "CBR", "CBG", "CBB"]
    return ([("CCM_" + n, q39(m[i // 3][i % 3])) for i, n in enumerate(names)] +
            [("CCM_OFS_" + c, offsets[i] & 0xFFF) for i, c in enumerate("RGB")] + [("CCM_CTRL", 0x5)])


def aec(nx, ny, zw, zh, mn=0, mx=4095, ue=272, oe=3744):
    """AEC zone grid, clip [mn, mx] inclusive, UE/OE thresholds (RAW12, strict).
    Default thresholds UE 272 / OE 3744 (ISP-UPD-004; the CSR reset values)."""
    return [("AEC_ZONE_CFG", nx | (ny << 8)), ("AEC_ZONE_SIZE", zw | (zh << 16)),
            ("AEC_SAMPLE_CLIP", mn | (mx << 16)), ("AEC_THRESH", ue | (oe << 16)), ("AEC_CTRL", 3)]


def aec_grid(aw, ah, n=16):
    """Default AEC zone grid on the active image (ISP-UPD-004): n x n zones when
    legal; the zone size is ceil(D / n) and the zone count ceil(D / size), so
    (N - 1) * Z < D <= N * Z always holds (MAS v1.07 §6.21.3.1) and N < n only
    where n zones cannot cover D (e.g. D = 34: Z = 3, N = 12)."""
    zw, zh = -(-aw // n), -(-ah // n)
    return -(-aw // zw), -(-ah // zh), zw, zh


def awb_grid(aw, ah, n=16):
    """Default AWB grid (ISP-UPD-003): n x n zones, at most min(64, W/2) x min(32, H/2)."""
    return min(n, 64, aw // 2), min(n, 32, ah // 2)


def awb(nx, ny, under, sat):
    return [("AWB_UNDEREXPOSED_LIMIT", under), ("AWB_SATURATION_LIMIT", sat),
            ("AWB_CTRL", 1 | (nx << 1) | (ny << 8))]


def resizer(scale):
    return [("RESIZER_CTRL", 0x1 | 0x4 | (scale << 3))]


def gamma(preset):
    # v1.07: the curve applies only after LUT_LOAD_CONFIRM with EN = 0 (MAS §6.14.4).
    return [("GAMMA_LUT_ADDR", 0)] + [("GAMMA_LUT_DATA", v) for v in gamma_curve(preset)] + \
        [("GAMMA_CTRL", 1 << 8), ("GAMMA_CTRL", 1)]


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


# ISP-UPD-009 (2026-11-10): the TUNE extension controls, whole-register CSR
# reset values (= Tuning v1.07 startup values; tests/tools/test_make_profile_defaults.py
# checks them against the header's _RESET macros). The tuning team's order.
TUNE_FIRST = "TUNE_DEMOSAIC_MARGIN"
CNF_RESET = [("CNF_CHROMA_TH", 0), ("CNF_LUMA_TH", 0), ("CNF_CTRL", 0)]
NR_KNOTS = 17


def tune_writes(cnf=False):
    """The TUNE block. cnf: also the CNF controls (for a preset that has none)."""
    return ([(TUNE_FIRST, 0), ("TUNE_CNF_BLEND", 0x100)] + (CNF_RESET if cnf else [])
            + [("TUNE_CSC_CTRL", 0x2), ("TUNE_CSC_CONTRAST", 0x100), ("TUNE_CSC_BRIGHTNESS", 0),
               ("TUNE_CSC_UU", 0x4000), ("TUNE_CSC_UV", 0), ("TUNE_CSC_VU", 0), ("TUNE_CSC_VV", 0x4000)]
            + [("TUNE_NR_CTRL", 0x4), ("TUNE_NR_VARIANCE", 0), ("TUNE_NR_SCALE", 0x100),
               ("TUNE_NR_LIMITS", 0xFFFF0000)]
            + [("TUNE_NR_%s_SCALE" % b, 0x100) for b in ("LL", "LH", "HL", "HH")]
            + [("TUNE_NR_BLEND", 0x100), ("TUNE_NR_SEED", 0)]
            + [("TUNE_BPC_CTRL", 0), ("TUNE_BPC_G_THRESH", 0x100), ("TUNE_BPC_RB_THRESH", 0x100),
               ("TUNE_BPC_COUNT", 0), ("TUNE_TABLE_CMD", 1 << 2)])   # BPC_CONFIRM of the empty list


# Comments format_profile writes before the first write of a register in the
# TUNE block. The templates are valid profile lines once uncommented.
TUNE_NOTES = {
    TUNE_FIRST: [
        "---- TUNE extension controls (CSR v1.07 TUNE page, ISP-UPD-009) ----",
        "Values: the CSR reset, i.e. the Tuning v1.07 startup value. In profile",
        "mode (the firmware's default bring-up; run_raw --profile-mode) they are",
        "written into the bank after INIT; legacy mode keeps the reset values in",
        "the datapath (MAS v1.07 9.9.2).",
        "Demosaic: equal-gradient margins (0 keeps the exact-tie decision).",
    ],
    "TUNE_CNF_BLEND": ["CNF: weight of the filtered chroma, 0..256 (256 = the CNF result)."],
    "CNF_CHROMA_TH": ["CNF thresholds and enable (legacy controls, also live in legacy mode)."],
    "TUNE_CSC_CTRL": [
        "CSC: LOOK_EN enables contrast, brightness and the chroma matrix (Q4.14);",
        "FULL_RANGE 1 = full-range output, 0 = limited range (the colorimetry of the NV12).",
    ],
    "TUNE_NR_CTRL": [
        "2DNR: MODE 0 automatic (prior-frame estimate; TUNE_NR_SEED until one is valid),",
        "1 manual (TUNE_NR_VARIANCE), 2 variance curve (below); MEDIAN_EN prefilter.",
    ],
    "TUNE_BPC_CTRL": (
        ["NR.variance_curve (MODE 2 only, calibration required): %d knots at luma" % NR_KNOTS,
         "0, 16, ..., 240, 255, each with an explicit index (no auto-increment), then",
         "NR_CONFIRM. Not loaded here: SEAL refuses MODE 2 (NOT_READY) until it is."]
        + sum((["TUNE_NR_LUT_ADDR VALUE=%d" % k, "TUNE_NR_LUT_DATA 0x0"] for k in range(NR_KNOTS)), [])
        + ["TUNE_TABLE_CMD NR_CONFIRM=1",
           "BPC: COLOR_THRESH_EN selects the G and RB thresholds; FACTORY_EN the point list."]),
    "TUNE_BPC_COUNT": [
        "BPC.factory_coordinates: COUNT points (at most 1024), active Bayer coordinates,",
        "sorted by y*W+x, unique. Point i: TUNE_BPC_ADDR VALUE=i, then TUNE_BPC_DATA",
        "X=x Y=y. BPC_CONFIRM checks the list. Empty at startup:",
    ],
    "TUNE_TABLE_CMD": ["TUNE_BPC_ADDR VALUE=0", "TUNE_BPC_DATA X=0 Y=0"],
}


def profile_writes(raw12, width, height, bayer, preset="basic", pedestal=200, tune=False):
    """Returns (writes, note) for one preset. raw12: container samples, bits [11:0].
    tune: end with the TUNE block (tune_writes)."""
    if preset not in PRESETS:
        raise ValueError("unknown preset %r" % preset)
    gr, gb, means = grey_world(raw12, width, height, bayer, pedestal)
    scale = round(4095 / (4095 - pedestal) * 65536)
    w = ([("COMMON_BAYER", BAYER[bayer]),
          ("BLC_CTRL", 1), ("BLC_GAIN_SEL", 0x100),       # profile 0, range scale on
          ("BLC_OFS_G0_DFT", pedestal)]
         + [("BLC_SCALE_G0_" + c, scale) for c in ("R", "GR", "GB", "B")]   # ISP-UPD-002: per channel, equal
         + [("WB_CTRL", 1), ("WB_GAIN_R", gr), ("WB_GAIN_G", 0x100), ("WB_GAIN_B", gb)])
    ids = [("AEC_CONTEXT_ID", 0xA1), ("AWB_CONTEXT_ID", 0xA2), ("AF_CONTEXT_ID", 0xA3)]
    if preset == "basic":
        w += gamma(0) + [("CSC_CTRL", 0)]
    elif preset in ("full", "full_vga_709"):
        vga = preset == "full_vga_709"
        # Boot sequence of MAS Table 31 / owner Q5: profile 0 is active at reset
        # and is never a destination, so load profile 1, validate, select it.
        # The mesh may not have more nodes than the active image has pixels on
        # either axis (MAS §6.8.4, CSR 07/10); clamp it on small frames.
        sh, sv = BAYER[bayer] & 1, (BAYER[bayer] >> 1) & 1
        aw, ah = (width - sh) // 2 * 2, (height - sv) // 2 * 2
        # ISP-UPD-001: target mesh 17x13 (user, 2026-11-09), the same radial
        # formula; duplicated into residents 1 and 2 here, the firmware copies it
        # into 0 after the first SOF (0 is active at reset and never a destination).
        w += lsc_load(min(17, aw), min(13, ah), (1, 2), 1.6)
        w += [("LSC_PROFILE_SEL", 1), ("LSC_STRENGTH", 0xC000 if vga else 0x10000), ("LSC_CTRL", 1)]
        w += [("BPC_MODE", 0), ("BPC_THRESH", (77 << 16) | 402), ("BPC_CTRL", 1)]
        w += [("DG_CTRL", 1), ("DG_GAIN", 0x0F0 if vga else 0x110)]   # UQ4.8
        w += ccm_writes([[1.4, -0.3, -0.1], [-0.2, 1.4, -0.2], [-0.1, -0.4, 1.5]])
        w += gamma(3 if vga else 0) + [("CSC_CTRL", 1 if vga else 0)]
        w += [("GTM_KEY", 0xBB), ("GTM_LWHITE", 0x0180), ("GTM_ROI_LOG2", 11), ("GTM_CTRL", 1)]
        w += [("NR_2D_CTRL", 1)]
        # EE tables at unity, written explicitly: they are undefined after reset
        # (MAS §6.5.2), so a profile that enables EE must load them.
        for bank, n in ((0, 64), (1, 64), (2, 32), (3, 32)):
            w += [("EE_LUT_CTRL", bank << 1), ("EE_LUT_ADDR", 0)] + [("EE_LUT_WDATA", 0x8000)] * n
        # v1.07: confirm the resident (tables and scalars) with EE disabled (MAS §9.9.1).
        w += [("EE_ALPHA", 0x6000), ("EE_BETA", 0x4000), ("EE_CLAMP", (40 << 8) | 30), ("EE_LUT_CTRL", 1 << 8),
              ("EE_CTRL", 1)]
        w += [("CNF_CHROMA_TH", 30), ("CNF_LUMA_TH", 20), ("CNF_CTRL", 0x5)]
        w += resizer(14 if vga else 5)
        if vga:
            w += aec(16, 12, 168, 128, mn=64, mx=3968, ue=400, oe=3600) + awb(64, 32, 32, 3900)
        else:
            # ISP-UPD-003/004: default 16x16 AWB and AEC grids on the active
            # image, AEC thresholds UE 272 / OE 3744, clip 0..4095.
            w += aec(*aec_grid(aw, ah)) + awb(*awb_grid(aw, ah), 16, 4000)
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
              ("EE_LUT_CTRL", 1 << 8), ("EE_CTRL", 1)]
        w += resizer(7)
        # A legal v1.07 grid: (NX - 1) * ZW < W <= NX * ZW on both axes (MAS
        # v1.07 §6.21.3.1); AWB NX <= min(64, W/2), NY <= min(32, H/2).
        nx, ny = 8, 6
        zw, zh = -(-aw // nx), -(-ah // ny)
        w += aec(nx, ny, zw, zh, ue=256, oe=3840) + awb(16, 8, 16, 4000) + [("AF_CTRL", 1)] + ids
    if tune:
        w += tune_writes(cnf=not any(n == "CNF_CTRL" for n, _ in w))
    note = "grey world means after BLC: R %.1f G %.1f B %.1f" % means
    return w, note


def format_profile(writes, syntax="fields"):
    """Profile text of a write list, one line per write (csrw_codec.format_write:
    a register with several fields as REGISTER f=V ...; the stream is unchanged).
    From TUNE_FIRST on, TUNE_NOTES comments precede a register's first write."""
    import csrw_codec
    cmap = csrw_codec.CsrMap()
    names = [n for n, _ in writes]
    start = names.index(TUNE_FIRST) if TUNE_FIRST in names else len(names)
    out, noted = [], set()
    for i, (n, v) in enumerate(writes):
        if i >= start and n in TUNE_NOTES and n not in noted:
            noted.add(n)
            out += ["# " + c for c in TUNE_NOTES[n]]
        out.append(csrw_codec.format_write(cmap.regs[n], v, syntax))
    return "".join(line + "\n" for line in out)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--input", required=True)
    ap.add_argument("--width", type=int, required=True)
    ap.add_argument("--height", type=int, required=True)
    ap.add_argument("--bayer", required=True, choices=sorted(BAYER))
    ap.add_argument("--preset", default="basic", choices=PRESETS)
    ap.add_argument("--pedestal", type=int, default=200)
    ap.add_argument("--out", required=True)
    ap.add_argument("--syntax", default="fields", choices=("fields", "legacy"),
                    help="profile text: field names (ISP-UPD-008, csrw_codec) or whole-register values; the "
                         "writes are the same")
    ap.add_argument("--no-tune", action="store_true",
                    help="omit the TUNE block (TUNE extension controls at their reset values, ISP-UPD-009)")
    ap.add_argument("--depth", default="raw12", choices=("raw12", "raw10"),
                    help="native depth of the right-aligned container (COMMON_INPUT_FORMAT, MAS v1.07 §6.6.3.2)")
    a = ap.parse_args()
    words = np.fromfile(a.input, dtype="<u2").astype(np.int64)
    # The profile is computed in the internal RAW12 domain: normalize exactly once.
    raw = ((words & 0x3FF) << 2) if a.depth == "raw10" else words & 0xFFF
    writes, note = profile_writes(raw, a.width, a.height, a.bayer, a.preset, a.pedestal, tune=not a.no_tune)
    if a.depth == "raw10":
        writes = [("COMMON_INPUT_FORMAT", 1)] + writes  # RAW12 is the reset value
        note += "; input RAW10 right-aligned (DEPTH 1)"
    with open(a.out, "w", encoding="utf-8") as f:
        f.write("# test profile '%s' (not a calibration), tools/make_profile.py\n# %s\n" % (a.preset, note))
        f.write(format_profile(writes, a.syntax))
    print("wrote %s (%s, %d writes): %s" % (a.out, a.preset, len(writes), note))


if __name__ == "__main__":
    main()
