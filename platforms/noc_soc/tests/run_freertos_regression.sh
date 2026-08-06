#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Step 12.5 platform-level FreeRTOS regression for `noc_soc`.
#
# This is a thin wrapper, on purpose. The acceptance contract for every Step 12
# level already lives in `fw/freertos_noc_soc/run_step_regression.sh`: it builds
# the firmware with `NPU=0`, validates the ELF32 load ranges before simulation,
# runs the platform in explicit firmware and fast-timing modes under a host
# timeout, requires every acceptance marker in order, and rejects failures,
# unexpected traps and synthetic survey traffic. Copying that contract into a
# second file would create two oracles that drift apart; roadmap section 9 asks
# for the existing one to be registered, not reimplemented.
#
# What this file adds is the platform level:
#
#   * it runs the whole staged set, so the claim that levels 121, 122, 123 and
#     the signed 124 workload stay independently reproducible is checked rather
#     than asserted before the Step 12.8 CLI;
#   * it accepts an externally supplied binary, which is how the packaging gate
#     points it at the installed package consumer;
#   * it keeps every log on failure and reports where.
#
# Environment:
#   NOC_SOC_BIN     platform binary to test; defaults to the development build
#   FREERTOS_STEPS  space-separated levels, default
#                   "12.1 12.2 12.3 12.4 12.8"
#   LOG_DIR         where to keep evidence; a private mktemp directory otherwise
#
# Exit codes: 0 pass, 1 fail, 77 skipped (CTest's SKIP_RETURN_CODE) when the
# RISC-V cross-toolchain is absent. Skipping is deliberate: a machine that
# cannot build the firmware should not report a red test it could never run.

set -u -o pipefail

readonly SKIP=77

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
platform_dir="$(cd "${script_dir}/.." && pwd)"
repo_root="$(cd "${platform_dir}/../.." && pwd)"

readonly step_runner="${repo_root}/fw/freertos_noc_soc/run_step_regression.sh"
read -r -a steps <<<"${FREERTOS_STEPS:-12.1 12.2 12.3 12.4 12.8}"

fail() { echo "FAIL: $*" >&2; exit 1; }

[[ -x "${step_runner}" ]] \
    || fail "the Step 12 acceptance runner is missing: ${step_runner}"
[[ ${#steps[@]} -gt 0 ]] || fail "FREERTOS_STEPS is empty"

# Validate the whole list before anything is created or run. Rejecting a typo
# halfway through would leave a stray evidence directory and a partially
# executed set behind.
for step in "${steps[@]}"; do
    case "${step}" in
        12.1|12.2|12.3|12.4|12.8) ;;
        *) fail "unsupported FreeRTOS step: ${step}" ;;
    esac
done

noc_soc_bin="${NOC_SOC_BIN:-}"
if [[ -z "${noc_soc_bin}" ]]; then
    for candidate in \
        "${repo_root}/build/platforms/noc_soc/noc_soc" \
        "${platform_dir}/noc_soc"; do
        [[ -x "${candidate}" ]] && { noc_soc_bin="${candidate}"; break; }
    done
fi
[[ -x "${noc_soc_bin:-}" ]] || fail "noc_soc binary not found; set NOC_SOC_BIN"

# The toolchain check happens here as well as inside the step runner. Deciding
# to skip after the first level has already passed would report a skip for a
# run that partly succeeded, which is a misleading result rather than a missing
# one.
if ! command -v riscv-none-elf-gcc >/dev/null 2>&1 \
   && [[ ! -x /opt/toolchains/riscv-none-elf/bin/riscv-none-elf-gcc ]]; then
    echo "SKIP: riscv-none-elf-gcc not found; cannot build the firmware." >&2
    exit "${SKIP}"
fi

if [[ -n "${LOG_DIR:-}" ]]; then
    log_dir="${LOG_DIR}"
    mkdir -p "${log_dir}"
else
    log_dir="$(mktemp -d "${TMPDIR:-/tmp}/noc_soc_freertos_regression.XXXXXX")"
fi

echo "noc_soc FreeRTOS regression"
echo "  binary: ${noc_soc_bin}"
echo "  steps:  ${steps[*]}"
echo "  logs:   ${log_dir}"

for step in "${steps[@]}"; do
    step_log_dir="${log_dir}/step_${step//./_}"
    step_output="${log_dir}/step_${step//./_}.txt"

    # `errexit` is deliberately not enabled in this script: every failure below
    # is reported with its own message and evidence path, and a bare non-zero
    # exit would lose both.
    env NOC_SOC_BIN="${noc_soc_bin}" LOG_DIR="${step_log_dir}" \
        "${step_runner}" "${step}" >"${step_output}" 2>&1
    status=$?

    if [[ ${status} -eq ${SKIP} ]]; then
        echo "SKIP: Step ${step} reported a missing toolchain" >&2
        cat "${step_output}" >&2
        exit "${SKIP}"
    fi
    if [[ ${status} -ne 0 ]]; then
        echo "--- Step ${step} output ---" >&2
        cat "${step_output}" >&2
        fail "Step ${step} regression failed (evidence kept at ${step_log_dir})"
    fi

    sed 's/^/  /' "${step_output}"
done

echo "noc_soc FreeRTOS regression PASS"
