#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Run a bounded FlooNoC design-space sweep and select a feasible row."""

from __future__ import annotations

import argparse
import csv
import hashlib
import importlib.util
import itertools
import json
import math
import os
import pathlib
import subprocess
import sys
import tempfile
from datetime import datetime, timezone
from typing import Any, Callable

# Loading the dashboard contract must not dirty the source tree during a sweep
# or CTest run.
sys.dont_write_bytecode = True


SWEEP_SCHEMA = "floo-noc-sweep-v1"
OBJECTIVES = (
    "min-p99",
    "max-bandwidth",
    "min-peak-util",
    "max-fairness",
)


class SweepError(RuntimeError):
    pass


def positive_csv(text: str, option: str, maximum: int) -> list[int]:
    values: list[int] = []
    for item in text.split(","):
        try:
            value = int(item)
        except ValueError as error:
            raise argparse.ArgumentTypeError(
                f"{option} must be comma-separated integers"
            ) from error
        if value <= 0 or value > maximum:
            raise argparse.ArgumentTypeError(
                f"{option} values must be in 1..{maximum}"
            )
        values.append(value)
    if not values:
        raise argparse.ArgumentTypeError(f"{option} cannot be empty")
    return values


def nonnegative_csv(text: str, option: str, maximum: int) -> list[int]:
    values: list[int] = []
    for item in text.split(","):
        try:
            value = int(item)
        except ValueError as error:
            raise argparse.ArgumentTypeError(
                f"{option} must be comma-separated integers"
            ) from error
        if value < 0 or value > maximum:
            raise argparse.ArgumentTypeError(
                f"{option} values must be in 0..{maximum}"
            )
        values.append(value)
    if not values:
        raise argparse.ArgumentTypeError(f"{option} cannot be empty")
    return values


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="FlooNoC detailed-SystemC design-space sweep"
    )
    parser.add_argument("--benchmark", type=pathlib.Path, required=True)
    parser.add_argument("--output-dir", type=pathlib.Path, required=True)
    parser.add_argument(
        "--dashboard-tool",
        type=pathlib.Path,
        default=pathlib.Path(__file__).with_name("noc_dashboard.py"),
    )
    parser.add_argument(
        "--topologies", default="2x2,3x3,4x4",
        help="comma-separated verified topology shapes",
    )
    parser.add_argument(
        "--workers", default="1,4,8",
        help="comma-separated concurrent workers per manager",
    )
    parser.add_argument(
        "--gaps", default="0",
        help="comma-separated post-completion injection gaps in cycles",
    )
    parser.add_argument(
        "--workload",
        choices=("quiet", "hotspot", "contention", "fairness"),
        default="fairness",
    )
    parser.add_argument("--transactions", type=int, default=64)
    parser.add_argument("--warmup-transactions", type=int, default=2)
    parser.add_argument("--read-percent", type=int, default=50)
    parser.add_argument(
        "--burst-bytes", type=int, choices=(4, 8, 16, 32, 64), default=8
    )
    parser.add_argument("--seed", type=int, default=17)
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--max-p99", type=float)
    parser.add_argument("--min-bandwidth", type=float, default=0.0)
    parser.add_argument("--max-link-util", type=float, default=100.0)
    parser.add_argument("--objective", choices=OBJECTIVES, default="min-p99")
    args = parser.parse_args(argv)

    args.topologies = [item for item in args.topologies.split(",") if item]
    supported = {"2x2", "3x3", "4x4", "4x2", "2x4"}
    if not args.topologies or not set(args.topologies) <= supported:
        parser.error(
            "--topologies may contain only 2x2, 3x3, 4x4, 4x2 and 2x4"
        )
    try:
        args.workers = positive_csv(args.workers, "--workers", 8)
        args.gaps = nonnegative_csv(args.gaps, "--gaps", 1_000_000)
    except argparse.ArgumentTypeError as error:
        parser.error(str(error))
    if args.transactions <= 0 or args.transactions > 1_000_000:
        parser.error("--transactions must be in 1..1000000")
    if args.warmup_transactions < 0:
        parser.error("--warmup-transactions must be non-negative")
    if not 0 <= args.read_percent <= 100:
        parser.error("--read-percent must be in 0..100")
    if args.seed < 0 or args.seed > 0xFFFF_FFFF:
        parser.error("--seed must be in 0..4294967295")
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    if args.max_p99 is not None and (
        not math.isfinite(args.max_p99) or args.max_p99 < 0
    ):
        parser.error("--max-p99 must be non-negative")
    if not math.isfinite(args.min_bandwidth) or args.min_bandwidth < 0:
        parser.error("--min-bandwidth must be non-negative")
    if not math.isfinite(args.max_link_util) \
            or not 0 <= args.max_link_util <= 100:
        parser.error("--max-link-util must be in 0..100")
    return args


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_dashboard_module(path: pathlib.Path):
    spec = importlib.util.spec_from_file_location("noc_dashboard_contract", path)
    if spec is None or spec.loader is None:
        raise SweepError(f"cannot load dashboard contract from {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def metric(report: dict[str, Any], name: str) -> float:
    entry = report["derived_metrics"][name]
    if entry.get("source") != "D" or entry.get("value") is None:
        raise SweepError(f"derived metric {name!r} is missing or mislabeled")
    return float(entry["value"])


def measured(report: dict[str, Any], *path: str) -> float:
    current: Any = report
    for item in path:
        current = current[item]
    if current.get("source") != "M" or current.get("value") is None:
        raise SweepError(
            "measured metric " + ".".join(path) + " is missing or mislabeled"
        )
    return float(current["value"])


def row_from_report(
    identifier: str,
    report: dict[str, Any],
    metrics_path: pathlib.Path,
    command: list[str],
    benchmark_hash: str,
    constraints: dict[str, float | None],
) -> dict[str, Any]:
    window = report["measurement_window"]
    config = report["configuration"]
    topology = config["topology"]
    workload = report["workload"]
    row: dict[str, Any] = {
        "id": identifier,
        "status": "measured",
        "rejection_reasons": [],
        "topology": f"{topology['width']}x{topology['height']}",
        "workload": workload["name"],
        "workload_kind": workload.get("kind"),
        "workers_per_manager": workload.get("workers_per_manager", 1),
        "injection_gap_cycles": workload.get("injection_gap_cycles"),
        "seed": workload.get("seed"),
        "drained": window["drained"],
        "completed_transactions": int(
            measured(report, "transaction_metrics", "global", "transactions")
        ),
        "mean_latency_cycles": measured(
            report, "transaction_metrics", "global",
            "latency_cycles", "mean"
        ),
        "p99_latency_cycles": measured(
            report, "transaction_metrics", "global",
            "latency_cycles", "p99"
        ),
        "payload_bandwidth_gb_s": metric(
            report, "payload_bandwidth_gb_s"
        ),
        "transaction_throughput_mtrans_s": metric(
            report, "transaction_throughput_mtrans_s"
        ),
        "peak_link_utilisation_percent": max(
            metric(report, "request_peak_link_utilisation"),
            metric(report, "response_peak_link_utilisation"),
        ),
        "peak_stall_ratio_percent": max(
            metric(report, "request_peak_stall_ratio"),
            metric(report, "response_peak_stall_ratio"),
        ),
        "jain_fairness": metric(report, "jain_fairness"),
        "average_hop_count": metric(report, "average_hop_count"),
        "provenance": {
            "metrics_path": str(metrics_path.resolve()),
            "metrics_sha256": sha256(metrics_path),
            "benchmark_sha256": benchmark_hash,
            "generated_utc": report["provenance"].get("generated_utc"),
            "command": command,
        },
    }
    reasons: list[str] = row["rejection_reasons"]
    if config.get("timing_mode") != "detailed":
        reasons.append("timing mode is not detailed")
    if workload.get("kind") != "synthetic":
        reasons.append("workload is not a controlled synthetic benchmark")
    if not window.get("drained"):
        reasons.append("measurement did not drain")
    if row["completed_transactions"] <= 0:
        reasons.append("required traffic was not exercised")
    if constraints["max_p99"] is not None \
            and row["p99_latency_cycles"] > constraints["max_p99"]:
        reasons.append("P99 latency exceeds constraint")
    if row["payload_bandwidth_gb_s"] < constraints["min_bandwidth"]:
        reasons.append("payload bandwidth is below constraint")
    if row["peak_link_utilisation_percent"] > \
            constraints["max_link_util"]:
        reasons.append("peak link utilisation exceeds constraint")
    row["feasible"] = not reasons
    return row


def objective_key(objective: str) -> tuple[Callable[[dict[str, Any]], float], bool]:
    if objective == "min-p99":
        return lambda row: float(row["p99_latency_cycles"]), False
    if objective == "max-bandwidth":
        return lambda row: float(row["payload_bandwidth_gb_s"]), True
    if objective == "min-peak-util":
        return lambda row: float(row["peak_link_utilisation_percent"]), False
    if objective == "max-fairness":
        return lambda row: float(row["jain_fairness"]), True
    raise SweepError(f"unknown objective {objective!r}")


def select_winner(
    rows: list[dict[str, Any]], objective: str
) -> dict[str, Any] | None:
    feasible = [
        row for row in rows
        if row.get("status") == "measured"
        and row.get("feasible") is True
        and row.get("drained") is True
    ]
    if not feasible:
        return None
    key, reverse = objective_key(objective)
    # ID is the deterministic tie-breaker; never use host completion order.
    return sorted(
        feasible,
        key=lambda row: (
            -key(row) if reverse else key(row),
            str(row["id"]),
        ),
    )[0]


def atomic_json(path: pathlib.Path, data: dict[str, Any]) -> None:
    descriptor, name = tempfile.mkstemp(
        prefix=path.name + ".", suffix=".tmp", dir=path.parent
    )
    temporary = pathlib.Path(name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as output:
            json.dump(data, output, indent=2, sort_keys=False)
            output.write("\n")
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
    except BaseException:
        temporary.unlink(missing_ok=True)
        raise


def write_csv(path: pathlib.Path, rows: list[dict[str, Any]]) -> None:
    fields = (
        "id",
        "status",
        "feasible",
        "topology",
        "workload",
        "workers_per_manager",
        "injection_gap_cycles",
        "completed_transactions",
        "mean_latency_cycles",
        "p99_latency_cycles",
        "payload_bandwidth_gb_s",
        "transaction_throughput_mtrans_s",
        "peak_link_utilisation_percent",
        "peak_stall_ratio_percent",
        "jain_fairness",
        "average_hop_count",
        "drained",
        "rejection_reasons",
    )
    with path.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        for row in rows:
            encoded = dict(row)
            encoded["rejection_reasons"] = "; ".join(
                row.get("rejection_reasons", [])
            )
            writer.writerow(encoded)


def print_table(rows: list[dict[str, Any]], winner: dict[str, Any] | None,
                objective: str) -> None:
    print("FlooNoC DESIGN-SPACE DECISION")
    print(
        "ID              Topo  W  Gap  P99cy  BW GB/s  Link%  Stall%  "
        "Fair    Result"
    )
    print("-" * 94)
    for row in rows:
        if row["status"] != "measured":
            result = "FAIL: " + "; ".join(row["rejection_reasons"])
            print(f"{row['id']:<15} {'-':<5} {'-':>2} {'-':>4} "
                  f"{'-':>6} {'-':>8} {'-':>6} {'-':>7} {'-':>7} {result}")
            continue
        result = "PASS" if row["feasible"] else \
            "reject: " + "; ".join(row["rejection_reasons"])
        print(
            f"{row['id']:<15} {row['topology']:<5} "
            f"{row['workers_per_manager']:>2} "
            f"{row['injection_gap_cycles']:>4} "
            f"{row['p99_latency_cycles']:>6.1f} "
            f"{row['payload_bandwidth_gb_s']:>8.3f} "
            f"{row['peak_link_utilisation_percent']:>6.2f} "
            f"{row['peak_stall_ratio_percent']:>7.2f} "
            f"{row['jain_fairness']:>7.4f} {result}"
        )
    print()
    if winner is None:
        print(f"WINNER ({objective}): none - no feasible complete measurement")
    else:
        print(
            f"WINNER ({objective}): {winner['id']} "
            f"{winner['topology']} workers={winner['workers_per_manager']} "
            f"P99={winner['p99_latency_cycles']:.1f} cycles "
            f"BW={winner['payload_bandwidth_gb_s']:.3f} GB/s"
        )
    print("Area/power/energy: unavailable; not used for selection.")


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    benchmark = args.benchmark.resolve()
    if not benchmark.is_file() or not os.access(benchmark, os.X_OK):
        raise SweepError(f"benchmark is not executable: {benchmark}")
    dashboard = load_dashboard_module(args.dashboard_tool.resolve())
    args.output_dir.mkdir(parents=True, exist_ok=True)
    benchmark_hash = sha256(benchmark)
    constraints = {
        "max_p99": args.max_p99,
        "min_bandwidth": args.min_bandwidth,
        "max_link_util": args.max_link_util,
    }
    rows: list[dict[str, Any]] = []
    configurations = itertools.product(
        args.topologies, args.workers, args.gaps
    )
    for index, (topology, workers, gap) in enumerate(configurations):
        identifier = f"cfg{index:03d}"
        metrics_path = args.output_dir / f"{identifier}.metrics.json"
        log_path = args.output_dir / f"{identifier}.log"
        command = [
            str(benchmark),
            "--workload", args.workload,
            "--topology", topology,
            "--transactions", str(args.transactions),
            "--warmup-transactions", str(args.warmup_transactions),
            "--workers-per-manager", str(workers),
            "--injection-gap-cycles", str(gap),
            "--read-percent", str(args.read_percent),
            "--burst-bytes", str(args.burst_bytes),
            "--seed", str(args.seed),
            "--noc-metrics", str(metrics_path),
        ]
        with log_path.open("w", encoding="utf-8") as log:
            try:
                run = subprocess.run(
                    command,
                    check=False,
                    stdout=log,
                    stderr=subprocess.STDOUT,
                    timeout=args.timeout,
                )
            except subprocess.TimeoutExpired:
                rows.append(
                    {
                        "id": identifier,
                        "status": "failed",
                        "feasible": False,
                        "drained": False,
                        "rejection_reasons": ["benchmark timeout"],
                        "provenance": {
                            "benchmark_sha256": benchmark_hash,
                            "command": command,
                        },
                    }
                )
                continue
        if run.returncode != 0 or not metrics_path.is_file():
            rows.append(
                {
                    "id": identifier,
                    "status": "failed",
                    "feasible": False,
                    "drained": False,
                    "rejection_reasons": [
                        f"benchmark exited {run.returncode}"
                    ],
                    "provenance": {
                        "benchmark_sha256": benchmark_hash,
                        "command": command,
                    },
                }
            )
            continue
        try:
            report = dashboard.load_metrics(metrics_path)
            row = row_from_report(
                identifier, report, metrics_path, command,
                benchmark_hash, constraints
            )
        except Exception as error:
            row = {
                "id": identifier,
                "status": "failed",
                "feasible": False,
                "drained": False,
                "rejection_reasons": [f"invalid metrics: {error}"],
                "provenance": {
                    "benchmark_sha256": benchmark_hash,
                    "command": command,
                },
            }
        rows.append(row)

    winner = select_winner(rows, args.objective)
    result = {
        "schema": SWEEP_SCHEMA,
        "generated_utc": datetime.now(timezone.utc).strftime(
            "%Y-%m-%dT%H:%M:%SZ"
        ),
        "objective": args.objective,
        "constraints": constraints,
        "benchmark": {
            "path": str(benchmark),
            "sha256": benchmark_hash,
        },
        "rows": rows,
        "winner_id": winner["id"] if winner is not None else None,
        "availability": {
            "area": False,
            "power": False,
            "energy_per_flit": False,
        },
    }
    aggregate = args.output_dir / "sweep.json"
    atomic_json(aggregate, result)
    write_csv(args.output_dir / "sweep.csv", rows)
    print_table(rows, winner, args.objective)
    print(f"Aggregate JSON: {aggregate}")
    print(f"CSV: {args.output_dir / 'sweep.csv'}")
    return 0 if winner is not None else 2


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, SweepError, ValueError, KeyError, TypeError) as error:
        print(f"noc_sweep: {error}", file=sys.stderr)
        raise SystemExit(1)
