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
  * a third profile, stats = basic + AEC (4x3 zones of 16x16), AWB (global)
    and AF, with the publication every frame must produce: AEC/AWB/AF
    FRAME_ID and RESULT_CONTEXT_ID, AEC global sums and counts per channel.
    Statistics do not change the image, so its NV12 CRCs equal basic's.

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
# AWB global only, AF; distinct context IDs so the tags are checked too.
STATS = (make_profile.aec(4, 3, 16, 16) + make_profile.awb(0, 0, 16, 4000) + [("AF_CTRL", 1)] +
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
            st, ro = regs.stats, regs.stats.ro
            stats.append({
                "aec_frame_id": ro["AEC_FRAME_ID"], "aec_ctx": ro["AEC_RESULT_CONTEXT_ID"],
                "aec_sum": [v & ((1 << 33) - 1) for v in st.aec["gsum"]],
                "aec_count": [v & 0x1FFFFF for v in st.aec["gcnt"]],
                "awb_frame_id": ro["AWB_FRAME_ID"], "awb_ctx": ro["AWB_RESULT_CONTEXT_ID"],
                "af_frame_id": ro["AF_FRAME_ID"], "af_ctx": ro["AF_RESULT_CONTEXT_ID"]})
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

    def table(name, writes):
        rows = ",\n".join("    {0x%04Xu, 0x%08Xu}" % (o, v) for o, v in offsets(writes))
        return "static const fx1_isp_reg_write %s[] ISP_FX_UNUSED = {\n%s\n};\n" % (name, rows)

    def crcs(name, values):
        return "static const uint32_t %s[ISP_FX_FRAMES] ISP_FX_UNUSED = {\n    %s\n};\n" % (
            name, ",\n    ".join("0x%08Xu" % v for v in values))

    def publications(name, values):
        rows = []
        for p in values:
            rows.append("    {%du, 0x%Xu, {%s}, {%s}, %du, 0x%Xu, %du, 0x%Xu}" % (
                p["aec_frame_id"], p["aec_ctx"],
                ", ".join("0x%XULL" % v for v in p["aec_sum"]),
                ", ".join("%du" % v for v in p["aec_count"]),
                p["awb_frame_id"], p["awb_ctx"], p["af_frame_id"], p["af_ctx"]))
        return ("typedef struct isp_fx_publication {\n"
                "    uint32_t aec_frame_id, aec_ctx;\n"
                "    uint64_t aec_sum[4];   /* R, Gr, Gb, B */\n"
                "    uint32_t aec_count[4];\n"
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
