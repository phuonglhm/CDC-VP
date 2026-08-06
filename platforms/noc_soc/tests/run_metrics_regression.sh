#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# D3-D5: bounded measurement-window and synthetic-workload regression.
#
# This runner uses the drainable production-wrapper benchmark.  It does not use
# the firmware platform because an executing CPU cannot be stopped at the end
# of an arbitrary measurement window, and therefore cannot satisfy the drain
# and conservation gate required before a DSE row is selectable.

set -u -o pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
platform_dir="$(cd "${script_dir}/.." && pwd)"
repo_root="$(cd "${platform_dir}/../.." && pwd)"

export CC="${CC:-/usr/bin/gcc}"
export CXX="${CXX:-/usr/bin/g++}"
export PATH="/usr/bin:/bin:${PATH}"

fail() { echo "FAIL: $*" >&2; exit 1; }

benchmark="${NOC_BENCHMARK_BIN:-${repo_root}/build/platforms/noc_soc/noc_benchmark}"
dashboard="${NOC_DASHBOARD_TOOL:-${repo_root}/tools/noc_dashboard.py}"
schema="${platform_dir}/metrics/metrics_schema_v1.json"
[[ -x "${benchmark}" ]] || fail "noc_benchmark not found; set NOC_BENCHMARK_BIN"
[[ -f "${dashboard}" ]] || fail "noc_dashboard.py not found"

work_dir="$(mktemp -d "${TMPDIR:-/tmp}/noc_metrics_regression.XXXXXX")"

echo "FlooNoC metrics regression"
echo "  host CC:  $(${CC} -dumpfullversion)"
echo "  host CXX: $(${CXX} --version | head -n 1)"
echo "  benchmark: ${benchmark}"
echo "  evidence:  ${work_dir}"

run_case() {
    local name="$1"
    shift
    timeout --kill-after=5 60 \
        "${benchmark}" "$@" \
        --noc-metrics "${work_dir}/${name}.json" \
        >"${work_dir}/${name}.log" 2>&1
    local status=$?
    if [[ ${status} -ne 0 ]]; then
        cat "${work_dir}/${name}.log" >&2
        fail "${name} benchmark exited ${status}"
    fi
    grep -Fq "noc_benchmark PASS" "${work_dir}/${name}.log" \
        || fail "${name} did not print its acceptance marker"
    python3 "${dashboard}" "${work_dir}/${name}.json" \
        --output "${work_dir}/${name}.txt" \
        || fail "${name} JSON failed semantic/dashboard validation"
}

common=(
    --topology 4x4
    --transactions 32
    --warmup-transactions 2
    --seed 17
)

run_case quiet --workload quiet "${common[@]}"
run_case hotspot --workload hotspot "${common[@]}"
run_case hotspot_warmup4 \
    --workload hotspot --topology 4x4 --transactions 32 \
    --warmup-transactions 4 --seed 17
run_case contention --workload contention "${common[@]}"
run_case fairness --workload fairness "${common[@]}"
run_case saturation_low \
    --workload fairness "${common[@]}" \
    --transactions 48 --workers-per-manager 1 --injection-gap-cycles 0
run_case saturation_high \
    --workload fairness "${common[@]}" \
    --transactions 48 --workers-per-manager 8 --injection-gap-cycles 0

# The same configuration and seed must reproduce every modeled field.  The UTC
# generation timestamp is archival metadata rather than modeled evidence and is
# the only field removed before comparison.
run_case deterministic_repeat --workload contention "${common[@]}"

python3 - "${work_dir}" "${schema}" <<'PY'
import copy
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
schema_path = pathlib.Path(sys.argv[2])

try:
    import jsonschema
except ImportError:
    jsonschema = None


def load(name):
    with (root / f"{name}.json").open(encoding="utf-8") as source:
        result = json.load(source)
    if jsonschema is not None:
        with schema_path.open(encoding="utf-8") as source:
            jsonschema.validate(instance=result, schema=json.load(source))
    return result


def value(report, name):
    return report["derived_metrics"][name]["value"]


reports = {
    name: load(name)
    for name in (
        "quiet",
        "hotspot",
        "hotspot_warmup4",
        "contention",
        "fairness",
        "saturation_low",
        "saturation_high",
        "deterministic_repeat",
    )
}

