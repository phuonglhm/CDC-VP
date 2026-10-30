#!/usr/bin/env python3
"""FX1 ISP RAW-frame demo on the whole platform.

One RAW frame goes through the complete FX1 SoC VP, the way firmware will
drive it on the chip:

  host:  RAW file -> ISP 16-bit container (isp/tools/raw_fixture.py)
         .csrw profile -> {offset, value} blob, plus a job descriptor
         fx1_soc --load: RAW, profile and job into DDR before reset
  VP:    hart 0 runs fw/fx1_soc/demo/isp_raw_frame (reference C99 driver):
         init -> geometry -> profile -> start -> queue -> ISP interrupt
         (PLIC) -> statistics -> result block; the ISP's IDMA reads the RAW
         from DDR and its ODMA writes NV12 back, over the FX1 bus
  host:  fx1_soc --dump: result block and NV12 -> platform.nv12, platform.png
         reference: the same input and profile through the stand-alone ISP
         model (fx1_isp_run_raw); the two NV12 SHA-256 must be equal

Example (BGGR 2688x1520 frame of the project dataset, 'full' test profile):

  platforms/fx1_soc/demo/run_isp_demo.py \\
      --raw "<dataset>/outdoor_hightlight_raw/khoid_outdoor1_raw_2688x1520_5376.raw" \\
      --out /tmp/isp_demo

Interactive variant, the frame processed by `isp run` in the FreeRTOS CLI:
  --interactive   all in one: prepare the job, start the FreeRTOS CLI image in
                  the background, open the console in this terminal (type
                  `isp run`, `isp stats`, `poweroff`), then pack and compare
  --prepare-only  steps 1-2 only, then print the fx1_soc command line that
                  loads the job and dumps the result block and NV12 at poweroff
  --pack-only     steps 4-5 only, from the result.bin/y.bin/uv.bin that
                  command dumped into --out; geometry and frame count come
                  from its job.bin, --profile must be the same file

Exit status: 0 the platform ran and (unless --no-reference) matches the
reference, 1 mismatch, 2 a step failed, 3 usage error.
"""
import argparse
import os
import re
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import isp_pack  # noqa: E402  (stdlib packer and checks, shared with the SDK)
from isp_pack import BAYER, JOB_MAGIC, MODE_LEGACY, MODE_PROFILE, RESULT_FIELDS, VERSION, PackError, sha256  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
ISP = os.path.join(ROOT, "components", "FX1_Components", "isp")
MAP_H = os.path.join(ROOT, "components", "FX1_Components", "fx1_map", "include", "fx1", "fx1_memory_map.h")
CSR_H = os.path.join(ISP, "include", "fx1_isp", "fx1_isp_csr.h")
DEFAULT_PROFILE = os.path.join(ISP, "samples", "profiles", "outdoor1_bggr_2688x1520_full.csrw")
# isp_demo_job.h: where the job, the result block and the profile live in the shared region
JOB_OFFSET, RESULT_OFFSET, PROFILE_OFFSET = 0x0000, 0x0100, 0x1000


def fail(msg, code=2):
    print("run_isp_demo: " + msg, file=sys.stderr)
    sys.exit(code)


def align(value, to):
    return (value + to - 1) // to * to


def map_constant(name):
    """A FX1_U32(...) constant of fx1_memory_map.h (one source of addresses)."""
    m = re.search(r"#define\s+%s\s+FX1_U32\((0x[0-9A-Fa-f]+|\d+)\)" % name, open(MAP_H).read())
    if not m:
        fail("%s not found in %s" % (name, MAP_H))
    return int(m.group(1), 0)


def csr_offsets():
    return {m.group(1): int(m.group(2), 16)
            for m in re.finditer(r"#define FX1_ISP_(\w+)_OFFSET (0x[0-9A-Fa-f]+)u", open(CSR_H).read())}


def load_profile(path):
    """Same rules as the ISP's read_profile(): '#' comments, '<REG> <value>'."""
    offsets, writes = csr_offsets(), []
    for n, line in enumerate(open(path), 1):
        tokens = line.split("#", 1)[0].split()
        if len(tokens) < 2:
            continue
        if tokens[0] not in offsets:
            fail("%s:%d: unknown register %s" % (path, n, tokens[0]))
        writes.append((offsets[tokens[0]], int(tokens[1], 0) & 0xFFFFFFFF))
    return writes


def run(cmd, log=None):
    print("+ " + " ".join(cmd), flush=True)
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if log:
        with open(log, "w") as f:
            f.write(r.stdout)
    return r


