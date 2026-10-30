"""Frame-based reference of the FX1 ISP statistics generators and of their
software-visible publication, CSR/MAS v1.07: AEC (MAS §6.21), AWB (§6.22),
AF (§6.23), result ownership and indexed readout (§9.8, §9.9.9).

- AEC measures RAW12 Bayer samples at the post-BPC or post-DG tap
  (TUNE_AEC_SOURCE): clip-qualified sum/count, strict OE/UE, extrema and the
  64-bin histogram over every sample, per zone and globally, plus the optional
  spatially weighted histogram (profile mode).
- AWB measures complete 2x2 post-BPC cells: strict four-sample limits, half-up
  green, optional cell ROI, R/G/B limits and ellipse union (profile mode).
- AF measures the combined Sobel of the CSC luma in the uniform 4x4 grid or
  sixteen rectangles, with the noise floor, focus/luma/count per window.
- AEC and AF publish into two result banks each, owned through ACQUIRE/
  RELEASE with 64-bit indexed readout; AWB keeps its own bank protocol.

The measurement functions work on whole frames with numpy; `StatsState`
holds what the register interface shows and answers reads of the statistics
registers (bus reads: a DATA_LO read takes the snapshot, a read without a
lease sets TUNE_ERROR.NO_LEASE). The reference is frame based: commands
arrive between frames, so the same-edge rules of MAS Table 9-198 do not arise
here; the C++ directed tests check them.
"""

import numpy as np

I64 = np.int64
U64 = (1 << 64) - 1
ERR_INVALID, ERR_NO_LEASE = 1 << 1, 1 << 6       # TUNE_ERROR bits
AE_ZONES_MAX = 768
DOMAIN_RAW12 = 1                                 # TUNE_AEC_RESULT_META.DOMAIN (Table 8-184)


def _sat(v):
    return min(int(v), U64)


# ---------------------------------------------------------------- AEC
def aec_legal(c, w, h):
    """MAS §6.21.4: grid coverage, clip and threshold order, weighted ROI/steps."""
    if not (1 <= c["nx"] <= 32 and 1 <= c["ny"] <= 24 and c["zw"] > 0 and c["zh"] > 0):
        return False
    if not ((c["nx"] - 1) * c["zw"] < w <= c["nx"] * c["zw"] and (c["ny"] - 1) * c["zh"] < h <= c["ny"] * c["zh"]):
        return False
    if not (c["min"] <= c["max"] and c["th_ue"] < c["th_oe"]):
        return False
    hc = c["hist"]
    if hc["en"]:
        if hc["sx"] == 0 or hc["sy"] == 0:
            return False
        if hc["roi_en"] and not (hc["x0"] < hc["x1"] <= w and hc["y0"] < hc["y1"] <= h):
            return False
    return True


def hist_roi(hc, w, h):
    """Effective weighted ROI [x0, x1) x [y0, y1) (§6.21.3.4)."""
    return (hc["x0"], hc["y0"], hc["x1"], hc["y1"]) if hc["roi_en"] else (0, 0, w, h)


