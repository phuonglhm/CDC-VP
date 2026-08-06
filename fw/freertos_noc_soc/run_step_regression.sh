#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Shared self-checking runner for Steps 12.1 through 12.8. Full CTest
# registration and the platform mutation registry intentionally remain in
# roadmap Steps 12.5 and 12.6.

set -u -o pipefail

readonly SKIP=77
readonly host_timeout=120
readonly rounds=8
readonly concurrent_checkpoints=4
readonly concurrent_dma_transfers=4
readonly concurrent_dma_bytes=16384
readonly concurrent_timer_irqs=20
readonly acceptance="${1:?usage: run_step_regression.sh 12.1|12.2|12.3|12.4|12.8}"

case "${acceptance}" in
    12.1)
        readonly firmware_level=121
        readonly modeled_us=20000
        ;;
    12.2)
        readonly firmware_level=122
        readonly modeled_us=50000
        ;;
    12.3)
        readonly firmware_level=123
        readonly modeled_us=100000
        ;;
    12.4)
        readonly firmware_level=124
        readonly modeled_us=500000
        ;;
    12.8)
        readonly firmware_level=128
        readonly modeled_us=550000
        ;;
    *)
        echo "FAIL: unsupported FreeRTOS acceptance step: ${acceptance}" >&2
        exit 1
        ;;
esac

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
noc_soc_bin="${NOC_SOC_BIN:-}"

if [[ -n "${LOG_DIR:-}" ]]; then
    log_dir="${LOG_DIR}"
    mkdir -p "${log_dir}"
else
    log_dir="$(mktemp -d \
        "${TMPDIR:-/tmp}/freertos_noc_step_${firmware_level}.XXXXXX")"
fi

fail()
{
    echo "FAIL: $*" >&2
    exit 1
}

require_log()
{
    local text="$1"
    if ! grep -Fq "${text}" "${run_log}"; then
        echo "--- Step ${acceptance} log tail ---" >&2
        tail -80 "${run_log}" >&2
        fail "required marker not found: ${text} (log kept at ${run_log})"
    fi
}

for candidate in \
    "${repo_root}/build/platforms/noc_soc/noc_soc" \
    "${repo_root}/platforms/noc_soc/noc_soc"; do
    if [[ -z "${noc_soc_bin}" && -x "${candidate}" ]]; then
        noc_soc_bin="${candidate}"
    fi
done
[[ -x "${noc_soc_bin:-}" ]] ||
    fail "noc_soc binary not found; build it or set NOC_SOC_BIN"

runner_original_path="${PATH}"
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH="/usr/bin:/bin:${runner_original_path}"

if [[ -x /opt/toolchains/riscv-none-elf/bin/riscv-none-elf-gcc ]]; then
    export PATH="/opt/toolchains/riscv-none-elf/bin:${PATH}"
elif ! command -v riscv-none-elf-gcc >/dev/null 2>&1; then
    echo "SKIP: riscv-none-elf toolchain not found" >&2
    exit "${SKIP}"
fi

echo "Step ${acceptance} FreeRTOS noc_soc regression"
echo "  host CC:  $(${CC} -dumpfullversion)"
echo "  host CXX: $(${CXX} --version | head -n 1)"
echo "  binary:   ${noc_soc_bin}"
echo "  logs:     ${log_dir}"

if [[ "${acceptance}" == "12.8" ]]; then
    help_text="$("${noc_soc_bin}" --help 2>&1)"
    if [[ "${help_text}" != *"--uart0-rx-file FILE"* ]]; then
        fail "noc_soc binary does not expose --uart0-rx-file; rebuild target" \
             "noc_soc from the current source tree before running Step 12.8"
    fi
fi

elf="${log_dir}/freertos_noc_soc_step_${firmware_level}.elf"
disassembly="${log_dir}/freertos_noc_soc_step_${firmware_level}.dis"
build_log="${log_dir}/firmware_build.log"

