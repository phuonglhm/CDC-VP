#!/usr/bin/env python3
"""
metrics_summary.py — aggregate per-boundary latency CSVs into a single
summary table.

Reads every ``*.csv`` in the given directory (default: output/metrics).
For each CSV computes count, mean, min, max, stddev, p50, p95, p99, and
throughput (tokens / second). Writes a combined summary to
``output/metrics/summary.csv`` and prints a table to stdout.

Only the Python standard library is used (csv, statistics, glob,
argparse, pathlib).

Usage:
    python3 metrics_summary.py [DIRECTORY]
"""

from __future__ import annotations

import argparse
import csv
import glob
import math
import os
import statistics
import sys
from pathlib import Path
from typing import List, Optional


def percentile(sorted_values: List[float], pct: float) -> float:
    """Linear-interpolation percentile (0..100)."""
    if not sorted_values:
        return 0.0
    if pct <= 0:
        return sorted_values[0]
    if pct >= 100:
        return sorted_values[-1]
    k = (len(sorted_values) - 1) * (pct / 100.0)
    lo = int(math.floor(k))
    hi = int(math.ceil(k))
    if lo == hi:
        return sorted_values[lo]
    return sorted_values[lo] + (sorted_values[hi] - sorted_values[lo]) * (k - lo)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Aggregate per-boundary latency CSVs into a summary table."
    )
    parser.add_argument(
        "directory",
        nargs="?",
        default="output/metrics",
        help="Directory containing per-boundary *.csv files "
             "(default: output/metrics).",
    )
    return parser.parse_args()


def summarise_csv(csv_path: Path) -> Optional[dict]:
    """Read one CSV file and return a summary-dict or None if empty/invalid."""
    latencies_ns: List[float] = []
    t_in_ns: List[float] = []
    t_out_ns: List[float] = []
    with csv_path.open("r", newline="") as f:
        reader = csv.reader(f)
        try:
            header = next(reader)
        except StopIteration:
            return None
        # Expected header: token_id, t_in_ns, t_out_ns, latency_ns
        # Detect columns defensively.
        col_lat = -1
        col_tin = -1
        col_tout = -1
        for idx, name in enumerate(header):
            n = name.strip().lower()
            if n == "latency_ns":
                col_lat = idx
            elif n == "t_in_ns":
                col_tin = idx
            elif n == "t_out_ns":
                col_tout = idx
        if col_lat < 0 or col_tin < 0 or col_tout < 0:
            print(f"[warn] {csv_path.name}: header missing expected columns; "
                  f"got {header}", file=sys.stderr)
            return None
        for row in reader:
            if len(row) <= max(col_lat, col_tin, col_tout):
                continue
            try:
                tin = float(row[col_tin])
                tout = float(row[col_tout])
                lat = float(row[col_lat])
            except ValueError:
                continue
            t_in_ns.append(tin)
            t_out_ns.append(tout)
            latencies_ns.append(lat)

    n = len(latencies_ns)
    if n == 0:
        return {
            "boundary": csv_path.stem,
            "count": 0,
            "mean_latency_ns": 0.0,
            "min_latency_ns": 0.0,
            "max_latency_ns": 0.0,
            "stddev_latency_ns": 0.0,
            "p50_latency_ns": 0.0,
            "p95_latency_ns": 0.0,
            "p99_latency_ns": 0.0,
            "t_in_first_ns": 0.0,
            "t_out_last_ns": 0.0,
            "throughput_tokens_s": 0.0,
        }

    latencies_sorted = sorted(latencies_ns)
    t_in_first = t_in_ns[0]
    t_out_last  = t_out_ns[-1]
    span_ns = max(t_out_last - t_in_first, 0.0)
    if span_ns > 0:
        throughput_tokens_per_ns = n / span_ns
        throughput_tokens_per_s = throughput_tokens_per_ns * 1e9
    else:
        throughput_tokens_per_s = float("inf")

    mean_lat = statistics.fmean(latencies_ns)
    try:
        stddev_lat = statistics.pstdev(latencies_ns)
    except statistics.StatisticsError:
        stddev_lat = 0.0

    return {
        "boundary": csv_path.stem,
        "count": n,
        "mean_latency_ns": mean_lat,
        "min_latency_ns": min(latencies_ns),
        "max_latency_ns": max(latencies_ns),
        "stddev_latency_ns": stddev_lat,
        "p50_latency_ns": percentile(latencies_sorted, 50.0),
        "p95_latency_ns": percentile(latencies_sorted, 95.0),
        "p99_latency_ns": percentile(latencies_sorted, 99.0),
        "t_in_first_ns": t_in_first,
        "t_out_last_ns": t_out_last,
        "throughput_tokens_s": throughput_tokens_per_s,
    }


