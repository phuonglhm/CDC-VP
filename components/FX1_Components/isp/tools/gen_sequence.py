#!/usr/bin/env python3
"""Generate the continuous-operation sequence (gate G-CONT, DEC-36) from the Python reference.

A fixed-seed script of bursts. Before each burst, software changes the
configuration while the pipeline is idle (random deltas over every block,
statistics, geometry and Bayer order). The burst then submits 1-3 frames
through the 4-buffer rotation, optionally with one event:

  none, underrun, overflow  every frame completes (DEC-18 stall is lossless)
  soft_reset_common/_dma    soft reset in the middle of the first frame (DEC-14):
                            that frame had its SOF only; rotation returns to 0
                            and software resubmits the whole burst
  idma_axi_error            read error in the middle of the first frame (DEC-19):
                            SOF only; software re-arms the same input buffer
  odma_axi_error            write error in the first frame's output (DEC-19): the
                            pipeline completed it (EOF effects happen) but it is
                            not DONE; the next frames stall on the output buffer
                            until software frees it again; the frame is
                            resubmitted at the end of the burst
  counter_wrap (before a burst) FRAME_ID counter and frame counters near 2^32

The reference processes the frames in the order the pipeline sees them,
including the effects of aborted frames. tests/integration/test_cont_tlm.cpp
replays the script over TLM, compares every completed frame bit for bit, and
compares the statistics and frame counters after every burst.

Output: tests/data/sequences/cont_100/ with sequence.txt, in_*.bin (LE uint16
containers), exp_*.bin (NV12) and stats_*.txt (readout scripts).
--check verifies the committed files are up to date.
"""

import argparse
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "reference"))
from fx1_isp_ref import blocks, regs as regs_mod  # noqa: E402
from fx1_isp_ref.stats import readout_script  # noqa: E402

OUT = os.path.join(HERE, "..", "tests", "data", "sequences", "cont_100")
SEED = 20261001
TARGET_FRAMES = 100
M32 = 0xFFFFFFFF
# (width, height, Bayer code); the last one exercises the Resizer (VGA).
GEOMETRIES = [(64, 48, 0), (66, 34, 3), (96, 64, 1), (130, 74, 2)]
RESIZE_GEOMETRY = (644, 484, 0)


def gamma_lut(preset):
    def f(v):
        if preset == 0:
            return 4.5 * v if v < 0.018 else 1.099 * v ** 0.45 - 0.099
        return v ** (1.0 / (1.8 + 0.2 * preset))
    return [min(4095, max(0, math.floor(4095 * f(i / 255) + 0.5))) for i in range(256)]


