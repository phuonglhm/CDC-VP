#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""One-factor-at-a-time screening of the supported NEO-CORE knobs (gate G4).

Screening, not a sweep. The plan is explicit that the Cartesian product is
1,296 configurations before modes and repeats, and that G4's job is to find
which knobs a workload is sensitive to so that G6 can combine only those. This
varies one knob at a time around a stated baseline and reports what moved.

Three rules this tool follows, each of which exists because breaking it would
produce a number that looks like evidence:

* **it reads result rows, never logs.** Section 11 of the plan requires it, and
  the rows carry `run_valid` and the G3 conservation identities that a log
  does not;
* **it refuses to summarise a rejected run.** A sensitivity figure averaged
  over a row whose bytes did not reconcile is a measurement of nothing. One
  invalid row fails the screening;
* **it reports insensitivity as a result.** A knob that changes nothing is a
  finding — it is what stops G6 spending configurations on it — and it is
  reported with the measured spread that justifies the word.
"""

from __future__ import annotations

import sys as _sys

_sys.dont_write_bytecode = True

import argparse
import csv
import json
import pathlib
import subprocess
import sys

SCHEMA = "neo-core-screening-v1"

# The baseline every point is measured against: the plan's section 7.1 current
# implementation reference.
BASELINE = {
    "--bank-width-bits": 128,
    "--bank-count": 4,
    "--pipeline-stages": 2,
    "--dma-max-burst": 2048,
    "--sram-capacity-bytes": 16 * 1024 * 1024,
}

# Only the knobs section 7.2 lists as configurable today. VLEN, MXU geometry,
# DMA channel count and external AXI width are not among them: D28 puts those
# behind G5 and the Neo Lite work packages, and the runner refuses them anyway.
KNOBS = {
    "local_bank_width_bits": ("--bank-width-bits", [64, 128, 256]),
    "local_bank_count": ("--bank-count", [1, 2, 4, 8]),
    "fabric_pipeline_stages": ("--pipeline-stages", [1, 2, 3]),
    "dma_max_burst_bytes": ("--dma-max-burst", [64, 256, 512, 2048]),
    "sram_capacity_bytes": (
        "--sram-capacity-bytes",
        [1024 * 1024, 4 * 1024 * 1024, 16 * 1024 * 1024],
    ),
}

# One representative case per benchmark, at the largest frozen size, in both
# modes. Screening every case would multiply the run count by ten for
# sensitivity information the largest case already carries most clearly.
WORKLOADS = [
    ("relu", "relu", "rvv", 9),
    ("vector_dot", "vector_dot", "rvv", 9),
    ("gemv_rvv", "gemv_rvv", "rvv", 8),
    ("gemv_mxu", "gemv_mxu", "mxu", 8),
    ("gemm", "gemm", "mxu", 9),
]
MODES = ["kernel", "end_to_end"]

# What a screening row reports. Each is a quantity G3 reconciles or a live
# readback; none is derived from a log.
METRICS = [
    "elapsed_ns",
    "retired_instructions",
    "fabric_physical_beats",
    "fabric_bank_conflicts",
    "fabric_bytes_per_cycle",
    "dma_chunks",
    "mxu_source_compute_cycles",
]


def extract(row: dict) -> dict:
    """Pull the screening metrics out of one result row."""
    hart = row["hart"]
    fabric = row["local_fabric"]
    requesters = fabric["requesters"]

    def maybe(value):
        return None if isinstance(value, dict) else value

    return {
        "elapsed_ns": maybe(hart["elapsed_ns"]),
        "retired_instructions": maybe(hart["retired_instructions"]),
        "fabric_physical_beats": sum(r["physical_beats"] for r in requesters),
        "fabric_bank_conflicts": sum(r["bank_conflicts"] for r in requesters),
        "fabric_bytes_per_cycle": maybe(fabric["achieved_bytes_per_cycle"]),
        "dma_chunks": row["dma"]["chunks"],
        "mxu_source_compute_cycles": maybe(row["mxu"]["source_compute_cycles"]),
    }


def run_point(args, workload, mode, overrides, config_id, result_path):
    """Run one configuration and return its row, or raise with the reason."""
    benchmark, image, impl, case = workload
    settings = dict(BASELINE)
    settings.update(overrides)

    command = [
        args.runner,
        "--elf", str(pathlib.Path(args.firmware) / image / f"neo_bench_{image}.elf"),
        "--benchmark", benchmark,
        "--impl", impl,
        "--case", str(case),
        "--mode", mode,
        "--seed", str(args.seed),
        "--timing", args.timing,
        "--build-type", args.build_type,
        "--source-revision-file", args.revision_file,
        "--config-id", config_id,
        "--result", str(result_path),
        "--quiet",
    ]
    if args.fault_inject_counter:
        command += ["--inject-counter", args.fault_inject_counter]
    for flag, value in settings.items():
        command += [flag, str(value)]

    completed = subprocess.run(command, capture_output=True, text=True)
    if completed.returncode != 0:
        raise SystemExit(
            f"screening point failed ({completed.returncode}): "
            f"{benchmark}/{mode} {config_id}\n{completed.stderr}"
        )
    row = json.loads(result_path.read_text())

    # A rejected run may not be summarised. `run_valid` covers the identity
    # gates and the G3 conservation identities alike.
    # Defensive rather than reachable today: the runner's exit code already
    # follows its failure list, so an invalid row arrives as a non-zero return
    # above. Kept because that coupling is the runner's contract, not this
    # tool's, and a screening that silently summarised a rejected row would be
    # the most expensive way to learn the contract had changed.
    if not row["run_valid"]:
        detail = "; ".join(f["detail"] for f in row["failures"])
        raise SystemExit(
            f"screening point produced an invalid row: "
            f"{benchmark}/{mode} {config_id}: {detail}"
        )
    if not row["correctness"]["passed"]:
        raise SystemExit(
            f"screening point disagreed with the golden: "
            f"{benchmark}/{mode} {config_id}"
        )
    verify_row(row, args, workload, mode, settings, config_id)
    return row


# The knob names the runner reports, by the flag that sets them, with the
# provenance each one must carry.
#
# Not "either is fine": four of these are read back from the instantiated
# component and one is not, and that difference is the whole value of the
# provenance field. Accepting `configured` everywhere would let a live knob
# regress into a value copied from the command line without the screening
# noticing — which is precisely the failure the readback exists to prevent.
#
# `dma_max_burst_bytes` is `configured` because the DMA exposes no accessor for
# it; D28 derives it from AXI width and burst beats, and WP3 is what makes it a
# readback.
READBACK = {
    "--bank-width-bits": ("local_bank_width_bits", "live_readback"),
    "--bank-count": ("local_bank_count", "live_readback"),
    "--pipeline-stages": ("fabric_pipeline_stages", "live_readback"),
    "--sram-capacity-bytes": ("sram_capacity_bytes", "live_readback"),
    "--dma-max-burst": ("dma_max_burst_bytes", "configured"),
}


def verify_row(row, args, workload, mode, settings, config_id):
    """Confirm the row describes the machine the point asked for.

    Without this the screening trusts its own command line. A runner that
    ignored a knob would produce a perfectly valid row for the baseline
    machine, the tool would record the requested value beside it, and the knob
    would be reported inert — a false negative that reads exactly like the
    genuine one this screening also produces.
    """
    benchmark, _image, impl, case = workload
    config = row["configuration"]
    identity = row["identity"]
    provenance = config["identity_provenance"]

    def require(what, actual, expected):
        if actual != expected:
            raise SystemExit(
                f"screening point {config_id} ({benchmark}/{mode}) reports "
                f"{what} = {actual!r}, but {expected!r} was requested")

    require("configuration.id", config["id"], config_id)
    require("benchmark", identity["benchmark"], benchmark)
    require("case_index", identity["case_index"], case)
    require("implementation", identity["implementation"], impl)
    require("mode", identity["mode"], mode)
    require("timing_mode", config["timing_mode"], args.timing)
    require("seed", identity["seed"], args.seed)
    require("build_type", identity["build_type"], args.build_type)

    # Every knob, not only the one being varied: the others must still hold
    # their baseline, or the point is not one factor away from it.
    for flag, (name, expected_provenance) in READBACK.items():
        require(name, config[name], settings[flag])
        if name not in provenance:
            raise SystemExit(
                f"screening point {config_id} reports {name} without saying "
                f"where the value came from")
        if provenance[name] != expected_provenance:
            raise SystemExit(
                f"screening point {config_id} reports {name} as "
                f"{provenance[name]!r}, expected {expected_provenance!r}. A "
                f"knob that stopped being read back would be screened from a "
                f"value copied out of this tool's own command line")


def summarise(rows, knobs, workloads):
    """Spread of each metric across one knob's values, per workload and mode.

    A separate function so `tests/test_screening_summary.py` can drive it with
    rows it constructs, including the cases a real run does not produce: a knob
    whose baseline point is missing, a metric that is `None` on some points,
    and a baseline of zero. Testing this through a simulation would take
    minutes and would not reach any of them.
    """
    findings = []
    for knob in knobs:
        for workload in workloads:
            for mode in MODES:
                points = [r for r in rows if r["knob"] == knob
                          and r["benchmark"] == workload[0]
                          and r["mode"] == mode]
                if not points:
                    continue
                base = next((p for p in points if p["is_baseline"]), None)
                for metric in METRICS:
                    values = [p[metric] for p in points
                              if p[metric] is not None]
                    if len(values) < 2 or base is None \
                            or base.get(metric) is None:
                        continue
                    low, high = min(values), max(values)
                    reference = base[metric]
                    # A zero baseline has no relative spread. Reporting one
                    # would mean dividing by it; reporting zero would claim
                    # the knob is inert when the absolute range may not be.
                    spread = None if reference == 0 \
                        else (high - low) / abs(reference)
                    findings.append({
                        "knob": knob,
                        "benchmark": workload[0],
                        "mode": mode,
                        "metric": metric,
                        "baseline": reference,
                        "min": low,
                        "max": high,
                        "relative_spread": spread,
                        "sensitive": high != low,
                    })
    return findings


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", required=True)
    parser.add_argument("--firmware", required=True,
                        help="directory holding the per-benchmark image trees")
    parser.add_argument("--revision-file", required=True)
    parser.add_argument("--build-type", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--seed", type=int, default=20260825)
    parser.add_argument("--timing", default="annotated",
                        choices=["annotated", "arbitrated"])
    parser.add_argument("--knob", action="append", default=None,
                        help="screen only these knobs; default is all")
    parser.add_argument("--benchmark", action="append", default=None,
                        help="screen only these benchmarks; default is all")
    parser.add_argument(
        "--fault-inject-counter", default=None,
        help="append --inject-counter to every point. Exists so a control can "
             "prove the screening aborts on a failing point rather than "
             "summarising it; there is no legitimate use.")
    args = parser.parse_args()

    out = pathlib.Path(args.out)
    raw = out / "results" / "raw"
    summary_dir = out / "results" / "summary"
    raw.mkdir(parents=True, exist_ok=True)
    summary_dir.mkdir(parents=True, exist_ok=True)

    # An unrecognised name is refused outright rather than filtered away.
    # Dropping it silently is worse than it sounds: `--knob local_bank_count
    # --knob no_such_knob` used to screen one knob and report the other as
    # absent, which reads exactly like a knob that was screened and found
    # inert.
    if args.knob is not None:
        unknown = sorted(set(args.knob) - set(KNOBS))
        if unknown:
            raise SystemExit(
                f"no known knob among {unknown}; known knobs are "
                f"{sorted(KNOBS)}")
    if args.benchmark is not None:
        unknown = sorted(set(args.benchmark) - {w[0] for w in WORKLOADS})
        if unknown:
            raise SystemExit(
                f"no known benchmark among {unknown}; known benchmarks are "
                f"{sorted(w[0] for w in WORKLOADS)}")

    knobs = {k: v for k, v in KNOBS.items()
             if args.knob is None or k in args.knob}
    workloads = [w for w in WORKLOADS
                 if args.benchmark is None or w[0] in args.benchmark]

    rows = []
    identities = []
    for workload in workloads:
        for mode in MODES:
            for knob, (flag, values) in knobs.items():
                for value in values:
                    config_id = f"screen_{knob}_{value}"
                    name = f"{workload[0]}_{mode}_{config_id}"
                    row = run_point(args, workload, mode, {flag: value},
                                    config_id, raw / f"{name}.json")
                    record = {
                        "benchmark": workload[0],
                        "mode": mode,
                        "case_index": workload[3],
                        "implementation": workload[2],
                        "knob": knob,
                        "value": value,
                        "is_baseline": value == BASELINE[flag],
                        "timing_mode": row["configuration"]["timing_mode"],
                        "config_id": config_id,
                    }
                    record.update(extract(row))
                    rows.append(record)
                    identities.append(row["identity"])

    csv_path = summary_dir / "screening.csv"
    with csv_path.open("w", newline="") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=["benchmark", "mode", "case_index", "implementation",
                        "knob", "value", "is_baseline", "timing_mode",
                        "config_id"] + METRICS)
        writer.writeheader()
        writer.writerows(rows)

    findings = summarise(rows, sorted(knobs), workloads)

    insensitive = sorted({f["knob"] for f in findings}
                         - {f["knob"] for f in findings if f["sensitive"]})
    sensitive = sorted({f["knob"] for f in findings if f["sensitive"]})
    # Provenance, so a summary lifted out of the build tree still says which
    # binaries produced it. The firmware hashes come from the rows rather than
    # from the file system: they identify the images that actually ran, mapped
    # to the benchmark each ran — an unlabelled list cannot say which image
    # produced which measurement, and one benchmark running against two images
    # inside a screening is a defect rather than a note.
    firmware = {}
    for record in identities:
        firmware.setdefault(record["benchmark"], set()).add(
            record["firmware_elf_sha256"])
    for benchmark, hashes in firmware.items():
        if len(hashes) != 1:
            raise SystemExit(
                f"benchmark {benchmark} ran against {len(hashes)} different "
                f"images during one screening: {sorted(hashes)}")
    firmware = {b: next(iter(h)) for b, h in sorted(firmware.items())}
    summary = {
        "schema": SCHEMA,
        "timing_mode": args.timing,
        "seed": args.seed,
        "build_type": args.build_type,
        "source_revision": sorted({r["source_revision"] for r in identities}),
        "firmware_elf_sha256": firmware,
        "workloads": [{"benchmark": w[0], "implementation": w[2],
                       "case_index": w[3]} for w in workloads],
        "modes": MODES,
        "baseline": BASELINE,
        "points": len(rows),
        "knobs_screened": sorted(knobs),
        "sensitive_knobs": sensitive,
        "insensitive_knobs": insensitive,
        "findings": findings,
        "limits": [
            "Screening, not a sweep: one knob varies at a time around the "
            "stated baseline, so an interaction between two knobs is outside "
            "what this can see.",
            "Every point passed its golden and its G3 conservation "
            "identities; a rejected row fails the screening rather than being "
            "averaged in.",
            f"Timing mode is '{args.timing}'. Under 'annotated' the local "
            "fabric never blocks, so no figure here may be quoted as bank "
            "contention or arbitration fairness (D16).",
            "No result here is a calibrated hardware figure: the model has no "
            "area, power or post-place-and-route frequency model.",
        ],
    }
    (summary_dir / "screening.json").write_text(
        json.dumps(summary, indent=2) + "\n")

    print(f"screening: {len(rows)} points, {len(findings)} knob/metric "
          f"observations, timing={args.timing}")
    if insensitive:
        print(f"insensitive knobs: {', '.join(insensitive)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
