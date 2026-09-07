#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Render the G4 table from the screening summaries and check the audit's claims.

The audit says its table is generated from the summary the tool writes. This is
what makes that true. It exists because the reporting layer is where the last
G4 defect lived: the tool measured ten workload/mode combinations, a hand-run
script rendered seven of them, and the surrounding prose generalised from the
truncated view. Every check below is a claim the audit makes in words; if the
measurement stops supporting one, this fails rather than the words quietly
becoming wrong.

Two summaries are required — `annotated` and `arbitrated` — because several of
the claims are about the relationship between them.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys

sys.dont_write_bytecode = True

BENCHMARK_ORDER = ["relu", "vector_dot", "gemv_rvv", "gemv_mxu", "gemm"]
MODE_ORDER = ["kernel", "end_to_end"]
KNOB_ORDER = [
    "fabric_pipeline_stages",
    "local_bank_width_bits",
    "dma_max_burst_bytes",
    "local_bank_count",
    "sram_capacity_bytes",
]
SHORT = {"relu": "relu", "vector_dot": "vdot", "gemv_rvv": "gv_rvv",
         "gemv_mxu": "gv_mxu", "gemm": "gemm"}

# What the audit asserts in prose. Kept here so that prose and measurement fail
# together.
EXPECTED_POINTS = 170
EXPECTED_SENSITIVE = ["dma_max_burst_bytes", "fabric_pipeline_stages",
                      "local_bank_width_bits"]
EXPECTED_INERT = ["local_bank_count", "sram_capacity_bytes"]
EXPECTED_CONFLICT_ROWS = {"annotated": 32, "arbitrated": 0}


def spreads(summary):
    return {(f["knob"], f["benchmark"], f["mode"]): f
            for f in summary["findings"] if f["metric"] == "elapsed_ns"}


def render_table(summary) -> str:
    columns = [(b, m) for b in BENCHMARK_ORDER for m in MODE_ORDER]
    found = spreads(summary)
    header = " | ".join(
        f"{SHORT[b]}/{'ker' if m == 'kernel' else 'e2e'}" for b, m in columns)
    lines = [f"| knob | {header} |",
             "| --- | " + " | ".join("---:" for _ in columns) + " |"]
    for knob in KNOB_ORDER:
        cells = []
        for b, m in columns:
            finding = found.get((knob, b, m))
            spread = None if finding is None else finding["relative_spread"]
            cells.append("-" if spread is None else f"{spread:.1%}")
        lines.append(f"| `{knob}` | " + " | ".join(cells) + " |")
    return "\n".join(lines)


def count_conflict_rows(rows) -> int:
    return sum(1 for row in rows.values()
               if any(r["bank_conflicts"]
                      for r in row["local_fabric"]["requesters"]))


def load_rows(raw_dir: pathlib.Path) -> dict:
    """Every result row in a screening, keyed by the point that produced it.

    Keyed by (benchmark, mode, config id) read out of the row rather than by
    filename, so a renamed or duplicated file cannot make two different points
    look like the same one.
    """
    rows = {}
    for path in sorted(raw_dir.glob("*.json")):
        row = json.loads(path.read_text())
        key = (row["identity"]["benchmark"], row["identity"]["mode"],
               row["configuration"]["id"])
        if key in rows:
            raise SystemExit(f"two rows in {raw_dir} describe the point {key}")
        rows[key] = row
    return rows


def stage_shape(row):
    """A row's stage timing, as a comparable value."""
    return [(s["stage"], s["begin_ns"], s["duration_ns"])
            for s in row["stage_timing"]]


def compare_points(left, right):
    """Every difference between two screenings, point by point.

    Comparing the aggregated spreads is not enough and the gap is not
    theoretical: min and max survive a permutation of the points between them,
    so two screenings could differ at an intermediate point, or in where an
    interval was spent, and still agree on every summary figure this tool used
    to check.
    """
    problems = []
    if set(left) != set(right):
        only_left = sorted(set(left) - set(right))
        only_right = sorted(set(right) - set(left))
        problems.append(
            f"the two timing modes did not measure the same points; "
            f"annotated-only {only_left[:3]}, arbitrated-only {only_right[:3]}")
        return problems

    for key in sorted(left):
        a, b = left[key], right[key]
        a_elapsed = a["hart"]["elapsed_ns"]
        b_elapsed = b["hart"]["elapsed_ns"]
        if a_elapsed != b_elapsed:
            problems.append(
                f"{key}: elapsed_ns {a_elapsed} against {b_elapsed}")
        if stage_shape(a) != stage_shape(b):
            problems.append(
                f"{key}: stage timing differs\n"
                f"    annotated  {stage_shape(a)}\n"
                f"    arbitrated {stage_shape(b)}")
    return problems


