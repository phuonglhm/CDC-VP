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

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="$ROOT/virtual_platform/build"
if [ $# -gt 0 ]; then BUILD="$1"; fi
: "${SYSTEMC_HOME:?set SYSTEMC_HOME to a SystemC 2.3.4 install (virtual_platform/scripts/install_systemc.sh)}"

GEN=()
command -v ninja >/dev/null 2>&1 && GEN=(-G Ninja)
cmake -S "$ROOT/virtual_platform" -B "$BUILD" "${GEN[@]}" -DSYSTEMC_HOME="$SYSTEMC_HOME" >/dev/null
cmake --build "$BUILD"

ctest --test-dir "$BUILD" -L "unit|scenario|platform|riscv" -j"$(nproc 2>/dev/null || echo 4)" \
      --output-on-failure --timeout 1800

# the markers, re-checked on the programs' own output
EXE=""
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) EXE=.exe ;; esac
C="$BUILD/components/eth_mac_tlm"
"$C/test_eth_mac_tlm$EXE" | grep -q "^=== eth_mac_tlm: [0-9]* checks, 0 failures ===$"
"$C/test_core_golden$EXE" "$ROOT/virtual_platform/vectors/core.txt" | grep -q "^=== core golden: [0-9]* cases, 0 failures ===$"
"$C/test_scenarios$EXE" "$ROOT/virtual_platform/vectors/scenarios" | grep -q "^=== scenarios: [0-9]* scenarios, [0-9]* checks, 0 failures"
"$BUILD/platforms/eth_soc/vp_eth_echo$EXE" --frames 64 | grep -q "^ECHO PASS$"
"$BUILD/platforms/eth_soc/vp_eth_echo$EXE" --burst 2000 | grep -q "^BURST PASS$"
if [ -x "$BUILD/cdc_vp/riscv_eth_soc" ]; then
    "$BUILD/cdc_vp/riscv_eth_soc" --fw "$BUILD/cdc_vp/eth_bringup/eth_bringup.elf" --sim-ms 200 \
        | grep -q "^FW: ALL PASS$"
fi
echo "REGRESSION: ALL PASS"
