#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Step 12.7: record a NoC and RTOS measurement baseline.
#
# This is a **measurement artifact, not a gate**. It checks that every required
# number was produced and is well formed; it does not compare any of them
# against a threshold, and it must not grow one until repeatability across clean
# runs and host-load variation has actually been measured. A threshold invented
# from a single sample is a future false failure with a confident-looking
# number attached to it.
#
# It runs level 125 — level 124 plus an RTOS tick hook — in `--noc-timing
# detailed`. Detailed mode is the only mode that may be quoted here: fast mode
# bypasses the cycle-stepped mesh entirely, so its latencies are no-contention
# estimates and its contention buckets are empty by construction.
#
# The archived artifact carries its own provenance: git revision, firmware
# hash, host compiler, build type, modeled window, network clock, floorplan and
# timing mode. A latency without those is not evidence, because the next reader
# cannot tell what it was a latency *of*.
#
# Exit codes: 0 pass, 1 fail, 77 skipped when the RISC-V toolchain is absent.

set -u -o pipefail

readonly SKIP=77

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
platform_dir="$(cd "${script_dir}/.." && pwd)"
repo_root="$(cd "${platform_dir}/../.." && pwd)"

export CC="${CC:-/usr/bin/gcc}"
export CXX="${CXX:-/usr/bin/g++}"
export PATH="/usr/bin:/bin:${PATH}"

# Long enough for the level-125 workload to reach its final PASS, which is when
# the tick-jitter summary is printed. It is a bound against a hang, not a
# performance assertion.
readonly modeled_us="${BASELINE_SIM_US:-45000}"
readonly host_timeout="${BASELINE_HOST_TIMEOUT:-1800}"
readonly firmware_level=125

fail() { echo "FAIL: $*" >&2; exit 1; }

if ! command -v riscv-none-elf-gcc >/dev/null 2>&1; then
    if [[ -x /opt/toolchains/riscv-none-elf/bin/riscv-none-elf-gcc ]]; then
        export PATH="/opt/toolchains/riscv-none-elf/bin:${PATH}"
    else
        echo "SKIP: riscv-none-elf-gcc not found; cannot build the firmware." >&2
        exit "${SKIP}"
    fi
fi

noc_soc_bin="${NOC_SOC_BIN:-}"
if [[ -z "${noc_soc_bin}" ]]; then
    for candidate in \
        "${repo_root}/build/platforms/noc_soc/noc_soc" \
        "${platform_dir}/noc_soc"; do
        [[ -x "${candidate}" ]] && { noc_soc_bin="${candidate}"; break; }
    done
fi
[[ -x "${noc_soc_bin:-}" ]] || fail "noc_soc binary not found; set NOC_SOC_BIN"

# Archived where the other platform evidence lives, so a reviewer looking for
# "what did this platform measure" finds it beside the regressions rather than
# in someone's scratch directory.
evidence_dir="${BASELINE_DIR:-${platform_dir}/evidence}"
mkdir -p "${evidence_dir}"
work_dir="$(mktemp -d "${TMPDIR:-/tmp}/noc_soc_baseline.XXXXXX")"

elf="${work_dir}/freertos_noc_soc_step_${firmware_level}.elf"
build_log="${work_dir}/firmware_build.log"
run_log="${work_dir}/noc_soc_detailed.log"
metrics_raw="${work_dir}/noc_metrics.raw.json"
dashboard_raw="${work_dir}/noc_dashboard.txt"
baseline="${evidence_dir}/noc_baseline.txt"
metrics="${evidence_dir}/noc_metrics.json"
dashboard="${evidence_dir}/noc_dashboard.txt"
source_patch="${evidence_dir}/noc_baseline.source.patch"

echo "noc_soc measurement baseline"
echo "  binary:   ${noc_soc_bin}"
echo "  window:   ${modeled_us} us, detailed timing"
echo "  evidence: ${baseline}"

if ! make -C "${repo_root}/fw/freertos_noc_soc" \
        NOC_STEP="${firmware_level}" \
        TARGET="${elf}" \
        DISASSEMBLY="${work_dir}/freertos_noc_soc_step_${firmware_level}.dis" \
        check >"${build_log}" 2>&1; then
    cat "${build_log}" >&2
    fail "level-${firmware_level} firmware build or ELF contract failed"
