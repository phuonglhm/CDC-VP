#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0

"""Directed winner-selection controls for noc_sweep.py."""

from __future__ import annotations

import importlib.util
import pathlib
import sys

sys.dont_write_bytecode = True


def load(path: pathlib.Path):
    spec = importlib.util.spec_from_file_location("noc_sweep_tested", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def row(identifier, p99, bandwidth, utilisation, fairness,
        *, status="measured", feasible=True, drained=True):
    return {
        "id": identifier,
        "status": status,
        "feasible": feasible,
        "drained": drained,
        "p99_latency_cycles": p99,
        "payload_bandwidth_gb_s": bandwidth,
        "peak_link_utilisation_percent": utilisation,
        "jain_fairness": fairness,
    }


def main() -> int:
    sweep = load(pathlib.Path(sys.argv[1]).resolve())
    valid = row("valid", 20, 2.0, 40, 0.9)
    failed_but_better = row(
        "failed", 1, 100.0, 1, 1.0,
        status="failed"
    )
    undrained_but_better = row(
        "undrained", 2, 90.0, 2, 1.0, drained=False
    )
    rejected_but_better = row(
        "constraint-reject", 3, 80.0, 3, 1.0, feasible=False
    )
    rows = [failed_but_better, undrained_but_better,
            rejected_but_better, valid]
    for objective in sweep.OBJECTIVES:
        selected = sweep.select_winner(rows, objective)
        if selected is None or selected["id"] != "valid":
            print(f"{objective} selected failed/incomplete row", file=sys.stderr)
            return 1

    choices = [
        row("latency", 8, 2.0, 50, 0.8),
        row("bandwidth", 20, 5.0, 60, 0.7),
        row("util", 16, 1.0, 20, 0.6),
        row("fair", 12, 3.0, 40, 1.0),
    ]
    expected = {
        "min-p99": "latency",
        "max-bandwidth": "bandwidth",
        "min-peak-util": "util",
        "max-fairness": "fair",
    }
    for objective, identifier in expected.items():
        selected = sweep.select_winner(choices, objective)
        if selected is None or selected["id"] != identifier:
            print(f"{objective} selected the wrong directed row",
                  file=sys.stderr)
            return 1

    print("PASS: FlooNoC DSE winner selection controls")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
