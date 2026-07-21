#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Run the packaged VP_FX1_V2.0 binary with its FreeRTOS/NPU/TFLM firmware.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "${here}/../../../.." && pwd)"
version="VP_FX1_V2.0"
bin_dir="${root}/vp/bin/${version}"
vp="${bin_dir}/vp_fx1_full_soc"
cfg="${bin_dir}/default.yaml"
fw="${root}/sw/bootloader/build/${version}/freertos_fx1.elf"
input="${here}/cli_smoke.txt"
log="${root}/sw/bootloader/build/${version}/last_run.log"

usage() {
    cat <<'EOF'
Usage:
  ./run_vp.sh smoke                 # default: deterministic self-check
  ./run_vp.sh run [VP arguments]    # boot and print UART output
  ./run_vp.sh interactive [port]    # CLI over TCP; connect with nc

Examples:
  ./run_vp.sh
  ./run_vp.sh interactive 5000
  ./run_vp.sh run --sim-ms 700 --quantum 10000
EOF
}

require_file() {
    [[ -f "$1" ]] || { echo "ERROR: missing $1" >&2; exit 1; }
}

[[ -x "${vp}" ]] || { echo "ERROR: VP is missing or not executable: ${vp}" >&2; exit 1; }
require_file "${cfg}"
require_file "${fw}"
export LD_LIBRARY_PATH="${bin_dir}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

mode="${1:-smoke}"
[[ $# -eq 0 ]] || shift

case "${mode}" in
    -h|--help|help)
        usage
        ;;
    run)
        if [[ $# -eq 0 ]]; then
            set -- --sim-ms 700 --quantum 10000
        fi
        exec "${vp}" -c "${cfg}" --fw "${fw}" "$@"
        ;;
    interactive)
        port="${1:-5000}"
        echo "VP waits for UART0 TCP client on 127.0.0.1:${port}"
        echo "Open another terminal: nc 127.0.0.1 ${port}"
        exec "${vp}" -c "${cfg}" --fw "${fw}" \
            --uart0-socket "${port}" --uart0-wait \
            --sim-ms 60000 --quantum 10000
        ;;
    smoke)
        require_file "${input}"
        mkdir -p "$(dirname "${log}")"
        echo "Running VP_FX1_V2.0 NPU/TFLM smoke test..."
        if ! timeout 300 "${vp}" -c "${cfg}" --fw "${fw}" \
                --uart0-rx-file "${input}" --uart0-rx-delay-us 400000 \
                --sim-ms 1200 --quantum 10000 >"${log}" 2>&1; then
            echo "[FAIL] VP execution; log: ${log}" >&2
            tail -n 60 "${log}" >&2
            exit 1
        fi

        fail=0
        check() {
            if grep -aFq "$1" "${log}"; then
                echo "[PASS] $1"
            else
                echo "[FAIL] missing: $1"
                fail=1
            fi
        }

        check "FreeRTOS FX1 boot"
        check "CLINT tick OK"
        check "PLIC TIMER0 OK irqs=3"
        check "FreeRTOS NPU PASS"
        check "FreeRTOS FX1 PASS"
        check "TFLM_HELLO PASS"
        check "TFLM_RUN PASS 4/4"
        check "TFLM_SIMPLE PASS 2/2"
        check "TFLM_NPU PASS 2/2"
        check "TFLM_DEMO PASS 2/2 bit-exact"
        check "HW_SCAN PASS implemented=25 reserved=2"
        check "REG_TEST PASS 42/42"
        check "sh: command not found: badcmd"

        if grep -aEq 'FATAL|FreeRTOS FX1 FAIL|HW_SCAN FAIL|REG_TEST FAIL|TFLM_(HELLO|RUN|SIMPLE|NPU|DEMO) FAIL' "${log}"; then
            echo "[FAIL] firmware reported a fatal or test failure"
            fail=1
        fi
        if [[ "${fail}" -eq 0 ]]; then
            echo "VP_FX1_V2.0 smoke PASS; full UART log: ${log}"
        else
            echo "VP_FX1_V2.0 smoke FAIL; full UART log: ${log}" >&2
        fi
        exit "${fail}"
        ;;
    *)
        echo "ERROR: unknown mode: ${mode}" >&2
        usage >&2
        exit 2
        ;;
esac