fi

started="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
timeout --kill-after=30 "${host_timeout}" \
    "${noc_soc_bin}" \
    --mode firmware \
    --noc-timing detailed \
    --noc-baseline \
    --noc-metrics "${metrics_raw}" \
    --fw "${elf}" \
    --sim-us "${modeled_us}" >"${run_log}" 2>&1
status=$?

if [[ ${status} -eq 124 || ${status} -eq 137 ]]; then
    tail -40 "${run_log}" >&2
    fail "the baseline run exceeded its ${host_timeout}s host bound"
fi
[[ ${status} -eq 0 ]] || {
    tail -40 "${run_log}" >&2
    fail "the baseline run exited ${status}"
}

# The workload must have completed, or the numbers describe a truncated run.
for marker in \
    "FreeRTOS NoC boot" \
    "CLINT tick OK" \
    "PLIC TIMER0 OK irqs=3" \
    "DMA NoC PASS" \
    "FreeRTOS NoC concurrent OK checkpoints=" \
    "FreeRTOS NoC PASS"; do
    grep -Fq "${marker}" "${run_log}" \
        || fail "the workload did not complete: '${marker}' is missing"
done
for forbidden in "FAIL" "FATAL" "Taking trap" "[PC] trapped"; do
    grep -Fq "${forbidden}" "${run_log}" \
        && fail "the baseline run reported '${forbidden}'"
done

# Completeness, not thresholds: every metric section 11 of the roadmap asks for
# must be present and must carry a number.
require_metric() {
    local label="$1" pattern="$2"
    grep -Eq "${pattern}" "${run_log}" \
        || { tail -40 "${run_log}" >&2
             fail "the baseline is missing its ${label} measurement"; }
}

readonly unsigned_decimal='[0-9]+([.][0-9]+)?'
readonly signed_decimal='-?[0-9]+([.][0-9]+)?'

# The report must remain self-describing. These are schema checks, not
# performance thresholds: changing the window or floorplan is allowed, but
# silently dropping that provenance would make the archived numbers unusable.
require_metric "modeled window" \
    '^ +modeled window: +[0-9]+([.][0-9]+)? us$'
require_metric "network clock" \
    '^ +network clock: +[0-9]+([.][0-9]+)? (fs|ps|ns|us) per cycle$'
require_metric "floorplan" \
    '^ +floorplan: +[0-9]+x[0-9]+ mesh; cpu [0-9]+,[0-9]+ dma [0-9]+,[0-9]+ ram [0-9]+,[0-9]+ plic [0-9]+,[0-9]+$'
require_metric "platform build type" '^ +build type: +[^[:space:]].*$'
require_metric "level-125 firmware identity" \
    '^ +firmware: +.*freertos_noc_soc_step_125[.]elf$'

require_metric "PLIC claim latency" \
    "^ +PLIC claim .*: n=[1-9][0-9]* min=[0-9]+ mean=${unsigned_decimal} max=[0-9]+ cycles$"
require_metric "PLIC complete latency" \
    "^ +PLIC complete .*: n=[1-9][0-9]* min=[0-9]+ mean=${unsigned_decimal} max=[0-9]+ cycles$"
require_metric "RAM latency with the DMA idle" \
    "^ +RAM, DMA idle +: n=[1-9][0-9]* min=[0-9]+ mean=${unsigned_decimal} max=[0-9]+ cycles$"
require_metric "RAM latency with the DMA active" \
    "^ +RAM, DMA active +: n=[1-9][0-9]* min=[0-9]+ mean=${unsigned_decimal} max=[0-9]+ cycles$"
require_metric "other CPU MMIO latency" \
    "^ +other CPU MMIO +: n=[1-9][0-9]* min=[0-9]+ mean=${unsigned_decimal} max=[0-9]+ cycles$"
require_metric "contention delta" \
    "^ +contention delta \\(mean RAM, active - idle\\): ${signed_decimal} cycles$"
require_metric "DMA manager latency" \
    "^ +DMA-issued transactions +: n=[1-9][0-9]* min=[0-9]+ mean=${unsigned_decimal} max=[0-9]+ cycles$"
require_metric "DMA channel-start-to-interrupt time" \
    '^ +channel start to completion interrupt: n=[1-9][0-9]* min=[0-9]+ (fs|ps|ns|us|ms|s) mean=[0-9]+ (fs|ps|ns|us|ms|s) max=[0-9]+ (fs|ps|ns|us|ms|s)$'