def print_table(rows: List[dict]) -> None:
    cols = [
        ("boundary",            "boundary"),
        ("count",               "n"),
        ("mean_latency_ns",     "mean_ns"),
        ("min_latency_ns",      "min_ns"),
        ("max_latency_ns",      "max_ns"),
        ("stddev_latency_ns",   "stddev_ns"),
        ("p50_latency_ns",      "p50_ns"),
        ("p95_latency_ns",      "p95_ns"),
        ("p99_latency_ns",      "p99_ns"),
        ("throughput_tokens_s", "tok/s"),
    ]
    formatted = []
    for r in rows:
        out = []
        for key, _ in cols:
            v = r.get(key, 0)
            if v is None:
                out.append("-")
            elif isinstance(v, float) and math.isinf(v):
                out.append("inf")
            elif isinstance(v, float):
                out.append(f"{v:.3f}")
            else:
                out.append(str(v))
        formatted.append(out)

    # Compute column widths from header + rows.
    widths = [len(cols[i][1]) for i in range(len(cols))]
    for row in formatted:
        for i, val in enumerate(row):
            widths[i] = max(widths[i], len(val))

    def fmt_row(values):
        return " | ".join(values[i].ljust(widths[i]) for i in range(len(widths)))

    sep = "-+-".join("-" * w for w in widths)
    print(fmt_row([cols[i][1] for i in range(len(cols))]))
    print(sep)
    for row in formatted:
        print(fmt_row(row))


def write_csv(rows: List[dict], path: Path) -> None:
    fieldnames = [
        "boundary", "count",
        "mean_latency_ns", "min_latency_ns", "max_latency_ns", "stddev_latency_ns",
        "p50_latency_ns", "p95_latency_ns", "p99_latency_ns",
        "t_in_first_ns", "t_out_last_ns", "throughput_tokens_s",
    ]
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for r in rows:
            row_out = {}
            for k in fieldnames:
                v = r.get(k, "")
                if isinstance(v, float):
                    if math.isinf(v):
                        row_out[k] = "inf"
                    else:
                        row_out[k] = f"{v:.9f}"
                else:
                    row_out[k] = v
            writer.writerow(row_out)


def main() -> int:
    args = parse_args()
    base = Path(args.directory)
    if not base.is_dir():
        print(f"[error] directory not found: {base}", file=sys.stderr)
        return 1

    csvs = sorted(base.glob("*.csv"))
    # Exclude our own summary.csv from being re-read.
    csvs = [p for p in csvs if p.name != "summary.csv"]

    if not csvs:
        print(f"[warn] no *.csv files found in {base}", file=sys.stderr)
        return 0

    rows: List[dict] = []
    for p in csvs:
        row = summarise_csv(p)
        if row is not None:
            rows.append(row)

    if not rows:
        print("[error] no usable CSVs parsed.", file=sys.stderr)
        return 1

    # Stable order: alphabetical by boundary.
    rows.sort(key=lambda r: r["boundary"])

    print(f"=== Metrics summary (from {len(rows)} boundaries in {base}) ===")
    print_table(rows)

    out = base / "summary.csv"
    write_csv(rows, out)
    print(f"\nWrote {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())