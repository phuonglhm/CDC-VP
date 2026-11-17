#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""ISP RAW sequence -> one FX1 boot, one ISP session -> output.mp4 (`run_vp.sh isp-stream`).

  isp_stream.py run (--manifest M.json | --raw A.raw [B.raw ...]) --profile P.csrw --out DIR
                [--repeat K] [--frames N | --loop] [--queue 1|2|4] [--legacy]
                [--fps F] [--crf N] [--keep-nv12] [--max-spool-mib N] [--min-psnr DB]
                [--width W --height H --source-format F --source-stride S]   (--raw form)
                [--fx1-soc PATH --elf PATH] [--run-raw PATH] [--drain-timeout-s S]
  isp_stream.py encode --out DIR [--fps F] [--crf N] [--min-psnr DB] [--debug-prefix]

Contract: platforms/fx1_soc/docs/ISP_STREAM_CONTRACT.md (section 6 and "Launcher").

run, in this order, and nothing of the user's is removed or overwritten (DIR must be new
or empty):
  1. preflight, before anything is prepared or booted: ffmpeg and ffprobe with libx264,
     mov_text and the mp4 muxer; the simulator and the firmware; DIR; free disk space for
     the spool (upper bound) or --max-spool-mib;
  2. prepare DIR/session (isp_stream_prep.py: manifest/RAW checks, profile compile, pool);
  3. boot fx1_soc once (FreeRTOS `isp stream start/wait/poweroff`, host bridge spool in
     DIR/spool), in its own process group; progress on stdout, its console in
     DIR/platform.log. Ctrl+C once: a stop request, forwarded once (the firmware drains,
     the frames captured become the video, USER_STOPPED); twice, or no drain within
     --drain-timeout-s: hard stop (spool kept, no video);
  4. check the spool: every frame's SHA-256, consecutive ordinals, one geometry and one
     colorimetry, captured == published; a torn tail is cut to the last whole frame;
     with --run-raw also every frame against the stand-alone model (fx1_isp_run_raw);
  5. encode the NV12 spool (libx264, CRF, full/limited range and BT.601/709 declared on
     the input and the output, one mov_text cue per frame `#ordinal name r=k/K`) to
     output.tmp.mp4, then verify it (ffprobe: H.264, geometry, fps, duration N/fps,
     range, matrix, N subtitle cues; decoded against the spool: exactly N frames, every
     frame's Y, U and V PSNR each >= --min-psnr) and only then rename it to output.mp4;
     --crf is 1..51 (0, lossless, is refused before the boot: the High profile cannot);
  6. DIR/summary.json + summary.txt; the spool NV12 is removed only after the video is
     verified, unless --keep-nv12 (frames.jsonl and status.json always stay).

encode re-runs steps 5-6 on DIR's kept spool (after an encode or verify failure, or with
another --fps/--crf), without the VP. It refuses a DIR that already has output.mp4.

encode --debug-prefix (opt-in, a failed run: FW_ERROR, CAPTURE_ERROR, HARD_STOP, SIM_ERROR,
REFERENCE_MISMATCH, NO_FRAMES): the longest prefix of whole, consecutive frames that pass
every spool check (read only: the spool stays as the run left it) is encoded and verified
like a video, into DIR/debug_prefix.mp4 (its files in DIR/debug_prefix/). The run's status,
message and exit in summary.json are unchanged; summary.json "debug_prefix" records the
result. It is never output.mp4, never COMPLETE, not for acceptance. Exit: 0 the debug video
verified, 6 no valid prefix, 2 refused (a run whose capture succeeded: use encode), 9/10.

Exit status (summary.json "status"):
  0  COMPLETE          the whole schedule, video verified
  2  INPUT_ERROR       manifest, RAW, profile or option refused before the boot
  3  USER_STOPPED      Ctrl+C: the captured prefix, video verified
  4  LIMIT_REACHED     the spool budget stopped the session: the prefix, video verified
  5  PREFLIGHT_FAILED  FFmpeg, simulator/firmware, DIR or disk space: nothing booted
  6  NO_FRAMES         the session ended without a frame: no video
  7  FW_ERROR          the firmware or the ISP ended the session in ERROR (or no verdict)
  8  CAPTURE_ERROR     the host bridge or the spool failed (frames lost or inconsistent)
  9  ENCODE_FAILED     ffmpeg failed: the spool is kept (`encode` retries)
  10 VERIFY_FAILED     the video does not match the spool: rejected.mp4, spool kept (only the
                       PSNR below the floor at a CRF above 10: re-encode with a lower --crf,
                       keeping --min-psnr)
  11 REFERENCE_MISMATCH  --run-raw: a frame differs from the stand-alone model
  12 HARD_STOP         a second Ctrl+C, the drain timeout, or the simulator killed
  13 SIM_ERROR         the simulator failed (integration or usage error)
  130                  Ctrl+C outside the VP run (while preparing, checking or encoding)
For 7, 8 and 12 the frames captured stay in DIR/spool; no video is made.
"""
import argparse
import hashlib
import json
import os
import shutil
import signal
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
sys.path.insert(0, HERE)
import isp_stream_prep as prep  # noqa: E402

EXIT = {"COMPLETE": 0, "INPUT_ERROR": 2, "USER_STOPPED": 3, "LIMIT_REACHED": 4, "PREFLIGHT_FAILED": 5,
        "NO_FRAMES": 6, "FW_ERROR": 7, "CAPTURE_ERROR": 8, "ENCODE_FAILED": 9, "VERIFY_FAILED": 10,
        "REFERENCE_MISMATCH": 11, "HARD_STOP": 12, "SIM_ERROR": 13}
VIDEO_OK = ("COMPLETE", "USER_STOPPED", "LIMIT_REACHED")   # outcomes of the capture that get a video
# Failed runs whose spool may hold a checked prefix for `encode --debug-prefix` (never a result).
DEBUG_OK = ("FW_ERROR", "CAPTURE_ERROR", "HARD_STOP", "SIM_ERROR", "REFERENCE_MISMATCH", "NO_FRAMES")
SCRIPT = "isp stream start\nisp stream wait\npoweroff\n"
RX_PACING = ["--uart0-rx-delay-us", "60000", "--uart0-rx-byte-us", "87"]
INSTALL_HINT = """FFmpeg with libx264 is not shipped (GPLv3 + x264). On AlmaLinux 9:
  sudo dnf install -y epel-release
  sudo dnf config-manager --set-enabled crb
  sudo dnf install -y --nogpgcheck https://mirrors.rpmfusion.org/free/el/rpmfusion-free-release-9.noarch.rpm
  sudo dnf install -y ffmpeg
