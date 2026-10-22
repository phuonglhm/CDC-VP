#!/usr/bin/env python3
"""Host check of the camera -> ISP large-frame test (CSI-2 M6a, tpg4k.c).

Runs the tpg4k image on fx1_soc --camera, dumps its record and the NV12 output
slots 0 and 1 from DDR, and checks that the NV12 of each frame (3840x2160,
Y then UV) has the CRC-32 of the stand-alone ISP model's output for the
CSI-2 RX colour bars (cam_isp_crc_tpg4k, camera_isp_golden.h).

  check_tpg4k.py --sim <fx1_soc> --elf <camera_isp_tpg4k.elf> --out <dir>

Exit status 0 pass, 1 a check failed, 2 the run failed. Standard library only.
"""

import argparse
import hashlib
import os
import re
import struct
import subprocess
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
W, H = 3840, 2160
REC = 0x80500000 + 0x800000  # FX1_FW_SHARED_BASE + 0x800000 (tpg4k.c CAM_TPG_REC)
YUV_BASE, SLOT, UV_OFF = 0x88000000, 0x1400000, 0xC00000
SLOT_LEN = UV_OFF + W * H // 2


def golden():
    with open(os.path.join(HERE, "camera_isp_golden.h"), encoding="utf-8") as f:
        m = re.search(r"cam_isp_crc_tpg4k\[1\][^{]*\{\s*(0x[0-9A-F]+)u", f.read())
    return int(m.group(1), 16)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--sim", required=True)
    ap.add_argument("--elf", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--timeout-ms", type=int, default=1500)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    rec = os.path.join(a.out, "record.bin")
    slots = [os.path.join(a.out, "slot%d.bin" % i) for i in range(2)]
    log = os.path.join(a.out, "uart.log")
    for p in [rec, log] + slots:
        if os.path.exists(p):
            os.remove(p)
    cmd = [a.sim, "--fw", a.elf, "--camera", "--timeout-ms", str(a.timeout_ms), "--uart-log", log,
           "--dump", "0x%X:0x40:%s" % (REC, rec)]
    for i, p in enumerate(slots):
        cmd += ["--dump", "0x%X:0x%X:%s" % (YUV_BASE + i * SLOT, SLOT_LEN, p)]
    print("run:", " ".join(cmd), flush=True)
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    sys.stdout.write(r.stdout[-4000:])
    if r.returncode != 0 or "RESULT PASS" not in r.stdout:
        print("check_tpg4k: the firmware run failed (exit %d)" % r.returncode)
        return 2
    with open(rec, "rb") as f:
        words = struct.unpack("<16I", f.read())
    if words[0] != 0x4B475054 or words[1] != 2:
        print("check_tpg4k: FAIL: record magic / frame count")
        return 1
    want = golden()
    for k in range(2):
        out, y, uv, capture = words[2 + 4 * k: 6 + 4 * k]
        if out != k or y != YUV_BASE + k * SLOT or uv != y + UV_OFF:
            print("check_tpg4k: FAIL: frame %d record (slot %d, Y 0x%X, UV 0x%X)" % (k + 1, out, y, uv))
            return 1
        with open(slots[out], "rb") as f:
            data = f.read()
        nv12 = data[:W * H] + data[UV_OFF:UV_OFF + W * H // 2]
        crc = zlib.crc32(nv12) & 0xFFFFFFFF
        print("frame %d  capture %d  slot %d  NV12 CRC 0x%08X (golden 0x%08X)  sha256 %s" % (
            k + 1, capture, out, crc, want, hashlib.sha256(nv12).hexdigest()))
        if crc != want:
            print("check_tpg4k: FAIL: frame %d NV12 differs from the ISP model's" % (k + 1))
            return 1
    print("check_tpg4k: PASS (2 frames 3840x2160 RAW12 TPG -> NV12 = fx1_isp_run_raw)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
