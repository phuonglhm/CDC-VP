#!/usr/bin/env python3
"""[S8-ME part A] Build the "CORE EVALUATION SUMMARY" (Section 0) from files sw_eval already produced. Reads only; runs nothing.

Reads, from <nine>: parameters.json, project_metrics.csv (network row), summary.csv (network row), coverage.csv.
Optionally, from the same directory as the source metrics CSV (--run-log / --detections, auto-detected if omitted):
a run.log RESULT/STATUS line (functional bit-exact check) and a detections.json (box count from a real SystemC run).

usage: python core_eval_summary.py --nine out/smoke/nine_csv [--csv path/to/metrics_tiles.csv] [--report out/smoke/smoke_table.md]
If --report is given, Section 0 is PREPENDED to that file (the file must already exist -- this script never creates the rest of the report).
Otherwise the section is printed to stdout and also written to <nine>/../core_eval_summary.md.
"""
import argparse
import csv
import json
import os
import re
import sys


def fnum(x):
    return "{:,}".format(int(round(x))).replace(",", " ")


def net_row(path):
    rows = list(csv.DictReader(open(path, encoding="utf-8")))
    return next(r for r in rows if r["layer_id"] == "network")


def find_sibling(csv_path, name):
    if not csv_path:
        return None
    p = os.path.join(os.path.dirname(csv_path), name)
    return p if os.path.isfile(p) else None


