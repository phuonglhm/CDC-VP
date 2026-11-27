#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Console report of an fx1_soc --bw-report run (plan CSI_ISP_BW3, schema fx1-bw/1).

  bw_report.py REPORT [--uart-log LOG] [--dump-dir DIR] [--golden camera_isp_golden.h]
               [--workload KEY=VALUE ...] [--frames]

One measurement window per report (the benchmark's START / STOP marks). A
batch window (START with a capture count N) reports the set of the first N
captures whose FS is at or after START: bytes per frame at each boundary and
the latency of each capture, the set's tail drain included. A steady window
reports the fixed interval [START, STOP): rates are bytes in the interval
divided by its simulated length, whatever drains after STOP.

Labels: MEASURED (from the report), ESTIMATED (from geometry / configured
timing), N/A (not observed: CPU, DMI, SYS_DMA, packet / PHY overhead). Times
and rates are simulated (VP timing model), never host time.
--workload gives what the report cannot know (bits, lanes, llp, fll, w, h,
profile); the launcher passes it. --frames adds one line per capture.
With --dump-dir and --golden the NV12 check of check_bw_report.py runs too.

Verdict (review BW3 F1-F3): PASS only for a nominal benchmark. The simulator's
footer reason is pass; with --uart-log, the firmware printed 'bw: PASS' and its
configuration (RAW bits, batch / steady, N, warm-up, line_length,
frame_length) matches the log and the window; the RAW bits come from the
received data type and the geometry from the outputs, and any --workload
value must agree with them; a batch has its N captures after START, all good;
a steady window has every capture good, exactly one axis_window and one
rx_window of its own window, drained, equal to the sum of its captures when
no capture crosses an edge; every measured per-frame volume equals its
estimate. Otherwise the table is still printed, titled FAILED, and the exit
status is 1. Lanes and profile are configured values, shown as unverified.
Exit 0 nominal PASS, 1 otherwise, 2 usage.

  bw_report.py selftest REPORT [options]   clean must PASS, each mutated copy FAIL
"""
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_bw_report  # noqa: E402  (same directory)

MASTERS = ("MIPI_DMA", "ISP_IDMA", "ISP_ODMA")
DT_BITS = {0x2B: 10, 0x2C: 12}


def table(title, head, rows, note=None):
    w = [max(len(str(h)), *(len(str(r[i])) for r in rows)) if rows else len(str(h)) for i, h in enumerate(head)]
    sep = "+" + "+".join("-" * (x + 2) for x in w) + "+"
    out = [title, sep, "| " + " | ".join(str(h).ljust(w[i]) for i, h in enumerate(head)) + " |", sep]
    for r in rows:
        out.append("| " + " | ".join((str(c).ljust(w[i]) if i == 0 else str(c).rjust(w[i])) for i, c in enumerate(r)) + " |")
    out.append(sep)
    if note:
        out.append(note)
    return "\n".join(out)


def mb(x):
    return f"{x / 1e6:.6f}"


def rate(x):
    return f"{x:.3f}"


class Run:
    def __init__(self, path):
        self.ev = [json.loads(l) for l in Path(path).read_text().splitlines() if l.strip()]
        self.hdr = self.ev[0] if self.ev and self.ev[0].get("ev") == "header" else None
        self.ftr = self.ev[-1] if self.ev and self.ev[-1].get("ev") == "footer" else None
        self.res = (self.hdr or {}).get("time_resolution_fs", 1)

    def sec(self, t):
        return t * self.res * 1e-15

    def complete(self):
        return self.hdr is not None and self.ftr is not None and self.ftr.get("complete") == 1


def analyse(run):
    ev = run.ev
    marks = [e for e in ev if e["ev"] == "mark"]
    starts = [m for m in marks if m["kind"] == "start"]
    stops = [m for m in marks if m["kind"] == "stop"]
    errors = [m for m in marks if m["kind"] == "error"]
    caps = {}

    def cap(s):
        return caps.setdefault(s, {"seq": s})

    for e in ev:
        t = e["ev"]
        if t == "capture_start":
            c = cap(e["seq"]); c["fs"] = e["t"]; c["frame_id"] = e["frame_id"]
        elif t == "capture_end":
            cap(e["seq"])["outcome"] = e["outcome"]
        elif t == "capture_stream_end":
            cap(e["seq"])["boundary"] = e["t"]
        elif t == "axis_capture":
            cap(e["seq"])["axis"] = e
        elif t == "rx_frame" and e["seq"]:
            cap(e["seq"])["rx"] = e
        elif t == "isp_admit":
            cap(e["seq"])["admit"] = e
        elif t == "isp_drop":
            cap(e["seq"])["drop"] = e
        elif t == "output" and e["direct"] and e["seq"]:
            cap(e["seq"]).setdefault("outputs", []).append(e)
        elif t == "retire" and e["direct"] and e["seq"]:
            cap(e["seq"])["retire"] = e
    for c in caps.values():
        done = [o for o in c.get("outputs", []) if o["how"] == "done"]
        c["out"] = done[0] if done else None
        c["good"] = (c.get("outcome") == "ok" and "boundary" in c and c["out"] is not None and
                     c.get("retire", {}).get("ok") == 1)
    dma = {}
    for e in ev:
        if e["ev"] == "dma":
            d = dma.setdefault((e["m"], e["id"]), {"m": e["m"]})
            if e["ph"] == "issue":
                d.update(w=e["w"], n=e["n"], t_issue=e["t"])
            else:
                d.update(t_resp=e["t"], ok=e["ok"])
    windows = [e for e in ev if e["ev"] == "axis_window"]
    rxw = [e for e in ev if e["ev"] == "rx_window"]
    return dict(starts=starts, stops=stops, errors=errors, caps=caps, dma=list(dma.values()), axis_w=windows, rx_w=rxw)


def workload(args, run):
    wl = {}
    for kv in args:
        k, _, v = kv.partition("=")
        wl[k] = v
    return wl


def uart_config(uart):
    """The benchmark firmware's own configuration lines (bw.c), before its window."""
    cfg = {}
    if not uart or not Path(uart).exists():
        return cfg
    keys = {"bits": "bits", "steady": "steady", "frames in the set / window": "n", "warm-up frames": "warmup",
            "line_length_pck": "llp", "frame_length_lines": "fll"}
    for line in Path(uart).read_text(errors="replace").splitlines():
        line = line.replace("\r", "")
        for k, v in keys.items():
            m = re.search(r"bw: %s (0x[0-9a-fA-F]+)" % re.escape(k), line)
            if m:
                cfg[v] = int(m.group(1), 16)
        if "bw: PASS" in line:
            cfg["pass"] = True
    return cfg


def report(path, uart=None, dump=None, golden=None, wl_args=(), frames=False):
    run = Run(path)
    out = []
    if not run.complete():
        print(f"REFUSED: {path} is incomplete (no header or no footer with complete=1)")
        return 1
    a = analyse(run)
    h, wl = run.hdr, workload(wl_args, run)
    if len(a["starts"]) != 1 or len(a["stops"]) != 1 or a["errors"]:
        print(f"REFUSED: expected one START / STOP pair and no marker error, got {len(a['starts'])} / "
              f"{len(a['stops'])} / {len(a['errors'])}")
        return 1
    st, sp = a["starts"][0], a["stops"][0]
    t0, t1 = st["t"], sp["t"]
    mode = st["mode"]
    caps = sorted(a["caps"].values(), key=lambda c: c.get("fs", 0))
    problems = []  # a nominal benchmark has none (review BW3 F1-F3)

    # Run verdict (F2): the event log being complete is not the benchmark passing.
    reason = run.ftr.get("reason")
    if reason != "pass":
        problems.append(f"the run did not pass (simulator verdict: {reason})")
    fw = uart_config(uart)
    if uart and not fw.get("pass"):
        problems.append("the firmware did not report 'bw: PASS' (UART log)")

    # Workload (F3): RAW bits from the received data type, geometry from the outputs.
    dts = {int(c["rx"]["dt"], 16) for c in caps if c.get("rx")}
    geos = {(c["out"]["w"], c["out"]["h"]) for c in caps if c.get("out")}
    bits_obs = DT_BITS.get(next(iter(dts))) if len(dts) == 1 else None
    if len(dts) != 1 or bits_obs is None:
        problems.append(f"RAW data type not unique / not RAW10-12 in the log: {sorted(hex(d) for d in dts)}")
    if len(geos) != 1:
        problems.append(f"output geometry not unique in the log: {sorted(geos)}")
    w, hgt = next(iter(geos)) if len(geos) == 1 else (int(wl.get("w", 0)), int(wl.get("h", 0)))
    p = w * hgt
    bits = bits_obs
    for key, obs in (("bits", bits_obs), ("w", w), ("h", hgt)):
        if key in wl and obs is not None and int(wl[key]) != obs:
            problems.append(f"workload {key}={wl[key]} but the log shows {obs}")
    if "bits" in fw and bits_obs is not None and fw["bits"] != bits_obs:
        problems.append(f"firmware configured RAW{fw['bits']}, the log shows RAW{bits_obs}")
    llp = fw.get("llp", int(wl["llp"]) if "llp" in wl else None)
    fll = fw.get("fll", int(wl["fll"]) if "fll" in wl else None)
    for key in ("llp", "fll"):
        if key in fw and key in wl and int(wl[key]) != fw[key]:
            problems.append(f"workload {key}={wl[key]} but the firmware configured {fw[key]}")
    timing_src = "firmware log" if "llp" in fw and "fll" in fw else "launcher, unverified"
    fps_req = 200e6 / (llp * fll) if llp and fll else None
    if "steady" in fw and bool(fw["steady"]) != (mode == "steady"):
        problems.append(f"firmware steady={fw['steady']} but the window is {mode}")
    clk = h.get("csi_sys_clk_period_ticks")

    def ns(ticks):
        return f"{ticks * run.res * 1e-6:g} ns" if ticks else "?"

    out.append(f"Build: {h.get('package')} {h.get('revision', '?')[:12]} ({h.get('revision_state')}), "
               f"{h.get('build_type')}, SystemC {h.get('systemc')}")
    out.append(f"Path: direct                 Source: sensor via detailed D-PHY, RAW{bits or '?'} (log data type), "
               f"{wl.get('lanes', '?')} lanes (configured, unverified)")
    out.append("Pipeline: Sensor -> D-PHY -> CSI-2 RX -> RAW Unpack -> AXIS -> ISP -> ODMA -> DDR NV12")
    out.append(f"Input: {w}x{hgt} (log), line_length {llp or '?'} pck x frame_length {fll or '?'} lines @ 200 MHz = "
               f"{fps_req:.3f} FPS requested [ESTIMATED; timing from the {timing_src}]" if fps_req
               else f"Input: {w}x{hgt} (log)")
    out.append(f"Output: NV12 {w}x{hgt}, one channel   Profile: {wl.get('profile', 'basic RGGB')} (configured)")
    out.append(f"Clocks: CSI sys {ns(clk)}, ISP core {ns(h.get('isp_core_period_ticks'))}; "
               f"TLM quantum {ns(h.get('quantum_ticks'))}")
    out.append(f"Scope: Camera DMA only (MIPI_DMA, ISP_IDMA, ISP_ODMA); CPU / DMI / SYS_DMA N/A")

    def bytes_of(c, key):
        if key == "csi":
            return c.get("rx", {}).get("image_bytes")
        if key == "axis":
            return c.get("axis", {}).get("useful_bytes")
        if key == "odma":
            return c["out"]["write_bytes"] if c.get("out") else None

    def lat(c):
        return run.sec(c["out"]["t_last_write_response"] - c["fs"]) * 1e3 if c.get("out") and "fs" in c else None

    # Window summaries (F1): exactly one of each, of this window, drained.
    axw = [e for e in a["axis_w"] if e.get("window") == st["window"]]
    rxw = [e for e in a["rx_w"] if e.get("window") == st["window"]]
    if len(a["axis_w"]) != 1 or len(a["rx_w"]) != 1 or len(axw) != 1 or len(rxw) != 1:
        problems.append(f"window summaries: {len(a['axis_w'])} axis_window / {len(a['rx_w'])} rx_window, "
                        f"{len(axw)} / {len(rxw)} of window {st['window']} (exactly one each expected)")
    ax = axw[0] if len(axw) == 1 else None
    rx = rxw[0] if len(rxw) == 1 else None
    if rx is not None and (rx.get("drained") != 1 or rx.get("t_start") != t0 or rx.get("t_stop") != t1):
        problems.append(f"rx_window not drained or not [START, STOP): {rx}")

    def need(d, key):
        if d is None or key not in d:
            problems.append(f"window summary field {key} missing")
            return None
        return d[key]

    # Window counters against the captures (F1), both modes: with no capture across an
    # edge of [START, STOP), the window holds exactly its captures' bytes and beats.
    w_in = [c for c in caps if "fs" in c and t0 <= c["fs"] < t1]
    w_cross = [c for c in caps if "fs" in c and c["fs"] < t1 and c.get("boundary", t1) >= t0 and
               not (c["fs"] >= t0 and c.get("boundary", t1) < t1)]
    if not w_cross and ax is not None and rx is not None:
        for name, got, want in (
                ("AXIS useful bytes", ax.get("useful_bytes"), sum(c.get("axis", {}).get("useful_bytes", 0) for c in w_in)),
                ("AXIS accepted beats", ax.get("accepted_beats"),
                 sum(c.get("axis", {}).get("accepted_beats", 0) for c in w_in)),
                ("RX image bytes", rx.get("image_bytes"), sum(c.get("rx", {}).get("image_bytes", 0) for c in w_in))):
            if got != want:
                problems.append(f"window {name} {got} != the captures' {want}")
    elif w_cross:
        out.append(f"Note: {len(w_cross)} capture(s) cross a window edge: window counters not compared with captures")

    est_rows_bad = False
    if mode == "batch":
        n = st["count"]
        if "n" in fw and fw["n"] != n:
            problems.append(f"firmware configured a set of {fw['n']}, the START mark {n}")
        after = [c for c in caps if c.get("fs", -1) >= t0]
        sset, extra = after[:n], after[n:]
        good = [c for c in sset if c["good"]]
        if len(sset) < n:
            problems.append(f"batch set of {n} requested, {len(sset)} capture(s) after START")
        if len(good) != len(sset):
            problems.append(f"{len(sset) - len(good)} capture(s) of the set not good")
        ends = [o["t_last_write_response"] if "t_last_write_response" in o else o["t_end"]
                for c in sset for o in c.get("outputs", [])]
        t_end = max(ends, default=t1)
        dur = run.sec(t_end - sset[0]["fs"]) if sset else 0
        out.append(f"Window: batch, set of the first {n} capture(s) from START; batch {dur * 1e3:.3f} ms "
                   f"(first FS -> last write response of the set, every capture of the set, tail drain included; "
                   f"not a steady rate)")
        out.append("")
        out.append(table("FRAME SUMMARY (the set)", ["Requested", "In set", "Good", "Dropped", "Errors", "Extra after set"],
                         [[n, len(sset), len(good), sum(c.get("outcome") == "dropped" or "drop" in c for c in sset),
                           sum(c.get("outcome") not in ("ok", None) for c in sset), len(extra)]]))
        rows = []
        for name, key, form, est in (("Sensor -> CSI RX", "csi", f"RAW{bits} packed", p * (bits or 0) // 8),
                                     ("CSI -> ISP AXIS", "axis", "RAW16 container", 2 * p),
                                     ("ISP ODMA -> DDR", "odma", "NV12", 3 * p // 2)):
            vals = [bytes_of(c, key) for c in sset if bytes_of(c, key) is not None]
            avg = sum(vals) / len(vals) if vals else 0
            ok = len(vals) == len(sset) and len(vals) > 0 and all(v == est for v in vals)
            est_rows_bad |= not ok
            rows.append([name, form, f"{avg:.0f}", est, "yes" if ok else "NO"])
        out.append("")
        out.append(table("LINK PAYLOAD PER FRAME - DO NOT ADD ROWS TO A DDR TOTAL",
                         ["Boundary", "Format", "MEASURED B/frame", "ESTIMATED B/frame", "= estimate"], rows))
        lo = sset[0]["fs"] if sset else t0
        drows = []
        for m in MASTERS:
            r = sum(d["n"] for d in a["dma"] if d["m"] == m and not d.get("w") and lo <= d.get("t_issue", -1) <= t_end)
            wr = sum(d["n"] for d in a["dma"] if d["m"] == m and d.get("w") and lo <= d.get("t_issue", -1) <= t_end)
            drows.append([m, r, wr, f"{(r + wr) / max(len(sset), 1):.0f}"])
        drows.append(["CPU staging / SYS_DMA", "N/A", "N/A", "N/A"])
        out.append("")
        out.append(table("CAMERA DMA DDR TRAFFIC OF THE SET (issued from the first FS to the last response)",
                         ["Master", "Read B", "Write B", "B/frame"], drows))
        ls = [lat(c) for c in good]
        if ls:
            out.append("")
            out.append(f"LATENCY  CSI FS -> ODMA last write response: min {min(ls):.3f}, mean {sum(ls) / len(ls):.3f}, "
                       f"max {max(ls):.3f} ms ({len(ls)} capture(s))")
        sel = sset
        need_good = len(sset)
    else:
        dt = run.sec(t1 - t0)
        inside = [c for c in caps if "fs" in c and t0 <= c["fs"] < t1]
        before = [c for c in caps if "fs" in c and c["fs"] < t0]
        whole = [c for c in inside if c.get("boundary", t1) < t1]
        good_done = [c for c in caps if c["good"] and t0 <= c["out"]["t_end"] < t1]
        crossing = [c for c in caps if "fs" in c and c["fs"] < t1 and c.get("boundary", t1) >= t0 and
                    not (c["fs"] >= t0 and c.get("boundary", t1) < t1)]
        drops = sum(1 for c in inside if c.get("outcome") == "dropped" or "drop" in c)
        errs = sum(1 for c in inside if c.get("outcome") not in ("ok", None) and c.get("outcome") != "dropped")
        bad_inside = [c["seq"] for c in inside if not c["good"]]
        if bad_inside:
            problems.append(f"capture(s) {bad_inside} of the window not good")
        if "n" in fw and fw["n"] != len(inside):
            problems.append(f"firmware window of {fw['n']} frame(s), {len(inside)} capture(s) started in it")
        if "warmup" in fw and fw["warmup"] != len(before):
            problems.append(f"firmware warm-up {fw['warmup']} frame(s), {len(before)} capture(s) before START")
        out.append(f"Window: steady-state, {dt * 1e3:.6f} ms simulated [START, STOP); drain after STOP not counted")
        out.append(f"Warm-up: {len(before)} capture(s) before START" +
                   (f" (firmware: {fw['warmup']})" if "warmup" in fw else ""))
        out.append(f"Captures: {len(inside)} started in the window, {len(whole)} wholly inside, "
                   f"{len(crossing)} crossing a window edge")
        out.append("")
        out.append(table("FRAME SUMMARY", ["Input", "Admitted", "Good output", "In FPS", "Out FPS", "Drops", "Errors"],
                         [[len(inside), sum("admit" in c and t0 <= c["admit"]["t"] < t1 for c in caps), len(good_done),
                           f"{len(inside) / dt:.2f}", f"{len(good_done) / dt:.2f}", drops, errs]]))
        rows = []
        for name, form, val, est_b in (("Sensor -> CSI RX", f"RAW{bits} packed", need(rx, "image_bytes"), p * (bits or 0) // 8),
                                       ("CSI -> ISP AXIS", "RAW16 container", need(ax, "useful_bytes"), 2 * p)):
            per = [bytes_of(c, "csi" if name.startswith("Sensor") else "axis") for c in whole]
            per = [v for v in per if v is not None]
            if per and any(v != est_b for v in per):
                est_rows_bad = True
            rows.append([name, form, f"{(sum(per) / len(per)) / 1e6:.6f}" if per else "N/A",
                         rate(val / dt / 1e6) if val is not None else "N/A",
                         rate(est_b * fps_req / 1e6) if fps_req else "N/A"])
        odma_w = [d for d in a["dma"] if d["m"] == "ISP_ODMA" and d.get("w") and t0 <= d.get("t_issue", -1) < t1]
        per = [c["out"]["write_bytes"] for c in whole if c.get("out")]
        if per and any(v != 3 * p // 2 for v in per):
            est_rows_bad = True
        rows.append(["ISP ODMA -> DDR", "NV12", f"{(sum(per) / len(per)) / 1e6:.6f}" if per else "N/A",
                     rate(sum(d["n"] for d in odma_w) / dt / 1e6), rate(1.5 * p * fps_req / 1e6) if fps_req else "N/A"])
        out.append("")
        out.append(table("LINK PAYLOAD - DO NOT ADD THESE ROWS TO DDR TOTAL",
                         ["Boundary", "Format", "MB/frame", "Avg MB/s", "ESTIMATED MB/s"], rows,
                         "MB/frame: mean over the captures wholly inside the window; ESTIMATED at the requested FPS."))
        if clk and ax is not None:
            cycles = (t1 - t0) / clk
            out.append(f"AXIS: occupancy {ax.get('accepted_beats', 0) / cycles * 100:.4f} % of {cycles:.0f} cycles, "
                       f"payload utilization {ax.get('useful_bytes', 0) / (8 * cycles) * 100:.4f} %, stall cycles "
                       f"{ax.get('stall_cycles', 0)}, longest TREADY-low run {ax.get('max_stall_run', 0)}, "
                       f"null-abort beats {ax.get('abort_beats', 0)}")
        drows, tot_r, tot_w = [], 0, 0
        for m in MASTERS:
            r = sum(d["n"] for d in a["dma"] if d["m"] == m and not d.get("w") and t0 <= d.get("t_issue", -1) < t1)
            wr = sum(d["n"] for d in a["dma"] if d["m"] == m and d.get("w") and t0 <= d.get("t_issue", -1) < t1)
            tot_r, tot_w = tot_r + r, tot_w + wr
            ng = max(len(good_done), 1)
            drows.append([m, rate(r / dt / 1e6), rate(wr / dt / 1e6), rate((r + wr) / dt / 1e6), mb((r + wr) / ng)])
        drows.append(["CPU staging", "N/A", "N/A", "N/A", "N/A"])
        drows.append(["SYS_DMA", "N/A", "N/A", "N/A", "N/A"])
        drows.append(["Camera DMA total", rate(tot_r / dt / 1e6), rate(tot_w / dt / 1e6), rate((tot_r + tot_w) / dt / 1e6),
                      mb((tot_r + tot_w) / max(len(good_done), 1))])
        out.append("")
        out.append(table("DDR IMAGE-DATA TRAFFIC (bytes issued in the window)",
                         ["Master", "Read MB/s", "Write MB/s", "Total MB/s", "MB/good frame"], drows,
                         "Total = read + write of the three camera DMA masters; not the platform's total DDR."))
        for m in MASTERS:
            def outstanding(t):
                return sum(1 for d in a["dma"] if d["m"] == m and d.get("t_issue", 1 << 62) <= t) - \
                       sum(1 for d in a["dma"] if d["m"] == m and d.get("t_resp", 1 << 62) <= t)
            o0, o1 = outstanding(t0), outstanding(t1)
            if o0 or o1:
                out.append(f"Outstanding {m} at START / STOP: {o0} / {o1}")
        out.append(f"Outstanding at START / STOP (all camera DMA): "
                   f"{sum(1 for d in a['dma'] if d.get('t_issue', 1 << 62) <= t0 < d.get('t_resp', 1 << 62))} / "
                   f"{sum(1 for d in a['dma'] if d.get('t_issue', 1 << 62) <= t1 < d.get('t_resp', 1 << 62))}")
        ins = [d for d in a["dma"] if t0 <= d.get("t_issue", -1) < t1]
        for bucket_us in (100, 1000):
            bt = int(bucket_us * 1e-6 / (run.res * 1e-15))
            nb = max(1, -(-(t1 - t0) // bt))
            b = [0] * nb
            for d in ins:
                b[(d["t_issue"] - t0) // bt] += d["n"]
            full = b[:-1] if (t1 - t0) % bt else b  # a partial last bucket is not a peak
            pk = max(full or b)
            out.append(f"Camera DMA peak, {bucket_us} us buckets (issue time, {len(full or b)} full bucket(s)): "
                       f"{pk / (bucket_us * 1e-6) / 1e6:.3f} MB/s")
        ls = [lat(c) for c in caps if c["good"] and "fs" in c and t0 <= c["fs"] < t1]
        if ls:
            out.append(f"LATENCY  CSI FS -> ODMA last write response (captures from the window): min {min(ls):.3f}, "
                       f"mean {sum(ls) / len(ls):.3f}, max {max(ls):.3f} ms ({len(ls)})")
        sel = inside
        need_good = len(inside)
    if est_rows_bad:
        problems.append("a measured per-frame volume differs from its estimate (= estimate: NO)")

    if frames:
        rows = []
        for c in sel:
            o = c.get("out") or {}
            rows.append([c["seq"], c.get("frame_id", ""), c.get("outcome", "-"), bytes_of(c, "csi") or "-",
                         bytes_of(c, "axis") or "-", o.get("write_bytes", "-"), o.get("slot", "-"),
                         f"{lat(c):.3f}" if lat(c) is not None else "-", "yes" if c["good"] else "no"])
        out.append("")
        out.append(table("FRAMES", ["seq", "FRAME_ID", "CSI end", "CSI B", "AXIS B", "ODMA B", "slot",
                                    "FS->last resp ms", "good"], rows))

    chk_args = [path, "--direct-only", "--min-good", str(max(need_good, 1))]
    if dump:
        chk_args += ["--dump-dir", dump]
    if golden:
        chk_args += ["--golden", golden]
    if uart:
        chk_args += ["--uart-log", uart]
    res = check_bw_report.check(chk_args)
    bad = res[0]
    out.append("")
    out.append(f"CHECKS: {'PASS' if not bad else 'FAIL'} (check_bw_report.py: report complete, DMA pairs, no RAW in DDR, "
               f"per-frame volumes, output times{', NV12 = golden (' + str(res[2]) + ' frames)' if len(res) > 2 and dump else ''})")
    for b in bad:
        out.append(f"  FAIL {b}")
    verdict_ok = not bad and not problems
    out.append(f"VERDICT: {'PASS (nominal benchmark)' if verdict_ok else 'FAIL - not a nominal benchmark result'}")
    for pr in problems:
        out.append(f"  FAIL {pr}")
    out.append("")
    out.append("NOTES")
    out.append("MB = 1,000,000 bytes; FPS and MB/s use simulated time, not host time.")
    out.append("Image data only: code / stack / metadata / profile writes are not camera DMA traffic.")
    out.append("Sensor payload excludes packet headers, CRC and PHY overhead (N/A).")
    out.append("Latency ends at the last write response, not at DONE, EOF_TS or the application's dequeue.")
    title = ("CSI -> ISP BANDWIDTH REPORT | MEASURED (VP timing model, simulated time)" if verdict_ok else
             "CSI -> ISP BANDWIDTH REPORT | FAILED RUN OR INCONSISTENT DATA - NOT A NOMINAL RESULT")
    print("\n".join([title] + out))
    return 0 if verdict_ok else 1


def selftest(path, rest):
    """Sensitivity of report() on a clean run (review BW3 F1-F3): clean must PASS,
    every copy below must FAIL."""
    import contextlib
    import io
    import shutil
    import tempfile

    def run_report(lines, args, uart_edit=None):
        with tempfile.TemporaryDirectory() as tmp:
            pth = Path(tmp) / "bw.jsonl"
            pth.write_text("\n".join(json.dumps(x, separators=(",", ":")) for x in lines) + "\n")
            a = list(args)
            if uart_edit and "--uart-log" in a:
                u = Path(tmp) / "uart.log"
                u.write_text(uart_edit(Path(a[a.index("--uart-log") + 1]).read_text(errors="replace")))
                a[a.index("--uart-log") + 1] = str(u)
            buf = io.StringIO()
            with contextlib.redirect_stdout(buf):
                rc = report(str(pth), **parse_opts(a))
            return rc

    base = [json.loads(l) for l in Path(path).read_text().splitlines() if l.strip()]
    results = [("clean", run_report(base, rest) == 0)]

    def mut(name, edit, args=None, uart_edit=None):
        lines = edit(json.loads(json.dumps(base)))
        results.append((name, run_report(lines, args if args is not None else rest, uart_edit) != 0))

    mut("axis/rx window summaries removed", lambda l: [x for x in l if x["ev"] not in ("axis_window", "rx_window")])
    def zero(l):
        for x in l:
            if x["ev"] == "axis_window":
                x["useful_bytes"] = 0
            if x["ev"] == "rx_window":
                x["image_bytes"] = 0
        return l
    mut("window bytes = 0", zero)
    def short(l):
        for x in l:
            if x["ev"] == "rx_window":
                x["image_bytes"] -= 4
        return l
    mut("window RX bytes short by 4", short)
    def dup(l):
        k = next(i for i, x in enumerate(l) if x["ev"] == "axis_window")
        return l[:k + 1] + [dict(l[k])] + l[k + 1:]
    mut("duplicate axis_window", dup)
    def wrong_id(l):
        for x in l:
            if x["ev"] in ("axis_window", "rx_window"):
                x["window"] += 1
        return l
    mut("window summaries of another window", wrong_id)
    def reason(l):
        l[-1]["reason"] = "fail"
        return l
    mut("footer reason=fail", reason)
    def bad_cap(l):
        st = next(x for x in l if x["ev"] == "mark" and x["kind"] == "start")
        seq = next(x["seq"] for x in l if x["ev"] == "capture_start" and x["t"] >= st["t"])
        for x in l:
            if x["ev"] == "capture_end" and x["seq"] == seq:
                x["outcome"] = "missing_fe"
        return l
    mut("one capture of the window / set missing_fe", bad_cap)
    def more(l):
        for x in l:
            if x["ev"] == "mark" and x["kind"] == "start" and x.get("mode") == "batch":
                x["count"] += 1
                x["value"] = "0x%x" % ((x["count"] << 8) | 1)
        return l
    if any(x["ev"] == "mark" and x.get("mode") == "batch" for x in base):
        mut("batch START count + 1", more)
    a = list(rest)
    if "--workload" in a:
        bits = next(v.split("=")[1] for v in rest if v.startswith("bits="))
        other = "10" if bits == "12" else "12"
        mut("workload bits " + other, lambda l: l, [x.replace("bits=" + bits, "bits=" + other) for x in rest])
        mut("workload w=128", lambda l: l, [x.replace("w=64", "w=128") for x in rest])
    if "--uart-log" in rest:
        mut("firmware without 'bw: PASS'", lambda l: l, None, lambda t: t.replace("bw: PASS", "bw: FAIL"))
        mut("firmware line_length 2400", lambda l: l, None, lambda t: t.replace("line_length_pck 0x000007d0",
                                                                                  "line_length_pck 0x00000960"))
    ok = results[0][1] and all(f for _, f in results[1:])
    for name, flag in results:
        print(f"  {name}: {('PASS' if flag else 'NOT AS REQUIRED') if name == 'clean' else ('FAIL as required' if flag else 'NOT AS REQUIRED')}")
    print(f"bw_report: selftest {'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


def parse_opts(argv):
    o = {"uart": None, "dump": None, "golden": None, "wl_args": [], "frames": False}
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in ("--uart-log", "--dump-dir", "--golden"):
            o[{"--uart-log": "uart", "--dump-dir": "dump", "--golden": "golden"}[a]] = argv[i + 1]
            i += 2
        elif a == "--workload":
            o["wl_args"].append(argv[i + 1])
            i += 2
        elif a == "--frames":
            o["frames"], i = True, i + 1
        else:
            raise SystemExit(2)
    return o


def main():
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help"):
        print(__doc__)
        return 2
    if sys.argv[1] == "selftest" and len(sys.argv) > 2:
        return selftest(sys.argv[2], sys.argv[3:])
    try:
        return report(sys.argv[1], **parse_opts(sys.argv[2:]))
    except SystemExit:
        print(__doc__)
        return 2


if __name__ == "__main__":
    sys.exit(main())