def port_listening(port):
    """True when some socket listens on TCP port (no connect: the VP would take it as its client)."""
    for table in ("/proc/net/tcp", "/proc/net/tcp6"):
        try:
            rows = open(table).read().splitlines()[1:]
        except OSError:
            continue
        for row in rows:
            cols = row.split()
            if cols[3] == "0A" and int(cols[1].rsplit(":", 1)[1], 16) == port:
                return True
    return False


def run_interactive(cmd, port, log):
    """Start the VP in the background, attach the console here, return once the VP has ended."""
    if port_listening(port):
        fail("TCP port %d is already in use (another fx1_soc still running?); pick one with --port" % port)
    print("+ " + " ".join(cmd) + "  > " + log, flush=True)
    with open(log, "w") as f:
        vp = subprocess.Popen(cmd, stdout=f, stderr=subprocess.STDOUT)
    try:
        for _ in range(300):
            if vp.poll() is not None or port_listening(port):
                break
            time.sleep(0.1)
        if vp.poll() is not None:
            fail("the platform exited before listening (exit %d), see %s" % (vp.returncode, log))
        print("\nType `isp run`, `isp stats`, then `poweroff` (the dumps are written at poweroff).\n", flush=True)
        subprocess.call([os.path.join(ROOT, "platforms", "fx1_soc", "tools", "fx1_console.sh"), str(port)])
        try:
            vp.wait(timeout=5)
        except subprocess.TimeoutExpired:
            fail("\nconsole closed before `poweroff`: platform stopped, nothing to pack")
    finally:
        if vp.poll() is None:
            vp.terminate()
            vp.wait()
    print("")
    if vp.returncode:
        fail("platform run failed (exit %d), see %s" % (vp.returncode, log))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--raw", help="RAW frame file (not needed with --pack-only)")
    ap.add_argument("--format", default="le16_raw10_msb", help="RAW format (raw_fixture.py), default le16_raw10_msb")
    ap.add_argument("--width", type=int, default=2688)
    ap.add_argument("--height", type=int, default=1520)
    ap.add_argument("--bayer", default="BGGR", choices=sorted(BAYER))
    ap.add_argument("--src-stride", type=int, help="RAW file stride in bytes (default 2 x width)")
    ap.add_argument("--profile", default=DEFAULT_PROFILE, help=".csrw profile (default: outdoor1 'full')")
    ap.add_argument("--frames", type=int, default=1, help="process the frame N times (temporal blocks)")
    ap.add_argument("--mode", default="profile", choices=("profile", "legacy"),
                    help="ISP mode of the job (CSR v1.07): profile (default; the profile is built into bank 0, "
                         "sealed and applied) or legacy, for comparison. Either is compared with the stand-alone "
                         "model's legacy run of the same profile")
    ap.add_argument("--expect-mode", choices=("profile", "legacy"),
                    help="--pack-only: the mode the console ran (`isp run legacy`), default the job's")
    ap.add_argument("--outputs", type=lambda v: int(v, 0), default=1,
                    help="OUTPUT_ENABLE channel mask (bit 0 CH0 NV12, required; bit 1 CH1 NV12; bit 2 CH2 RGB888); "
                         "CH1/CH2 are dumped and reported, CH0 is compared")
    ap.add_argument("--build-dir", default=os.path.join(ROOT, "build-fx1"))
    ap.add_argument("--out", required=True, help="output directory")
    ap.add_argument("--timeout-ms", type=int, default=5000, help="simulated-time budget")
    ap.add_argument("--no-reference", action="store_true", help="skip the stand-alone ISP comparison")
    ap.add_argument("--prepare-only", action="store_true",
                    help="only write input.isp16, profile.bin and job.bin, and print the command that starts "
                         "the FreeRTOS CLI image with them loaded (then type `isp run` in the console)")
    ap.add_argument("--interactive", action="store_true",
                    help="prepare, run the FreeRTOS CLI image with the console in this terminal, then pack "
                         "and compare after `poweroff`")
    ap.add_argument("--port", type=int, default=5000, help="TCP port of the console (--interactive, --prepare-only)")
    ap.add_argument("--job-env", metavar="FILE",
                    help="with --prepare-only: also write the --load/--dump addresses and lengths as shell "
                         "variables to FILE (the SDK's run_vp.sh reads them)")
    ap.add_argument("--pack-only", action="store_true",
                    help="only pack, preview and compare the result.bin, y.bin and uv.bin already dumped into "
                         "--out by the --prepare-only command")
    a = ap.parse_args()
    if a.frames < 1:
        fail("--frames must be >= 1", 3)
    if not (a.outputs & 1) or a.outputs > 7:
        fail("--outputs must include CH0 (bit 0) and only bits 0..2", 3)
    if a.prepare_only + a.pack_only + a.interactive > 1:
        fail("--prepare-only, --pack-only and --interactive exclude each other", 3)
    if a.job_env and not a.prepare_only:
        fail("--job-env needs --prepare-only", 3)
    if not a.pack_only and not a.raw:
        fail("--raw is required (except with --pack-only)", 3)

    sim = os.path.join(a.build_dir, "platforms", "fx1_soc", "fx1_soc")
    elf = os.path.join(a.build_dir, "fw", "fx1_soc", "freertos_cli.elf" if a.prepare_only or a.interactive else "isp_demo.elf")
    ref_tool = os.path.join(a.build_dir, "components", "FX1_Components", "isp", "fx1_isp_run_raw")
    out = lambda name: os.path.join(a.out, name)
    if a.pack_only:
        needed = [(out(n), "dump of the --prepare-only run") for n in ("result.bin", "y.bin", "uv.bin")]
        needed.append((out("job.bin"), "--prepare-only job descriptor"))
        if not a.no_reference:
            needed.append((out("input.isp16"), "--prepare-only input"))
    else:
        needed = [(sim, "simulator"), (elf, "firmware"), (a.raw, "RAW frame")]
    for name in ("platform.nv12", "platform.png", "summary.txt"):  # a failed run leaves none of an earlier one
        if os.path.isfile(out(name)):
            os.remove(out(name))
    for path, what in needed + [(a.profile, "profile")]:
        if not os.path.exists(path):
            fail("%s not found: %s (%s)" % (what, path, "run --prepare-only and the VP first" if a.pack_only
                                             else "build target fx1_all first"))
    os.makedirs(a.out, exist_ok=True)
    if a.pack_only:  # pack exactly what was prepared, whatever the command line says
        try:
            job = isp_pack.read_job(out("job.bin"))
        except PackError as e:
            fail(str(e))
        a.width, a.height, a.frames = job["width"], job["height"], job["frames"]
        a.bayer = next(k for k, v in BAYER.items() if v == job["bayer"])
        a.mode = "legacy" if job["mode"] == MODE_LEGACY else "profile"
        a.outputs = job["outputs"]

    shared, raw_base, yuv_base = (map_constant(n) for n in
                                  ("FX1_FW_SHARED_BASE", "FX1_FW_RAW_BUF_BASE", "FX1_FW_YUV_BUF_BASE"))
    in_stride = align(2 * a.width, 16)   # IDMA stride: multiple of the 16-byte AXI beat
    out_stride = align(a.width, 16)      # >= output width (the resizer only reduces)
    y_addr = yuv_base
    uv_addr = yuv_base + align(out_stride * a.height, 1 << 20)
    if raw_base + in_stride * a.height > yuv_base:
        fail("frame too large for the RAW buffer region")
    # CH1 (NV12) and CH2 (packed RGB888) after CH0, each plane 1 MiB aligned.
    ch1_stride, ch2_stride = out_stride, align(3 * a.width, 16)
    ch1_y_addr = uv_addr + align(out_stride * (a.height // 2), 1 << 20)
    ch1_uv_addr = ch1_y_addr + align(ch1_stride * a.height, 1 << 20)
    ch2_addr = ch1_uv_addr + align(ch1_stride * (a.height // 2), 1 << 20)
    if ch2_addr + ch2_stride * a.height > map_constant("FX1_FW_TEST_AREA_BASE"):
        fail("frame too large for the YUV buffer region")
    extra_dumps = []  # (address, length, file) of the enabled CH1 / CH2 planes
    if a.outputs & 2:
        extra_dumps += [(ch1_y_addr, ch1_stride * a.height, "y1.bin"), (ch1_uv_addr, ch1_stride * (a.height // 2), "uv1.bin")]
    if a.outputs & 4:
        extra_dumps += [(ch2_addr, ch2_stride * a.height, "rgb2.bin")]

    writes = load_profile(a.profile)
    y_len, uv_len = out_stride * a.height, out_stride * (a.height // 2)
    if a.pack_only and (job["in_stride"], job["out_stride"], job["profile_count"]) != (in_stride, out_stride,
                                                                                      len(writes)):
        fail("job.bin does not match: strides %d/%d and %d profile writes, expected %d/%d and %d (--profile "
             "must be the one used for --prepare-only)" % (job["in_stride"], job["out_stride"], job["profile_count"],
                                                           in_stride, out_stride, len(writes)))
    if not a.pack_only:
        # 1. RAW -> ISP container
        conv = [sys.executable, os.path.join(ISP, "tools", "raw_fixture.py"), "convert", "--in", a.raw,
                "--format", a.format, "--width", str(a.width), "--height", str(a.height), "--bayer", a.bayer,
                "--out", out("input.isp16"), "--out-stride", str(in_stride)]
        if a.src_stride:
            conv += ["--src-stride", str(a.src_stride)]
        if run(conv, out("convert.log")).returncode:
            fail("RAW conversion failed, see " + out("convert.log"))

        # 2. profile blob and job descriptor
        with open(out("profile.bin"), "wb") as f:
            for off, val in writes:
                f.write(struct.pack("<II", off, val))
        if PROFILE_OFFSET + 8 * len(writes) > map_constant("FX1_FW_SHARED_SIZE"):
            fail("profile too large")
        job = isp_pack.pack_job(magic=JOB_MAGIC, version=VERSION, width=a.width, height=a.height,
                                bayer=BAYER[a.bayer], in_addr=raw_base, in_stride=in_stride, y_addr=y_addr,
                                uv_addr=uv_addr, out_stride=out_stride, profile_count=len(writes), frames=a.frames,
                                mode=MODE_LEGACY if a.mode == "legacy" else MODE_PROFILE, outputs=a.outputs,
                                profile_id=1, ch1_y_addr=ch1_y_addr, ch1_uv_addr=ch1_uv_addr, ch1_stride=ch1_stride,
                                ch2_addr=ch2_addr, ch2_stride=ch2_stride)
        with open(out("job.bin"), "wb") as f:
            f.write(job)
        for name in ["result.bin", "y.bin", "uv.bin", "y1.bin", "uv1.bin", "rgb2.bin"]:  # never pack the dumps of an earlier run
            if os.path.exists(out(name)):
                os.remove(out(name))

        if a.job_env:
            with open(a.job_env, "w") as f:
                f.write("# Generated by run_isp_demo.py --prepare-only: where fx1_soc loads and dumps this job.\n")
                for k, v in (("RAW_ADDR", raw_base), ("JOB_ADDR", shared + JOB_OFFSET),
                             ("PROFILE_ADDR", shared + PROFILE_OFFSET), ("RESULT_ADDR", shared + RESULT_OFFSET),
                             ("RESULT_LEN", 4 * len(RESULT_FIELDS)), ("Y_ADDR", y_addr), ("Y_LEN", y_len),
                             ("UV_ADDR", uv_addr), ("UV_LEN", uv_len)):
                    f.write("%s=%s\n" % (k, ("0x%08x" % v) if k.endswith("ADDR") else str(v)))
                # CH1 / CH2 planes of the job (version 2), when enabled.
                f.write("EXTRA_DUMPS=\"%s\"\n" % " ".join("0x%08x:%d:%s" % (ad, ln, n) for ad, ln, n in extra_dumps))
        if a.prepare_only:
            print("\nJob prepared in %s. Start the FreeRTOS CLI with it loaded:\n" % a.out)
            print("  %s --fw %s \\\n    --uart0-socket %d --uart0-wait --timeout-ms 0 --realtime \\\n"
                  "    --load 0x%x:%s --load 0x%x:%s --load 0x%x:%s \\\n"
                  "    --dump 0x%x:%d:%s \\\n    --dump 0x%x:%d:%s --dump 0x%x:%d:%s%s"
                  % (sim, elf, a.port, raw_base, out("input.isp16"), shared + JOB_OFFSET, out("job.bin"),
                     shared + PROFILE_OFFSET, out("profile.bin"), shared + RESULT_OFFSET, 4 * len(RESULT_FIELDS),
                     out("result.bin"), y_addr, y_len, out("y.bin"), uv_addr, uv_len, out("uv.bin"),
                     "".join(" \\\n    --dump 0x%x:%d:%s" % (ad, ln, out(n)) for ad, ln, n in extra_dumps)))
            print("\nthen in another terminal: platforms/fx1_soc/tools/fx1_console.sh %d" % a.port)
            print("and type: isp run (or isp run legacy), isp stats, poweroff (the --dump files are written at poweroff)")
            print("finally: %s --pack-only --out %s%s" % (sys.argv[0], a.out, "" if a.profile == DEFAULT_PROFILE
                                                             else " --profile " + a.profile))
            sys.exit(0)

        # 3. the platform
        io = ["--load", "0x%x:%s" % (raw_base, out("input.isp16")),
              "--load", "0x%x:%s" % (shared + JOB_OFFSET, out("job.bin")),
              "--load", "0x%x:%s" % (shared + PROFILE_OFFSET, out("profile.bin")),
              "--dump", "0x%x:%d:%s" % (shared + RESULT_OFFSET, 4 * len(RESULT_FIELDS), out("result.bin")),
              "--dump", "0x%x:%d:%s" % (y_addr, y_len, out("y.bin")),
              "--dump", "0x%x:%d:%s" % (uv_addr, uv_len, out("uv.bin"))]
        for ad, ln, n in extra_dumps:
            io += ["--dump", "0x%x:%d:%s" % (ad, ln, out(n))]
        if a.interactive:
            run_interactive([sim, "--fw", elf, "--uart0-socket", str(a.port), "--uart0-wait", "--timeout-ms", "0",
                             "--realtime"] + io, a.port, out("platform.log"))
        else:
            r = run([sim, "--fw", elf, "--timeout-ms", str(a.timeout_ms), "--uart-log", out("uart.log")] + io,
                    out("platform.log"))
            print(r.stdout)
            if r.returncode:
                fail("platform run failed (exit %d), see %s" % (r.returncode, out("platform.log")))
    # 4. checks, NV12 active bytes (Y rows then UV rows) and preview: isp_pack.py
    try:
        res = isp_pack.read_result(out("result.bin"))
        y, uv = open(out("y.bin"), "rb").read(), open(out("uv.bin"), "rb").read()
        isp_pack.check_output(res, a.width, a.height, a.frames, out_stride, y, uv)
        job = isp_pack.read_job(out("job.bin"))
        isp_pack.check_identity(job, res, None if a.expect_mode is None else
                                (MODE_LEGACY if a.expect_mode == "legacy" else MODE_PROFILE))
        channel_lines = isp_pack.pack_channels(a.out, job, res)
        ow, oh, stride = res["out_width"], res["out_height"], res["out_stride"]
        nv12 = isp_pack.pack_nv12(y, uv, ow, oh, stride)
        with open(out("platform.nv12"), "wb") as f:
            f.write(nv12)
        try:
            isp_pack.nv12_to_png(nv12, ow, oh, out("platform.png"))
        except OSError as e:
            os.remove(out("platform.nv12"))
            raise PackError("preview failed: cannot write %s: %s" % (out("platform.png"), e))
    except PackError as e:
        fail(str(e))
    platform_sha = sha256(out("platform.nv12"))


    # 5. the stand-alone ISP model, same input and profile
    verdict, ref_sha = "not compared (--no-reference)", None
    if not a.no_reference:
        if not os.path.exists(ref_tool):
            fail("reference tool not found: " + ref_tool)
        os.makedirs(out("reference"), exist_ok=True)
        rr = run([ref_tool, "--input", out("input.isp16"), "--width", str(a.width), "--height", str(a.height),
                  "--profile", a.profile, "--frames", str(a.frames), "--out", out("reference/frame")],
                 out("reference.log"))
        if rr.returncode or not os.path.exists(out("reference/frame.nv12")):
            fail("reference run failed, see " + out("reference.log"))
        ref_sha = sha256(out("reference/frame.nv12"))
        verdict = "MATCH" if ref_sha == platform_sha else "MISMATCH"

    lines = [
        "RAW            %s (%dx%d %s, %s)" % (a.raw or out("input.isp16"), a.width, a.height, a.bayer, a.format),
        "profile        %s (%d CSR writes)" % (a.profile, len(writes)),
        "frames         %d" % a.frames,
        "mode           %s%s; outputs 0x%x" % (a.mode, " (profile ID %d)" % res["profile_id"] if a.mode == "profile"
                                                 else "", a.outputs),
        "output         %dx%d NV12, stride %d" % (ow, oh, stride),
        "ISP counters   IDMA %d, ODMA %d frames; DMA_ERR 0x%x" % (res["idma_frames"], res["odma_frames"], res["dma_err"]),
        "FRAME_ID       AEC %d, AWB %d, AF %d" % (res["aec_frame_id"], res["awb_frame_id"], res["af_frame_id"]),
        "simulated time %.3f ms first queue to last completion" % (res["frame_ticks"] * 50e-6),
        "platform NV12  %s  sha256 %s" % (out("platform.nv12"), platform_sha),
        "reference NV12 %s" % ("sha256 " + ref_sha if ref_sha else "-"),
    ] + channel_lines + [
        "result         %s (CH0 against the stand-alone model's legacy run)" % verdict,
        "preview        %s" % out("platform.png"),
    ]
    with open(out("summary.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")
    print("\n".join(lines))
    sys.exit(1 if verdict == "MISMATCH" else 0)


if __name__ == "__main__":
    main()
