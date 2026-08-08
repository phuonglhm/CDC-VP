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


def block(name, x, y, hops, transactions, mean, survey_net, survey_total,
          accepted=True):
    return {
        "name": name,
        "x": x,
        "y": y,
        "hops": hops,
        "transactions": metric(transactions, "transactions", "M"),
        "network_cycles_mean": metric(mean, "cycles", "M"),
        "survey_network_cycles": metric(survey_net, "cycles", "M"),
        "survey_total_ns": metric(survey_total, "ns", "M"),
        "survey_accepted": accepted,
    }


def peripheral_map():
    return {
        "reference": {"name": "probe", "x": 3, "y": 3},
        "blocks": [
            # Measured on both paths.
            block("uart0", 1, 0, 5, 12, 11.5, 26, 36),
            # Surveyed, but this workload never touched it.
            block("spi0", 2, 0, 4, 0, None, 23, 43, accepted=False),
            # Firmware traffic only: no survey columns.
            block("clint", 1, 0, 5, 400, 9.25, None, None),
        ],
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
        "peripheral_map": peripheral_map(),
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
            "[6] PERIPHERAL MAP & LATENCY",
            "[7] HARDWARE DECISION INPUTS",
            "[8] SUMMARY & CONCLUSIONS",
            "Hops are from probe at (3,3)",
            # The block's two measured paths, and the two dashes that say this
            # run measured only one of them.
            "26 cyc",
            "36 ns",
            "11.500 cyc",
            "Register refused by the IP",
            "spi0",
            "No traffic from this workload to 1 of 3 blocks",
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

        # A latency column in the peripheral map may be null, but never
        # anything other than measured. This is the mutation that matters:
        # relabelling a cell [A] is exactly how a no-contention estimate would
        # get into a table the reader takes for measurement.
        estimated = fixture()
        estimated["peripheral_map"]["blocks"][0][
            "survey_network_cycles"]["source"] = "A"
        metrics.write_text(json.dumps(estimated), encoding="utf-8")
        analytic = subprocess.run(
            [sys.executable, str(script), str(metrics)],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if analytic.returncode == 0 or "survey_network_cycles" not in \
                analytic.stderr:
            print("dashboard accepted an analytic peripheral latency",
                  file=sys.stderr)
            return 1

        duplicated = fixture()
        duplicated["peripheral_map"]["blocks"].append(
            duplicated["peripheral_map"]["blocks"][0])
        metrics.write_text(json.dumps(duplicated), encoding="utf-8")
        repeated = subprocess.run(
            [sys.executable, str(script), str(metrics)],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if repeated.returncode == 0 or "more than once" not in \
                repeated.stderr:
            print("dashboard accepted a duplicated peripheral block",
                  file=sys.stderr)
            return 1

        no_hops = fixture()
        del no_hops["peripheral_map"]["blocks"][0]["hops"]
        metrics.write_text(json.dumps(no_hops), encoding="utf-8")
        incomplete_block = subprocess.run(
            [sys.executable, str(script), str(metrics)],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if incomplete_block.returncode == 0 or "hops" not in \
                incomplete_block.stderr:
            print("dashboard accepted a peripheral block without placement",
                  file=sys.stderr)
            return 1

        # --peripheral-baseline: a survey run's directed-read latencies fill
        # the columns a firmware run cannot produce at all.
        firmware = fixture()
        for entry in firmware["peripheral_map"]["blocks"]:
            entry["survey_network_cycles"] = metric(None, "cycles", "M")
            entry["survey_total_ns"] = metric(None, "ns", "M")
        metrics.write_text(json.dumps(firmware), encoding="utf-8")
        survey = pathlib.Path(temp) / "survey.json"
        survey.write_text(json.dumps(fixture()), encoding="utf-8")
        merged = subprocess.run(
            [sys.executable, str(script), str(metrics),
             "--peripheral-baseline", str(survey)],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if merged.returncode != 0:
            print(merged.stderr, file=sys.stderr)
            return 1
        for marker in ("from a SEPARATE run", "26 cyc", "36 ns",
                       "measured from probe at (3,3)"):
            if marker not in merged.stdout:
                print(f"missing merged-baseline marker: {marker}",
                      file=sys.stderr)
                print(merged.stdout, file=sys.stderr)
                return 1
        # The traffic columns still belong to this run alone.
        if "12" not in merged.stdout:
            print("merge lost this run's own traffic counts", file=sys.stderr)
            return 1

        # Latency depends on placement, so a baseline describing another
        # floorplan is a different SoC and must be refused, not merged.
        moved = fixture()
        moved["peripheral_map"]["blocks"][0]["x"] = 2
        survey.write_text(json.dumps(moved), encoding="utf-8")
        misplaced = subprocess.run(
            [sys.executable, str(script), str(metrics),
             "--peripheral-baseline", str(survey)],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if misplaced.returncode == 0 or "depends on placement" not in \
                misplaced.stderr:
            print("dashboard merged a baseline with a moved block",
                  file=sys.stderr)
            return 1

        renamed = fixture()
        renamed["peripheral_map"]["blocks"][0]["name"] = "uart9"
        survey.write_text(json.dumps(renamed), encoding="utf-8")
        different_map = subprocess.run(
            [sys.executable, str(script), str(metrics),
             "--peripheral-baseline", str(survey)],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if different_map.returncode == 0 or "different set of blocks" not in \
                different_map.stderr:
            print("dashboard merged a baseline with a different block set",
                  file=sys.stderr)
            return 1

        # A fast-mode file carries no-contention estimates. Merging one would
        # put an analytic number in a measured column through the back door.
        estimated_baseline = fixture()
        estimated_baseline["configuration"]["timing_mode"] = "fast"
        survey.write_text(json.dumps(estimated_baseline), encoding="utf-8")
        fast_baseline = subprocess.run(
            [sys.executable, str(script), str(metrics),
             "--peripheral-baseline", str(survey)],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if fast_baseline.returncode == 0 or "fast mode" not in \
                fast_baseline.stderr:
            print("dashboard merged a fast-mode baseline", file=sys.stderr)
            return 1

        # A firmware file has no survey measurements to give; silently
        # merging nothing would look like success.
        survey.write_text(json.dumps(firmware), encoding="utf-8")
        empty_baseline = subprocess.run(
            [sys.executable, str(script), str(metrics),
             "--peripheral-baseline", str(survey)],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if empty_baseline.returncode == 0 or "no survey measurements" not in \
                empty_baseline.stderr:
            print("dashboard accepted a baseline with nothing to merge",
                  file=sys.stderr)
            return 1

        metrics.write_text(json.dumps(fixture()), encoding="utf-8")

        # The section is optional: noc_benchmark has no SoC floorplan, and its
        # files must keep rendering.
        without_map = fixture()
        del without_map["peripheral_map"]
        metrics.write_text(json.dumps(without_map), encoding="utf-8")
        benchmark = subprocess.run(
            [sys.executable, str(script), str(metrics)],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if benchmark.returncode != 0 or \
                "No SoC peripheral map" not in benchmark.stdout:
            print("dashboard rejected a file without a peripheral map",
                  file=sys.stderr)
            print(benchmark.stderr, file=sys.stderr)
            return 1

    print("PASS: FlooNoC terminal dashboard")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
