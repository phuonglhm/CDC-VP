#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Render a FlooNoC metrics-v1 JSON file as a terminal dashboard."""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any, Iterable


SCHEMA = "floo-noc-metrics-v1"
PORT_ORDER = ["North", "East", "South", "West", "Eject"]


class DashboardError(RuntimeError):
    pass


def require(mapping: dict[str, Any], key: str, where: str) -> Any:
    if key not in mapping:
        raise DashboardError(f"{where} is missing required field '{key}'")
    return mapping[key]


def require_source(metric: dict[str, Any], source: str, where: str) -> None:
    if metric.get("source") != source:
        raise DashboardError(
            f"{where} must carry metric source {source!r}, "
            f"got {metric.get('source')!r}"
        )


def validate_bucket(bucket: dict[str, Any], where: str) -> None:
    require_source(require(bucket, "transactions", where), "M",
                   f"{where}.transactions")
    require_source(require(bucket, "payload_bytes", where), "M",
                   f"{where}.payload_bytes")
    latency = require(bucket, "latency_cycles", where)
    for field in ("min", "mean", "p50", "p95", "p99", "max"):
        require_source(require(latency, field, f"{where}.latency_cycles"),
                       "M", f"{where}.latency_cycles.{field}")


def load_metrics(path: pathlib.Path) -> dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8") as source:
            data = json.load(source)
    except OSError as error:
        raise DashboardError(f"cannot read '{path}': {error}") from error
    except json.JSONDecodeError as error:
        raise DashboardError(f"invalid JSON in '{path}': {error}") from error

    if not isinstance(data, dict):
        raise DashboardError("metrics root must be a JSON object")
    if data.get("schema") != SCHEMA:
        raise DashboardError(
            f"unsupported metrics schema {data.get('schema')!r}; "
            f"expected {SCHEMA!r}"
        )
    for field in (
        "provenance",
        "configuration",
        "workload",
        "measurement_window",
        "transaction_metrics",
        "physical_meshes",
        "availability",
        "warnings",
    ):
        require(data, field, "metrics root")

    config = data["configuration"]
    if config.get("port_order") != PORT_ORDER:
        raise DashboardError(
            "port order is not the signed North/East/South/West/Eject mapping"
        )
    meshes = data["physical_meshes"]
    if not isinstance(meshes, list) or {m.get("channel") for m in meshes} != {
        "request",
        "response",
    }:
        raise DashboardError(
            "physical_meshes must contain request and response fabrics"
        )
    topology = config.get("topology", {})
    width = topology.get("width")
    height = topology.get("height")
    if not isinstance(width, int) or not isinstance(height, int):
        raise DashboardError("configuration topology must have integer dimensions")
    for mesh in meshes:
        if mesh.get("width") != width or mesh.get("height") != height:
            raise DashboardError(
                f"{mesh.get('channel', 'unknown')} mesh dimensions do not "
                "match configuration"
            )
        routers = mesh.get("routers")
        if not isinstance(routers, list) or len(routers) != width * height:
            raise DashboardError(
                f"{mesh.get('channel', 'unknown')} mesh must contain exactly "
                f"{width * height} routers"
            )
        coordinates = {(router.get("x"), router.get("y")) for router in routers}
        if coordinates != {
            (x, y) for y in range(height) for x in range(width)
        }:
            raise DashboardError(
                f"{mesh.get('channel', 'unknown')} mesh router coordinates "
                "are incomplete or duplicated"
            )
        for router in routers:
            for boundary in ("inputs", "outputs"):
                ports = router.get(boundary)
                if not isinstance(ports, list) or [
                    port.get("port") for port in ports
                ] != PORT_ORDER:
                    raise DashboardError(
                        f"{mesh.get('channel')} router "
                        f"({router.get('x')},{router.get('y')}) {boundary} "
                        "do not use canonical port order"
                    )

    transactions = data["transaction_metrics"]
    validate_bucket(require(transactions, "global", "transaction_metrics"),
                    "transaction_metrics.global")
    for index, bucket in enumerate(transactions.get("classes", [])):
        validate_bucket(bucket, f"transaction_metrics.classes[{index}]")
    accounting = data.get("traffic_accounting")
    if accounting is not None:
        for name in ("offered_transactions", "delivered_transactions"):
            require_source(require(accounting, name, "traffic_accounting"),
                           "M", f"traffic_accounting.{name}")
    for name, metric in data.get("derived_metrics", {}).items():
        require_source(metric, "D", f"derived_metrics.{name}")
    validate_peripheral_map(data.get("peripheral_map"))
    return data


