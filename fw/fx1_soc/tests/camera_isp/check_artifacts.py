#!/usr/bin/env python3
"""Host check of the camera -> ISP artifacts (CSI-2 M6a, artifacts.c).

Runs the artifacts image on fx1_soc --camera, dumps its artifact area from DDR
and checks, for every frame the firmware kept:

  RAW      the CSI-2 RAW16 side = the sensor pattern of one frame number n
           (CcsSensor::pattern), every sample, nothing above `bits`
  input    RAW10: the ISP staging buffer = RAW << 2 (the maximum 0x3FF staged
           as 0xFFC); RAW12: zero-copy, the ISP read the CSI-2 side itself
           (record address = that side), 0xFFF reaching it unchanged
  NV12     CRC-32 = the committed golden of frame n (camera_isp_golden.h),
           = the CRC the firmware computed, and SHA-256 = fx1_isp_run_raw on
           the dumped ISP input with the "basic" profile (end to end)
  links    record frame number = n; frame numbers, captures, ISP sequences
           and CSI-2 FRAME_IDs consecutive; CSI-2 sides alternate; output
           slot = ISP sequence mod 4, its Y/UV addresses those of that slot

  check_artifacts.py --sim <fx1_soc> --elf <camera_isp_art_rawNN.elf> --bits NN \\
                     --run-raw <fx1_isp_run_raw> --out <dir>
  check_artifacts.py --dump <dir>/artifacts.bin --bits NN --run-raw <...>   (check only)

Exit status 0 all checks pass, 1 a check failed, 2 the run failed.
Requires numpy (gen_golden.py, make_profile).
"""

import argparse
import hashlib
import os
import re
import struct
import subprocess
import sys
import tempfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.dont_write_bytecode = True  # no __pycache__ in the source tree
import numpy as np  # noqa: E402
import gen_golden  # noqa: E402

ART_BASE = 0x81000000 + 0x400000  # FX1_FW_RAW_BUF_BASE + 0x400000 (artifacts.c)
ART_LEN = 0x10000
MAGIC = 0x414D4143
REC_FIELDS = ("capture", "csi_frame_id", "csi_side", "isp_seq", "isp_input", "isp_output", "zero_copy", "isp_in",
              "y", "uv", "number", "nv12_crc", "raw_capture", "width", "height", "reserved")
YUV_BASE, SLOT = 0x88000000, 0x10000  # cam_isp.h CAM_ISP_Y / CAM_ISP_UV


class CheckError(Exception):
    pass


def check(cond, what):
    if not cond:
        raise CheckError(what)


def golden_crcs(bits):
    with open(os.path.join(HERE, "camera_isp_golden.h"), encoding="utf-8") as f:
        text = f.read()
    m = re.search(r"cam_isp_crc_raw%d\[\d+\][^{]*\{([^}]*)\}" % bits, text)
    return [int(v.rstrip("u"), 16) for v in re.findall(r"0x[0-9A-F]+u", m.group(1))]


def run_sim(a):
    os.makedirs(a.out, exist_ok=True)
    dump = os.path.join(a.out, "artifacts.bin")
    log = os.path.join(a.out, "uart.log")
    for p in (dump, log):
        if os.path.exists(p):
            os.remove(p)
    cmd = [a.sim, "--fw", a.elf, "--camera", "--timeout-ms", str(a.timeout_ms), "--uart-log", log,
           "--dump", "0x%X:0x%X:%s" % (ART_BASE, ART_LEN, dump)]
    print("run:", " ".join(cmd), flush=True)
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    sys.stdout.write(r.stdout[-4000:])
    if r.returncode != 0 or "RESULT PASS" not in r.stdout:
        print("check_artifacts: the firmware run failed (exit %d)" % r.returncode)
        sys.exit(2)
    return dump


