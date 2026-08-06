#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# D3-D6 mutation controls for the drainable benchmark, dashboard and sweep.
# Private source copies are built under /tmp; the working tree is never
# mutated. A control is detected only when the mutant builds, the intended
# regression runs, and it fails with its defect-specific marker.

set -u -o pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
platform_dir="$(cd "${script_dir}/.." && pwd)"
repo_root="$(cd "${platform_dir}/../.." && pwd)"

export CC="${CC:-/usr/bin/gcc}"
export CXX="${CXX:-/usr/bin/g++}"
export PATH="/usr/bin:/bin:${PATH}"

systemc_home="${FLOO_SYSTEMC_HOME:-/opt/systemc-2.3.4}"
systemc_lib_dir="${systemc_home}/lib64"
if [[ ! -f "${systemc_lib_dir}/libsystemc.so" ]]; then
    systemc_lib_dir="${systemc_home}/lib"
fi

benchmark_source="${platform_dir}/src/noc_benchmark.cpp"
benchmark_regression="${script_dir}/run_metrics_regression.sh"
interconnect_library="${NOC_INTERCONNECT_LIBRARY:-${repo_root}/build/components/floo_noc_model/libnoc_interconnect.a}"
dashboard="${NOC_DASHBOARD_TOOL:-${repo_root}/tools/noc_dashboard.py}"
sweep="${NOC_SWEEP_TOOL:-${repo_root}/tools/noc_sweep.py}"
sweep_test="${script_dir}/test_noc_sweep.py"

fail() { echo "FAIL: $*" >&2; exit 1; }

[[ -f "${benchmark_source}" ]] || fail "noc_benchmark source not found"
[[ -x "${benchmark_regression}" ]] || fail "metrics regression is not executable"
[[ -f "${interconnect_library}" ]] || fail "noc_interconnect library not found"
[[ -f "${systemc_home}/include/systemc" ]] || fail "SystemC headers not found"
[[ -f "${systemc_lib_dir}/libsystemc.so" ]] || fail "SystemC library not found"
[[ -f "${dashboard}" ]] || fail "dashboard tool not found"
[[ -f "${sweep}" ]] || fail "sweep tool not found"

work_dir="$(mktemp -d "${TMPDIR:-/tmp}/noc_metrics_negative.XXXXXX")"
detected=0
missed=0
built_binary=""

echo "FlooNoC metrics negative controls"
echo "  host CC:  $(${CC} -dumpfullversion)"
echo "  host CXX: $(${CXX} --version | head -n 1)"
echo "  evidence: ${work_dir}"

apply_mutation()
{
    local source="$1"
    local target="$2"
    local needle="$3"
    local replacement="$4"
    NEEDLE="${needle}" REPLACEMENT="${replacement}" \
        python3 - "${source}" "${target}" <<'PY'
import os
import pathlib
import sys

source = pathlib.Path(sys.argv[1])
target = pathlib.Path(sys.argv[2])
text = source.read_text(encoding="utf-8")
needle = os.environ["NEEDLE"]
replacement = os.environ["REPLACEMENT"]
if text.count(needle) != 1:
    print(
        f"mutation point count is {text.count(needle)}, expected exactly one",
        file=sys.stderr,
    )
    raise SystemExit(2)
target.write_text(text.replace(needle, replacement, 1), encoding="utf-8")
PY
}

build_benchmark_mutant()
{
    local name="$1"
    local needle="$2"
    local replacement="$3"
    local source="${work_dir}/${name}.cpp"
    local binary="${work_dir}/${name}"
    local build_log="${work_dir}/${name}.build.log"

    if ! apply_mutation \
            "${benchmark_source}" "${source}" "${needle}" "${replacement}" \
            >"${build_log}" 2>&1; then
        echo "MISSED ${name}: injection point is missing or ambiguous" >&2
        missed=$((missed + 1))
        return 1
    fi
    if ! "${CXX}" -std=c++17 -DNOC_SOC_BUILD_TYPE=\"negative-control\" \
            -I"${repo_root}/components/floo_noc_model/include" \
            -I"${systemc_home}/include" \
            "${source}" "${interconnect_library}" \
            -L"${systemc_lib_dir}" -Wl,-rpath,"${systemc_lib_dir}" \
            -lsystemc -pthread -o "${binary}" \
            >>"${build_log}" 2>&1; then
        echo "MISSED ${name}: mutated benchmark does not build" >&2
        missed=$((missed + 1))
        return 1
    fi
    built_binary="${binary}"
}

detect_regression_mutant()
{
    local name="$1"
    local needle="$2"
    local replacement="$3"
    local expected="$4"
    build_benchmark_mutant \
        "${name}" "${needle}" "${replacement}" || return
    local binary="${built_binary}"
    local log="${work_dir}/${name}.test.log"

    NOC_BENCHMARK_BIN="${binary}" NOC_DASHBOARD_TOOL="${dashboard}" \
        "${benchmark_regression}" >"${log}" 2>&1
    local status=$?
    if [[ ${status} -eq 0 ]]; then
        echo "MISSED ${name}: metrics regression still passes" >&2
        missed=$((missed + 1))
    elif [[ ${status} -eq 124 || ${status} -eq 137 ]]; then
        echo "MISSED ${name}: regression timed out" >&2
        missed=$((missed + 1))
    elif ! grep -Fq -- "${expected}" "${log}"; then
        echo "MISSED ${name}: failed for the wrong reason; expected '${expected}'" >&2
        missed=$((missed + 1))
    else
        echo "detected ${name}"
        detected=$((detected + 1))
    fi
}