def validate_peripheral_map(peripherals: Any) -> None:
    """Check the optional peripheral map before anything is rendered.

    The section is optional because a metrics file written by `noc_benchmark`
    has no SoC floorplan to describe. What is not optional is the labelling: a
    latency column here may be null, but it may never be anything except [M].
    Placement is the only part that is static, and it is carried as plain
    integers so it cannot be mistaken for a measurement.
    """
    if peripherals is None:
        return
    if not isinstance(peripherals, dict):
        raise DashboardError("peripheral_map must be a JSON object")
    reference = require(peripherals, "reference", "peripheral_map")
    for field in ("name", "x", "y"):
        require(reference, field, "peripheral_map.reference")
    blocks = require(peripherals, "blocks", "peripheral_map")
    if not isinstance(blocks, list):
        raise DashboardError("peripheral_map.blocks must be an array")
    seen: set[str] = set()
    for index, block in enumerate(blocks):
        where = f"peripheral_map.blocks[{index}]"
        if not isinstance(block, dict):
            raise DashboardError(f"{where} must be a JSON object")
        name = require(block, "name", where)
        # A duplicated block would double-count in the reader's head even
        # though each row is individually correct.
        if name in seen:
            raise DashboardError(
                f"peripheral_map lists block {name!r} more than once"
            )
        seen.add(name)
        for field in ("x", "y", "hops"):
            value = require(block, field, where)
            if not isinstance(value, int) or isinstance(value, bool):
                raise DashboardError(
                    f"{where}.{field} must be an integer, got {value!r}"
                )
        for field in (
            "transactions",
            "network_cycles_mean",
            "survey_network_cycles",
            "survey_total_ns",
        ):
            require_source(require(block, field, where), "M",
                           f"{where}.{field}")


def metric_value(metric: dict[str, Any]) -> Any:
    return require(metric, "value", "metric")


def format_number(value: Any, decimals: int = 3) -> str:
    if value is None:
        return "unavailable"
    if isinstance(value, bool):
        return "yes" if value else "no"
    if isinstance(value, int):
        return f"{value:,}"
    if isinstance(value, float):
        return f"{value:,.{decimals}f}"
    return str(value)


def line(char: str = "-", width: int = 106) -> str:
    return char * width


def title(text: str) -> list[str]:
    return [line("="), text, line("=")]


def section(number: int, name: str) -> list[str]:
    return ["", f"[{number}] {name}", line("-")]


def table(headers: list[str], rows: Iterable[list[Any]]) -> list[str]:
    materialized = [[str(cell) for cell in row] for row in rows]
    widths = [len(header) for header in headers]
    for row in materialized:
        if len(row) != len(headers):
            raise DashboardError("internal dashboard table width mismatch")
        for index, cell in enumerate(row):
            widths[index] = max(widths[index], len(cell))
    rule = "-+-".join("-" * width for width in widths)
    output = [
        " | ".join(
            header.ljust(widths[index])
            for index, header in enumerate(headers)
        ),
        rule,
    ]
    output.extend(
        " | ".join(
            cell.ljust(widths[index]) for index, cell in enumerate(row)
        )
        for row in materialized
    )
    return output


def transaction_classes(data: dict[str, Any]) -> dict[str, dict[str, Any]]:
    classes = data["transaction_metrics"].get("classes", [])
    return {
        item["name"]: item
        for item in classes
        if isinstance(item, dict) and isinstance(item.get("name"), str)
    }


def latency_row(
    label: str, bucket: dict[str, Any] | None
) -> list[str]:
    if bucket is None:
        return [label, "unavailable", "-", "-", "-", "-", "[M]"]
    latency = bucket["latency_cycles"]
    transactions = metric_value(bucket["transactions"])
    return [
        label,
        format_number(transactions),
        format_number(metric_value(latency["mean"])),
        format_number(metric_value(latency["p50"])),
        format_number(metric_value(latency["p95"])),
        format_number(metric_value(latency["p99"])),
        "[M]",
    ]


