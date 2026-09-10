#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Unit tests for the screening's per-point verification.

`verify_row()` is what stops the screening trusting its own command line. It
has no coverage from a real run, because a real run never produces a row that
disagrees with what was asked for — which is exactly why the check would rot
unnoticed. These drive it with rows constructed to disagree in one field each.
"""

from __future__ import annotations

import argparse
import copy
import importlib.util
import pathlib
import sys

sys.dont_write_bytecode = True

TOOL = pathlib.Path(__file__).resolve().parent.parent / "neo_core_screen.py"
spec = importlib.util.spec_from_file_location("neo_core_screen", TOOL)
screen = importlib.util.module_from_spec(spec)
sys.modules["neo_core_screen"] = screen
spec.loader.exec_module(screen)

FAILURES = []
WORKLOAD = ("relu", "relu", "rvv", 9)
MODE = "kernel"
CONFIG_ID = "screen_local_bank_count_4"
SETTINGS = dict(screen.BASELINE)
ARGS = argparse.Namespace(timing="annotated", seed=20260825,
                          build_type="Release")


def good_row():
    provenance = {name: expected
                  for _flag, (name, expected) in screen.READBACK.items()}
    return {
        "identity": {
            "benchmark": "relu", "case_index": 9, "implementation": "rvv",
            "mode": MODE, "seed": 20260825, "build_type": "Release",
        },
        "configuration": {
            "id": CONFIG_ID, "timing_mode": "annotated",
            "local_bank_width_bits": SETTINGS["--bank-width-bits"],
            "local_bank_count": SETTINGS["--bank-count"],
            "fabric_pipeline_stages": SETTINGS["--pipeline-stages"],
            "dma_max_burst_bytes": SETTINGS["--dma-max-burst"],
            "sram_capacity_bytes": SETTINGS["--sram-capacity-bytes"],
            "identity_provenance": provenance,
        },
    }


def check(condition, message):
    if not condition:
        FAILURES.append(message)


def verify(row):
    screen.verify_row(row, ARGS, WORKLOAD, MODE, SETTINGS, CONFIG_ID)


def rejects(mutate, what):
    row = good_row()
    mutate(row)
    try:
        verify(row)
    except SystemExit:
        return
    FAILURES.append(f"verify_row accepted a row whose {what} disagreed")


def a_matching_row_is_accepted():
    try:
        verify(good_row())
    except SystemExit as error:
        check(False, f"a correct row was rejected: {error}")


def every_requested_field_is_checked():
    rejects(lambda r: r["configuration"].__setitem__("id", "something_else"),
            "configuration id")
    rejects(lambda r: r["identity"].__setitem__("benchmark", "gemm"),
            "benchmark")
    rejects(lambda r: r["identity"].__setitem__("case_index", 0), "case index")
    rejects(lambda r: r["identity"].__setitem__("implementation", "mxu"),
            "implementation")
    rejects(lambda r: r["identity"].__setitem__("mode", "end_to_end"), "mode")
    rejects(lambda r: r["configuration"].__setitem__("timing_mode",
                                                     "arbitrated"),
            "timing mode")
    rejects(lambda r: r["identity"].__setitem__("seed", 1), "seed")
    rejects(lambda r: r["identity"].__setitem__("build_type", "Debug"),
            "build type")


def a_knob_the_runner_ignored_is_caught():
    # The failure this check exists for: the runner returns a valid row for the
    # baseline machine and the screening would otherwise record the requested
    # value beside it and report the knob inert.
    for flag, (name, _provenance) in screen.READBACK.items():
        rejects(lambda r, n=name: r["configuration"].__setitem__(n, 999999),
                f"knob {name}")


def provenance_must_be_the_expected_kind_not_merely_present():
    for flag, (name, expected) in screen.READBACK.items():
        wrong = "configured" if expected == "live_readback" else "live_readback"
        rejects(
            lambda r, n=name, w=wrong:
                r["configuration"]["identity_provenance"].__setitem__(n, w),
            f"provenance of {name}")
        rejects(
            lambda r, n=name:
                r["configuration"]["identity_provenance"].__setitem__(
                    n, "structural_literal"),
            f"provenance of {name} as a structural literal")
        rejects(
            lambda r, n=name:
                r["configuration"]["identity_provenance"].pop(n),
            f"missing provenance for {name}")


def the_live_knobs_are_the_ones_the_model_reads_back():
    # A regression that made a live knob `configured` would still be a valid
    # row; the table below is the contract that makes it a failure.
    expected = {
        "local_bank_width_bits": "live_readback",
        "local_bank_count": "live_readback",
        "fabric_pipeline_stages": "live_readback",
        "sram_capacity_bytes": "live_readback",
        "dma_max_burst_bytes": "configured",
    }
    actual = {name: kind for _flag, (name, kind) in screen.READBACK.items()}
    check(actual == expected,
          f"the screened-knob provenance contract drifted: {actual}")


def main() -> int:
    for test in (a_matching_row_is_accepted,
                 every_requested_field_is_checked,
                 a_knob_the_runner_ignored_is_caught,
                 provenance_must_be_the_expected_kind_not_merely_present,
                 the_live_knobs_are_the_ones_the_model_reads_back):
        test()
    if FAILURES:
        for failure in FAILURES:
            print(f"CHECK failed: {failure}", file=sys.stderr)
        return 1
    print("test_screening_verify: all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
