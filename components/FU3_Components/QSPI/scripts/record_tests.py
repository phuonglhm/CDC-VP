#!/usr/bin/env python3
"""Record actual QSPI CTest runs; never infer success from existing artifacts."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
from datetime import datetime, timezone


def command(args):
    return subprocess.run(args, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("normal_build", type=Path)
    parser.add_argument("ubsan_build", type=Path)
    parser.add_argument("--output-dir", type=Path,
                        help="Report directory (default: NORMAL_BUILD/test-reports)")
    parser.add_argument("--spec-pdf", type=Path,
                        help="Verify the supplied PDF matches the pinned spec SHA-256 before running tests")
    options = parser.parse_args()
    component = Path(__file__).resolve().parents[1]
    reports = (options.output_dir or options.normal_build / "test-reports").resolve()
    reports.mkdir(parents=True, exist_ok=True)
    baseline_path = component / "spec-baseline.json"
    baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
    specification = dict(baseline["specification"])
    specification["pdf_hash_verified"] = False
    if options.spec_pdf:
        actual = hashlib.sha256(options.spec_pdf.read_bytes()).hexdigest()
        if actual != specification["sha256"]:
            raise SystemExit(f"Spec PDF does not match pinned {specification['version']} "
                             f"({specification['date']}): expected {specification['sha256']}, got {actual}")
        specification["pdf_hash_verified"] = True
    expected = {"fu3_qspi_" + s for s in (
        "registers protocol triggers indirect fifo polling timeout dma xip phases "
        "errors reset clock concurrency random components threshold_words spec_timing bus "
        "flash_commands flash_program flash_erase flash_protocol flash_reset flash_image flash_sioo flash_integrity "
        "p0_triggers p0_dlr p0_fifo p0_irq_w1c p0_active_reset p0_bus_errors "
        "p1_polling p1_dma p1_domains hds_cache hds_control hds_dma_clock hds_flags hds_timeout hds_abort "
        "hds_frame_matrix hds_dividers hds_parameters").split()}
    expected.update("fu3_qspi_p1_parameters_" + depth for depth in ("4", "8", "16", "32", "4x32", "8x32"))
    expected.update("fu3_qspi_hds_cache_parameters_" + str(n) for n in (1, 2, 3, 4, 8, 16))
    groups = {**baseline["functional_groups"], **baseline["support_groups"]}
    mapped = {"fu3_qspi_" + test for group in groups.values() for test in group["tests"]}
    if mapped != expected or any(not group["tests"] for group in groups.values()):
        raise SystemExit(f"Incomplete spec-to-test mapping: {mapped ^ expected}")
    summary = {"captured_at_utc": datetime.now(timezone.utc).isoformat(),
               "specification": specification, "verification_scope": baseline["verification_scope"],
               "rtl_only_checks": baseline["rtl_only_checks"], "runs": []}
    passed = True
    for kind, directory in zip(("debug", "ubsan"), (options.normal_build, options.ubsan_build)):
        build = str(Path(directory).resolve())
        cache = (Path(build) / "CMakeCache.txt").read_text()
        flags = re.search(r"^CMAKE_CXX_FLAGS:STRING=(.*)$", cache, re.M).group(1)
        if kind == "ubsan" and not all(flag in flags for flag in (
                "-fsanitize=undefined", "-fno-sanitize-recover=undefined", "-Werror")):
            raise SystemExit("UBSan build is missing required sanitizer/warning flags")
        listing = command(["ctest", "--test-dir", build, "--show-only=json-v1"])
        if listing.returncode:
            raise SystemExit(listing.stdout)
        registered = {t["name"] for t in json.loads(listing.stdout)["tests"]}
        if registered != expected:
            raise SystemExit(f"Unexpected test set in {build}: {registered ^ expected}")
        args = ["ctest", "--test-dir", build, "-V", "--output-on-failure"]
        result = command(args)
        name = "test-results.txt" if kind == "debug" else "ubsan-results.txt"
        (reports / name).write_text("Command: " + " ".join(args) + "\n\n" + result.stdout, encoding="utf-8")
        records = {}
        for scenario, count, time in re.findall(r"PASS (\w+) assertions=(\d+) time=([^\r\n]+)", result.stdout):
            records["fu3_qspi_" + scenario] = {"status": "passed", "assertions": int(count), "simulation_time": time}
        ok = result.returncode == 0 and set(records) == expected
        feature_results = {
            name: {"source": group["source"], "tests": ["fu3_qspi_" + t for t in group["tests"]],
                   "passed": all("fu3_qspi_" + t in records for t in group["tests"])}
            for name, group in groups.items()
        }
        passed = passed and ok
        summary["runs"].append({"configuration": kind, "build_directory": build, "cxx_flags": flags,
            "exit_code": result.returncode, "passed": ok, "test_count": len(expected),
            "assertions": sum(r["assertions"] for r in records.values()),
            "tests": records, "feature_results": feature_results, "transcript": name})
        print(f"{kind}: {'PASS' if ok else 'FAIL'}, {len(records)}/{len(expected)} tests")
    repo = component.parents[2]
    roots = [component / "include", component / "src", component / "tests", component / "scripts",
             component.parent / "bus/include", component.parent / "bus/src"]
    paths = [component / "CMakeLists.txt", component.parent / "bus/CMakeLists.txt", baseline_path,
             component / "SPEC_VERSION.md", component / "README.md"]
    paths.extend((component / "docs").glob("*.md"))
    for root in roots:
        paths.extend(p for p in root.rglob("*") if p.suffix in {".h", ".cpp", ".py"})
    summary["source_sha256"] = {p.relative_to(repo).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                                 for p in sorted(paths)}
    summary["toolchain"] = {name: command([name, "--version"]).stdout.splitlines()[0]
                            for name in ("cmake", "ctest", "c++")}
    summary["systemc"] = "2.3.4 (see runtime banner in transcripts)"
    summary["passed"] = passed
    (reports / "test-summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(f"Reports: {reports}")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
