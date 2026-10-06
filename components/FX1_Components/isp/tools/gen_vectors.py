#!/usr/bin/env python3
"""Generate the committed FX1 ISP pipeline test vectors from the Python reference.

Each vector directory under tests/data/vectors/<name>/ holds:
  input.bin        LE uint16 containers, width x height (HAS Table 6-88 layout)
  profile.csrw     the CSR writes to replay (reference/fx1_isp_ref/regs.py format)
  expected_y.bin   NV12 luma, out_w x out_h bytes
  expected_uv.bin  NV12 interleaved chroma, out_w x out_h/2 bytes
  meta.txt         "in_w in_h out_w out_h frames"
  frames.csrw      optional: "<k> <REGISTER> <value>" writes issued before the
                   SOF of frame k (k >= 1), between frames
  expected_stats.txt  "<REGISTER> <value>" expected reads, in order, after the
                   last frame; "SET <REGISTER> <value>" lines are writes (readout
                   selectors) issued before the reads that follow
plus tests/data/vectors/resizer_plans.txt, a table of Resizer plans (DEC-23).

The expected output comes only from the Python reference, never from the C++
model. Run with --check to verify the committed files are up to date.
"""

import argparse
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "reference"))
from fx1_isp_ref import blocks, regs as regs_mod  # noqa: E402
from fx1_isp_ref.stats import readout_script as stats_readout  # noqa: E402
sys.path.insert(0, HERE)
import make_profile  # noqa: E402

OUT = os.path.join(HERE, "..", "tests", "data", "vectors")


def gamma_lut(preset):
    """Software-built curves of CSR #192 / HAS p94-95 (not hardware)."""
    def f(v):
        if preset == 0:
            return 4.5 * v if v < 0.018 else 1.099 * v ** 0.45 - 0.099
        if preset == 7:
            return v
        return v ** (1.0 / (1.8 + 0.2 * (preset - 1)))
    return [min(4095, max(0, math.floor(4095 * f(i / 255) + 0.5))) for i in range(256)]


def profile_gamma(preset):
    writes = [("GAMMA_LUT_ADDR", 0)]
    writes += [("GAMMA_LUT_DATA", v) for v in gamma_lut(preset)]
    return writes


def q39(v):
    return int(round(v * 512)) & 0xFFF


