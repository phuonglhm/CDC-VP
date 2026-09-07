#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Unit tests for the screening aggregation.

The screening's conclusions come out of `summarise()`, and a simulation-driven
test cannot reach the cases that make it wrong: a knob whose baseline point is
missing, a metric absent on some points, a baseline of zero. Those are cheap
here and unreachable there.
"""

from __future__ import annotations

import importlib.util
import pathlib
import sys

TOOL = pathlib.Path(__file__).resolve().parent.parent / "neo_core_screen.py"
spec = importlib.util.spec_from_file_location("neo_core_screen", TOOL)
screen = importlib.util.module_from_spec(spec)
sys.modules["neo_core_screen"] = screen
spec.loader.exec_module(screen)

FAILURES = []


def check(condition, message):
    if not condition:
        FAILURES.append(message)


def point(knob, value, elapsed, *, baseline=False, benchmark="relu",
          mode="kernel", **extra):
    row = {
        "knob": knob, "value": value, "is_baseline": baseline,
        "benchmark": benchmark, "mode": mode,
    }
    row.update({metric: None for metric in screen.METRICS})
    row["elapsed_ns"] = elapsed
    row.update(extra)
    return row


WORKLOAD = [("relu", "relu", "rvv", 9)]


def spread_of(rows, knob="k", metric="elapsed_ns"):
    findings = [f for f in screen.summarise(rows, [knob], WORKLOAD)
                if f["metric"] == metric]
    return findings[0] if findings else None


def a_moving_metric_is_sensitive_and_scaled_to_its_baseline():
    rows = [point("k", 1, 100.0, baseline=True), point("k", 2, 150.0)]
    finding = spread_of(rows)
    check(finding is not None, "a two-point knob produced no finding")
    check(finding["sensitive"] is True, "a moving metric was called insensitive")
    check(abs(finding["relative_spread"] - 0.5) < 1e-9,
          f"spread should be 0.5, got {finding['relative_spread']}")
    check(finding["min"] == 100.0 and finding["max"] == 150.0,
          "the range does not bracket the observed values")


def a_flat_metric_is_insensitive_with_zero_spread():
    rows = [point("k", 1, 100.0, baseline=True), point("k", 2, 100.0)]
    finding = spread_of(rows)
    check(finding["sensitive"] is False, "a flat metric was called sensitive")
    check(finding["relative_spread"] == 0.0,
          "a flat metric has a non-zero spread")


def one_point_yields_no_finding():
    # A single value cannot show a spread, and reporting one as insensitive
    # would claim evidence the run does not contain.
    check(spread_of([point("k", 1, 100.0, baseline=True)]) is None,
          "a single point produced a sensitivity finding")


def a_missing_baseline_yields_no_finding():
    # Without the baseline there is nothing to be relative to. Silently using
    # the first point instead would make the number depend on iteration order.
    rows = [point("k", 2, 150.0), point("k", 4, 200.0)]
    check(spread_of(rows) is None,
          "a knob with no baseline point produced a relative spread")


def a_zero_baseline_reports_no_relative_spread():
    rows = [point("k", 1, 0.0, baseline=True), point("k", 2, 5.0)]
    finding = spread_of(rows)
    check(finding is not None, "a zero baseline dropped the finding entirely")
    check(finding["relative_spread"] is None,
          "a zero baseline produced a relative spread rather than None")
    check(finding["sensitive"] is True,
          "a zero baseline hid a metric that did move")


def absent_metrics_are_skipped_not_counted_as_zero():
    # `mxu_source_compute_cycles` is None on every RVV run. Treating None as
    # zero would invent a spread of 100% for every hart-driven workload.
    rows = [point("k", 1, 100.0, baseline=True), point("k", 2, 100.0)]
    findings = screen.summarise(rows, ["k"], WORKLOAD)
    names = {f["metric"] for f in findings}
    check("mxu_source_compute_cycles" not in names,
          "a metric absent from every point produced a finding")
    check("elapsed_ns" in names, "the present metric lost its finding")


def a_partly_absent_metric_uses_only_the_points_that_have_it():
    rows = [point("k", 1, 100.0, baseline=True, dma_chunks=10),
            point("k", 2, 100.0, dma_chunks=None),
            point("k", 4, 100.0, dma_chunks=40)]
    finding = spread_of(rows, metric="dma_chunks")
    check(finding is not None, "a partly present metric produced no finding")
    check(finding["min"] == 10 and finding["max"] == 40,
          "a None value entered the range")


def modes_and_benchmarks_do_not_leak_into_each_other():
    rows = [point("k", 1, 100.0, baseline=True),
            point("k", 2, 100.0),
            point("k", 1, 10.0, baseline=True, mode="end_to_end"),
            point("k", 2, 900.0, mode="end_to_end")]
    findings = [f for f in screen.summarise(rows, ["k"], WORKLOAD)
                if f["metric"] == "elapsed_ns"]
    by_mode = {f["mode"]: f for f in findings}
    check(set(by_mode) == {"kernel", "end_to_end"},
          f"expected one finding per mode, got {sorted(by_mode)}")
    check(by_mode["kernel"]["sensitive"] is False,
          "the kernel-mode points were contaminated by end-to-end ones")
    check(by_mode["end_to_end"]["sensitive"] is True,
          "the end-to-end points lost their spread")


def main() -> int:
    for test in (a_moving_metric_is_sensitive_and_scaled_to_its_baseline,
                 a_flat_metric_is_insensitive_with_zero_spread,
                 one_point_yields_no_finding,
                 a_missing_baseline_yields_no_finding,
                 a_zero_baseline_reports_no_relative_spread,
                 absent_metrics_are_skipped_not_counted_as_zero,
                 a_partly_absent_metric_uses_only_the_points_that_have_it,
                 modes_and_benchmarks_do_not_leak_into_each_other):
        test()
    if FAILURES:
        for failure in FAILURES:
            print(f"CHECK failed: {failure}", file=sys.stderr)
        return 1
    print("test_screening_summary: all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
