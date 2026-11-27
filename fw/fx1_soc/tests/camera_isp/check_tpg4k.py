#!/usr/bin/env python3
"""Host check of the camera -> ISP large-frame test (CSI-2 M6a, tpg4k.c).

Runs the tpg4k image on fx1_soc --camera, dumps its record and the NV12 output
slots 0 and 1 from DDR, and checks that the NV12 of each frame (3840x2160,
Y then UV) has the CRC-32 of the stand-alone ISP model's output for the
CSI-2 RX colour bars (cam_isp_crc_tpg4k, camera_isp_golden.h).

  check_tpg4k.py --sim <fx1_soc> --elf <camera_isp_tpg4k.elf> --out <dir> [--direct]

--direct (the image built with CAM_DIRECT=1, plan DIRECT_CSI_ISP D3.3): the run
also writes --traffic-report, checked by camera_direct/check_direct_traffic.py
(no MIPI_DMA write, no ISP_IDMA traffic, ODMA writes inside the four NV12 slots);
with --raw-pattern (camera_direct/raw_sentinel.py, review D4 F1) the whole
firmware RAW region is loaded with the sentinel pattern and must be unchanged
after the run, whichever master would write it.

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
RAW_BASE, RAW_SIZE = 0x81000000, 0x7000000  # FX1_FW_RAW_BUF_BASE .. FX1_FW_YUV_BUF_BASE


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
    ap.add_argument("--direct", action="store_true")
    ap.add_argument("--raw-pattern", help="RAW sentinel pattern (direct only)")
    a = ap.parse_args()
    if a.raw_pattern and not a.direct:
        ap.error("--raw-pattern needs --direct")
    os.makedirs(a.out, exist_ok=True)
    rec = os.path.join(a.out, "record.bin")
    slots = [os.path.join(a.out, "slot%d.bin" % i) for i in range(2)]
    log = os.path.join(a.out, "uart.log")
    raw_dump = os.path.join(a.out, "raw_sentinel.bin")
    for p in [rec, log, raw_dump] + slots:
        if os.path.exists(p):
            os.remove(p)
    cmd = [a.sim, "--fw", a.elf, "--camera", "--timeout-ms", str(a.timeout_ms), "--uart-log", log,
           "--dump", "0x%X:0x40:%s" % (REC, rec)]
    for i, p in enumerate(slots):
        cmd += ["--dump", "0x%X:0x%X:%s" % (YUV_BASE + i * SLOT, SLOT_LEN, p)]
    traffic = os.path.join(a.out, "traffic.txt")
    if a.direct:
        if os.path.exists(traffic):
            os.remove(traffic)
        cmd += ["--traffic-report", traffic]
        if a.raw_pattern:
            cmd += ["--load", "0x%X:%s" % (RAW_BASE, a.raw_pattern),
                    "--dump", "0x%X:0x%X:%s" % (RAW_BASE, RAW_SIZE, raw_dump)]
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
    if a.direct:
        chk = os.path.join(HERE, "..", "camera_direct", "check_direct_traffic.py")
        raw = (["--raw-base", "0x%X" % RAW_BASE, "--raw-size", "0x%X" % RAW_SIZE,
                "--raw-pattern", a.raw_pattern, "--raw-dump", raw_dump] if a.raw_pattern else [])
        t = subprocess.run([sys.executable, chk, "--odma-lo", "0x%X" % YUV_BASE,
                            "--odma-hi", "0x%X" % (YUV_BASE + 4 * SLOT - 1)] + raw + [traffic],
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        sys.stdout.write(t.stdout)
        if t.returncode != 0:
            print("check_tpg4k: FAIL: direct-only traffic")
            return 1
    print("check_tpg4k: PASS (2 frames 3840x2160 RAW12 TPG -> NV12 = fx1_isp_run_raw%s)" %
          (", direct path, no RAW in DDR" if a.direct else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
