#!/usr/bin/env python3
"""Generate the FX1 platform ISP fixture (plan P4, stage G4a) from the pinned
Python reference of components/FX1_Components/isp.

Writes isp_fixture.h next to this script:
  * the input geometry (64x48 RGGB) and frame count (8);
  * two CSR write lists (profiles), as ISP-relative offsets:
      basic     tools/make_profile.py preset "basic" (BLC, grey-world WB from
                frame 0, Gamma BT.709, CSC BT.601);
      gtm_auto  basic + GTM in automatic mode (key 0xBB, Lwhite 0x180,
                ROI log2 11), whose curve depends on the previous frames;
  * the CRC-32 (IEEE, as zlib.crc32) of every expected NV12 frame (Y then UV,
    active bytes, strides = width) for both profiles, frames processed in
    order through the reference so temporal state carries over;
  * a third profile, stats = basic + AEC (4x3 zones of 16x16), AWB (one zone
    over the frame) and AF, with the publication every frame must produce: AEC/AWB/AF
    FRAME_ID and RESULT_CONTEXT_ID, the AEC global Bayer sum and count (MAS v1.07
    Table 9-193: one aggregate, no per-CFA sums), all read under a lease;
    Statistics do not change the image, so its NV12 CRCs equal basic's.
  * the AWB ownership script (CSR/MAS 07/10, MAS §6.22.4): the stats profile
    run one frame at a time with REPORT / ACQUIRE / RELEASE / CLEAR_REPORT
    steps after given frames, so results are dropped when software holds
    both banks; every step's expected status, identity and global sums;
  * a resize case (MAS §6.20.4): 648x368 RGGB (raw_sample_rz), its own basic
    profile, and the CRCs of three frames: scaled to 640x360 from the first
    frame, then pass-through, then 640x360 again (mode switched between
    frames; v1.07: with the stream stopped, RESIZER_CTRL is IDLE class).

The RAW frames are not stored: raw_sample() below is an integer formula that
the firmware evaluates identically (isp_stream/main.c, raw_sample()).

  python3 gen_fixture.py           regenerate isp_fixture.h
  python3 gen_fixture.py --check   exit 1 if isp_fixture.h is out of date

Requires numpy. Not run at build or test time: the header is committed.
"""

import argparse
import hashlib
import os
import sys
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ISP = os.path.normpath(os.path.join(HERE, "..", "..", "..", "..", "components", "FX1_Components", "isp"))
sys.path.insert(0, os.path.join(ISP, "reference"))
sys.path.insert(0, os.path.join(ISP, "tools"))
from fx1_isp_ref import blocks, regs as regs_mod  # noqa: E402
import make_profile  # noqa: E402

WIDTH, HEIGHT, BAYER, FRAMES = 64, 48, "RGGB", 8
OUT = os.path.join(HERE, "isp_fixture.h")
GTM_AUTO = [("GTM_KEY", 0xBB), ("GTM_LWHITE", 0x0180), ("GTM_ROI_LOG2", 11), ("GTM_CTRL", 1)]
# Statistics on, small and legal: AEC 4x3 zones of 16x16 covering the frame,
# AWB one 1x1 zone (a 0x0 grid is illegal since CSR/MAS 07/10, MAS §6.22.3.7),
# AF; distinct context IDs so the tags are checked too.
STATS = (make_profile.aec(4, 3, 16, 16) + make_profile.awb(1, 1, 16, 4000) + [("AF_CTRL", 1)] +
         [("AEC_CONTEXT_ID", 0xA1), ("AWB_CONTEXT_ID", 0xA2), ("AF_CONTEXT_ID", 0xA3)])


def raw_sample(frame, x, y):
    """12-bit RAW sample; must match raw_sample() in main.c exactly (uint32 math)."""
    v = 400 + 160 * frame                         # exposure ramp: GTM sees a changing scene
    v += (x * 37 + y * 91 + frame * 53) % 512     # gradient texture
    v += ((x ^ y) & 7) * 24                       # fine pattern
    if (x + 2 * frame) % 16 < 3:                  # moving bright bars
        v += 900
    if (x * 7 + y * 13 + frame) % 211 == 0:       # sparse defects (hot pixels)
        v = 4095
    return min(v, 4095)


def raw_frame(frame):
    return np.array([[raw_sample(frame, x, y) for x in range(WIDTH)] for y in range(HEIGHT)],
                    dtype=np.uint16)


# Resize case: the smallest frame a Resizer mode fits (640x360 inside 648x368).
RZ_WIDTH, RZ_HEIGHT, RZ_FRAMES = 648, 368, 3
RZ_SCALED = 1 | (8 << 3)   # RESIZER_CTRL: en, scale 8 = 640x360
RZ_MODES = [RZ_SCALED, 0, RZ_SCALED]   # per frame: scaled, pass-through, scaled