def aec_measure(p, c):
    """p: selected RAW12 Bayer tap [H, W]; c: the frame's AEC set (legal)."""
    h, w = p.shape
    S = p.astype(I64)
    yy, xx = np.mgrid[0:h, 0:w]
    q = (S >= c["min"]) & (S <= c["max"])                       # inclusive clip
    oe, ue = S > c["th_oe"], S < c["th_ue"]                     # strict, not clip-qualified
    nx, ny = c["nx"], c["ny"]
    z = np.minimum(yy // c["zh"], ny - 1) * nx + np.minimum(xx // c["zw"], nx - 1)
    n = nx * ny
    zsum = np.zeros(n, I64)
    np.add.at(zsum, z[q], S[q])
    zmin = np.full(n, 4095, I64)
    zmax = np.zeros(n, I64)
    np.minimum.at(zmin, z.ravel(), S.ravel())
    np.maximum.at(zmax, z.ravel(), S.ravel())
    res = {
        "sum": int(S[q].sum()), "count": int(q.sum()), "oe": int(oe.sum()), "ue": int(ue.sum()),
        "min": int(S.min()) if S.size else 4095, "max": int(S.max()) if S.size else 0,
        "hist": np.bincount((S >> 6).ravel(), minlength=64).tolist(),
        "zones": [{"sum": int(zsum[i]), "count": int(c_), "oe": int(o), "ue": int(u),
                   "min": int(zmin[i]), "max": int(zmax[i])}
                  for i, (c_, o, u) in enumerate(zip(np.bincount(z[q], minlength=n),
                                                     np.bincount(z[oe], minlength=n),
                                                     np.bincount(z[ue], minlength=n)))],
        "whist": [0] * 64, "wtotal": 0, "wsum": 0, "roi": (0, 0, 0, 0), "steps": (0, 0),
    }
    hc = c["hist"]
    if hc["en"]:
        x0, y0, x1, y1 = hist_roi(hc, w, h)
        sel = ((xx >= x0) & (xx < x1) & (yy >= y0) & (yy < y1) &
               ((xx - x0) % hc["sx"] == 0) & ((yy - y0) % hc["sy"] == 0))
        cx = np.minimum((5 * (xx - x0)) // max(x1 - x0, 1), 4)
        cy = np.minimum((5 * (yy - y0)) // max(y1 - y0, 1), 4)
        wt = np.asarray(hc["w"], dtype=I64)[np.clip(cy, 0, 4) * 5 + np.clip(cx, 0, 4)]
        wt = np.where(sel, wt, 0)
        wh = np.zeros(64, I64)
        np.add.at(wh, (S >> 6).ravel(), wt.ravel())
        res.update(whist=wh.tolist(), wtotal=int(wt.sum()), wsum=int((wt * S).sum()),
                   roi=(x0, y0, x1, y1), steps=(hc["sx"], hc["sy"]))
    return res


def aec_index(res, i):
    """Logical 64-bit AEC result (MAS Table 9-193): (value, error bits)."""
    pack = lambda x, y: (y << 16) | x
    if 0 <= i <= 8:
        return [res["sum"], res["count"], res["oe"], res["ue"], res["min"], res["max"],
                res["wtotal"], res["wsum"], 0][i], 0
    if i == 0x10:
        return pack(*res["roi"][0:2]), 0
    if i == 0x11:
        return pack(*res["roi"][2:4]), 0
    if i == 0x12:
        return res["steps"][0] | (res["steps"][1] << 8), 0
    if 0x100 <= i < 0x140:
        return res["hist"][i - 0x100], 0
    if 0x140 <= i < 0x180:
        return res["whist"][i - 0x140], 0
    if 0x200 <= i < 0x200 + 8 * AE_ZONES_MAX:
        zi, k = divmod(i - 0x200, 8)
        if zi >= len(res["zones"]) or k >= 6:
            return 0, 0                          # beyond the captured grid / padding (V107-A23)
        zz = res["zones"][zi]
        return [zz["sum"], zz["count"], zz["oe"], zz["ue"], zz["min"], zz["max"]][k], 0
    return 0, ERR_INVALID


# ---------------------------------------------------------------- AWB
def awb_legal(c, w, h):
    """MAS §6.22.3.7: even active frame, 1 <= NX <= min(64, W/2), 1 <= NY <= min(32, H/2)."""
    return (32 <= w <= 3840 and 32 <= h <= 2160 and 1 <= c["nx"] <= min(64, w // 2)
            and 1 <= c["ny"] <= min(32, h // 2))


def _rnd_div(n, d):
    return (n + d // 2) // d                     # R(n, d) = floor((n + floor(d/2)) / d)


def awb_measure(p, c):
    """p: post-BPC RGGB Bayer [H, W]; c: the frame's AWB set (legal). Cells
    (i, j) of the 2x2 raster: R(2i,2j), Gr(2i+1,2j), Gb(2i,2j+1), B(2i+1,2j+1)."""
    h, w = p.shape
    hc, wc = h // 2, w // 2
    P = p.astype(I64)[:2 * hc, :2 * wc]
    R, Gr, Gb, B = P[0::2, 0::2], P[0::2, 1::2], P[1::2, 0::2], P[1::2, 1::2]
    st = np.stack([R, Gr, Gb, B])
    ok = (st.min(axis=0) > c["under"]) & (st.max(axis=0) < c["sat"])   # strict at both limits
    G = (Gr + Gb + 1) >> 1
    lim = c["limits"]
    ok &= (R >= lim[0][0]) & (R <= lim[0][1]) & (G >= lim[1][0]) & (G <= lim[1][1]) & \
          (B >= lim[2][0]) & (B <= lim[2][1])
    jj, ii = np.mgrid[0:hc, 0:wc]
    if c["roi_en"]:
        x0, y0, x1, y1 = c["roi"]
        ok &= (ii >= x0) & (ii < x1) & (jj >= y0) & (jj < y1)
    if c["color_en"]:
        ok &= G >= c["min_g"]
        Gs = np.maximum(G, 1)
        rg, bg = (256 * R + Gs // 2) // Gs, (256 * B + Gs // 2) // Gs
        ok &= (rg <= 65535) & (bg <= 65535)
        inside = np.zeros_like(ok)
        for k, e in enumerate(c["ellipses"]):
            if not (c["mask"] >> k) & 1:
                continue
            dr, db = rg - e["rg"], bg - e["bg"]
            inside |= e["a"] * dr * dr + 2 * e["b"] * dr * db + e["c"] * db * db <= e["r2"] * 16384
        ok &= inside
    nx, ny = c["nx"], c["ny"]
    zx = np.minimum(((np.arange(wc, dtype=I64) + 1) * nx - 1) // wc, nx - 1)
    zy = np.minimum(((np.arange(hc, dtype=I64) + 1) * ny - 1) // hc, ny - 1)
    z = (zy[:, None] * nx + zx[None, :])[ok]
    zsum = np.zeros((nx * ny, 3), I64)
    for k, ch in enumerate((R, G, B)):
        np.add.at(zsum[:, k], z, ch[ok])
    return {"sum": [int(R[ok].sum()), int(G[ok].sum()), int(B[ok].sum())], "n": int(ok.sum()),
            "nx": nx, "ny": ny, "zsum": zsum.tolist(), "zcnt": np.bincount(z, minlength=nx * ny).tolist()}


# ---------------------------------------------------------------- AF
def af_windows(c, w, h):
    """Effective rectangles [x0, x1) x [y0, y1) of the sixteen windows and the
    enable mask: the uniform 4x4 grid of z = floor(y*4/H)*4 + floor(x*4/W) in
    WINDOW_MODE = 0 (all sixteen), the programmed ones in WINDOW_MODE = 1."""
    if c["mode"] == 0:
        cut = lambda d, k: (k * d + 3) // 4      # first coordinate of column/row k
        return [(cut(w, z % 4), cut(h, z // 4), cut(w, z % 4 + 1), cut(h, z // 4 + 1)) for z in range(16)], 0xFFFF
    return [r if (c["mask"] >> k) & 1 else (0, 0, 0, 0) for k, r in enumerate(c["rects"])], c["mask"]


def af_measure(y, c):
    """y: CSC luma [H, W]: Sobel |gx| + |gy| with replicate borders, minus NOISE
    floored at zero; per window focus, luma and count (§6.23.3.7)."""
    h, w = y.shape
    e = np.pad(y.astype(I64), 1, mode="edge")
    s = lambda dy, dx: e[1 + dy:1 + dy + h, 1 + dx:1 + dx + w]
    gx = (s(-1, 1) + 2 * s(0, 1) + s(1, 1)) - (s(-1, -1) + 2 * s(0, -1) + s(1, -1))
    gy = (s(1, -1) + 2 * s(1, 0) + s(1, 1)) - (s(-1, -1) + 2 * s(-1, 0) + s(-1, 1))
    d = np.maximum(np.abs(gx) + np.abs(gy) - c["noise"], 0)
    rects, mask = af_windows(c, w, h)
    win = []
    for x0, y0, x1, y1 in rects:
        win.append({"focus": int(d[y0:y1, x0:x1].sum()), "luma": int(y[y0:y1, x0:x1].astype(I64).sum()),
                    "count": max(x1 - x0, 0) * max(y1 - y0, 0)})
    return {"win": win, "rects": rects, "mask": mask, "mode": c["mode"], "w": w, "h": h}


def af_index(res, i):
    """Logical 64-bit AF result (MAS Table 9-193, §9.9.9): (value, error bits)."""
    pack = lambda x, y: (y << 16) | x
    if i == 0:
        return 0, 0                              # flags: no accumulator overflow
    if 0x10 <= i <= 0x13:
        return [res["w"], res["h"], res["mask"], res["mode"]][i - 0x10], 0
    if 0x100 <= i < 0x140:
        wi, k = divmod(i - 0x100, 4)
        return ([res["win"][wi]["focus"], res["win"][wi]["luma"], res["win"][wi]["count"], 0][k]), 0
    if 0x200 <= i < 0x220:
        wi, k = divmod(i - 0x200, 2)
        r = res["rects"][wi]
        return pack(r[0], r[1]) if k == 0 else pack(r[2], r[3]), 0
    return 0, ERR_INVALID


# ---------------------------------------------------------------- result banks (AEC, AF)
class LeasedEngine:
    """Two result banks with FREE/FILLING/READY/LEASED ownership (MAS §9.8)."""

    def __init__(self):
        self.clear()

    def clear(self):
        self.banks = [None, None]                # None = FREE, else {"state", "res", "meta"}
        self.ready = []                          # completion order
        self.filling = None
        self.leased = None
        self.drops = 0
        self.snap = None                         # DATA_LO snapshot (high word)

    def admit(self, enabled, legal, meta):
        self.filling = None
        if not enabled:
            return False                         # disabled frames leave banks and drops
        free = [b for b in (0, 1) if self.banks[b] is None]
        if not legal or not free:
            self.drops = min(self.drops + 1, 0xFFFFFFFF)
            return False
        self.filling = free[0]                   # bank 0 first
        self.banks[self.filling] = {"state": "filling", "res": None, "meta": meta}
        return True

    def publish(self, res):
        b = self.banks[self.filling]
        b["res"], b["state"] = res, "ready"
        self.ready.append(self.filling)
        self.filling = None

    def abort(self):
        if self.filling is not None:
            self.banks[self.filling] = None
            self.filling = None

    def command(self, value):
        """Combined RELEASE + ACQUIRE: release first, then the oldest READY bank."""
        err = 0
        if value & 2:
            if self.leased is None:
                err |= ERR_NO_LEASE
            else:
                self.banks[self.leased] = None
                self.leased = None
                self.snap = None
        if value & 1:
            if self.leased is not None:
                err |= ERR_NO_LEASE              # duplicate acquire
            elif self.ready:
                self.leased = self.ready.pop(0)
                self.banks[self.leased]["state"] = "leased"
        return err

    def lease(self):
        return self.banks[self.leased] if self.leased is not None else None


# ---------------------------------------------------------------- register view
class StatsState:
    def __init__(self):
        self.counter = 0                # frames started since reset (ALG-STAT-01)
        self.fid = 0
        self.aec_commit = False
        self.aec_active = self._aec_reset_set()
        self.awb_sof = None
        self.af_sof = None
        self.ctx = {}
        self.ro = {}                    # published status bits
        self.aec = LeasedEngine()
        self.af = LeasedEngine()
        self._awb_clear()

    @staticmethod
    def _aec_reset_set():
        return {"en": 0, "source": 0, "nx": 32, "ny": 24, "zw": 120, "zh": 90, "min": 0, "max": 4095,
                "th_ue": 272, "th_oe": 3744,
                "hist": {"en": 0, "roi_en": 0, "x0": 0, "y0": 0, "x1": 0, "y1": 0, "sx": 1, "sy": 1, "w": [1] * 25}}

    def _awb_clear(self):
        """AWB ownership and reporting after a hard or COMMON soft reset (MAS §6.22.4.1)."""
        self.awb_banks = [None, None]   # None = FREE, else {"res", "fid", "ctx", "state", ...}
        self.awb_ready = []             # READY banks, completion order
        self.awb_writing = None
        self.awb_leased = None
        self.awb_overrun = False
        self.awb_drops = 0

    def soft_reset(self):
        """COMMON soft reset (MAS Table 9-187): results and ownership of all three
        engines, done bits; a pending AEC commit is kept (owner Q4); FRAME_IDs
        (the counter) and the active configuration are kept."""
        self.ro = {}
        self.aec.clear()
        self.af.clear()
        self._awb_clear()

    def abort(self):
        """A frame abandoned after its SOF: the measurements in flight free their
        banks; READY and LEASED banks survive (MAS Table 9-198)."""
        self.aec.abort()
        self.af.abort()
        if self.awb_writing is not None:
            self.awb_banks[self.awb_writing] = None
            self.awb_writing = None

    def _awb_loss(self):
        self.awb_overrun = True
        self.awb_drops = min(self.awb_drops + 1, 0xFFFFFFFF)

    def _awb_admit(self, regs):
        """Reserve a FREE bank at an enabled SOF, bank 0 first; an illegal grid
        or no FREE bank skips the measurement and reports it (MAS §6.22.4.8)."""
        self.awb_writing = None
        c = self.awb_sof
        if not c["en"]:
            return
        w, h = regs.active_size()
        free = [b for b in (0, 1) if self.awb_banks[b] is None]
        if not awb_legal(c, w, h) or not free:
            self._awb_loss()
            c["en"] = 0
            return
        b = free[0]
        self.awb_banks[b] = {"state": "writing", "res": None, "fid": self.fid, "ctx": self.ctx["AWB"],
                             "profile": self.profile_id, "size": (w, h)}
        self.awb_writing = b

    def awb_command(self, value):
        """AWB_RESULT_CTRL (MAS §6.22.4.3): CLEAR_REPORT, then RELEASE (wins
        over ACQUIRE), else ACQUIRE of the oldest READY bank."""
        if value & 4:
            self.awb_overrun = False
            self.awb_drops = 0
        if value & 2:
            if self.awb_leased is not None:
                self.awb_banks[self.awb_leased] = None
                self.awb_leased = None
        elif value & 1:
            if self.awb_leased is None and self.awb_ready:
                self.awb_leased = self.awb_ready.pop(0)
                self.awb_banks[self.awb_leased]["state"] = "leased"

    def _awb_lease(self):
        return self.awb_banks[self.awb_leased] if self.awb_leased is not None else None

    # -- frame configuration ------------------------------------------------
    @staticmethod
    def _aec_set(regs):
        hist = {"en": regs.ext("TUNE_HIST_CTRL", "ENABLE"), "roi_en": regs.ext("TUNE_HIST_CTRL", "ROI_EN"),
                "x0": regs.ext("TUNE_HIST_START", "X"), "y0": regs.ext("TUNE_HIST_START", "Y"),
                "x1": regs.ext("TUNE_HIST_END", "X"), "y1": regs.ext("TUNE_HIST_END", "Y"),
                "sx": regs.ext("TUNE_HIST_STEP", "X"), "sy": regs.ext("TUNE_HIST_STEP", "Y"),
                "w": [regs.ext("TUNE_HIST_WEIGHT%d" % k, "VALUE") for k in range(25)]}
        return {"en": regs.f("AEC_CTRL", "en"), "source": regs.f("TUNE_AEC_SOURCE", "SOURCE"),
                "nx": regs.f("AEC_ZONE_CFG", "num_zone_x"), "ny": regs.f("AEC_ZONE_CFG", "num_zone_y"),
                "zw": regs.f("AEC_ZONE_SIZE", "zone_width"), "zh": regs.f("AEC_ZONE_SIZE", "zone_height"),
                "min": regs.f("AEC_SAMPLE_CLIP", "sample_min_clip"),
                "max": regs.f("AEC_SAMPLE_CLIP", "sample_max_clip"),
                "th_ue": regs.f("AEC_THRESH", "th_ue"), "th_oe": regs.f("AEC_THRESH", "th_oe"), "hist": hist}

    @staticmethod
    def _awb_set(regs):
        lim = [(regs.ext("TUNE_AWB_%s_LIMITS" % ch, "MIN"), regs.ext("TUNE_AWB_%s_LIMITS" % ch, "MAX")) for ch in "RGB"]
        sext16 = lambda v: v - 0x10000 if v & 0x8000 else v
        ell = [{"rg": regs.ext("TUNE_AWB_E%d_CENTER" % k, "RG"), "bg": regs.ext("TUNE_AWB_E%d_CENTER" % k, "BG"),
                "a": regs.ext("TUNE_AWB_E%d_A" % k, "VALUE"), "b": sext16(regs.ext("TUNE_AWB_E%d_B" % k, "VALUE")),
                "c": regs.ext("TUNE_AWB_E%d_C" % k, "VALUE"), "r2": regs.ext("TUNE_AWB_E%d_RADIUS2" % k, "VALUE")}
               for k in range(8)]
        return {"en": regs.f("AWB_CTRL", "en"), "nx": regs.f("AWB_CTRL", "num_zone_x"),
                "ny": regs.f("AWB_CTRL", "num_zone_y"),
                "under": regs.f("AWB_UNDEREXPOSED_LIMIT", "underexposed_limit"),
                "sat": regs.f("AWB_SATURATION_LIMIT", "saturation_limit"),
                "roi_en": regs.ext("TUNE_AWB_CTRL", "ROI_EN"), "color_en": regs.ext("TUNE_AWB_CTRL", "COLOR_EN"),
                "mask": regs.ext("TUNE_AWB_CTRL", "ELLIPSE_MASK"),
                "roi": (regs.ext("TUNE_AWB_ROI_START", "X"), regs.ext("TUNE_AWB_ROI_START", "Y"),
                        regs.ext("TUNE_AWB_ROI_END", "X"), regs.ext("TUNE_AWB_ROI_END", "Y")),
                "limits": lim, "min_g": regs.ext("TUNE_AWB_MIN_G", "VALUE"), "ellipses": ell}

    @staticmethod
    def _af_set(regs):
        return {"en": regs.f("AF_CTRL", "en"), "mode": regs.ext("TUNE_AF_CTRL", "WINDOW_MODE"),
                "mask": regs.ext("TUNE_AF_CTRL", "WINDOW_MASK"), "noise": regs.ext("TUNE_AF_NOISE", "VALUE"),
                "rects": [(regs.ext("TUNE_AF_W%d_START" % k, "X"), regs.ext("TUNE_AF_W%d_START" % k, "Y"),
                           regs.ext("TUNE_AF_W%d_END" % k, "X"), regs.ext("TUNE_AF_W%d_END" % k, "Y"))
                          for k in range(16)]}

    def sof(self, regs):
        self.counter = (self.counter + 1) & 0xFFFFFFFF
        self.fid = self.counter
        # The profile identity the hardware applied (one epoch in the reference).
        self.profile_id = regs.value["TUNE_PROFILE_ID"] if regs.profile_mode() else 0
        # AEC: legacy mode transfers the shadow set (with TUNE_AEC_SOURCE) at
        # the SOF after a commit; profile mode uses the active bank (§8.3).
        if regs.profile_mode():
            self.aec_active = self._aec_set(regs)
        elif self.aec_commit:
            self.aec_active = self._aec_set(regs)
            self.aec_active["hist"] = self._aec_reset_set()["hist"]   # storage only in legacy mode
            self.aec_commit = False
        self.awb_sof = self._awb_set(regs)
        self.af_sof = self._af_set(regs)
        self.ctx = {b: regs.value[b + "_CONTEXT_ID"] for b in ("AEC", "AWB", "AF")}
        w, h = regs.active_size()
        a = self.aec_active
        self.aec.admit(a["en"], aec_legal(a, w, h),
                       {"fid": self.fid, "ctx": self.ctx["AEC"], "profile": self.profile_id, "set": dict(a),
                        "depth": regs.f("COMMON_INPUT_FORMAT", "DEPTH"), "size": (w, h)})
        self.af.admit(self.af_sof["en"], True,
                      {"fid": self.fid, "ctx": self.ctx["AF"], "profile": self.profile_id})
        self._awb_admit(regs)

    def eof(self, regs, taps):
        if self.aec.filling is not None:
            a = self.aec.banks[self.aec.filling]["meta"]["set"]
            self.aec.publish(aec_measure(taps["aec_dg" if a["source"] else "aec_bpc"], a))
            self.ro["AEC_STATUS"] = self.ro.get("AEC_STATUS", 0) | 0x2
        if self.awb_writing is not None:    # the complete bank becomes READY
            bank = self.awb_banks[self.awb_writing]
            bank["res"] = awb_measure(taps["aec_bpc"], self.awb_sof)
            bank["state"] = "ready"
            self.awb_ready.append(self.awb_writing)
            self.awb_writing = None
            self.ro["AWB_STATUS"] = self.ro.get("AWB_STATUS", 0) | 0x1
        if self.af.filling is not None:     # EN sampled at SOF (MAS §6.23.2)
            self.af.publish(af_measure(taps["af"], self.af_sof))
            self.ro["AF_STATUS"] = self.ro.get("AF_STATUS", 0) | 0x2

    # -- commands -------------------------------------------------------------
    def tune_error(self, regs, bits):
        regs.value["TUNE_ERROR"] |= bits

    def command(self, regs, name, value):
        if name == "TUNE_AEC_CMD":
            self.tune_error(regs, self.aec.command(value))
        elif name == "TUNE_AF_CMD":
            self.tune_error(regs, self.af.command(value))
        elif name == "TUNE_AEC_INDEX":
            self.aec.snap = None
        elif name == "TUNE_AF_INDEX":
            self.af.snap = None
        elif name in ("TUNE_AEC_DROPS", "TUNE_AF_DROPS") and value:
            (self.aec if name == "TUNE_AEC_DROPS" else self.af).drops = 0    # W1C clears the whole word
        elif name == "AWB_RESULT_CTRL":
            self.awb_command(value)

    # -- reads (bus reads, with their side effects) -----------------------------
    def _leased_read(self, regs, eng, fn):
        lease = eng.lease()
        if lease is None:
            self.tune_error(regs, ERR_NO_LEASE)
            return 0
        return fn(lease)

    def _data(self, regs, eng, index_reg, index_fn, high):
        lease = eng.lease()
        if high:
            if lease is None or eng.snap is None:
                self.tune_error(regs, ERR_NO_LEASE)
                return 0
            return eng.snap
        if lease is None:
            self.tune_error(regs, ERR_NO_LEASE)
            return 0
        v, err = index_fn(lease["res"], regs.f(index_reg, "VALUE"))
        self.tune_error(regs, err)
        eng.snap = (v >> 32) & 0xFFFFFFFF
        return v & 0xFFFFFFFF

    def read(self, regs, name):
        sel = regs.f("AEC_CHANNEL_SEL", "channel_sel")
        eng = self.aec
        lr = lambda fn: self._leased_read(regs, eng, fn)
        zone = lambda L: (L["res"]["zones"][regs.f("AEC_ZONE_ADDR", "zone_addr")]
                          if regs.f("AEC_ZONE_ADDR", "zone_addr") < len(L["res"]["zones"]) else None)
        aec_reads = {
            "AEC_GLOBAL_SUM_LO": lambda L: L["res"]["sum"] & 0xFFFFFFFF if sel == 0 else 0,
            "AEC_GLOBAL_SUM_HI": lambda L: (L["res"]["sum"] >> 32) & 1 if sel == 0 else 0,
            "AEC_GLOBAL_COUNT": lambda L: L["res"]["count"] & 0x7FFFFF if sel == 0 else 0,
            "AEC_ZONE_SUM": lambda L: zone(L)["sum"] & 0x0FFFFFFF if zone(L) and sel == 0 else 0,
            "AEC_ZONE_COUNT": lambda L: zone(L)["count"] & 0xFFFF if zone(L) and sel == 0 else 0,
            "AEC_ZONE_GREEN_OE_UE": lambda L: (((zone(L)["oe"] & 0xFFFF) << 16) | (zone(L)["ue"] & 0xFFFF))
            if zone(L) else 0,
            "AEC_ZONE_GREEN_MIN_MAX": lambda L: (zone(L)["max"] << 12) | zone(L)["min"] if zone(L) else 0,
            "AEC_HIST_DATA": lambda L: L["res"]["hist"][regs.f("AEC_HIST_ADDR", "hist_addr")] & 0x7FFFFF,
            "AEC_FRAME_ID": lambda L: L["meta"]["fid"],
            "AEC_RESULT_CONTEXT_ID": lambda L: L["meta"]["ctx"],
            "TUNE_AEC_FRAME": lambda L: L["meta"]["fid"],
            "TUNE_AEC_PROFILE": lambda L: L["meta"]["profile"],
            "TUNE_AEC_RESULT_META": lambda L: (L["meta"]["set"]["source"] | (DOMAIN_RAW12 << 4) |
                                               (L["meta"]["depth"] << 8) | (1 << 31)),
            "TUNE_AE_FRAME_SIZE": lambda L: L["meta"]["size"][0] | (L["meta"]["size"][1] << 16),
            "TUNE_AE_GRID": lambda L: L["meta"]["set"]["nx"] | (L["meta"]["set"]["ny"] << 16),
            "TUNE_AE_ZONE_SIZE": lambda L: L["meta"]["set"]["zw"] | (L["meta"]["set"]["zh"] << 16),
        }
        if name in aec_reads:
            return lr(aec_reads[name])
        if name == "TUNE_AEC_DATA_LO" or name == "TUNE_AEC_DATA_HI":
            return self._data(regs, self.aec, "TUNE_AEC_INDEX", aec_index, name.endswith("HI"))
        if name == "TUNE_AF_DATA_LO" or name == "TUNE_AF_DATA_HI":
            return self._data(regs, self.af, "TUNE_AF_INDEX", af_index, name.endswith("HI"))
        for blk, e in (("AEC", self.aec), ("AF", self.af)):
            if name == "TUNE_%s_STATUS" % blk:
                return ((1 if e.leased is not None else 0) | (len(e.ready) << 1) |
                        (0x8 if e.filling is not None else 0))
            if name == "TUNE_%s_DROPS" % blk:
                return e.drops
        af_reads = {
            "AF_STAT_DATA": lambda L: L["res"]["win"][regs.f("AF_STAT_ADDR", "zone_addr")]["focus"] & 0xFFFFFFFF,
            "AF_FRAME_ID": lambda L: L["meta"]["fid"],
            "AF_RESULT_CONTEXT_ID": lambda L: L["meta"]["ctx"],
            "TUNE_AF_FRAME": lambda L: L["meta"]["fid"],
            "TUNE_AF_PROFILE": lambda L: L["meta"]["profile"],
        }
        if name in af_reads:
            return self._leased_read(regs, self.af, af_reads[name])
        if name == "AF_STATUS":                  # SCORE_VALID: a lease holds published scores (V107-A24)
            return self.ro.get("AF_STATUS", 0) | (0x20 if self.af.leased is not None else 0)
        lease = self._awb_lease()
        if name == "AWB_RESULT_STATUS":
            v = len(self.awb_ready) | (0x40 if self.awb_overrun else 0) | (0x20 if self.awb_writing is not None else 0)
            if lease:
                b = lease["res"]
                v |= 0xC | (self.awb_leased << 4) | (b["nx"] << 8) | (b["ny"] << 16)
            return v
        if name == "AWB_DROP_COUNT":
            return self.awb_drops
        if name in ("AWB_RESULT_CTRL", "TUNE_AEC_CMD", "TUNE_AF_CMD"):
            return 0                    # commands read zero
        if name in ("TUNE_AWB_PROFILE", "TUNE_AWB_FRAME_SIZE"):
            if not lease:
                return 0                # zero without a lease, no error (§9.9.9)
            return lease["profile"] if name == "TUNE_AWB_PROFILE" else lease["size"][0] | (lease["size"][1] << 16)
        if name in ("AWB_FRAME_ID", "AWB_RESULT_CONTEXT_ID", "AWB_GLOBAL_COUNT") or name.startswith("AWB_GLOBAL_SUM_"):
            if not lease:
                return 0                # reads without a lease return zero
            b = lease["res"]
            if name == "AWB_FRAME_ID":
                return lease["fid"]
            if name == "AWB_RESULT_CONTEXT_ID":
                return lease["ctx"]
            if name == "AWB_GLOBAL_COUNT":
                return b["n"] & 0x7FFFFF
            v = b["sum"]["RGB".index(name[15])]
            return (v >> 32) & 7 if name.endswith("_H") else v & 0xFFFFFFFF
        if name.startswith("AWB_ZONE_") and name != "AWB_ZONE_ADDR":
            b = lease["res"] if lease else None
            z = regs.f("AWB_ZONE_ADDR", "zone_addr")
            if not (b and z < b["nx"] * b["ny"]):
                return 0
            if name == "AWB_ZONE_COUNT":
                return b["zcnt"][z] & 0x7FFFFF
            v = b["zsum"][z]["RGB".index(name[13])]
            return (v >> 32) & 7 if name.endswith("_H") else v & 0xFFFFFFFF
        if name == "AEC_CTRL":
            return regs.value["AEC_CTRL"] | (0x2 if self.aec_commit else 0)
        if name == "AEC_STATUS":
            return self.ro.get("AEC_STATUS", 0) | (1 if self.aec.filling is not None else 0)
        if name in self.ro:
            return self.ro[name]
        return regs.value[name]


# ---------------------------------------------------------------- readout script
def zone_sample(n, row):
    """Zone addresses read back: all of a small grid, a spread plus the edges of
    a large one, and one address beyond the grid (ALG-AEC-03 / ALG-AWB-04)."""
    zs = set(range(n)) if n <= 64 else set(range(0, n, max(1, n // 40))) | {row - 1, n - row, n - 1}
    return sorted(zs) + [n]


def readout_script(r):
    """Software reads of every statistics result (MAS §9.8): each READY result
    of every engine acquired in completion order, read in full through the
    wide indexed port and the narrow windows, then released. Reads without a
    lease come first (zero, NO_LEASE); TUNE_ERROR is checked and cleared last."""
    st, out = r.stats, []

    def rd(name):
        out.append("%s %d\n" % (name, st.read(r, name)))

    def sel(name, v):
        r.write(name, v)
        out.append("SET %s %d\n" % (name, v))

    def wide(prefix, index):
        sel(prefix + "_INDEX", index)
        rd(prefix + "_DATA_LO")
        rd(prefix + "_DATA_HI")

    # ---- AEC ----
    for n in ("AEC_CTRL", "AEC_STATUS", "TUNE_AEC_STATUS", "TUNE_AEC_DROPS", "AEC_FRAME_ID", "AEC_GLOBAL_SUM_LO"):
        rd(n)
    sel("TUNE_ERROR", 0xFF)
    while st.aec.ready:
        sel("TUNE_AEC_CMD", 1)                    # ACQUIRE
        res = st.aec.lease()["res"]
        for n in ("TUNE_AEC_STATUS", "AEC_FRAME_ID", "AEC_RESULT_CONTEXT_ID", "TUNE_AEC_FRAME", "TUNE_AEC_PROFILE",
                  "TUNE_AEC_RESULT_META", "TUNE_AE_FRAME_SIZE", "TUNE_AE_GRID", "TUNE_AE_ZONE_SIZE"):
            rd(n)
        for c in range(4):
            sel("AEC_CHANNEL_SEL", c)
            for n in ("AEC_GLOBAL_SUM_LO", "AEC_GLOBAL_SUM_HI", "AEC_GLOBAL_COUNT"):
                rd(n)
        sel("AEC_CHANNEL_SEL", 0)
        for i in list(range(9)) + [0x10, 0x11, 0x12] + list(range(0x100, 0x180)):
            wide("TUNE_AEC", i)
        nx = len(res["zones"]) and st.aec.lease()["meta"]["set"]["nx"]
        for z in zone_sample(len(res["zones"]), max(nx, 1)):
            for k in range(8):
                wide("TUNE_AEC", 0x200 + 8 * z + k)
            sel("AEC_ZONE_ADDR", z & 0x3FF)
            for n in ("AEC_ZONE_SUM", "AEC_ZONE_COUNT", "AEC_ZONE_GREEN_OE_UE", "AEC_ZONE_GREEN_MIN_MAX"):
                rd(n)
        for b in range(64):
            sel("AEC_HIST_ADDR", b)
            rd("AEC_HIST_DATA")
        wide("TUNE_AEC", 0x9)                     # reserved: zero and INVALID
        sel("TUNE_AEC_CMD", 2)                    # RELEASE
    rd("TUNE_AEC_STATUS")
    # ---- AWB (MAS §6.22.4.1) ----
    for n in ("AWB_STATUS", "AWB_RESULT_STATUS", "AWB_DROP_COUNT", "AWB_FRAME_ID", "AWB_GLOBAL_COUNT"):
        rd(n)
    while st.awb_ready:
        sel("AWB_RESULT_CTRL", 1)       # ACQUIRE
        rd("AWB_RESULT_STATUS")
        for n in ("AWB_FRAME_ID", "AWB_RESULT_CONTEXT_ID", "TUNE_AWB_PROFILE", "TUNE_AWB_FRAME_SIZE",
                  "AWB_GLOBAL_COUNT") + tuple("AWB_GLOBAL_SUM_%s_%s" % (c, hl) for c in "RGB" for hl in "HL"):
            rd(n)
        b = st._awb_lease()["res"]
        nx, ny = max(b["nx"], 1), max(b["ny"], 1)
        for z in zone_sample(nx * ny, nx):
            sel("AWB_ZONE_ADDR", z & 0x7FF)
            for n in ("AWB_ZONE_COUNT",) + tuple("AWB_ZONE_SUM_%s_%s" % (c, hl) for c in "RGB" for hl in "HL"):
                rd(n)
        sel("AWB_RESULT_CTRL", 2)       # RELEASE
    rd("AWB_RESULT_STATUS")
    # ---- AF ----
    for n in ("AF_STATUS", "TUNE_AF_STATUS", "TUNE_AF_DROPS", "AF_FRAME_ID"):
        rd(n)
    while st.af.ready:
        sel("TUNE_AF_CMD", 1)                     # ACQUIRE
        for n in ("AF_STATUS", "TUNE_AF_STATUS", "AF_FRAME_ID", "AF_RESULT_CONTEXT_ID", "TUNE_AF_FRAME",
                  "TUNE_AF_PROFILE"):
            rd(n)
        for z in range(16):
            sel("AF_STAT_ADDR", z)
            rd("AF_STAT_DATA")
        for i in [0, 0x10, 0x11, 0x12, 0x13] + list(range(0x100, 0x140)) + list(range(0x200, 0x220)):
            wide("TUNE_AF", i)
        wide("TUNE_AF", 0x14)                     # reserved: zero and INVALID
        sel("TUNE_AF_CMD", 2)                     # RELEASE
    rd("TUNE_AF_STATUS")
    rd("TUNE_ERROR")
    sel("TUNE_ERROR", 0xFF)
    return out