def hotspots(data: dict[str, Any]) -> list[dict[str, Any]]:
    found: list[dict[str, Any]] = []
    output_depth = data["configuration"].get("output_fifo_depth")
    for mesh in data["physical_meshes"]:
        channel = mesh["channel"]
        for router in mesh["routers"]:
            measurement_cycles = data["measurement_window"]["counted_cycles"]
            sampled_cycles = router["counted_cycles"]
            for port in router["outputs"]:
                if port["port"] == "Eject":
                    continue
                busy = port["busy_cycles"]
                utilisation = (
                    100.0 * port["accepted_flits"] / measurement_cycles
                    if measurement_cycles
                    else 0.0
                )
                stall = 100.0 * port["stall_cycles"] / busy if busy else 0.0
                status = (
                    "CRITICAL"
                    if utilisation >= 85.0
                    else "WARN"
                    if (
                        utilisation >= 70.0
                        or stall >= 10.0
                        or (
                            isinstance(output_depth, int)
                            and output_depth > 0
                            and port["occupancy_high_water"] >= output_depth
                        )
                    )
                    else "OK"
                )
                if busy or port["occupancy_high_water"]:
                    found.append(
                    {
                        "channel": channel,
                        "x": router["x"],
                        "y": router["y"],
                        "port": port["port"],
                        "flits": port["accepted_flits"],
                        "utilisation": utilisation,
                        "stall": stall,
                        "occupancy_mean": (
                            port["occupancy_sum"] / sampled_cycles
                            if sampled_cycles
                            else 0.0
                        ),
                        "occupancy_high": port["occupancy_high_water"],
                        "status": status,
                    })
    return sorted(
        found,
        key=lambda item: (
            item["utilisation"],
            item["stall"],
            item["flits"],
        ),
        reverse=True,
    )


def merge_peripheral_baseline(
    data: dict[str, Any],
    baseline: dict[str, Any],
    source: pathlib.Path,
) -> dict[str, Any]:
    """Fill this run's empty survey columns from a separate survey run.

    This is sound only because of what the survey columns are: the cost of one
    directed register read to a block, which is a property of the floorplan and
    the mesh, not of the workload. A firmware run cannot produce them at all —
    it is forbidden from issuing synthetic traffic — so without this the two
    columns are structurally empty rather than merely unmeasured.

    What makes it sound is also what has to be checked. Latency depends on
    placement, so a baseline whose block set, topology or node assignment
    differs describes a different SoC and is refused rather than merged. The
    traffic columns are never touched: those belong to this run alone.
    """
    target = data.get("peripheral_map")
    if target is None:
        raise DashboardError(
            "this metrics file has no peripheral_map to merge a baseline into"
        )
    donor = baseline.get("peripheral_map")
    if donor is None:
        raise DashboardError(f"'{source}' has no peripheral_map")

    if baseline["configuration"].get("timing_mode") != "detailed":
        raise DashboardError(
            f"'{source}' was produced in fast mode; its latencies are "
            "no-contention estimates, not measurements"
        )
    if baseline["configuration"]["topology"] != data["configuration"][
            "topology"]:
        raise DashboardError(
            f"'{source}' has a different topology; its per-block latencies "
            "describe another mesh"
        )

    donor_blocks = {block["name"]: block for block in donor["blocks"]}
    if set(donor_blocks) != {block["name"] for block in target["blocks"]}:
        raise DashboardError(
            f"'{source}' maps a different set of blocks; the address map "
            "changed between the two runs"
        )

    filled = 0
    available = 0
    for block in target["blocks"]:
        other = donor_blocks[block["name"]]
        if (other["x"], other["y"]) != (block["x"], block["y"]):
            raise DashboardError(
                f"block {block['name']!r} sits at "
                f"({other['x']},{other['y']}) in '{source}' but at "
                f"({block['x']},{block['y']}) here; latency depends on "
                "placement"
            )
        for field in ("survey_network_cycles", "survey_total_ns"):
            if other[field].get("value") is None:
                continue
            available += 1
            if block[field].get("value") is None:
                block[field] = dict(other[field])
                filled += 1
        if other.get("survey_accepted") is False:
            block["survey_accepted"] = False

    if available == 0:
        raise DashboardError(
            f"'{source}' carries no survey measurements; run it with "
            "--mode survey"
        )
    return {
        "path": str(source),
        "reference": donor["reference"],
        "filled": filled,
        "workload": baseline["workload"].get("name", "unknown"),
        "generated": baseline["provenance"].get("generated_utc", "unknown"),
    }