detect_direct_mutant()
{
    local name="$1"
    local needle="$2"
    local replacement="$3"
    local expected="$4"
    build_benchmark_mutant \
        "${name}" "${needle}" "${replacement}" || return
    local binary="${built_binary}"
    local log="${work_dir}/${name}.test.log"

    timeout --kill-after=5 60 "${binary}" \
        --workload quiet --topology 2x2 --transactions 2 \
        --warmup-transactions 1 --seed 17 \
        --noc-metrics "${work_dir}/${name}.json" >"${log}" 2>&1
    local status=$?
    if [[ ${status} -eq 0 ]]; then
        echo "MISSED ${name}: mutant produced a complete measurement" >&2
        missed=$((missed + 1))
    elif [[ ${status} -eq 124 || ${status} -eq 137 ]]; then
        echo "MISSED ${name}: mutant timed out" >&2
        missed=$((missed + 1))
    elif ! grep -Fq -- "${expected}" "${log}"; then
        echo "MISSED ${name}: failed for the wrong reason; expected '${expected}'" >&2
        missed=$((missed + 1))
    else
        echo "detected ${name}"
        detected=$((detected + 1))
    fi
}

detect_sweep_mutant()
{
    local name="$1"
    local needle="$2"
    local replacement="$3"
    local expected="$4"
    local mutant="${work_dir}/${name}.py"
    local log="${work_dir}/${name}.test.log"

    if ! apply_mutation "${sweep}" "${mutant}" "${needle}" "${replacement}" \
            >"${work_dir}/${name}.build.log" 2>&1; then
        echo "MISSED ${name}: sweep injection point is missing or ambiguous" >&2
        missed=$((missed + 1))
        return
    fi
    python3 "${sweep_test}" "${mutant}" >"${log}" 2>&1
    local status=$?
    if [[ ${status} -eq 0 ]]; then
        echo "MISSED ${name}: sweep selection test still passes" >&2
        missed=$((missed + 1))
    elif ! grep -Fq -- "${expected}" "${log}"; then
        echo "MISSED ${name}: failed for the wrong reason; expected '${expected}'" >&2
        missed=$((missed + 1))
    else
        echo "detected ${name}"
        detected=$((detected + 1))
    fi
}

# D3 measurement-window and attribution controls.
detect_regression_mutant \
    "warmup-counters-not-reset" \
    '        noc_.reset_detailed_counters();' \
    '' \
    'warm-up traffic leaked into measured physical_meshes'

detect_regression_mutant \
    "completed-transaction-omitted-from-manager-bucket" \
    '                if (completion.port < manager_stats_.size()) {' \
    '                if (completion.port < manager_stats_.size()
                    && completion.port != 0) {' \
    'noc_benchmark: manager buckets do not reconcile'

# D5: each workload must have a behavioural control, not only a smoke run.
detect_regression_mutant \
    "quiet-gap-collapsed" \
    '            result.workload == "quiet" ? 100u : 0u;' \
    '            result.workload == "quiet" ? 0u : 0u;' \
    'quiet and hotspot throughput use clock-active rather than modeled cycles'

detect_regression_mutant \
    "hotspot-gap-expanded" \
    '            result.workload == "quiet" ? 100u : 0u;' \
    '            result.workload == "quiet" ? 100u : 100u;' \
    'quiet and hotspot throughput use clock-active rather than modeled cycles'

detect_regression_mutant \
    "contention-manager-removed" \
    '    if (workload == "contention") {
        return 2;
    }' \
    '    if (workload == "contention") {
        return 1;
    }' \
    'contention: offered/delivered traffic does not reconcile'

detect_regression_mutant \
    "fairness-manager-removed" \
    '    if (workload == "fairness") {
        return 3;
    }' \
    '    if (workload == "fairness") {
        return 2;
    }' \
    'fairness: offered/delivered traffic does not reconcile'

# D4 source semantics and D3 timing-mode eligibility.
detect_regression_mutant \
    "measured-source-mislabeled-analytic" \
    '        out, static_cast<double>(metrics.transactions()), "transactions", "M",' \
    '        out, static_cast<double>(metrics.transactions()), "transactions", "A",' \
    'metric source'

detect_direct_mutant \
    "fast-backend-used-for-measurement" \
    '              noc_interconnect::timing_mode::detailed)' \
    '              noc_interconnect::timing_mode::fast)' \
    'router counters are unavailable in fast mode'

# D6 winner eligibility: failed and non-drained rows are independent guards.
detect_sweep_mutant \
    "sweep-accepts-failed-row" \
    '        if row.get("status") == "measured"' \
    '        if True' \
    'selected failed/incomplete row'

detect_sweep_mutant \
    "sweep-accepts-undrained-row" \
    '        and row.get("drained") is True' \
    '        and True' \
    'selected failed/incomplete row'

echo
echo "metrics negative controls: ${detected} detected, ${missed} missed"
echo "evidence kept under ${work_dir}"
if [[ ${missed} -ne 0 || ${detected} -ne 10 ]]; then
    fail "every metrics control must build, run and fail for its own reason"
fi
echo "FlooNoC metrics negative controls PASS"
