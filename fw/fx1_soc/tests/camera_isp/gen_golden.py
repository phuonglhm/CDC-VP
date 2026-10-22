#!/usr/bin/env python3
"""Generate the golden NV12 of the camera -> ISP tests (CSI-2 M6a).

Writes camera_isp_golden.h next to this script. The expected images come from
the stand-alone SystemC ISP model, fx1_isp_run_raw (components/FX1_Components/
isp/tools/run_raw.cpp), run on the exact input the ISP receives on fx1_soc:

  raw12   sensor 64x16 RAW12 (zero-copy: the CSI-2 RAW16 side is the ISP input),
          sample = CcsSensor::pattern(f, x, y, 12)
  raw10   sensor 64x16 RAW10, staged by the firmware as sample << 2
          (ISP container bits [11:2]): pattern(f, x, y, 10) << 2
  tpg4k   CSI-2 RX test pattern generator, 3840x2160 RAW12 (colour bars,
          spec_decisions C35/D11), zero-copy
  tpg640  the same at 640x480 (FreeRTOS CLI `camera` check, M6b: a 4K NV12
          CRC on the simulated CPU takes about ten minutes of host time)

For tpg4k also 32 Y and 32 UV sample bytes at fixed positions (corners, every
colour bar, first / middle / last lines): the FreeRTOS CLI skips the CRC of
an NV12 above 1 MiB and spot-checks these instead (M6b review F2).

For each case: the ISP "basic" profile (tools/make_profile.py: BLC, grey-world
WB from frame 1 of the case, gamma BT.709, CSC BT.601; no temporal state, so
every frame's image depends on its input only), the CSR writes as ISP
offsets, and the CRC-32 (IEEE, zlib.crc32) of the active NV12 bytes (Y then UV)
of frames 1..N (the sensor's frame numbers; a frame is identified by number).

  python3 gen_golden.py --run-raw <build>/components/FX1_Components/isp/fx1_isp_run_raw
  python3 gen_golden.py --run-raw ... --check     exit 1 if the header is out of date

Requires numpy (make_profile). Not run at build time: the header is committed
and fx1_regression_camera_isp_golden_check runs --check.
"""

import argparse
import hashlib
import os
import subprocess
import sys
import tempfile
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ISP = os.path.normpath(os.path.join(HERE, "..", "..", "..", "..", "components", "FX1_Components", "isp"))
sys.path.insert(0, os.path.join(ISP, "reference"))
sys.path.insert(0, os.path.join(ISP, "tools"))
from fx1_isp_ref import regs as regs_mod  # noqa: E402
import make_profile  # noqa: E402

OUT = os.path.join(HERE, "camera_isp_golden.h")
CAM_W, CAM_H, CAM_FRAMES = 64, 16, 32
TPG_W, TPG_H = 3840, 2160
TPG640_W, TPG640_H = 640, 480


def pattern(f, x, y, bits):
    """CcsSensor::pattern / fx1_ccs_pattern."""
    return (x * 37 + y * 101 + f * 53 + 5) & ((1 << bits) - 1)


def sensor_frame(f, bits):
    y, x = np.mgrid[0:CAM_H, 0:CAM_W].astype(np.uint32)
    v = pattern(f, x, y, bits)
    return (v << 2 if bits == 10 else v).astype(np.uint16)  # ISP container bits [11:0]


