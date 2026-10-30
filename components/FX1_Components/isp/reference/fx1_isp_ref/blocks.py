"""Frame-based integer reference of the FX1 ISP blocks.

Every function takes and returns numpy int64 arrays (Bayer: [H, W]; colour:
[H, W, 3] in R,G,B or Y,Cb,Cr order) and reads its configuration from a
`Registers` view. Arithmetic follows the HAS numeric contracts; the ALG-* and
DEC-* identifiers name the review item a choice comes from
(plan/alg/ALG_*.md, docs/ISP_DECISIONS_AND_DISCREPANCIES.md).
"""

import numpy as np

from . import geometry

I64 = np.int64


def _floor_shift(x, n):
    return np.right_shift(x, n)  # numpy >> on signed ints is arithmetic (floor)


# ---------------------------------------------------------------- Input Formatter
def normalize_input(raw, depth):
    """Input-boundary adapter (MAS v1.07 §6.6.3.2, Table 6-18): right-aligned RAW12 passes
    unchanged, RAW10 is shifted left two exactly once. The container bits above the native
    depth are not part of the sample (V107-A8). DEPTH 2/3 never starts a frame (V107-A10)."""
    raw = np.asarray(raw).astype(I64)
    return ((raw & 0x3FF) << 2) if depth == 1 else raw & 0x0FFF


def input_formatter(raw, regs):
    """Normalize the native depth, crop to RGGB phase, round the size down to even
    (MAS v1.07 §6.6.3.2-6.6.3.5)."""
    pattern = regs.f("COMMON_BAYER", "pattern")
    sh, sv = pattern & 1, (pattern >> 1) & 1
    h, w = raw.shape
    wo, ho = (w - sh) // 2 * 2, (h - sv) // 2 * 2
    return normalize_input(raw[sv:sv + ho, sh:sh + wo], regs.f("COMMON_INPUT_FORMAT", "DEPTH"))


# ---------------------------------------------------------------- BLC (HAS §6.7)
def blc(p, regs, en_isp=True):
    if not (en_isp and regs.f("BLC_CTRL", "en")):
        return p.copy()
    g = regs.f("BLC_GAIN_SEL", "gain_level")
    rse = regs.f("BLC_GAIN_SEL", "range_scale_en")
    dft = regs.f("BLC_OFS_G%d_DFT" % g, "ofs_g%d_dft" % g)
    trims = [regs.f("BLC_OFS_G%d_%s" % (g, c), "ofs_g%d_%s" % (g, c.lower())) for c in ("R", "GR", "GB", "B")]
    k = regs.f("BLC_SCALE_G%d" % g, "scale_g%d" % g)
    ch = _bayer_channel(p.shape)
    ofs = np.minimum(dft + np.array(trims, dtype=I64)[ch], 4095)   # CSR #38 saturated sum
    d = np.maximum(p - ofs, 0)
    if not rse:
        return d
    return np.minimum((d * k + (1 << 15)) >> 16, 4095)             # ALG-BLC-01: round half up


def _bayer_channel(shape):
    h, w = shape
    y = np.arange(h, dtype=I64)[:, None]
    x = np.arange(w, dtype=I64)[None, :]
    return 2 * (y & 1) + (x & 1)   # 0 R, 1 Gr, 2 Gb, 3 B (RGGB after the Input Formatter)


