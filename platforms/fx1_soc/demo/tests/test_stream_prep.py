#!/usr/bin/env python3
"""isp_stream_prep.py: the stream ABI mirror, the input checks and the session layout.

  test_stream_prep.py [--cc CC] [--run-raw fx1_isp_run_raw]

1. ABI: a host C program built from fx1_isp_stream.h prints every block's size
   and field offsets and the layout macros; they must equal the Python mirror.
2. Layout: every DDR region of a session (blocks, profile area, SYS_DMA chains,
   slots, source pool) lies inside its firmware region of fx1_memory_map.h and
   no two overlap.
3. Conversion: every source_format x input_depth pair gives the container an
   independent per-sample formula gives; source rows longer than the image
   (source_stride) are cut; RAW10 native and RAW12 normalized of one scene
   differ by exactly the 2-bit shift the ISP applies.
4. Sessions: the manifest (relative paths with spaces, explicit order) and the
   --raw form; desc.bin, loads.txt, reference.list and session.json agree with
   the schedule identity (n // K) mod S.
5. Refusals: every bad manifest, RAW, profile, option or output directory
   exits 2 with a message naming the problem, before any session is written.
6. With --run-raw: the reference of the fixture's BGGR case is identical fed as
   RAW10 native and as RAW12 normalized, and its loop wrap (A after C) keeps
   the GTM history (differs from the first A).
Python stdlib only, like isp_stream_prep.py.
"""
import argparse
import io
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time
from contextlib import redirect_stderr, redirect_stdout

HERE = os.path.dirname(os.path.abspath(__file__))
DEMO = os.path.join(HERE, "..")
REPO = os.path.abspath(os.path.join(DEMO, "..", "..", ".."))
MAP_INC = os.path.join(REPO, "components", "FX1_Components", "fx1_map", "include")
sys.path.insert(0, DEMO)
sys.dont_write_bytecode = True
import isp_stream_prep as prep  # noqa: E402
import stream_fixture  # noqa: E402

failures = []


def check(cond, what):
    print(("ok   " if cond else "FAIL ") + what)
    if not cond:
        failures.append(what)


def run(argv):
    out, err = io.StringIO(), io.StringIO()
    with redirect_stdout(out), redirect_stderr(err):
        try:
            rc = prep.main(argv)
        except SystemExit as e:   # argparse
            rc = e.code
    return rc, out.getvalue() + err.getvalue()


