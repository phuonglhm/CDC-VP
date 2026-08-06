#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0

"""Directed standard-library-only contract test for noc_dashboard.py."""

from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import tempfile


def metric(value, unit, source):
    return {"value": value, "unit": unit, "source": source}


def bucket(name=None):
    result = {
        "transactions": metric(2, "transactions", "M"),
        "payload_bytes": metric(16, "bytes", "M"),
        "latency_cycles": {
            "min": metric(10, "cycles", "M"),
            "mean": metric(12.0, "cycles", "M"),
            "p50": metric(10, "cycles", "M"),
            "p95": metric(14, "cycles", "M"),
            "p99": metric(14, "cycles", "M"),
            "max": metric(14, "cycles", "M"),
        },
    }
    if name is not None:
        result["name"] = name
    return result


def port(name, flits):
    return {
        "port": name,
        "accepted_flits": flits,
        "accepted_packets": flits,
        "stall_cycles": 1 if flits else 0,
        "busy_cycles": flits + (1 if flits else 0),
        "occupancy_sum": flits,
        "occupancy_high_water": 1 if flits else 0,
    }


def mesh(channel):
    ports = [port(name, 8 if name == "East" else 0) for name in
             ["North", "East", "South", "West", "Eject"]]
    return {
        "channel": channel,
        "width": 1,
        "height": 1,
        "routers": [{
            "x": 0,
            "y": 0,
            "counted_cycles": 10,
            "inputs": ports,
            "outputs": ports,
        }],
    }


def fixture():
    return {
        "schema": "floo-noc-metrics-v1",
        "provenance": {
            "generated_utc": "2026-08-05T00:00:00Z",
            "build_type": "Release",
        },
        "configuration": {
            "timing_mode": "detailed",
            "topology": {"width": 1, "height": 1},
            "routing": "XY",
            "port_order": ["North", "East", "South", "West", "Eject"],
            "clock_period_ns": 1.0,
            "managers": [{"name": "cpu", "x": 0, "y": 0}],
        },
        "workload": {"name": "directed-test", "kind": "synthetic", "seed": 1},
        "measurement_window": {
            "warmup_cycles": 0,
            "start_cycle": 0,
            "end_cycle": 10,
            "counted_cycles": 10,
            "drained": True,
        },
        "transaction_metrics": {
            "global": bucket(),
            "classes": [
                bucket("cpu_ram_dma_idle"),
                bucket("flow_cpu_ram"),
            ],
        },
        "physical_meshes": [mesh("request"), mesh("response")],
        "derived_metrics": {
            "transaction_throughput_mtrans_s": metric(
                1.0, "Mtrans/s", "D"),
            "payload_bandwidth_gb_s": metric(2.0, "GB/s", "D"),
            "ram_contention_delta": metric(0.0, "cycles", "D"),
        },
        "availability": {
            "router_counters": True,
            "area": False,
            "power": False,
            "energy_per_flit": False,
        },
        "warnings": [],
    }


def main():
    script = pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="noc_dashboard_test.") as temp:
        metrics = pathlib.Path(temp) / "metrics.json"
        metrics.write_text(json.dumps(fixture()), encoding="utf-8")
        run = subprocess.run(
            [sys.executable, str(script), str(metrics), "--top", "2"],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if run.returncode != 0:
            print(run.stderr, file=sys.stderr)
            return 1
        required = [
            "FlooNoC SystemC Virtual Platform",
            "[1] INPUT",
            "[3] LATENCY DASHBOARD",
            "[4] MANAGER -> TARGET TRAFFIC",
            "[5] HOTSPOTS",
            "request",
            "80.000",
            "Area",
            "unavailable",
            "No DSE winner",
            "[M] measured",
        ]
        for marker in required:
            if marker not in run.stdout:
                print(f"missing dashboard marker: {marker}", file=sys.stderr)
                print(run.stdout, file=sys.stderr)
                return 1

        bad = fixture()
        bad["configuration"]["port_order"] = [
            "East", "West", "South", "North", "Eject"]
        metrics.write_text(json.dumps(bad), encoding="utf-8")
        rejected = subprocess.run(
            [sys.executable, str(script), str(metrics)],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if rejected.returncode == 0 or "port order" not in rejected.stderr:
            print("dashboard accepted an invalid canonical port order",
                  file=sys.stderr)
            return 1

        metrics.write_text('{"schema":', encoding="utf-8")
        corrupt = subprocess.run(
            [sys.executable, str(script), str(metrics)],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if corrupt.returncode == 0 or "invalid JSON" not in corrupt.stderr:
            print("dashboard accepted corrupt JSON", file=sys.stderr)
            return 1

        partial = fixture()
        del partial["physical_meshes"]
        metrics.write_text(json.dumps(partial), encoding="utf-8")
        incomplete = subprocess.run(
            [sys.executable, str(script), str(metrics)],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if incomplete.returncode == 0 or "physical_meshes" not in \
                incomplete.stderr:
            print("dashboard accepted partial metrics", file=sys.stderr)
            return 1

        wrong_source = fixture()
        wrong_source["transaction_metrics"]["global"]["transactions"][
            "source"] = "A"
        metrics.write_text(json.dumps(wrong_source), encoding="utf-8")
        mislabeled = subprocess.run(
            [sys.executable, str(script), str(metrics)],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if mislabeled.returncode == 0 or "metric source" not in \
                mislabeled.stderr:
            print("dashboard accepted a mislabeled measured metric",
                  file=sys.stderr)
            return 1

    print("PASS: FlooNoC terminal dashboard")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
