#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""fx1_isp_run_raw --dump-demosaic against the Python reference (decisions §12).

    test_demosaic_capture.py RUN_RAW VECTORS_DIR OUT_DIR

  - every R, G, B component of each frame's capture equals the reference tap
    (tools/demosaic_check.py) on vectors exercising each block before the tap
    (BLC, LSC, BPC, WB, the four Bayer patterns, RAW10 and RAW12) and a CH0
    crop that ends before the shared pipeline;
  - RAW10 native and RAW12 normalized of one scene give the same capture;
  - frame selection: three different inputs in sequence (queue 1 and 2):
    PREFIX.fNNN.demosaic = reference frame NNN, PREFIX.demosaic = the last;
  - the capture changes no output: NV12, statistics and the time-stamped
    event log equal a run without it;
  - a used capture directory is refused (exit 2) and left as it was;
  - demosaic_validate.py rejects a truncated file, a 13-bit component, a
    corrupted PNG, a wrong SHA-256 and a wrong status.
"""

import filecmp
import json
import os
import shutil
import struct
import subprocess
import sys
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.join(HERE, "..", "..", "tools")
sys.path.insert(0, TOOLS)
import demosaic_validate  # noqa: E402

failures = []


def check(cond, what):
    if not cond:
        failures.append(what)
        print("FAIL: " + what)


def run(cmd, **kw):
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, **kw)


def meta(vdir):
    w, h, ow, oh, frames = (int(v) for v in open(os.path.join(vdir, "meta.txt")).read().split())
    return w, h, frames


def run_raw(rr, out, inputs, w, h, profile, frames, extra=()):
    args = [rr, "--width", str(w), "--height", str(h), "--profile", profile, "--frames", str(frames), "--out", out]
    for i in inputs:
        args += ["--input", i]
    return run(args + list(extra))


def demosaic_check(capture_dir, inputs, w, h, profile, frames):
    args = [sys.executable, os.path.join(TOOLS, "demosaic_check.py"), "--capture-dir", capture_dir, "--width", str(w),
            "--height", str(h), "--profile", profile, "--frames", str(frames)]
    for i in inputs:
        args += ["--input", i]
    return run(args)


def main():
    rr, vectors, out = sys.argv[1], sys.argv[2], sys.argv[3]
    out = os.path.join(out, "demosaic capture")  # a space in every path
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)

    # 1. Reference match, and off/on identity of every other output.
    cases = ["full_bggr_66x34", "blc_profile3_grbg_64x32", "lsc_bggr_96x64", "bpc_hot_rggb_80x48",
             "tune_wb_dms_csc_full_look_cnf_64x48", "tune_bpc_lsc_sectors_80x48",
             "mandatory_rggb_64x32", "grbg_gamma22_128x72", "gbrg_ccm_gate_64x36", "input_raw10_full_bggr_66x34",
             "input_raw12_full_bggr_66x34", "crop_ch0_vga_bggr_720x528", "full_chain_bggr_2frames_130x74",
             "gtm_auto_3frames_96x64"]
    for name in cases:
        v = os.path.join(vectors, name)
        w, h, frames = meta(v)
        inp, prof = [os.path.join(v, "input.bin")], os.path.join(v, "profile.csrw")
        stats = ["--stats-script", os.path.join(v, "expected_stats.txt")]
        off = run_raw(rr, os.path.join(out, name + ".off"), inp, w, h, prof, frames, stats)
        on = run_raw(rr, os.path.join(out, name), inp, w, h, prof, frames, stats + ["--dump-demosaic", "--per-frame"])
        check(off.returncode == 0, name + ": run without capture: " + off.stdout[-300:])
        check(on.returncode == 0, name + ": run with capture: " + on.stdout[-300:])
        for ext in (".nv12", ".stats.txt", ".events.log"):
            check(filecmp.cmp(os.path.join(out, name + ".off" + ext), os.path.join(out, name + ext), shallow=False),
                  name + ": " + ext + " differs with the capture on")
        r = demosaic_check(os.path.join(out, name + ".demosaic.d"), inp, w, h, prof, frames)
        check(r.returncode == 0, name + ": reference comparison:\n" + r.stdout)
        last = "%s.f%03d.demosaic" % (os.path.join(out, name), frames - 1)
        for ext in (".rgb16", ".png", ".json"):
            check(filecmp.cmp(last + ext, os.path.join(out, name + ".demosaic" + ext), shallow=False),
                  name + ": PREFIX.demosaic%s is not the last frame's" % ext)
        check(not demosaic_validate.validate(os.path.join(out, name + ".demosaic")), name + ": validation")
        print("%s: %s" % (name, "MATCH" if r.returncode == 0 else "MISMATCH"))

    # 2. RAW10 native and RAW12 normalized of the same scene: one capture.
    a = open(os.path.join(out, "input_raw10_full_bggr_66x34.demosaic.rgb16"), "rb").read()
    b = open(os.path.join(out, "input_raw12_full_bggr_66x34.demosaic.rgb16"), "rb").read()
    check(a == b, "RAW10 and RAW12 of one scene give different captures")

    # 3. Frame selection with three different inputs, queue 1 and 2.
    v = os.path.join(vectors, "mandatory_rggb_64x32")
    prof = os.path.join(v, "profile.csrw")
    inputs = [os.path.join(vectors, n, "input.bin") for n in
              ("mandatory_rggb_64x32", "blc_profile0_trims_noscale_64x32", "lsc_saturate_64x32")]
    for q in (1, 2):
        p = os.path.join(out, "sequence_q%d" % q)
        r = run_raw(rr, p, inputs, 64, 32, prof, 3, ["--queue", str(q), "--dump-demosaic", "--per-frame"])
        check(r.returncode == 0, "sequence queue %d: %s" % (q, r.stdout[-300:]))
        c = demosaic_check(p + ".demosaic.d", inputs, 64, 32, prof, 3)
        check(c.returncode == 0, "sequence queue %d: reference comparison:\n%s" % (q, c.stdout))
        data = [open("%s.f%03d.demosaic.rgb16" % (p, k), "rb").read() for k in range(3)]
        check(len(set(data)) == 3, "sequence queue %d: the three frames are not distinct" % q)
        check(open(p + ".demosaic.rgb16", "rb").read() == data[2], "sequence queue %d: PREFIX.demosaic is not frame 2" % q)
        ids = [json.load(open("%s.f%03d.demosaic.json" % (p, k)))["frame"]["frame_id"] for k in range(3)]
        check(ids == [1, 2, 3], "sequence queue %d: frame IDs %s" % (q, ids))

    # 4. A used directory is refused, its content untouched.
    p = os.path.join(out, "full_bggr_66x34")
    before = open(p + ".demosaic.d/index.jsonl").read()
    w, h, frames = meta(os.path.join(vectors, "full_bggr_66x34"))
    r = run_raw(rr, p, [os.path.join(vectors, "full_bggr_66x34", "input.bin")], w, h,
                os.path.join(vectors, "full_bggr_66x34", "profile.csrw"), frames, ["--dump-demosaic"])
    check(r.returncode == 2 and "not empty" in r.stdout, "a used capture directory is not refused: " + r.stdout[-300:])
    check(open(p + ".demosaic.d/index.jsonl").read() == before, "the refused directory was changed")

    # 4b. The frames complete but the capture cannot be selected (a directory
    # in the way of PREFIX.demosaic.png): exit 5, the NV12 as without it.
    v = os.path.join(vectors, "full_bggr_66x34")
    p = os.path.join(out, "blocked")
    os.makedirs(p + ".demosaic.png")
    r = run_raw(rr, p, [os.path.join(v, "input.bin")], w, h, os.path.join(v, "profile.csrw"), frames, ["--dump-demosaic"])
    check(r.returncode == 5 and "demosaic capture FAILED" in r.stdout, "capture failure: exit %d: %s" % (r.returncode, r.stdout[-400:]))
    check(filecmp.cmp(p + ".nv12", os.path.join(out, "full_bggr_66x34.off.nv12"), shallow=False),
          "capture failure: NV12 differs")
    check(not os.path.exists(p + ".demosaic.json"), "capture failure: a JSON was published")

    # 5. The validator rejects broken captures.
    src = os.path.join(out, "full_chain_bggr_2frames_130x74.demosaic")
    check(not demosaic_validate.validate(src), "the reference capture is not valid")

    def broken(tag, edit, message):
        dst = os.path.join(out, "broken_" + tag)
        for ext in (".rgb16", ".png", ".json"):
            shutil.copyfile(src + ext, dst + ext)
        edit(dst)
        r = run([sys.executable, os.path.join(TOOLS, "demosaic_validate.py"), dst])
        check(r.returncode == 1 and message in r.stdout,
              "validator on a capture with %s: exit %d, expected 1 and %r: %s" % (tag, r.returncode, message, r.stdout))

    def truncate(d):
        with open(d + ".rgb16", "r+b") as f:
            f.truncate(os.path.getsize(d + ".rgb16") - 6)

    def thirteen_bits(d):
        data = bytearray(open(d + ".rgb16", "rb").read())
        data[1] |= 0x10
        open(d + ".rgb16", "wb").write(data)
        m = json.load(open(d + ".json"))
        import hashlib
        m["sha256"] = hashlib.sha256(data).hexdigest()  # consistent SHA: the range check must report it
        json.dump(m, open(d + ".json", "w"))

    def png_byte(d):
        data = bytearray(open(d + ".png", "rb").read())
        data[len(data) // 2] ^= 0x01
        open(d + ".png", "wb").write(data)

    def sha(d):
        m = json.load(open(d + ".json"))
        m["sha256"] = "0" * 64
        json.dump(m, open(d + ".json", "w"))

    def status(d):
        m = json.load(open(d + ".json"))
        m["status"] = "frame_reset"
        json.dump(m, open(d + ".json", "w"))

    def png_pixel(d):  # a well-formed PNG whose one pixel is not v >> 4
        data = open(d + ".png", "rb").read()
        probs = []
        px, w, h = demosaic_validate.png_pixels(data, probs)
        px[3 * w + 1] ^= 0x01
        raw = b"".join(b"\x00" + bytes(px[y * 3 * w:(y + 1) * 3 * w]) for y in range(h))

        def chunk(kind, body):
            return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)
        png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
               chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))
        open(d + ".png", "wb").write(png)

    for tag, edit, message in (("PNG pixel", png_pixel, "differ from v >> 4"),("truncation", truncate, "truncated"), ("13-bit component", thirteen_bits, "> 4095"),
                               ("PNG corruption", png_byte, "PNG:"), ("wrong SHA-256", sha, "JSON has"),
                               ("wrong status", status, "status 'frame_reset'")):
        broken(tag.replace(" ", "_"), edit, message)

    # 5b. CH0 is a 64 x 32 crop of the first rows of a 720 x 528 frame: its
    # DONE comes long before the shared pipeline has the last rows; the last
    # frame's capture exists only because run_raw waits for its retirement.
    v = os.path.join(vectors, "crop_ch0_vga_bggr_720x528")
    prof = os.path.join(out, "early_crop.csrw")
    keep = [l for l in open(os.path.join(v, "profile.csrw")) if not l.startswith(("CH0_CROP", "RESIZER_CTRL"))]
    open(prof, "w").write("".join(keep) + "CH0_CROP_ORIGIN 0x0\nCH0_CROP_SIZE 0x200040\nCH0_CROP_CTRL 0x1\n")
    p = os.path.join(out, "early_crop")
    inp = [os.path.join(v, "input.bin")]
    r = run_raw(rr, p, inp, 720, 528, prof, 2, ["--dump-demosaic"])
    check(r.returncode == 0 and "NV12 64x32, 2/2" in r.stdout and "2 completed, 0 not completed" in r.stdout,
          "early-ending crop: " + r.stdout[-400:])
    c = demosaic_check(p + ".demosaic.d", inp, 720, 528, prof, 2)
    check(c.returncode == 0, "early-ending crop: reference comparison:\n" + c.stdout)

    # 5c. Large frames, CH0 = 64 x 32 at the top (review DMS-2 R1): the
    # shared pipeline needs many ms after CH0 DONE; 1080p two frames queue 2,
    # and 4K. Synthetic input (tools/gen_4k_vectors.py pattern, RGGB, the
    # defaults, GTM bypassed).
    sys.path.insert(0, os.path.join(TOOLS, "..", "reference"))
    import gen_4k_vectors
    prof = os.path.join(out, "top_crop.csrw")
    open(prof, "w").write("COMMON_BAYER 0x0\nCH0_CROP_ORIGIN 0x0\nCH0_CROP_SIZE 0x200040\nCH0_CROP_CTRL 0x1\n")
    for w, h, frames, q in ((1920, 1080, 2, 2), (3840, 2160, 1, 1)):
        tag = "top_crop_%dx%d" % (w, h)
        inp = [os.path.join(out, tag + ".isp16")]
        gen_4k_vectors.pattern(w, h).astype("<u2").tofile(inp[0])
        off = run_raw(rr, os.path.join(out, tag + ".off"), inp, w, h, prof, frames, ["--queue", str(q)])
        r = run_raw(rr, os.path.join(out, tag), inp, w, h, prof, frames, ["--queue", str(q), "--dump-demosaic"])
        check(off.returncode == 0 and r.returncode == 0 and ("NV12 64x32, %d/%d" % (frames, frames)) in r.stdout and
              ("%d completed, 0 not completed" % frames) in r.stdout, tag + ": " + r.stdout[-400:])
        check(filecmp.cmp(os.path.join(out, tag + ".off.nv12"), os.path.join(out, tag + ".nv12"), shallow=False),
              tag + ": NV12 differs with the capture on")
        c = demosaic_check(os.path.join(out, tag + ".demosaic.d"), inp, w, h, prof, frames)
        check(c.returncode == 0, tag + ": reference comparison:\n" + c.stdout)
        print("%s: %s" % (tag, "MATCH" if c.returncode == 0 else "MISMATCH"))

    # 5d. The checker never passes without comparing (review DMS-2 R2).
    name = "full_chain_bggr_2frames_130x74"
    v = os.path.join(vectors, name)
    w, h, frames = meta(v)
    inp, prof = [os.path.join(v, "input.bin")], os.path.join(v, "profile.csrw")
    good = os.path.join(out, name + ".demosaic.d")
    entries = [json.loads(l) for l in open(os.path.join(good, "index.jsonl"))]

    def fake(tag, lines):
        d = os.path.join(out, "checker_" + tag)
        os.makedirs(d)
        open(os.path.join(d, "index.jsonl"), "w").write("".join(json.dumps(e) + "\n" for e in lines))
        return d

    reset = dict(entries[0], status="frame_reset", base="", sha256="")
    for tag, d, fr, code, message in (
            ("empty index", fake("empty", []), frames, 1, "no completed capture"),
            ("only reset frames", fake("reset", [reset, dict(reset, ordinal=2)]), frames, 1, "no completed capture"),
            ("a missing frame", fake("missing", entries[:1]), frames, 1, "1 completed capture(s), 2 frame(s) expected"),
            ("a missing frame, --frames omitted", fake("missing_default", entries[:1]), None, 0, "MATCH"),
            ("--frames 0", good, 0, 2, ""), ("--frames -1", good, -1, 2, "")):
        args = [sys.executable, os.path.join(TOOLS, "demosaic_check.py"), "--capture-dir", d, "--width", str(w),
                "--height", str(h), "--profile", prof]
        args += ["--input", inp[0]] + ([] if fr is None else ["--frames", str(fr)])
        r = run(args)
        check(r.returncode == code and message in r.stdout + (r.stderr or ""),
              "checker with %s: exit %d (expected %d), %s" % (tag, r.returncode, code, r.stdout))
    # Omitted --frames means one frame per input (as run_raw), not "whatever was captured":
    r = run([sys.executable, os.path.join(TOOLS, "demosaic_check.py"), "--capture-dir", fake("one_of_two", entries[:1]),
             "--width", str(w), "--height", str(h), "--profile", prof, "--input", inp[0], "--input", inp[0]])
    check(r.returncode == 1 and "2 frame(s) expected" in r.stdout, "checker, 2 inputs, 1 capture: " + r.stdout)

    # 5e. One --input-list for runner and checker (review DMS-2 R3): comment
    # and empty lines, file names with spaces, three different inputs.
    d = os.path.join(out, "list inputs")
    os.makedirs(d)
    names = ("mandatory_rggb_64x32", "blc_profile0_trims_noscale_64x32", "lsc_saturate_64x32")
    paths = []
    for n in names:
        paths.append(os.path.join(d, "frame of " + n + ".isp16"))
        shutil.copyfile(os.path.join(vectors, n, "input.bin"), paths[-1])
    lst = os.path.join(d, "sequence list.txt")
    open(lst, "w").write("# RAW sequence\n\n" + paths[0] + "\n# second\n" + paths[1] + "\n\n" + paths[2] + "\n")
    prof = os.path.join(vectors, "mandatory_rggb_64x32", "profile.csrw")
    p = os.path.join(out, "listed")
    r = run([rr, "--input-list", lst, "--width", "64", "--height", "32", "--profile", prof, "--out", p, "--dump-demosaic"])
    check(r.returncode == 0 and "3 completed" in r.stdout, "input list: run_raw: " + r.stdout[-300:])
    c = run([sys.executable, os.path.join(TOOLS, "demosaic_check.py"), "--capture-dir", p + ".demosaic.d", "--input-list",
             lst, "--width", "64", "--height", "32", "--profile", prof])
    check(c.returncode == 0 and c.stdout.count("MATCH") == 3, "input list: checker:\n" + c.stdout)

    # 6. The reference comparison finds one wrong component and locates it.
    name = "full_chain_bggr_2frames_130x74"
    v = os.path.join(vectors, name)
    w, h, frames = meta(v)
    src_dir, bad_dir = os.path.join(out, name + ".demosaic.d"), os.path.join(out, "corrupt.demosaic.d")
    shutil.copytree(src_dir, bad_dir)
    lines = [json.loads(l) for l in open(os.path.join(bad_dir, "index.jsonl"))]
    for e in lines:
        e["base"] = e["base"].replace(src_dir, bad_dir)
    open(os.path.join(bad_dir, "index.jsonl"), "w").write("".join(json.dumps(e) + "\n" for e in lines))
    base = lines[1]["base"]
    cw = json.load(open(base + ".json"))["geometry"]["width"]
    data = bytearray(open(base + ".rgb16", "rb").read())
    i = 2 * (3 * (3 * cw + 5) + 1)  # x 5, y 3, G
    data[i] ^= 0x01
    open(base + ".rgb16", "wb").write(data)
    m = json.load(open(base + ".json"))
    import hashlib
    m["sha256"] = hashlib.sha256(data).hexdigest()  # a self-consistent capture with one wrong value
    json.dump(m, open(base + ".json", "w"))
    r = demosaic_check(bad_dir, [os.path.join(v, "input.bin")], w, h, os.path.join(v, "profile.csrw"), frames)
    check(r.returncode == 1 and "frame 1: x 5, y 3, G:" in r.stdout and "frame 1: 1 mismatching" in r.stdout
          and "frame 0 (capture 1" in r.stdout and "MATCH" in r.stdout,
          "the reference comparison misses or mislocates a wrong component:\n" + r.stdout)

    print("test_demosaic_capture: %d failure(s)" % len(failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
