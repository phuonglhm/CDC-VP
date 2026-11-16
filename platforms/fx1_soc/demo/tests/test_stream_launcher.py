#!/usr/bin/env python3
"""ISP stream S3: the launcher (isp_stream.py run / encode) end to end.

  test_stream_launcher.py --fx1-soc BUILD/platforms/fx1_soc/fx1_soc --elf BUILD/fw/fx1_soc/freertos_cli.elf
                          --run-raw BUILD/.../fx1_isp_run_raw [--real] [--only NAME ...] [--keep DIR]

Every video is checked here independently of the launcher's own verify (ffprobe with
decoded frame count, stream flags, duration, subtitle cues and their text) and the NV12
spool against the stand-alone model (--run-raw). Fake encoders (a Python ffmpeg wrapper on
PATH that delegates to the real one) give the encoder failure paths. Cases:
  complete        A/B/C x repeat 2 at 5 fps: COMPLETE, 6 frames, 1.2 s, cues `#n name r=k/K`
  colorimetry     BT.601/BT.709 x full/limited (profile CSC_CTRL.std, TUNE_CSC_CTRL.FULL_RANGE):
                  the flags in the MP4 follow the firmware's colorimetry; NV12 kept and = model
  user_stop       loop, one Ctrl+C to the launcher's process group: one stop request, the
                  firmware drains, USER_STOPPED video of the prefix
  limit           loop with --max-spool-mib 1: LIMIT_REACHED, exactly the frames that fit
  no_frames       Ctrl+C before the first frame: NO_FRAMES, no video
  hard_stop       two Ctrl+C: HARD_STOP, spool kept, no video
  drain_timeout   a host held 1 s per frame, --drain-timeout-s 1: HARD_STOP naming the last seq
  preflight       no ffmpeg, ffmpeg without libx264, a stale output.mp4 in DIR, no simulator:
                  refused before anything is booted, nothing touched
  input           a missing RAW, --loop without a budget, --crf 0 (lossless: not with the High
                  profile), --crf 52, --min-psnr nan: INPUT_ERROR before the boot, DIR not made
  encoder         ffmpeg fails / crashes: ENCODE_FAILED, spool kept; `encode --crf 0` is refused
                  with the DIR untouched; `encode` makes the video without the VP; a second
                  `encode` refuses to overwrite it
  bad_video       ffmpeg exits 0 but drops a frame / reverses the order / tags the wrong range /
                  writes junk / swaps U and V / halves U only / halves V only (Y, flags and
                  count right: only the chroma check sees these): VERIFY_FAILED, rejected.mp4,
                  spool kept
  high_crf        --crf 51: VERIFY_FAILED on the PSNR only, the message says to re-encode with a
                  lower CRF and the same --min-psnr (video_hint); `encode --crf 10` then makes
                  the video without the VP (platform.log unchanged); no hint at the default CRF
  fw_error        a firmware ERROR (foreign ACK fault): FW_ERROR, spool kept, no video
  capture_error   the spool cannot grow (RLIMIT_FSIZE): CAPTURE_ERROR, no video
  reference_mismatch  --run-raw disagrees on one frame: REFERENCE_MISMATCH, no video, spool kept
  bridge_lies     a simulator (fake) reporting more frames than its spool holds, or DONE short of
                  the schedule, without a bridge error: CAPTURE_ERROR (the launcher's own checks)
  summary_json    (no VP) an identical plane's PSNR (inf) is written "inf": summary.json stays
                  strict JSON; summary.txt shows Y/U/V
  spool_tail      a torn tail (partial line, NV12 past the last frame) is cut; a bad SHA-256
                  is refused; the disk-space preflight refuses a run that cannot fit
  real            (--real) D65 -> A -> D65 x 2 at 2688x1520: COMPLETE, the shipped golden at
                  frames 1 and 5, 1920x1080 video
Exit 0 when every case passes.
"""
import argparse
import json
import os
import resource
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
DEMO = os.path.dirname(HERE)
ROOT = os.path.abspath(os.path.join(DEMO, "..", "..", ".."))
sys.dont_write_bytecode = True
sys.path.insert(0, DEMO)
import isp_stream as launcher  # noqa: E402

D65_GOLDEN = "8da0af679e4e"
LAUNCHER = os.path.join(DEMO, "isp_stream.py")
FFMPEG, FFPROBE = shutil.which("ffmpeg"), shutil.which("ffprobe")
failures = []


def check(cond, what):
    print(("ok   " if cond else "FAIL ") + what, flush=True)
    if not cond:
        failures.append(what)
    return cond


class Ctx:
    def __init__(self, a, work):
        self.a, self.work = a, work

    def path(self, *p):
        return os.path.join(self.work, *p)