require_metric "CPU peak outstanding" \
    '^ +cpu: peak outstanding [1-9][0-9]*$'
require_metric "DMA peak outstanding" \
    '^ +dma: peak outstanding [1-9][0-9]*$'
require_metric "probe peak outstanding" \
    '^ +probe: peak outstanding [0-9]+$'
require_metric "completed transactions and total latency" \
    '^ +all managers: [1-9][0-9]* completed transactions, [1-9][0-9]* total network cycles$'
require_metric "cycle-stepped mesh activity" \
    '^ +mesh clocked [1-9][0-9]* cycles$'
require_metric "clock-gate transitions" \
    '^ +clock-gate transitions [1-9][0-9]*$'
require_metric "mesh-quiescent/wrapper-busy cycles" \
    '^ +mesh quiescent while wrapper busy [1-9][0-9]* cycles$'
require_metric "request-mesh production counters" \
    '^ +request mesh *: [1-9][0-9]* accepted flits, [1-9][0-9]* packets, [0-9]+ output stall cycles$'
require_metric "response-mesh production counters" \
    '^ +response mesh *: [1-9][0-9]* accepted flits, [1-9][0-9]* packets, [0-9]+ output stall cycles$'
require_metric "request-mesh peak link" \
    '^ +peak directed-link utilisation [0-9]+([.][0-9]+)?% at \([0-9]+,[0-9]+\) (North|East|South|West); peak stall ratio [0-9]+([.][0-9]+)?%$'
require_metric "input/output FIFO high-water" \
    '^ +FIFO high-water input=[0-9]+ output=[0-9]+$'
require_metric "metrics JSON publication" \
    "^ +metrics JSON: ${metrics_raw//\//\\/}$"
require_metric "configured RTOS tick period" \
    '^RTOS tick period us=[1-9][0-9]*$'
require_metric "RTOS tick sample count" \
    '^RTOS tick samples=[1-9][0-9]*$'
require_metric "RTOS tick interval minimum" \
    '^RTOS tick interval min us=[0-9]+$'
require_metric "RTOS tick interval maximum" \
    '^RTOS tick interval max us=[0-9]+$'
require_metric "RTOS tick maximum deviation" \
    '^RTOS tick deviation max us=[0-9]+$'

# Detailed mode must actually have been used; the fast-mode warning appearing
# here would mean the artifact is estimates wearing a measurement's label.
grep -Fq "timing mode:      detailed cycle-stepped" "${run_log}" \
    || fail "the baseline was not produced in detailed mode"
grep -Fq "fast mode bypasses" "${run_log}" \
    && fail "the baseline log carries the fast-mode warning"

[[ -s "${metrics_raw}" ]] || fail "the simulator did not publish metrics JSON"
python3 "${repo_root}/tools/noc_dashboard.py" \
    "${metrics_raw}" --output "${dashboard_raw}" \
    || fail "the metrics JSON did not satisfy the dashboard contract"
[[ -s "${dashboard_raw}" ]] || fail "the dashboard renderer produced no report"

# Full JSON-Schema validation is used when the already-installed Python module
# is available.  Rendering above remains dependency-free and always runs.
if python3 -c 'import jsonschema' >/dev/null 2>&1; then
    python3 - "${metrics_raw}" \
        "${platform_dir}/metrics/metrics_schema_v1.json" <<'PY'
import json
import sys
import jsonschema

with open(sys.argv[1], encoding="utf-8") as source:
    instance = json.load(source)
with open(sys.argv[2], encoding="utf-8") as source:
    schema = json.load(source)
jsonschema.validate(instance=instance, schema=schema)
PY
    [[ $? -eq 0 ]] || fail "metrics JSON failed schema v1 validation"
else
    echo "WARNING: python jsonschema is unavailable; semantic renderer validation only" >&2
fi

git_revision="$(git -C "${repo_root}" rev-parse --short HEAD 2>/dev/null)" \
    || fail "the baseline needs a Git checkout to record reproducible source provenance"
git_dirty="unknown"
if git_status="$(git -C "${repo_root}" status --porcelain \
        --untracked-files=normal 2>/dev/null)"; then
    git_dirty="clean"
    [[ -n "${git_status}" ]] && git_dirty="dirty"