def ccm_writes(m, off=(0, 0, 0)):
    names = ["CRR", "CRG", "CRB", "CGR", "CGG", "CGB", "CBR", "CBG", "CBB"]
    w = [("CCM_" + n, q39(m[i // 3][i % 3])) for i, n in enumerate(names)]
    w += [("CCM_OFS_" + c, off[i] & 0xFFF) for i, c in enumerate("RGB")]
    return w


def lsc_load(nx, ny, dest, gain_fn):
    """LSC_LOAD sequence (CSR #94-100): begin, 4*nx*ny coefficients R,Gr,Gb,B per node, x then y, validate."""
    w = [("LSC_MESH_NODES", nx | (ny << 8)), ("LSC_LOAD_CTRL", dest | 0x100)]
    for j in range(ny):
        for i in range(nx):
            for c in range(4):
                w.append(("LSC_COEF_DATA", gain_fn(i, j, c)))
    w.append(("LSC_LOAD_CTRL", dest | 0x200))
    return w


def vignette(nx, ny):
    """Radial gain 1.0 at the centre up to about 2.2 at the corners, per-channel tilt, UQ3.18."""
    def f(i, j, c):
        r2 = ((i / (nx - 1) - 0.5) ** 2 + (j / (ny - 1) - 0.5) ** 2) * 2
        return int(round((1.0 + 1.2 * r2 + 0.05 * c) * (1 << 18)))
    return f


def hot_pixels(w, h, rng):
    img = pattern_edges(w, h, rng)
    for _ in range(40):
        y, x = rng.integers(0, h), rng.integers(0, w)
        img[y, x] = 4095 if rng.integers(0, 2) else 0
    img[0, 0], img[h - 1, w - 1], img[1, 1] = 4095, 0, 4095    # borders and the reflect-101 self-fold
    return img


def noisy(w, h, rng):
    return np.clip(pattern_edges(w, h, rng) + rng.normal(0, 90, size=(h, w)).astype(np.int64), 0, 4095)


def ee_tables():
    w = []
    for bank, n in ((0, 64), (1, 64), (2, 32), (3, 32)):
        w += [("EE_LUT_CTRL", bank << 1), ("EE_LUT_ADDR", 0)]
        w += [("EE_LUT_WDATA", 0x4000 + ((i * 2749 + bank * 811) % 0x5000)) for i in range(n)]
    return w


def pattern_edges(w, h, rng):
    """Flat areas, steps in four directions, a ramp and noise: exercises every demosaic branch."""
    y, x = np.mgrid[0:h, 0:w]
    img = np.full((h, w), 800, dtype=np.int64)
    img[:, w // 4: w // 2] = 3200
    img[h // 2:, :] += 600
    img[(x + y) % 16 < 8] += 300
    img += (x * 4095 // max(w - 1, 1)) // 4
    img += rng.integers(-40, 41, size=(h, w))
    return np.clip(img, 0, 4095)


def aec_cfg(nx, ny, zw, zh, mn=0, mx=4095, ue=256, oe=3840, en=1, commit=True):
    return [("AEC_ZONE_CFG", nx | (ny << 8)), ("AEC_ZONE_SIZE", zw | (zh << 16)),
            ("AEC_SAMPLE_CLIP", mn | (mx << 16)), ("AEC_THRESH", ue | (oe << 16)),
            ("AEC_CTRL", en | (2 if commit else 0))]


def awb_cfg(nx, ny, under=0, sat=4095, en=1):
    return [("AWB_UNDEREXPOSED_LIMIT", under), ("AWB_SATURATION_LIMIT", sat),
            ("AWB_CTRL", en | (nx << 1) | (ny << 8))]


def ctx_ids(base):
    return [("AEC_CONTEXT_ID", base + 1), ("AWB_CONTEXT_ID", base + 2), ("AF_CONTEXT_ID", base + 3)]


def flat_channels(values):
    def make(w, h, rng):
        y, x = np.mgrid[0:h, 0:w]
        return np.choose(((y & 1) << 1) | (x & 1), values)
    return make


def af_scene(w, h, rng):
    """Vertical step, horizontal ramp and corner impulses in luma (after demosaic/CSC)."""
    y, x = np.mgrid[0:h, 0:w]
    img = np.where(x >= w // 3, 3000, 600) + (y * 1500 // max(h - 1, 1))
    img[:4, :4] = 4095
    img[-4:, -4:] = 0
    return np.clip(img + rng.integers(-20, 21, size=(h, w)), 0, 4095)


def blc_profiles(sel, scale=True):
    """All four BLC gain profiles programmed with distinct pedestals, trims and scales; `sel` selected."""
    w = []
    for g in range(4):
        ped = 120 + 60 * g
        w.append(("BLC_OFS_G%d_DFT" % g, ped))
        for i, c in enumerate(("R", "GR", "GB", "B")):
            w.append(("BLC_OFS_G%d_%s" % (g, c), 3 * g + 7 * i + 1))
        w.append(("BLC_SCALE_G%d" % g, round(4095 / (4095 - ped) * 65536) + 97 * g))
    return w + [("BLC_GAIN_SEL", sel | (0x100 if scale else 0)), ("BLC_CTRL", 1)]


def full_chain(bayer):
    """Every block on (make_profile preset 'full'), profile computed from the frame."""
    return lambda raw, w, h: make_profile.profile_writes(raw & 0xFFF, w, h, bayer, "full")[0]


VECTORS = [
    # name, width, height, input builder, CSR writes
    ("mandatory_rggb_64x32", 64, 32,
     lambda w, h, rng: rng.integers(0, 4096, size=(h, w)),
     [("COMMON_BAYER", 0)]),
    ("full_bggr_66x34", 66, 34, pattern_edges,
     [("COMMON_BAYER", 3), ("BLC_CTRL", 1), ("BLC_GAIN_SEL", 0x101),
      ("BLC_OFS_G1_DFT", 200), ("BLC_OFS_G1_R", 3), ("BLC_OFS_G1_GR", 1), ("BLC_OFS_G1_GB", 2), ("BLC_OFS_G1_B", 4),
      ("BLC_SCALE_G1", round(4095 / (4095 - 200) * 65536)),
      ("WB_CTRL", 1), ("WB_GAIN_R", 0x1C0), ("WB_GAIN_G", 0x100), ("WB_GAIN_B", 0x160),
      ("DG_CTRL", 1), ("DG_GAIN", 0x120)]
     + ccm_writes([[1.6, -0.4, -0.2], [-0.3, 1.5, -0.2], [-0.1, -0.5, 1.6]], (5, -3, 0))
     + [("CCM_CTRL", 0x5)]
     + profile_gamma(0) + [("GAMMA_CTRL", 1), ("CSC_CTRL", 1)]),
    ("grbg_gamma22_128x72", 128, 72, pattern_edges,
     [("COMMON_BAYER", 1), ("WB_CTRL", 1), ("WB_GAIN_R", 0x200), ("WB_GAIN_B", 0x080)]
     + profile_gamma(3) + [("GAMMA_CTRL", 1)]),
    ("gbrg_ccm_gate_64x36", 64, 36, lambda w, h, rng: rng.integers(0, 4096, size=(h, w)),
     # CCM written without `updated`: the committed identity set applies (DEC-24).
     [("COMMON_BAYER", 2)] + ccm_writes([[0.5, 0, 0], [0, 0.5, 0], [0, 0, 0.5]]) + [("CCM_CTRL", 1)]),
    ("lsc_bggr_96x64", 96, 64, pattern_edges,
     [("COMMON_BAYER", 3)] + lsc_load(9, 7, 1, vignette(9, 7)) +
     [("LSC_PROFILE_SEL", 1), ("LSC_STRENGTH", 0xC000), ("LSC_CTRL", 1)]),
    ("lsc_saturate_64x32", 64, 32, lambda w, h, rng: np.full((h, w), 3000) + rng.integers(0, 50, size=(h, w)),
     lsc_load(2, 2, 0, lambda i, j, c: 0x80000 + (c << 12)) + [("LSC_STRENGTH", 0x10000), ("LSC_CTRL", 1)]),
    ("lsc_strength_over_unity_64x32", 64, 32, pattern_edges,   # ALG-LSC-02: negative Ge possible
     lsc_load(3, 3, 2, lambda i, j, c: (i * 3 + j) << 15) + [("LSC_PROFILE_SEL", 2), ("LSC_STRENGTH", 0x1FFFF),
                                                             ("LSC_CTRL", 1)]),
    ("bpc_hot_rggb_80x48", 80, 48, hot_pixels,
     [("BPC_MODE", 0), ("BPC_THRESH", (77 << 16) | 402), ("BPC_CTRL", 1)]),
    ("bpc_dynamic_requested_64x32", 64, 32, hot_pixels,   # DEC-28: static correction, counters 0
     [("BPC_THRESH", (0 << 16) | 100), ("BPC_CTRL", 1)]),
    ("gtm_auto_3frames_96x64", 96, 64, pattern_edges,       # frame 1 pass-through, then HW curves
     [("GTM_KEY", 0xBB), ("GTM_LWHITE", 0x0180), ("GTM_ROI_LOG2", 5), ("GTM_CTRL", 1)], 3),
    ("gtm_manual_lut_64x32", 64, 32, pattern_edges,
     [("GTM_CTRL", 2), ("GTM_LUT_ADDR", 0)] + [("GTM_LUT_DATA", 0x100 + ((i * 37) % 97) - 40) for i in range(65)]
     + [("GTM_CTRL", 3)]),
    ("nr2d_3frames_66x34", 66, 34, noisy, [("NR_2D_CTRL", 1)], 3),
    ("nr2d_bggr_68x40", 68, 40, noisy, [("COMMON_BAYER", 3), ("NR_2D_CTRL", 1)], 2),   # odd 2DNR sizes are unreachable (ALG-2DNR-11)
    ("ee_features_96x64", 96, 64, pattern_edges,
     [("EE_ALPHA", 0x7000), ("EE_BETA", 0x5000), ("EE_CLAMP", (40 << 8) | 30), ("EE_FEATURE_EN", 7),
      ("EE_RADIAL_CENTER", (63 << 16) | 95), ("EE_RADIAL_R2_TH0", 900), ("EE_RADIAL_R2_TH1", 4000),
      ("EE_RADIAL_R2_TH2", 9000), ("EE_RADIAL_GAIN_01", (0x9000 << 16) | 0x8000), ("EE_RADIAL_GAIN_23", (0xFFFF << 16) | 0xA000)]
     + ee_tables() + [("EE_CTRL", 1)]),
    ("cnf_64x48", 64, 48, noisy,
     [("CNF_CHROMA_TH", 40), ("CNF_LUMA_TH", 25), ("CNF_CTRL", 0x5)]),
    ("full_chain_bggr_2frames_130x74", 130, 74, noisy,
     [("COMMON_BAYER", 3), ("BLC_CTRL", 1), ("WB_CTRL", 1), ("WB_GAIN_R", 0x1A0), ("WB_GAIN_B", 0x170),
      ("BPC_MODE", 0), ("BPC_CTRL", 1)] + profile_gamma(0) +
     [("GAMMA_CTRL", 1), ("GTM_KEY", 0xBB), ("GTM_CTRL", 1), ("NR_2D_CTRL", 1), ("EE_CTRL", 1),
      ("CNF_CHROMA_TH", 30), ("CNF_LUMA_TH", 20), ("CNF_CTRL", 0x5)], 2),
    ("resize_qhd_1000x600", 1000, 600, pattern_edges,
     [("RESIZER_CTRL", 0x1 | 0x4 | (7 << 3))]),     # qHD 960x540: k = 0, bilinear 1.04 x 1.11
    ("resize_360p_1280x720", 1280, 720, pattern_edges,
     [("RESIZER_CTRL", 0x1 | 0x4 | (8 << 3))]),     # 640x360: k = 1, then a 1:1 bilinear
    ("resize_vga_bggr_700x520", 700, 520, pattern_edges,
     [("COMMON_BAYER", 3), ("RESIZER_CTRL", 0x1 | 0x4 | (14 << 3))]),   # 698x518 -> 640x480, k = 0
    # ---- statistics (ALG-F §5 ids in the comments) ----
    # A01/A03/A05/B01 flat per-channel field, clip and threshold edges; 16x12 grid of 3x3 zones,
    # so the last column and row absorb the remainder (CSR #302, ALG-AEC-01).
    ("stats_flat_channels_64x48", 64, 48, flat_channels([100, 1000, 3300, 4000]),
     aec_cfg(16, 12, 3, 3, mn=100, mx=3300, ue=1000, oe=3299) + awb_cfg(4, 3, under=99, sat=4095)
     + ctx_ids(0x100) + [("AF_CTRL", 1)]),
    # A06-like remainder zones, AWB floor boundaries, AF ceil boundaries on an odd grid, 2 frames.
    ("stats_scene_bggr_2frames_134x98", 134, 98, pattern_edges,
     # WB/DG and GTM enabled so that the AEC (pre-WB) and AF (pre-GTM) tap points matter.
     [("COMMON_BAYER", 3), ("BLC_CTRL", 1), ("BPC_MODE", 0), ("BPC_CTRL", 1), ("WB_CTRL", 1),
      ("WB_GAIN_R", 0x1A0), ("WB_GAIN_B", 0x170), ("DG_CTRL", 1), ("DG_GAIN", 0x110),
      ("GTM_KEY", 0xBB), ("GTM_ROI_LOG2", 5), ("GTM_CTRL", 1)]
     + aec_cfg(7, 5, 20, 21, mn=300, mx=3900, ue=900, oe=3000) + awb_cfg(5, 5, under=700, sat=3800)
     + ctx_ids(0x200) + [("AF_CTRL", 1)], 2),
    # A08 CSR reset AEC grid (32x24 of 120x90) on a small frame; B03 AWB reset grid 0x0: global only;
    # F02-F04 AF edges/ramp/corners with W not a multiple of 4.
    ("stats_reset_grids_af_edges_130x90", 130, 90, af_scene,
     [("AEC_CTRL", 3), ("AWB_CTRL", 1), ("AF_CTRL", 1)] + ctx_ids(0x300)),
    # A09 illegal AEC grid (NX = 0): global + histogram only; B03 AWB 65x33: global only; AF disabled.
    ("stats_illegal_grids_64x32", 64, 32, pattern_edges,
     aec_cfg(0, 4, 8, 8) + awb_cfg(65, 33)),
    # A07 oversized last zone: NX = NY = 1 of 1x1 over 512x512, per-channel zone count 65536 wraps to 0.
    ("stats_zone_wrap_512x512", 512, 512, flat_channels([4095, 2048, 1024, 1]),
     aec_cfg(1, 1, 1, 1) + awb_cfg(64, 32)),
    # A10/B07/F07/S01/S02: frame 1 commits; frame 2 new config without commit (AEC keeps its set,
    # AWB takes it at SOF), AF disabled (frozen); frame 3 commits AEC en=0 (frozen from frame 3).
    ("stats_commit_sequence_4frames_64x48", 64, 48, pattern_edges,
     aec_cfg(8, 6, 8, 8) + awb_cfg(4, 4) + ctx_ids(0x400) + [("AF_CTRL", 1)], 4,
     [(1, "AEC_ZONE_CFG", 2 | (2 << 8)), (1, "AEC_CONTEXT_ID", 0x4101), (1, "AWB_CTRL", 1 | (2 << 1) | (2 << 8)),
      (1, "AWB_CONTEXT_ID", 0x4102), (1, "AF_CTRL", 0),
      (2, "AEC_ZONE_CFG", 4 | (3 << 8)), (2, "AEC_ZONE_SIZE", 16 | (16 << 16)), (2, "AEC_CTRL", 3),
      (2, "AF_CTRL", 1), (2, "AF_CONTEXT_ID", 0x4203),
      (3, "AEC_CTRL", 2), (3, "AEC_CONTEXT_ID", 0x4301)]),
    # ---- BLC gain profiles: the selected one, with every other profile holding decoy values ----
    ("blc_profile0_trims_noscale_64x32", 64, 32, pattern_edges, blc_profiles(0, scale=False)),
    ("blc_profile2_bggr_64x32", 64, 32, pattern_edges, [("COMMON_BAYER", 3)] + blc_profiles(2)),
    ("blc_profile3_grbg_64x32", 64, 32, pattern_edges, [("COMMON_BAYER", 1)] + blc_profiles(3)),
    # ---- boundary geometry, every block enabled (G-E2E; 3840x2160 itself: tools/gen_4k_vectors.py) ----
    # Smallest frame the pipeline supports (ALG-DMS-03: 4x4); the 17x9 LSC mesh, 32x24 AEC and 32x16
    # AWB grids and the FHD Resizer mode are all larger than the frame.
    ("edge_min_4x4_full_rggb", 4, 4, pattern_edges, full_chain("RGGB"), 2),
    # BGGR crop of both axes down to the minimum.
    ("edge_crop_6x6_full_bggr", 6, 6, noisy, full_chain("BGGR"), 2),
    # Maximum width, minimum height: 3840-sample lines, FHD target not applicable.
    ("edge_strip_3840x4_full_rggb", 3840, 4, pattern_edges, full_chain("RGGB"), 2),
    # Minimum width (after the GRBG crop), maximum height.
    ("edge_strip_6x2160_full_grbg", 6, 2160, noisy, full_chain("GRBG"), 2),
]


def build(name, w, h, make, writes, check, frames=1, between=()):
    rng = np.random.default_rng(sum(map(ord, name)))  # deterministic per vector
    raw = np.asarray(make(w, h, rng), dtype=np.int64).astype(np.uint16)
    r = regs_mod.Registers()
    if callable(writes):                        # profile derived from the frame (grey world)
        writes = writes(raw.astype(np.int64), w, h)
    writes = [("COMMON_FRAME_WIDTH", w), ("COMMON_FRAME_HEIGHT", h)] + list(writes)
    for n, v in writes:
        r.write(n, v)
    for k in range(frames):                     # the same input repeated; expected = last frame
        for fk, n, v in between:
            if fk == k:
                r.write(n, v)
        stats = {}
        y, uv = blocks.run_frame(raw, r, stats)
    stats = {k: v for k, v in stats.items() if not k.startswith("_")}
    readout = stats_readout(r) if name.startswith(("stats_", "edge_")) else []
    files = {
        "input.bin": raw.astype("<u2").tobytes(),
        "profile.csrw": ("# generated by tools/gen_vectors.py\n" +
                         "".join("%s 0x%X\n" % (n, v) for n, v in writes)).encode(),
        "expected_y.bin": y.tobytes(),
        "expected_uv.bin": uv.tobytes(),
        "meta.txt": ("%d %d %d %d %d\n" % (w, h, y.shape[1], y.shape[0], frames)).encode(),
        "expected_stats.txt": ("".join("%s %d\n" % kv for kv in sorted(stats.items())) + "".join(readout)).encode(),
    }
    if between:
        files["frames.csrw"] = ("# generated by tools/gen_vectors.py: <frame> <register> <value>\n" +
                                "".join("%d %s 0x%X\n" % b for b in between)).encode()
    return emit(os.path.join(OUT, name), files, check)


def resizer_plan_table():
    lines = ["# w h scale -> pass k out_w out_h (DEC-23); generated by tools/gen_vectors.py"]
    sizes = [(3840, 2160), (3840, 2158), (3838, 2160), (2686, 1518), (2592, 1944), (1920, 1080),
             (1282, 722), (641, 361), (2882, 1622), (1024, 768)]
    for w, h in sizes:
        for scale in range(16):
            p = blocks.resizer_plan(1, scale, w, h)
            if p is None:
                lines.append("%d %d %d 1 0 %d %d" % (w, h, scale, w, h))
            else:
                lines.append("%d %d %d 0 %d %d %d" % (w, h, scale, p[0], p[1], p[2]))
    return ("\n".join(lines) + "\n").encode()


def emit(d, files, check):
    stale = []
    os.makedirs(d, exist_ok=True)
    for fn, data in files.items():
        path = os.path.join(d, fn)
        if check:
            if not os.path.exists(path) or open(path, "rb").read() != data:
                stale.append(path)
        else:
            with open(path, "wb") as f:
                f.write(data)
    return stale


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    stale = []
    for v in VECTORS:
        stale += build(*v[:5], a.check, *v[5:])
    stale += emit(OUT, {"resizer_plans.txt": resizer_plan_table()}, a.check)
    if stale:
        sys.exit("gen_vectors.py: out of date:\n  " + "\n  ".join(stale))
    print("%s %d vectors + resizer plan table" % ("checked" if a.check else "wrote", len(VECTORS)))


if __name__ == "__main__":
    main()
