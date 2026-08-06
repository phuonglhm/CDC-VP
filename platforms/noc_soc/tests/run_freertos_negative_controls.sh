#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Step 12.6: qualify the FreeRTOS platform regression with negative controls.
#
# Acceptance markers alone are not evidence. Firmware that always prints PASS is
# the easiest test in the world to satisfy, so every major acceptance path must
# be shown to fail when the defect it exists to catch is put back.
#
# ## Where it runs
#
# Never in the working tree. Each control gets a private copy of exactly the
# repository scope it needs under `/tmp`, and the mutation is applied there. An
# interrupt, a SIGKILL or a lost machine cannot leave a mutated source behind,
# and two runs cannot collide.
#
# This is deliberately **not** added to
# `components/floo_noc_model/rtl_crosscheck/run_negative_controls.sh`. That
# runner intentionally copies and builds only the component; these controls span
# platform code, shared BSP code, the FreeRTOS port and the application.
#
# ## Two scopes, because the cost differs by an order of magnitude
#
#   * `firmware` — mutates firmware/BSP/port source. Only the firmware is
#     rebuilt, against the platform binary under test. A few seconds.
#   * `platform` — mutates `platforms/noc_soc` source. Needs a private
#     configure and build of the platform, which is why exactly one control
#     uses it.
#
# ## What counts as detection
#
# A control is detected only when **all five** hold:
#
#   1. the exact needle is found exactly once in the private copy;
#   2. the mutated platform and firmware still build;
#   3. the model runs inside the normal modeled-time and host-time bounds;
#   4. the positive regression returns non-zero; and
#   5. the named control-specific diagnostic appears in the evidence.
#
# A compilation error, an unrelated trap, an arbitrary non-zero exit or a
# generic host timeout is a **control failure**, not a detection. Two separate
# expectations are therefore recorded per control: what the runner itself must
# say, and what the platform log must contain. Checking only the exit code would
# score a mutation that merely broke the build the same as one that broke
# behaviour.
#
# A missing marker counts only where the runner names that exact marker, and the
# clean run below has already proved the marker is normally produced. That clean
# run is mandatory and runs first: a control registry qualifying a regression
# that does not currently pass proves nothing.
#
# Some defects produce no firmware diagnostic at all — a scheduler tick that
# never arrives simply leaves tasks blocked. For those the log expectation is
# written `!text`, meaning that text must be **absent**, and the clean run is
# required to have produced it. That keeps the evidence positive in both
# directions: the marker exists when the code is right and is gone when it is
# not, rather than "the run failed somehow".
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

# Deliberately not `$SYSTEMC_HOME`: a stale value in the environment is a known
# trap on this host.
systemc_home="${FLOO_SYSTEMC_HOME:-/opt/systemc-2.3.4}"

# The level every control is judged against. Step 12.8 retains the complete
# level-124 workload and adds the UART CLI; it is also the image used by the
# packaging gate, so the controls qualify what now ships.
readonly acceptance_step="${FREERTOS_CONTROL_STEP:-12.8}"

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

work_dir="$(mktemp -d "${TMPDIR:-/tmp}/noc_soc_freertos_controls.XXXXXX")"

echo "noc_soc FreeRTOS negative controls"
echo "  binary:     ${noc_soc_bin}"
echo "  acceptance: Step ${acceptance_step}"
echo "  compiler:   $(${CC} -dumpfullversion)"
echo "  evidence:   ${work_dir}"

# ── the registry ─────────────────────────────────────────────────────────────
#
# Parallel arrays rather than delimited strings: the literals span several
# lines and `read` stops at the first one.
names=()
scopes=()
files=()
needles=()
replacements=()
expect_runners=()
expect_logs=()
whys=()

add_control() {
    local field index=0
    for field in "$1" "$2" "$3" "$4" "$6" "$7" "$8"; do
        index=$((index + 1))
        if [[ -z "${field}" ]]; then
            echo "control registry: entry '$1' has an empty required field" >&2
            echo "(position ${index} of name/scope/file/needle/runner/log/why)." >&2
            exit 1
        fi
    done
    case "$2" in
        firmware|platform) ;;
        *) echo "control registry: '$1' has unknown scope '$2'" >&2; exit 1 ;;
    esac
    names+=("$1"); scopes+=("$2"); files+=("$3"); needles+=("$4")
    replacements+=("$5"); expect_runners+=("$6"); expect_logs+=("$7")
    whys+=("$8")
}

