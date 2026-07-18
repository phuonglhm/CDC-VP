#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Self-checking FreeRTOS milestone test on VP_FX1_Full_SoC.
#
#   ./run_freertos_test.sh                 # uses BUILD_DIR=build-soc
#   BUILD_DIR=build-public NPU=0 ./run_freertos_test.sh
#
# Exit code 0 = PASS.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
cd "${ROOT}"

BUILD_DIR="${BUILD_DIR:-build-soc}"
NPU="${NPU:-1}"
EXE="${BUILD_DIR}/platforms/VP_FX1_Full_SoC/vp_fx1_full_soc"
ELF="fw/freertos_fx1/freertos_fx1.elf"
LOG="$(mktemp)"
trap 'rm -f "${LOG}"' EXIT

if [ -f tools/third_party/setup_env.sh ]; then
    # shellcheck disable=SC1091
    source tools/third_party/setup_env.sh >/dev/null 2>&1 || true
fi

[ -x "${EXE}" ] || { echo "FAIL: ${EXE} not built"; exit 1; }

make -C fw/freertos_fx1 clean >/dev/null
make -C fw/freertos_fx1 NPU="${NPU}" >/dev/null || { echo "FAIL: firmware build"; exit 1; }

timeout 300 "${EXE}" --fw "${ELF}" --sim-ms 700 --quantum 10000 > "${LOG}" 2>&1

fail=0
check() {
    if grep -aq "$1" "${LOG}"; then
        echo "[PASS] $1"
    else
        echo "[FAIL] missing: $1"
        fail=1
    fi
}

check "FreeRTOS FX1 boot"
check "CLINT tick OK"
check "PLIC TIMER0 OK irqs=3"
[ "${NPU}" = "1" ] && check "FreeRTOS NPU PASS"
check "FreeRTOS FX1 PASS"

if grep -aq "FAIL" "${LOG}"; then
    echo "[FAIL] firmware reported a failure:"
    grep -a "FAIL" "${LOG}"
    fail=1
fi

exit "${fail}"