def tpg_frame(width, height):
    """C35: W' = width & ~3, band = min(7, x / (W' >> 3)), MAS Table 625 bars, RGGB, RAW12 level 0xFFF."""
    R = [1, 1, 0, 0, 1, 1, 0, 0]
    G = [1, 1, 1, 1, 0, 0, 0, 0]
    B = [1, 0, 1, 0, 1, 0, 1, 0]
    wt = width & ~3
    y, x = np.mgrid[0:height, 0:width]
    band = np.minimum(7, x // (wt >> 3))
    lut = lambda t: np.array(t, dtype=np.uint16)[band]
    on = np.where((y % 2 == 0) & (x % 2 == 0), lut(R), np.where((y % 2 == 1) & (x % 2 == 1), lut(B), lut(G)))
    return (on * 0x0FFF).astype(np.uint16)


def profile_for(first):
    h, w = first.shape
    writes, _ = make_profile.profile_writes(first.astype(np.int64), w, h, "RGGB", "basic")
    return writes


def run_raw(run_raw_bin, frames, profile, workdir, name):
    """CRC-32 of each frame's NV12 from fx1_isp_run_raw."""
    return [zlib.crc32(d) & 0xFFFFFFFF for d in run_raw_nv12(run_raw_bin, frames, profile, workdir, name)]


def run_raw_nv12(run_raw_bin, frames, profile, workdir, name):
    """NV12 bytes of each frame from fx1_isp_run_raw (frames: uint16 ISP containers, H x W)."""
    h, w = frames[0].shape
    files = []
    for i, fr in enumerate(frames):
        p = os.path.join(workdir, "%s_%03d.isp16" % (name, i))
        fr.astype("<u2").tofile(p)
        files.append(p)
    lst = os.path.join(workdir, name + ".list")
    with open(lst, "w") as f:
        f.write("\n".join(files) + "\n")
    csrw = os.path.join(workdir, name + ".csrw")
    with open(csrw, "w") as f:
        f.writelines("%s 0x%X\n" % (reg, val) for reg, val in profile)
    out = os.path.join(workdir, name)
    r = subprocess.run([run_raw_bin, "--input-list", lst, "--width", str(w), "--height", str(h), "--profile", csrw,
                        "--out", out, "--frames", str(len(frames)), "--per-frame"],
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if r.returncode:
        raise SystemExit("fx1_isp_run_raw failed for %s:\n%s" % (name, r.stdout))
    nv12 = []
    for k in range(len(frames)):  # run_raw numbers the per-frame files from 0
        with open("%s.f%03d.nv12" % (out, k), "rb") as f:
            data = f.read()
        if len(data) != w * h * 3 // 2:
            raise SystemExit("%s frame %d: NV12 of %d bytes, expected %d" % (name, k, len(data), w * h * 3 // 2))
        nv12.append(data)
    return nv12


def spot_positions(w, h):
    """32 (x, y) of the Y plane and 32 of the interleaved UV plane (w bytes x h/2 rows)."""
    bar = (w & ~3) >> 3
    y_pos, uv_pos = [], []
    for b in range(8):
        x0 = b * bar
        y_pos += [(x0 + bar // 2, 0), (x0 + bar // 2 + 1, h - 1), (x0 + 7, h // 2), (min(x0 + bar - 1, w - 1), 1)]
        uv_pos += [(x0 + bar // 2, 0), (x0 + bar // 2 + 1, h // 2 - 1), (x0 + 10, h // 4), (min(x0 + bar - 1, w - 1), 1)]
    y_pos[3], uv_pos[3] = (0, 0), (0, h // 2 - 1)            # corners too
    y_pos[-1], uv_pos[-1] = (w - 1, h - 1), (w - 1, h // 2 - 1)
    return y_pos, uv_pos


def spots(nv12, w, h):
    y_pos, uv_pos = spot_positions(w, h)
    uv = nv12[w * h:]
    return [(x, y, nv12[y * w + x]) for x, y in y_pos], [(x, y, uv[y * w + x]) for x, y in uv_pos]


def offsets(profile):
    regs = regs_mod.Registers()
    return [(regs.by_name[name]["offset"], value) for name, value in profile]


def provenance():
    h = hashlib.sha256()
    for rel in ("tools/make_profile.py", "tools/run_raw.cpp", "src/engine/engines.cpp"):
        with open(os.path.join(ISP, rel), "rb") as f:
            h.update(f.read())
    with open(os.path.abspath(__file__), "rb") as f:
        h.update(f.read())
    return h.hexdigest()


def render(run_raw_bin):
    with tempfile.TemporaryDirectory() as work:
        cases = []
        for name, bits in (("raw12", 12), ("raw10", 10)):
            frames = [sensor_frame(f, bits) for f in range(1, CAM_FRAMES + 1)]
            prof = profile_for(frames[0])
            cases.append((name, prof, run_raw(run_raw_bin, frames, prof, work, name)))
        spot = None
        for name, w, h in (("tpg4k", TPG_W, TPG_H), ("tpg640", TPG640_W, TPG640_H)):
            tpg = tpg_frame(w, h)
            prof = profile_for(tpg)
            nv12 = run_raw_nv12(run_raw_bin, [tpg], prof, work, name)[0]
            cases.append((name, prof, [zlib.crc32(nv12) & 0xFFFFFFFF]))
            if name == "tpg4k":
                spot = spots(nv12, w, h)

    def table(name, writes):
        rows = ",\n".join("    {0x%04Xu, 0x%08Xu}" % (o, v) for o, v in offsets(writes))
        return "static const fx1_isp_reg_write %s[] CAM_ISP_UNUSED = {\n%s\n};\n" % (name, rows)

    def crcs(name, values):
        return "static const uint32_t %s[%d] CAM_ISP_UNUSED = {\n    %s\n};\n" % (
            name, len(values), ",\n    ".join("0x%08Xu" % v for v in values))

    parts = [
        "/* Generated by gen_golden.py: do not edit (CSI-2 M6a camera -> ISP golden).",
        " * Expected NV12 from fx1_isp_run_raw (stand-alone ISP model), profile \"basic\".",
        " * Sources SHA-256 %s */" % provenance(),
        "#ifndef CAMERA_ISP_GOLDEN_H",
        "#define CAMERA_ISP_GOLDEN_H",
        "",
        "#include <stdint.h>",
        "",
        "#include \"fx1_isp/fx1_isp_drv.h\"",
        "",
        "#define CAM_ISP_UNUSED __attribute__((unused))",
        "typedef struct { uint16_t x, y; uint8_t v; } cam_isp_spot; /* one NV12 byte: column, row of its plane */",
        "#define CAM_ISP_W %du" % CAM_W,
        "#define CAM_ISP_H %du" % CAM_H,
        "#define CAM_ISP_FRAMES %du /* CRCs of frame numbers 1..CAM_ISP_FRAMES */" % CAM_FRAMES,
        "#define CAM_ISP_TPG_W %du" % TPG_W,
        "#define CAM_ISP_TPG_H %du" % TPG_H,
        "#define CAM_ISP_TPG640_W %du" % TPG640_W,
        "#define CAM_ISP_TPG640_H %du" % TPG640_H,
        "",
    ]
    for name, prof, values in cases:
        parts.append(table("cam_isp_profile_%s" % name, prof))
        parts.append(crcs("cam_isp_crc_%s" % name, values))
    for plane, vals in zip(("y", "uv"), spot):
        rows = ",\n".join("    {%du, %du, 0x%02Xu}" % v for v in vals)
        parts.append("static const cam_isp_spot cam_isp_spot_tpg4k_%s[%d] CAM_ISP_UNUSED = {\n%s\n};\n" % (
            plane, len(vals), rows))
    parts += ["#endif /* CAMERA_ISP_GOLDEN_H */", ""]
    return "\n".join(parts)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--run-raw", required=True, help="path of the fx1_isp_run_raw executable")
    ap.add_argument("--check", action="store_true", help="verify the committed header is up to date")
    a = ap.parse_args()
    text = render(a.run_raw)
    if a.check:
        with open(OUT, encoding="utf-8") as f:
            if f.read() != text:
                print("camera_isp_golden.h is out of date: run gen_golden.py")
                return 1
        print("camera_isp_golden.h is up to date")
        return 0
    with open(OUT, "w", encoding="utf-8") as f:
        f.write(text)
    print("wrote", OUT)
    return 0


if __name__ == "__main__":
    sys.exit(main())
