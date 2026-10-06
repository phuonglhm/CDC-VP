"""Frame-based reference of the FX1 ISP statistics generators and of their
software-visible publication: AEC (HAS §6.21), AWB (§6.22), AF (§6.23), §9.5.

The measurement functions work on whole frames with numpy; `StatsState`
holds what the register interface shows (frame counter, AEC shadow/active
sets, published results and the zone/histogram memories) and answers reads
of the statistics registers. Rules follow plan/alg/ALG_F (ALG-AEC-*,
ALG-AWB-*, ALG-AF-*, ALG-STAT-*), DEC-29 and DEC-30.

The reference is frame based: it is read only at frame boundaries, where the
single zone/histogram memory (DEC-30) holds the last enabled frame complete.
The partial sums visible during a frame are checked by the C++ directed tests
(tests/unit/test_stats.cpp) and the TLM probe in test_e2e_tlm.cpp.
"""

import numpy as np

I64 = np.int64


# ---------------------------------------------------------------- measurement
def aec_measure(p, c):
    """p: post-BPC RGGB Bayer [H, W]; c: active AEC set (dict)."""
    h, w = p.shape
    yy, xx = np.mgrid[0:h, 0:w]
    ch = ((yy & 1) << 1) | (xx & 1)                       # 0 R, 1 Gr, 2 Gb, 3 B
    q = (p >= c["min"]) & (p <= c["max"])                 # clip, inclusive
    green = (ch == 1) | (ch == 2)
    res = {
        "gsum": [int(p[q & (ch == k)].sum()) for k in range(4)],
        "gcnt": [int((q & (ch == k)).sum()) for k in range(4)],
        "hist": np.bincount((p[green] >> 6).ravel(), minlength=64).tolist(),
        "nx": c["nx"], "ny": c["ny"],
        "zones": 1 <= c["nx"] <= 32 and 1 <= c["ny"] <= 24 and c["zw"] > 0 and c["zh"] > 0,
    }
    if res["zones"]:
        n = c["nx"] * c["ny"]
        z = np.minimum(yy // c["zh"], c["ny"] - 1) * c["nx"] + np.minimum(xx // c["zw"], c["nx"] - 1)
        zsum = np.zeros((n, 4), I64)
        zcnt = np.zeros((n, 4), I64)
        for k in range(4):
            m = q & (ch == k)
            np.add.at(zsum[:, k], z[m], p[m])
            np.add.at(zcnt[:, k], z[m], 1)
        zg, pg = z[green], p[green]
        gmin = np.full(n, 4095, I64)
        gmax = np.zeros(n, I64)
        np.minimum.at(gmin, zg, pg)
        np.maximum.at(gmax, zg, pg)
        res.update(zsum=zsum.tolist(), zcnt=zcnt.tolist(),
                   oe=np.bincount(zg[pg > c["th_oe"]], minlength=n).tolist(),
                   ue=np.bincount(zg[pg < c["th_ue"]], minlength=n).tolist(),
                   gmin=gmin.tolist(), gmax=gmax.tolist())
    return res


def awb_measure(rgb, c):
    """rgb: post-Demosaic [H, W, 3]; c: AWB set sampled at SOF (dict)."""
    h, w, _ = rgb.shape
    ok = np.all((rgb > c["under"]) & (rgb < c["sat"]), axis=-1)   # strict at both limits
    res = {"sum": [int(rgb[..., k][ok].sum()) for k in range(3)], "n": int(ok.sum()),
           "nx": c["nx"], "ny": c["ny"], "zones": 1 <= c["nx"] <= 64 and 1 <= c["ny"] <= 32}
    if res["zones"]:
        nx, ny = c["nx"], c["ny"]
        zx = np.minimum(((np.arange(w, dtype=I64) + 1) * nx - 1) // w, nx - 1)
        zy = np.minimum(((np.arange(h, dtype=I64) + 1) * ny - 1) // h, ny - 1)
        z = (zy[:, None] * nx + zx[None, :])[ok]
        zsum = np.zeros((nx * ny, 3), I64)
        for k in range(3):
            np.add.at(zsum[:, k], z, rgb[..., k][ok])
        res.update(zsum=zsum.tolist(), zcnt=np.bincount(z, minlength=nx * ny).tolist())
    return res


def af_measure(y):
    """y: CSC luma [H, W]: Sobel |gx| + |gy|, replicate borders, 4x4 zones."""
    h, w = y.shape
    e = np.pad(y.astype(I64), 1, mode="edge")
    def s(dy, dx):
        return e[1 + dy:1 + dy + h, 1 + dx:1 + dx + w]
    gx = (s(-1, 1) + 2 * s(0, 1) + s(1, 1)) - (s(-1, -1) + 2 * s(0, -1) + s(1, -1))
    gy = (s(1, -1) + 2 * s(1, 0) + s(1, 1)) - (s(-1, -1) + 2 * s(-1, 0) + s(-1, 1))
    d = np.abs(gx) + np.abs(gy)
    z = (np.arange(h)[:, None] * 4 // h) * 4 + (np.arange(w)[None, :] * 4 // w)
    fv = np.zeros(16, I64)
    np.add.at(fv, z, d)
    return [int(v) & 0xFFFFFFFF for v in fv]


# ---------------------------------------------------------------- register view
class StatsState:
    def __init__(self):
        self.counter = 0                # frames started since reset (ALG-STAT-01)
        self.fid = 0
        self.aec_commit = False
        self.aec_active = {"en": 0, "nx": 32, "ny": 24, "zw": 120, "zh": 90, "min": 0, "max": 4095,
                           "th_ue": 256, "th_oe": 3840}
        self.awb_sof = None
        self.af_sof = 0
        self.ctx = {}
        self.aec = None                 # published results
        self.awb = None
        self.af = [0] * 16
        self.aec_live = self.awb_live = False   # memory restarted by an enabled SOF since reset
        self.ro = {}                    # published register values

    def soft_reset(self):
        """DEC-14 / ALG-STAT-07: results, done bits and context tags cleared,
        memories read the reset value until the next enabled SOF, a pending
        AEC commit is dropped (DEC-32); FRAME_IDs, the counter and the active
        configuration are kept."""
        self.aec = self.awb = None
        self.af = [0] * 16
        self.aec_live = self.awb_live = False
        self.aec_commit = False
        self.ro = {k: v for k, v in self.ro.items() if k.endswith("_FRAME_ID")}

    def sof(self, regs):
        self.counter = (self.counter + 1) & 0xFFFFFFFF
        self.fid = self.counter
        if self.aec_commit:             # shadow -> active (#296)
            self.aec_active = {
                "en": regs.f("AEC_CTRL", "en"),
                "nx": regs.f("AEC_ZONE_CFG", "num_zone_x"), "ny": regs.f("AEC_ZONE_CFG", "num_zone_y"),
                "zw": regs.f("AEC_ZONE_SIZE", "zone_width"), "zh": regs.f("AEC_ZONE_SIZE", "zone_height"),
                "min": regs.f("AEC_SAMPLE_CLIP", "sample_min_clip"),
                "max": regs.f("AEC_SAMPLE_CLIP", "sample_max_clip"),
                "th_ue": regs.f("AEC_THRESH", "th_ue"), "th_oe": regs.f("AEC_THRESH", "th_oe")}
            self.aec_commit = False
        self.awb_sof = {"en": regs.f("AWB_CTRL", "en"), "nx": regs.f("AWB_CTRL", "num_zone_x"),
                        "ny": regs.f("AWB_CTRL", "num_zone_y"),
                        "under": regs.f("AWB_UNDEREXPOSED_LIMIT", "underexposed_limit"),
                        "sat": regs.f("AWB_SATURATION_LIMIT", "saturation_limit")}
        self.af_sof = regs.f("AF_CTRL", "en")
        self.ctx = {b: regs.value[b + "_CONTEXT_ID"] for b in ("AEC", "AWB", "AF")}
        if self.aec_active["en"]:
            self.aec_live = True        # single memory restarted (DEC-30)
        if self.awb_sof["en"]:
            self.awb_live = True

    def eof(self, regs, taps):
        if self.aec_active["en"]:
            self.aec = aec_measure(taps["aec"], self.aec_active)
            self._publish("AEC", "AEC_STATUS", 0x2)
        if self.awb_sof["en"]:
            self.awb = awb_measure(taps["awb"], self.awb_sof)
            for k, c in enumerate("RGB"):
                v = self.awb["sum"][k]
                self.ro["AWB_GLOBAL_SUM_%s_H" % c] = (v >> 32) & 7
                self.ro["AWB_GLOBAL_SUM_%s_L" % c] = v & 0xFFFFFFFF
            self.ro["AWB_GLOBAL_COUNT"] = self.awb["n"] & 0x7FFFFF
            self._publish("AWB", "AWB_STATUS", 0x1)
        if regs.f("AF_CTRL", "en"):     # publication iff EN at EOF (ALG-AF-01)
            self.af = af_measure(taps["af"])
            full = 0x20 if self.af_sof else 0   # score_valid (ALG-AF-02)
            self.ro["AF_STATUS"] = (self.ro.get("AF_STATUS", 0) & ~0x20) | full
            self._publish("AF", "AF_STATUS", 0x2)

    def _publish(self, blk, status, done):
        self.ro[blk + "_FRAME_ID"] = self.fid
        self.ro[blk + "_RESULT_CONTEXT_ID"] = self.ctx[blk]
        self.ro[status] = self.ro.get(status, 0) | done

    # Reads of the statistics registers, as software sees them after the frame.
    def read(self, regs, name):
        sel = regs.f("AEC_CHANNEL_SEL", "channel_sel")
        a = self.aec
        if name in ("AEC_GLOBAL_SUM_LO", "AEC_GLOBAL_SUM_HI", "AEC_GLOBAL_COUNT"):
            if a is None:
                return 0
            return {"AEC_GLOBAL_SUM_LO": a["gsum"][sel] & 0xFFFFFFFF,
                    "AEC_GLOBAL_SUM_HI": (a["gsum"][sel] >> 32) & 1,
                    "AEC_GLOBAL_COUNT": a["gcnt"][sel] & 0x1FFFFF}[name]
        if name.startswith("AEC_ZONE_") and name != "AEC_ZONE_ADDR":
            z = regs.f("AEC_ZONE_ADDR", "zone_addr")
            if not self.aec_live or a is None:
                return 0                # CSR reset value before the first enabled SOF
            inside = a["zones"] and z < a["nx"] * a["ny"]
            if name == "AEC_ZONE_GREEN_MIN_MAX":
                return (a["gmax"][z] << 12) | a["gmin"][z] if inside else 4095
            if not inside:
                return 0
            return {"AEC_ZONE_SUM": a["zsum"][z][sel] & 0x0FFFFFFF,
                    "AEC_ZONE_COUNT": a["zcnt"][z][sel] & 0xFFFF,
                    "AEC_ZONE_GREEN_OE_UE": ((a["oe"][z] & 0xFFFF) << 16) | (a["ue"][z] & 0xFFFF)}[name]
        if name == "AEC_HIST_DATA":
            return a["hist"][regs.f("AEC_HIST_ADDR", "hist_addr")] & 0x3FFFFF if a and self.aec_live else 0
        if name.startswith("AWB_ZONE_") and name != "AWB_ZONE_ADDR":
            b = self.awb
            z = regs.f("AWB_ZONE_ADDR", "zone_addr")
            if not (b and self.awb_live and b["zones"] and z < b["nx"] * b["ny"]):
                return 0
            if name == "AWB_ZONE_COUNT":
                return b["zcnt"][z] & 0x7FFFFF
            v = b["zsum"][z]["RGB".index(name[13])]
            return (v >> 32) & 7 if name.endswith("_H") else v & 0xFFFFFFFF
        if name == "AF_STAT_DATA":
            return self.af[regs.f("AF_STAT_ADDR", "zone_addr")]
        if name == "AEC_CTRL":
            return regs.value["AEC_CTRL"] | (0x2 if self.aec_commit else 0)
        if name in self.ro:
            return self.ro[name]
        return regs.value[name]


# ---------------------------------------------------------------- readout script
def zone_sample(n, row):
    """Zone addresses read back: all of a small grid, a spread plus the edges of
    a large one, and two addresses beyond the grid (ALG-AEC-03 / ALG-AWB-04)."""
    zs = set(range(n)) if n <= 64 else set(range(0, n, max(1, n // 40))) | {row - 1, n - row, n - 1}
    return sorted(zs) + [n, 2 ** 20]


def readout_script(r):
    """Software reads of every statistics result, through the readout muxes."""
    st, out = r.stats, []

    def rd(name):
        out.append("%s %d\n" % (name, st.read(r, name)))

    def sel(name, v):
        r.write(name, v)
        out.append("SET %s %d\n" % (name, v))

    for n in ("AEC_CTRL", "AEC_STATUS", "AEC_FRAME_ID", "AEC_RESULT_CONTEXT_ID"):
        rd(n)
    for c in range(4):
        sel("AEC_CHANNEL_SEL", c)
        for n in ("AEC_GLOBAL_SUM_LO", "AEC_GLOBAL_SUM_HI", "AEC_GLOBAL_COUNT"):
            rd(n)
    a = st.aec
    if a is not None:
        nx, ny = max(a["nx"], 1), max(a["ny"], 1)
        for z in zone_sample(nx * ny, nx):
            sel("AEC_ZONE_ADDR", z & 0x3FF)
            rd("AEC_ZONE_GREEN_OE_UE")
            rd("AEC_ZONE_GREEN_MIN_MAX")
            for c in range(4):
                sel("AEC_CHANNEL_SEL", c)
                rd("AEC_ZONE_SUM")
                rd("AEC_ZONE_COUNT")
    for b in range(64):
        sel("AEC_HIST_ADDR", b)
        rd("AEC_HIST_DATA")
    for n in ("AWB_STATUS", "AWB_FRAME_ID", "AWB_RESULT_CONTEXT_ID", "AWB_GLOBAL_COUNT") + tuple(
            "AWB_GLOBAL_SUM_%s_%s" % (c, hl) for c in "RGB" for hl in "HL"):
        rd(n)
    b = st.awb
    if b is not None:
        nx, ny = max(b["nx"], 1), max(b["ny"], 1)
        for z in zone_sample(nx * ny, nx):
            sel("AWB_ZONE_ADDR", z & 0x7FF)
            for n in ("AWB_ZONE_COUNT",) + tuple("AWB_ZONE_SUM_%s_%s" % (c, hl) for c in "RGB" for hl in "HL"):
                rd(n)
    for n in ("AF_STATUS", "AF_FRAME_ID", "AF_RESULT_CONTEXT_ID"):
        rd(n)
    for z in range(16):
        sel("AF_STAT_ADDR", z)
        rd("AF_STAT_DATA")
    return out
