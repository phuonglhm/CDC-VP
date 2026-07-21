#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# B.1 CLI smoke test using the VP's deterministic UART0 RX-file backend.
#
#   ./run_console_test.sh
#   BUILD_DIR=build-public NPU=0 ./run_console_test.sh
#
# With TFLM=1 (default), this also validates the hello-world interpreter smoke
# and CPU inference for both the hello-world sine model and SECDA simple model.
# An NPU=1/TFLM=1 run also validates Conv2D offload and CPU/NPU bit-exactness.
# The hardware scan and safe register test are validated in both NPU-enabled
# and public NPU-disabled configurations.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
cd "${ROOT}"

BUILD_DIR="${BUILD_DIR:-build-soc}"
NPU="${NPU:-1}"
TFLM="${TFLM:-1}"
EXE="${BUILD_DIR}/platforms/VP_FX1_Full_SoC/vp_fx1_full_soc"
ELF="fw/freertos_fx1/freertos_fx1.elf"
INPUT="fw/freertos_fx1/tests/cli_smoke.txt"
LOG="$(mktemp)"
trap 'rm -f "${LOG}"' EXIT

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:${PATH}

if [ -f tools/third_party/setup_env.sh ]; then
    # shellcheck disable=SC1091
    source tools/third_party/setup_env.sh >/dev/null
fi

[ -x "${EXE}" ] || { echo "FAIL: ${EXE} not built"; exit 1; }
[ -f "${INPUT}" ] || { echo "FAIL: ${INPUT} missing"; exit 1; }

make -C fw/freertos_fx1 clean >/dev/null
make -C fw/freertos_fx1 NPU="${NPU}" TFLM="${TFLM}" >/dev/null ||
    { echo "FAIL: firmware build"; exit 1; }

if ! timeout 300 "${EXE}" --fw "${ELF}" --uart0-rx-file "${INPUT}" \
        --uart0-rx-delay-us 400000 --sim-ms 1200 --quantum 10000 \
        >"${LOG}" 2>&1; then
    echo "FAIL: VP execution"
    tail -n 40 "${LOG}"
    exit 1
fi

fail=0
check() {
    if grep -aFq "$1" "${LOG}"; then
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
check "FX1 FreeRTOS NPU Console"
check "root@fx1:~#"
check "  help : List available commands"
check "  tflm_hello : TFLite-Micro link and interpreter smoke test"
check "  tflm_run : Run the hello_world INT8 sine model on the CPU"
check "  tflm_simple : Run the SECDA simple model CPU baseline"
check "  tflm_npu : Run the simple model with FX1 NPU offload"
check "  tflm_demo : Compare CPU and NPU outputs bit-exactly"
check "  hw_scan : Enumerate the implemented FX1 platform IP"
check "  reg_test : Run safe RW, RO, and W1C register checks"
if [ "${TFLM}" = "1" ]; then
    check "TFLM_HELLO PASS"
    check "TFLM_RUN PASS 4/4"
    check "[TFLM-SIMPLE] output[0]=-113 golden=-113 OK"
    check "[TFLM-SIMPLE] output[1]=127 golden=127 OK"
    check "TFLM_SIMPLE PASS 2/2"
else
    check "TFLM_HELLO unavailable"
    check "TFLM_RUN unavailable"
    check "TFLM_SIMPLE unavailable"
fi
if [ "${NPU}" = "1" ]; then
    if [ "${TFLM}" = "1" ]; then
        check "TFLM_NPU PASS 2/2"
        check "=== TFLite Micro on FX1 RV32 FreeRTOS - NPU Offload Demo ==="
        check "    0 |    -113 |     -113 |  OK"
        check "    1 |     127 |      127 |  OK"
        check " Bit-exact: 2/2   =>  PASS"
        check "  CPU : 0.017 0.292"
        check "  NPU : 0.017 0.292"
        check "=== Done ==="
        check "TFLM_DEMO PASS 2/2 bit-exact"
    else
        check "TFLM_NPU unavailable: TFLite-Micro is not integrated"
        check "TFLM_DEMO unavailable: TFLite-Micro is not integrated"
    fi
else
    check "TFLM_NPU unavailable: NPU absent"
    check "TFLM_DEMO unavailable: NPU absent"
fi
if [ "${NPU}" = "1" ]; then
    check "HW_SCAN PASS implemented=25 reserved=2"
    check "NPU0     0x100F0000  0x00001020  0x53415534  SAU4"
    check "NPU0.CORE_ID"
    check "NPU0.K_DIMENSION"
else
    check "HW_SCAN PASS implemented=24 reserved=3"
    check "NPU0     0x100F0000  ----------  ----------  reserved"
fi
check "QSPI0    0x100C0000  0x00000008  0x00EF4018  JEDEC EF4018"
check "REG_TEST PASS"
check "TIMER1.INTSTATUS"
check "PMU0.INTR_STATE"
check "ADC0.STATUS.EOC"
check "QSPI0.DONE+JEDEC"
check "sh: command not found: badcmd"

if grep -aEq \
        "FATAL|UART RX warning|FreeRTOS FX1 FAIL|HW_SCAN FAIL|REG_TEST FAIL|TFLM_(HELLO|RUN|SIMPLE|NPU|DEMO) FAIL" \
        "${LOG}"; then
    echo "[FAIL] firmware reported a fatal, RX drop, or test failure:"
    grep -aE \
        "FATAL|UART RX warning|FreeRTOS FX1 FAIL|HW_SCAN FAIL|REG_TEST FAIL|TFLM_(HELLO|RUN|SIMPLE|NPU|DEMO) FAIL" \
        "${LOG}"
    fail=1
fi

exit "${fail}"