# 1. A valid but wrong destination: mapped RAM, below the reserved page, and
#    outside every firmware object, so nothing rejects it before the data is
#    compared. This control keeps the byte comparison honest — without it, a DMA
#    that completed and raised its interrupt would look correct.
#
#    The first attempt used `dma_destination + 4`, which ran off the end of the
#    destination array and into the *source* buffer, so it tripped the source
#    comparison instead of the destination one and qualified the wrong check. A
#    wrong destination has to land where nothing else is verified.
add_control \
    "freertos-dma-destination-corrupted" \
    "firmware" \
    "fw/freertos_noc_soc/src/main.c" \
    '    const uint32_t destination_address =
        (uint32_t)(uintptr_t)dma_destination;' \
    '    const uint32_t destination_address =
        (uint32_t)(CDC_RAM0_BASE + 0x00800000u);' \
    "DMA NoC IRQ source=7 count=1" \
    "DMA NoC FAIL: mismatch byte=" \
    "the DMA landed 8 MiB away from the verified buffer, so the transfer completed and interrupted correctly while the destination was never written"

# 2. TIMER0 enabled under a different, valid source. The handler registration is
#    left on source 4 on purpose: this control asks whether the *enable* path is
#    observed, not whether the firmware can be broken in general.
add_control \
    "freertos-plic-timer-source-wrong" \
    "firmware" \
    "fw/freertos_noc_soc/src/main.c" \
    '    plic_enable(CDC_IRQ_TIMER0, 1u);
    TIMER0_RELOAD = TIMER0_RELOAD_COUNTS;' \
    '    plic_enable(CDC_IRQ_WDT0, 1u);
    TIMER0_RELOAD = TIMER0_RELOAD_COUNTS;' \
    "PLIC TIMER0 OK irqs=3" \
    "PLIC TIMER0 FAIL: timeout after 0 irqs" \
    "TIMER0 was enabled under PLIC source 5, so its line never reached the CPU and no interrupt was ever delivered"

# 3. The claim is still read and the device cause is still acknowledged; only
#    the completion write is removed. The model's PLIC gateway then holds the
#    source claimed forever, so exactly one interrupt is delivered and no more.
#    Kept separate from control 2 on purpose: a wrong source proves the
#    claim/dispatch path is observed, a missing completion proves the
#    claim/complete contract is. One mutation covering both would leave half of
#    it unqualified.
add_control \
    "freertos-plic-complete-omitted" \
    "firmware" \
    "fw/freertos_fx1/bsp/plic.c" \
    '        write32(CDC_PLIC_BASE + CDC_PLIC_CLAIM, claim);' \
    '' \
    "PLIC TIMER0 OK irqs=3" \
    "PLIC TIMER0 FAIL: timeout after 1 irqs" \
    "the PLIC claim was never completed, so the gateway stayed closed after the first interrupt and the source went permanently silent"

# 4. `mtimecmp` is never written, so MTIP never asserts and the scheduler tick
#    never runs.
#
#    This one mutates the vendored FreeRTOS port rather than firmware we own,
#    and that is a finding rather than a convenience: every firmware-side way of
#    breaking this — the CLINT base, the mtimecmp offset, the config macro — is
#    already rejected at compile time by the `_Static_assert` pair in
#    `main.c`. The misconfiguration class of this defect cannot reach a run, so
#    the only way to exercise the runtime path is to remove the write itself.
#    The mutation is applied to a private copy; the pinned tree is untouched.
add_control \
    "freertos-clint-tick-disabled" \
    "firmware" \
    "third_party/FreeRTOS-Kernel/portable/GCC/RISC-V/port.c" \
    '        *pullMachineTimerCompareRegister = ullNextTime;' \
    '' \
    "CLINT tick OK" \
    "!NoC tick wake=" \
    "mtimecmp was never programmed, so MTIP never asserted, the FreeRTOS tick never advanced and every vTaskDelay blocked forever"

# 5. One synthetic probe access in firmware mode. The platform must own no
#    traffic at all when firmware does; a single write is enough to break that,
#    and it is the smallest violation the ownership counters can see.
add_control \
    "firmware-survey-ownership-broken" \
    "platform" \
    "platforms/noc_soc/src/noc_soc_top.cpp" \
    '        if (mode == noc_soc_mode::firmware) {
            run_firmware_mode();
            return;
        }' \
    '        if (mode == noc_soc_mode::firmware) {
            write32(kSurveyScratch, 0u);
            run_firmware_mode();
            return;
        }' \
    "  transactions 0" \
    "  RAM writes 1" \
    "the synthetic probe issued a RAM write while firmware owned the platform, so firmware and probe traffic were mixed in the same run"