def frame_content(w, h, rng):
    y, x = np.mgrid[0:h, 0:w]
    img = np.full((h, w), int(rng.integers(300, 1500)), dtype=np.int64)
    img[:, int(rng.integers(0, w)):] += int(rng.integers(500, 2000))
    img[(x + y) % int(rng.integers(6, 20)) < 4] += int(rng.integers(100, 600))
    img += (x * int(rng.integers(0, 2000)) // max(w - 1, 1))
    img += rng.normal(0, rng.uniform(5, 120), size=(h, w)).astype(np.int64)
    for _ in range(int(rng.integers(0, 8))):            # a few defects for BPC
        img[rng.integers(0, h), rng.integers(0, w)] = int(rng.choice([0, 4095]))
    return np.clip(img, 0, 4095).astype(np.uint16)


class Generator:
    def __init__(self):
        self.rng = np.random.default_rng(SEED)
        self.regs = regs_mod.Registers()
        self.lines = []
        self.files = {}
        self.n_in = self.n_exp = self.n_stats = 0
        self.idma_count = self.odma_count = 0
        self.submitted = 0
        self.geom = GEOMETRIES[0]
        self.lsc_active = 0
        self.lsc_nodes = None

    # -- helpers -------------------------------------------------------------
    def write(self, name, value):
        self.regs.write(name, value)
        self.lines.append("write %s 0x%X" % (name, value))

    def ri(self, lo, hi):
        return int(self.rng.integers(lo, hi + 1))

    def add_file(self, name, data):
        self.files[name] = data
        return name

    # -- configuration deltas (pipeline idle) --------------------------------
    def set_geometry(self, g):
        self.geom = g
        w, h, b = g
        self.write("COMMON_FRAME_WIDTH", w)
        self.write("COMMON_FRAME_HEIGHT", h)
        self.write("COMMON_BAYER", b)

    def lsc_load(self):
        nx, ny = self.ri(2, 6), self.ri(2, 5)
        if (nx, ny) == self.lsc_nodes:
            nx = nx + 1
        self.lsc_nodes = (nx, ny)
        dest = (self.lsc_active + 1) % 3         # never the active profile (ALG-LSC-05)
        self.write("LSC_MESH_NODES", nx | (ny << 8))  # invalidates every profile
        self.write("LSC_LOAD_CTRL", dest | 0x100)
        for _ in range(4 * nx * ny):
            self.write("LSC_COEF_DATA", self.ri(0x30000, 0x70000))
        self.write("LSC_LOAD_CTRL", dest | 0x200)
        self.write("LSC_PROFILE_SEL", dest)
        self.lsc_active = dest
        self.write("LSC_STRENGTH", self.ri(0x4000, 0x14000))
        self.write("LSC_CTRL", 1)

    def delta(self, kind):
        r = self.ri
        if kind == "wb":
            self.write("WB_CTRL", 1)
            self.write("WB_GAIN_R", r(0xC0, 0x220))
            self.write("WB_GAIN_B", r(0xC0, 0x220))
        elif kind == "dg":
            self.write("DG_CTRL", r(0, 1))
            self.write("DG_GAIN", r(0xA0, 0x180))
        elif kind == "blc":
            ped = r(64, 300)
            self.write("BLC_CTRL", r(0, 1))
            self.write("BLC_OFS_G0_DFT", ped)
            self.write("BLC_SCALE_G0", round(4095 / (4095 - ped) * 65536))
        elif kind == "ccm":
            if r(0, 2) == 0:                      # coefficients without `updated`: gated (DEC-24)
                self.write("CCM_CTRL", 1)
            names = ["CRR", "CRG", "CRB", "CGR", "CGG", "CGB", "CBR", "CBG", "CBB"]
            for i, n in enumerate(names):
                v = (1.0 if i % 4 == 0 else 0.0) + self.rng.uniform(-0.3, 0.3)
                self.write("CCM_" + n, int(round(v * 512)) & 0xFFF)
            if r(0, 2):
                self.write("CCM_CTRL", 0x5)
        elif kind == "gamma":
            if r(0, 3) == 0:
                self.write("GAMMA_CTRL", 0)
            else:
                self.write("GAMMA_LUT_ADDR", 0)
                for v in gamma_lut(r(0, 3)):
                    self.write("GAMMA_LUT_DATA", v)
                self.write("GAMMA_CTRL", 1)
        elif kind == "csc":
            self.write("CSC_CTRL", r(0, 1))
        elif kind == "gtm":
            m = r(0, 2)
            if m == 0:
                self.write("GTM_CTRL", 0)
            elif m == 1:
                self.write("GTM_KEY", r(0x40, 0x100))
                self.write("GTM_LWHITE", r(0x100, 0x400))
                self.write("GTM_ROI_LOG2", r(3, 11))
                self.write("GTM_CTRL", 1)
            else:
                self.write("GTM_CTRL", 2)
                self.write("GTM_LUT_ADDR", 0)
                for i in range(65):
                    self.write("GTM_LUT_DATA", r(0xC0, 0x180))
                self.write("GTM_CTRL", 3)
        elif kind == "nr":
            self.write("NR_2D_CTRL", r(0, 1))
        elif kind == "ee":
            self.write("EE_ALPHA", r(0, 0xFFFF))
            self.write("EE_BETA", r(0, 0xFFFF))
            self.write("EE_CLAMP", (r(0, 255) << 8) | r(0, 255))
            self.write("EE_CTRL", r(0, 1))
        elif kind == "cnf":
            self.write("CNF_CHROMA_TH", r(0, 60))
            self.write("CNF_LUMA_TH", r(0, 60))
            self.write("CNF_CTRL", r(0, 1) | (4 if r(0, 2) else 0))
        elif kind == "bpc":
            self.write("BPC_MODE", 0)
            self.write("BPC_THRESH", (r(20, 120) << 16) | r(200, 600))
            self.write("BPC_CTRL", r(0, 1))
        elif kind == "lsc":
            if r(0, 3) == 0:
                self.write("LSC_CTRL", 0)
            else:
                self.lsc_load()
        elif kind == "aec":
            legal = r(0, 4) != 0
            nx, ny = (r(1, 32), r(1, 24)) if legal else (r(0, 63) if r(0, 1) else 0, r(25, 31))
            self.write("AEC_ZONE_CFG", nx | (ny << 8))
            self.write("AEC_ZONE_SIZE", r(1, 40) | (r(1, 30) << 16))
            mn = r(0, 600)
            self.write("AEC_SAMPLE_CLIP", mn | (r(mn, 4095) << 16))
            self.write("AEC_THRESH", r(0, 1500) | (r(2000, 4095) << 16))
            self.write("AEC_CONTEXT_ID", r(0, M32))
            self.write("AEC_CTRL", [3, 3, 1, 2][r(0, 3)])     # commit, en without commit, commit en=0
        elif kind == "awb":
            self.write("AWB_UNDEREXPOSED_LIMIT", r(0, 400))
            self.write("AWB_SATURATION_LIMIT", r(3000, 4095))
            self.write("AWB_CONTEXT_ID", r(0, M32))
            self.write("AWB_CTRL", r(0, 1) | (r(0, 70) << 1) | (r(0, 35) << 8))
        elif kind == "af":
            self.write("AF_CONTEXT_ID", r(0, M32))
            self.write("AF_CTRL", r(0, 1))
        elif kind == "geometry":
            self.set_geometry(GEOMETRIES[r(0, len(GEOMETRIES) - 1)])
        elif kind == "resizer":
            self.write("RESIZER_CTRL", r(0, 1) | 0x4 | (r(0, 15) << 3))

    # -- frames ---------------------------------------------------------------
    def new_input(self):
        w, h, _ = self.geom
        raw = frame_content(w, h, self.rng)
        name = self.add_file("in_%03d.bin" % self.n_in, raw.astype("<u2").tobytes())
        self.n_in += 1
        return name, raw

    def done(self, raw):
        y, uv = blocks.run_frame(raw, self.regs, {})
        name = self.add_file("exp_%03d.bin" % self.n_exp, y.tobytes() + uv.tobytes())
        self.n_exp += 1
        self.idma_count = (self.idma_count + 1) & M32
        self.odma_count = (self.odma_count + 1) & M32
        self.lines.append("expect %s %d %d" % (name, y.shape[1], y.shape[0]))

    def burst(self, n, event):
        inputs = [self.new_input() for _ in range(n)]
        w, h, _ = self.geom
        self.lines.append("burst %d %s %d %d" % (n, event, w, h))
        for name, _ in inputs:
            self.lines.append("input %s" % name)
        self.submitted += n
        raws = [raw for _, raw in inputs]
        if event.startswith("soft_reset"):
            blocks.frame_sof(self.regs)              # first frame: SOF, then the reset
            self.regs.soft_reset()
            for raw in raws:                         # resubmitted from rotation slot 0
                self.done(raw)
        elif event == "idma_axi_error":
            blocks.frame_sof(self.regs)
            for raw in raws:                         # the same buffer re-armed, then the rest
                self.done(raw)
        elif event == "odma_axi_error":
            st = {}
            blocks.frame_sof(self.regs)
            blocks.run(raws[0], self.regs, st)
            blocks.frame_eof(self.regs, st)          # completed by the pipeline, not DONE
            self.idma_count = (self.idma_count + 1) & M32
            for raw in raws[1:] + raws[:1]:          # the rest, then the resubmitted frame
                self.done(raw)
        else:
            for raw in raws:
                self.done(raw)
        name = self.add_file("stats_%03d.txt" % self.n_stats, "".join(readout_script(self.regs)).encode())
        self.n_stats += 1
        self.lines.append("end %s %d %d" % (name, self.idma_count, self.odma_count))

    # -- script -----------------------------------------------------------------
    def base_config(self):
        self.set_geometry(GEOMETRIES[0])
        for k in ("blc", "wb", "dg", "gamma", "csc", "bpc", "lsc", "gtm", "nr", "ee", "cnf"):
            self.delta(k)
        self.write("AEC_ZONE_CFG", 8 | (6 << 8))
        self.write("AEC_ZONE_SIZE", 8 | (8 << 16))
        self.write("AEC_CTRL", 3)
        self.write("AWB_CTRL", 1 | (4 << 1) | (4 << 8))
        self.write("AF_CTRL", 1)

    def generate(self):
        self.base_config()
        events = (["soft_reset_common"] * 4 + ["soft_reset_dma"] * 4 + ["idma_axi_error"] * 5 +
                  ["odma_axi_error"] * 5 + ["overflow"] * 5 + ["underrun"] * 5)
        kinds = ["wb", "dg", "blc", "ccm", "gamma", "csc", "gtm", "nr", "ee", "cnf", "bpc", "lsc", "aec", "awb",
                 "af", "geometry", "resizer"]
        plan = []
        while sum(p[0] for p in plan) < TARGET_FRAMES:
            plan.append([self.ri(1, 3), "none"])
        for e, i in zip(events, self.rng.choice(np.arange(1, len(plan)), size=len(events), replace=False)):
            plan[i][1] = e
            if e == "overflow" and plan[i][0] < 2:
                plan[i][0] = 2
        wrap_at = len(plan) // 2
        step = len(plan) // (2 * len(GEOMETRIES))
        coverage = {step * (2 * k + 1): k for k in range(len(GEOMETRIES))}  # forced geometry changes
        resize_at = len(plan) // 3
        for i, (n, e) in enumerate(plan):
            if i:
                for k in self.rng.choice(kinds, size=self.ri(1, 3), replace=False):
                    self.delta(str(k))
            if i in coverage:                        # every geometry / Bayer order, not left to chance
                self.set_geometry(GEOMETRIES[coverage[i]])
            if i == resize_at:                       # one burst at a size the Resizer modes fit
                self.set_geometry(RESIZE_GEOMETRY)
                self.write("RESIZER_CTRL", 0x1 | 0x4 | (14 << 3))
            elif self.geom == RESIZE_GEOMETRY:
                self.set_geometry(GEOMETRIES[0])
            if i == wrap_at:
                v = M32 - 1                          # FRAME_ID and frame counters wrap in this burst
                self.regs.stats.counter = v
                self.idma_count = self.odma_count = v
                self.lines.append("counter 0x%X" % v)
            self.burst(n, e)
        header = ["# generated by tools/gen_sequence.py (seed %d): %d bursts, %d frames submitted, %d expected" %
                  (SEED, len(plan), self.submitted, self.n_exp)]
        self.files["sequence.txt"] = ("\n".join(header + self.lines) + "\n").encode()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    g = Generator()
    g.generate()
    os.makedirs(OUT, exist_ok=True)
    stale = []
    for fn, data in sorted(g.files.items()):
        p = os.path.join(OUT, fn)
        if a.check:
            if not os.path.exists(p) or open(p, "rb").read() != data:
                stale.append(p)
        else:
            open(p, "wb").write(data)
    if not a.check:
        for fn in os.listdir(OUT):
            if fn not in g.files:
                os.remove(os.path.join(OUT, fn))
    if stale:
        sys.exit("gen_sequence.py: out of date:\n  " + "\n  ".join(stale[:10]))
    print("%s sequence: %d files, %d frames submitted, %d expected outputs" %
          ("checked" if a.check else "wrote", len(g.files), g.submitted, g.n_exp))


if __name__ == "__main__":
    main()