def optional_cell(metric: dict[str, Any], suffix: str = "",
                  decimals: int = 3) -> str:
    """Render a measured cell, or a dash when this run did not measure it.

    A dash is load-bearing. The alternative that keeps suggesting itself is to
    fill the gap from the no-contention formula, and that would put an [A]
    estimate in a column labelled [M] for every block the workload never
    touched.
    """
    value = metric.get("value")
    if value is None:
        return "-"
    return format_number(value, decimals) + suffix


def peripheral_section(
    data: dict[str, Any], baseline: dict[str, Any] | None = None
) -> list[str]:
    peripherals = data.get("peripheral_map")
    output = section(6, "PERIPHERAL MAP & LATENCY")
    if peripherals is None:
        output.append(
            "No SoC peripheral map in this file; it was produced by a "
            "synthetic benchmark rather than by noc_soc."
        )
        return output

    reference = peripherals["reference"]
    blocks = peripherals["blocks"]
    output.append(
        f"Hops are from {reference['name']} at "
        f"({reference['x']},{reference['y']}); another manager sees a "
        "different distance to the same block."
    )
    if baseline is not None:
        donor = baseline["reference"]
        output.append(
            f"Survey columns: {baseline['filled']} cells from a SEPARATE run, "
            f"{baseline['path']}"
        )
        output.append(
            f"  ({baseline['workload']}, {baseline['generated']}), measured "
            f"from {donor['name']} at ({donor['x']},{donor['y']}) - not from "
            f"{reference['name']}, and not from this run's traffic."
        )
    output += table(
        [
            "Block",
            "Node",
            "Hops",
            "Trans",
            "Mean net",
            "Survey net",
            "Survey total",
            "Src",
        ],
        [
            [
                block["name"],
                f"({block['x']},{block['y']})",
                str(block["hops"]),
                optional_cell(block["transactions"], decimals=0),
                optional_cell(block["network_cycles_mean"], " cyc"),
                optional_cell(block["survey_network_cycles"], " cyc",
                              decimals=0),
                optional_cell(block["survey_total_ns"], " ns", decimals=0),
                "[M]",
            ]
            for block in blocks
        ],
    )
    output.append(
        "Node and hops are [S] floorplan. 'Trans'/'Mean net' are this run's "
        "own traffic, network cycles only."
    )
    output.append(
        "'Survey net'/'Survey total' come from one directed register read per "
        "block and exist in survey mode only;"
    )
    output.append(
        "  the total additionally carries the peripheral's own access "
        "latency, which the completion observer excludes."
    )
    refused = [
        block["name"] for block in blocks
        if block.get("survey_accepted") is False
    ]
    if refused:
        output.append(
            "Register refused by the IP (the latency is still real, the "
            "register choice was not): " + ", ".join(refused) + "."
        )
    untouched = [
        block["name"] for block in blocks
        if block["transactions"].get("value") in (0, None)
    ]
    if untouched:
        output.append(
            f"No traffic from this workload to {len(untouched)} of "
            f"{len(blocks)} blocks: " + ", ".join(untouched) + "."
        )
    # Naming these matters more than it looks: an empty survey cell here is a
    # block that was never probed, which is not the same statement as a block
    # that was probed and found fast.
    unprobed = [
        block["name"] for block in blocks
        if block["survey_network_cycles"].get("value") is None
    ]
    if unprobed:
        output.append(
            f"Never probed by a survey, so no directed-read latency exists "
            f"for {len(unprobed)} of {len(blocks)} blocks: "
            + ", ".join(unprobed) + "."
        )
    return output