def check_dump(path, bits, run_raw_bin):
    with open(path, "rb") as f:
        art = f.read()
    check(len(art) == ART_LEN, "dump of %d bytes, expected %d" % (len(art), ART_LEN))
    hdr = struct.unpack_from("<16I", art, 0)
    (magic, version, hbits, w, h, frames, side0, side1, rec_off, rec_size, frame_off, frame_stride,
     raw_off, in_off, y_off, uv_off) = hdr
    check(magic == MAGIC and version == 1, "header magic/version")
    check(hbits == bits and (w, h) == (gen_golden.CAM_W, gen_golden.CAM_H), "header bits/geometry")
    check(frames >= 1, "no frames (the firmware did not complete)")
    golden = golden_crcs(bits)
    mask = (1 << bits) - 1
    recs, inputs, outputs = [], [], []
    for k in range(frames):
        r = dict(zip(REC_FIELDS, struct.unpack_from("<16I", art, rec_off + k * rec_size)))
        fb = frame_off + k * frame_stride
        raw = np.frombuffer(art, "<u2", w * h, fb + raw_off).reshape(h, w)
        # frame number from the first sample, then every sample
        n = next((f for f in range(1, gen_golden.CAM_FRAMES + 1) if gen_golden.pattern(f, 0, 0, bits) == raw[0, 0]), 0)
        check(n and np.array_equal(raw, gen_golden.sensor_frame(n, 12) if bits == 12 else
                                   (gen_golden.sensor_frame(n, 10) >> 2)), "frame %d: RAW = sensor pattern" % k)
        check(int(raw.max()) <= mask, "frame %d: RAW above %d bits" % (k, bits))
        if bits == 10:
            staged = np.frombuffer(art, "<u2", w * h, fb + in_off).reshape(h, w)
            check(np.array_equal(staged, raw << 2), "frame %d: staging = RAW << 2" % k)
            check(int(raw.max()) == 0x3FF and int(staged.max()) == 0xFFC, "frame %d: RAW10 0x3FF staged as 0xFFC" % k)
            check(not r["zero_copy"], "frame %d: RAW10 is not zero-copy" % k)
            inputs.append(staged.copy())
        else:
            check(r["zero_copy"] and r["isp_in"] == (side0, side1)[r["csi_side"]],
                  "frame %d: RAW12 zero-copy from its CSI-2 side" % k)
            inputs.append(raw.copy())
        nv12 = art[fb + y_off: fb + y_off + w * h] + art[fb + uv_off: fb + uv_off + w * h // 2]
        crc = zlib.crc32(nv12) & 0xFFFFFFFF
        check(crc == golden[n - 1], "frame %d: NV12 CRC 0x%08X = golden of frame %d 0x%08X" % (k, crc, n, golden[n - 1]))
        check(crc == r["nv12_crc"] and r["number"] == n, "frame %d: firmware record (CRC, number)" % k)
        check(r["raw_capture"] == r["capture"] and (r["width"], r["height"]) == (w, h), "frame %d: record" % k)
        check(r["isp_output"] == r["isp_seq"] % 4 and r["y"] == YUV_BASE + r["isp_output"] * SLOT
              and r["uv"] == r["y"] + 0x8000, "frame %d: output slot and addresses" % k)
        r["n"] = n
        recs.append(r)
        outputs.append(nv12)
    if bits == 12:
        check(max(int(i.max()) for i in inputs) == 0xFFF, "RAW12 0xFFF reaches the ISP unchanged")
    for a, b in zip(recs, recs[1:]):
        check(b["n"] == a["n"] + 1 and b["capture"] == a["capture"] + 1 and b["isp_seq"] == a["isp_seq"] + 1,
              "frames %d/%d: consecutive" % (a["n"], b["n"]))
        check(b["csi_frame_id"] == (a["csi_frame_id"] + 1) & 0xFFFF and b["csi_side"] != a["csi_side"],
              "frames %d/%d: CSI-2 FRAME_ID consecutive, sides alternate" % (a["n"], b["n"]))
    # end to end: the stand-alone ISP model on the dumped inputs
    prof = gen_golden.profile_for(gen_golden.sensor_frame(1, bits))
    with tempfile.TemporaryDirectory() as work:
        ref = gen_golden.run_raw_nv12(run_raw_bin, inputs, prof, work, "art%d" % bits)
    for r, out, exp in zip(recs, outputs, ref):
        sha, esha = hashlib.sha256(out).hexdigest(), hashlib.sha256(exp).hexdigest()
        check(sha == esha, "frame %d: NV12 SHA-256 %s = fx1_isp_run_raw %s" % (r["n"], sha, esha))
        print("frame %2d  capture %d  FRAME_ID %d  side %d  isp seq %d in %d out %d  %s  NV12 sha256 %s" % (
            r["n"], r["capture"], r["csi_frame_id"], r["csi_side"], r["isp_seq"], r["isp_input"], r["isp_output"],
            "zero-copy" if r["zero_copy"] else "staged", sha))
    return frames


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--bits", type=int, choices=(10, 12), required=True)
    ap.add_argument("--run-raw", required=True, help="fx1_isp_run_raw executable")
    ap.add_argument("--dump", help="check this dump only")
    ap.add_argument("--sim", help="fx1_soc executable")
    ap.add_argument("--elf", help="camera_isp_art_rawNN.elf")
    ap.add_argument("--out", help="directory for the dump and the UART log")
    ap.add_argument("--timeout-ms", type=int, default=80)
    a = ap.parse_args()
    if not a.dump and not (a.sim and a.elf and a.out):
        ap.error("--dump, or --sim, --elf and --out")
    path = a.dump or run_sim(a)
    try:
        frames = check_dump(path, a.bits, a.run_raw)
    except CheckError as e:
        print("check_artifacts: FAIL:", e)
        return 1
    print("check_artifacts: PASS (RAW%d, %d frames: RAW, ISP input, NV12, records)" % (a.bits, frames))
    return 0


if __name__ == "__main__":
    sys.exit(main())
