#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Unit tests for the G4 comparator.

The comparator is the thing that keeps the audit's prose tied to the
measurement, so its own failure modes matter. Two of the cases below are ones
the previous version could not see at all: it compared aggregated min/max per
knob, and min and max survive both a permutation of the points between them and
any change to how an interval was spent.
"""

from __future__ import annotations

import copy
import importlib.util
import pathlib
import sys

sys.dont_write_bytecode = True

TOOL = (pathlib.Path(__file__).resolve().parent.parent
        / "neo_core_screen_report.py")
spec = importlib.util.spec_from_file_location("neo_core_screen_report", TOOL)
report = importlib.util.module_from_spec(spec)
sys.modules["neo_core_screen_report"] = report
spec.loader.exec_module(report)

FAILURES = []


def check(condition, message):
    if not condition:
        FAILURES.append(message)


def row(elapsed=1000.0, stages=None, conflicts=0):
    return {
        "hart": {"elapsed_ns": elapsed},
        "stage_timing": stages if stages is not None else [
            {"stage": "the kernel", "begin_ns": 10.0, "duration_ns": 990.0},
        ],
        "local_fabric": {"requesters": [
            {"requester": "cpu", "bank_conflicts": conflicts},
        ]},
    }


def pair():
    left = {("relu", "kernel", "screen_a_1"): row(1000.0),
            ("relu", "kernel", "screen_a_2"): row(2000.0)}
    return left, copy.deepcopy(left)


def identical_screenings_compare_clean():
    left, right = pair()
    check(report.compare_points(left, right) == [],
          "two identical screenings were reported as differing")


def a_differing_intermediate_point_is_caught():
    # The case aggregated min/max cannot see: swap the interior value while
    # the extremes stay put.
    left = {("relu", "kernel", f"screen_a_{i}"): row(float(v))
            for i, v in enumerate([100, 200, 300])}
    right = copy.deepcopy(left)
    right[("relu", "kernel", "screen_a_1")]["hart"]["elapsed_ns"] = 250.0
    problems = report.compare_points(left, right)
    check(any("elapsed_ns" in p for p in problems),
          "an intermediate point differing in elapsed_ns was not reported")


def a_differing_stage_duration_is_caught():
    # Same total interval, spent differently. The audit claims stage timing
    # agrees between the modes, so this must be a failure and not a note.
    left, right = pair()
    key = ("relu", "kernel", "screen_a_1")
    right[key]["stage_timing"][0]["duration_ns"] += 1
    problems = report.compare_points(left, right)
    check(any("stage timing differs" in p for p in problems),
          "a stage duration difference was not reported")


def a_reordered_stage_list_is_caught():
    left, right = pair()
    key = ("relu", "kernel", "screen_a_1")
    left[key]["stage_timing"] = [
        {"stage": "DMA-in", "begin_ns": 10.0, "duration_ns": 100.0},
        {"stage": "the kernel", "begin_ns": 110.0, "duration_ns": 890.0}]
    right[key]["stage_timing"] = list(reversed(left[key]["stage_timing"]))
    check(report.compare_points(left, right) != [],
          "a reordered stage list compared equal")


def a_missing_or_extra_point_is_caught():
    left, right = pair()
    right.pop(("relu", "kernel", "screen_a_2"))
    problems = report.compare_points(left, right)
    check(any("same points" in p for p in problems),
          "a missing point was not reported")

    left, right = pair()
    right[("relu", "kernel", "screen_a_3")] = row(3000.0)
    check(report.compare_points(left, right) != [],
          "an extra point was not reported")


def manifests_must_describe_the_same_experiment():
    base = {field: "same" for field in report.MANIFEST_FIELDS}
    check(report.compare_manifests(base, dict(base)) == [],
          "two identical manifests were reported as differing")
    for field in report.MANIFEST_FIELDS:
        other = dict(base)
        other[field] = "different"
        problems = report.compare_manifests(base, other)
        check(any(field in p for p in problems),
              f"a differing {field} was not reported")


def the_conflict_count_reads_the_rows():
    rows = {("a",): row(conflicts=0), ("b",): row(conflicts=1),
            ("c",): row(conflicts=3)}
    count = report.count_conflict_rows(rows)
    check(count == 2,
          f"expected 2 rows with a conflict, counted {count}; the figure is "
          f"rows, not conflicts")


def the_table_renders_every_workload_and_mode():
    # The defect this whole comparator exists for: a table that renders fewer
    # columns than the tool measured.
    findings = []
    for benchmark in report.BENCHMARK_ORDER:
        for mode in report.MODE_ORDER:
            for knob in report.KNOB_ORDER:
                findings.append({
                    "knob": knob, "benchmark": benchmark, "mode": mode,
                    "metric": "elapsed_ns", "relative_spread": 0.123,
                })
    table = report.render_table({"findings": findings})
    header = table.splitlines()[0]
    check(header.count("|") == len(report.BENCHMARK_ORDER)
          * len(report.MODE_ORDER) + 2,
          f"the table header does not carry every workload/mode: {header}")
    for line in table.splitlines()[2:]:
        check(line.count("12.3%") == len(report.BENCHMARK_ORDER)
              * len(report.MODE_ORDER),
              f"a knob row lost columns: {line}")


def main() -> int:
    for test in (identical_screenings_compare_clean,
                 a_differing_intermediate_point_is_caught,
                 a_differing_stage_duration_is_caught,
                 a_reordered_stage_list_is_caught,
                 a_missing_or_extra_point_is_caught,
                 manifests_must_describe_the_same_experiment,
                 the_conflict_count_reads_the_rows,
                 the_table_renders_every_workload_and_mode):
        test()
    if FAILURES:
        for failure in FAILURES:
            print(f"CHECK failed: {failure}", file=sys.stderr)
        return 1
    print("test_screening_report: all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
