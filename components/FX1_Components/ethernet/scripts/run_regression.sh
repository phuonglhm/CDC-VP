#!/usr/bin/env bash
# =============================================================================
# Integrated Circuits and Systems laboratory
# Author: Hung Nguyen <ngthung@hcmut.edu.vn>
# Module: virtual_platform.scripts.run_regression
# Description: Configure, build and run the whole VP regression, with markers.
# Changelog:
#   2026-09-29: Initial version.
# =============================================================================
#
#   SYSTEMC_HOME=/opt/systemc-2.3.4 virtual_platform/scripts/run_regression.sh [build-dir]
#
# 1. configure + build (virtual_platform/, Ninja if present)
# 2. ctest: unit (register contract, frame core vs the reference vectors),
#    scenario (IP level vs the reference expectations), platform (the driver
#    on two nodes), and riscv when enabled (Linux, cdc_vp/run_in_docker.sh)
# 3. the anchored markers of each layer, re-checked on the programs' own
#    output, so a program that exits 0 while spinning in a failure branch is
#    not taken for a pass
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"
if [ $# -gt 0 ]; then BUILD="$1"; fi
: "${SYSTEMC_HOME:?set SYSTEMC_HOME to a SystemC 2.3.4 install (virtual_platform/scripts/install_systemc.sh)}"

GEN=()
command -v ninja >/dev/null 2>&1 && GEN=(-G Ninja)
cmake -S "$ROOT" -B "$BUILD" "${GEN[@]}" -DSYSTEMC_HOME="$SYSTEMC_HOME" >/dev/null
cmake --build "$BUILD"

ctest --no-tests=error --test-dir "$BUILD" -L "unit|scenario|platform|riscv" -j"$(nproc 2>/dev/null || echo 4)" \
      --output-on-failure --timeout 1800

# the markers, re-checked on the programs' own output. The whole output is
# captured before grep looks at it: `prog | grep -q` under pipefail can fail
# on SIGPIPE when grep exits at the marker while prog still writes.
EXE=""
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) EXE=.exe ;; esac
C="$BUILD/tests"
marker() {                                          # marker <regex> <program> [args...]
    local re="$1" out rc=0
    shift
    out="$("$@" 2>&1)" || rc=$?
    if [ "$rc" -ne 0 ] || ! grep -q -- "$re" <<<"$out"; then
        echo "REGRESSION: FAIL: $* exited $rc; expected marker: $re" >&2
        printf '%s\n' "$out" | tail -n 20 >&2
        exit 1
    fi
    echo "marker ok: $re"
}
marker "^=== eth_mac_tlm: [0-9]* checks, 0 failures ===$" "$C/test_eth_mac_tlm$EXE"
marker "^=== core golden: [0-9]* cases, 0 failures ===$" "$C/test_core_golden$EXE" "$ROOT/vectors/core.txt"
marker "^=== scenarios: [0-9]* scenarios, [0-9]* checks, 0 failures" "$C/test_scenarios$EXE" "$ROOT/vectors/scenarios"
marker "^ECHO PASS$" "$BUILD/examples/eth_soc/vp_eth_echo$EXE" --frames 64
marker "^BURST PASS$" "$BUILD/examples/eth_soc/vp_eth_echo$EXE" --burst 2000
if [ -x "$BUILD/examples/cdc_vp/riscv_eth_soc" ]; then
    marker "^FW: ALL PASS$" "$BUILD/examples/cdc_vp/riscv_eth_soc" \
        --fw "$BUILD/examples/cdc_vp/eth_bringup/eth_bringup.elf" --sim-ms 200
fi
echo "REGRESSION: ALL PASS"