def functional_line(run_log):
    if not run_log:
        return None
    pat = re.compile(r"(RESULT: PASS \(steps \d+.*?\))|(STATUS\s*:\s*(PASS|FAIL)\s*\([^)]*\))|(\[RESULT\] TEST PASSED[^\n]*)")
    last = None
    for ln in open(run_log, errors="replace"):
        m = pat.search(ln)
        if m:
            last = m.group(0)
    return last


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--nine", required=True, help="directory holding the 9 CSVs + parameters.json (sw_eval output)")
    ap.add_argument("--csv", default=None, help="the source metrics_tiles.csv this run came from, if any (used to auto-find run.log/detections.json)")
    ap.add_argument("--run-log", default=None)
    ap.add_argument("--detections", default=None)
    ap.add_argument("--report", default=None, help="prepend Section 0 to this existing report file instead of writing a standalone one")
    a = ap.parse_args()

    P = json.load(open(os.path.join(a.nine, "parameters.json"), encoding="utf-8"))
    pm = net_row(os.path.join(a.nine, "project_metrics.csv"))
    sm = net_row(os.path.join(a.nine, "summary.csv"))
    cov_path = os.path.join(a.nine, "coverage.csv")
    cov = list(csv.DictReader(open(cov_path, encoding="utf-8"))) if os.path.isfile(cov_path) else []

    run_log = a.run_log or find_sibling(a.csv, "run.log")
    detections = a.detections or find_sibling(a.csv, "detections.json")

    freq_hz = float(P["frequency_hz"])
    pes = int(P["pe_count"])
    total_cycles = int(pm["core_busy_cycles"])  # network-level "core busy" == total_cycles for the network row
    exec_cycles = int(pm["exec_cycles"])
    total_macs = int(pm["total_macs"])
    pe_util = float(pm["pe_mac_utilization_pct"])
    exec_share = float(pm["array_exec_share_of_busy_pct"])
    ips = sm["ips"]
    ips_bw = sm["ips_bw_limited"]
    achieved_tops = float(sm["algorithmic_tops"])
    peak_tops = 2.0 * pes * freq_hz / 1e12
    latency_s = total_cycles / freq_hz if freq_hz else 0.0

    tc = P.get("tile_counts", {})
    frame = tc.get("program_frame")
    measured = tc.get("measured_core_port", 0)
    basis = tc.get("with_measurement_basis")
    cov_pct = 100.0 * measured / basis if basis else 0.0

    bottleneck = None
    if ips not in (None, "") and ips_bw not in (None, ""):
        ips_f, ips_bw_f = float(ips), float(ips_bw)
        if ips_bw_f > 0:
            ratio = ips_f / ips_bw_f
            if ratio < 0.5:
                bottleneck = ("**COMPUTE-BOUND**: achieved FPS (%.2f) is only %.0f%% of the DMA-bandwidth-limited ceiling (%.2f FPS) "
                              "-- the arithmetic path (pipeline stalls / low PE utilization), not DDR bandwidth, is what limits throughput." % (ips_f, 100 * ratio, ips_bw_f))
            elif ratio > 0.9:
                bottleneck = "**BANDWIDTH-BOUND**: achieved FPS (%.2f) is close to the DMA-bandwidth-limited ceiling (%.2f FPS)." % (ips_f, ips_bw_f)
            else:
                bottleneck = "**MIXED**: achieved FPS (%.2f) is %.0f%% of the bandwidth-limited ceiling (%.2f FPS); neither bound dominates cleanly." % (ips_f, 100 * ratio, ips_bw_f)
    if bottleneck is None:
        bottleneck = "not computable (`ips`/`ips_bw_limited` blank -- these are network-level-only fields, see summary.csv)"

    func = functional_line(run_log)
    n_det = None
    if detections and os.path.isfile(detections):
        try:
            dj = json.load(open(detections, encoding="utf-8"))
            n_det = dj.get("n_det")
            if n_det is None:
                for k in ("detections", "detections_letterbox640", "boxes"):
                    if k in dj:
                        n_det = len(dj[k])
                        break
        except Exception:  # noqa: BLE001
            n_det = None

    rows = []
    rows.append(("Latency per image @%.2f GHz (core-only)" % (freq_hz / 1e9),
                  "%s cycles = %.4f s" % (fnum(total_cycles), latency_s), "derived|CORE-ONLY|" + P.get("mode", "B")))
    rows.append(("Throughput (FPS, core-only)", "%s" % (ips if ips not in (None, "") else "n/a"), "derived|CORE-ONLY"))
    rows.append(("Achieved TOPS", "%.3f" % achieved_tops, "derived"))
    rows.append(("Peak TOPS (%d MAC x2 x%.2f GHz)" % (pes, freq_hz / 1e9), "%.4f" % peak_tops, "derived|STATIC-CONFIG"))
    rows.append(("Efficiency vs peak", "%.2f%%" % (100.0 * achieved_tops / peak_tops if peak_tops else 0), "derived"))
    rows.append(("PE utilization (Total MACs / (busy*%d))" % pes, "%.2f%%" % pe_util, "derived"))
    rows.append(("exec/busy (array actually computing vs. occupied)", "%.2f%%" % exec_share, "derived"))
    rows.append(("ips_bw_limited (DMA-bandwidth ceiling)", "%s" % (ips_bw if ips_bw not in (None, "") else "n/a"), "derived|UPPER-BOUND"))
    rows.append(("Bottleneck verdict", bottleneck, "derived"))
    rows.append(("Measurement coverage (% of tiles measured through the core port)",
                  "%.2f%% (%s / %s measured tiles; program frame %s, %s gated)" % (cov_pct, fnum(measured), fnum(basis or 0), fnum(frame or 0), fnum(tc.get("gated_no_pass_shape", 0))),
                  "measured" if measured else "MODE-B(0% measured)"))
    if func:
        rows.append(("Functional accuracy (bit-exact vs. golden)", func, "measured"))
    else:
        rows.append(("Functional accuracy (bit-exact vs. golden)", "not evaluated in this output (no accompanying run.log found)", "n/a"))
    rows.append(("Detected boxes (SystemC output, no IoU baseline computed here)",
                  "%d boxes" % n_det if n_det is not None else "not evaluated in this output (no detections.json found)",
                  "measured" if n_det is not None else "n/a"))
    rows.append(("Total algorithmic MACs", fnum(total_macs), "derived"))
    rows.append(("Tiles / measurement basis / gated", "%s / %s / %s" % (fnum(frame or 0), fnum(basis or 0), fnum(tc.get("gated_no_pass_shape", 0))), "derived"))
    l2 = int(P["l2_size_bytes"])
    l1 = int(P["l1_size_bytes"])
    rows.append(("SRAM configuration (L1 / L2)", "%s B / %s B (verified: L1=%s)" % (fnum(l1), fnum(l2), P.get("l1_size_verified")), "derived|see unverified_parameters"))
    ext_bytes_hint = None
    try:
        bw_row = net_row(os.path.join(a.nine, "bandwidth.csv"))
        ext_bytes_hint = int(bw_row["ddr_read_bytes"]) + int(bw_row["ddr_write_bytes"])
    except Exception:  # noqa: BLE001
        pass
    if ext_bytes_hint and total_macs:
        ai = total_macs / ext_bytes_hint
        machine_balance = peak_tops * 1e12 / 2 / (float(P["peak_external_bw_gbps"]) * 1e9)  # MAC/s / (B/s) = MAC/byte
        rows.append(("Arithmetic intensity (MACs / external byte)", "%.2f MAC/B (machine balance = %.2f MAC/B)" % (ai, machine_balance),
                     "derived|roofline: %s" % ("compute-bound region" if ai > machine_balance else "bandwidth-bound region")))

    lines = ["## 0. CORE EVALUATION SUMMARY\n",
             "One line, one provenance label each. \"Core-only\" = the 83 conv layers of the core; excludes OBP, OBP config load, host ops and inter-layer DMA.\n",
             "| Metric | Value | Label |\n|---|---|---|"]
    for name, val, label in rows:
        lines.append("| %s | %s | %s |" % (name, val, label))
    lines.append("")
    if ext_bytes_hint and total_macs and bottleneck.startswith("**COMPUTE-BOUND"):
        lines.append("*Note: the roofline row and the bottleneck verdict can look contradictory -- they answer different questions. "
                      "Arithmetic intensity says which wall the workload would hit at PEAK efficiency (its own MAC-to-byte ratio vs. the hardware's balance point). "
                      "The bottleneck verdict says which wall it is ACTUALLY hitting today (achieved FPS vs. the bandwidth-limited ceiling): here, achieved FPS is far below even that ceiling, "
                      "so today's limiter is compute-side inefficiency (stalls / low PE utilization), not the bandwidth wall the roofline predicts as the asymptotic ceiling.*\n")
    top = sorted([r for r in csv.DictReader(open(os.path.join(a.nine, "project_metrics.csv"), encoding="utf-8")) if r["layer_id"] != "network"],
                 key=lambda r: -int(r["core_busy_cycles"]))[:5]
    if top:
        lines.append("**Top-5 most expensive layers (core busy cycles):** " + "; ".join(
            "%s (%s, %.1f%% PE util)" % (r["layer_name"], fnum(int(r["core_busy_cycles"])), float(r["pe_mac_utilization_pct"])) for r in top) + "\n")
    section = "\n".join(lines) + "\n"

    if a.report:
        if not os.path.isfile(a.report):
            sys.exit("--report %s does not exist; run the rest of the pipeline first" % a.report)
        body = open(a.report, encoding="utf-8").read()
        title_end = body.find("\n\n")
        if title_end < 0:
            title_end = body.find("\n")
        new_body = body[:title_end + 1] + "\n" + section + body[title_end + 1:]
        open(a.report, "w", encoding="utf-8").write(new_body)
        print("ok: Section 0 prepended to", a.report)
    else:
        out = os.path.join(a.nine, "..", "core_eval_summary.md")
        open(out, "w", encoding="utf-8").write(section)
        print(section)
        print("ok:", out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
