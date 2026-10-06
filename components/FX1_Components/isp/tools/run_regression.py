#!/usr/bin/env python3
"""Real-RAW regression of the FX1 ISP model (plan task P13, gates G-E2E and G-CONT; DEC-34/35).

Two kinds of run, both compared frame by frame, bit for bit, with the Python
reference (NV12 of every frame, statistics through every readout mux after the
last frame):

  image     every RAW frame of the inventory x every profile preset, in its own
            VP session; presets with temporal state submit the frame twice and
            both outputs are compared.
  sequence  per preset, all selected RAW frames back to back in ONE VP session
            (inventory order, up to 4 frames in flight through the rotation,
            one profile applied once, computed from the first frame), so the
            temporal state (GTM curve, 2DNR variance, statistics IDs) carries
            from one real image to the next.

Each run:
  1. verifies the sources against the inventory SHA-256 and converts them to the
     ISP input contract (raw_fixture, explicit format);
  2. builds the preset's test profile (make_profile, not a calibration);
  3. computes the expected NV12 of every frame and the statistics readout with
     the reference;
  4. runs the C++ model end to end over TLM (fx1_isp_run_raw: CSR, IDMA,
     pipeline, ODMA, RAM);
  5. writes the plan §10 artefacts into <out>/<raw stem>/<preset>/ or
     <out>/sequence/<preset>/:
       run_manifest.json   input/profile/LUT checksums, geometry, versions
       output.nv12         the model's NV12 of the last frame (active bytes)
       output_preview.png  visual check only (not a pass/fail criterion)
       statistics.json     statistics as software read them over the CSR port
       events.log          frame lifecycle and interrupt events
       comparison.json     per-frame results against the reference, first mismatches
Summary: <out>/summary.json and <out>/summary.md. Exit status 0 iff every run
passes. The dataset is never copied into the source tree; <out> must be
outside it. "Pass" means agreement with the project's own reference (DEC-05),
not with an owner golden.

Baseline (acceptance evidence, docs/evidence/): --write-baseline FILE writes the
full SHA-256 of every frame's NV12 and of every statistics readout, without
host paths or timings, plus FILE with .md for reading. --baseline FILE fails the
run if any of them changed: an intended model change must regenerate it. With
--only (a subset), the runs the baseline has are compared and the subset's own
sequence run, absent from the baseline, is reported as not compared; use
--mode image to run single frames only.

Usage:
  run_regression.py --inventory ISP_RAW_INVENTORY.json --run-raw build/fx1_isp_run_raw --out DIR
                    [--root DIR] [--presets basic,full,...] [--only SUBSTR] [--mode image,sequence]
                    [--jobs N] [--no-preview] [--keep-intermediate]
                    [--baseline FILE] [--write-baseline FILE]
"""

import argparse
import concurrent.futures
import datetime
import hashlib
import json
import os
import platform
import subprocess
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(REPO, "reference"))
sys.path.insert(0, HERE)
from fx1_isp_ref import blocks, regs as regs_mod  # noqa: E402
from fx1_isp_ref.stats import readout_script  # noqa: E402
import make_profile  # noqa: E402
import raw_fixture  # noqa: E402

LUT_PORTS = {"gamma": "GAMMA_LUT_DATA", "ee": "EE_LUT_WDATA", "gtm": "GTM_LUT_DATA", "lsc": "LSC_COEF_DATA"}
SEQUENCE_QUEUE = 4


def sha256_bytes(b):
    return hashlib.sha256(b).hexdigest()


def align16(v):
    return (v + 15) & ~15