MANIFEST_FIELDS = ["seed", "build_type", "source_revision",
                   "firmware_elf_sha256", "baseline", "workloads", "modes",
                   "knobs_screened", "sensitive_knobs", "insensitive_knobs"]


def compare_manifests(left, right):
    """The two screenings must describe the same experiment.

    Without this they could differ in seed, build type, firmware image or knob
    set and still be compared point for point as though they were one
    experiment run twice.
    """
    return [f"summary field {field!r} differs: {left[field]!r} against "
            f"{right[field]!r}"
            for field in MANIFEST_FIELDS if left[field] != right[field]]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--annotated", required=True,
                        help="the annotated screening output directory")
    parser.add_argument("--arbitrated", required=True)
    parser.add_argument("--audit", default=None,
                        help="if given, require this file to contain the table")
    parser.add_argument("--write-table", default=None)
    args = parser.parse_args()

    problems = []
    summaries = {}
    rows = {}
    for mode in ("annotated", "arbitrated"):
        root = pathlib.Path(getattr(args, mode))
        summary = json.loads(
            (root / "results" / "summary" / "screening.json").read_text())
        summaries[mode] = summary

        if summary["timing_mode"] != mode:
            problems.append(
                f"{mode}: summary reports timing_mode "
                f"{summary['timing_mode']!r}")
        if summary["points"] != EXPECTED_POINTS:
            problems.append(
                f"{mode}: {summary['points']} points, expected "
                f"{EXPECTED_POINTS}")
        if sorted(summary["sensitive_knobs"]) != EXPECTED_SENSITIVE:
            problems.append(
                f"{mode}: sensitive knobs {summary['sensitive_knobs']}, "
                f"expected {EXPECTED_SENSITIVE}")
        if sorted(summary["insensitive_knobs"]) != EXPECTED_INERT:
            problems.append(
                f"{mode}: inert knobs {summary['insensitive_knobs']}, "
                f"expected {EXPECTED_INERT}")

        rows[mode] = load_rows(root / "results" / "raw")
        if len(rows[mode]) != EXPECTED_POINTS:
            problems.append(
                f"{mode}: {len(rows[mode])} result rows on disk, expected "
                f"{EXPECTED_POINTS}")

        conflicts = count_conflict_rows(rows[mode])
        if conflicts != EXPECTED_CONFLICT_ROWS[mode]:
            problems.append(
                f"{mode}: {conflicts} rows with a bank conflict, expected "
                f"{EXPECTED_CONFLICT_ROWS[mode]}. The audit explains the "
                f"annotated count as a stage-boundary decoupling artefact and "
                f"the arbitrated count as its absence; a different number "
                f"means that explanation no longer describes the model")

    # Point by point, not summary against summary.
    problems += compare_manifests(summaries["annotated"],
                                  summaries["arbitrated"])
    problems += compare_points(rows["annotated"], rows["arbitrated"])

    table = render_table(summaries["annotated"])
    if args.write_table:
        pathlib.Path(args.write_table).write_text(table + "\n")

    if args.audit:
        audit = pathlib.Path(args.audit).read_text()
        if table not in audit:
            problems.append(
                f"the audit does not contain the generated table verbatim. "
                f"The table the measurement supports is:\n{table}")

    if problems:
        for problem in problems:
            print(f"G4 report check failed: {problem}", file=sys.stderr)
        return 1

    print(f"G4 report: {EXPECTED_POINTS} points per timing mode, sensitive "
          f"{EXPECTED_SENSITIVE}, inert {EXPECTED_INERT}, conflicts "
          f"{EXPECTED_CONFLICT_ROWS}, table matches the audit")
    return 0


if __name__ == "__main__":
    sys.exit(main())
