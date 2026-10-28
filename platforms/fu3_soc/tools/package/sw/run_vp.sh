#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Run the VP_FU3_V1.0 FU3 SoC VP with the example firmware built from
# sw/bootloader/sources/VP_FU3_V1.0 (make) into sw/bootloader/build/VP_FU3_V1.0.

set -uo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "${here}/../../../.." && pwd)"
version="VP_FU3_V1.0"
vp="${root}/vp/bin/${version}/vp_fu3_soc"
data="${root}/vp/configs/${version}/data"
build_dir="${root}/sw/bootloader/build/${version}"

usage()
{
    cat <<'EOF'
Usage:
  ./run_vp.sh smoke                  run the three demos and check their verdicts (default)
  ./run_vp.sh console [VP args]      Ibex console: straps, reset cause, console input, NOR via XIP
  ./run_vp.sh handoff [VP args]      Ibex software reset, CVA6 handoff, CVA6 FP result
  ./run_vp.sh wake [VP args]         WFI wakes on both CPUs, SWIRQ, watchdog reset
  ./run_vp.sh gdb [VP args]          the handoff demo waiting for GDB on ports 3333 (CVA6) / 3334 (Ibex)
  ./run_vp.sh run <rom.elf> [VP args]  any image: ROM content first, then e.g. --elf <cva6.elf>
  ./run_vp.sh version                VP and package provenance

Build the images first: make -C ../../sources/VP_FU3_V1.0
VP options: vp_fu3_soc --help, vp/doc/VP_FU3_V1.0/README.md.
EOF
}

[[ -x "${vp}" ]] || { echo "ERROR: VP executable is missing: ${vp}" >&2; exit 1; }
image() {
    [[ -f "${build_dir}/$1" ]] || { echo "ERROR: ${build_dir}/$1 is missing: run make in sources/${version}" >&2; exit 1; }
    echo "${build_dir}/$1"
}
vp_run() { env -u LD_LIBRARY_PATH "${vp}" "$@"; }

console() {
    vp_run --rom "$(image t20_console.elf)" --mode 10 --boot-pin 1 --uart-rx-file "${data}/t20_rx.txt" \
        --uart-rx-delay-us 400 --flash "${data}/t20_flash.bin" --expect-uart PASS --fail-uart FAIL --timeout-ms 20 "$@"
}
handoff() {
    vp_run --rom "$(image t19_ibex.elf)" --elf "$(image t19_cva6.elf)" --expect-uart PASS --fail-uart FAIL \
        --timeout-ms 50 "$@"
}
wake() {
    vp_run --rom "$(image t21_ibex.elf)" --elf "$(image t21_cva6.elf)" --expect-uart PASS --fail-uart FAIL \
        --timeout-ms 50 "$@"
}

# smoke NAME MARKER -- the demo function; log in the build directory.
smoke_one() {
    local name=$1 marker=$2
    local log="${build_dir}/${name}.run.log"
    "$name" > "${log}" 2>&1
    local rc=$?
    if [[ $rc -eq 0 ]] && tr '\n' ' ' < "${log}" | grep -Eq -- "${marker}" && ! grep -Eq "Warning|Error" "${log}"; then
        echo "ok   ${name}"
        return 0
    fi
    echo "FAIL ${name} (exit ${rc}, log ${log})"
    tail -5 "${log}"
    return 1
}

mode=${1:-smoke}
[[ $# -gt 0 ]] && shift
case "${mode}" in
smoke)
    failed=0
    smoke_one console "straps MODE=10 BOOT=1 .*rx: hello vp .*flash 0x44332211 .*vp_fu3_soc: PASS" || failed=1
    smoke_one handoff "T19 boot 2 .*cva6 result 0x40580000 .*vp_fu3_soc: PASS" || failed=1
    smoke_one wake "cva6 wfi 0x80000007 .*reset cause 0x00000009 .*vp_fu3_soc: PASS" || failed=1
    exit ${failed}
    ;;
console) console "$@" ;;
handoff) handoff "$@" ;;
wake) wake "$@" ;;
gdb)
    echo "GDB: riscv-none-elf-gdb $(image t19_cva6.elf) -ex 'target remote :3333'   (CVA6)"
    echo "     riscv-none-elf-gdb $(image t19_ibex.elf) -ex 'target remote :3334'   (Ibex)"
    handoff --gdb-cva6 3333 --gdb-ibex 3334 "$@"
    ;;
run)
    [[ $# -ge 1 ]] || { usage; exit 4; }
    rom=$1
    shift
    vp_run --rom "${rom}" "$@"
    ;;
version) vp_run --version ;;
-h|--help|help) usage ;;
*) usage; exit 4 ;;
esac
