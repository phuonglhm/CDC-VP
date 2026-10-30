#!/usr/bin/env python3
"""Pack the ISP demo's DDR dumps into NV12 and a PNG preview; Python stdlib only.

The demo firmware (bare-metal isp_demo or `isp run` in the FreeRTOS CLI)
leaves a result block and the NV12 planes in DDR; fx1_soc --dump writes them
to result.bin, y.bin and uv.bin next to the job.bin they were made from. This
module checks them, keeps the active bytes (platform.nv12: Y rows, then UV
rows) and writes platform.png.

Used by run_isp_demo.py (CDC-VP) and shipped as is in the SDK, where no numpy
or Pillow can be assumed. As a command:

  isp_pack.py DIR [--expect-sha256 HEX]

Exit status: 0 packed (and equal to HEX when given), 1 NV12 differs from HEX,
2 the inputs were rejected.
"""
import argparse
import hashlib
import os
import struct
import sys
import zlib

# fw/fx1_soc/demo/isp_raw_frame/isp_demo_job.h
# Version 2 (V107-10): mode (0 profile, the default; 1 legacy), output
# channels and their buffers; a version-1 job (64 bytes) still reads, as
# legacy mode, CH0 only.
JOB_MAGIC, RESULT_MAGIC, VERSION, VERSION_V1 = 0x4A315846, 0x52315846, 2, 1
JOB_SIZE, JOB_SIZE_V1 = 96, 64
MODE_PROFILE, MODE_LEGACY = 0, 1
JOB_FIELDS = ("magic version width height bayer in_addr in_stride y_addr uv_addr out_stride profile_count frames "
              "mode outputs profile_id ch1_y_addr ch1_uv_addr ch1_stride ch2_addr ch2_stride r0 r1 r2 r3").split()
RESULT_FIELDS = ("magic status out_width out_height out_stride frames_done idma_frames odma_frames "
                 "dma_err aec_frame_id awb_frame_id af_frame_id frame_ticks "
                 "mode outputs profile_id tune_error ch1_width ch1_height ch2_width ch2_height").split()
RESULT_SIZE = 4 * len(RESULT_FIELDS)
BAYER = {"RGGB": 0, "GRBG": 1, "GBRG": 2, "BGGR": 3}  # COMMON_BAYER codes (DEC-11)
KR_KB = {0: (0.299, 0.114), 1: (0.2126, 0.0722)}       # BT.601, BT.709 (CSC_CTRL.std)


class PackError(Exception):
    """The inputs cannot hold a valid frame; the message says why."""


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def pack_job(**kw):
    """A version-2 job.bin; missing fields are 0 (mode 0 = profile, outputs 0 = CH0)."""
    return struct.pack("<%dI" % len(JOB_FIELDS), *[kw.get(k, 0) for k in JOB_FIELDS])