def fixture(c, case, w=64, h=48):
    d = c.path("fx_%s_%dx%d" % (case, w, h))
    if not os.path.isdir(d):
        r = subprocess.run([sys.executable, "-B", os.path.join(DEMO, "stream_fixture.py"), d, "--case", case,
                            "--width", str(w), "--height", str(h)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           text=True)
        assert r.returncode == 0, r.stdout
    return os.path.join(d, case, "manifest.json"), os.path.join(d, case, case + ".csrw")


def launch(c, name, args, env=None, sigint_at=None, sigint_twice=False, preexec=None, wall=300, tool=True):
    """isp_stream.py run ... in its own process group (as a terminal would); SIGINT to that
    group (the launcher only: the simulator runs in a session of its own)."""
    out = c.path(name + " run")   # a space in DIR: paths are quoted everywhere
    argv = [sys.executable, "-B", LAUNCHER, "run", "--out", out] + list(args)
    if tool:
        argv += ["--fx1-soc", c.a.fx1_soc, "--elf", c.a.elf]
    logp = c.path(name + ".launcher.log")
    e = dict(os.environ, **(env or {}))
    with open(logp, "w") as log:
        p = subprocess.Popen(argv, stdout=log, stderr=subprocess.STDOUT, env=e, start_new_session=True,
                             preexec_fn=preexec)
        t0, sent = time.time(), 0
        while p.poll() is None:
            if time.time() - t0 > wall:
                os.killpg(p.pid, signal.SIGKILL)
                p.wait()
                break
            if sigint_at is not None and sent < (2 if sigint_twice else 1):
                text = open(logp, errors="replace").read()
                n = 0
                try:
                    n = sum(1 for _ in open(os.path.join(out, "spool", "frames.jsonl")))
                except OSError:
                    pass
                ready = "booting FX1" in text and (n >= sigint_at if sigint_at > 0 else True)
                if ready and (sent == 0 or "stop requested" in text):
                    os.killpg(p.pid, signal.SIGINT)
                    sent += 1
                    time.sleep(0.3)
            time.sleep(0.05)
    return p.returncode, open(logp, errors="replace").read(), out


def strict(tok):
    raise ValueError("summary.json is not strict JSON: %s" % tok)


def summary(out):
    """summary.json, parsed strictly (Infinity / NaN are refused: other tools read it)."""
    try:
        return json.load(open(os.path.join(out, "summary.json")), parse_constant=strict)
    except (OSError, ValueError):
        return {}


def db(x):
    """A PSNR of the summary ("inf" for an identical plane) as a number; None as -1."""
    return float(x) if x is not None else -1.0


def psnr_ok(s, floor=40.0):
    return all(db(s.get("psnr_%s_min" % p)) >= floor for p in "yuv")


def probe(mp4):
    """Independent of the launcher: decoded frame count, flags, duration, cues and their text."""
    r = subprocess.run([FFPROBE, "-v", "error", "-count_frames", "-count_packets", "-show_entries",
                        "stream=codec_type,codec_name,width,height,color_range,color_space,r_frame_rate,"
                        "nb_read_frames,nb_read_packets,duration", "-of", "json", mp4],
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if r.returncode:
        return None
    st = json.loads(r.stdout)["streams"]
    v = [x for x in st if x["codec_type"] == "video"][0]
    s = [x for x in st if x["codec_type"] == "subtitle"][0]
    cues = subprocess.run([FFMPEG, "-v", "error", "-i", mp4, "-map", "0:s", "-f", "srt", "-"], stdout=subprocess.PIPE,
                          stderr=subprocess.DEVNULL, text=True).stdout
    text = [l for l in cues.splitlines() if l.startswith("#")]
    return {"codec": v["codec_name"], "w": v["width"], "h": v["height"], "range": v.get("color_range"),
            "space": v.get("color_space"), "rate": v["r_frame_rate"], "frames": int(v["nb_read_frames"]),
            "duration": float(v["duration"]), "cues": int(s["nb_read_packets"]), "cue_text": text}


def frames_of(out):
    try:
        return [json.loads(l) for l in open(os.path.join(out, "spool", "frames.jsonl"))]
    except OSError:
        return []


def video_ok(name, out, n, fps, rng="pc", space="smpte170m", w=64, h=48, labels=None):
    p = probe(os.path.join(out, "output.mp4")) if os.path.isfile(os.path.join(out, "output.mp4")) else None
    good = (p is not None and p["codec"] == "h264" and (p["w"], p["h"]) == (w, h) and p["frames"] == n
            and p["cues"] == n and abs(p["duration"] - n / fps) < 1e-3 and (p["range"], p["space"]) == (rng, space)
            and (labels is None or p["cue_text"] == labels))
    check(good, "%s: output.mp4 decodes to %d frame(s) %dx%d, %.3f s at %s fps, %s/%s, %d cue(s)%s (%s)"
          % (name, n, w, h, n / fps, fps, rng, space, n, " in order" if labels else "",
             p and {k: p[k] for k in ("frames", "w", "h", "duration", "range", "space", "cues")}))
    return p


def no_video(out):
    return not os.path.exists(os.path.join(out, "output.mp4")) and not os.path.exists(os.path.join(out, "output.tmp.mp4"))


def fake_ffmpeg(c, mode):
    """A ffmpeg on PATH that answers the preflight queries as the real one and misbehaves
    on the encode (the call that has -c:v)."""
    d = c.path("fake_" + mode)
    if os.path.isdir(d):
        return d
    os.makedirs(d)
    script = r'''#!%(py)s
import os, subprocess, sys
real, mode, args = %(ff)r, %(mode)r, sys.argv[1:]
CHROMA = {"swapuv": "swapuv", "u_half": "lutyuv=y=val:u=val/2:v=val", "v_half": "lutyuv=y=val:u=val:v=val/2"}
if "-encoders" in args and mode == "no_x264":
    out = subprocess.run([real] + args, stdout=subprocess.PIPE, text=True).stdout
    sys.stdout.write("".join(l for l in out.splitlines(True) if "libx264" not in l))
    sys.exit(0)
if "-c:v" not in args:
    os.execv(real, [real] + args)
out = args[-1]
if mode == "fail":
    sys.stderr.write("fake ffmpeg: failing on purpose\n"); sys.exit(1)
if mode == "crash":
    os.kill(os.getpid(), 11)
if mode == "junk":
    open(out, "wb").write(b"\0" * 4096); sys.exit(0)
if mode == "drop":   # one frame less, exit 0
    n = args.index("-movflags")
    args = args[:n] + ["-vf", "select='gt(n\\,0)'"] + args[n:]
if mode == "reverse":   # every frame, in the reverse order, exit 0
    n = args.index("-movflags")
    args = args[:n] + ["-vf", "reverse"] + args[n:]
if mode in CHROMA:  # Y, the flags and the count stay right; only a chroma plane is wrong (lutyuv: every
    # plane named, its default `clipval` would also clip a full-range Y to 16..235)
    n = args.index("-movflags")
    args = args[:n] + ["-vf", CHROMA[mode]] + args[n:]
if mode == "range":  # the output tagged limited although the input is full
    k = [i for i, a in enumerate(args) if a == "-color_range"][-1]
    args[k + 1] = "tv"
sys.exit(subprocess.call([real] + args))
''' % {"py": sys.executable, "ff": FFMPEG, "mode": mode}
    p = os.path.join(d, "ffmpeg")
    with open(p, "w") as f:
        f.write(script)
    os.chmod(p, os.stat(p).st_mode | stat.S_IEXEC)
    os.symlink(FFPROBE, os.path.join(d, "ffprobe"))
    return d


def env_path(d):
    return {"PATH": d + os.pathsep + "/usr/bin:/bin"}


# ---- cases ----------------------------------------------------------------------------------
def case_complete(c):
    man, prof = fixture(c, "rggb_raw12")
    rc, log, out = launch(c, "complete", ["--manifest", man, "--profile", prof, "--repeat", "2", "--fps", "5",
                                          "--run-raw", c.a.run_raw])
    s = summary(out)
    names = ["frame A.raw", "frame B.raw", "frame C.raw"]
    labels = ["#%d %s r=%d/2" % (n, names[n // 2], n % 2 + 1) for n in range(6)]
    check(rc == 0 and s.get("status") == "COMPLETE" and s.get("captured") == 6 and s.get("queue") == 2
          and "equals fx1_isp_run_raw" in s.get("reference", "")
          and not os.path.exists(os.path.join(out, "spool", "frames.nv12"))
          and os.path.isfile(os.path.join(out, "spool", "frames.jsonl"))
          and os.path.isfile(os.path.join(out, "summary.txt")) and psnr_ok(s)
          and "min PSNR Y/U/V" in open(os.path.join(out, "summary.txt")).read(),
          "complete: exit 0 COMPLETE, 6 frames = the model, queue 2 by default, NV12 removed after the verified video, "
          "strict summary.json with Y/U/V PSNR >= 40 (rc %s, %s, Y/U/V %s)" % (rc, s.get("message"), s.get("psnr_min_yuv")))
    video_ok("complete", out, 6, 5.0, labels=labels)


def case_colorimetry(c):
    man, prof = fixture(c, "rggb_raw12")
    base = open(prof).read()
    for std, full, rng, space, text in ((0, 1, "pc", "smpte170m", "full range, BT.601"),
                                        (1, 1, "pc", "bt709", "full range, BT.709"),
                                        (0, 0, "tv", "smpte170m", "limited range, BT.601"),
                                        (1, 0, "tv", "bt709", "limited range, BT.709")):
        name = "col_%s_%s" % (space, rng)
        p = c.path(name + ".csrw")
        with open(p, "w") as f:
            f.write(base + "CSC_CTRL std=%d\nTUNE_CSC_CTRL LOOK_EN=0 FULL_RANGE=%d\n" % (std, full))
        rc, log, out = launch(c, name, ["--manifest", man, "--profile", p, "--keep-nv12", "--run-raw", c.a.run_raw,
                                        "--fps", "30"])
        s = summary(out)
        fr = frames_of(out)
        word = 0x80000000 | (std << 1) | full
        check(rc == 0 and s.get("colorimetry") == text and fr and all(f["colorimetry"] == word for f in fr)
              and os.path.isfile(os.path.join(out, "spool", "frames.nv12")) and "equals" in s.get("reference", "")
              and psnr_ok(s),
              "%s: firmware colorimetry 0x%08X (%s), NV12 kept and = the model, min Y PSNR %s (rc %s)"
              % (name, word, text, s.get("psnr_min_yuv"), rc))
        video_ok(name, out, 3, 30.0, rng=rng, space=space)


def case_user_stop(c):
    man, prof = fixture(c, "rggb_raw12")
    rc, log, out = launch(c, "user_stop", ["--manifest", man, "--profile", prof, "--loop", "--max-spool-mib", "64",
                                           "--run-raw", c.a.run_raw], sigint_at=3, wall=240)
    s = summary(out)
    br = s.get("bridge") or {}
    n = s.get("captured", 0)
    check(rc == 3 and s.get("status") == "USER_STOPPED" and br.get("user_stops") == 1
          and br.get("stop_requested") == "USER" and (br.get("firmware") or {}).get("stop_reason") == "USER"
          and n >= 3 and log.count("stop requested") == 1 and "equals fx1_isp_run_raw" in s.get("reference", ""),
          "user_stop: one Ctrl+C -> one stop request, USER_STOPPED (exit 3), %d frame(s) = the model (rc %s)" % (n, rc))
    video_ok("user_stop", out, n, 30.0)


def case_limit(c):
    man, prof = fixture(c, "bggr_raw12", 256, 144)
    rc, log, out = launch(c, "limit", ["--manifest", man, "--profile", prof, "--loop", "--max-spool-mib", "1"])
    s = summary(out)
    fit = (1 << 20) // (254 * 142 * 3 // 2)
    check(rc == 4 and s.get("status") == "LIMIT_REACHED" and s.get("captured") == fit and "spool budget" in s.get("message", ""),
          "limit: --max-spool-mib 1, LIMIT_REACHED (exit 4), exactly the %d frames that fit (rc %s, %s)"
          % (fit, rc, s.get("captured")))
    video_ok("limit", out, fit, 30.0, w=254, h=142)


def case_no_frames(c):
    man, prof = fixture(c, "rggb_raw12")
    rc, log, out = launch(c, "no_frames", ["--manifest", man, "--profile", prof, "--loop", "--max-spool-mib", "8"],
                          sigint_at=0)
    s = summary(out)
    fw = (s.get("bridge") or {}).get("firmware") or {}
    check(rc == 6 and s.get("status") == "NO_FRAMES" and s.get("captured") == 0 and fw.get("submitted") == 0
          and fw.get("stop_reason") == "USER" and no_video(out),
          "no_frames: Ctrl+C before the first frame: NO_FRAMES (exit 6), nothing submitted, no video (rc %s)" % rc)


def case_hard_stop(c):
    man, prof = fixture(c, "rggb_raw12")
    rc, log, out = launch(c, "hard_stop", ["--manifest", man, "--profile", prof, "--loop", "--max-spool-mib", "8",
                                           "--queue", "4", "--vp-arg=--isp-stream-hold-us", "--vp-arg=200000"],
                          sigint_at=2, sigint_twice=True, wall=240)
    s = summary(out)
    check(rc == 12 and s.get("status") == "HARD_STOP" and "second Ctrl+C" in s.get("message", "") and no_video(out)
          and os.path.isfile(os.path.join(out, "spool", "frames.nv12")) and s.get("captured", 0) >= 2,
          "hard_stop: a second Ctrl+C: HARD_STOP (exit 12), %s frame(s) kept in the spool, no video (rc %s)"
          % (s.get("captured"), rc))


def case_drain_timeout(c):
    man, prof = fixture(c, "rggb_raw12")
    rc, log, out = launch(c, "drain_timeout", ["--manifest", man, "--profile", prof, "--loop", "--max-spool-mib", "8",
                                               "--queue", "4", "--drain-timeout-s", "1",
                                               "--vp-arg=--isp-stream-hold-us", "--vp-arg=1000000"],
                          sigint_at=1, wall=240)
    s = summary(out)
    check(rc == 12 and s.get("status") == "HARD_STOP" and "drain-timeout" in s.get("message", "")
          and "last frame captured: seq" in s.get("message", "") and no_video(out),
          "drain_timeout: no end of the drain within 1 s: HARD_STOP naming the last seq (rc %s, %s)"
          % (rc, s.get("message")))


def case_preflight(c):
    man, prof = fixture(c, "rggb_raw12")
    empty = c.path("empty_bin")
    os.makedirs(empty, exist_ok=True)
    rc, log, out = launch(c, "pre_noffmpeg", ["--manifest", man, "--profile", prof], env={"PATH": empty})
    check(rc == 5 and "PREFLIGHT_FAILED" in log and "ffmpeg and ffprobe not found" in log and "dnf install" in log
          and not os.path.exists(out),
          "preflight: no ffmpeg: PREFLIGHT_FAILED (exit 5), install steps printed, DIR not created (rc %s)" % rc)
    rc, log, out = launch(c, "pre_nox264", ["--manifest", man, "--profile", prof], env=env_path(fake_ffmpeg(c, "no_x264")))
    check(rc == 5 and "lacks encoder libx264" in log and not os.path.exists(out),
          "preflight: ffmpeg without libx264: refused before the boot (rc %s)" % rc)
    stale = c.path("pre_stale run")
    os.makedirs(stale, exist_ok=True)
    with open(os.path.join(stale, "output.mp4"), "wb") as f:
        f.write(b"an older video")
    rc, log, out = launch(c, "pre_stale", ["--manifest", man, "--profile", prof])
    check(rc == 5 and "not an empty directory" in log and open(os.path.join(stale, "output.mp4"), "rb").read()
          == b"an older video" and sorted(os.listdir(stale)) == ["output.mp4"],
          "preflight: a DIR with an older output.mp4: refused, the file untouched, nothing written (rc %s)" % rc)
    rc, log, out = launch(c, "pre_nosim", ["--manifest", man, "--profile", prof, "--fx1-soc", c.path("nope"),
                                           "--elf", c.a.elf], tool=False)
    check(rc == 5 and "simulator not found" in log and not os.path.exists(out),
          "preflight: no simulator: refused (rc %s)" % rc)


def case_input(c):
    man, prof = fixture(c, "rggb_raw12")
    rc, log, out = launch(c, "in_missing", ["--raw", c.path("missing.raw"), "--profile", prof])
    check(rc == 2 and "INPUT_ERROR" in log and not os.path.exists(os.path.join(out, "platform.log")),
          "input: a missing RAW: INPUT_ERROR (exit 2), nothing booted (rc %s)" % rc)
    rc, log, out = launch(c, "in_loop", ["--manifest", man, "--profile", prof, "--loop"])
    check(rc == 2 and "--loop needs --max-spool-mib" in log and not os.path.exists(out),
          "input: --loop without a spool budget: refused (rc %s)" % rc)
    for name, opt, why in (("crf0", ["--crf", "0"], "lossless"), ("crf52", ["--crf", "52"], "--crf 52: 1..51"),
                           ("psnr_nan", ["--min-psnr", "nan"], "--min-psnr nan"),
                           ("psnr_0", ["--min-psnr", "0"], "--min-psnr 0")):
        rc, log, out = launch(c, "in_" + name, ["--manifest", man, "--profile", prof] + opt)
        check(rc == 2 and "INPUT_ERROR" in log and why in log and not os.path.exists(out),
              "input: %s: INPUT_ERROR (exit 2) before the boot, DIR not made (rc %s)" % (" ".join(opt), rc))


def case_encoder(c):
    man, prof = fixture(c, "rggb_raw12")
    for mode in ("fail", "crash"):
        rc, log, out = launch(c, "enc_" + mode, ["--manifest", man, "--profile", prof, "--repeat", "2"],
                              env=env_path(fake_ffmpeg(c, mode)))
        s = summary(out)
        check(rc == 9 and s.get("status") == "ENCODE_FAILED" and no_video(out)
              and os.path.getsize(os.path.join(out, "spool", "frames.nv12")) == 6 * 64 * 48 * 3 // 2
              and "encode --out" in s.get("message", ""),
              "encoder %s: ENCODE_FAILED (exit 9), the 6 frames kept in the spool, no video (rc %s)" % (mode, rc))
    # The same checks in `encode`: refused, the run directory untouched.
    before = {f: open(os.path.join(out, f), "rb").read() for f in ("summary.json", "summary.txt")}
    listing = sorted(os.listdir(out))
    for opt, why in ((["--crf", "0"], "lossless"), (["--crf", "-1"], "--crf -1: 1..51"),
                     (["--min-psnr", "inf"], "--min-psnr inf")):
        r = subprocess.run([sys.executable, "-B", LAUNCHER, "encode", "--out", out] + opt, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, text=True)
        check(r.returncode == 2 and why in r.stdout and sorted(os.listdir(out)) == listing
              and all(open(os.path.join(out, f), "rb").read() == b for f, b in before.items()),
              "encode %s: INPUT_ERROR (exit 2), the run directory untouched (rc %s)" % (" ".join(opt), r.returncode))
    # The kept spool, encoded again without the VP (the real ffmpeg).
    r = subprocess.run([sys.executable, "-B", LAUNCHER, "encode", "--out", out, "--fps", "10"], stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, text=True)
    s = summary(out)
    check(r.returncode == 0 and s.get("status") == "COMPLETE" and "re-encoded" in " ".join(s.get("notes", []))
          and os.path.isfile(os.path.join(out, "spool", "frames.nv12")) and s.get("fps") == 10.0 and psnr_ok(s),
          "encode: the kept spool re-encoded at 10 fps without the VP: COMPLETE, NV12 kept (rc %s)" % r.returncode)
    video_ok("encode", out, 6, 10.0)
    r = subprocess.run([sys.executable, "-B", LAUNCHER, "encode", "--out", out], stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, text=True)
    check(r.returncode == 2 and "not overwritten" in r.stdout, "encode: a second encode refuses to overwrite output.mp4")


def case_bad_video(c):
    man, prof = fixture(c, "rggb_raw12")
    for mode, why in (("drop", "video_packets 5"), ("reverse", "below 40.0 dB Y PSNR"), ("range", "color_range"),
                      ("junk", "ffprobe cannot read"), ("swapuv", "dB U PSNR"), ("u_half", "dB U PSNR"),
                      ("v_half", "dB V PSNR")):
        rc, log, out = launch(c, "bad_" + mode, ["--manifest", man, "--profile", prof, "--repeat", "2"],
                              env=env_path(fake_ffmpeg(c, mode)))
        s = summary(out)
        check("video_hint" not in s and "keep --min-psnr" not in s.get("message", ""),
              "bad video (%s) at the default CRF: no lower-CRF hint (the encode loss is not the cause)" % mode)
        check(rc == 10 and s.get("status") == "VERIFY_FAILED" and why in s.get("message", "") and no_video(out)
              and os.path.isfile(os.path.join(out, "rejected.mp4"))
              and os.path.isfile(os.path.join(out, "spool", "frames.nv12")),
              "bad video (%s, ffmpeg exit 0): VERIFY_FAILED (exit 10), rejected.mp4, spool kept (rc %s, %s)"
              % (mode, rc, s.get("message", "")[:90]))
        if mode in ("swapuv", "u_half", "v_half"):
            # Only the chroma check sees it: Y, flags, count and duration are right, and the
            # plane left alone passes (u_half / v_half), so each plane is checked on its own.
            probs = s.get("video_problems", [])
            want = {"swapuv": "UV", "u_half": "U", "v_half": "V"}[mode]
            planes = "".join(p for p in "YUV" if any("dB %s PSNR" % p in x for x in probs))
            check(db(s.get("psnr_y_min")) >= 40 and len(probs) == len(want) and planes == want,
                  "bad video (%s): the only problems are the %s PSNR (Y min %s, Y/U/V %s, %s)"
                  % (mode, "/".join(want), s.get("psnr_y_min"), s.get("psnr_min_yuv"), probs))
        if mode == "range":
            # `encode` again with an encoder that fails: the PSNR and problems of the rejected
            # video must not stay in the summary as if they described this attempt.
            r = subprocess.run([sys.executable, "-B", LAUNCHER, "encode", "--out", out], stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True, env=dict(os.environ, **env_path(fake_ffmpeg(c, "fail"))))
            s2 = summary(out)
            stale = [k for k in ("video_check", "video_problems", "psnr_y_min", "psnr_u_min", "psnr_v_min", "psnr_min_yuv")
                     if k in s2]
            check(r.returncode == 9 and s2.get("status") == "ENCODE_FAILED" and not stale
                  and "min PSNR" not in open(os.path.join(out, "summary.txt")).read(),
                  "bad video (range), then `encode` with a failing ffmpeg: ENCODE_FAILED, no PSNR of the earlier video "
                  "left in the summary (rc %s, stale %s)" % (r.returncode, stale))


def case_high_crf(c):
    man, prof = fixture(c, "rggb_raw12")
    rc, log, out = launch(c, "high_crf", ["--manifest", man, "--profile", prof, "--repeat", "2", "--crf", "51"])
    s = summary(out)
    msg = s.get("message", "")
    check(rc == 10 and s.get("status") == "VERIFY_FAILED" and s.get("capture_status") == "COMPLETE"
          and s.get("video_hint") == "lower --crf, keep --min-psnr" and "--crf 10` re-encodes the kept spool" in msg
          and "keep --min-psnr; lower it only to accept more loss on purpose" in msg and s.get("min_psnr") == 40.0
          and no_video(out) and os.path.isfile(os.path.join(out, "rejected.mp4"))
          and os.path.isfile(os.path.join(out, "spool", "frames.nv12")),
          "high_crf: --crf 51 below 40 dB: VERIFY_FAILED (exit 10), the hint says lower CRF and keep --min-psnr, "
          "spool kept (rc %s, Y/U/V %s)" % (rc, s.get("psnr_min_yuv")))
    plat = open(os.path.join(out, "platform.log"), "rb").read()
    r = subprocess.run([sys.executable, "-B", LAUNCHER, "encode", "--out", out, "--crf", "10"], stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, text=True)
    s = summary(out)
    check(r.returncode == 0 and s.get("status") == "COMPLETE" and s.get("crf") == 10 and s.get("min_psnr") == 40.0
          and psnr_ok(s) and "video_hint" not in s and open(os.path.join(out, "platform.log"), "rb").read() == plat,
          "high_crf: `encode --crf 10` with the floor kept at 40: COMPLETE without the VP (platform.log unchanged), "
          "hint cleared (rc %s, Y/U/V %s)" % (r.returncode, s.get("psnr_min_yuv")))
    video_ok("high_crf encode", out, 6, 30.0)


def case_fw_error(c):
    man, prof = fixture(c, "rggb_raw12")
    rc, log, out = launch(c, "fw_error", ["--manifest", man, "--profile", prof, "--repeat", "2",
                                          "--vp-arg=--isp-stream-fault", "--vp-arg=foreign-ack"])
    s = summary(out)
    check(rc == 7 and s.get("status") == "FW_ERROR" and "ERROR HOST" in s.get("message", "") and no_video(out)
          and s.get("captured") == 4 and os.path.isfile(os.path.join(out, "spool", "frames.nv12")),
          "fw_error: firmware ERROR HOST: FW_ERROR (exit 7), the 4 frames kept, no video (rc %s)" % rc)


def case_capture_error(c):
    man, prof = fixture(c, "bggr_raw12", 256, 144)
    limit = 2 * 54102 + 20000

    def lim():
        resource.setrlimit(resource.RLIMIT_FSIZE, (limit, limit))
    # The simulator through a wrapper that ignores SIGXFSZ (subprocess restores it to the
    # default in the child): the bridge's write() then fails with EFBIG instead.
    wrap = c.path("fx1_soc_nofsz")
    with open(wrap, "w") as f:
        f.write("#!%s\nimport os, signal, sys\nsignal.signal(signal.SIGXFSZ, signal.SIG_IGN)\n"
                "os.execv(%r, [%r] + sys.argv[1:])\n" % (sys.executable, c.a.fx1_soc, c.a.fx1_soc))
    os.chmod(wrap, 0o755)
    rc, log, out = launch(c, "capture_error", ["--manifest", man, "--profile", prof, "--repeat", "2",
                                               "--fx1-soc", wrap, "--elf", c.a.elf], preexec=lim, tool=False)
    s = summary(out)
    check(rc == 8 and s.get("status") == "CAPTURE_ERROR" and "write of frames.nv12" in s.get("message", "")
          and no_video(out) and s.get("captured") == 2,
          "capture_error: the spool cannot grow: CAPTURE_ERROR (exit 8), 2 frames kept, no video (rc %s)" % rc)


def case_reference_mismatch(c):
    # A model that disagrees on frame 3 (a run_raw wrapper editing its own report): the launcher
    # must refuse the run (REFERENCE_MISMATCH), make no video and keep the spool.
    wrap = c.path("run_raw_wrong")
    with open(wrap, "w") as f:
        f.write("""#!%s
import subprocess, sys
rc = subprocess.call([%r] + sys.argv[1:])
out = sys.argv[sys.argv.index("--out") + 1] + ".txt"
lines = open(out).read().splitlines()
lines = [l if not l.startswith("frame_sha256_3 ") else "frame_sha256_3 " + "0" * 64 for l in lines]
open(out, "w").write("\\n".join(lines) + "\\n")
sys.exit(rc)
""" % (sys.executable, c.a.run_raw))
    os.chmod(wrap, 0o755)
    man, prof = fixture(c, "rggb_raw12")
    rc, log, out = launch(c, "ref_mismatch", ["--manifest", man, "--profile", prof, "--repeat", "2", "--run-raw", wrap])
    s = summary(out)
    check(rc == 11 and s.get("status") == "REFERENCE_MISMATCH" and "frame 3 differs" in s.get("message", "")
          and no_video(out) and os.path.isfile(os.path.join(out, "spool", "frames.nv12")),
          "reference_mismatch: a frame unlike the model: REFERENCE_MISMATCH (exit 11), no video, spool kept (rc %s, %s)"
          % (rc, s.get("message", "")[:80]))


def case_bridge_lies(c):
    # A simulator that writes a consistent spool of 2 frames but reports, without any bridge
    # error, that the firmware published 3 (or DONE with 2 of a 6-frame schedule): the
    # launcher's own cross-checks must refuse both (CAPTURE_ERROR), never make a video.
    import hashlib
    man, prof = fixture(c, "rggb_raw12")
    for name, completed in (("lies_count", 3), ("lies_done", 2)):
        fake = c.path(name + "_fx1_soc")
        with open(fake, "w") as f:
            f.write("""#!%s
import hashlib, json, os, sys
a = sys.argv
sp = a[a.index("--isp-stream-spool") + 1]
os.makedirs(sp)
print("[fx1_soc] fake simulator", flush=True)
lines, data = [], b""
for n in range(2):
    px = bytes([16 + n]) * (64 * 48 * 3 // 2)
    lines.append(json.dumps({"ordinal": n, "seq": n + 1, "source_index": 0, "repeat_index": n, "frame_id": n + 1,
                             "width": 64, "height": 48, "colorimetry": 0x80000001, "offset": len(data),
                             "bytes": len(px), "sha256": hashlib.sha256(px).hexdigest()}))
    data += px
open(os.path.join(sp, "frames.nv12"), "wb").write(data)
open(os.path.join(sp, "frames.jsonl"), "w").write("".join(l + "\\n" for l in lines))
fw = {"state": "STOPPED", "stop_reason": "DONE", "completed": %d, "submitted": %d}
json.dump({"captured": 2, "errors": [], "firmware": fw}, open(os.path.join(sp, "status.json"), "w"))
print("[fx1_soc] RESULT PASS")
""" % (sys.executable, completed, completed))
        os.chmod(fake, 0o755)
        rc, log, out = launch(c, name, ["--manifest", man, "--profile", prof, "--repeat", "2",
                                        "--fx1-soc", fake, "--elf", c.a.elf], tool=False)
        s = summary(out)
        want = "published 3 frame(s), the spool holds 2" if completed == 3 else "DONE with 2 of 6"
        check(rc == 8 and s.get("status") == "CAPTURE_ERROR" and want in s.get("message", "") and no_video(out),
              "%s: firmware %d published / DONE vs a 2-frame spool, no bridge error: CAPTURE_ERROR, no video (rc %s, %s)"
              % (name, completed, rc, s.get("message", "")[:70]))


def case_spool_tail(c):
    d = c.path("tail")
    sp = os.path.join(d, "spool")
    os.makedirs(sp)
    import hashlib
    frames, data = [], b""
    for n in range(2):
        px = bytes([n + 1]) * (4 * 2 * 3 // 2)
        frames.append({"ordinal": n, "offset": len(data), "bytes": len(px), "width": 4, "height": 2,
                       "colorimetry": 0x80000001, "sha256": hashlib.sha256(px).hexdigest(), "seq": n + 1})
        data += px
    with open(os.path.join(sp, "frames.nv12"), "wb") as f:
        f.write(data + b"\x07" * 5)
    with open(os.path.join(sp, "frames.jsonl"), "w") as f:
        f.write("".join(json.dumps(x) + "\n" for x in frames) + '{"ordinal": 2, "off')
    got, notes = launcher.read_spool(sp)
    check(len(got) == 2 and os.path.getsize(os.path.join(sp, "frames.nv12")) == len(data) and len(notes) == 2,
          "spool_tail: a partial last line and 5 bytes past the last frame are cut: 2 whole frames (%s)" % notes)
    with open(os.path.join(sp, "frames.nv12"), "r+b") as f:
        f.seek(0)
        f.write(b"\x09")
    try:
        launcher.read_spool(sp)
        ok = False
    except launcher.Fail as e:
        ok = e.status == "CAPTURE_ERROR" and "SHA-256" in str(e)
    check(ok, "spool_tail: a frame whose bytes differ from its SHA-256 is refused (CAPTURE_ERROR)")
    try:
        launcher.disk_budget(d, 1000, 3110400, 0, free=1 << 30)
        ok = False
    except launcher.Fail as e:
        ok = e.status == "PREFLIGHT_FAILED" and "MiB free" in str(e)
    check(ok, "spool_tail: 1000 frames of 1920x1080 against 1 GiB free: refused before the boot")
    # No guess on colorimetry: a word without the valid bit or with a reserved matrix gets no
    # video (ENCODE_FAILED, the spool kept), and the four valid words map exactly.
    s = {"_frames": [dict(frames[0], colorimetry=0x00000001), dict(frames[1], colorimetry=0x00000001)],
         "_names": ["a", "b"], "_repeat": 1}
    st, msg = launcher.make_video(d, s, FFMPEG, FFPROBE, 30.0, 10, 40.0)
    maps = [launcher.colorimetry(w) for w in (0x80000001, 0x80000003, 0x80000000, 0x80000002, 0x80000005, 0x1)]
    check(st == "ENCODE_FAILED" and "no usable colorimetry" in msg and no_video(d)
          and maps == [("pc", "smpte170m"), ("pc", "bt709"), ("tv", "smpte170m"), ("tv", "bt709"), None, None],
          "spool_tail: colorimetry without the valid bit (or a reserved matrix): no video, no guess (%s)" % msg[:60])


def case_summary_json(c):
    d = c.path("summary_json")
    os.makedirs(d, exist_ok=True)
    inf = float("inf")
    launcher.write_summary(d, {"status": "COMPLETE", "psnr_y_min": 66.57, "psnr_u_min": inf, "psnr_v_min": inf,
                               "psnr_min_yuv": "66.57/inf/inf", "video_check": {"psnr_u_mean": inf, "x": [inf]}})
    s = summary(d)
    check(s.get("psnr_u_min") == "inf" and s["video_check"] == {"psnr_u_mean": "inf", "x": ["inf"]}
          and psnr_ok(s) and "min PSNR Y/U/V    66.57/inf/inf" in open(os.path.join(d, "summary.txt")).read(),
          "summary_json: an inf PSNR is written \"inf\", summary.json strict JSON, summary.txt Y/U/V (%s)"
          % {k: s.get(k) for k in ("psnr_u_min", "psnr_min_yuv")})


def case_real(c):
    s_dir = os.path.join(ROOT, "components", "isp_tlm", "input")
    d65, a_ = os.path.join(s_dir, "D65_raw_2688x1520_5376.raw"), os.path.join(s_dir, "A_raw_2688x1520_5376.raw")
    prof = os.path.join(ROOT, "components", "FX1_Components", "isp", "samples", "profiles", "shared_bggr_2688x1520_full.csrw")
    rc, log, out = launch(c, "real", ["--raw", d65, a_, d65, "--profile", prof, "--repeat", "2", "--fps", "2",
                                      "--keep-nv12", "--run-raw", c.a.run_raw], wall=900)
    s = summary(out)
    fr = frames_of(out)
    check(rc == 0 and s.get("status") == "COMPLETE" and len(fr) == 6 and fr[1]["sha256"].startswith(D65_GOLDEN)
          and fr[5]["sha256"].startswith(D65_GOLDEN) and "equals" in s.get("reference", ""),
          "real: D65 -> A -> D65 x 2: COMPLETE, every frame = the model, frames 1 and 5 = the shipped golden %s "
          "(rc %s, min PSNR Y/U/V %s)" % (D65_GOLDEN, rc, s.get("psnr_min_yuv")))
    names = ["D65_raw_2688x1520_5376.raw", "A_raw_2688x1520_5376.raw", "D65_raw_2688x1520_5376.raw"]
    video_ok("real", out, 6, 2.0, w=1920, h=1080, labels=["#%d %s r=%d/2" % (n, names[n // 2], n % 2 + 1) for n in range(6)])


CASES = [("complete", case_complete), ("colorimetry", case_colorimetry), ("user_stop", case_user_stop),
         ("limit", case_limit), ("no_frames", case_no_frames), ("hard_stop", case_hard_stop),
         ("drain_timeout", case_drain_timeout), ("preflight", case_preflight), ("input", case_input),
         ("encoder", case_encoder), ("bad_video", case_bad_video), ("high_crf", case_high_crf),
         ("fw_error", case_fw_error),
         ("capture_error", case_capture_error), ("reference_mismatch", case_reference_mismatch), ("bridge_lies", case_bridge_lies),
         ("spool_tail", case_spool_tail), ("summary_json", case_summary_json)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fx1-soc", required=True)
    ap.add_argument("--elf", required=True)
    ap.add_argument("--run-raw", required=True)
    ap.add_argument("--real", action="store_true")
    ap.add_argument("--only", nargs="+")
    ap.add_argument("--keep")
    a = ap.parse_args()
    if not FFMPEG or not FFPROBE:
        print("test_stream_launcher.py: ffmpeg/ffprobe not on PATH: the launcher cannot be tested here")
        return 1
    work = a.keep or tempfile.mkdtemp(prefix="stream_launcher_")
    os.makedirs(work, exist_ok=True)
    c = Ctx(a, work)
    cases = CASES + ([("real", case_real)] if a.real else [])
    try:
        for name, fn in cases:
            if a.only and name not in a.only:
                continue
            t0 = time.time()
            try:
                fn(c)
            except Exception as e:  # a harness error is a failure, never a pass
                check(False, "%s: %s: %s" % (name, type(e).__name__, e))
            print("     (%s: %.1f s)" % (name, time.time() - t0), flush=True)
    finally:
        if not a.keep:
            shutil.rmtree(work, ignore_errors=True)
    print("%s: %d failure(s)" % (os.path.basename(__file__), len(failures)))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