expected_managers = {
    "quiet": 1,
    "hotspot": 1,
    "hotspot_warmup4": 1,
    "contention": 2,
    "fairness": 3,
    "saturation_low": 3,
    "saturation_high": 3,
}
for name, managers in expected_managers.items():
    report = reports[name]
    window = report["measurement_window"]
    workload = report["workload"]
    global_bucket = report["transaction_metrics"]["global"]
    workers = workload.get("workers_per_manager", 1)
    expected = managers * workers * 48 \
        if name.startswith("saturation_") else managers * workers * 32
    offered = workload["offered_transactions"]
    delivered = global_bucket["transactions"]["value"]
    assert window["warmup_cycles"] > 0, f"{name}: warm-up was not observed"
    assert window["start_cycle"] == window["warmup_cycles"], \
        f"{name}: measurement did not start at the warm-up boundary"
    assert window["end_cycle"] > window["start_cycle"], \
        f"{name}: empty measurement window"
    assert window["counted_cycles"] == \
        window["end_cycle"] - window["start_cycle"], \
        f"{name}: counted cycles use the wrong denominator"
    assert window["drained"] is True, f"{name}: network did not drain"
    assert offered == delivered == expected, \
        f"{name}: offered/delivered traffic does not reconcile"
    accounting = report["traffic_accounting"]
    assert accounting["offered_transactions"]["value"] == delivered
    assert accounting["delivered_transactions"]["value"] == delivered

quiet = reports["quiet"]
hotspot = reports["hotspot"]
assert value(quiet, "transaction_throughput_mtrans_s") < \
    value(hotspot, "transaction_throughput_mtrans_s"), \
    "quiet and hotspot throughput use clock-active rather than modeled cycles"
assert value(quiet, "request_peak_link_utilisation") < \
    value(hotspot, "request_peak_link_utilisation"), \
    "quiet and hotspot link utilisation use the wrong denominator"

warm = reports["hotspot"]
cold = reports["hotspot_warmup4"]
for field in ("transaction_metrics", "physical_meshes", "derived_metrics",
              "traffic_accounting"):
    assert warm[field] == cold[field], \
        f"warm-up traffic leaked into measured {field}"

fairness = reports["fairness"]
assert 0.0 < value(fairness, "jain_fairness") <= 1.0, \
    "Jain fairness must remain in its defined range"

low = reports["saturation_low"]
high = reports["saturation_high"]
low_latency = \
    low["transaction_metrics"]["global"]["latency_cycles"]["p99"]["value"]
high_latency = \
    high["transaction_metrics"]["global"]["latency_cycles"]["p99"]["value"]
low_throughput = value(low, "transaction_throughput_mtrans_s")
high_throughput = value(high, "transaction_throughput_mtrans_s")
assert high_latency > low_latency, \
    "high offered concurrency did not grow P99 latency"
assert high_throughput > low_throughput, \
    "higher offered concurrency did not raise delivered throughput"
assert high_throughput < low_throughput * 8.0, \
    "saturation case scaled linearly; no reproducible knee was exercised"

first = copy.deepcopy(reports["contention"])
second = copy.deepcopy(reports["deterministic_repeat"])
first["provenance"].pop("generated_utc", None)
second["provenance"].pop("generated_utc", None)
assert first == second, \
    "same seed/config did not reproduce identical modeled counters"

print("PASS: D3 measurement window, drain, conservation and determinism")
print("PASS: D5 quiet/hotspot/contention/fairness and saturation knee")
if jsonschema is None:
    print("WARNING: jsonschema unavailable; semantic validation still passed")
else:
    print("PASS: all benchmark artifacts validate against metrics schema v1")
PY
status=$?
[[ ${status} -eq 0 ]] || fail "metrics contract checks failed"

for marker in \
    "[1] INPUT" \
    "[3] LATENCY DASHBOARD" \
    "[4] MANAGER -> TARGET TRAFFIC" \
    "[5] HOTSPOTS" \
    "Drained" \
    "yes" \
    "Jain fairness"; do
    grep -Fq "${marker}" "${work_dir}/fairness.txt" \
        || fail "dashboard is missing '${marker}'"
done

echo "FlooNoC metrics regression PASS"