def read_job(path):
    """job.bin (isp_demo_job.h): the geometry and mode the dumps were made with."""
    data = open(path, "rb").read()
    if len(data) not in (JOB_SIZE, JOB_SIZE_V1):
        raise PackError("%s: %d bytes, a job descriptor has %d (version 1: %d)" % (path, len(data), JOB_SIZE,
                                                                                 JOB_SIZE_V1))
    f = dict(zip(JOB_FIELDS, struct.unpack("<%dI" % (len(data) // 4), data)))
    version = VERSION_V1 if len(data) == JOB_SIZE_V1 else VERSION
    if f["magic"] != JOB_MAGIC or f["version"] != version or f["bayer"] not in BAYER.values():
        raise PackError("%s: not a version-%d job descriptor" % (path, version))
    if version == VERSION_V1:
        f.update(mode=MODE_LEGACY, outputs=1, profile_id=0)
    elif f["mode"] not in (MODE_PROFILE, MODE_LEGACY) or not (f["outputs"] in (0, 1) or
                                                             (f["outputs"] & 1 and f["outputs"] <= 7)):
        raise PackError("%s: mode %d / outputs 0x%x not supported" % (path, f["mode"], f["outputs"]))
    f["outputs"] = f["outputs"] or 1
    return f


def read_result(path):
    """result.bin: the block the firmware writes once the frames are done."""
    data = open(path, "rb").read()
    if len(data) != RESULT_SIZE:
        raise PackError("result.bin: %d bytes, the result block has %d" % (len(data), RESULT_SIZE))
    res = dict(zip(RESULT_FIELDS, struct.unpack("<%dI" % len(RESULT_FIELDS), data)))
    if res["magic"] == RESULT_MAGIC and res["status"] == 2:
        raise PackError("the firmware refused the profile (profile mode: TUNE_ERROR 0x%x); nothing ran, and there "
                        "is no fallback to legacy mode" % res["tune_error"])
    if res["magic"] != RESULT_MAGIC or res["status"] != 0:
        raise PackError("no valid result block from the firmware (was `isp run` PASS before poweroff?)")
    return res


def check_output(res, width, height, frames, out_stride, y, uv):
    """Reject a result or dump that cannot hold the frame (review G6a R2): slicing
    short data would otherwise give a truncated NV12 without any error."""
    ow, oh, stride = res["out_width"], res["out_height"], res["out_stride"]
    y_len, uv_len = out_stride * height, out_stride * (height // 2)
    problems = []
    if not (0 < ow <= width and 0 < oh <= height) or ow % 2 or oh % 2:
        problems.append("output %dx%d is not an even size within the %dx%d input" % (ow, oh, width, height))
    if stride != out_stride or stride < ow:
        problems.append("output stride %d, the job used %d (width %d)" % (stride, out_stride, ow))
    if res["frames_done"] != frames or res["odma_frames"] != frames or res["idma_frames"] != frames:
        problems.append("frames done %d, IDMA %d, ODMA %d, the job asked for %d"
                        % (res["frames_done"], res["idma_frames"], res["odma_frames"], frames))
    if len(y) != y_len or len(uv) != uv_len:
        problems.append("dumps hold Y %d and UV %d bytes, expected %d and %d" % (len(y), len(uv), y_len, uv_len))
    if problems:
        raise PackError("output rejected: " + "; ".join(problems))


def check_identity(job, res, expect_mode=None):
    """The result must be the job's run (review V107-10 completion R3): the
    mode (`expect_mode` when the console forced one, e.g. `isp run legacy`,
    else the job's; a version-1 job is legacy), the output channels, and in
    profile mode the job's profile ID (0 reads as 1) on the outputs."""
    mode = job["mode"] if expect_mode is None else expect_mode
    want_id = (job["profile_id"] or 1) if mode == MODE_PROFILE else 0
    problems = []
    if res["mode"] != mode:
        problems.append("ran mode %d, expected %d (0 profile, 1 legacy)" % (res["mode"], mode))
    if res["outputs"] != job["outputs"]:
        problems.append("wrote outputs 0x%x, the job asked for 0x%x" % (res["outputs"], job["outputs"]))
    if res["profile_id"] != want_id:
        problems.append("outputs carry profile ID %d, expected %d" % (res["profile_id"], want_id))
    if problems:
        raise PackError("result identity rejected: " + "; ".join(problems))


def pack_channels(d, job, res):
    try:
        return _pack_channels(d, job, res)
    except PackError:
        for name in ("platform_ch1.nv12", "platform_ch2.rgb"):
            if os.path.isfile(os.path.join(d, name)):
                os.remove(os.path.join(d, name))
        raise


def _pack_channels(d, job, res):
    """CH1 (NV12) and CH2 (RGB888) of a version-2 job: geometry, strides and
    dump lengths checked before anything is written (review V107-10
    completion R2); platform_ch1.nv12 / platform_ch2.rgb and their summary
    lines. Nothing named platform_ch* survives a failure."""
    out = lambda name: os.path.join(d, name)
    for name in ("platform_ch1.nv12", "platform_ch2.rgb"):
        if os.path.isfile(out(name)):
            os.remove(out(name))
    width, height, lines = job["width"], job["height"], []

    def load(name):
        if not os.path.isfile(out(name)):
            raise PackError("%s not found (the enabled channel's dump)" % out(name))
        return open(out(name), "rb").read()

    def geometry(ch, w, h):
        if not (0 < w <= width and 0 < h <= height) or w % 2 or h % 2:
            raise PackError("CH%d: output %dx%d is not an even size within the %dx%d input" % (ch, w, h, width, height))

    if job["outputs"] & 2:
        w, h, stride = res["ch1_width"], res["ch1_height"], job["ch1_stride"]
        geometry(1, w, h)
        y1, uv1 = load("y1.bin"), load("uv1.bin")
        if stride < w or len(y1) != stride * height or len(uv1) != stride * (height // 2):
            raise PackError("CH1: dumps hold Y %d and UV %d bytes with stride %d, expected %d and %d (width %d)"
                            % (len(y1), len(uv1), stride, stride * height, stride * (height // 2), w))
        nv12 = pack_nv12(y1, uv1, w, h, stride)
        if len(nv12) != w * h * 3 // 2:
            raise PackError("CH1: packed %d bytes, expected %d" % (len(nv12), w * h * 3 // 2))
        with open(out("platform_ch1.nv12"), "wb") as f:
            f.write(nv12)
        lines.append("CH1 NV12       %dx%d  sha256 %s" % (w, h, hashlib.sha256(nv12).hexdigest()))
    if job["outputs"] & 4:
        w, h, stride = res["ch2_width"], res["ch2_height"], job["ch2_stride"]
        if not (0 < w <= width and 0 < h <= height):
            raise PackError("CH2: output %dx%d is not within the %dx%d input" % (w, h, width, height))
        rgb = load("rgb2.bin")
        if stride < 3 * w or len(rgb) != stride * height:
            raise PackError("CH2: dump holds %d bytes with stride %d, expected %d (width %d)"
                            % (len(rgb), stride, stride * height, w))
        packed = b"".join(rgb[r * stride: r * stride + 3 * w] for r in range(h))
        with open(out("platform_ch2.rgb"), "wb") as f:
            f.write(packed)
        lines.append("CH2 RGB888     %dx%d  sha256 %s" % (w, h, hashlib.sha256(packed).hexdigest()))
    return lines


def pack_nv12(y, uv, width, height, stride):
    """The active bytes: Y rows, then the interleaved UV rows."""
    rows = [y[r * stride:r * stride + width] for r in range(height)]
    rows += [uv[r * stride:r * stride + width] for r in range(height // 2)]
    return b"".join(rows)


def _clip_table(values):
    return bytes(0 if v < 0 else 255 if v > 255 else v for v in values)


def nv12_to_png(nv12, width, height, path, std=0):
    """Write an RGB PNG. Same inverse transform as isp/tools/nv12_preview.py:
    limited-range YCbCr (Y 16..235, C 16..240), BT.601 or BT.709, chroma by
    nearest neighbour. A preview is a visual check, not a pass/fail criterion."""
    if len(nv12) != width * height * 3 // 2:
        raise PackError("NV12 holds %d bytes, %dx%d needs %d" % (len(nv12), width, height, width * height * 3 // 2))
    kr, kb = KR_KB[std]
    kg = 1 - kr - kb
    # R = Y' + a*Pr, B = Y' + b*Pb, G = Y' - c*Pr - d*Pb, all in 255 units; one
    # rounding at the end, offset by 1024 so the tables index non-negative sums.
    ylut = [(v - 16) / 219.0 * 255 for v in range(256)]
    r_v = [2 * (1 - kr) * (v - 128) / 224.0 * 255 for v in range(256)]
    b_u = [2 * (1 - kb) * (u - 128) / 224.0 * 255 for u in range(256)]
    g_v = [-kr * 2 * (1 - kr) * (v - 128) / 224.0 * 255 / kg for v in range(256)]
    g_u = [-kb * 2 * (1 - kb) * (u - 128) / 224.0 * 255 / kg for u in range(256)]
    clip = _clip_table(range(-1024, 1280))
    raw = bytearray()
    ysize = width * height
    for row in range(height):
        yrow = nv12[row * width:(row + 1) * width]
        uvrow = nv12[ysize + (row // 2) * width:ysize + (row // 2 + 1) * width]
        out = bytearray(3 * width + 1)  # filter byte 0, then RGB
        for x in range(0, width, 2):
            u, v = uvrow[x], uvrow[x + 1]
            rv, bu, gc = r_v[v], b_u[u], g_v[v] + g_u[u]
            for k in (x, x + 1):
                yy = ylut[yrow[k]]
                o = 1 + 3 * k
                out[o] = clip[int(yy + rv + 1024.5)]
                out[o + 1] = clip[int(yy + gc + 1024.5)]
                out[o + 2] = clip[int(yy + bu + 1024.5)]
        raw += out

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(bytes(raw), 6)))
        f.write(chunk(b"IEND", b""))


def pack_dir(d, expect_sha=None, expect_mode=None):
    """job.bin + result.bin + y.bin + uv.bin (+ the CH1/CH2 dumps of the job) in d ->
    platform.nv12, platform.png, platform_ch*, summary.txt. Returns (result, sha,
    verdict); raises PackError. Nothing named platform* survives a failure."""
    out = lambda name: os.path.join(d, name)
    for name in ("platform.nv12", "platform.png", "summary.txt", "platform_ch1.nv12", "platform_ch2.rgb"):
        if os.path.isfile(out(name)):
            os.remove(out(name))
    for name in ("job.bin", "result.bin", "y.bin", "uv.bin"):
        if not os.path.isfile(out(name)):
            raise PackError("%s not found (the VP writes the dumps at `poweroff`)" % out(name))
    job = read_job(out("job.bin"))
    res = read_result(out("result.bin"))
    y, uv = open(out("y.bin"), "rb").read(), open(out("uv.bin"), "rb").read()
    check_output(res, job["width"], job["height"], job["frames"], job["out_stride"], y, uv)
    check_identity(job, res, expect_mode)
    channel_lines = pack_channels(d, job, res)
    ow, oh = res["out_width"], res["out_height"]
    nv12 = pack_nv12(y, uv, ow, oh, res["out_stride"])
    with open(out("platform.nv12"), "wb") as f:
        f.write(nv12)
    try:
        nv12_to_png(nv12, ow, oh, out("platform.png"))
    except OSError as e:
        os.remove(out("platform.nv12"))
        raise PackError("cannot write %s: %s" % (out("platform.png"), e))
    sha = hashlib.sha256(nv12).hexdigest()
    verdict = "-" if not expect_sha else ("MATCH" if sha == expect_sha.lower() else "MISMATCH")
    lines = [
        "input          %dx%d %s, %d frame(s)" % (job["width"], job["height"],
                                                   next(k for k, v in BAYER.items() if v == job["bayer"]), job["frames"]),
        "output         %dx%d NV12, stride %d" % (ow, oh, res["out_stride"]),
        "mode           %s%s; outputs 0x%x" % ("legacy" if res["mode"] == MODE_LEGACY else "profile",
                                                "" if res["mode"] == MODE_LEGACY else " (profile ID %d)" % res["profile_id"],
                                                res["outputs"]),
    ] + channel_lines + [
        "ISP counters   IDMA %d, ODMA %d frames; DMA_ERR 0x%x" % (res["idma_frames"], res["odma_frames"], res["dma_err"]),
        "FRAME_ID       AEC %d, AWB %d, AF %d" % (res["aec_frame_id"], res["awb_frame_id"], res["af_frame_id"]),
        "platform NV12  %s  sha256 %s" % (out("platform.nv12"), sha),
        "expected       %s" % (expect_sha or "-"),
        "result         %s" % verdict,
        "preview        %s" % out("platform.png"),
    ]
    with open(out("summary.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")
    print("\n".join(lines))
    return res, sha, verdict


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("dir", help="directory with job.bin, result.bin, y.bin and uv.bin")
    ap.add_argument("--expect-sha256", help="NV12 SHA-256 the run must reproduce")
    ap.add_argument("--expect-mode", choices=("profile", "legacy"),
                    help="the mode the console ran (`isp run legacy`): default the job's")
    a = ap.parse_args()
    mode = None if a.expect_mode is None else (MODE_LEGACY if a.expect_mode == "legacy" else MODE_PROFILE)
    try:
        _, _, verdict = pack_dir(a.dir, a.expect_sha256, mode)
    except PackError as e:
        print("isp_pack: " + str(e), file=sys.stderr)
        return 2
    return 1 if verdict == "MISMATCH" else 0


if __name__ == "__main__":
    sys.exit(main())