def git_state():
    try:
        rev = subprocess.run(["git", "-C", REPO, "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
        dirty = subprocess.run(["git", "-C", REPO, "status", "--porcelain", "--", "."],
                               capture_output=True, text=True).stdout.strip()
        return {"commit": rev or None, "dirty": bool(dirty)}
    except OSError:
        return {"commit": None, "dirty": None}


def parse_reads(lines):
    """Readout script lines -> list of {reg, value, selectors} for the reads."""
    sel, out = {}, []
    for ln in lines:
        p = ln.split()
        if not p:
            continue
        if p[0] == "SET":
            sel[p[1]] = int(p[2])
        else:
            out.append({"reg": p[0], "value": int(p[1]), "selectors": dict(sel)})
    return out


def compare_nv12(got, want, w, h):
    n = min(len(got), len(want))
    a, b = np.frombuffer(got[:n], np.uint8), np.frombuffer(want[:n], np.uint8)
    diff = np.nonzero(a != b)[0]
    rec = {"size_ok": len(got) == len(want), "mismatches": int(diff.size), "first_mismatch": None,
           "max_abs_diff": 0, "sha256_model": sha256_bytes(got), "sha256_reference": sha256_bytes(want)}
    if diff.size:
        i = int(diff[0])
        plane, off = ("Y", i) if i < w * h else ("UV", i - w * h)
        rec["first_mismatch"] = {"plane": plane, "x": off % w, "y": off // w, "got": int(a[i]), "want": int(b[i])}
        rec["max_abs_diff"] = int(np.abs(a.astype(int) - b.astype(int)).max())
    rec["pass"] = rec["size_ok"] and not diff.size
    return rec


def load_raw(entry, a, decl):
    src = os.path.join(a["root"], entry["path"])
    sha = raw_fixture.sha256_file(src)
    if sha != entry["sha256"]:
        raise ValueError("%s: SHA-256 %s does not match the inventory" % (src, sha))
    samples, _ = raw_fixture.load_source(src, a["format"], decl["width"], decl["height"])
    return src, sha, samples


def run_case(job):
    """One image run or one sequence run."""
    a, decl, preset = job["args"], job["declared"], job["preset"]
    w, h, bayer = decl["width"], decl["height"], decl["bayer"]
    entries = job["entries"]
    if job["kind"] == "image":
        name = os.path.splitext(os.path.basename(entries[0]["path"]))[0]
        d = os.path.join(a["out"], name, preset)
    else:
        name = "sequence of %d" % len(entries)
        d = os.path.join(a["out"], "sequence", preset)
    os.makedirs(d, exist_ok=True)
    res = {"kind": job["kind"], "raw": name, "preset": preset, "dir": d, "pass": False}
    t = time.time()
    try:
        loaded = [load_raw(e, a, decl) for e in entries]
    except ValueError as e:
        res["error"] = str(e)
        return res
    inputs = []
    for k, (_, _, samples) in enumerate(loaded):
        p = os.path.join(d, "input_%03d.isp16" % k)
        with open(p, "wb") as f:
            f.write(samples.astype("<u2").tobytes())
        inputs.append(p)
    # One profile, from the first frame (grey world).
    writes, note = make_profile.profile_writes(loaded[0][2].astype(np.int64), w, h, bayer, preset)
    prof = os.path.join(d, "profile.csrw")
    prof_text = "# test profile '%s' (not a calibration)\n# %s\n" % (preset, note) + \
        "".join("%s 0x%X\n" % (n, v) for n, v in writes)
    with open(prof, "w", encoding="utf-8") as f:
        f.write(prof_text)
    if job["kind"] == "image":
        order = [0] * make_profile.FRAMES[preset]          # the same frame, repeated
        queue = 1
    else:
        order = list(range(len(entries)))                  # every frame once, back to back
        queue = SEQUENCE_QUEUE

    # Reference: every frame in the order the model sees them.
    t_ref = time.time()
    r = regs_mod.Registers()
    r.write("COMMON_FRAME_WIDTH", w)
    r.write("COMMON_FRAME_HEIGHT", h)
    for n, v in writes:
        r.write(n, v)
    want = []
    for k in order:
        y, uv = blocks.run_frame(loaded[k][2].reshape(h, w), r, {})
        want.append(y.tobytes() + uv.tobytes())
    oh, ow = y.shape
    script = readout_script(r)
    exp_stats = os.path.join(d, "expected_stats.txt")
    with open(exp_stats, "w") as f:
        f.write("".join(script))
    t_ref = time.time() - t_ref

    # Model.
    lst = os.path.join(d, "inputs.txt")
    with open(lst, "w") as f:
        f.write("".join(inputs[k] + "\n" for k in order))
    prefix = os.path.join(d, "output")
    cmd = [a["run_raw"], "--input-list", lst, "--width", str(w), "--height", str(h), "--profile", prof,
           "--queue", str(queue), "--per-frame", "--stats-script", exp_stats, "--out", prefix]
    t_dut = time.time()
    p = subprocess.run(cmd, capture_output=True, text=True)
    t_dut = time.time() - t_dut
    status = {}
    if os.path.exists(prefix + ".txt"):
        for ln in open(prefix + ".txt"):
            k, v = ln.split()
            status[k] = v
    if os.path.exists(prefix + ".events.log"):
        os.replace(prefix + ".events.log", os.path.join(d, "events.log"))

    frames = []
    for k in range(len(order)):
        fn = "%s.f%03d.nv12" % (prefix, k)
        got = open(fn, "rb").read() if os.path.exists(fn) else b""
        rec = compare_nv12(got, want[k], ow, oh)
        rec.update({"frame": k, "input": entries[order[k]]["path"]})
        frames.append(rec)
    got_stats = open(prefix + ".stats.txt").read().splitlines() if os.path.exists(prefix + ".stats.txt") else []
    exp_lines = [ln.rstrip("\n") for ln in script]
    stat_bad = [{"expected": e, "got": g} for e, g in zip(exp_lines, got_stats) if e != g]
    stat_bad += [{"expected": e, "got": None} for e in exp_lines[len(got_stats):]]
    frames_ok = all(f["pass"] for f in frames)
    res.update({
        "pass": p.returncode == 0 and frames_ok and not stat_bad,
        "frames": len(order), "frames_pass": sum(f["pass"] for f in frames),
        "nv12_sha256": frames[-1]["sha256_model"], "out_w": ow, "out_h": oh, "queue": queue,
        "host_s_reference": round(t_ref, 2), "host_s_model": round(t_dut, 2),
        "simulated_ms": float(status.get("simulated_ns", "nan")) / 1e6, "dma_err": status.get("dma_err"),
        "frame_sha256": [f["sha256_model"] for f in frames],
        "statistics_sha256": sha256_bytes("\n".join(got_stats).encode()),
        "inputs": [entries[k]["path"] for k in order],
    })
    comparison = {
        "reference": "reference/fx1_isp_ref (project's own, DEC-05); not an owner golden",
        "kind": job["kind"], "frames": frames,
        "statistics": {"reads": sum(1 for e in exp_lines if not e.startswith("SET")),
                       "mismatches": len(stat_bad), "first_mismatches": stat_bad[:20],
                       "after_frame": len(order) - 1},
        "run_raw": {"exit": p.returncode, "stdout": p.stdout.strip().splitlines()[-4:],
                    "dma_err": status.get("dma_err"),
                    "dma_err_note": "0x2 = IDMA_UNDERRUN after the last buffer, expected (M3-A9)"},
        "pass": res["pass"],
    }
    json.dump(comparison, open(os.path.join(d, "comparison.json"), "w"), indent=1)
    json.dump(parse_reads(got_stats), open(os.path.join(d, "statistics.json"), "w"), indent=1)
    luts = {}
    for k, port in LUT_PORTS.items():
        vals = [v for n, v in writes if n == port]
        if vals:
            luts[k] = {"entries_written": len(vals), "sha256": sha256_bytes(np.array(vals, "<u4").tobytes())}
    manifest = {
        "date": datetime.datetime.now().isoformat(timespec="seconds"),
        "kind": job["kind"],
        "inputs": [{"path": e["path"], "sha256": sha, "container_sha256": sha256_bytes(s.astype("<u2").tobytes())}
                   for e, (_, sha, s) in zip(entries, loaded)],
        "frame_order": order, "queue": queue,
        "geometry": {"width": w, "height": h, "bayer": bayer, "adapter_format": a["format"]},
        "profile": {"preset": preset, "writes": len(writes), "from_input": entries[0]["path"],
                    "sha256": sha256_bytes(prof_text.encode()), "luts": luts, "note": note},
        "output": {"width": ow, "height": oh, "y_stride": align16(ow), "uv_stride": align16(ow),
                   "format": "NV12, active bytes, Y plane then interleaved UV",
                   "csc_std": int(status.get("csc_std", 0))},
        "versions": {"git": git_state(), "python": platform.python_version(), "numpy": np.__version__,
                     "run_raw_sha256": raw_fixture.sha256_file(a["run_raw"])},
    }
    json.dump(manifest, open(os.path.join(d, "run_manifest.json"), "w"), indent=1)
    if os.path.exists(prefix + ".nv12"):
        os.replace(prefix + ".nv12", os.path.join(d, "output.nv12"))
        if not a["no_preview"] and frames[-1]["size_ok"]:
            from PIL import Image
            from nv12_preview import nv12_to_rgb
            Image.fromarray(nv12_to_rgb(open(os.path.join(d, "output.nv12"), "rb").read(), ow, oh,
                                        manifest["output"]["csc_std"])).save(os.path.join(d, "output_preview.png"))
    if not a["keep_intermediate"]:
        for f in inputs + [lst, exp_stats, prefix + ".stats.txt", prefix + ".txt"] + \
                ["%s.f%03d.nv12" % (prefix, k) for k in range(len(order))]:
            if os.path.exists(f):
                os.remove(f)
    res["host_s_total"] = round(time.time() - t, 2)
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--inventory", required=True)
    ap.add_argument("--root", help="directory the inventory paths are relative to (default: inventory's parent's parent)")
    ap.add_argument("--run-raw", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--presets", default=",".join(make_profile.PRESETS))
    ap.add_argument("--mode", default="image,sequence")
    ap.add_argument("--format", default="le16_raw10_msb", choices=sorted(raw_fixture.FORMATS))
    ap.add_argument("--only", default="", help="run only RAW files whose path contains this text")
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--no-preview", action="store_true")
    ap.add_argument("--keep-intermediate", action="store_true")
    ap.add_argument("--baseline", help="fail if any frame or statistics SHA-256 differs from this baseline")
    ap.add_argument("--write-baseline", help="write the baseline JSON (and .md) of this run")
    a = ap.parse_args()
    inv = json.load(open(a.inventory))
    root = a.root or os.path.dirname(os.path.dirname(os.path.abspath(a.inventory)))
    out = os.path.abspath(a.out)
    if out == REPO or out.startswith(REPO + os.sep):
        sys.exit("--out must be outside the source tree (%s)" % REPO)
    presets = [p for p in a.presets.split(",") if p]
    modes = [m for m in a.mode.split(",") if m]
    for p in presets:
        if p not in make_profile.PRESETS:
            sys.exit("unknown preset %s" % p)
    for m in modes:
        if m not in ("image", "sequence"):
            sys.exit("unknown mode %s" % m)
    args = {"root": root, "out": out, "run_raw": os.path.abspath(a.run_raw), "format": a.format,
            "no_preview": a.no_preview, "keep_intermediate": a.keep_intermediate}
    selected = [e for e in inv["files"] if a.only in e["path"]]
    if not selected:
        sys.exit("no RAW file selected")
    base = {"args": args, "declared": inv["declared_by_user"]}
    jobs = []
    if "sequence" in modes:                                # longest first
        jobs += [dict(base, kind="sequence", entries=selected, preset=p) for p in presets]
    if "image" in modes:
        jobs += [dict(base, kind="image", entries=[e], preset=p) for e in selected for p in presets]
    os.makedirs(out, exist_ok=True)
    t0 = time.time()
    results = []
    with concurrent.futures.ProcessPoolExecutor(max_workers=a.jobs) as ex:
        for r in ex.map(run_case, jobs):
            results.append(r)
            print("%-4s %-8s %-40s %-15s %s" % (
                "PASS" if r["pass"] else "FAIL", r["kind"], r["raw"], r["preset"],
                r.get("error") or "%d/%d frames, %dx%d, last sha %s" % (
                    r["frames_pass"], r["frames"], r["out_w"], r["out_h"], r["nv12_sha256"][:16])), flush=True)
    ok = sum(r["pass"] for r in results)
    nframes = sum(r.get("frames", 0) for r in results)
    nframes_ok = sum(r.get("frames_pass", 0) for r in results)
    summary = {"date": datetime.datetime.now().isoformat(timespec="seconds"), "runs": len(results), "pass": ok,
               "frames_compared": nframes, "frames_pass": nframes_ok,
               "host_s_wall": round(time.time() - t0, 1), "git": git_state(), "results": results}
    json.dump(summary, open(os.path.join(out, "summary.json"), "w"), indent=1)
    with open(os.path.join(out, "summary.md"), "w") as f:
        f.write("# FX1 ISP real-RAW regression\n\n%d / %d runs pass; %d / %d frames bit-exact with the Python "
                "reference (project reference, DEC-05; not an owner golden).\n\n" % (ok, len(results), nframes_ok,
                                                                                    nframes))
        f.write("| kind | RAW | preset | result | output | frames | queue | last NV12 SHA-256 | model host s | "
                "reference host s | simulated ms |\n|---|---|---|---|---|---|---|---|---|---|---|\n")
        for r in results:
            f.write("| %s | %s | %s | %s | %sx%s | %s/%s | %s | `%s` | %s | %s | %.2f |\n" % (
                r["kind"], r["raw"], r["preset"], "PASS" if r["pass"] else "FAIL " + r.get("error", ""),
                r.get("out_w"), r.get("out_h"), r.get("frames_pass"), r.get("frames"), r.get("queue"),
                r.get("nv12_sha256", "")[:16], r.get("host_s_model"), r.get("host_s_reference"),
                r.get("simulated_ms", float("nan"))))
    print("%d / %d runs pass, %d / %d frames, %.0f s; summary in %s" % (ok, len(results), nframes_ok, nframes,
                                                                       time.time() - t0, out))
    key = lambda r: "%s|%s|%s" % (r["kind"], r["raw"], r["preset"])
    runs = {key(r): {"kind": r["kind"], "raw": r["raw"], "preset": r["preset"], "output": [r.get("out_w"), r.get("out_h")],
                     "inputs": r.get("inputs"), "frame_sha256": r.get("frame_sha256"),
                     "statistics_sha256": r.get("statistics_sha256"), "pass": r["pass"]} for r in results}
    if a.write_baseline:
        base = {"description": "FX1 ISP real-RAW regression baseline: SHA-256 of every frame's NV12 (active bytes) "
                               "and of the statistics readout after the last frame, model and Python reference "
                               "bit-exact (DEC-05: project reference, not an owner golden)",
                "date": summary["date"], "git": git_state(), "inventory": os.path.basename(a.inventory),
                "presets": presets, "modes": modes, "runs": [runs[k] for k in sorted(runs)]}
        json.dump(base, open(a.write_baseline, "w"), indent=1)
        md = os.path.splitext(a.write_baseline)[0] + ".md"
        with open(md, "w") as f:
            f.write("# FX1 ISP real-RAW regression baseline\n\n%s.\n\nGenerated %s from git %s%s by "
                    "`tools/run_regression.py --write-baseline`. %d runs, %d frames; all bit-exact with the "
                    "reference.\n\n" % (base["description"], base["date"], (base["git"]["commit"] or "?")[:12],
                                         " (dirty tree)" if base["git"]["dirty"] else "", len(runs), nframes))
            f.write("| kind | RAW | preset | output | frame | NV12 SHA-256 |\n|---|---|---|---|---|---|\n")
            for k in sorted(runs):
                r = runs[k]
                for i, h in enumerate(r["frame_sha256"] or []):
                    f.write("| %s | %s | %s | %sx%s | %d | `%s` |\n" % (r["kind"], r["raw"], r["preset"], r["output"][0],
                                                                     r["output"][1], i, h))
            f.write("\nStatistics readout SHA-256 (after the last frame):\n\n| kind | RAW | preset | SHA-256 |\n"
                    "|---|---|---|---|\n")
            for k in sorted(runs):
                r = runs[k]
                f.write("| %s | %s | %s | `%s` |\n" % (r["kind"], r["raw"], r["preset"], r["statistics_sha256"]))
        print("baseline written: %s, %s" % (a.write_baseline, md))
    drift, skipped = [], []
    if a.baseline:
        # A full run must match the baseline exactly. A subset run (--only) is
        # compared on the runs the baseline has; its own sequence ("sequence
        # of N" for a subset) is not in the 28-frame baseline and is reported
        # as not compared (it is still checked against the reference).
        base = {key(r): r for r in json.load(open(a.baseline))["runs"]}
        subset = a.only != ""
        for k, r in runs.items():
            b = base.get(k)
            if b is None:
                (skipped if subset else drift).append("%s: not in the baseline" % k)
            elif b["frame_sha256"] != r["frame_sha256"] or b["statistics_sha256"] != r["statistics_sha256"]:
                drift.append("%s: output or statistics differ from the baseline" % k)
        if not subset:
            for k in base:
                if k not in runs and k.split("|")[2] in presets and k.split("|")[0] in modes:
                    drift.append("%s: baseline run not executed" % k)
        compared = len(runs) - len(skipped)
        print("baseline %s: %d run(s) compared, %s%s" % (
            a.baseline, compared, "%d difference(s)" % len(drift) if drift else "all identical",
            ", %d not in the baseline (subset, not compared)" % len(skipped) if skipped else ""))
        for d_ in drift[:20] + skipped[:8]:
            print("  " + d_)
    sys.exit(0 if ok == len(results) and not drift else 1)


if __name__ == "__main__":
    main()