if ! make -C "${script_dir}" \
        NOC_STEP="${firmware_level}" \
        TARGET="${elf}" \
        DISASSEMBLY="${disassembly}" \
        check >"${build_log}" 2>&1; then
    echo "--- firmware build log ---" >&2
    cat "${build_log}" >&2
    fail "Step ${acceptance} firmware build/ELF contract failed"
fi

run_log="${log_dir}/noc_soc_fast.log"
platform_args=(
    --mode firmware
    --noc-timing fast
    --fw "${elf}"
    --sim-us "${modeled_us}"
)
if [[ "${acceptance}" == "12.8" ]]; then
    cli_input="${log_dir}/cli_input.txt"
    printf 'help\r\nsoc\r\nnoc_dashboard\r\nhw_scan\r\nreg_test\r\n' \
        >"${cli_input}"
    platform_args+=(
        --uart0-rx-file "${cli_input}"
        --uart0-rx-delay-us 10000
    )
fi
timeout --kill-after=10 "${host_timeout}" \
    "${noc_soc_bin}" "${platform_args[@]}" >"${run_log}" 2>&1
status=$?

if [[ ${status} -eq 124 || ${status} -eq 137 ]]; then
    tail -80 "${run_log}" >&2
    fail "noc_soc exceeded the ${host_timeout}s host timeout"
fi
if [[ ${status} -ne 0 ]]; then
    tail -80 "${run_log}" >&2
    fail "noc_soc exited ${status}"
fi

require_log "FreeRTOS NoC boot"
require_log "FreeRTOS NoC tasks priorities low=1 high=2"
require_log "FreeRTOS NoC context switching OK rounds=${rounds}"
require_log "noc_soc mode: firmware"
require_log "interconnect timing: fast approximately-timed FlooNoC"
require_log "noc_soc: firmware mode; synthetic survey disabled"
require_log "  transactions 0"
require_log "  RAM writes 0"
require_log "  DMA register writes 0"

