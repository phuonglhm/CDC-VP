#!/usr/bin/env python3
"""ISP RAW-sequence stream on the platform (ISP stream S2): FreeRTOS `isp stream`,
the sim-control doorbell and the fx1_soc --isp-stream-spool host bridge.

  test_stream_platform.py --fx1-soc BUILD/platforms/fx1_soc/fx1_soc
                          --elf BUILD/fw/fx1_soc/freertos_cli.elf
                          --run-raw BUILD/.../fx1_isp_run_raw [--real] [--only NAME ...]

Every captured frame is compared, by SHA-256 of its NV12 and its identity, with the
stand-alone model on the same session (isp_stream_prep.py reference), never only the
last one. Cases (stdlib only, synthetic fixtures unless --real):
  queue_1/2/4     A/B/C x repeat 2, 12 frames: the ring of 4 slots turns 3 times
  phase_*         each Bayer phase at its smallest geometry, RAW10 native and RAW12
  legacy          legacy mode (profile ID 0)
  held_host       the bridge holds every frame 20 ms (--isp-stream-hold-us), queue 4:
                  the firmware must not reuse a slot the host has not acknowledged
  loop_limit      a loop session stopped by the spool budget (LIMIT): exactly the
                  frames that fit, the prefix
  budget_*        the spool budget is an admission limit (firmware): below one frame
                  (refused), one / two frames, exactly eight (LIMIT, spool == budget),
                  a schedule exactly the budget (DONE), a slow host at queue 4
  budget_guard    the firmware is made to admit more than the host's budget (mw into
                  the control block): the bridge refuses the frame that would not fit
                  before writing it, never ACKs it (ERROR HOST, spool <= budget)
  held_sigint     Ctrl+C while a held host drains a loop continuously (queue 4): the
                  stop is forwarded between frames, the firmware drains (USER)
  foreign_ack     an ACK in a record whose session the host altered: never taken
                  (the slot is not reused; ERROR HOST, no ACK)
  detached        the control block does not name the session: start refused
  local_stop      `isp stream stop` right after start (USER, frames cancelled)
  sigint          Ctrl+C (SIGINT to fx1_soc) once frames arrive (USER)
  no_bridge       no --isp-stream-spool: start refused (HOST), nothing runs
  bad_desc        a descriptor word out of range: refused, the word named
  twice           a session runs once per boot
  stale_record    a PUBLISHED record of another session preloaded: never taken
  spool_failure   the spool cannot grow (RLIMIT_FSIZE): never ACKed, ERROR HOST
  exclusive       `isp selftest` while the stream owns the ISP: refused
  copy_error      an uncorrectable DDR error (--ddr3-fault ue) in the last SYS_DMA segment of
                  source 1: frame 2's copy fails after the earlier segments landed; the
                  session ends ERROR COPY, the partly copied input is never queued
                  (submitted 2), the frames before stay published and equal the model,
                  failed = submitted - completed, and the ISP counters are reported
  isp_dma_fault   fx1_soc --isp-dma-fault (VP-only, opt-in): armed past the run it changes
                  nothing (every frame = the model); the IDMA read / ODMA write of frame 2
                  answered with a bus error: ERROR ISP (DMA_ERR.AXI_RD / AXI_WR), the
                  frame never published, frames 0..1 kept = the model, failed = submitted -
                  completed; an IDMA read of frame 2 held past the frame budget: ERROR
                  TIMEOUT (detail = completed), no fatal DMA_ERR, its stop drains past the
                  budget: the terminal state is published with the ISP held (isp_drain
                  PENDING), released once the read returns (DONE). After each, `isp stream
                  idle` then `isp run` configures the ISP from scratch: its NV12 equals the
                  model frame (the reuse oracle). A read held 5 s (past the run): the CLI
                  runs (status, `isp selftest` and a new start refused while the ISP is
                  held), `poweroff` ends the run (not the timeout)
  real            (--real) D65 -> A -> D65 x 2 at 2688x1520 with the shared profile;
                  D65's second frame equals the shipped golden
Exit 0 when every case passes.
"""
import argparse
import json
import os
import resource
import shutil
import signal
import struct
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
DEMO = os.path.dirname(HERE)
ROOT = os.path.abspath(os.path.join(DEMO, "..", "..", ".."))
sys.dont_write_bytecode = True
sys.path.insert(0, DEMO)
import isp_stream_prep as prep  # noqa: E402

D65_GOLDEN = "8da0af679e4e"   # the shipped d65 golden (shared profile, steady state)
failures = []


def check(cond, what):
    print(("ok   " if cond else "FAIL ") + what, flush=True)
    if not cond:
        failures.append(what)
    return cond


class Ctx:
    def __init__(self, a, work):
        self.a, self.work = a, work

        self.sigint_at = None   # frames in the spool when SIGINT was sent

    def path(self, *p):
        return os.path.join(self.work, *p)