# 6. Corrupt the final worker generation after the normal loop has stopped.
#    Earlier generations are checked by the next iteration; only the explicit
#    teardown readback can see this one.
add_control \
    "freertos-concurrent-final-buffer-corrupted" \
    "firmware" \
    "fw/freertos_noc_soc/src/main.c" \
    '    for (uint32_t index = 0u; index < NOC_WORK_WORDS; ++index) {
        if (buffer[index] != work_word(seed, index, generation)) {' \
    '    buffer[0] ^= 1u;
    for (uint32_t index = 0u; index < NOC_WORK_WORDS; ++index) {
        if (buffer[index] != work_word(seed, index, generation)) {' \
    "NoC concurrent DMA transfers=4" \
    "NoC concurrent FAIL: final RAM word=0" \
    "the final worker generation was corrupted after the iterative checks, so only the teardown readback can prevent silent data loss"

# 7. Lose one participant's measured progress while the DMA task is blocked.
#    This qualifies the Step 12.4 distinction between coexistence and actual
#    overlap rather than relying only on controls for the earlier staged steps.
add_control \
    "freertos-concurrent-overlap-lost" \
    "firmware" \
    "fw/freertos_noc_soc/src/main.c" \
    '    overlap_timer_irqs = timer_irq_count - overlap_irqs_before;' \
    '    overlap_timer_irqs = timer_irq_count - overlap_irqs_before;
    overlap_worker_a = 0u;' \
    "NoC concurrent DMA transfers=4" \
    "NoC concurrent FAIL: overlap cpu-a=0" \
    "CPU task A made progress in the run but its in-flight DMA overlap evidence was lost, so coexistence could otherwise be mistaken for concurrency"

# 8. Leave the CLI task present but do not arm UART0 RX or PLIC source 1. The
#    scripted host input still enters the platform bridge, so the only broken
#    link is the firmware-owned interrupt path. A bounded modeled window makes
#    this fail as a missing command marker rather than a host hang.
add_control \
    "freertos-cli-uart-rx-disabled" \
    "firmware" \
    "fw/freertos_noc_soc/src/cli.c" \
    '    uart_rx_start();
    uart_puts("FreeRTOS NoC CLI RX armed source=1\n");' \
    '    uart_puts("FreeRTOS NoC CLI RX deliberately disabled\n");' \
    "FreeRTOS NoC CLI RX armed source=1" \
    "FreeRTOS NoC CLI RX deliberately disabled" \
    "the CLI task was released after bring-up but UART0 RX and PLIC source 1 were never armed, so no host command could reach the parser"

# 9. Keep the command and its visible acknowledgement, but remove the
#    out-of-band UART marker consumed by noc_soc. The platform must therefore
#    never publish or even acknowledge a live dashboard request.
add_control \
    "freertos-cli-dashboard-marker-omitted" \
    "firmware" \
    "fw/freertos_noc_soc/src/cli.c" \
    '    uart_puts(CDC_NOC_DASHBOARD_REQUEST);' \
    '    uart_puts("dashboard marker deliberately omitted\n");' \
    "noc_soc live dashboard unavailable: live dashboard requires detailed mode and --noc-metrics FILE" \
    "dashboard marker deliberately omitted" \
    "the CLI accepted noc_dashboard but never sent the private UART control record, so the host side could not take a metrics snapshot"

# 10. Corrupt one hardware-scan identity expectation without changing the MMIO
#    access itself. The scan must print MISMATCH and the Step 12.8 gate must
#    refuse its aggregate PASS marker.
add_control \
    "freertos-hw-scan-identity-wrong" \
    "firmware" \
    "fw/freertos_fx1/src/hw_diag.c" \
    '  scan_probe(&summary, "WDT0", CDC_WDT0_BASE, PRIME_PID0, WDT_PID0_VALUE, 0xFFu,
             "SP805");' \
    '  scan_probe(&summary, "WDT0", CDC_WDT0_BASE, PRIME_PID0, 0x06u, 0xFFu,
             "SP805");' \
    "HW_SCAN PASS implemented=22 reserved=3 absent=2" \
    "MISMATCH" \
    "the WDT identity returned by hardware no longer matches the scan contract, so enumeration must not be allowed to report PASS"

# 11. Bypass the pattern write in the restoring RW helper. RO and W1C checks
#     still run, but every RW row observes the old value instead of the test
#     pattern. This demonstrates that the gate reads REG_TEST's aggregate
#     result rather than accepting the presence of a table.
add_control \
    "freertos-reg-test-rw-write-bypassed" \
    "firmware" \
    "fw/freertos_fx1/src/hw_diag.c" \
    '  mmio_write(base, offset, pattern);
  const uint32_t actual = mmio_read(base, offset) & mask;' \
    '  mmio_write(base, offset, old_value);
  const uint32_t actual = mmio_read(base, offset) & mask;' \
    "REG_TEST PASS 37/37" \
    "REG_TEST FAIL" \
    "the restoring test never writes its requested pattern, so readback must fail even though the original value remains safely restored"

# ── the clean run ────────────────────────────────────────────────────────────
#
# Mandatory, and first. Every control below is judged by a marker disappearing
# or a check firing; without proof that the unmutated regression currently
# passes and produces those markers, a "detection" could just be a broken
# baseline.
echo
echo "clean run (Step ${acceptance_step}) must pass before any control counts"
clean_dir="${work_dir}/clean"
mkdir -p "${clean_dir}"
if ! env NOC_SOC_BIN="${noc_soc_bin}" LOG_DIR="${clean_dir}" \
        "${repo_root}/fw/freertos_noc_soc/run_step_regression.sh" \
        "${acceptance_step}" >"${clean_dir}/runner.log" 2>&1; then
    cat "${clean_dir}/runner.log" >&2
    fail "the unmutated regression does not pass; nothing below would mean anything"
fi
clean_log="${clean_dir}/noc_soc_fast.log"
[[ -f "${clean_log}" ]] || fail "the clean run produced no platform log"

# Every marker a control expects to lose must be shown to exist now.
for index in "${!names[@]}"; do
    case "${expect_runners[${index}]}" in
        "  transactions 0") continue ;;  # an ownership count, not a marker
    esac
    grep -Fq "${expect_runners[${index}]}" "${clean_log}" \
        || fail "the clean run does not produce '${expect_runners[${index}]}'," \
                "so ${names[${index}]} could not prove its absence means anything"

    # An absence expectation is only meaningful if the text is normally there.
    expect_log="${expect_logs[${index}]}"
    if [[ "${expect_log}" == "!"* ]]; then
        grep -Fq "${expect_log#!}" "${clean_log}" \
            || fail "the clean run does not produce '${expect_log#!}', so" \
                    "${names[${index}]} cannot use its absence as evidence"
    fi
done
echo "clean run PASS; every expected marker is present"

# ── the private scopes ───────────────────────────────────────────────────────
#
# `tflite-micro` is 300 MB and is not reachable from this platform or from a
# `NPU=0 TFLM=0` firmware build, so it is excluded rather than copied five
# times.
copy_firmware_scope() {
    local destination="$1"
    mkdir -p "${destination}/third_party"
    cp -r "${repo_root}/fw" "${destination}/fw"
    cp -r "${repo_root}/third_party/FreeRTOS-Kernel" \
          "${destination}/third_party/FreeRTOS-Kernel"
}

copy_platform_scope() {
    local destination="$1"
    mkdir -p "${destination}/third_party"
    local item
    for item in CMakeLists.txt cmake components cpu_models platforms tools fw; do
        cp -r "${repo_root}/${item}" "${destination}/${item}"
    done
    for item in riscv-vp FreeRTOS-Kernel README.md; do
        cp -r "${repo_root}/third_party/${item}" \
              "${destination}/third_party/${item}"
    done
    rm -rf "${destination}/build" "${destination}/components/*/build"
}

# ── run them ─────────────────────────────────────────────────────────────────
detected=0
missed=0

for index in "${!names[@]}"; do
    name="${names[${index}]}"
    scope="${scopes[${index}]}"
    evidence="${work_dir}/${name}"
    source_copy="${evidence}/src"
    mkdir -p "${evidence}"

    if [[ "${scope}" == "platform" ]]; then
        copy_platform_scope "${source_copy}"
    else
        copy_firmware_scope "${source_copy}"
    fi

    target_file="${source_copy}/${files[${index}]}"
    if [[ ! -f "${target_file}" ]]; then
        echo "MISSED ${name}: ${files[${index}]} is not in the ${scope} scope." >&2
        missed=$((missed + 1))
        continue
    fi

    # Exactly once. A needle that matches twice would mutate an arbitrary one of
    # them, and the control would be testing something nobody chose.
    if ! NEEDLE="${needles[${index}]}" REPLACEMENT="${replacements[${index}]}" \
         python3 - "${target_file}" <<'PY'
import os, sys
path = sys.argv[1]
needle = os.environ["NEEDLE"]
replacement = os.environ["REPLACEMENT"]
text = open(path).read()
count = text.count(needle)
if count != 1:
    sys.stderr.write("needle occurs %d times, expected exactly 1\n" % count)
    sys.exit(2)
open(path, "w").write(text.replace(needle, replacement, 1))
PY
    then
        echo "MISSED ${name}: the injection point is gone or ambiguous in" \
             "${files[${index}]}. The code changed; update this control." >&2
        missed=$((missed + 1))
        continue
    fi

    control_bin="${noc_soc_bin}"
    if [[ "${scope}" == "platform" ]]; then
        build_dir="${evidence}/build"
        if ! cmake -S "${source_copy}" -B "${build_dir}" \
                  -DCMAKE_BUILD_TYPE=Release \
                  -DCDC_BUILD_NOC_SOC=ON \
                  -DSYSTEMC_HOME="${systemc_home}" \
                  >"${evidence}/configure.log" 2>&1; then
            echo "MISSED ${name}: the mutated platform does not configure." >&2
            echo "        See ${evidence}/configure.log" >&2
            missed=$((missed + 1))
            continue
        fi
        if ! cmake --build "${build_dir}" --target noc_soc --parallel \
                  >"${evidence}/build.log" 2>&1; then
            # Not a detection. A mutation that will not compile proves nothing
            # about whether the regression covers the behaviour it broke.
            echo "MISSED ${name}: the mutated platform does not build." >&2
            echo "        A build failure is not detection. See" \
                 "${evidence}/build.log" >&2
            missed=$((missed + 1))
            continue
        fi
        control_bin="${build_dir}/platforms/noc_soc/noc_soc"
        [[ -x "${control_bin}" ]] || {
            echo "MISSED ${name}: the mutated build produced no executable." >&2
            missed=$((missed + 1)); continue; }
    fi

    # The firmware build happens inside the acceptance runner, which lives in
    # the private copy, so a firmware mutation is picked up automatically. Its
    # own host timeout and modeled-time bound apply unchanged: a control that
    # only ever produced a host timeout would not be evidence of anything.
    run_dir="${evidence}/run"
    mkdir -p "${run_dir}"
    env NOC_SOC_BIN="${control_bin}" LOG_DIR="${run_dir}" \
        "${source_copy}/fw/freertos_noc_soc/run_step_regression.sh" \
        "${acceptance_step}" >"${evidence}/runner.log" 2>&1
    status=$?

    if [[ ${status} -eq 0 ]]; then
        echo "MISSED ${name}: the regression still passes." >&2
        echo "        Defect: ${whys[${index}]}" >&2
        echo "        See ${evidence}/runner.log" >&2
        missed=$((missed + 1))
        continue
    fi
    if [[ ${status} -eq ${SKIP} ]]; then
        echo "MISSED ${name}: the mutated run reported a missing toolchain." >&2
        missed=$((missed + 1))
        continue
    fi

    # (a) The runner must name what it lost, not merely exit non-zero.
    if ! grep -Fq "${expect_runners[${index}]}" "${evidence}/runner.log"; then
        echo "MISSED ${name}: the regression failed, but never named" >&2
        echo "        '${expect_runners[${index}]}'. An unclassified failure is" >&2
        echo "        not detection. See ${evidence}/runner.log" >&2
        missed=$((missed + 1))
        continue
    fi

    # (b) The platform log must carry the control-specific diagnostic, so a
    #     failure for an unrelated reason cannot be counted.
    run_log="${run_dir}/noc_soc_fast.log"
    if [[ ! -f "${run_log}" ]]; then
        echo "MISSED ${name}: the mutated run produced no platform log." >&2
        missed=$((missed + 1))
        continue
    fi
    expect_log="${expect_logs[${index}]}"
    if [[ "${expect_log}" == "!"* ]]; then
        if grep -Fq "${expect_log#!}" "${run_log}"; then
            echo "MISSED ${name}: '${expect_log#!}' is still in the platform" >&2
            echo "        log, so the defect did not reach the behaviour this" >&2
            echo "        control exists to break. See ${run_log}" >&2
            missed=$((missed + 1))
            continue
        fi
    elif ! grep -Fq "${expect_log}" "${run_log}"; then
        echo "MISSED ${name}: the expected diagnostic" >&2
        echo "        '${expect_log}' is not in the platform log." >&2
        echo "        See ${run_log}" >&2
        missed=$((missed + 1))
        continue
    fi

    echo "detected ${name} (${scope} scope; '${expect_logs[${index}]}')"
    detected=$((detected + 1))
    # Keep only failing evidence; a detected control's copy is large and dull.
    rm -rf "${source_copy}" "${evidence}/build"
done

echo
echo "FreeRTOS negative controls: ${detected} detected, ${missed} missed"
echo "evidence kept under ${work_dir}"
if [[ ${missed} -ne 0 ]]; then
    echo "FAIL: every control must build, run, and fail for its own reason" >&2
    exit 1
fi
echo "FreeRTOS negative controls PASS"