# ---- 1. ABI ------------------------------------------------------------------------
def abi(cc, tmp):
    blocks = {"desc": ("fx1_isp_stream_desc", prep.DESC_FIELDS), "status": ("fx1_isp_stream_status", prep.STATUS_FIELDS),
              "control": ("fx1_isp_stream_control", prep.CONTROL_FIELDS),
              "record": ("fx1_isp_stream_record", prep.RECORD_FIELDS)}
    macros = {"FX1_ISP_STREAM_DESC_ADDR": prep.DESC_ADDR, "FX1_ISP_STREAM_STATUS_ADDR": prep.STATUS_ADDR,
              "FX1_ISP_STREAM_CONTROL_ADDR": prep.CONTROL_ADDR, "FX1_ISP_STREAM_RECORD_ADDR": prep.RECORD_ADDR,
              "FX1_ISP_STREAM_RECORD_BYTES": prep.RECORD_BYTES, "FX1_ISP_STREAM_PROFILE_ADDR": prep.PROFILE_ADDR,
              "FX1_ISP_STREAM_PROFILE_MAX": prep.PROFILE_MAX, "FX1_ISP_STREAM_CHAIN_ADDR": prep.CHAIN_ADDR,
              "FX1_ISP_STREAM_CHAIN_PITCH": prep.CHAIN_PITCH, "FX1_ISP_STREAM_SLOTS": prep.SLOTS,
              "FX1_ISP_STREAM_SLOT_BYTES": prep.SLOT_BYTES, "FX1_ISP_STREAM_IN_ADDR(3)": prep.FW_RAW_BUF_BASE + 3 * prep.SLOT_BYTES,
              "FX1_ISP_STREAM_OUT_ADDR(3)": prep.FW_YUV_BUF_BASE + 3 * prep.SLOT_BYTES,
              "FX1_ISP_STREAM_POOL_BASE": prep.POOL_BASE, "FX1_ISP_STREAM_POOL_END": prep.POOL_END,
              "FX1_ISP_STREAM_POOL_ALIGN": prep.POOL_ALIGN, "FX1_ISP_STREAM_DMA_CHANNEL": prep.DMA_CHANNEL,
              "FX1_ISP_STREAM_DMA_CHUNK": prep.DMA_CHUNK, "FX1_ISP_STREAM_DESC_MAGIC": prep.DESC_MAGIC,
              "FX1_ISP_STREAM_STATUS_MAGIC": prep.STATUS_MAGIC, "FX1_ISP_STREAM_CONTROL_MAGIC": prep.CONTROL_MAGIC,
              "FX1_ISP_STREAM_RECORD_MAGIC": prep.RECORD_MAGIC, "FX1_ISP_STREAM_VERSION": prep.VERSION,
              "FX1_ISP_STREAM_FLAG_LOOP": prep.FLAG_LOOP, "FX1_ISP_STREAM_MODE_LEGACY": prep.MODE_LEGACY,
              "FX1_ISP_STREAM_ERROR": prep.STATES.index("ERROR"), "FX1_ISP_STREAM_STOP_ERROR": prep.STOP_REASONS.index("ERROR"),
              "FX1_ISP_STREAM_ERR_HOST": prep.ERRORS.index("HOST"), "FX1_ISP_STREAM_REQ_STOP": prep.REQ_STOP,
              "FX1_ISP_STREAM_REASON_LIMIT": prep.REASON_LIMIT, "FX1_ISP_STREAM_REC_PUBLISHED": prep.REC_PUBLISHED,
              "FX1_ISP_STREAM_COLORIMETRY_VALID": prep.COLORIMETRY_VALID,
              "FX1_ISP_STREAM_UV_OFFSET(1920u * 1080u)": prep.align(1920 * 1080, 0x100000)}
    src = ["#include <stddef.h>", "#include <stdio.h>", '#include "fx1/fx1_isp_stream.h"', "int main(void) {"]
    for key, (name, fields) in blocks.items():
        src.append('printf("size %s %%u\\n", (unsigned)sizeof(%s));' % (key, name))
        for i, f in enumerate(fields):
            member = f if not f.startswith("reserved") else "reserved[%d]" % (i - fields.index(
                next(x for x in fields if x.startswith("reserved"))))
            src.append('printf("off %s %s %%u\\n", (unsigned)offsetof(%s, %s));' % (key, f, name, member))
    for m in macros:
        src.append('printf("macro %s %%lu\\n", (unsigned long)(%s));' % (m.replace(" ", ""), m))
    src += ["return 0;", "}"]
    c, exe = os.path.join(tmp, "abi.c"), os.path.join(tmp, "abi")
    open(c, "w").write("\n".join(src) + "\n")
    r = subprocess.run([cc, "-std=c11", "-pedantic", "-Wall", "-Wextra", "-Werror", "-I", MAP_INC, c, "-o", exe],
                       capture_output=True, text=True)
    check(r.returncode == 0, "ABI: fx1_isp_stream.h compiles as C11 -pedantic, the firmware dialect (%s)" % r.stderr.strip()[:200])
    if r.returncode:
        return
    got = {}
    for line in subprocess.run([exe], capture_output=True, text=True, check=True).stdout.splitlines():
        kind, *rest = line.split()
        got[(kind,) + tuple(rest[:-1])] = int(rest[-1])
    for key, (name, fields) in blocks.items():
        check(got[("size", key)] == prep.SIZES[key] == 4 * len(fields), "ABI: sizeof(%s) = %d" % (name, prep.SIZES[key]))
        bad = [f for i, f in enumerate(fields) if got[("off", key, f)] != 4 * i]
        check(not bad, "ABI: %s field offsets = the Python field order%s" % (name, (" (differ: %s)" % bad) if bad else ""))
    bad = [m for m, v in macros.items() if got[("macro", m.replace(" ", ""))] != v]
    check(not bad, "ABI: %d layout and code macros = the Python mirror%s" % (len(macros), (" (differ: %s)" % bad) if bad else ""))