def py(*args):
    r = subprocess.run([sys.executable, "-B"] + list(args), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    return r.returncode, r.stdout


def fixture(c, name, case, w=64, h=48):
    d = c.path(name + "_fx")
    if not os.path.isdir(d):
        rc, out = py(os.path.join(DEMO, "stream_fixture.py"), d, "--case", case, "--width", str(w), "--height", str(h))
        assert rc == 0, out
    return os.path.join(d, case, "manifest.json"), os.path.join(d, case, case + ".csrw")


def prepare(c, name, manifest=None, profile=None, raw=None, args=()):
    s = c.path(name)
    argv = [os.path.join(DEMO, "isp_stream_prep.py"), "prepare", "--out", s, "--profile", profile]
    argv += ["--manifest", manifest] if manifest else ["--raw"] + raw
    rc, out = py(*(argv + list(args)))
    assert rc == 0, out
    return s


def reference(c, s, frames=None):
    out = os.path.join(s, "reference.json")
    if os.path.exists(out):
        os.remove(out)
        shutil.rmtree(os.path.join(s, "reference"), ignore_errors=True)
    argv = [os.path.join(DEMO, "isp_stream_prep.py"), "reference", "--session", s, "--run-raw", c.a.run_raw]
    if frames is not None:
        argv += ["--frames", str(frames)]
    rc, txt = py(*argv)
    assert rc == 0, txt
    return json.load(open(out))


def loads(s):
    out = []
    for line in open(os.path.join(s, "loads.txt")):
        out += ["--load", line.strip()]
    return out


def run(c, name, session, script, extra=(), spool=True, timeout_ms=0, wall=300, sigint_after=None, preexec=None):
    """fx1_soc with the FreeRTOS CLI; returns (rc, log, spool dir)."""
    cli = c.path(name + ".cli.txt")
    with open(cli, "w") as f:
        f.write(script)
    sp = c.path(name + ".spool")
    argv = [c.a.fx1_soc, "--fw", c.a.elf, "--timeout-ms", str(timeout_ms), "--uart0-rx-file", cli,
            "--uart0-rx-delay-us", "60000", "--uart0-rx-byte-us", "87"] + (loads(session) if session else [])
    if spool:
        argv += ["--isp-stream-spool", sp]
    argv += list(extra)
    logp = c.path(name + ".log")
    with open(logp, "w") as log:
        p = subprocess.Popen(argv, stdout=log, stderr=subprocess.STDOUT, preexec_fn=preexec)
        t0 = time.time()
        sent = False
        while p.poll() is None:
            if time.time() - t0 > wall:
                p.kill()
                p.wait()
                break
            if sigint_after is not None and not sent:
                try:
                    n = sum(1 for _ in open(os.path.join(sp, "frames.jsonl")))
                except OSError:
                    n = 0
                if n >= sigint_after:
                    p.send_signal(signal.SIGINT)
                    sent = True
                    c.sigint_at = n
            time.sleep(0.05)
    return p.returncode, open(logp, errors="replace").read(), sp


def spool_frames(sp):
    frames = [json.loads(l) for l in open(os.path.join(sp, "frames.jsonl"))]
    data = open(os.path.join(sp, "frames.nv12"), "rb").read()
    import hashlib
    ok = all(hashlib.sha256(data[f["offset"]:f["offset"] + f["bytes"]]).hexdigest() == f["sha256"] for f in frames)
    ok = ok and sum(f["bytes"] for f in frames) == len(data)
    return frames, ok


def status(sp):
    return json.load(open(os.path.join(sp, "status.json")))


def compare(what, ref, frames, n=None):
    """Every captured frame against the reference: SHA-256, identity, frame ID."""
    n = len(frames) if n is None else n
    bad = []
    for f, g in zip(ref["frames"][:n], frames):
        for k in ("ordinal", "source_index", "repeat_index", "sha256", "frame_id"):
            if f[k] != g[k]:
                bad.append((f["ordinal"], k, f[k], g[k]))
    geo = all(g["width"] == ref["width"] and g["height"] == ref["height"] for g in frames)
    return check(len(frames) == n and n <= len(ref["frames"]) and not bad and geo,
                 "%s: %d frame(s) equal the model frame by frame (SHA-256, identity, frame ID, %dx%d)%s"
                 % (what, n, ref["width"], ref["height"], (" mismatches %s" % bad[:3]) if bad else ""))


def nominal(c, what, s, ref_frames=None, extra=(), expect="DONE"):
    rc, log, sp = run(c, what, s, "isp stream start\nisp stream wait\npoweroff\n", extra)
    st = status(sp) if os.path.exists(os.path.join(sp, "status.json")) else {}
    fw = st.get("firmware") or {}
    frames, spool_ok = spool_frames(sp) if os.path.exists(os.path.join(sp, "frames.jsonl")) else ([], False)
    check(rc == 0 and "RESULT PASS" in log and fw.get("state") == "STOPPED" and fw.get("stop_reason") == expect
          and spool_ok and not st.get("errors") and fw.get("completed") == len(frames) == st.get("captured"),
          "%s: STOPPED (%s), %d published = captured, spool consistent (rc %s)" % (what, expect, len(frames), rc))
    ref = reference(c, s, ref_frames)
    compare(what, ref, frames)
    return frames, st, log, ref


# ---- cases ----------------------------------------------------------------------------------
def case_queue(c):
    # 640x480: the ISP takes longer than the firmware's turn between a queue and the
    # next copy, so the copy/processing overlap of queue >= 2 is measurable.
    man, prof = fixture(c, "abc_vga", "rggb_raw12", 640, 480)
    for q in (1, 2, 4):
        s = prepare(c, "queue_%d" % q, man, prof, args=["--repeat", "2", "--frames", "12", "--queue", str(q)])
        frames, st, log, ref = nominal(c, "queue_%d" % q, s)
        fw = st.get("firmware") or {}
        check([f["slot"] for f in frames] == [n % 4 for n in range(12)] and fw.get("idma_frames") == 12
              and fw.get("odma_frames") == 12 and all(f["profile_id"] == 1 for f in frames),
              "queue_%d: slots 0..3 three times, 12 ISP frames in and out, profile ID 1" % q)
        # The depth is invisible in the pixels (S1: queue-invariant); the timestamps
        # show it. Queue 1: frame n+1's copy starts after frame n's EOF; queue >= 2: during it.
        overlap = [n for n in range(11) if frames[n + 1]["copy_ts"] < frames[n]["eof_ts"]]
        check(len(overlap) == (11 if q > 1 else 0), "queue_%d: %d of 11 copies started before the previous frame's EOF%s"
              % (q, len(overlap), " (none at queue 1)" if q == 1 else " (all)"))
        busy = max(sum(1 for m in range(n) if frames[m]["eof_ts"] > frames[n]["ts"]) for n in range(12))
        print("     queue_%d: at most %d earlier frame(s) still in the ISP when a frame was queued" % (q, busy))
    h = [f["sha256"] for f in ref["frames"]]
    check(len(set(h[:6])) == 6 and h[6] != h[0], "queue: A,A',B,B',C,C' all differ; A after C differs from the first A (history kept)")
    check(ref["width"] == 640 and ref["height"] == 480, "queue: 640x480 RGGB output")


def case_phases(c):
    for case in ("rggb_raw12", "grbg_raw10", "gbrg_raw12", "bggr_raw10"):
        b = prep.BAYER[case[:4].upper()]
        w, h = 32 + 2 * (b & 1), 32 + 2 * ((b >> 1) & 1)
        man, prof = fixture(c, "min_" + case, case, w, h)
        s = prepare(c, "phase_" + case, man, prof, args=["--frames", "4", "--queue", "2"])
        frames, st, log, ref = nominal(c, "phase_%s %dx%d" % (case, w, h), s)
        check(ref["width"] == 32 and ref["height"] == 32, "phase_%s: output 32x32" % case)


def case_legacy(c):
    man, prof = fixture(c, "abc", "rggb_raw12")
    s = prepare(c, "legacy", man, prof, args=["--repeat", "2", "--legacy", "--queue", "2"])
    frames, st, log, ref = nominal(c, "legacy", s)
    check(frames and all(f["profile_id"] == 0 for f in frames) and "legacy mode" in log, "legacy: profile ID 0 on every frame")


def case_held(c):
    man, prof = fixture(c, "abc", "rggb_raw12")
    s = prepare(c, "held_host", man, prof, args=["--repeat", "2", "--frames", "12", "--queue", "4"])
    frames, st, log, ref = nominal(c, "held_host", s, extra=["--isp-stream-hold-us", "20000", "--quantum-ns", "10000"])
    check(st.get("max_batch", 0) >= 2 and st.get("hold_us") == 20000,
          "held_host: several frames waited published at once (max batch %s): drained by seq" % st.get("max_batch"))


def case_loop_limit(c):
    man, prof = fixture(c, "abc_big", "bggr_raw12", 256, 144)
    s = prepare(c, "loop_limit", man, prof, args=["--loop", "--queue", "2"])
    rc, log, sp = run(c, "loop_limit", s, "isp stream start\nisp stream wait\npoweroff\n", ["--isp-stream-max-mib", "1"],
                      wall=120)
    st = status(sp)
    fw = st.get("firmware") or {}
    frames, ok = spool_frames(sp)
    fit = (1 << 20) // (fw.get("out_width", 1) * fw.get("out_height", 1) * 3 // 2)
    check(rc == 0 and fw.get("state") == "STOPPED" and fw.get("stop_reason") == "LIMIT" and fw.get("detail") == 1
          and st.get("stop_requested") is None and ok and fw.get("completed") == len(frames) == fit
          and fw.get("submitted") == fit and st.get("spooled_bytes") <= 1 << 20,
          "loop_limit: loop stopped by the spool budget (LIMIT, firmware admission), %d frame(s) = the %d that fit, "
          "%d bytes <= 1 MiB" % (len(frames), fit, st.get("spooled_bytes", 0)))
    compare("loop_limit", reference(c, s, len(frames)), frames)


def budget_run(c, name, case, w, h, mib, frames_args, expect_n, expect="LIMIT", extra=()):
    """A finite session under --isp-stream-max-mib: exactly expect_n frames, never past the budget."""
    man, prof = fixture(c, "%s_%dx%d" % (case, w, h), case, w, h)
    s = prepare(c, name, man, prof, args=frames_args)
    rc, log, sp = run(c, name, s, "isp stream start\nisp stream wait\npoweroff\n",
                      ["--isp-stream-max-mib", str(mib)] + list(extra), wall=240)
    st = status(sp)
    fw = st.get("firmware") or {}
    frames, ok = spool_frames(sp)
    frame = w * h * 3 // 2
    budget = mib << 20
    check(rc == 0 and fw.get("state") == "STOPPED" and fw.get("stop_reason") == expect
          and fw.get("detail") == (1 if expect == "LIMIT" else 0) and ok and not st.get("errors")
          and fw.get("submitted") == fw.get("completed") == len(frames) == expect_n
          and st.get("spooled_bytes") == expect_n * frame <= budget and st.get("max_spool_bytes") == budget
          and ("spool budget %d frame(s)" % (budget // frame)) in log,
          "%s: %dx%d (%d B/frame), budget %d MiB: %s, %d frame(s) = %d B <= %d B (rc %s)"
          % (name, w, h, frame, mib, expect, len(frames), st.get("spooled_bytes", -1), budget, rc))
    compare(name, reference(c, s, len(frames)), frames)
    return fw, st


def case_budget(c):
    # Below one frame: 1024x768 NV12 is 1.125 MiB > 1 MiB: refused at start, nothing runs.
    man, prof = fixture(c, "rggb_1024x768", "rggb_raw12", 1024, 768)
    s = prepare(c, "budget_below", man, prof, args=["--frames", "4", "--queue", "2"])
    rc, log, sp = run(c, "budget_below", s, "isp stream start\nisp stream status\npoweroff\n",
                      ["--isp-stream-max-mib", "1"], timeout_ms=3000)
    st = status(sp)
    fw = st.get("firmware") or {}
    check(rc == 1 and "holds no frame of this output" in log and fw.get("state") == "ERROR" and fw.get("error") == "HOST"
          and fw.get("detail") == prep.HOST_BUDGET and fw.get("submitted") == 0 and st.get("captured") == 0
          and st.get("spooled_bytes") == 0,
          "budget_below: 1024x768 (1179648 B) under 1 MiB: start refused (ERROR HOST detail %s), nothing submitted"
          % fw.get("detail"))
    # One frame fits (768x512: 589824 B), queue 2 (the default of the launcher).
    fw, _ = budget_run(c, "budget_one", "rggb_raw12", 768, 512, 1, ["--frames", "12", "--queue", "2"], 1)
    check(fw.get("cancelled") == 11, "budget_one: the 11 frames not admitted are cancelled (%s)" % fw.get("cancelled"))
    # Two fit (640x480: 460800 B; the review's repro wrote three).
    budget_run(c, "budget_two", "rggb_raw12", 640, 480, 1, ["--frames", "12", "--queue", "2"], 2)
    # Exactly eight (512x512: 393216 B, 8 x = 3 MiB): the spool ends exactly at the budget.
    budget_run(c, "budget_exact", "rggb_raw12", 512, 512, 3, ["--frames", "12", "--queue", "2"], 8)
    # A schedule of exactly the budget completes (DONE, not LIMIT).
    budget_run(c, "budget_fits", "rggb_raw12", 512, 512, 3, ["--frames", "8", "--queue", "2"], 8, expect="DONE")
    # A slow host at queue 4 (the review's repro wrote all 12 frames, 5529600 B).
    budget_run(c, "budget_held", "rggb_raw12", 640, 480, 1, ["--frames", "12", "--queue", "4"], 2,
               extra=["--isp-stream-hold-us", "20000", "--quantum-ns", "10000"])


def case_local_stop(c):
    man, prof = fixture(c, "abc_big", "bggr_raw12", 256, 144)
    s = prepare(c, "local_stop", man, prof, args=["--repeat", "4", "--queue", "1"])
    rc, log, sp = run(c, "local_stop", s, "isp stream start\nisp stream stop\nisp stream wait\npoweroff\n")
    st = status(sp)
    fw = st.get("firmware") or {}
    frames, ok = spool_frames(sp)
    check(rc == 0 and "isp stream stop: requested" in log and fw.get("stop_reason") == "USER" and ok
          and fw.get("cancelled", 0) > 0 and fw.get("cancelled") + fw.get("submitted") == 12
          and fw.get("completed") == fw.get("submitted") == len(frames) and fw.get("failed") == 0,
          "local_stop: USER, %s submitted + %s cancelled = 12, every submitted frame published and captured"
          % (fw.get("submitted"), fw.get("cancelled")))
    if frames:
        compare("local_stop", reference(c, s, len(frames)), frames)


def case_sigint(c):
    man, prof = fixture(c, "abc_big", "bggr_raw12", 256, 144)
    s = prepare(c, "sigint", man, prof, args=["--loop", "--queue", "2"])
    rc, log, sp = run(c, "sigint", s, "isp stream start\nisp stream wait\npoweroff\n", sigint_after=3, wall=120)
    st = status(sp)
    fw = st.get("firmware") or {}
    frames, ok = spool_frames(sp)
    check(rc == 0 and "stop requested (USER): Ctrl+C" in log and fw.get("state") == "STOPPED"
          and fw.get("stop_reason") == "USER" and st.get("user_stops") == 1 and ok and fw.get("completed") == len(frames) >= 3,
          "sigint: one Ctrl+C -> STOP, USER; the firmware drains, %d frame(s), RESULT PASS (rc %s)" % (len(frames), rc))
    if frames:
        compare("sigint", reference(c, s, len(frames)), frames)


def case_budget_guard(c):
    # The bridge's byte guard, independent of the firmware's admission: the control
    # block's budget is raised to 8 MiB (mw) before start, the bridge keeps 1 MiB.
    man, prof = fixture(c, "rggb_raw12_640x480", "rggb_raw12", 640, 480)
    s = prepare(c, "budget_guard", man, prof, args=["--frames", "6", "--queue", "2"])
    off = prep.CONTROL_FIELDS.index("max_bytes_lo") * 4
    recs = c.path("budget_guard.records.bin")
    rc, log, sp = run(c, "budget_guard", s, "mw 0x%08X 0x800000\nisp stream start\nisp stream wait\npoweroff\n"
                      % (prep.CONTROL_ADDR + off),
                      ["--isp-stream-max-mib", "1", "--dump", "0x%08X:%d:%s" % (prep.RECORD_ADDR, 4 * prep.RECORD_BYTES, recs)])
    st = status(sp)
    fw = st.get("firmware") or {}
    frames, ok = spool_frames(sp)
    size = os.path.getsize(os.path.join(sp, "frames.nv12"))
    words = [struct.unpack_from("<32I", open(recs, "rb").read(), 128 * i) for i in range(4)] if os.path.exists(recs) else []
    third = [w for w in words if w[1] == prep.REC_PUBLISHED and w[2] == 3]   # state, seq
    check(rc == 1 and "spool budget 18 frame(s)" in log and "would exceed the spool budget" in log
          and "stop requested (HOST)" in log and fw.get("state") == "ERROR" and fw.get("error") == "HOST"
          and st.get("captured") == 2 == len(frames) and ok and size == 2 * 460800 <= 1 << 20
          and len(third) == 1 and third[0][3] != 3,
          "budget_guard: the firmware admits 18 frames, the bridge's 1 MiB holds 2: frame 3 refused before the write, "
          "never ACKed (ack_seq %s), %d B spooled, ERROR %s (rc %s)"
          % (third[0][3] if third else "?", size, fw.get("error"), rc))
    compare("budget_guard", reference(c, s, len(frames)), frames)


def case_held_sigint(c):
    # The review's repro: a loop at queue 4 with a host holding every frame 20 ms
    # drains without returning to its wake loop; Ctrl+C must still become STOP, USER.
    man, prof = fixture(c, "abc", "rggb_raw12")
    s = prepare(c, "held_sigint", man, prof, args=["--loop", "--queue", "4"])
    t0 = time.time()
    rc, log, sp = run(c, "held_sigint", s, "isp stream start\nisp stream wait\npoweroff\n", sigint_after=2, wall=120,
                      extra=["--isp-stream-hold-us", "20000", "--quantum-ns", "10000"])
    wall = time.time() - t0
    st = status(sp) if os.path.exists(os.path.join(sp, "status.json")) else {}
    fw = st.get("firmware") or {}
    frames, ok = spool_frames(sp)
    at = c.sigint_at or 0
    check(rc == 0 and "stop requested (USER): Ctrl+C" in log and fw.get("state") == "STOPPED"
          and fw.get("stop_reason") == "USER" and st.get("user_stops") == 1 and ok and not st.get("errors")
          and fw.get("completed") == len(frames) >= 2 and len(frames) <= at + 8 and st.get("max_batch", 0) >= 2,
          "held_sigint: Ctrl+C at %d frame(s) while the held host drains: STOP, USER; drained to %d frame(s) "
          "(<= %d + 8), %.0f s wall, max batch %s (rc %s)" % (at, len(frames), at, wall, st.get("max_batch"), rc))
    if frames:
        compare("held_sigint", reference(c, s, len(frames)), frames)


def case_foreign_ack(c):
    # Queue 2, 6 frames: slots 0..3 carry frames 0..3; frame 4 needs slot 0, whose ACK
    # sits in a record of another session: the firmware must not reuse it.
    man, prof = fixture(c, "abc", "rggb_raw12")
    s = prepare(c, "foreign_ack", man, prof, args=["--repeat", "2", "--queue", "2"])
    rc, log, sp = run(c, "foreign_ack", s, "isp stream start\nisp stream wait\npoweroff\n",
                      ["--isp-stream-fault", "foreign-ack"])
    st = status(sp)
    fw = st.get("firmware") or {}
    frames, ok = spool_frames(sp)
    check(rc == 1 and "fault injected: foreign-ack on record 0 seq 1" in log and fw.get("state") == "ERROR"
          and fw.get("error") == "HOST" and fw.get("detail") == prep.HOST_NO_ACK and fw.get("submitted") == 4
          and fw.get("completed") == 4 == len(frames) and ok and not st.get("errors") and st.get("stale_records_ignored", 0) > 0,
          "foreign_ack: the ACK in a foreign record is not taken: slot 0 never reused, 4 of 6 frames, ERROR HOST detail %s "
          "(rc %s)" % (fw.get("detail"), rc))
    compare("foreign_ack", reference(c, s, len(frames)), frames)


def case_detached(c):
    # The bridge attached the session at release; the control block is then made to
    # name another one (mw): start refuses the session.
    man, prof = fixture(c, "abc", "rggb_raw12")
    s = prepare(c, "detached", man, prof)
    rc, log, sp = run(c, "detached", s, "mw 0x%08X 0x1\nisp stream start\npoweroff\n" % (prep.CONTROL_ADDR + 16),
                      timeout_ms=3000)
    st = status(sp)
    fw = st.get("firmware") or {}
    check(rc == 1 and "did not attach this session" in log and fw.get("error") == "HOST"
          and fw.get("detail") == prep.HOST_DETACHED and st.get("attached") is True and st.get("captured") == 0,
          "detached: a control block naming another session: start refused (ERROR HOST detail %s)" % fw.get("detail"))


def case_no_bridge(c):
    man, prof = fixture(c, "abc", "rggb_raw12")
    s = prepare(c, "no_bridge", man, prof)
    rc, log, sp = run(c, "no_bridge", s, "isp stream start\nisp stream status\npoweroff\n", spool=False, timeout_ms=3000)
    check(rc == 1 and "no host bridge attached" in log and "isp stream: ERROR (HOST" in log and "RESULT FAIL" in log
          and "isp stream (VP host bridge)" not in log,
          "no_bridge: start refused (ERROR HOST), poweroff fails, no frame processed")


def case_bad_desc(c):
    man, prof = fixture(c, "abc", "rggb_raw12")
    s = prepare(c, "bad_desc", man, prof)
    d = bytearray(open(os.path.join(s, "desc.bin"), "rb").read())
    word = prep.DESC_FIELDS.index("queue")
    struct.pack_into("<I", d, 4 * word, 3)
    open(os.path.join(s, "desc.bin"), "wb").write(bytes(d))
    rc, log, sp = run(c, "bad_desc", s, "isp stream start\npoweroff\n", timeout_ms=3000)
    check(rc == 1 and ("descriptor word %d invalid" % word) in log and status(sp)["captured"] == 0,
          "bad_desc: queue 3 refused, the word (%d) named, nothing captured" % word)


def case_twice(c):
    man, prof = fixture(c, "abc", "rggb_raw12")
    s = prepare(c, "twice", man, prof)
    rc, log, sp = run(c, "twice", s, "isp stream start\nisp stream wait\nisp stream start\npoweroff\n")
    check(rc == 1 and "this session already ran" in log and status(sp)["captured"] == 3,
          "twice: the second start of the same session is refused; 3 frames captured once")


def case_stale(c):
    man, prof = fixture(c, "abc", "rggb_raw12")
    s = prepare(c, "stale_record", man, prof, args=["--repeat", "2"])
    rec = [0] * 32
    rec[0], rec[1], rec[2], rec[3] = prep.RECORD_MAGIC, prep.REC_PUBLISHED, 1, 0
    rec[4], rec[5] = 0x1234, 0x5678                         # another session
    rec[prep.RECORD_FIELDS.index("y_addr")] = prep.FW_YUV_BUF_BASE
    path = os.path.join(s, "stale_record.bin")
    open(path, "wb").write(struct.pack("<32I", *rec))
    frames, st, log, ref = nominal(c, "stale_record", s, extra=["--load", "0x%08X:%s" % (prep.RECORD_ADDR, path)])
    check(st.get("stale_records_ignored", 0) > 0 and len(frames) == 6,
          "stale_record: the preloaded record of another session was seen (%s) and never taken" % st.get("stale_records_ignored"))


def case_spool_failure(c):
    man, prof = fixture(c, "abc_big", "bggr_raw12", 256, 144)
    s = prepare(c, "spool_failure", man, prof, args=["--repeat", "2", "--queue", "2"])
    frame = 254 * 142 * 3 // 2
    limit = 2 * frame + frame // 2                            # the third frame does not fit

    def lim():
        signal.signal(signal.SIGXFSZ, signal.SIG_IGN)       # write() fails with EFBIG instead
        resource.setrlimit(resource.RLIMIT_FSIZE, (limit, limit))
    recs = c.path("spool_failure.records.bin")
    rc, log, sp = run(c, "spool_failure", s, "isp stream start\nisp stream wait\npoweroff\n", preexec=lim,
                      extra=["--dump", "0x%08X:%d:%s" % (prep.RECORD_ADDR, 4 * prep.RECORD_BYTES, recs)])
    lines = sum(1 for _ in open(os.path.join(sp, "frames.jsonl")))
    st = status(sp)
    fw = st.get("firmware") or {}
    words = [struct.unpack_from("<32I", open(recs, "rb").read(), 128 * i) for i in range(4)] if os.path.exists(recs) else []
    third = [w for w in words if w[1] == prep.REC_PUBLISHED and w[2] == 3]   # state, seq
    check(rc == 1 and "write of frames.nv12" in log and "stop requested (HOST)" in log and lines == 2
          and fw.get("state") == "ERROR" and fw.get("error") == "HOST" and st.get("captured") == 2
          and len(third) == 1 and third[0][3] != 3,
          "spool_failure: frame 3 not stored -> its record never ACKed (ack_seq %s); ERROR HOST; 2 frames captured (rc %s)"
          % (third[0][3] if third else "?", rc))


def case_exclusive(c):
    man, prof = fixture(c, "abc_big", "bggr_raw12", 256, 144)
    s = prepare(c, "exclusive", man, prof, args=["--repeat", "2"])
    rc, log, sp = run(c, "exclusive", s, "isp stream start\nisp selftest\nisp stream start\nisp stream wait\npoweroff\n")
    check("isp selftest: the ISP is in use by the camera or the stream" in log
          and "a session is running" in log and status(sp)["firmware"]["stop_reason"] == "DONE",
          "exclusive: `isp selftest` and a second start refused while the stream runs; the stream completes")


def case_copy_error(c):
    man, prof = fixture(c, "abc", "rggb_raw12")
    s = prepare(c, "copy_error", man, prof, args=["--repeat", "2", "--queue", "2"])
    info = json.load(open(os.path.join(s, "session.json")))
    src1 = int(info["sources"][1]["address"], 16)
    size = 64 * 48 * 2                                     # one source container (RAW16)
    ue = src1 + size - 64                                  # in the last 1008-byte SYS_DMA segment
    check(size > 1008, "copy_error: the source spans several SYS_DMA segments (%d B)" % size)
    rc, log, sp = run(c, "copy_error", s, "isp stream start\nisp stream wait\npoweroff\n",
                      ["--ddr3-fault", "0:ue:0x%08X" % ue])
    st = status(sp)
    fw = st.get("firmware") or {}
    frames, ok = spool_frames(sp)
    check(rc == 1 and "ddr3 fault at" in log and "UE 1" in log and fw.get("state") == "ERROR"
          and fw.get("error") == "COPY" and fw.get("detail", 0) != 0 and fw.get("submitted") == 2
          and fw.get("completed") == len(frames) == st.get("captured") and 1 <= len(frames) <= 2
          and fw.get("failed") == fw.get("submitted") - fw.get("completed") and fw.get("cancelled") == 0
          and ok and not st.get("errors") and all(f["source_index"] == 0 for f in frames),
          "copy_error: UE in source 1 -> ERROR COPY (status 0x%X), the partial copy never queued (submitted %s of 6), "
          "%d published frame(s) of source 0 kept, failed %s (rc %s)"
          % (fw.get("detail", 0), fw.get("submitted"), len(frames), fw.get("failed"), rc))
    check(fw.get("idma_frames") == fw.get("odma_frames") == fw.get("completed"),
          "copy_error: the ISP counters of an ERROR session are reported (in %s, out %s, completed %s)"
          % (fw.get("idma_frames"), fw.get("odma_frames"), fw.get("completed")))
    compare("copy_error", reference(c, s, len(frames)), frames)


def fault_count(log, what):
    """N of `[fx1_soc] isp dma fault (injection): ..., N IDMA read(s) seen` (what: IDMA read / ODMA write)."""
    for line in log.splitlines():
        if line.startswith("[fx1_soc] isp dma fault (injection):") and (what + "(s) seen") in line:
            return int(line.split(", ")[1].split()[0])
    return None


def budget_copy(c, s, name, budget_us):
    """A copy of session s whose frame budget is budget_us (desc.bin and loads.txt rewritten)."""
    sess = c.path(name)
    if os.path.isdir(sess):
        shutil.rmtree(sess)
    shutil.copytree(s, sess)
    d = bytearray(open(os.path.join(sess, "desc.bin"), "rb").read())
    struct.pack_into("<I", d, 4 * prep.DESC_FIELDS.index("frame_budget_us"), budget_us)
    open(os.path.join(sess, "desc.bin"), "wb").write(bytes(d))
    with open(os.path.join(sess, "loads.txt")) as f:
        lines = f.read().replace(s + os.sep, sess + os.sep)
    open(os.path.join(sess, "loads.txt"), "w").write(lines)
    return sess


def reuse_job(c, s, name, frames=2):
    """An `isp run` job after the session (the oracle of the ISP's reuse): the whole configuration again
    (the session's profile, geometry and mode), its input source 0 of the pool, its output output slot 0.
    Returns (fx1_soc --load/--dump arguments, result.bin, nv12 parts)."""
    info = json.load(open(os.path.join(s, "session.json")))
    lay, inp = info["layout"], info["input"]
    aw, ah = inp["active_width"], inp["active_height"]
    y = prep.FW_YUV_BUF_BASE
    uv = y + lay["uv_offset"]
    job = struct.pack("<24I", 0x4A315846, 2, inp["width"], inp["height"], prep.BAYER[inp["bayer"]],
                      int(info["sources"][0]["address"], 16), lay["in_stride"], y, uv, lay["out_stride"],
                      info["profile"]["writes"], frames, 1 if info["mode"] == "legacy" else 0, 1, 1,
                      *([0] * 9))
    jp = c.path(name + ".job.bin")
    open(jp, "wb").write(job)
    res, yp, uvp = c.path(name + ".result.bin"), c.path(name + ".y.bin"), c.path(name + ".uv.bin")
    for f in (res, yp, uvp):
        if os.path.exists(f):
            os.remove(f)
    args = ["--load", "0x%08X:%s" % (prep.FW_SHARED_BASE, jp),
            "--load", "0x%08X:%s" % (prep.FW_SHARED_BASE + 0x1000, os.path.join(s, "profile.bin")),
            "--dump", "0x%08X:96:%s" % (prep.FW_SHARED_BASE + 0x100, res),
            "--dump", "0x%08X:%d:%s" % (y, lay["out_stride"] * ah, yp),
            "--dump", "0x%08X:%d:%s" % (uv, lay["out_stride"] * ah // 2, uvp)]
    return args, (res, yp, uvp, lay["out_stride"], aw, ah)


def reuse_ok(what, parts, want_sha, frames=2):
    """The `isp run` after the session completed and its NV12 equals the model frame."""
    import hashlib
    res, yp, uvp, stride, w, h = parts
    if not all(os.path.exists(f) for f in (res, yp, uvp)):
        return check(False, "%s: the `isp run` dumps exist" % what)
    r = struct.unpack("<24I", open(res, "rb").read())
    y, uv = open(yp, "rb").read(), open(uvp, "rb").read()
    nv12 = b"".join(y[k * stride:k * stride + w] for k in range(h)) + \
        b"".join(uv[k * stride:k * stride + w] for k in range(h // 2))
    got = hashlib.sha256(nv12).hexdigest()
    return check(r[0] == 0x52315846 and r[1] == 0 and r[5] == frames and got == want_sha,
                 "%s: the ISP reused after the session: `isp run` (the whole configuration again) completed %d "
                 "frame(s) (status %d), its NV12 = the model frame (%s)" % (what, r[5], r[1], got[:12]))


ISP_PRIORITY = 0x0C000000 + 4 * 3   # PLIC priority of source 3 (FX1_IRQ_ISP): the stream masks with 0


def priorities(log):
    """The values `md ISP_PRIORITY 1` printed, in order."""
    import re
    return [int(v, 16) for v in re.findall(r"0x%08x: (0x[0-9a-f]+)" % ISP_PRIORITY, log, re.I)]


def case_isp_dma_fault(c):
    man, prof = fixture(c, "abc", "rggb_raw12")
    s = prepare(c, "dma_fault", man, prof, args=["--repeat", "2", "--queue", "2"])
    ref = reference(c, s)
    # Armed beyond the run: the transactions of the 6 frames are counted, nothing changes.
    per = {}
    for kind, what in (("idma-error", "IDMA read"), ("odma-error", "ODMA write")):
        frames, st, log, _ = nominal(c, "dma_fault_idle_" + kind, s, extra=["--isp-dma-fault", "%s:4000000000" % kind])
        n = fault_count(log, what)
        per[kind] = n // 6 if n and n % 6 == 0 else None
        check(per[kind] and "not reached" in log and [f["sha256"] for f in frames] == [f["sha256"] for f in ref["frames"]]
              and (st.get("firmware") or {}).get("isp_drain") == "NONE",
              "isp_dma_fault: %s armed past the run: %s %s(s) (%s per frame), every frame = the model (no change), "
              "the ISP stopped before the end (isp_drain NONE)" % (kind, n, what, per[kind]))
    if not all(per.values()):
        return
    # After the session, `isp stream idle` waits for the ISP, then `isp run` reconfigures it from scratch:
    # frame 1 of the model (source 0, its second frame: the same RAW twice) is the oracle of its reuse.
    # `md` reads the ISP source's PLIC priority before the session and once the ISP is free: restored (the
    # 64x48 `isp run` itself completes without its interrupt, so it cannot show a missing unmask).
    md = "md 0x%08X 1\n" % ISP_PRIORITY
    script = md + "isp stream start\nisp stream wait\nisp stream status\nisp stream idle 5000\n" + md + "isp run\npoweroff\n"
    # Frame 2 (0-based), a few transactions in: frames 0 and 1 are whole before it. The stall holds one
    # IDMA read 200 ms against a 20 ms frame budget: TIMEOUT, then its stop (the soft reset's recovery)
    # cannot retire within the next budget: the terminal state is published with the ISP held, the
    # stop finishes when the read returns (isp_drain DONE). (The default 2 s budget would take minutes.)
    cases = (("axi_rd", "idma-error:%d" % (2 * per["idma-error"] + 5), "ISP", 0x08, None, "NONE"),
             ("axi_wr", "odma-error:%d" % (2 * per["odma-error"] + 10), "ISP", 0x10, None, "NONE"),
             ("timeout", "idma-stall:%d:200000" % (2 * per["idma-error"] + 5), "TIMEOUT", None, 20000, "DONE"))
    for name, spec, error, bit, budget, drain in cases:
        sess = budget_copy(c, s, "dma_fault_" + name, budget) if budget else s
        job, parts = reuse_job(c, sess, "dma_fault_" + name)
        rc, log, sp = run(c, "dma_fault_" + name, sess, script, ["--isp-dma-fault", spec] + job, wall=120)
        st = status(sp) if os.path.exists(os.path.join(sp, "status.json")) else {}
        fw = st.get("firmware") or {}
        frames, ok = spool_frames(sp)
        fatal = fw.get("dma_err", 0) & 0x19   # ALIGN | AXI_RD | AXI_WR
        detail_ok = (fw.get("detail") == bit and fatal == bit) if bit else (fw.get("detail") == fw.get("completed") and not fatal)
        check(rc == 1 and ("fired at #%s" % spec.split(":")[1]) in log and fw.get("state") == "ERROR"
              and fw.get("error") == error and detail_ok and len(frames) == 2 == fw.get("completed") == st.get("captured")
              and fw.get("submitted", 0) >= 3 and fw.get("failed") == fw.get("submitted") - fw.get("completed")
              and ok and not st.get("errors") and fw.get("odma_frames") == 2 and fw.get("isp_drain") == drain,
              "isp_dma_fault %s (%s): ERROR %s detail 0x%X, DMA_ERR 0x%X; frame 2 never published, frames 0..1 kept; "
              "submitted %s, failed %s; ISP in %s out %s; isp_drain %s (rc %s)"
              % (name, spec, fw.get("error"), fw.get("detail", 0), fw.get("dma_err", 0), fw.get("submitted"),
                 fw.get("failed"), fw.get("idma_frames"), fw.get("odma_frames"), fw.get("isp_drain"), rc))
        compare("isp_dma_fault " + name, ref, frames, 2)
        after = log.split("isp stream wait")[-1]
        if budget:
            check("ISP stop draining" in after.split("isp stream idle")[0] and "(held, completed at" in log
                  and "ISP stopped after the end (released)" in after and "isp stream idle: PASS" in log,
                  "isp_dma_fault %s: the session ended while its stop drained (status: ISP held), the held read "
                  "returned, the stop finished and released the ISP (`isp stream idle` passes)" % name)
        pr = priorities(log)
        check(len(pr) == 2 and pr[0] > 0 and pr[1] == pr[0],
              "isp_dma_fault %s: the ISP interrupt is unmasked once the ISP is free (PLIC priority %s)" % (name, pr))
        reuse_ok("isp_dma_fault " + name, parts, ref["frames"][1]["sha256"])

    # A held read that outlasts the run (5 s against a 20 ms budget): the CLI keeps running, the ISP
    # stays held (never handed to another command) and `poweroff` ends the run, not the timeout.
    spec = "idma-stall:%d:5000000" % (2 * per["idma-error"] + 5)
    sess = budget_copy(c, s, "dma_fault_long", 20000)
    rc, log, sp = run(c, "dma_fault_long", sess,
                      "isp stream start\nisp stream wait\nisp stream status\nisp selftest\nisp stream start\n"
                      "isp stream idle 100\nmd 0x%08X 1\npoweroff\n" % ISP_PRIORITY,
                      ["--isp-dma-fault", spec], timeout_ms=3000, wall=180)
    st = status(sp) if os.path.exists(os.path.join(sp, "status.json")) else {}
    fw = st.get("firmware") or {}
    frames, ok = spool_frames(sp)
    after = log.split("isp stream wait")[-1]
    check(rc == 1 and "RESULT TIMEOUT" not in log
          and fw.get("state") == "ERROR" and fw.get("error") == "TIMEOUT" and fw.get("isp_drain") == "PENDING"
          and len(frames) == 2 == fw.get("completed") and ok and not st.get("errors")
          and fw.get("failed") == fw.get("submitted") - fw.get("completed") and "(held, pending" in log,
          "isp_dma_fault long stall (%s): ERROR TIMEOUT with the read still held at the end (isp_drain %s), "
          "frames 0..1 kept, failed %s; `poweroff` ended the run (rc %s)" % (spec, fw.get("isp_drain"), fw.get("failed"), rc))
    compare("isp_dma_fault long stall", ref, frames, 2)
    check("ISP stop draining" in after and "isp selftest: the ISP is in use" in after
          and "the last session's ISP stop still drains" in after
          and "isp stream idle: the ISP stop still drains at the limit" in after and priorities(log) == [0],
          "isp_dma_fault long stall: the CLI runs while the stop drains: status (ISP held), `isp selftest` and a new "
          "start refused, `isp stream idle 100` reports the drain at its limit, the ISP source masked (priority %s)"
          % priorities(log))


def case_real(c):
    s_dir = os.path.join(ROOT, "components", "isp_tlm", "input")
    d65, a_ = os.path.join(s_dir, "D65_raw_2688x1520_5376.raw"), os.path.join(s_dir, "A_raw_2688x1520_5376.raw")
    prof = os.path.join(ROOT, "components", "FX1_Components", "isp", "samples", "profiles", "shared_bggr_2688x1520_full.csrw")
    s = prepare(c, "real", raw=[d65, a_, d65], profile=prof, args=["--repeat", "2", "--queue", "2"])
    frames, st, log, ref = nominal(c, "real", s)
    check(len(frames) == 6 and frames[1]["sha256"].startswith(D65_GOLDEN) and frames[5]["sha256"].startswith(D65_GOLDEN)
          and frames[4]["sha256"] != frames[0]["sha256"],
          "real: D65 r=1 (frames 1 and 5) = the shipped golden %s; D65 after A (frame 4) differs from frame 0" % D65_GOLDEN)


CASES = [("queue", case_queue), ("phases", case_phases), ("legacy", case_legacy), ("held_host", case_held),
         ("loop_limit", case_loop_limit), ("budget", case_budget), ("budget_guard", case_budget_guard),
         ("local_stop", case_local_stop),
         ("sigint", case_sigint), ("held_sigint", case_held_sigint), ("foreign_ack", case_foreign_ack),
         ("detached", case_detached),
         ("no_bridge", case_no_bridge), ("bad_desc", case_bad_desc), ("twice", case_twice),
         ("stale_record", case_stale), ("spool_failure", case_spool_failure), ("exclusive", case_exclusive),
         ("copy_error", case_copy_error), ("isp_dma_fault", case_isp_dma_fault)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fx1-soc", required=True)
    ap.add_argument("--elf", required=True)
    ap.add_argument("--run-raw", required=True)
    ap.add_argument("--real", action="store_true", help="also D65 -> A -> D65 x 2 at 2688x1520 (minutes)")
    ap.add_argument("--only", nargs="+")
    ap.add_argument("--keep", help="work directory to keep (default: a temporary one, removed)")
    a = ap.parse_args()
    work = a.keep or tempfile.mkdtemp(prefix="stream_platform_")
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