# ---------------------------------------------------------------- LSC (HAS §6.8, Eq 2, 9-12)
def _lsc_axis(d, n, sectors=None):
    """Uniform DDA nodes (Eq 2), or with sectors (MAS v1.07 §6.8.3.9) node k is the
    sum of the first k programmed intervals; the cell and weight rules are the same
    (V107-A18)."""
    s, c = d - 1, n - 1
    if sectors is not None:
        nodes = [0]
        for v in sectors[:c]:
            nodes.append(nodes[-1] + v)
    else:
        nodes = [(k * s) // c for k in range(c + 1)]
    cell = np.zeros(d, dtype=I64)
    w = np.zeros(d, dtype=I64)
    for x in range(d):
        k = max(k for k in range(c) if nodes[k] <= x)
        span = nodes[k + 1] - nodes[k]
        cell[x] = k
        w[x] = ((x - nodes[k]) * 65536 + span // 2) // span
    return cell, w


def lsc(p, regs, stats):
    nx, ny = regs.f("LSC_MESH_NODES", "mesh_nx"), regs.f("LSC_MESH_NODES", "mesh_ny")
    mesh = regs.lsc_mesh[regs.lsc_active]
    h, w = p.shape
    geometry_ok = 2 <= nx <= 32 and 2 <= ny <= 32 and nx <= w and ny <= h   # MAS v1.07 §6.8.4 (V107-A2)
    stats.update({"LSC_STAT_OVF_CNT_" + c: 0 for c in ("R", "GR", "GB", "B")})
    if not (regs.f("LSC_CTRL", "en") and mesh is not None and geometry_ok):
        return p.copy()
    g = np.array(mesh, dtype=I64).reshape(ny, nx, 4)
    sec = regs.ext("TUNE_LSC_CTRL", "SECTOR_EN")
    ci, wx = _lsc_axis(w, nx, regs.lsc_sector[0] if sec else None)
    cj, wy = _lsc_axis(h, ny, regs.lsc_sector[1] if sec else None)
    ch = _bayer_channel(p.shape)
    I, J = ci[None, :], cj[:, None]
    WX, WY = wx[None, :], wy[:, None]
    rnd16 = lambda v: (v + (1 << 15)) >> 16
    gt = rnd16(((1 << 16) - WX) * g[J, I, ch] + WX * g[J, I + 1, ch])
    gb = rnd16(((1 << 16) - WX) * g[J + 1, I, ch] + WX * g[J + 1, I + 1, ch])
    gm = rnd16(((1 << 16) - WY) * gt + WY * gb)
    prod = regs.f("LSC_STRENGTH", "strength") * (gm - (1 << 18))
    ge = (1 << 18) + np.sign(prod) * ((np.abs(prod) + (1 << 15)) >> 16)   # srnd16
    pr = (p * ge + (1 << 17)) >> 18
    for i, c in enumerate(("R", "GR", "GB", "B")):
        stats["LSC_STAT_OVF_CNT_" + c] = int(np.count_nonzero((pr > 4095) & (ch == i)))
    return np.clip(pr, 0, 4095)


# ---------------------------------------------------------------- BPC (MAS §6.9)
def bpc(p, regs, stats):
    """Spatial detection; DYNAMIC_DET_EN has no effect (DEC-28 superseded).
    The counters clear at SOF and count during an enabled frame, 14-bit
    saturating; a disabled frame leaves them at 0."""
    if not regs.f("BPC_CTRL", "en"):
        stats["BPC_NUM_CANDIDATES"] = stats["BPC_NUM_DEFECTIVE"] = 0
        return p.copy()
    floor = regs.f("BPC_THRESH", "thresh_floor")
    k = regs.f("BPC_THRESH", "thresh_k")
    h, w = p.shape
    if regs.ext("TUNE_BPC_CTRL", "COLOR_THRESH_EN"):
        # Independent G and R/B K (UQ4.8) and floor (MAS v1.07 §6.9.3.6).
        green = (_bayer_channel(p.shape) == 1) | (_bayer_channel(p.shape) == 2)
        k = np.where(green, regs.ext("TUNE_BPC_G_THRESH", "K"), regs.ext("TUNE_BPC_RB_THRESH", "K"))
        floor = np.where(green, regs.ext("TUNE_BPC_G_THRESH", "FLOOR"), regs.ext("TUNE_BPC_RB_THRESH", "FLOOR"))
    P = _reflect_pad(p.astype(I64), 2)
    nb = np.stack([P[2 + dy:2 + dy + h, 2 + dx:2 + dx + w]
                   for dy in (-2, 0, 2) for dx in (-2, 0, 2) if (dx, dy) != (0, 0)])
    avg8 = nb.sum(axis=0) >> 3
    t = np.maximum(floor, (k * (nb.max(axis=0) - nb.min(axis=0)) + 128) >> 8)
    dev = np.abs(p - avg8)
    defect = dev > t
    if regs.ext("TUNE_BPC_CTRL", "FACTORY_EN"):
        # Listed sites are forced to the same correction and counted (§6.9.3.6).
        listed = np.zeros_like(defect)
        for v in regs.bpc_list[:regs.ext("TUNE_BPC_COUNT", "VALUE")]:
            x, y = v & 0xFFFF, v >> 16
            if x < w and y < h:
                listed[y, x] = True
        defect = defect | listed
    stats["BPC_NUM_CANDIDATES"] = min(int(np.count_nonzero(dev > floor)), 0x3FFF)
    stats["BPC_NUM_DEFECTIVE"] = min(int(np.count_nonzero(defect)), 0x3FFF)
    return np.where(defect, avg8, p)


# ---------------------------------------------------------------- WB / DG (HAS §6.10-6.11)
def wb(p, regs, en_isp=True):
    if not (en_isp and regs.f("WB_CTRL", "en")):
        return p.copy()
    # Gb uses TUNE_WB_GB in profile mode, WB_GAIN_G in legacy mode (MAS v1.07 §6.10.3.6).
    gb = regs.ext("TUNE_WB_GB", "VALUE") if regs.profile_mode() else regs.f("WB_GAIN_G", "gain_g")
    gains = np.array([regs.f("WB_GAIN_R", "gain_r"), regs.f("WB_GAIN_G", "gain_g"),
                      gb, regs.f("WB_GAIN_B", "gain_b")], dtype=I64)
    return np.minimum((p * gains[_bayer_channel(p.shape)]) >> 8, 4095)   # floor, sat 4095


def dg(p, regs, en_isp=True):
    if not (en_isp and regs.f("DG_CTRL", "en")):
        return p.copy()
    return np.minimum((p * regs.f("DG_GAIN", "gain")) >> 8, 4095)


# ---------------------------------------------------------------- Demosaic (HAS §6.12)
def _reflect_pad(a, r):
    return np.pad(a, [(r, r), (r, r)] + [(0, 0)] * (a.ndim - 2), mode="reflect")  # reflect-101


def demosaic(p, margin_green=0, margin_rb=0):
    """Adams-Hamilton, working scale 64, one final rounding (HAS §6.12.6-6.12.7, p82-85)."""
    h, w = p.shape
    if h < 4 or w < 4:
        raise ValueError("demosaic needs W, H >= 4 (ALG-DMS-03)")
    P = _reflect_pad(p.astype(I64), 2)
    c = lambda dy, dx: P[2 + dy:2 + dy + h, 2 + dx:2 + dx + w]
    c5, c1, c9, c3, c7 = c(0, 0), c(-2, 0), c(2, 0), c(0, -2), c(0, 2)
    g2, g8, g4, g6 = c(-1, 0), c(1, 0), c(0, -1), c(0, 1)
    dh = np.abs(g4 - g6) + np.abs(2 * c5 - c3 - c7)
    dv = np.abs(g2 - g8) + np.abs(2 * c5 - c1 - c9)
    g_v = 32 * (g2 + g8) + 16 * (2 * c5 - c1 - c9)
    g_h = 32 * (g4 + g6) + 16 * (2 * c5 - c3 - c7)
    g_t = 16 * (g2 + g8 + g4 + g6) + 8 * (4 * c5 - c1 - c9 - c3 - c7)
    # Decision margins (MAS v1.07 §6.12.3.7): equal branch within the margin.
    g64 = np.where(np.abs(dh - dv) <= margin_green, g_t, np.where(dh > dv, g_v, np.where(dh < dv, g_h, g_t)))
    ch = _bayer_channel(p.shape)
    is_g = (ch == 1) | (ch == 2)
    g64 = np.where(is_g, 64 * p.astype(I64), g64)

    G = _reflect_pad(g64, 1)
    K = 64 * _reflect_pad(p.astype(I64), 1)
    PB = _reflect_pad(p.astype(I64), 1)
    t = lambda A, n: A[1 + (n - 1) // 3 - 1:1 + (n - 1) // 3 - 1 + h, 1 + (n - 1) % 3 - 1:1 + (n - 1) % 3 - 1 + w]
    g5 = t(G, 5)

    def col():
        num = t(K, 2) + t(K, 8) + 2 * g5 - t(G, 2) - t(G, 8)
        return _exact_div(num, 2)

    def row():
        num = t(K, 4) + t(K, 6) + 2 * g5 - t(G, 4) - t(G, 6)
        return _exact_div(num, 2)

    dn = 64 * np.abs(t(PB, 1) - t(PB, 9)) + np.abs(2 * g5 - t(G, 1) - t(G, 9))
    dp = 64 * np.abs(t(PB, 3) - t(PB, 7)) + np.abs(2 * g5 - t(G, 3) - t(G, 7))
    diag_37 = _exact_div(2 * (t(K, 3) + t(K, 7)) + 2 * g5 - t(G, 3) - t(G, 7), 4)
    diag_19 = _exact_div(2 * (t(K, 1) + t(K, 9)) + 2 * g5 - t(G, 1) - t(G, 9), 4)
    diag_t = _exact_div(2 * (t(K, 1) + t(K, 3) + t(K, 7) + t(K, 9)) + 4 * g5
                        - t(G, 1) - t(G, 3) - t(G, 7) - t(G, 9), 8)
    diag = np.where(np.abs(dn - dp) <= margin_rb, diag_t,
                    np.where(dn > dp, diag_37, np.where(dn < dp, diag_19, diag_t)))   # ALG-DMS-05: as printed

    native = 64 * p.astype(I64)
    r64 = np.select([ch == 0, ch == 1, ch == 2, ch == 3], [native, row(), col(), diag])
    b64 = np.select([ch == 0, ch == 1, ch == 2, ch == 3], [diag, col(), row(), native])

    def rnd(x):   # nearest, ties away from zero, then clamp (p85)
        r = np.where(x >= 0, (x + 32) >> 6, -((-x + 32) >> 6))
        return np.clip(r, 0, 4095)

    return np.stack([rnd(r64), rnd(g64), rnd(b64)], axis=-1)


def _exact_div(num, d):
    if np.any(num % d):
        raise AssertionError("demosaic: non-exact division (spec says every division is exact)")
    return num // d


# ---------------------------------------------------------------- CCM (HAS §6.13)
def _s12(v):
    return v - 4096 if v & 0x800 else v


def ccm(rgb, regs, en_isp=True):
    """3x3 signed Q3.9 matrix + Q3.9 offset at the accumulator scale, floor, clamp (ALG-CCM-02).
    Coefficients sampled at the accepted SOF; `updated` has no effect (U1, DEC-24 superseded)."""
    if not (en_isp and regs.f("CCM_CTRL", "en")):
        return rgb.copy()
    names = [["CRR", "CRG", "CRB"], ["CGR", "CGG", "CGB"], ["CBR", "CBG", "CBB"]]
    m = np.array([[_s12(regs.fc("CCM_" + n, "coef" + n[1:].lower())) for n in rowv] for rowv in names], dtype=I64)
    off = np.array([_s12(regs.fc("CCM_OFS_" + c, "offset_" + c.lower())) for c in "RGB"], dtype=I64)
    acc = np.einsum("hwk,ck->hwc", rgb.astype(I64), m) + off
    return np.clip(acc >> 9, 0, 4095)


# ---------------------------------------------------------------- Gamma (HAS §6.14)
def gamma(rgb, regs, en_isp=True):
    if not (en_isp and regs.f("GAMMA_CTRL", "en") and regs.gamma_valid):
        return rgb.copy()          # unconfirmed curve: pass-through (MAS v1.07 §6.14.4)
    lut = np.array(regs.gamma_lut, dtype=I64)
    return lut[rgb >> 4]          # index = top 8 bits, no interpolation (p95, CSR #194)


# ---------------------------------------------------------------- CSC (HAS §6.15, SPEC-03)
CSC_LUMA = {0: (19595, 7471), 1: (13933, 4732)}   # std: LKR, LKB (Q16)
CSC_RANGE = {  # (full_range, std): RSY, RSCB, RSCR, OY (MAS v1.07 Table 6-92)
    (1, 0): (4081, 2303, 2911, 0), (1, 1): (4081, 2199, 2591, 0),
    (0, 0): (3505, 2023, 2557, 16), (0, 1): (3505, 1932, 2276, 16),
}


def _r(n, log2d):
    """R(n, d) = floor((n + floor(d/2)) / d), signed n: ties toward +infinity (MAS v1.07 §6.15.3.3)."""
    return (n + (1 << (log2d - 1))) >> log2d


def _sext(v, bits):
    return (v ^ (1 << (bits - 1))) - (1 << (bits - 1))


def csc(rgb, regs):
    """MAS v1.07 §6.15.3.3: range-selected conversion (full range by reset, in legacy
    mode too), then the optional look (profile mode)."""
    std = regs.f("CSC_CTRL", "std")
    full = regs.ext("TUNE_CSC_CTRL", "FULL_RANGE")
    lkr, lkb = CSC_LUMA[std]
    rsy, rscb, rscr, oy = CSC_RANGE[(full, std)]
    r, g, b = (rgb[..., i].astype(I64) for i in range(3))
    l = lkr * (r - g) + (g << 16) + lkb * (b - g)
    y = np.clip(_r(rsy * l, 32) + oy, 0, 255)
    cb = np.clip(_r(rscb * ((b << 16) - l), 32) + 128, 0, 255)
    cr = np.clip(_r(rscr * ((r << 16) - l), 32) + 128, 0, 255)
    if regs.ext("TUNE_CSC_CTRL", "LOOK_EN"):
        ped = 0 if full else 16
        contrast = regs.ext("TUNE_CSC_CONTRAST", "VALUE")
        bright = _sext(regs.ext("TUNE_CSC_BRIGHTNESS", "VALUE"), 10)
        m = [_sext(regs.ext("TUNE_CSC_" + n, "VALUE"), 18) for n in ("UU", "UV", "VU", "VV")]
        du, dv = cb - 128, cr - 128
        y, cb, cr = (np.clip(ped + _r(contrast * (y - ped), 8) + bright, 0, 255),
                     np.clip(128 + _r(m[0] * du + m[1] * dv, 14), 0, 255),
                     np.clip(128 + _r(m[2] * du + m[3] * dv, 14), 0, 255))
    return np.stack([y, cb, cr], axis=-1)


# ---------------------------------------------------------------- GTM (HAS §6.16.4)
def gtm_curve(yavg, key, lwhite):
    if lwhite == 0:
        recip = 0xFFFF
    else:
        sq = lwhite * lwhite
        recip = min(((1 << 32) + sq // 2) // sq, 0xFFFF)      # ALG-GTM-01
    yavg = max(yavg, 1)
    r = [0] * 65
    for i in range(1, 65):
        yi = 4 * i
        l = min(key * yi * 256 // yavg, 0xFFFFFF)
        l2 = min((l * l) >> 16, 0xFFFFFF)
        num = l + ((l2 * recip) >> 16)
        ld = (num << 16) // ((1 << 16) + l)                    # full width (ALG-GTM-02)
        out = min((ld * 255 + (1 << 15)) >> 16, 255)
        r[i] = min((out * 256 + (yi >> 1)) // yi, 0xFFFF)
    r[0] = r[1]
    return r


def gtm(yuv, regs, stats):
    h, w = yuv.shape[:2]
    roi = regs.f("GTM_ROI_LOG2", "roi_log2")
    kx = min(roi, w.bit_length() - 1)
    ky = min(roi, h.bit_length() - 1)
    x0, y0 = (w - (1 << kx)) >> 1, (h - (1 << ky)) >> 1        # ALG-GTM-03
    total = int(yuv[y0:y0 + (1 << ky), x0:x0 + (1 << kx), 0].sum())
    stats["_gtm_yavg"] = max(total >> (kx + ky), 1)
    if regs.f("GTM_CTRL", "manual"):
        # Manual bank 0, applied only when all 65 points are complete (MAS v1.07 §6.16.4.3).
        if not (regs.f("GTM_CTRL", "en") and all(regs.gtm_manual_written)):
            return yuv.copy()
        R = np.array(regs.gtm_manual, dtype=I64)
    else:
        if not (regs.f("GTM_CTRL", "en") and regs.gtm_valid):
            return yuv.copy()
        R = np.array(regs.gtm_banks[regs.gtm_read], dtype=I64)
    y = yuv[..., 0].astype(I64)
    a, f = y >> 2, y & 3
    ratio = R[a] + ((f * (R[a + 1] - R[a])) >> 2)
    out = np.empty_like(yuv, dtype=I64)
    out[..., 0] = np.minimum((ratio * y + 128) >> 8, 255)
    for c in (1, 2):
        out[..., c] = np.clip(128 + ((ratio * (yuv[..., c].astype(I64) - 128) + 128) >> 8), 0, 255)
    return out


# ---------------------------------------------------------------- 2DNR (HAS §6.17.7)
def _isqrt(v):
    r = np.floor(np.sqrt(v.astype(np.float64))).astype(I64)
    r = np.where(r * r > v, r - 1, r)
    return np.where((r + 1) * (r + 1) <= v, r + 1, r)


def _box3(a):
    """3x3 neighbourhood sum with replicate borders."""
    P = np.pad(a, 1, mode="edge")
    h, w = a.shape
    return sum(P[1 + dy:1 + dy + h, 1 + dx:1 + dx + w] for dy in (-1, 0, 1) for dx in (-1, 0, 1))


def nr2d(y, regs, stats):
    if not regs.f("NR_2D_CTRL", "en"):
        return y.copy()
    p = {"mode": regs.ext("TUNE_NR_CTRL", "MODE"), "median": regs.ext("TUNE_NR_CTRL", "MEDIAN_EN"),
         "seed": regs.ext("TUNE_NR_SEED", "VALUE"), "manual": regs.ext("TUNE_NR_VARIANCE", "VALUE"),
         "scale": regs.ext("TUNE_NR_SCALE", "VALUE"), "min": regs.ext("TUNE_NR_LIMITS", "MIN"),
         "max": regs.ext("TUNE_NR_LIMITS", "MAX"), "blend": regs.ext("TUNE_NR_BLEND", "VALUE"),
         "bands": [regs.ext("TUNE_NR_%s_SCALE" % b, "VALUE") for b in ("LL", "LH", "HL", "HH")],
         "curve": regs.nr_curve, "estimate_valid": regs.nr_have}
    out, stats["_nr_var"] = nr2d_core(y, regs.nr_var, p)
    return out


def _nr_group_variance(sum4, v_est, p):
    """MAS v1.07 §6.17.3.10: base variance per coefficient group, SCALE, limits and
    band scales. Curve mode interpolates at L = (A+B+C+D+2)>>2 of the Haar inputs
    (V107-A19), rounding f*d/len half-up with floor (V107-A20)."""
    if p["mode"] == 1:
        base = np.full(sum4.shape, p["manual"], dtype=I64)
    elif p["mode"] == 2:
        l = (sum4 + 2) >> 2
        k = np.minimum(l >> 4, 15)
        ln = np.where(k == 15, 15, 16)
        knot = np.array(p["curve"], dtype=I64)
        d = knot[k + 1] - knot[k]
        base = knot[k] + (((l - 16 * k) * d + ln // 2) // ln)
    else:
        base = np.full(sum4.shape, v_est if p["estimate_valid"] else p["seed"], dtype=I64)
    v = np.clip((base * p["scale"] + 128) >> 8, p["min"], p["max"])
    return [np.minimum(65535, (v * s + 128) >> 8) for s in p["bands"]]


def nr_variance(hist, n):
    """Lower median m of |HH|, sigma = m / 0.6745 in UQ, var = sigma^2 (HAS §6.17.7.2)."""
    cum = np.cumsum(np.asarray(hist, dtype=I64))
    med = int(np.argmax(2 * cum >= n)) if n else 0
    sq = (med * 97162) >> 8
    return min((sq * sq) >> 16, 65535)


NR_LEGACY = {"mode": 0, "median": 1, "seed": 0, "manual": 0, "scale": 256, "min": 0, "max": 65535, "blend": 256,
             "bands": [256] * 4, "curve": [0] * 17, "estimate_valid": True}


def nr2d_core(y, v, p=None):
    p = NR_LEGACY if p is None else p
    h, w = y.shape
    P = np.pad(y.astype(I64), 1, mode="edge")
    m = np.sort(np.stack([P[1 + dy:1 + dy + h, 1 + dx:1 + dx + w] for dy in (-1, 0, 1) for dx in (-1, 0, 1)]), axis=0)[4]
    if not p["median"]:
        m = y.astype(I64)              # MEDIAN_EN = 0: the original aligned sample feeds Haar
    hb, wb = h // 2, w // 2
    blend = lambda f: ((256 - p["blend"]) * y.astype(I64) + p["blend"] * f + 128) >> 8   # §6.17.3.10
    if hb == 0 or wb == 0:
        return blend(m), 0             # no Haar blocks: median only, var 0 (Table 6-56)
    A, B = m[0:2 * hb:2, 0:2 * wb:2], m[0:2 * hb:2, 1:2 * wb:2]
    C, D = m[1:2 * hb:2, 0:2 * wb:2], m[1:2 * hb:2, 1:2 * wb:2]
    ll, lh = (A + B + C + D) >> 1, (A - B + C - D) >> 1
    hl, hh = (A + B - C - D) >> 1, (A - B - C + D) >> 1
    # Noise estimate from the |HH| lower median (for the next frame).
    var_out = nr_variance(np.bincount(np.abs(hh).ravel(), minlength=256), hb * wb)
    v_ll, v_lh, v_hl, v_hh = _nr_group_variance(A + B + C + D, v, p)   # the centre group's (V107-A21)
    # Wiener on LL (Table 6-54).
    S, Q = _box3(ll), _box3(ll * ll)
    V = 9 * Q - S * S
    r = np.minimum(81 * v_ll, V)
    g = np.where(V > 0, (256 * (V - r)) // np.maximum(V, 1), 0)
    llh = np.clip((256 * S + g * (9 * ll - S) + 1152) // 2304, 0, 510)

    def shrink(band, vb):  # BayesShrink (Table 6-55)
        d9 = np.maximum(_box3(band * band) - 9 * vb, 0)
        t = np.where(d9 > 0, np.minimum(65535, (768 * vb) // np.maximum(_isqrt(d9), 1)), 65535)
        return np.sign(band) * np.maximum(0, (256 * np.abs(band) - t + 128) // 256)

    lhh, hlh, hhh = shrink(lh, v_lh), shrink(hl, v_hl), shrink(hh, v_hh)
    out = m.copy()                                     # odd leftovers keep the median (Table 6-56)
    sat = lambda a: np.clip(a, 0, 255)
    out[0:2 * hb:2, 0:2 * wb:2] = sat((llh + lhh + hlh + hhh) >> 1)
    out[0:2 * hb:2, 1:2 * wb:2] = sat((llh - lhh + hlh - hhh) >> 1)
    out[1:2 * hb:2, 0:2 * wb:2] = sat((llh + lhh - hlh - hhh) >> 1)
    out[1:2 * hb:2, 1:2 * wb:2] = sat((llh - lhh - hlh + hhh) >> 1)
    return blend(out), var_out


# ---------------------------------------------------------------- EE (HAS §6.18.6, DEC-25)
def _sround(x, s):
    return np.sign(x) * ((np.abs(x) + (1 << (s - 1))) >> s)


def ee_gain_chain(gl, ga, gc, gr):
    q = lambda p, r: np.minimum(0xFFFF, (p * r + (1 << 14)) >> 15)
    return q(q(q(gl, ga), gc), gr)                     # ALG-EE-04: L, A, C, R


def ee(y, regs):
    """Enable, profile and scalars as sampled at SOF (regs.ee_frame, MAS
    §6.18.4.1); the profile's tables (frame based here: no write mid-frame)."""
    if not regs.ee_frame["en"]:
        return y.copy()
    h, w = y.shape
    if h < 5 or w < 5:
        return y.copy()
    Y = y.astype(I64)
    t = lambda dy, dx: Y[2 + dy:h - 2 + dy, 2 + dx:w - 2 + dx]
    c = t(0, 0)
    n8 = sum(t(dy, dx) for dy in (-1, 0, 1) for dx in (-1, 0, 1) if dy or dx)
    win = np.stack([t(dy, dx) for dy in range(-2, 3) for dx in range(-2, 3)])
    e_f = _sround(8 * c - n8, 3)
    hc = 16 * c - 2 * (t(-1, 0) + t(1, 0) + t(0, -1) + t(0, 1)) - (t(-1, -1) + t(-1, 1) + t(1, -1) + t(1, 1)) \
        - (t(-2, 0) + t(2, 0) + t(0, -2) + t(0, 2))
    e_c = _sround(hc, 4)
    a = min(regs.ee_f("EE_ALPHA", "alpha"), 0x8000)
    b = min(regs.ee_f("EE_BETA", "beta"), 0x8000)
    d0 = _sround(a * e_f + b * e_c, 15)
    tab = [np.minimum(np.array(regs.ee_luts[regs.ee_frame["profile"]][i], dtype=I64), 0x8000) for i in range(4)]
    gl = tab[0][c >> 2]
    ga = tab[1][np.minimum(np.maximum(np.abs(e_f), np.abs(e_c)) >> 2, 63)] if regs.ee_f("EE_FEATURE_EN", "activity_en") \
        else np.full_like(c, 0x8000)
    ic = (win.max(axis=0) - win.min(axis=0)) >> 3
    gc = np.where(d0 >= 0, tab[2][ic], tab[3][ic]) if regs.ee_f("EE_FEATURE_EN", "contrast_en") else np.full_like(c, 0x8000)
    if regs.ee_f("EE_FEATURE_EN", "radial_en"):
        yy, xx = np.mgrid[2:h - 2, 2:w - 2]
        rr = (2 * xx - regs.ee_f("EE_RADIAL_CENTER", "center_x2")) ** 2 + (2 * yy - regs.ee_f("EE_RADIAL_CENTER", "center_y2")) ** 2
        g = [min(regs.ee_f("EE_RADIAL_GAIN_01", "gain0"), 0xC000), min(regs.ee_f("EE_RADIAL_GAIN_01", "gain1"), 0xC000),
             min(regs.ee_f("EE_RADIAL_GAIN_23", "gain2"), 0xC000), min(regs.ee_f("EE_RADIAL_GAIN_23", "gain3"), 0xC000)]
        th = [regs.ee_f("EE_RADIAL_R2_TH%d" % i, "r2_th%d" % i) for i in range(3)]
        gr = np.where(rr < th[0], g[0], np.where(rr < th[1], g[1], np.where(rr < th[2], g[2], g[3])))
    else:
        gr = np.full_like(c, 0x8000)
    gf = ee_gain_chain(gl, ga, gc, gr)
    d1 = _sround(d0 * gf, 15)
    d2 = np.where(d1 >= 0, np.minimum(d1, regs.ee_f("EE_CLAMP", "clamp_pos")), np.maximum(d1, -regs.ee_f("EE_CLAMP", "clamp_neg")))
    out = Y.copy()
    out[2:h - 2, 2:w - 2] = np.clip(c + d2, 0, 255)
    return out


# ---------------------------------------------------------------- CNF (HAS §6.19.5, DEC-27)
def cnf(yuv, regs):
    if not regs.f("CNF_CTRL", "en"):
        return yuv[..., 1:3].copy()
    filt = cnf_core(yuv, regs.fc("CNF_CHROMA_TH", "chroma_th"), regs.fc("CNF_LUMA_TH", "luma_th"))
    # Output blend with the aligned original (MAS v1.07 §6.19.3.6); 256 in legacy mode.
    w = regs.ext("TUNE_CNF_BLEND", "VALUE")
    return ((256 - w) * yuv[..., 1:3].astype(I64) + w * filt + 128) >> 8


def cnf_core(yuv, tc, ty):
    h, w = yuv.shape[:2]
    P = np.pad(yuv.astype(I64), [(2, 2), (2, 2), (0, 0)], mode="edge")
    c = P[2:2 + h, 2:2 + w]
    n = np.ones((h, w), dtype=I64)
    su, sv = c[..., 1].copy(), c[..., 2].copy()
    for dy in range(-2, 3):
        for dx in range(-2, 3):
            if dy == 0 and dx == 0:
                continue
            p = P[2 + dy:2 + dy + h, 2 + dx:2 + dx + w]
            sel = (np.abs(p[..., 1] - c[..., 1]) + np.abs(p[..., 2] - c[..., 2]) <= tc) & (np.abs(p[..., 0] - c[..., 0]) <= ty)
            n += sel
            su += np.where(sel, p[..., 1], 0)
            sv += np.where(sel, p[..., 2], 0)
    return np.stack([(su + n // 2) // n, (sv + n // 2) // n], axis=-1)


# ---------------------------------------------------------------- Output Formatter (HAS §6.24)
def output_formatter(yuv):
    """YCbCr 4:4:4 -> NV12: Y unchanged; UV = top-left cosited sample of each 2x2 (p201)."""
    y = yuv[..., 0].astype(np.uint8)
    u = yuv[0::2, 0::2, 1].astype(np.uint8)
    v = yuv[0::2, 0::2, 2].astype(np.uint8)
    uv = np.empty((u.shape[0], u.shape[1] * 2), dtype=np.uint8)
    uv[:, 0::2] = u
    uv[:, 1::2] = v
    return y, uv


# ---------------------------------------------------------------- Resizer (MAS v1.07 §6.20)
def _decimate_axis(a, axis):
    """[1,3,3,1]/8 with clamped taps, (sum+4)>>3 (MAS §6.20.3.6)."""
    n = a.shape[axis]
    j = np.arange(n // 2)
    tap = lambda i: np.take(a, np.clip(i, 0, n - 1), axis=axis)
    s = tap(2 * j - 1) + 3 * tap(2 * j) + 3 * tap(2 * j + 1) + tap(2 * j + 2)
    return np.minimum((s + 4) >> 3, 255)


def resizer(yuv, plan):
    """A crop through its Resizer plan (k, out_w, out_h), or unchanged (None)."""
    if plan is None:
        return yuv.copy()
    k, wo, ho = plan
    img = yuv.astype(I64)
    for _ in range(k):                                    # each stage: vertical, then horizontal (normative)
        img = _decimate_axis(img, 0)
        img = _decimate_axis(img, 1)
    nh, nw = img.shape[:2]

    def coords(n, o):
        step = (n * (1 << 17) + o) // (2 * o)             # round to nearest
        init = (step - (1 << 16) + 1) >> 1                # half up
        acc = init + np.arange(o, dtype=I64) * step
        i0 = acc >> 16
        return i0, np.minimum(i0 + 1, n - 1), acc & 0xFFFF

    x0, x1, al = coords(nw, wo)
    y0, y1, be = coords(nh, ho)
    al = al[None, :, None]
    be = be[:, None, None]
    p00, p10 = img[y0][:, x0], img[y0][:, x1]
    p01, p11 = img[y1][:, x0], img[y1][:, x1]
    top = ((1 << 16) - al) * p00 + al * p10
    bot = ((1 << 16) - al) * p01 + al * p11
    v = ((1 << 16) - be) * top + be * bot
    return np.minimum((v + (1 << 31)) >> 32, 255)


# Table 6-89 inverse CSC (Q16): {Y scale, CrR, CbG, CrG, CbB} by (full range, BT.709).
INV_CSC = {(True, 0): (65536, 91881, 22553, 46802, 116130), (True, 1): (65536, 103206, 12276, 30679, 121609),
           (False, 0): (76309, 104597, 25675, 53279, 132201), (False, 1): (76309, 117489, 13975, 34925, 138438)}


def rgb888(yuv, std, full):
    """CH2 YCbCr -> packed R, G, B bytes (MAS §6.15.3, §6.24.7): one rounding
    R(n, 65536) = floor((n + 32768) / 65536) per output, then clamp 0..255."""
    ys, crr, cbg, crg, cbb = INV_CSC[(bool(full), 1 if std else 0)]
    y, d, e = (yuv[..., i].astype(I64) for i in range(3))
    d, e = d - 128, e - 128
    lum = y * 65536 if full else (y - 16) * ys
    out = [lum + crr * e, lum - cbg * d - crg * e, lum + cbb * d]
    return np.stack([np.clip((c + 32768) >> 16, 0, 255) for c in out], axis=-1).astype(np.uint8)


def channel_output(yuv, regs, ch):
    """Channel `ch` of the post-IQ YUV444 frame: crop, Resizer, then NV12
    (CH0/CH1) or RGB888 bytes (CH2)."""
    s = geometry.for_frame(regs, ch)
    x, y, w, h = s["crop"]
    out = resizer(yuv[y:y + h, x:x + w], s["plan"])
    if ch == 2:
        return rgb888(out, regs.f("CSC_CTRL", "std"), regs.ext("TUNE_CSC_CTRL", "FULL_RANGE")).tobytes()
    return output_formatter(out)


# ---------------------------------------------------------------- whole pipeline
def run(raw, regs, stats=None):
    """RAW container frame (uint16 [H, W]) -> (Y plane, interleaved UV plane).
    `stats` receives the published block counters (internal keys start with _)."""
    stats = {} if stats is None else stats
    p = input_formatter(raw, regs)
    p = blc(p, regs)
    p = lsc(p, regs, stats)
    p = bpc(p, regs, stats)
    stats["_tap_aec_bpc"] = p                           # AEC (BPC source) and AWB tap (MAS §6.21-6.22)
    p = wb(p, regs)
    p = dg(p, regs)
    stats["_tap_aec_dg"] = p                            # AEC DG source (TUNE_AEC_SOURCE = 1)
    rgb = demosaic(p, regs.ext("TUNE_DEMOSAIC_MARGIN", "GREEN"), regs.ext("TUNE_DEMOSAIC_MARGIN", "RB"))
    rgb = ccm(rgb, regs)
    rgb = gamma(rgb, regs)
    yuv = csc(rgb, regs)
    stats["_tap_af"] = yuv[..., 0]
    yuv = gtm(yuv, regs, stats)
    y = ee(nr2d(yuv[..., 0], regs, stats), regs)        # luma path
    uv = cnf(yuv, regs)                                 # chroma path, GTM luma (ALG-CNF-01)
    yuv = np.concatenate([y[..., None], uv], axis=-1)
    # CH1/CH2 are computed when enabled; the integrated ODMA writes them from V107-8.
    outputs = regs.f("OUTPUT_ENABLE", "channels")
    stats["_channels"] = {ch: channel_output(yuv, regs, ch) for ch in (1, 2) if outputs & (1 << ch)}
    return channel_output(yuv, regs, 0)


def frame_sof(regs):
    """State updates at the accepted SOF: frame counter, AEC commit, AWB/AF
    sampling, context latch, GTM bank swap, LSC profile selection. A frame
    aborted after its SOF (AXI read error, soft reset) has had these and
    nothing else."""
    regs.stats.sof(regs)
    regs.lsc_sof()
    regs.ee_sof()
    # GTM curve-build controls of this frame (MAS v1.07 §6.16.4.3).
    regs.gtm_cap = (regs.f("GTM_CTRL", "manual"), regs.f("GTM_KEY", "key"), regs.f("GTM_LWHITE", "lwhite"))
    if regs.gtm_pending:
        regs.gtm_read ^= 1
        regs.gtm_pending = False
        regs.gtm_valid = True


def frame_abort(regs):
    """A frame abandoned after its SOF (AXI read error): no publication; the
    AWB measurement in flight frees its bank."""
    regs.stats.abort()


def frame_eof(regs, stats):
    """State updates at the end of a frame the pipeline completed (also when
    the ODMA then fails to write it): GTM curve build, 2DNR variance hand-over,
    statistics publication."""
    manual, key, lwhite = regs.gtm_cap         # captured at the frame's SOF, not live at EOF
    if not manual:
        regs.gtm_banks[regs.gtm_read ^ 1] = gtm_curve(stats["_gtm_yavg"], key, lwhite)
        regs.gtm_pending = True
    if "_nr_var" in stats:
        regs.nr_var = stats["_nr_var"]
        regs.nr_have = True
        stats["NR_2D_EST_VAR"] = stats["_nr_var"]
    regs.stats.eof(regs, {"aec_bpc": stats["_tap_aec_bpc"], "aec_dg": stats["_tap_aec_dg"], "af": stats["_tap_af"]})


def run_frame(raw, regs, stats=None):
    """One frame including the SOF and EOF state updates, in engine order."""
    stats = {} if stats is None else stats
    frame_sof(regs)
    out = run(raw, regs, stats)
    frame_eof(regs, stats)
    return out