then check: ffmpeg -hide_banner -encoders | grep libx264"""
WB_NOTE = ("WB, CCM and the GTM key are fixed by the profile for the whole session (no per-frame auto-WB): "
           "scenes far from the profile's illuminant show a colour cast.")


class Fail(Exception):
    def __init__(self, status, message):
        Exception.__init__(self, message)
        self.status = status


def say(msg):
    print("isp-stream: " + msg, flush=True)


# ---- colorimetry ------------------------------------------------------------------------------
def colorimetry(word):
    """(range, colorspace) FFmpeg names of a colorimetry word (bit 31 valid, bit 0 full range,
    bits [2:1] CSC_CTRL.std); None when the word does not say (no guess)."""
    if not word & 0x80000000:
        return None
    std = (word >> 1) & 3
    if std > 1:
        return None
    return ("pc" if word & 1 else "tv"), ("smpte170m" if std == 0 else "bt709")


def colorimetry_text(word):
    c = colorimetry(word)
    return "invalid (0x%08X)" % word if not c else "%s range, %s" % ("full" if c[0] == "pc" else "limited",
                                                                       "BT.601" if c[1] == "smpte170m" else "BT.709")


# ---- preflight --------------------------------------------------------------------------------
def find_ffmpeg():
    """(ffmpeg, ffprobe, version line); raises Fail(PREFLIGHT_FAILED)."""
    ff, fp = shutil.which("ffmpeg"), shutil.which("ffprobe")
    if not ff or not fp:
        raise Fail("PREFLIGHT_FAILED", "%s not found on PATH.\n%s" % (" and ".join(
            n for n, p in (("ffmpeg", ff), ("ffprobe", fp)) if not p), INSTALL_HINT))

    def query(*args):
        try:
            r = subprocess.run([ff, "-hide_banner"] + list(args), stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True, timeout=60)
        except (OSError, subprocess.TimeoutExpired) as e:
            raise Fail("PREFLIGHT_FAILED", "%s %s: %s" % (ff, " ".join(args), e))
        if r.returncode:
            raise Fail("PREFLIGHT_FAILED", "%s %s: exit %d" % (ff, " ".join(args), r.returncode))
        return r.stdout
    enc, mux = query("-encoders"), query("-muxers")
    missing = [what for what, ok in (("encoder libx264", any(l.split()[1:2] == ["libx264"] for l in enc.splitlines()
                                                             if len(l.split()) > 1)),
                                     ("encoder mov_text", any(l.split()[1:2] == ["mov_text"] for l in enc.splitlines()
                                                              if len(l.split()) > 1)),
                                     ("muxer mp4", any(l.split()[1:2] == ["mp4"] for l in mux.splitlines()
                                                       if len(l.split()) > 1))) if not ok]
    if missing:
        raise Fail("PREFLIGHT_FAILED", "%s lacks %s.\n%s" % (ff, ", ".join(missing), INSTALL_HINT))
    version = query("-version").splitlines()[0].strip()
    return ff, fp, version


def free_bytes(path):
    st = os.statvfs(path)
    return st.f_bavail * st.f_frsize


def check_out_dir(out):
    if os.path.exists(out):
        if not os.path.isdir(out) or os.listdir(out):
            raise Fail("PREFLIGHT_FAILED", "%s exists and is not an empty directory (nothing is removed or "
                       "overwritten: give a new path)" % out)
    else:
        try:
            os.makedirs(out)
        except OSError as e:
            raise Fail("PREFLIGHT_FAILED", "cannot create %s: %s" % (out, e))


def disk_budget(out, frames, frame_bytes, max_spool, free=None):
    """The spool's upper bound against the free space of DIR's filesystem (video and metadata
    on top: 1/8 of the spool + 64 MiB). Returns the bound in bytes."""
    spool = max_spool if max_spool else frames * frame_bytes
    need = spool + spool // 8 + (64 << 20)
    have = free_bytes(out) if free is None else free
    if need > have:
        raise Fail("PREFLIGHT_FAILED", "%s: %d MiB free, the run needs up to %d MiB (spool %d MiB%s + video and "
                   "metadata); free space, or a smaller --frames / --max-spool-mib"
                   % (out, have >> 20, need >> 20, spool >> 20, " budget" if max_spool else ""))
    return spool


# ---- spool ------------------------------------------------------------------------------------
RECORD_INTS = ("ordinal", "offset", "bytes", "width", "height", "colorimetry", "source_index", "repeat_index")


def frame_problem(fr, n, off, first, sources, repeat):
    """None when the frames.jsonl record fr is frame n of this session at NV12 offset off
    (first: frame 0's record, or None for frame 0), else what is wrong. Every field used
    downstream is checked: types, the geometry, the NV12 size and the identity, which must be
    the schedule's ((n // repeat) mod sources, n mod repeat)."""
    if not isinstance(fr, dict):
        return "not a frame record"
    for k in RECORD_INTS:
        if not isinstance(fr.get(k), int) or isinstance(fr.get(k), bool):
            return "%s missing or not an integer" % k
    sha = fr.get("sha256")
    if not isinstance(sha, str) or len(sha) != 64 or any(ch not in "0123456789abcdef" for ch in sha):
        return "sha256 missing or not 64 hex digits"
    if fr["ordinal"] != n or fr["offset"] != off:
        return "ordinal %s offset %s (expected %d, %d)" % (fr["ordinal"], fr["offset"], n, off)
    if fr["width"] < 2 or fr["height"] < 2:
        return "geometry %sx%s" % (fr["width"], fr["height"])
    if first is not None and (fr["width"], fr["height"], fr["colorimetry"]) != (first["width"], first["height"],
                                                                              first["colorimetry"]):
        return "geometry/colorimetry differs from frame 0"
    if fr["bytes"] != fr["width"] * fr["height"] * 3 // 2:
        return "%d bytes for %dx%d NV12" % (fr["bytes"], fr["width"], fr["height"])
    want = ((n // repeat) % sources, n % repeat)
    if (fr["source_index"], fr["repeat_index"]) != want:
        return "identity source %s repeat %s, the session's schedule (%d source(s), repeat %d) gives %d, %d" % (
            fr["source_index"], fr["repeat_index"], sources, repeat, want[0], want[1])
    return None


def sha_problem(f, fr):
    """None when the NV12 bytes at f's position hash to fr's SHA-256, else what is wrong."""
    h, left = hashlib.sha256(), fr["bytes"]
    while left:
        chunk = f.read(min(left, 1 << 22))
        if not chunk:
            return "NV12 short (torn tail)"
        h.update(chunk)
        left -= len(chunk)
    return None if h.hexdigest() == fr["sha256"] else "NV12 SHA-256 differs from frames.jsonl"


def read_spool(spool, sources, repeat, fix=True):
    """frames (list of frames.jsonl records), notes. Cuts a torn tail (an incomplete last line,
    or NV12 bytes past the last whole frame) when fix; checks every record (frame_problem:
    fields, ordinals 0..n-1, offsets, one geometry and colorimetry, the schedule's identity)
    and every frame's SHA-256. Raises Fail(CAPTURE_ERROR)."""
    jl, nv = os.path.join(spool, "frames.jsonl"), os.path.join(spool, "frames.nv12")
    if not os.path.isfile(jl) or not os.path.isfile(nv):
        raise Fail("CAPTURE_ERROR", "%s: no frames.jsonl / frames.nv12" % spool)
    notes = []
    raw = open(jl, "rb").read()
    frames = []
    good = 0
    for line in raw.splitlines(keepends=True):
        if not line.endswith(b"\n"):
            break
        try:
            frames.append(json.loads(line))
        except ValueError:
            raise Fail("CAPTURE_ERROR", "%s: line %d is not JSON" % (jl, len(frames) + 1))
        good += len(line)
    if good != len(raw):
        if not fix:
            raise Fail("CAPTURE_ERROR", "%s: torn last line" % jl)
        with open(jl, "r+b") as f:
            f.truncate(good)
        notes.append("frames.jsonl: an incomplete last line (%d bytes) cut" % (len(raw) - good))
    off = 0
    for n, fr in enumerate(frames):
        bad = frame_problem(fr, n, off, frames[0] if n else None, sources, repeat)
        if bad:
            raise Fail("CAPTURE_ERROR", "frame %d: %s" % (n, bad))
        off += fr["bytes"]
    size = os.path.getsize(nv)
    if size < off:
        raise Fail("CAPTURE_ERROR", "%s: %d bytes, frames.jsonl accounts for %d (frames missing)" % (nv, size, off))
    if size > off:
        if not fix:
            raise Fail("CAPTURE_ERROR", "%s: %d bytes past the last whole frame" % (nv, size - off))
        with open(nv, "r+b") as f:
            f.truncate(off)
        notes.append("frames.nv12: %d bytes past the last whole frame cut (torn tail)" % (size - off))
    with open(nv, "rb") as f:
        for n, fr in enumerate(frames):
            bad = sha_problem(f, fr)
            if bad:
                raise Fail("CAPTURE_ERROR", "frame %d: %s" % (n, bad))
    return frames, notes


def read_prefix(spool, sources, repeat):
    """For `encode --debug-prefix`: the longest prefix of whole, consecutive frames that pass
    every check of read_spool (frame_problem, then the NV12 SHA-256). Read only: the spool
    stays as the failed run left it. Returns (frames, records in frames.jsonl, the reason the
    prefix ends or None)."""
    jl, nv = os.path.join(spool, "frames.jsonl"), os.path.join(spool, "frames.nv12")
    if not os.path.isfile(jl) or not os.path.isfile(nv):
        return [], 0, "%s: no frames.jsonl / frames.nv12" % spool
    lines = open(jl, "rb").read().splitlines(keepends=True)
    frames, off = [], 0
    with open(nv, "rb") as f:
        for n, line in enumerate(lines):
            if not line.endswith(b"\n"):
                return frames, len(lines), "record %d: torn (no end of line)" % n
            try:
                fr = json.loads(line)
            except ValueError:
                return frames, len(lines), "record %d: not JSON" % n
            bad = frame_problem(fr, n, off, frames[0] if frames else None, sources, repeat)
            if not bad:
                f.seek(off)
                bad = sha_problem(f, fr)
            if bad:
                return frames, len(lines), "frame %d: %s" % (n, bad)
            frames.append(fr)
            off += fr["bytes"]
    return frames, len(lines), None


# ---- encode and verify ------------------------------------------------------------------------
def srt_time(ms):
    return "%02d:%02d:%02d,%03d" % (ms // 3600000, ms // 60000 % 60, ms // 1000 % 60, ms % 1000)


def write_labels(path, frames, names, repeat, fps):
    """One cue per frame at its own PTS: `#ordinal name r=k/K` (k 1-based)."""
    with open(path, "w", encoding="utf-8") as f:
        for n, fr in enumerate(frames):
            a, b = int(round(n * 1000.0 / fps)), int(round((n + 1) * 1000.0 / fps))
            f.write("%d\n%s --> %s\n#%d %s r=%d/%d\n\n" % (n + 1, srt_time(a), srt_time(max(b, a + 1)), fr["ordinal"],
                                                         names[fr["source_index"]], fr["repeat_index"] + 1, repeat))


PLANES = ("y", "u", "v")
DEFAULT_CRF = 10
PSNR_LOW = " PSNR against the spool (first: frame "   # verify's text of a plane below the floor


def check_video_options(fps, crf, min_psnr):
    """The encode/verify options, shared by `run` (before anything is booted) and `encode`."""
    if not 0 < fps < float("inf"):
        raise Fail("INPUT_ERROR", "--fps %s: a finite number > 0" % fps)
    if crf == 0:
        raise Fail("INPUT_ERROR", "--crf 0 is lossless, which the H.264 High profile of the video cannot encode: "
                                  "1..51 (default 10)")
    if not 1 <= crf <= 51:
        raise Fail("INPUT_ERROR", "--crf %d: 1..51 (default 10)" % crf)
    if not 0 < min_psnr < float("inf"):
        raise Fail("INPUT_ERROR", "--min-psnr %s: a finite number of dB > 0 (default 40)" % min_psnr)


def fps_text(fps):
    return ("%.6f" % fps).rstrip("0").rstrip(".")


def encode(ff, spool_nv12, labels, out_mp4, w, h, fps, crf, col, log, frames=None):
    """frames (--debug-prefix): only the spool's first `frames` NV12 frames, fed on stdin, so
    the input itself ends there (an output frame limit would also cut the last cues)."""
    rng, space = col
    cmd = [ff, "-hide_banner", "-v", "error", "-f", "rawvideo", "-pix_fmt", "nv12", "-color_range", rng,
           "-colorspace", space, "-s", "%dx%d" % (w, h), "-framerate", fps_text(fps),
           "-i", spool_nv12 if frames is None else "pipe:0",
           "-i", labels, "-map", "0:v", "-map", "1:s", "-c:v", "libx264", "-preset", "medium", "-crf", str(crf),
           "-profile:v", "high", "-pix_fmt", "nv12", "-color_range", rng, "-colorspace", space,
           "-c:s", "mov_text", "-movflags", "+faststart", "-f", "mp4", out_mp4]
    if frames is None:
        cmd.insert(2, "-nostdin")
    with open(log, "w") as lf:
        lf.write(" ".join(cmd) + ("" if frames is None else "   < the first %d frame(s) of %s" % (frames, spool_nv12))
                 + "\n")
        lf.flush()
        try:
            if frames is None:
                rc = subprocess.call(cmd, stdout=lf, stderr=subprocess.STDOUT)
            else:
                p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=lf, stderr=subprocess.STDOUT)
                left = frames * w * h * 3 // 2
                try:
                    with open(spool_nv12, "rb") as f:
                        while left:
                            chunk = f.read(min(left, 1 << 22))
                            if not chunk:
                                break
                            p.stdin.write(chunk)
                            left -= len(chunk)
                except BrokenPipeError:
                    pass
                try:
                    p.stdin.close()
                except BrokenPipeError:
                    pass
                rc = p.wait()
                if rc == 0 and left:
                    rc = "the spool ended %d byte(s) short of the prefix" % left
        except OSError as e:
            rc = "%s" % e
    return cmd, rc


def verify(ff, fp, mp4, spool_nv12, n, w, h, fps, col, min_psnr, work):
    """The video against the spool: [] when it holds, else the problems; plus the facts."""
    bad, facts = [], {}
    try:
        r = subprocess.run([fp, "-v", "error", "-count_packets", "-show_entries",
                            "stream=index,codec_type,codec_name,profile,width,height,pix_fmt,color_range,color_space,"
                            "r_frame_rate,nb_read_packets,duration:format=duration", "-of", "json", mp4],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=600)
        probe = json.loads(r.stdout) if r.returncode == 0 else None
    except (OSError, ValueError, subprocess.TimeoutExpired) as e:
        probe, r = None, None
        bad.append("ffprobe: %s" % e)
    if probe is None:
        bad.append("ffprobe cannot read %s%s" % (mp4, (": " + r.stderr.strip()[:200]) if r is not None else ""))
        return bad, facts
    vs = [s for s in probe.get("streams", []) if s.get("codec_type") == "video"]
    ss = [s for s in probe.get("streams", []) if s.get("codec_type") == "subtitle"]
    if len(vs) != 1 or len(ss) != 1 or len(probe.get("streams", [])) != 2:
        bad.append("streams: %d video, %d subtitle, %d in all (expected 1, 1, 2)"
                   % (len(vs), len(ss), len(probe.get("streams", []))))
        return bad, facts
    v, s = vs[0], ss[0]
    num, _, den = v.get("r_frame_rate", "0/1").partition("/")
    rate = float(num) / float(den or 1) if float(den or 1) else 0.0
    dur = float(v.get("duration") or probe.get("format", {}).get("duration") or 0)
    facts.update({"codec": v.get("codec_name"), "profile": v.get("profile"), "width": v.get("width"),
                  "height": v.get("height"), "pix_fmt": v.get("pix_fmt"), "color_range": v.get("color_range"),
                  "color_space": v.get("color_space"), "fps": rate, "duration_s": dur,
                  "video_packets": int(v.get("nb_read_packets", -1)), "subtitle_codec": s.get("codec_name"),
                  "subtitle_cues": int(s.get("nb_read_packets", -1))})
    want = [("codec", "h264"), ("width", w), ("height", h), ("color_range", col[0]), ("color_space", col[1]),
            ("subtitle_codec", "mov_text"), ("video_packets", n), ("subtitle_cues", n)]
    bad += ["%s %r, expected %r" % (k, facts[k], x) for k, x in want if facts[k] != x]
    if facts["pix_fmt"] not in ("yuv420p", "yuvj420p", "nv12"):
        bad.append("pix_fmt %s: not 8-bit 4:2:0" % facts["pix_fmt"])
    if abs(rate - fps) > 1e-3 * fps:
        bad.append("fps %.6f, expected %s" % (rate, fps_text(fps)))
    if abs(dur - n / fps) > 0.5 / fps + 1e-3:
        bad.append("duration %.6f s, expected %d / %s = %.6f s" % (dur, n, fps_text(fps), n / fps))
    # Decoded against the spool, frame by frame in order: count and PSNR.
    stats = os.path.join(work, "psnr.log")
    if os.path.exists(stats):
        os.remove(stats)
    cmd = [ff, "-hide_banner", "-nostdin", "-v", "error", "-i", mp4, "-f", "rawvideo", "-pix_fmt", "nv12",
           "-color_range", col[0], "-colorspace", col[1], "-s", "%dx%d" % (w, h), "-framerate", fps_text(fps),
           "-i", spool_nv12, "-lavfi", "[0:v][1:v]psnr=stats_file=psnr.log:shortest=1:repeatlast=0", "-f", "null", "-"]
    try:   # cwd: the stats file name never meets the filtergraph syntax (spaces, ':' in DIR)
        r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=36000, cwd=work)
        rc = r.returncode
    except (OSError, subprocess.TimeoutExpired) as e:
        rc, r = str(e), None
    # One line per compared frame; every plane is checked on its own (a mean of the three, or
    # Y alone, hides a broken chroma plane: swapped or shifted U/V keep Y and the flags right).
    rows, torn = [], 0
    if os.path.exists(stats):
        for line in open(stats):
            got = dict(tok.split(":", 1) for tok in line.split() if ":" in tok)
            try:
                rows.append({p: float(got["psnr_" + p]) for p in PLANES})   # float("inf") parses "inf"
            except (KeyError, ValueError):
                torn += 1
    facts["decoded_frames"] = len(rows) + torn
    for p in PLANES:
        vals = [r_[p] for r_ in rows]
        fin = [x for x in vals if x != float("inf")]
        facts["psnr_%s_min" % p] = min(vals) if vals else None
        facts["psnr_%s_mean" % p] = (sum(fin) / len(fin)) if fin else (float("inf") if vals else None)
    if rc != 0:
        bad.append("decode against the spool failed (exit %s)%s" % (rc, (": " + r.stdout.strip()[:200]) if r else ""))
    if torn:
        bad.append("%d PSNR line(s) without psnr_y/psnr_u/psnr_v in %s" % (torn, stats))
    if len(rows) + torn != n:
        bad.append("%d frame(s) decoded and compared, expected %d" % (len(rows) + torn, n))
    for p in PLANES:
        low = [(k, r_[p]) for k, r_ in enumerate(rows) if not r_[p] >= min_psnr]
        if low:
            bad.append("%d frame(s) below %.1f dB %s" % (len(low), min_psnr, p.upper()) + PSNR_LOW + "%d, %.2f dB): the encode loss "
                       "(--crf), a wrong frame, order, colorimetry%s" % (low[0][0], low[0][1],
                                                                          "" if p == "y" else " or chroma plane"))
    return bad, facts


# ---- summary ----------------------------------------------------------------------------------
def jsonable(x):
    if isinstance(x, float) and x != x:
        return "nan"
    if isinstance(x, float) and x in (float("inf"), float("-inf")):
        return "inf" if x > 0 else "-inf"
    if isinstance(x, dict):
        return {k: jsonable(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [jsonable(v) for v in x]
    return x


def write_summary(out, s):
    path = os.path.join(out, "summary.json")
    tmp = path + ".tmp"
    with open(tmp, "w") as f:   # strict JSON: an identical plane's PSNR (inf) is written "inf"
        json.dump(jsonable(s), f, indent=2, allow_nan=False)
        f.write("\n")
    os.replace(tmp, path)
    lines = ["isp-stream summary", "status            %s (exit %d)" % (s["status"], EXIT[s["status"]])]
    if s.get("message"):
        lines.append("message           %s" % s["message"])
    for k, label in (("session_id", "session"), ("scheduled", "scheduled frames"), ("captured", "captured frames"),
                     ("geometry", "output"), ("colorimetry", "colorimetry"), ("video", "video"),
                     ("fps", "fps"), ("duration_s", "duration (s)"), ("crf", "crf"), ("psnr_min_yuv", "min PSNR Y/U/V"),
                     ("reference", "reference"), ("spool", "spool"), ("simulated_ns", "simulated (ns)"),
                     ("wall_s", "wall (s)")):
        if s.get(k) is not None:
            lines.append("%-17s %s" % (label, s[k]))
    d = s.get("debug_prefix")
    if d:
        lines.append("debug prefix      %s: %s of %s record(s)%s -> %s (debug only; the run stays %s)"
                     % (d.get("status"), d.get("frames"), d.get("records"), (", cut at " + d["cut"]) if d.get("cut") else "",
                        d.get("video", "no video"), s.get("status")))
    for note in s.get("notes", []):
        lines.append("note              %s" % note)
    with open(os.path.join(out, "summary.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")


def finish(out, s, status, message=None):
    s["status"] = status
    s["exit"] = EXIT[status]
    if message:
        s["message"] = message
    if out and os.path.isdir(out):
        write_summary(out, s)
    say("%s%s" % (status, (": " + message) if message else ""))
    return EXIT[status]


# ---- the run ----------------------------------------------------------------------------------
class Runner:
    """fx1_soc in its own process group; Ctrl+C forwarded once, a second one is a hard stop."""

    def __init__(self, argv, log, spool, drain_timeout, total):
        self.argv, self.log, self.spool, self.drain_timeout, self.total = argv, log, spool, drain_timeout, total
        self.presses, self.forwarded, self.stop_at, self.proc = 0, False, None, None
        self.hard = None

    def on_sigint(self, signum, frame):
        self.presses += 1
        if self.presses == 1:
            self.stop_at = time.time()
            print("\nisp-stream: stop requested: the frames in flight are drained, then the video is made "
                  "(Ctrl+C again: hard stop, no video)", flush=True)
        else:
            self.hard = "second Ctrl+C"

    def captured(self):
        try:
            with open(os.path.join(self.spool, "frames.jsonl"), "rb") as f:
                return sum(1 for line in f if line.endswith(b"\n"))
        except OSError:
            return 0

    def on_sigterm(self, signum, frame):
        self.hard = "terminated (SIGTERM)"

    def run(self):
        old = signal.signal(signal.SIGINT, self.on_sigint)
        old_term = signal.signal(signal.SIGTERM, self.on_sigterm)
        try:
            with open(self.log, "wb") as lf:
                self.proc = subprocess.Popen(self.argv, stdout=lf, stderr=subprocess.STDOUT, start_new_session=True)
                last_print, last_n = 0.0, -1
                while self.proc.poll() is None:
                    # The simulator installs its SIGINT handler before it prints: forward once it has.
                    if self.presses and not self.forwarded and os.path.getsize(self.log) > 0:
                        try:
                            self.proc.send_signal(signal.SIGINT)
                        except OSError:
                            pass
                        self.forwarded = True
                    if not self.hard and self.stop_at and time.time() - self.stop_at > self.drain_timeout:
                        self.hard = "no end of the drain within --drain-timeout-s %d" % self.drain_timeout
                    if self.hard:
                        try:
                            os.killpg(self.proc.pid, signal.SIGKILL)
                        except OSError:
                            pass
                        self.proc.wait()
                        break
                    n = self.captured()
                    now = time.time()
                    if n != last_n and now - last_print > 2.0:
                        say("captured %d%s frame(s)" % (n, "/%d" % self.total if self.total else ""))
                        last_print, last_n = now, n
                    time.sleep(0.05)
        finally:
            signal.signal(signal.SIGINT, old)
            signal.signal(signal.SIGTERM, old_term)
            if self.proc is not None and self.proc.poll() is None:   # never leave the simulator behind
                try:
                    os.killpg(self.proc.pid, signal.SIGKILL)
                except OSError:
                    pass
                self.proc.wait()
        return self.proc.returncode


def classify(rc, st, hard, presses):
    """The capture outcome: (status, message)."""
    fw = (st or {}).get("firmware") or {}
    errors = (st or {}).get("errors") or []
    if hard:
        return "HARD_STOP", hard
    if rc is not None and rc < 0:
        return "HARD_STOP", "the simulator was killed by signal %d" % -rc
    if errors or rc == 6:
        return "CAPTURE_ERROR", "host bridge: %s" % ("; ".join(errors) if errors else "exit 6")
    if rc in (3, 4) or st is None:
        return "SIM_ERROR", "the simulator failed (exit %s%s)" % (rc, ", no status.json" if st is None else "")
    state, reason = fw.get("state"), fw.get("stop_reason")
    if state == "ERROR":
        held = "; the ISP stop still drained at the end (the ISP held by the stream)" \
            if fw.get("isp_drain") == "PENDING" else ""
        return "FW_ERROR", "firmware ERROR %s detail 0x%x%s" % (fw.get("error"), fw.get("detail", 0), held)
    if state != "STOPPED" or rc not in (0,):
        return "FW_ERROR", "no clean end of the session (simulator exit %s, firmware %s)" % (rc, state or "no status")
    if reason == "DONE":
        return "COMPLETE", None
    if reason == "USER":
        return "USER_STOPPED", None
    if reason == "LIMIT":
        return "LIMIT_REACHED", ("the spool budget" if fw.get("detail") == 1 else "the 32-bit publish sequence")
    return "FW_ERROR", "stop reason %s" % reason


def sim_time_ns(log):
    try:
        for line in open(log, errors="replace"):
            if line.startswith("[fx1_soc] simulated "):   # "<value> <unit>," (SystemC time)
                v, unit = line.split()[2], line.split()[3].rstrip(",")
                scale = {"fs": 1e-6, "ps": 1e-3, "ns": 1.0, "us": 1e3, "ms": 1e6, "s": 1e9}[unit]
                return int(round(float(v) * scale))
    except (OSError, ValueError, IndexError, KeyError):
        pass
    return None


def make_video(out, s, ff, fp, fps, crf, min_psnr, debug=None):
    """Steps 5-6 on DIR/spool; returns the final status. debug (the summary's debug_prefix
    record): the checked prefix of a failed run into DIR/debug_prefix.mp4, its files in
    DIR/debug_prefix/, the facts in that record; never output.mp4."""
    spool = os.path.join(out, "spool")
    nv12 = os.path.join(spool, "frames.nv12")
    frames = s["_frames"]
    n = len(frames)
    w, h, word = frames[0]["width"], frames[0]["height"], frames[0]["colorimetry"]
    col = colorimetry(word)
    rec = s if debug is None else debug
    work = out if debug is None else os.path.join(out, "debug_prefix")
    os.makedirs(work, exist_ok=True)
    if debug is None:
        labels, tmp, rej = (os.path.join(out, x) for x in ("labels.srt", "output.tmp.mp4", "rejected.mp4"))
        final, log, again = os.path.join(out, "output.mp4"), os.path.join(out, "encode.log"), ""
    else:
        labels, tmp, rej = (os.path.join(work, x) for x in ("labels.srt", "video.tmp.mp4", "rejected.mp4"))
        final, log, again = os.path.join(out, "debug_prefix.mp4"), os.path.join(work, "encode.log"), " --debug-prefix"
    rec.update({"fps": fps, "crf": crf, "min_psnr": min_psnr, "geometry": "%dx%d NV12" % (w, h),
                "colorimetry": colorimetry_text(word)})
    if not col:
        return "ENCODE_FAILED", ("the frames carry no usable colorimetry (0x%08X): no guess is made; spool kept in %s"
                                 % (word, spool))
    if w % 2 or h % 2:
        return "ENCODE_FAILED", "%dx%d: odd geometry for 4:2:0 (never cropped or padded); spool kept" % (w, h)
    write_labels(labels, frames, s["_names"], s["_repeat"], fps)
    for p in (tmp, rej):
        if os.path.exists(p):
            os.remove(p)   # our own leftovers of an earlier encode of this DIR (never output.mp4)
    say("encoding %d frame(s) %dx%d at %s fps, CRF %d, %s%s" % (n, w, h, fps_text(fps), crf, colorimetry_text(word),
                                                              "" if debug is None else " (debug prefix)"))
    cmd, rc = encode(ff, nv12, labels, tmp, w, h, fps, crf, col, log, None if debug is None else n)
    rec["encode_command"] = cmd
    if rc != 0 or not os.path.isfile(tmp):
        if os.path.exists(tmp):
            os.remove(tmp)
        return "ENCODE_FAILED", "ffmpeg exit %s (see %s); spool kept: `isp_stream.py encode --out %s%s` retries" % (
            rc, log, out, again)
    say("verifying the video against the spool")
    bad, facts = verify(ff, fp, tmp, nv12, n, w, h, fps, col, min_psnr, work)
    rec["video_check"] = facts
    for p in PLANES:
        rec["psnr_%s_min" % p] = facts.get("psnr_%s_min" % p)
    if all(rec["psnr_%s_min" % p] is not None for p in PLANES):
        rec["psnr_min_yuv"] = "/".join("%.2f" % rec["psnr_%s_min" % p] for p in PLANES)
    if bad:
        os.replace(tmp, rej)
        rec["video_problems"] = bad
        hint = ""
        if crf > DEFAULT_CRF and all(PSNR_LOW in x for x in bad):
            # Only the encode loss failed, at a CRF above the default: a lower CRF, the floor
            # kept. Never a lower --min-psnr as the way to pass (that accepts more loss).
            hint = ("; at CRF %d the encode loses more than --min-psnr %s allows: `isp_stream.py encode --out %s%s "
                    "--crf %d` re-encodes the kept spool without the VP (keep --min-psnr; lower it only to accept "
                    "more loss on purpose)" % (crf, fps_text(min_psnr), out, again, DEFAULT_CRF))
            rec["video_hint"] = "lower --crf, keep --min-psnr"
        return "VERIFY_FAILED", "%s; video kept as %s, spool kept%s" % ("; ".join(bad), rej, hint)
    if os.path.exists(final):
        return "VERIFY_FAILED", "%s appeared meanwhile: not overwritten (%s kept)" % (final, tmp)
    os.replace(tmp, final)
    rec["video"] = final
    rec["video_sha256"] = prep.sha256_file(final)
    rec["duration_s"] = facts["duration_s"]
    return None, None


def cmd_run(a):
    t0 = time.time()
    out = os.path.abspath(a.out)
    s = {"tool": "isp_stream.py", "command": sys.argv, "out": out, "notes": [WB_NOTE]}
    try:
        check_video_options(a.fps, a.crf, a.min_psnr)
        s.update({"fps": a.fps, "crf": a.crf, "min_psnr": a.min_psnr})   # also for a failed run's --debug-prefix
        if a.loop and not a.max_spool_mib:
            raise Fail("INPUT_ERROR", "--loop needs --max-spool-mib N (the spool budget)")
        if a.max_spool_mib is not None and a.max_spool_mib < 1:
            raise Fail("INPUT_ERROR", "--max-spool-mib %d: N >= 1" % a.max_spool_mib)
        for what, p in (("simulator", a.fx1_soc), ("firmware", a.elf)):
            if not p or not os.path.isfile(p):
                raise Fail("PREFLIGHT_FAILED", "%s not found: %s" % (what, p))
        if a.run_raw and not os.path.isfile(a.run_raw):
            raise Fail("PREFLIGHT_FAILED", "--run-raw not found: %s" % a.run_raw)
        ff, fp, ffv = find_ffmpeg()
        s["ffmpeg"] = ffv
        check_out_dir(out)
    except Fail as e:
        return finish(None, s, e.status, str(e))
    # 2. prepare (the input checks; nothing booted yet)
    session = os.path.join(out, "session")
    argv = ["prepare", "--out", session, "--profile", a.profile, "--repeat", str(a.repeat), "--queue", str(a.queue)]
    argv += ["--manifest", a.manifest] if a.manifest else ["--raw"] + a.raw
    if a.frames is not None:
        argv += ["--frames", str(a.frames)]
    if a.loop:
        argv.append("--loop")
    if a.legacy:
        argv.append("--legacy")
    for opt in ("width", "height", "source_format", "source_stride", "codec_dir", "csr_header"):
        if getattr(a, opt) is not None:
            argv += ["--" + opt.replace("_", "-"), str(getattr(a, opt))]
    say("preparing the session in %s" % session)
    if prep.main(argv) != 0:
        return finish(out, s, "INPUT_ERROR", "the session was refused (see above); nothing booted")
    info = json.load(open(os.path.join(session, "session.json")))
    sch = info["schedule"]
    s.update({"session_id": info["session_id"], "scheduled": None if sch["loop"] else sch["frames"],
              "loop": sch["loop"], "repeat": sch["repeat"], "queue": sch["queue"], "mode": info["mode"],
              "profile": info["profile"], "sources": [{k: x[k] for k in ("index", "name", "path", "sha256")}
                                                       for x in info["sources"]]})
    aw, ah = info["input"]["active_width"], info["input"]["active_height"]
    try:
        bound = disk_budget(out, sch["frames"], aw * ah * 3 // 2, (a.max_spool_mib or 0) << 20)
    except Fail as e:
        return finish(out, s, e.status, str(e))
    # 3. the VP
    spool = os.path.join(out, "spool")
    script = os.path.join(session, "cli.txt")
    with open(script, "w") as f:
        f.write(SCRIPT)
    vp = [a.fx1_soc, "--fw", a.elf, "--timeout-ms", "0", "--uart0-rx-file", script] + RX_PACING
    for line in open(os.path.join(session, "loads.txt")):
        vp += ["--load", line.strip()]
    vp += ["--isp-stream-spool", spool]
    if a.max_spool_mib:
        vp += ["--isp-stream-max-mib", str(a.max_spool_mib)]
    vp += a.vp_arg or []
    s["vp_command"] = vp
    log = os.path.join(out, "platform.log")
    say("booting FX1 (%s, %s); console: %s; Ctrl+C: stop and make the video of what was captured"
        % ("loop" if sch["loop"] else "%d frame(s)" % sch["frames"], "spool bound %.1f MiB" % (bound / 1048576.0), log))
    r = Runner(vp, log, spool, a.drain_timeout_s, None if sch["loop"] else sch["frames"])
    rc = r.run()
    s["simulator_exit"] = rc
    s["simulated_ns"] = sim_time_ns(log)
    s["user_stops"] = r.presses
    st = None
    try:
        st = json.load(open(os.path.join(spool, "status.json")))
    except (OSError, ValueError):
        pass
    s["bridge"] = st
    status, msg = classify(rc, st, r.hard, r.presses)
    # 4. the spool (always checked: also what an error leaves behind)
    try:
        frames, notes = read_spool(spool, sch["sources"], sch["repeat"])
        s["notes"] += notes
    except Fail as e:
        frames = []
        if status in VIDEO_OK or status == "NO_FRAMES":
            status, msg = e.status, str(e)
        else:
            s["notes"].append("spool: %s" % e)
    s["captured"] = len(frames)
    s["spool"] = spool
    s["wall_s"] = round(time.time() - t0, 1)
    if status == "HARD_STOP" and r.stop_at:
        fw = (st or {}).get("firmware") or {}
        msg = "%s; last frame captured: seq %s, firmware %s" % (msg, frames[-1]["seq"] if frames else "none",
                                                                  fw.get("state", "state unknown (no final status)"))
    if status not in VIDEO_OK:
        hint = ""
        if status in DEBUG_OK and os.path.isdir(spool):
            hint = ("; `isp_stream.py encode --out %s --debug-prefix` makes a debug video of the checked prefix "
                    "(never the run's result)" % out)
        return finish(out, s, status, "%s; %d frame(s) kept in %s, no video%s" % (msg, len(frames), spool, hint))
    fw = st.get("firmware") or {}
    if fw.get("completed") != len(frames):
        return finish(out, s, "CAPTURE_ERROR", "the firmware published %s frame(s), the spool holds %d"
                      % (fw.get("completed"), len(frames)))
    if status == "COMPLETE" and not sch["loop"] and len(frames) != sch["frames"]:
        return finish(out, s, "CAPTURE_ERROR", "DONE with %d of %d frame(s)" % (len(frames), sch["frames"]))
    if not frames:
        return finish(out, s, "NO_FRAMES", "the session ended (%s) before a frame was captured: no video" % status)
    if a.run_raw:
        say("checking every frame against the stand-alone model (fx1_isp_run_raw)")
        ref_argv = ["reference", "--session", session, "--run-raw", a.run_raw]
        if sch["loop"] or len(frames) != sch["frames"]:
            ref_argv += ["--frames", str(len(frames))]
        if prep.main(ref_argv) != 0:
            return finish(out, s, "REFERENCE_MISMATCH", "the reference run failed (see above); spool kept")
        ref = json.load(open(os.path.join(session, "reference.json")))
        bad = [(f["ordinal"], k) for f, g in zip(ref["frames"], frames) for k in ("ordinal", "source_index",
                                                                                 "repeat_index", "sha256", "frame_id")
               if f[k] != g[k]]
        if bad or len(ref["frames"]) != len(frames):
            s["reference"] = "MISMATCH %s" % bad[:4]
            return finish(out, s, "REFERENCE_MISMATCH", "frame %s differs from the stand-alone model (%s); spool kept"
                          % (bad[0][0] if bad else "count", bad[0][1] if bad else "frame count"))
        s["reference"] = "every frame (%d) equals fx1_isp_run_raw (SHA-256, identity, frame ID)" % len(frames)
    s["_frames"], s["_names"], s["_repeat"] = frames, [x["name"] for x in info["sources"]], sch["repeat"]
    vstatus, vmsg = make_video(out, s, ff, fp, a.fps, a.crf, a.min_psnr)
    for k in ("_frames", "_names", "_repeat"):
        s.pop(k)
    s["capture_status"] = status
    s["wall_s"] = round(time.time() - t0, 1)
    if vstatus:
        return finish(out, s, vstatus, vmsg)
    if not a.keep_nv12:
        os.remove(os.path.join(spool, "frames.nv12"))
        s["spool"] = "%s (frames.nv12 removed after the verified video; --keep-nv12 keeps it)" % spool
    done = {"COMPLETE": "all %d frame(s)" % len(frames),
            "USER_STOPPED": "stopped by Ctrl+C: the %d frame(s) captured" % len(frames),
            "LIMIT_REACHED": "%s reached: the %d frame(s) that fit" % (msg, len(frames))}[status]
    return finish(out, s, status, "%s, %s" % (s["video"], done))


def cmd_encode(a):
    out = os.path.abspath(a.out)
    path = os.path.join(out, "summary.json")
    try:
        s = json.load(open(path))
    except (OSError, ValueError) as e:
        print("isp-stream: %s: no summary.json of a run (%s)" % (out, e), file=sys.stderr)
        return EXIT["INPUT_ERROR"]
    if not a.debug_prefix and os.path.exists(os.path.join(out, "output.mp4")):
        print("isp-stream: %s already has output.mp4: not overwritten (give a copy of the run directory)" % out,
              file=sys.stderr)
        return EXIT["INPUT_ERROR"]
    fps = a.fps if a.fps is not None else s.get("fps", 30.0)
    crf = a.crf if a.crf is not None else s.get("crf", DEFAULT_CRF)
    min_psnr = a.min_psnr if a.min_psnr is not None else s.get("min_psnr", 40.0)
    try:   # the same checks as `run`; the run directory is left as it is
        check_video_options(fps, crf, min_psnr)
    except Fail as e:
        print("isp-stream: %s" % e, file=sys.stderr)
        return EXIT["INPUT_ERROR"]
    capture = s.get("capture_status") or s.get("status")
    if a.debug_prefix:
        return debug_prefix(out, s, capture, fps, crf, min_psnr)
    if capture not in VIDEO_OK:
        print("isp-stream: the run ended %s: no video is made of it%s" % (
            capture, " (--debug-prefix: a debug video of the checked prefix)" if capture in DEBUG_OK else ""),
            file=sys.stderr)
        return EXIT["INPUT_ERROR"]
    try:
        info = json.load(open(os.path.join(out, "session", "session.json")))
    except (OSError, ValueError) as e:
        print("isp-stream: %s: no session of the run (%s)" % (out, e), file=sys.stderr)
        return EXIT["INPUT_ERROR"]
    try:
        ff, fp, ffv = find_ffmpeg()
        s["ffmpeg"] = ffv
        frames, notes = read_spool(os.path.join(out, "spool"), info["schedule"]["sources"], info["schedule"]["repeat"])
    except Fail as e:
        return finish(out, s, e.status, str(e))
    if len(frames) != s.get("captured"):
        return finish(out, s, "CAPTURE_ERROR", "the spool holds %d frame(s), the run captured %s" % (len(frames), s.get("captured")))
    s["notes"] = s.get("notes", []) + notes + ["re-encoded by `isp_stream.py encode` (no VP run)"]
    s["_frames"], s["_names"], s["_repeat"] = frames, [x["name"] for x in info["sources"]], info["schedule"]["repeat"]
    for k in ("video_problems", "video", "video_sha256", "message", "video_check", "psnr_y_min", "psnr_u_min",
              "psnr_v_min", "psnr_min_yuv", "duration_s", "video_hint"):
        s.pop(k, None)
    vstatus, vmsg = make_video(out, s, ff, fp, fps, crf, min_psnr)
    for k in ("_frames", "_names", "_repeat"):
        s.pop(k)
    s["capture_status"] = capture
    if vstatus:
        return finish(out, s, vstatus, vmsg)
    return finish(out, s, capture, "%s, %d frame(s) (re-encoded; the spool NV12 kept)" % (s["video"], len(frames)))


def debug_prefix(out, s, capture, fps, crf, min_psnr):
    """`encode --debug-prefix`: a debug video of a failed run's checked prefix. The run's status,
    message and spool stay as they are; the result is the summary's debug_prefix record and
    DIR/debug_prefix.mp4, never output.mp4 and never COMPLETE."""
    status = s.get("status")
    if capture in VIDEO_OK:
        print("isp-stream: the run's capture ended %s: its frames make the run's video: `isp_stream.py encode --out %s` "
              "(no --debug-prefix)" % (capture, out), file=sys.stderr)
        return EXIT["INPUT_ERROR"]
    if status not in DEBUG_OK:
        print("isp-stream: the run ended %s: nothing was captured, no prefix to encode" % status, file=sys.stderr)
        return EXIT["INPUT_ERROR"]
    final = os.path.join(out, "debug_prefix.mp4")
    if os.path.exists(final):
        print("isp-stream: %s exists: not overwritten (give a copy of the run directory)" % final, file=sys.stderr)
        return EXIT["INPUT_ERROR"]
    try:
        info = json.load(open(os.path.join(out, "session", "session.json")))
        ff, fp, ffv = find_ffmpeg()
    except (OSError, ValueError) as e:
        print("isp-stream: %s: no session of the run (%s)" % (out, e), file=sys.stderr)
        return EXIT["INPUT_ERROR"]
    except Fail as e:
        print("isp-stream: %s: %s" % (e.status, e), file=sys.stderr)
        return EXIT[e.status]
    frames, records, cut = read_prefix(os.path.join(out, "spool"), info["schedule"]["sources"], info["schedule"]["repeat"])
    if not frames:
        print("isp-stream: no valid prefix in %s (%s): no debug video" % (os.path.join(out, "spool"), cut or "no frame"),
              file=sys.stderr)
        return EXIT["NO_FRAMES"]
    rec = {"frames": len(frames), "records": records, "cut": cut, "ffmpeg": ffv,
           "note": "debug only: the run stays %s; this video is not the run's result and not for acceptance" % status}
    s["debug_prefix"] = rec
    s["_frames"], s["_names"], s["_repeat"] = frames, [x["name"] for x in info["sources"]], info["schedule"]["repeat"]
    say("debug prefix: frames 0..%d of %d record(s)%s; the run stays %s"
        % (len(frames) - 1, records, (", cut at " + cut) if cut else "", status))
    vstatus, vmsg = make_video(out, s, ff, fp, fps, crf, min_psnr, debug=rec)
    for k in ("_frames", "_names", "_repeat"):
        s.pop(k)
    rec["status"] = vstatus or "VERIFIED"
    rec["message"] = vmsg or "%s, %d frame(s), checked against the spool" % (rec["video"], len(frames))
    write_summary(out, s)   # status, exit and message of the run unchanged
    say("debug prefix %s: %s (the run stays %s)" % (rec["status"], rec["message"], status))
    return EXIT[vstatus] if vstatus else 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("run", help="prepare, boot, capture, encode and verify")
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--manifest", help="sequence manifest (JSON, version 1)")
    g.add_argument("--raw", nargs="+", help="RAW files in this order (geometry, Bayer and depth from the profile)")
    p.add_argument("--profile", required=True, help="the tuning profile (.csrw), compiled by the launcher")
    p.add_argument("--out", required=True, help="the run directory: new or empty")
    p.add_argument("--repeat", type=int, default=1, help="K >= 1 ISP frames per source (default 1)")
    f = p.add_mutually_exclusive_group()
    f.add_argument("--frames", type=int, help="N physical frames (default: sources x repeat)")
    f.add_argument("--loop", action="store_true", help="until Ctrl+C or the spool budget (needs --max-spool-mib)")
    p.add_argument("--queue", type=int, default=2, choices=(1, 2, 4), help="frames in flight (default 2)")
    p.add_argument("--legacy", action="store_true", help="legacy mode instead of profile mode")
    p.add_argument("--fps", type=float, default=30.0, help="playback rate of the video (default 30)")
    p.add_argument("--crf", type=int, default=DEFAULT_CRF, help="libx264 CRF 1..51 (default 10, near lossless; 0 refused)")
    p.add_argument("--keep-nv12", action="store_true", help="keep the spool NV12 after the verified video")
    p.add_argument("--max-spool-mib", type=int, help="spool budget: the session stops when it is full (LIMIT)")
    p.add_argument("--min-psnr", type=float, default=40.0, help="verify: lowest Y, U and V PSNR of every frame (default 40 dB)")
    p.add_argument("--width", type=int)
    p.add_argument("--height", type=int)
    p.add_argument("--source-format", choices=sorted(prep.FORMATS))
    p.add_argument("--source-stride", type=int)
    p.add_argument("--codec-dir", help=argparse.SUPPRESS)
    p.add_argument("--csr-header", help=argparse.SUPPRESS)
    p.add_argument("--fx1-soc", help="the simulator (run_vp.sh gives it)")
    p.add_argument("--elf", help="the FreeRTOS CLI firmware (run_vp.sh gives it)")
    p.add_argument("--run-raw", help="also check every frame against fx1_isp_run_raw (the stand-alone model)")
    p.add_argument("--drain-timeout-s", type=int, default=300, help="wall-clock drain bound after Ctrl+C (300)")
    p.add_argument("--vp-arg", action="append", help=argparse.SUPPRESS)  # test / debug: extra fx1_soc arguments
    p.set_defaults(func=cmd_run)
    e = sub.add_parser("encode", help="encode and verify a run's kept spool again (no VP run)")
    e.add_argument("--out", required=True)
    e.add_argument("--fps", type=float)
    e.add_argument("--crf", type=int)
    e.add_argument("--min-psnr", type=float)
    e.add_argument("--debug-prefix", action="store_true",
                   help="a failed run: a debug video (debug_prefix.mp4) of the checked prefix; the run stays failed")
    e.set_defaults(func=cmd_encode)
    a = ap.parse_args(argv)
    try:
        return a.func(a)
    except KeyboardInterrupt:   # outside the VP run (prepare, check, encode): nothing is removed
        print("\nisp-stream: interrupted; nothing is removed (a spool, if any, is kept)", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