def raw_sample_rz(frame, x, y):
    """Cheaper 12-bit sample for the resize frames (no division: the CPU
    evaluates 238k of them); must match isp_raw_sample_rz() in isp_mas0710.c."""
    v = 400 + 160 * frame + ((x * 37 + y * 91 + frame * 53) & 511) + ((x ^ y) & 7) * 24
    if ((x + 2 * frame) & 15) < 3:
        v += 900
    return min(v, 4095)


def raw_frame_rz(frame):
    x = np.arange(RZ_WIDTH, dtype=np.int64)[None, :]
    y = np.arange(RZ_HEIGHT, dtype=np.int64)[:, None]
    v = 400 + 160 * frame + ((x * 37 + y * 91 + frame * 53) & 511) + ((x ^ y) & 7) * 24
    v = v + np.where(((x + 2 * frame) & 15) < 3, 900, 0)
    return np.minimum(v, 4095).astype(np.uint16)


def run_resize(profile, raws):
    regs = regs_mod.Registers()
    regs.write("COMMON_FRAME_WIDTH", RZ_WIDTH)
    regs.write("COMMON_FRAME_HEIGHT", RZ_HEIGHT)
    for name, value in profile:
        regs.write(name, value)
    crcs = []
    for raw, mode in zip(raws, RZ_MODES):
        regs.write("RESIZER_CTRL", mode)
        y, uv = blocks.run_frame(raw, regs, {})
        want = (360, 640) if mode else (RZ_HEIGHT, RZ_WIDTH)
        assert y.shape == want and uv.shape == (want[0] // 2, want[1]), (y.shape, uv.shape)
        crcs.append(zlib.crc32(y.astype(np.uint8).tobytes() + uv.astype(np.uint8).tobytes()) & 0xFFFFFFFF)
    return crcs


# AWB ownership script: after frame k, the steps OWN_SCRIPT[k] in order.
#   R report (ready count, leased, overrun, drop count)   A acquire, expect a result
#   N acquire, expect none   L release   C clear the report
OWN_SCRIPT = {0: "R", 1: "R", 2: "R", 3: "RALALNCR", 4: "A", 5: "R", 6: "RLAL", 7: "ALR"}
OWN_OPS = {"R": 0, "A": 1, "N": 2, "L": 3, "C": 4}


def run_ownership(profile, raws):
    regs = regs_mod.Registers()
    regs.write("COMMON_FRAME_WIDTH", WIDTH)
    regs.write("COMMON_FRAME_HEIGHT", HEIGHT)
    for name, value in profile:
        regs.write(name, value)
    st, steps = regs.stats, []
    for k, raw in enumerate(raws):
        blocks.run_frame(raw, regs, {})
        for op in OWN_SCRIPT.get(k, ""):
            e = {"frame": k, "op": OWN_OPS[op], "ready": 0, "leased": 0, "overrun": 0, "drops": 0,
                 "fid": 0, "ctx": 0, "sum": [0, 0, 0], "count": 0}
            if op == "R":
                e.update(ready=len(st.awb_ready), leased=int(st.awb_leased is not None),
                         overrun=int(st.awb_overrun), drops=st.awb_drops)
            elif op in "AN":
                had = st.awb_leased
                regs.write("AWB_RESULT_CTRL", 1)
                assert (st.awb_leased is not None and had is None) == (op == "A"), (k, op)
                if op == "A":
                    e.update(fid=st.read(regs, "AWB_FRAME_ID"), ctx=st.read(regs, "AWB_RESULT_CONTEXT_ID"),
                             count=st.read(regs, "AWB_GLOBAL_COUNT"),
                             sum=[(st.read(regs, "AWB_GLOBAL_SUM_%s_H" % c) << 32) |
                                  st.read(regs, "AWB_GLOBAL_SUM_%s_L" % c) for c in "RGB"])
            elif op == "L":
                regs.write("AWB_RESULT_CTRL", 2)
            else:
                regs.write("AWB_RESULT_CTRL", 4)
            steps.append(e)
    return steps


def run(profile, raws, stats=None):
    """CRCs of every frame; with `stats` (a list), also each frame's publication."""
    regs = regs_mod.Registers()
    regs.write("COMMON_FRAME_WIDTH", WIDTH)
    regs.write("COMMON_FRAME_HEIGHT", HEIGHT)
    for name, value in profile:
        regs.write(name, value)
    crcs = []
    for raw in raws:
        y, uv = blocks.run_frame(raw, regs, {})
        assert y.shape == (HEIGHT, WIDTH) and uv.shape == (HEIGHT // 2, WIDTH), (y.shape, uv.shape)
        crcs.append(zlib.crc32(y.astype(np.uint8).tobytes() + uv.astype(np.uint8).tobytes()) & 0xFFFFFFFF)
        if stats is not None:
            st = regs.stats
            # Every engine's result is read under a lease, frame by frame, as
            # the firmware's driver does (MAS v1.07 §9.8, §6.22.4): acquire,
            # read, release. AEC globals through the wide indexed port (index
            # 0 sum, 1 count).
            def wide(index):
                regs.write("TUNE_AEC_INDEX", index)
                return (st.read(regs, "TUNE_AEC_DATA_HI") << 32) | st.read(regs, "TUNE_AEC_DATA_LO")
            regs.write("TUNE_AEC_CMD", 1)
            aec = (st.read(regs, "AEC_FRAME_ID"), st.read(regs, "AEC_RESULT_CONTEXT_ID"), wide(0), wide(1))
            regs.write("TUNE_AEC_CMD", 2)
            regs.write("AWB_RESULT_CTRL", 1)
            awb_fid, awb_ctx = st.read(regs, "AWB_FRAME_ID"), st.read(regs, "AWB_RESULT_CONTEXT_ID")
            regs.write("AWB_RESULT_CTRL", 2)
            regs.write("TUNE_AF_CMD", 1)
            af_fid, af_ctx = st.read(regs, "AF_FRAME_ID"), st.read(regs, "AF_RESULT_CONTEXT_ID")
            regs.write("TUNE_AF_CMD", 2)
            stats.append({
                "aec_frame_id": aec[0], "aec_ctx": aec[1], "aec_sum": aec[2], "aec_count": aec[3],
                "awb_frame_id": awb_fid, "awb_ctx": awb_ctx, "af_frame_id": af_fid, "af_ctx": af_ctx})
    return crcs


def offsets(profile):
    regs = regs_mod.Registers()
    return [(regs.by_name[name]["offset"], value) for name, value in profile]


def provenance():
    h = hashlib.sha256()
    for rel in ("reference/fx1_isp_ref/blocks.py", "reference/fx1_isp_ref/regs.py",
                "reference/fx1_isp_ref/stats.py", "tools/make_profile.py",
                "docs/csr/fx1_isp_csr_schema.json"):
        with open(os.path.join(ISP, rel), "rb") as f:
            h.update(f.read())
    return h.hexdigest()


def render():
    raws = [raw_frame(k) for k in range(FRAMES)]
    raw12 = raws[0].astype(np.int64).ravel()
    basic, note = make_profile.profile_writes(raw12, WIDTH, HEIGHT, BAYER, "basic")
    gtm = basic + GTM_AUTO
    crc_basic, crc_gtm = run(basic, raws), run(gtm, raws)
    assert crc_basic != crc_gtm, "GTM auto must change the output"
    stats_profile = basic + STATS
    pubs = []
    assert run(stats_profile, raws, pubs) == crc_basic, "statistics must not change the image"
    assert [p["aec_frame_id"] for p in pubs] == list(range(1, FRAMES + 1)), "FRAME_ID 1..N after reset"

    own = run_ownership(stats_profile, raws)
    assert sum(1 for e in own if e["op"] == 0 and e["drops"]) >= 2, "the script must drop results"
    raws_rz = [raw_frame_rz(k) for k in range(RZ_FRAMES)]
    assert all(int(raws_rz[1][y, x]) == raw_sample_rz(1, x, y) for y, x in ((0, 0), (5, 7), (367, 647)))
    rz_basic, rz_note = make_profile.profile_writes(raws_rz[0].astype(np.int64).ravel(), RZ_WIDTH, RZ_HEIGHT,
                                                    BAYER, "basic")
    rz_basic = [(n, v) for n, v in rz_basic if n != "RESIZER_CTRL"]
    crc_rz = run_resize(rz_basic, raws_rz)

    def own_steps(name, steps):
        rows = ["    {%du, %du, %du, %du, %du, %du, %du, 0x%Xu, {%s}, %du}" % (
            e["frame"], e["op"], e["ready"], e["leased"], e["overrun"], e["drops"], e["fid"], e["ctx"],
            ", ".join("0x%XULL" % v for v in e["sum"]), e["count"]) for e in steps]
        return ("typedef struct isp_fx_awb_step {\n"
                "    uint32_t frame, op;\n"
                "    uint32_t ready, leased, overrun, drops;  /* op 0 */\n"
                "    uint32_t fid, ctx;                       /* op 1 */\n"
                "    uint64_t sum[3];                         /* op 1: R, G, B */\n"
                "    uint32_t count;                          /* op 1 */\n"
                "} isp_fx_awb_step;\n"
                "#define ISP_FX_AWB_STEPS %du\n"
                "static const isp_fx_awb_step %s[ISP_FX_AWB_STEPS] ISP_FX_UNUSED = {\n%s\n};\n" % (
                    len(steps), name, ",\n".join(rows)))

    def table(name, writes):
        rows = ",\n".join("    {0x%04Xu, 0x%08Xu}" % (o, v) for o, v in offsets(writes))
        return "static const fx1_isp_reg_write %s[] ISP_FX_UNUSED = {\n%s\n};\n" % (name, rows)

    def crcs(name, values):
        return "static const uint32_t %s[ISP_FX_FRAMES] ISP_FX_UNUSED = {\n    %s\n};\n" % (
            name, ",\n    ".join("0x%08Xu" % v for v in values))

    def publications(name, values):
        rows = []
        for p in values:
            rows.append("    {%du, 0x%Xu, 0x%XULL, %dULL, %du, 0x%Xu, %du, 0x%Xu}" % (
                p["aec_frame_id"], p["aec_ctx"], p["aec_sum"], p["aec_count"],
                p["awb_frame_id"], p["awb_ctx"], p["af_frame_id"], p["af_ctx"]))
        return ("typedef struct isp_fx_publication {\n"
                "    uint32_t aec_frame_id, aec_ctx;\n"
                "    uint64_t aec_sum, aec_count;   /* clip-qualified Bayer aggregate (MAS v1.07 Table 9-193) */\n"
                "    uint32_t awb_frame_id, awb_ctx, af_frame_id, af_ctx;\n"
                "} isp_fx_publication;\n"
                "static const isp_fx_publication %s[ISP_FX_FRAMES] ISP_FX_UNUSED = {\n%s\n};\n" % (name, ",\n".join(rows)))

    return "\n".join([
        "/* GENERATED by gen_fixture.py -- do not edit. Regenerate: python3 gen_fixture.py",
        " * ISP reference + make_profile + CSR schema SHA-256: %s" % provenance(),
        " * basic profile: %s" % note,
        " * This is a test profile, not a sensor calibration. */",
        "#ifndef ISP_FIXTURE_H",
        "#define ISP_FIXTURE_H",
        "#include <stdint.h>",
        '#include "fx1_isp/fx1_isp_drv.h"',
        "",
        "#define ISP_FX_WIDTH  %du" % WIDTH,
        "#define ISP_FX_HEIGHT %du" % HEIGHT,
        "#define ISP_FX_BAYER  %du /* %s */" % (make_profile.BAYER[BAYER], BAYER),
        "#define ISP_FX_FRAMES %du" % FRAMES,
        "/* Each test uses some of the tables below. */",
        "#define ISP_FX_UNUSED __attribute__((unused))",
        "",
        table("isp_fx_profile_basic", basic),
        table("isp_fx_profile_gtm_auto", gtm),
        crcs("isp_fx_crc_basic", crc_basic),
        crcs("isp_fx_crc_gtm_auto", crc_gtm),
        "/* basic + AEC/AWB/AF; its NV12 CRCs are isp_fx_crc_basic. */",
        table("isp_fx_profile_stats", stats_profile),
        publications("isp_fx_stats_expected", pubs),
        "/* AWB ownership script on the stats profile (op: 0 report, 1 acquire, 2 acquire",
        " * expecting none, 3 release, 4 clear report), after frame `frame`. */",
        own_steps("isp_fx_awb_script", own),
        "",
        "#define ISP_FX_RZ_WIDTH  %du" % RZ_WIDTH,
        "#define ISP_FX_RZ_HEIGHT %du" % RZ_HEIGHT,
        "#define ISP_FX_RZ_FRAMES %du" % RZ_FRAMES,
        "#define ISP_FX_RZ_SCALED 0x%02Xu /* RESIZER_CTRL: en, 640x360 */" % RZ_SCALED,
        "/* basic profile of the resize case: %s */" % rz_note,
        table("isp_fx_profile_rz", rz_basic),
        "/* RESIZER_CTRL of each resize frame and its NV12 CRC (640x360 or 648x368). */",
        "static const uint32_t isp_fx_rz_mode[ISP_FX_RZ_FRAMES] ISP_FX_UNUSED = {%s};" % ", ".join(
            "0x%02Xu" % m for m in RZ_MODES),
        "static const uint32_t isp_fx_crc_rz[ISP_FX_RZ_FRAMES] ISP_FX_UNUSED = {%s};" % ", ".join(
            "0x%08Xu" % c for c in crc_rz),
        "#endif /* ISP_FIXTURE_H */",
        ""])


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true", help="verify the committed header is up to date")
    a = ap.parse_args()
    text = render()
    if a.check:
        with open(OUT, encoding="utf-8") as f:
            if f.read() != text:
                print("isp_fixture.h is out of date: run gen_fixture.py")
                return 1
        print("isp_fixture.h is up to date")
        return 0
    with open(OUT, "w", encoding="utf-8") as f:
        f.write(text)
    print("wrote", OUT)
    return 0


if __name__ == "__main__":
    sys.exit(main())