# ---- 2. layout ----------------------------------------------------------------------
def layout_regions():
    shared = (prep.FW_SHARED_BASE, prep.FW_SHARED_BASE + 0xB00000)
    regs = [("demo job/result/profile", prep.FW_SHARED_BASE, prep.FW_SHARED_BASE + 0x100000, shared),
            ("desc", prep.DESC_ADDR, prep.DESC_ADDR + 128, shared),
            ("status", prep.STATUS_ADDR, prep.STATUS_ADDR + 128, shared),
            ("control", prep.CONTROL_ADDR, prep.CONTROL_ADDR + 64, shared),
            ("records", prep.RECORD_ADDR, prep.RECORD_ADDR + 4 * prep.RECORD_BYTES, shared),
            ("profile", prep.PROFILE_ADDR, prep.PROFILE_ADDR + 8 * prep.PROFILE_MAX, shared),
            ("tpg4k record (camera_isp test)", prep.FW_SHARED_BASE + 0x800000, prep.FW_SHARED_BASE + 0x800100, shared)]
    for i in range(prep.SLOTS):
        regs.append(("chain %d" % i, prep.CHAIN_ADDR + i * prep.CHAIN_PITCH, prep.CHAIN_ADDR + (i + 1) * prep.CHAIN_PITCH, shared))
        regs.append(("input slot %d" % i, prep.FW_RAW_BUF_BASE + i * prep.SLOT_BYTES,
                     prep.FW_RAW_BUF_BASE + (i + 1) * prep.SLOT_BYTES, (prep.FW_RAW_BUF_BASE, prep.FW_YUV_BUF_BASE)))
        regs.append(("output slot %d" % i, prep.FW_YUV_BUF_BASE + i * prep.SLOT_BYTES,
                     prep.FW_YUV_BUF_BASE + (i + 1) * prep.SLOT_BYTES, (prep.FW_YUV_BUF_BASE, prep.FW_TEST_AREA_BASE)))
    regs.append(("dma test buffers", prep.FW_TEST_AREA_BASE + 0x100000, prep.FW_TEST_AREA_BASE + 0x110000 + 1023,
                 (prep.FW_TEST_AREA_BASE, prep.POOL_END)))
    regs.append(("source pool", prep.POOL_BASE, prep.POOL_END, (prep.FW_TEST_AREA_BASE, prep.DDR_BASE + prep.DDR_SIZE)))
    inside = [n for n, lo, hi, (rlo, rhi) in regs if not (rlo <= lo < hi <= rhi)]
    check(not inside, "layout: %d regions inside their firmware regions%s" % (len(regs), inside and " (outside: %s)" % inside or ""))
    overlap = [(a[0], b[0]) for i, a in enumerate(regs) for b in regs[i + 1:] if a[1] < b[2] and b[1] < a[2]]
    check(not overlap, "layout: no two regions overlap%s" % (overlap and " (%s)" % overlap or ""))
    chain = -(-prep.SLOT_BYTES // prep.DMA_CHUNK)
    check(chain * 16 <= prep.CHAIN_PITCH and prep.DMA_CHUNK % 16 == 0 and prep.DMA_CHUNK <= 1023,
          "layout: a full 16 MiB slot is %d SYS_DMA commands of %d bytes, within one chain" % (chain, prep.DMA_CHUNK))


# ---- 3. conversion ----------------------------------------------------------------------
def conversion():
    w, h, stride = 6, 4, 16
    words = {"le16_raw10_msb": lambda v10: v10 << 6, "le16_raw10_has": lambda v10: v10 << 2,
             "le16_raw12_has": lambda v12: v12}
    for fmt in sorted(prep.FORMATS):
        for depth_name, depth in sorted(prep.DEPTHS.items()):
            vals = [[(x * 97 + y * 389 + 5) % (4096 if fmt == "le16_raw12_has" else 1024) for x in range(w)] for y in range(h)]
            data = b"".join(struct.pack("<%dH" % (stride // 2), *([words[fmt](v) for v in row] + [0xFFFF] * (stride // 2 - w)))
                            for row in vals)
            if depth_name == "raw10" and fmt not in prep.RAW10_SOURCES:
                try:
                    prep.normalize(data, fmt, w, h, stride, depth, 2 * w, "t")
                    check(False, "conversion: %s as raw10 is refused" % fmt)
                except prep.PrepError as e:
                    check("needs a RAW10 source" in str(e), "conversion: %s as raw10 is refused" % fmt)
                continue
            out = prep.normalize(data, fmt, w, h, stride, depth, 16, "t")
            want = []
            for row in vals:
                if fmt == "le16_raw12_has":
                    c = row
                else:
                    c = [v if depth else v << 2 for v in row]   # RAW10: native, or normalized to 12 bits
                want.append(struct.pack("<%dH" % w, *c) + bytes(16 - 2 * w))
            check(out == b"".join(want), "conversion: %s -> %s (source stride %d cut, container stride 16)"
                  % (fmt, depth_name, stride))
    bad = struct.pack("<2H", 1 << 6, (1 << 6) | 1) * 2
    try:
        prep.normalize(bad, "le16_raw10_msb", 2, 2, 4, 0, 4, "x.raw")
        check(False, "conversion: a sample with a declared-zero bit set is refused")
    except prep.PrepError as e:
        check("2 samples have bits 0x003F set (first at x=1, y=0)" in str(e),
              "conversion: a sample with a declared-zero bit set is refused, counted, first located (%s)" % e)
    # review S1 R3: 2688x1520 with bad bits at the far end; the count is linear, not one shift per sample
    w, h = 2688, 1520
    for marks, text in (([w * h - 1], "1 sample has bits 0x003F set (first at x=2687, y=1519)"),
                        ([0, 12345, w * h - 1], "3 samples have bits 0x003F set (first at x=0, y=0)")):
        big = bytearray(2 * w * h)
        for k in marks:
            big[2 * k] = 0x01
        t0 = time.monotonic()
        try:
            prep.normalize(bytes(big), "le16_raw10_msb", w, h, 2 * w, 0, 2 * w, "big")
            msg = "accepted"
        except prep.PrepError as e:
            msg = str(e)
        dt = time.monotonic() - t0
        check(text in msg and dt < 3.0, "conversion: 2688x1520, bad bits in %d sample(s) up to the last: %.2f s, \"%s\"" % (len(marks), dt, text))


# ---- 4/5. sessions and refusals ---------------------------------------------------------------
def write(path, data, mode="wb"):
    with open(path, mode) as f:
        f.write(data)


def sessions(tmp):
    fx = os.path.join(tmp, "fixture")
    stream_fixture.main([fx, "--case", "bggr_raw12", "--case", "grbg_raw10", "--width", "40", "--height", "36"])
    case = os.path.join(fx, "bggr_raw12")
    man, prof = os.path.join(case, "manifest.json"), os.path.join(case, "bggr_raw12.csrw")
    m = json.load(open(man))
    m["frames"] = ["frame C.raw", "frame A.raw", "frame B.raw"]          # explicit order, not sorted
    write(man, json.dumps(m), "w")
    out = os.path.join(tmp, "s1")
    rc, log = run(["prepare", "--out", out, "--manifest", man, "--profile", prof, "--repeat", "2", "--queue", "2"])
    check(rc == 0, "session: manifest with relative paths containing spaces, repeat 2, queue 2 (%s)" % log.strip()[-160:])
    if rc:
        return
    d = prep.unpack_block(prep.DESC_FIELDS, open(os.path.join(out, "desc.bin"), "rb").read(), 128, "desc")
    lay = prep.layout({"width": 40, "height": 36}, 3)
    want = {"magic": prep.DESC_MAGIC, "version": 1, "size": 128, "flags": 0, "width": 40, "height": 36, "bayer": 3,
            "depth": 0, "in_stride": 80, "frame_bytes": 80 * 36, "source_count": 3, "source_base": prep.POOL_BASE,
            "source_pitch": 0x1000, "repeat": 2, "frames_lo": 6, "frames_hi": 0, "queue": 2, "mode": 0,
            "profile_id": 1, "out_stride": 48, "frame_budget_us": prep.FRAME_BUDGET_US}
    bad = {k: (d[k], v) for k, v in want.items() if d[k] != v}
    check(not bad and d["profile_count"] == 9 and (d["session_lo"] or d["session_hi"]),
          "session: desc.bin fields (frames = 3 x 2, pitch 4 KiB, profile 9 writes, session id)%s" % (bad and " %s" % bad or ""))
    loads = [l.split(":", 1) for l in open(os.path.join(out, "loads.txt")).read().splitlines()]
    check([int(a, 16) for a, _ in loads] == [prep.DESC_ADDR, prep.PROFILE_ADDR] + [prep.POOL_BASE + k * 0x1000 for k in range(3)]
          and lay["source_pitch"] == 0x1000, "session: loads.txt = desc, profile, then the sources at base + k x pitch")
    info = json.load(open(os.path.join(out, "session.json")))
    names = [s["name"] for s in info["sources"]]
    check(names == ["frame C.raw", "frame A.raw", "frame B.raw"], "session: sources keep the manifest order (%s)" % names)
    ref = open(os.path.join(out, "reference.list")).read().splitlines()
    idx = [ref.index(r) for r in ref]
    check([os.path.basename(r) for r in ref] == ["src_000.isp16"] * 2 + ["src_001.isp16"] * 2 + ["src_002.isp16"] * 2,
          "session: reference.list = C C A A B B (repeat 2)")
    check([prep.identity(n, 3, 2) for n in range(8)] == [(0, 0), (0, 1), (1, 0), (1, 1), (2, 0), (2, 1), (0, 0), (0, 1)],
          "session: identity (n // K) mod S wraps after S x K frames")
    raw_c = open(os.path.join(case, "frame C.raw"), "rb").read()
    check(open(os.path.join(out, "pool", "src_000.isp16"), "rb").read() == prep.normalize(raw_c, "le16_raw10_has", 40, 36, 80, 0, 80, "c"),
          "session: pool/src_000 is frame C converted once")
    check(info["sources"][0]["sha256"] == prep.sha256_bytes(raw_c) and info["profile"]["sha256"] == prep.sha256_file(prof)
          and open(os.path.join(out, "profile.csrw"), "rb").read() == open(prof, "rb").read(),
          "session: hashes of the sources and the profile; the profile is snapshotted")
    # loop and --frames
    rc, _ = run(["prepare", "--out", os.path.join(tmp, "s2"), "--manifest", man, "--profile", prof, "--loop"])
    d = prep.unpack_block(prep.DESC_FIELDS, open(os.path.join(tmp, "s2", "desc.bin"), "rb").read(), 128, "desc")
    check(rc == 0 and d["flags"] == prep.FLAG_LOOP and d["frames_lo"] == 0, "session: --loop sets FLAG_LOOP, frames 0")
    rc, _ = run(["prepare", "--out", os.path.join(tmp, "s3"), "--manifest", man, "--profile", prof, "--frames", "5", "--repeat", "3"])
    d = prep.unpack_block(prep.DESC_FIELDS, open(os.path.join(tmp, "s3", "desc.bin"), "rb").read(), 128, "desc")
    check(rc == 0 and d["frames_lo"] == 5 and d["repeat"] == 3, "session: --frames 5 with repeat 3 (ends inside a repeat group)")
    # quick form: geometry from the options, Bayer and depth from the profile
    g = os.path.join(fx, "grbg_raw10")
    rc, log = run(["prepare", "--out", os.path.join(tmp, "s4"), "--raw", os.path.join(g, "frame A.raw"), os.path.join(g, "frame B.raw"),
                   "--profile", os.path.join(g, "grbg_raw10.csrw"), "--width", "40", "--height", "36"])
    info = json.load(open(os.path.join(tmp, "s4", "session.json"))) if rc == 0 else {}
    inp = info.get("input", {})
    check(rc == 0 and inp.get("bayer") == "GRBG" and inp.get("input_depth") == "raw10" and inp.get("source_format") == "le16_raw10_msb"
          and inp.get("source_format_defaulted") is True and (inp.get("active_width"), inp.get("active_height")) == (38, 36)
          and "le16_raw10_msb (the --raw default" in log
          and json.load(open(os.path.join(tmp, "s4", "manifest.resolved.json")))["frames"][1].endswith("frame B.raw"),
          "session: --raw takes Bayer GRBG and depth raw10 from the profile; manifest.resolved.json written (%s)" % log.strip()[-120:])
    quick_format(tmp, case, prof)
    races(tmp, case, man, prof)
    refusals(tmp, case, man, prof)


def quick_format(tmp, case, prof):
    """The zero-bit check is no packing detection: a right-aligned RAW12 frame passes it as
    le16_raw10_msb (review S1, quick form). prepare must say so, and say when the format is a default."""
    raw12 = os.path.join(tmp, "raw12_right_aligned.raw")
    write(raw12, struct.pack("<H", 0x0400) * (40 * 36))      # RAW12 1024: a valid le16_raw10_msb 16
    for extra, defaulted in (([], True), (["--source-format", "le16_raw10_msb"], False)):
        out = os.path.join(tmp, "q%d" % defaulted)
        rc, log = run(["prepare", "--out", out, "--raw", raw12, "--profile", prof, "--width", "40", "--height", "36"] + extra)
        info = json.load(open(os.path.join(out, "session.json"))) if rc == 0 else {}
        check(rc == 0 and "warning: source(s) 0: every sample is below 0x1000" in log
              and ("(the --raw default)" in log) == defaulted and info["input"]["source_format_defaulted"] is defaulted,
              "quick form: a right-aligned RAW12 file read as le16_raw10_msb%s is accepted with a warning%s (%s)"
              % (" (default)" if defaulted else " (given)", " naming the default" if defaulted else "", log.strip()[-100:]))
    rc, log = run(["prepare", "--out", os.path.join(tmp, "q_ok"), "--raw", os.path.join(case, "..", "grbg_raw10", "frame A.raw"),
                   "--profile", os.path.join(case, "..", "grbg_raw10", "grbg_raw10.csrw"), "--width", "40", "--height", "36"])
    check(rc == 0 and "warning" not in log, "quick form: a real le16_raw10_msb frame gives no warning")


def races(tmp, case, man, prof):
    """A tuner saving the profile, or a RAW being rewritten, right after prepare read it: the session
    must hold one consistent version (review S1 R2). The file changes between the read and any later
    step, so a second read anywhere in prepare would see the new content."""
    orig_read = prep.read_bytes
    base_prof = open(prof, "rb").read()
    raw_a = os.path.join(case, "frame A.raw")
    base_a = open(raw_a, "rb").read()

    def racing(path):
        data = orig_read(path)
        if os.path.abspath(path) == os.path.abspath(prof):
            write(prof, base_prof + b"WB_GAIN_R gain_r=0x200\n")
        elif os.path.abspath(path) == os.path.abspath(raw_a):
            write(raw_a, bytes(len(data)))                       # another valid frame (all zero)
        return data

    out = os.path.join(tmp, "race")
    prep.read_bytes = racing
    try:
        rc, log = run(["prepare", "--out", out, "--manifest", man, "--profile", prof])
    finally:
        prep.read_bytes = orig_read
        write(prof, base_prof)
        write(raw_a, base_a)
    if rc:
        check(False, "race: prepare with files saved during it (%s)" % log.strip()[-120:])
        return
    codec_dir, header = prep.default_paths(None, None)
    codec = prep.load_codec(codec_dir)
    snap = os.path.join(out, "profile.csrw")
    rebuilt = prep.profile_facts(snap, codec, codec.CsrMap(header))["bin"]
    info = json.load(open(os.path.join(out, "session.json")))
    check(open(snap, "rb").read() == base_prof and rebuilt == open(os.path.join(out, "profile.bin"), "rb").read()
          and info["profile"]["sha256"] == prep.sha256_bytes(base_prof)
          and info["profile"]["bin_sha256"] == prep.sha256_bytes(rebuilt),
          "race: a profile saved right after it was read: profile.bin = the compiled snapshot, hashes of that version")
    k = [s["name"] for s in info["sources"]].index("frame A.raw")
    cont = open(os.path.join(out, "pool", "src_%03d.isp16" % k), "rb").read()
    check(info["sources"][k]["sha256"] == prep.sha256_bytes(base_a)
          and cont == prep.normalize(base_a, "le16_raw10_has", 40, 36, 80, 0, 80, "a")
          and info["sources"][k]["container_sha256"] == prep.sha256_bytes(cont),
          "race: a RAW rewritten right after it was read: container and hash are the version checked")


def geometry():
    """The driver's contract (fx1_isp_set_geometry): container even <= 3842x2162, active image after the
    Bayer phase trim 32..3840 x 32..2160 (review S1 R1). Boundaries for every phase."""
    for name, b in sorted(prep.BAYER.items(), key=lambda x: x[1]):
        dx, dy = 2 * (b & 1), 2 * ((b >> 1) & 1)

        def ok(w, h):
            m = {"width": w, "height": h, "bayer": name, "source_format": "le16_raw12_has", "input_depth": "raw12",
                 "frames": []}
            try:
                prep.check_manifest(m, "t")
                return True
            except prep.PrepError:
                return False
        lo, hi = (32 + dx, 32 + dy), (3840 + dx, 2160 + dy)
        got = [ok(*lo), ok(lo[0] - 2, lo[1]), ok(lo[0], lo[1] - 2), ok(*hi), ok(hi[0] + 2, hi[1]), ok(hi[0], hi[1] + 2)]
        check(got == [True, False, False, True, False, False],
              "geometry %s: %dx%d .. %dx%d accepted, 2 less or more on either axis refused %s" % (name, lo[0], lo[1], hi[0], hi[1], got))
    act = [prep.active_geometry(256, 144, b) for b in range(4)]
    check(act == [(256, 144), (254, 144), (256, 142), (254, 142)],
          "geometry: the active image of 256x144 per phase is the output S1 measured on the model %s" % act)


def refusals(tmp, case, man, prof):
    good = json.load(open(man))

    serial = [0]

    def manifest(**kw):
        serial[0] += 1
        m = dict(good)
        m.update(kw)
        for k in [k for k, v in kw.items() if v is None]:
            del m[k]
        p = os.path.join(case, "bad%d.json" % serial[0])
        write(p, json.dumps(m), "w")
        return p

    def profile(text):
        serial[0] += 1
        p = os.path.join(case, "bad%d.csrw" % serial[0])
        write(p, text, "w")
        return p

    write(os.path.join(case, "short.raw"), bytes(10))
    write(os.path.join(case, "dirty.raw"), struct.pack("<H", 0x8001) * (40 * 36))   # bit 0 set: not RAW10 HAS
    nonempty = os.path.join(tmp, "nonempty")
    os.makedirs(nonempty)
    write(os.path.join(nonempty, "keep.txt"), b"user data")
    base_prof = open(prof).read()
    cases = [
        ("empty frame list", ["--manifest", manifest(frames=[])], "non-empty list"),
        ("missing RAW file", ["--manifest", manifest(frames=["frame A.raw", "nope.raw"])], "RAW file not found"),
        ("RAW of the wrong size", ["--manifest", manifest(frames=["short.raw"])], "10 bytes, expected 2880"),
        ("bits declared zero set", ["--manifest", manifest(frames=["frame A.raw", "dirty.raw"])], "bits 0xF003 set"),
        ("odd source_stride", ["--manifest", manifest(source_stride=81)], "source_stride 81 invalid"),
        ("source_stride below the width", ["--manifest", manifest(source_stride=78)], "source_stride 78 invalid"),
        ("unknown key", ["--manifest", manifest(gain=2)], "unknown keys gain"),
        ("missing key", ["--manifest", manifest(bayer=None)], "missing keys bayer"),
        ("version 2", ["--manifest", manifest(version=2)], "version 2"),
        ("unknown Bayer", ["--manifest", manifest(bayer="RGBG")], "bayer 'RGBG'"),
        ("unknown source_format", ["--manifest", manifest(source_format="mipi_raw10")], "source_format 'mipi_raw10'"),
        ("raw10 depth from a RAW12 source", ["--manifest", manifest(source_format="le16_raw12_has", input_depth="raw10")],
         "needs a RAW10 source_format"),
        ("odd geometry", ["--manifest", manifest(width=41)], "geometry 41x36 invalid"),
        ("geometry above 3842x2162", ["--manifest", manifest(width=4096)], "geometry 4096x36 invalid"),
        ("BGGR 32x32: active 30x30 after the phase trim", ["--manifest", manifest(width=32, height=32)],
         "leaves an active image of 30x30 after the Bayer phase trim; the ISP needs 32..3840 x 32..2160"),
        ("profile Bayer differs", ["--manifest", manifest(bayer="RGGB")], "COMMON_BAYER is BGGR, the sequence is RGGB"),
        ("profile depth differs", ["--manifest", manifest(input_depth="raw10")], "input_depth raw10 (depth 1)"),
        ("profile without COMMON_BAYER", ["--manifest", man, "--profile", profile("WB_GAIN_R gain_r=0x100\n")],
         "no COMMON_BAYER write"),
        ("profile geometry differs", ["--manifest", man, "--profile", profile(base_prof + "COMMON_FRAME_WIDTH 64\n")],
         "COMMON_FRAME_WIDTH 64, the sequence is 40"),
        ("profile with an unknown register", ["--manifest", man, "--profile", profile(base_prof + "NOT_A_REG 1\n")],
         "unknown register or field NOT_A_REG"),
        ("profile without writes", ["--manifest", man, "--profile", profile("# nothing\n")], "no CSR writes"),
        ("profile not .csrw", ["--manifest", man, "--profile", man], "not a .csrw file"),
        ("repeat 0", ["--manifest", man, "--repeat", "0"], "K must be >= 1"),
        ("queue 3", ["--manifest", man, "--queue", "3"], "1, 2 or 4"),
        ("frames 0", ["--manifest", man, "--frames", "0"], "N must be >= 1"),
        ("frames and loop together", ["--manifest", man, "--frames", "3", "--loop"], "not allowed with argument"),
        ("--raw options with a manifest", ["--manifest", man, "--width", "40"], "belong to the --raw form"),
        ("--raw with a profile lacking COMMON_BAYER", ["--raw", os.path.join(case, "frame A.raw"), "--width", "40", "--height",
                                                       "36", "--profile", profile("WB_GAIN_R gain_r=0x100\n")],
         "--raw takes the Bayer phase from the profile"),
        ("--raw at the default geometry, file too small", ["--raw", os.path.join(case, "frame A.raw")],
         "2880 bytes, expected 8171520"),
        ("output directory not empty", ["--manifest", man, "--out", nonempty], "not an empty directory"),
    ]
    for what, args, text in cases:
        out = os.path.join(tmp, "refused")
        argv = ["prepare"] + (["--out", out] if "--out" not in args else []) + args
        if "--profile" not in args:
            argv += ["--profile", prof]
        rc, log = run(argv)
        check(rc == 2 and text in log and not os.path.exists(out),
              "refused (exit 2, nothing written): %s -- %s" % (what, log.strip().splitlines()[-1][:150] if log.strip() else "no message"))
        shutil.rmtree(out, ignore_errors=True)
    check(os.listdir(nonempty) == ["keep.txt"], "refused: the non-empty output directory is left as it was")
    rc, log = run(["reference", "--session", os.path.join(tmp, "refused"), "--run-raw", "/bin/false"])
    check(rc == 2 and "not a session" in log, "refused: reference on a directory prepare did not make (%s)" % log.strip()[-80:])
    # the pool budget: 40 sources of 2688x1520 do not fit 254 MiB; checked before any RAW is read
    big = os.path.join(tmp, "big.raw")
    with open(big, "wb") as f:
        f.truncate(2688 * 1520 * 2)
    m = {"version": 1, "width": 2688, "height": 1520, "bayer": "BGGR", "source_format": "le16_raw10_msb",
         "input_depth": "raw12", "frames": [big] * 40}
    write(os.path.join(tmp, "big.json"), json.dumps(m), "w")
    bp = os.path.join(tmp, "big.csrw")
    write(bp, "COMMON_BAYER pattern=3\n", "w")
    rc, log = run(["prepare", "--out", os.path.join(tmp, "big"), "--manifest", os.path.join(tmp, "big.json"), "--profile", bp])
    check(rc == 2 and "40 sources" in log and "(32 such frames) are available" in log,
          "refused: 40 x 2688x1520 exceeds the source pool, the message gives what fits (%s)" % log.strip()[-110:])
    lay = prep.layout({"width": 2688, "height": 1520}, 32)    # the layout alone: no 256 MB of pool files
    check(lay["pool_end"] <= prep.POOL_END < lay["pool_end"] + lay["source_pitch"],
          "layout: 32 x 2688x1520 fit the 254 MiB pool, the most that does (end 0x%08X)" % lay["pool_end"])
    try:
        prep.layout({"width": 2688, "height": 1520}, 33)
        check(False, "layout: 33 x 2688x1520 are refused")
    except prep.PrepError:
        check(True, "layout: 33 x 2688x1520 are refused")
    for geo in ({"width": 3840, "height": 2160}, {"width": 3842, "height": 2162, "bayer": "BGGR"}):
        try:
            prep.layout(geo, 1)
            check(True, "layout: one %dx%d frame fits a 16 MiB slot and one SYS_DMA chain" % (geo["width"], geo["height"]))
        except prep.PrepError as e:
            check(False, "layout: one %dx%d frame fits a 16 MiB slot and one SYS_DMA chain (%s)" % (geo["width"], geo["height"], e))


# ---- 6. reference ------------------------------------------------------------------------------
def reference(tmp, run_raw):
    fx = os.path.join(tmp, "fixture_ref")
    stream_fixture.main([fx, "--case", "bggr_raw10", "--case", "bggr_raw12", "--width", "64", "--height", "48"])
    hashes = {}
    for c in ("bggr_raw10", "bggr_raw12"):
        s = os.path.join(tmp, "ref_" + c)
        rc, log = run(["prepare", "--out", s, "--manifest", os.path.join(fx, c, "manifest.json"),
                       "--profile", os.path.join(fx, c, c + ".csrw"), "--frames", "4"])
        if c == "bggr_raw12":   # the tuner edits the profile after prepare: the session keeps its snapshot
            with open(os.path.join(fx, c, c + ".csrw"), "a") as f:
                f.write("WB_GAIN_R gain_r=0x300\n")
        rc2, log2 = run(["reference", "--session", s, "--run-raw", run_raw])
        check(rc == 0 and rc2 == 0, "reference: %s, 4 frames (%s)" % (c, (log + log2).strip()[-120:]))
        if rc2 == 0:
            hashes[c] = [f["sha256"] for f in json.load(open(os.path.join(s, "reference.json")))["frames"]]
    if len(hashes) == 2:
        check(hashes["bggr_raw10"] == hashes["bggr_raw12"], "reference: RAW10 native = RAW12 normalized, frame by frame (and from the profile snapshot, not the edited file)")
        h = hashes["bggr_raw12"]
        check(len(set(h[:3])) == 3 and h[3] != h[0], "reference: A, B, C differ, and A after C (loop wrap) differs from the first A")


def reference_bounds(tmp, run_raw):
    """The smallest geometry prepare accepts, per Bayer phase, completes on the model, with the active
    image as output geometry (review S1 R1: prepare and the ISP agree on the boundary)."""
    for case in ("rggb_raw12", "grbg_raw10", "gbrg_raw12", "bggr_raw10"):
        b = prep.BAYER[case[:4].upper()]
        w, h = 32 + 2 * (b & 1), 32 + 2 * ((b >> 1) & 1)
        fx = os.path.join(tmp, "fixture_min_" + case)
        stream_fixture.main([fx, "--case", case, "--width", str(w), "--height", str(h)])
        s = os.path.join(tmp, "min_" + case)
        rc, log = run(["prepare", "--out", s, "--manifest", os.path.join(fx, case, "manifest.json"),
                       "--profile", os.path.join(fx, case, case + ".csrw"), "--frames", "1"])
        rc2, log2 = run(["reference", "--session", s, "--run-raw", run_raw]) if rc == 0 else (rc, "")
        ref = json.load(open(os.path.join(s, "reference.json"))) if rc2 == 0 else {}
        d = prep.unpack_block(prep.DESC_FIELDS, open(os.path.join(s, "desc.bin"), "rb").read(), 128, "desc") if rc == 0 else {}
        check(rc == 0 and rc2 == 0 and (ref.get("width"), ref.get("height")) == (32, 32) and d["out_stride"] == 32
              and not os.path.exists(os.path.join(s, "reference", "packed")),
              "reference: %s at its smallest accepted %dx%d completes, output 32x32 at out_stride 32 as run_raw%s (%s)"
              % (case, w, h, " (row padding cut for the model)" if (2 * w) % 16 else "", (log + log2).strip()[-100:]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default=os.environ.get("CC") or "cc")
    ap.add_argument("--run-raw")
    a = ap.parse_args()
    tmp = tempfile.mkdtemp(prefix="stream_prep_")
    try:
        abi(a.cc, tmp)
        layout_regions()
        conversion()
        geometry()
        sessions(tmp)
        if a.run_raw:
            reference(tmp, a.run_raw)
            reference_bounds(tmp, a.run_raw)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print("%s: %d failure(s)" % (os.path.basename(__file__), len(failures)))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