def render(
    data: dict[str, Any],
    top_count: int,
    baseline: dict[str, Any] | None = None,
) -> str:
    config = data["configuration"]
    topology = config["topology"]
    workload = data["workload"]
    provenance = data["provenance"]
    window = data["measurement_window"]
    global_metrics = data["transaction_metrics"]["global"]
    derived = data.get("derived_metrics", {})
    classes = transaction_classes(data)
    hot = hotspots(data)

    output = title(
        "FlooNoC SystemC Virtual Platform - Network Metrics Report"
    )
    output.append(
        "Input -> Measure (detailed SystemC) -> Metric Dashboard "
        "-> Hardware Decision"
    )
    output.append(
        f"Generated: {provenance.get('generated_utc', 'unavailable')}   "
        f"Build: {provenance.get('build_type', 'unavailable')}   "
        f"Schema: {data['schema']}"
    )

    output += section(1, "INPUT")
    managers = ", ".join(
        f"{manager['name']}@({manager['x']},{manager['y']})"
        for manager in config.get("managers", [])
    )
    output += table(
        ["Field", "Value", "Src"],
        [
            ["Topology", f"{topology['width']}x{topology['height']} XY", "[S]"],
            [
                "NoC clock",
                f"{config['clock_period_ns']} ns/cycle",
                "[S]",
            ],
            ["Port order", ", ".join(config["port_order"]), "[S]"],
            ["Managers", managers, "[S]"],
            ["Workload", workload["name"], "[S]"],
            ["Timing", config["timing_mode"], "[S]"],
            ["Warm-up", f"{window['warmup_cycles']} cycles", "[M]"],
            ["Measured", f"{window['counted_cycles']:,} cycles", "[M]"],
            ["Drained", format_number(window["drained"]), "[M]"],
        ],
    )

    output += section(2, "MEASUREMENT")
    output += table(
        ["Metric", "Value", "Unit", "Src"],
        [
            [
                "Completed transactions",
                format_number(metric_value(global_metrics["transactions"])),
                "transactions",
                "[M]",
            ],
            [
                "Useful payload",
                format_number(metric_value(global_metrics["payload_bytes"])),
                "bytes",
                "[M]",
            ],
            [
                "Transaction throughput",
                format_number(
                    metric_value(derived["transaction_throughput_mtrans_s"])
                ),
                "Mtrans/s",
                "[D]",
            ],
            [
                "Payload bandwidth",
                format_number(
                    metric_value(derived["payload_bandwidth_gb_s"])
                ),
                "GB/s",
                "[D]",
            ],
            [
                "Jain fairness",
                (
                    format_number(metric_value(derived["jain_fairness"]))
                    if "jain_fairness" in derived
                    else "unavailable"
                ),
                "ratio",
                "[D]",
            ],
            [
                "Average hop count",
                (
                    format_number(metric_value(derived["average_hop_count"]))
                    if "average_hop_count" in derived
                    else "unavailable"
                ),
                "hops",
                "[D]",
            ],
        ],
    )

    output += section(3, "LATENCY DASHBOARD")
    if workload.get("kind") == "synthetic":
        latency_rows = [latency_row("All traffic", global_metrics)]
        latency_rows.extend(
            latency_row(
                item["name"].removeprefix("manager_").replace("_", " "),
                item,
            )
            for item in data["transaction_metrics"].get("classes", [])
            if item.get("name", "").startswith("manager_")
        )
    else:
        latency_rows = [
            latency_row("All traffic", global_metrics),
            latency_row(
                "CPU -> RAM, DMA idle", classes.get("cpu_ram_dma_idle")
            ),
            latency_row(
                "CPU -> RAM, DMA active", classes.get("cpu_ram_dma_active")
            ),
            latency_row("PLIC claim", classes.get("cpu_plic_claim")),
            latency_row("PLIC complete", classes.get("cpu_plic_complete")),
            latency_row("Other CPU MMIO", classes.get("cpu_other_mmio")),
            latency_row("DMA all", classes.get("dma_all")),
        ]
    output += table(
        ["Class", "Samples", "Mean", "P50", "P95", "P99", "Src"],
        latency_rows,
    )
    output.append("Latency unit: network cycles; target delay excluded.")

    output += section(4, "MANAGER -> TARGET TRAFFIC")
    if workload.get("kind") == "synthetic":
        flows = [
            (
                item["name"][len("flow_"):-len("_ram")],
                "RAM",
                item["name"],
            )
            for item in data["transaction_metrics"].get("classes", [])
            if item.get("name", "").startswith("flow_")
            and item["name"].endswith("_ram")
        ]
    else:
        flows = [
            ("CPU", "RAM", "flow_cpu_ram"),
            ("CPU", "PLIC", "flow_cpu_plic"),
            ("CPU", "other MMIO", "flow_cpu_other_mmio"),
            ("DMA", "RAM", "flow_dma_ram"),
            ("DMA", "PLIC", "flow_dma_plic"),
            ("DMA", "other MMIO", "flow_dma_other_mmio"),
            ("probe", "RAM", "flow_probe_ram"),
            ("probe", "PLIC", "flow_probe_plic"),
            ("probe", "other MMIO", "flow_probe_other_mmio"),
        ]
    output += table(
        ["Source", "Target", "Transactions", "Bytes", "Mean", "P95", "P99"],
        [
            [
                source,
                target,
                latency_row("", classes.get(key))[1],
                (
                    format_number(
                        metric_value(classes[key]["payload_bytes"])
                    )
                    if key in classes
                    else "unavailable"
                ),
                latency_row("", classes.get(key))[2],
                latency_row("", classes.get(key))[4],
                latency_row("", classes.get(key))[5],
            ]
            for source, target, key in flows
        ],
    )

    output += section(5, f"HOTSPOTS - TOP {min(top_count, len(hot))}")
    output += table(
        [
            "Fabric",
            "Router",
            "Port",
            "Flits",
            "Util%",
            "Stall%",
            "FIFO mean/max",
            "Status",
        ],
        [
            [
                item["channel"],
                f"({item['x']},{item['y']})",
                item["port"],
                f"{item['flits']:,}",
                f"{item['utilisation']:.3f}",
                f"{item['stall']:.3f}",
                f"{item['occupancy_mean']:.3f}/{item['occupancy_high']}",
                item["status"],
            ]
            for item in hot[:top_count]
        ],
    )

    output += peripheral_section(data, baseline)

    output += section(7, "HARDWARE DECISION INPUTS")
    availability = data["availability"]
    output += table(
        ["Metric", "Value", "Reason"],
        [
            [
                "Area",
                "unavailable",
                "pending calibrated RTL synthesis"
                if not availability["area"]
                else "available",
            ],
            [
                "Power",
                "unavailable",
                "pending calibrated activity/power flow"
                if not availability["power"]
                else "available",
            ],
            [
                "Energy/flit",
                "unavailable",
                "pending calibrated RTL evidence"
                if not availability["energy_per_flit"]
                else "available",
            ],
        ],
    )
    output.append(
        "No DSE winner is selected by this single-configuration report."
    )

    output += section(8, "SUMMARY & CONCLUSIONS")
    if hot:
        peak = hot[0]
        output.append(
            f"- Peak directed link: {peak['channel']} "
            f"({peak['x']},{peak['y']}) {peak['port']} at "
            f"{peak['utilisation']:.3f}% utilisation and "
            f"{peak['stall']:.3f}% stall."
        )
    contention = derived.get("ram_contention_delta")
    contention_value = (
        metric_value(contention) if contention is not None else None
    )
    output.append(
        "- CPU RAM contention delta: "
        + (
            f"{format_number(contention_value)} cycles."
            if contention_value is not None
            else "unavailable."
        )
    )
    if not window["drained"]:
        output.append(
            "- Measurement ended with an active platform; use it as diagnostic "
            "evidence, not as a conservation/DSE sign-off."
        )
    for warning in data.get("warnings", []):
        output.append(f"- WARNING: {warning}")
    output.append(
        "- Legend: [M] measured, [D] derived, [A] analytic, [S] static/spec."
    )
    return "\n".join(output) + "\n"


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Render a FlooNoC metrics-v1 JSON terminal dashboard"
    )
    parser.add_argument("metrics", type=pathlib.Path)
    parser.add_argument(
        "--top", type=int, default=10, help="number of hotspot links to show"
    )
    parser.add_argument(
        "--output",
        type=pathlib.Path,
        help="write the report to a file instead of stdout",
    )
    parser.add_argument(
        "--peripheral-baseline",
        type=pathlib.Path,
        help=(
            "a --mode survey metrics file whose per-block directed-read "
            "latencies fill this run's empty survey columns; refused unless "
            "its topology, block set and placement match"
        ),
    )
    args = parser.parse_args(argv)
    if args.top <= 0:
        parser.error("--top must be positive")
    return args


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        data = load_metrics(args.metrics)
        baseline = None
        if args.peripheral_baseline is not None:
            baseline = merge_peripheral_baseline(
                data, load_metrics(args.peripheral_baseline),
                args.peripheral_baseline)
        report = render(data, args.top, baseline)
        if args.output is None:
            sys.stdout.write(report)
        else:
            args.output.write_text(report, encoding="utf-8")
    except (DashboardError, KeyError, TypeError, ValueError, OSError) as error:
        print(f"noc_dashboard: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