mapfile -t events < <(
    grep -E '^NoC (low|high) round=[0-9]+$' "${run_log}"
)
if [[ ${#events[@]} -ne $((rounds * 2)) ]]; then
    fail "expected $((rounds * 2)) task handoff markers, observed ${#events[@]}"
fi
for ((round = 1; round <= rounds; ++round)); do
    low_index=$(((round - 1) * 2))
    high_index=$((low_index + 1))
    [[ "${events[low_index]}" == "NoC low round=${round}" ]] ||
        fail "task handoff ${low_index} is '${events[low_index]}'"
    [[ "${events[high_index]}" == "NoC high round=${round}" ]] ||
        fail "task handoff ${high_index} is '${events[high_index]}'"
done

if [[ "${acceptance}" == "12.1" ]]; then
    for forbidden in \
        "FreeRTOS NoC Step 12.2" \
        "FreeRTOS NoC Step 12.3" \
        "FreeRTOS NoC Step 12.4" \
        "NoC concurrent" \
        "NoC tick wake=" \
        "CLINT tick OK" \
        "PLIC TIMER0" \
        "DMA NoC"; do
        if grep -Fq "${forbidden}" "${run_log}"; then
            tail -80 "${run_log}" >&2
            fail "later-step text found in Step 12.1: ${forbidden}"
        fi
    done
else
    require_log "FreeRTOS NoC Step 12.2 CLINT+PLIC"
    require_log "CLINT tick OK"
    require_log "PLIC TIMER0 OK irqs=3"

    mapfile -t tick_events < <(
        grep -E '^NoC tick wake=[0-9]+$' "${run_log}"
    )
    if [[ ${#tick_events[@]} -ne 3 ]]; then
        fail "expected 3 CLINT wake markers, observed ${#tick_events[@]}"
    fi
    for ((wake = 1; wake <= 3; ++wake)); do
        [[ "${tick_events[wake - 1]}" == "NoC tick wake=${wake}" ]] ||
            fail "CLINT wake $((wake - 1)) is '${tick_events[wake - 1]}'"
    done

    if [[ "${acceptance}" == "12.2" ]]; then
        for forbidden in \
            "FreeRTOS NoC Step 12.3" \
            "FreeRTOS NoC Step 12.4" \
            "NoC concurrent" \
            "DMA NoC" \
            "[IRQ] dma0 asserted"; do
            if grep -Fq "${forbidden}" "${run_log}"; then
                tail -80 "${run_log}" >&2
                fail "later-step text found in Step 12.2: ${forbidden}"
            fi
        done
    else
        require_log "FreeRTOS NoC Step 12.3 DMA"
        require_log "[IRQ] dma0 asserted at"
        require_log "DMA NoC IRQ source=7 count=1"
        require_log "DMA NoC bytes match=32"
        require_log "DMA NoC PASS"
        [[ "$(grep -Fxc "DMA NoC PASS" "${run_log}")" -eq 1 ]] ||
            fail "DMA NoC PASS is missing or duplicated"
    fi

    if [[ "${acceptance}" == "12.3" ]]; then
        for forbidden in \
            "FreeRTOS NoC Step 12.4" \
            "NoC concurrent"; do
            if grep -Fq "${forbidden}" "${run_log}"; then
                tail -80 "${run_log}" >&2
                fail "later-step text found in Step 12.3: ${forbidden}"
            fi
        done
    elif [[ "${acceptance}" == "12.4" ||
            "${acceptance}" == "12.8" ]]; then
        require_log "FreeRTOS NoC Step 12.4 concurrent"

        # Forward progress: the supervisor emits one checkpoint per bounded
        # delay, and only after every participant advanced since the previous
        # one. Requiring them in strict ascending order rejects a run that
        # printed the summary without actually cycling.
        mapfile -t checkpoint_events < <(
            grep -E '^NoC concurrent checkpoint=[0-9]+$' "${run_log}"
        )
        if [[ ${#checkpoint_events[@]} -lt ${concurrent_checkpoints} ]]; then
            fail "expected at least ${concurrent_checkpoints} concurrent" \
                 "checkpoints, observed ${#checkpoint_events[@]}"
        fi
        for ((index = 0; index < ${#checkpoint_events[@]}; ++index)); do
            expected="NoC concurrent checkpoint=$((index + 1))"
            [[ "${checkpoint_events[index]}" == "${expected}" ]] ||
                fail "concurrent checkpoint ${index} is" \
                     "'${checkpoint_events[index]}'"
        done

        require_log "NoC concurrent DMA transfers=${concurrent_dma_transfers}"
        require_log "NoC concurrent DMA bytes=${concurrent_dma_bytes}"
        require_log "FreeRTOS NoC concurrent OK checkpoints="

        # The remaining counters are floors rather than fixed values: iteration
        # and interrupt counts depend on the scheduling of a genuinely
        # concurrent phase and must not be turned into a timing threshold.
        for counted in \
            "NoC concurrent cpu-a iterations=:1" \
            "NoC concurrent cpu-b iterations=:1" \
            "NoC concurrent cpu-a words=:${concurrent_checkpoints}" \
            "NoC concurrent cpu-b words=:${concurrent_checkpoints}" \
            "NoC concurrent TIMER0 irqs=:${concurrent_timer_irqs}" \
            "NoC concurrent overlap cpu-a words=:1" \
            "NoC concurrent overlap cpu-b words=:1" \
            "NoC concurrent overlap TIMER0 irqs=:1"; do
            prefix="${counted%:*}"
            floor="${counted##*:}"
            line="$(grep -F "${prefix}" "${run_log}" | tail -n 1)"
            [[ -n "${line}" ]] || fail "missing counter line: ${prefix}"
            value="${line##*=}"
            [[ "${value}" =~ ^[0-9]+$ ]] ||
                fail "counter '${prefix}' is not numeric: '${line}'"
            [[ "${value}" -ge "${floor}" ]] ||
                fail "counter '${prefix}' is ${value}, below the floor ${floor}"
        done
    fi

    pass_line="$(grep -nFx "FreeRTOS NoC PASS" "${run_log}" |
        cut -d: -f1)"
    [[ "${pass_line}" =~ ^[0-9]+$ ]] ||
        fail "FreeRTOS NoC PASS is missing or duplicated"
    for prerequisite in \
        "FreeRTOS NoC context switching OK rounds=${rounds}" \
        "CLINT tick OK" \
        "PLIC TIMER0 OK irqs=3"; do
        prerequisite_line="$(grep -nFx "${prerequisite}" "${run_log}" |
            cut -d: -f1)"
        [[ "${prerequisite_line}" =~ ^[0-9]+$ &&
           ${prerequisite_line} -lt ${pass_line} ]] ||
            fail "'${prerequisite}' did not precede the final PASS"
    done
    if [[ "${acceptance}" == "12.3" || "${acceptance}" == "12.4" ||
          "${acceptance}" == "12.8" ]]; then
        dma_pass_line="$(grep -nFx "DMA NoC PASS" "${run_log}" |
            cut -d: -f1)"
        [[ "${dma_pass_line}" =~ ^[0-9]+$ &&
           ${dma_pass_line} -lt ${pass_line} ]] ||
            fail "DMA NoC PASS did not precede the final PASS"
    fi
    if [[ "${acceptance}" == "12.4" || "${acceptance}" == "12.8" ]]; then
        # The concurrent phase must open after the sequential Step 12.3 proof
        # and close before the final PASS. Without both bounds a run could
        # print the summary from a phase that never overlapped anything.
        phase_line="$(grep -nFx "FreeRTOS NoC Step 12.4 concurrent" \
            "${run_log}" | cut -d: -f1)"
        [[ "${phase_line}" =~ ^[0-9]+$ ]] ||
            fail "the concurrent phase marker is missing or duplicated"
        [[ ${dma_pass_line} -lt ${phase_line} ]] ||
            fail "the concurrent phase did not follow the Step 12.3 DMA PASS"

        concurrent_ok_line="$(grep -n "^FreeRTOS NoC concurrent OK checkpoints=" \
            "${run_log}" | cut -d: -f1)"
        [[ "${concurrent_ok_line}" =~ ^[0-9]+$ ]] ||
            fail "the concurrent OK marker is missing or duplicated"
        [[ ${phase_line} -lt ${concurrent_ok_line} &&
           ${concurrent_ok_line} -lt ${pass_line} ]] ||
            fail "the concurrent OK marker is out of order"
    fi

    if [[ "${acceptance}" == "12.8" ]]; then
        for cli_marker in \
            "FreeRTOS NoC Step 12.8 CLI" \
            "FreeRTOS NoC CLI RX armed source=1" \
            "FreeRTOS NoC CLI ready" \
            "Commands:" \
            "help : Show available commands" \
            "soc : Show the FlooNoC platform topology and address map" \
            "noc_dashboard : Request the full host-rendered NoC dashboard" \
            "hw_scan : Scan mapped FlooNoC SoC hardware" \
            "reg_test : Run safe restoring RO, RW and W1C checks" \
            "CLI soc mesh=4x4 managers=cpu,dma,probe" \
            "CLI noc_dashboard host request" \
            "noc_soc live dashboard unavailable: live dashboard requires detailed mode and --noc-metrics FILE" \
            "=== FlooNoC SoC Hardware Scan ===" \
            "HW_SCAN PASS implemented=22 reserved=3 absent=2" \
            "=== FlooNoC SoC Safe Register Test ===" \
            "REG_TEST PASS 37/37" \
            "[IRQ] uart0 asserted"; do
            require_log "${cli_marker}"
        done

        cli_ready_line="$(grep -nFx "FreeRTOS NoC CLI ready" "${run_log}" |
            cut -d: -f1)"
        mapfile -t command_list_lines < <(
            grep -nFx "Commands:" "${run_log}" | cut -d: -f1
        )
        [[ ${#command_list_lines[@]} -eq 2 ]] ||
            fail "expected command list in banner and help response"
        help_line="${command_list_lines[1]}"
        soc_line="$(grep -nFx "CLI soc mesh=4x4 managers=cpu,dma,probe" \
            "${run_log}" | cut -d: -f1)"
        dashboard_line="$(grep -nFx "CLI noc_dashboard host request" \
            "${run_log}" | cut -d: -f1)"
        dashboard_host_line="$(grep -nFx \
            "noc_soc live dashboard unavailable: live dashboard requires detailed mode and --noc-metrics FILE" \
            "${run_log}" | cut -d: -f1)"
        hw_scan_line="$(grep -nFx \
            "HW_SCAN PASS implemented=22 reserved=3 absent=2" \
            "${run_log}" | cut -d: -f1)"
        reg_test_line="$(grep -nFx "REG_TEST PASS 37/37" \
            "${run_log}" | cut -d: -f1)"
        [[ "${cli_ready_line}" =~ ^[0-9]+$ &&
           "${soc_line}" =~ ^[0-9]+$ &&
           "${dashboard_line}" =~ ^[0-9]+$ &&
           "${dashboard_host_line}" =~ ^[0-9]+$ &&
           "${hw_scan_line}" =~ ^[0-9]+$ &&
           "${reg_test_line}" =~ ^[0-9]+$ &&
           ${pass_line} -lt ${cli_ready_line} &&
           ${cli_ready_line} -lt ${help_line} &&
           ${help_line} -lt ${soc_line} &&
           ${soc_line} -lt ${dashboard_line} &&
           ${dashboard_line} -lt ${dashboard_host_line} &&
           ${dashboard_host_line} -lt ${hw_scan_line} &&
           ${hw_scan_line} -lt ${reg_test_line} ]] ||
            fail "CLI diagnostics did not run in order after complete SoC PASS"

        for removed in \
            "status : " \
            "irq : " \
            "uptime : " \
            "exit : " \
            "CLI status " \
            "CLI irq " \
            "CLI uptime " \
            "CLI session closed"; do
            if grep -Fq "${removed}" "${run_log}"; then
                fail "removed CLI command/output is still present: ${removed}"
            fi
        done
    fi
fi

# Checked last on purpose. The final PASS is the least informative marker in the
# log: it is absent whenever anything upstream failed, so reporting it first
# would name the symptom for every possible defect. Requiring the specific
# prerequisites above first means a failing run says which stage broke.
require_log "FreeRTOS NoC PASS"

for forbidden in \
    "FreeRTOS FX1" \
    "[IRQ] dma0_abort" \
    "FAIL" \
    "FATAL" \
    "Taking trap" \
    "[PC] trapped" \
    "watchdog"; do
    if grep -Fq "${forbidden}" "${run_log}"; then
        tail -80 "${run_log}" >&2
        fail "forbidden Step ${acceptance} text found: ${forbidden}"
    fi
done
if grep -Eq 'Error:|\([EW][0-9]{3}\)|CPU pc 0x0\b' "${run_log}"; then
    tail -80 "${run_log}" >&2
    fail "platform error or invalid CPU terminal state detected"
fi

echo "  task handoffs: $((rounds * 2)) in strict low/high order"
if [[ "${acceptance}" == "12.4" || "${acceptance}" == "12.8" ]]; then
    echo "  concurrent:    ${#checkpoint_events[@]} checkpoints," \
         "${concurrent_dma_transfers} overlapped DMA transfers"
    grep -E '^NoC concurrent (cpu-a|cpu-b|TIMER0|overlap) ' "${run_log}" |
        sed 's/^/    /'
fi
echo "  modeled time:  ${modeled_us} us"
echo "  log:           ${run_log}"
echo "Step ${acceptance} FreeRTOS noc_soc regression PASS"