fi

# A dirty revision is not a reproducible source identity by itself. Preserve a
# binary patch for every tracked and untracked source file, excluding the
# evidence directory that receives the patch. A clean checkout at the recorded
# revision plus this patch reconstructs the source state used for the run.
source_patch_tmp="${work_dir}/noc_baseline.source.patch"
git -C "${repo_root}" diff --binary HEAD -- . \
    ':(exclude)platforms/noc_soc/evidence/**' >"${source_patch_tmp}" \
    || fail "could not capture the tracked source diff"
while IFS= read -r -d '' relative; do
    case "${relative}" in
        platforms/noc_soc/evidence/*) continue ;;
    esac
    (
        cd "${repo_root}"
        git diff --no-index --binary -- /dev/null "${relative}"
    ) >>"${source_patch_tmp}"
    patch_status=$?
    [[ ${patch_status} -eq 0 || ${patch_status} -eq 1 ]] \
        || fail "could not capture untracked source file ${relative}"
done < <(git -C "${repo_root}" ls-files --others --exclude-standard -z)
cp "${source_patch_tmp}" "${source_patch}"

git_dirty_bool=false
[[ "${git_dirty}" == "dirty" ]] && git_dirty_bool=true
source_patch_sha256="$(sha256sum "${source_patch}" | cut -d' ' -f1)"
firmware_sha256="$(sha256sum "${elf}" | cut -d' ' -f1)"
platform_sha256="$(sha256sum "${noc_soc_bin}" | cut -d' ' -f1)"
host_cc="$(${CC} -dumpfullversion)"
host_cxx="$(${CXX} --version | head -n 1)"

python3 "${repo_root}/tools/noc_metrics_stamp.py" \
    "${metrics_raw}" "${metrics}" \
    --git-revision "${git_revision}" \
    --git-dirty "${git_dirty_bool}" \
    --source-patch-sha256 "${source_patch_sha256}" \
    --platform-binary "${noc_soc_bin}" \
    --platform-sha256 "${platform_sha256}" \
    --firmware "${elf}" \
    --firmware-sha256 "${firmware_sha256}" \
    --host-cc "${host_cc}" \
    --host-cxx "${host_cxx}" \
    || fail "could not stamp metrics provenance"

python3 "${repo_root}/tools/noc_dashboard.py" \
    "${metrics}" --output "${dashboard}" \
    || fail "could not render the archived metrics dashboard"

{
    echo "# noc_soc NoC and RTOS measurement baseline"
    echo "#"
    echo "# Diagnostic evidence, not a timing-closure claim. No pass/fail"
    echo "# threshold is attached to any number here, and none should be added"
    echo "# until repeatability across clean runs and host load has been"
    echo "# measured. Production request/response router counters are included;"
    echo "# this firmware run remains diagnostic because the CPU is still active"
    echo "# at the end of its fixed window and the mesh is therefore not drained."
    echo "#"
    echo "# recorded:        ${started}"
    echo "# git revision:    ${git_revision} (${git_dirty})"
    echo "# source patch:    $(basename "${source_patch}")"
    echo "# source patch sha256: ${source_patch_sha256}"
    echo "# firmware:        level ${firmware_level}"
    echo "# firmware sha256: ${firmware_sha256}"
    echo "# platform sha256: ${platform_sha256}"
    echo "# host CC:         ${host_cc}"
    echo "# host CXX:        ${host_cxx}"
    echo "# platform binary: ${noc_soc_bin}"
    echo
    sed -n '/^noc_soc measurement baseline$/,/No threshold is implied/p' \
        "${run_log}"
    echo
    echo "  RTOS tick jitter (platform/RTOS metric, NOT a NoC metric: CLINT"
    echo "  drives MTIP straight into the CPU; only the mtime read and the"
    echo "  interrupted code's own traffic cross the network)"
    grep -E '^RTOS tick ' "${run_log}" | sed 's/^/    /'
} >"${baseline}"

echo
sed -n '/^# recorded:/,$p' "${baseline}"
echo
echo "baseline archived at ${baseline}"
echo "metrics archived at ${metrics}"
echo "dashboard archived at ${dashboard}"
echo "noc_soc measurement baseline PASS (completeness only; no thresholds)"
