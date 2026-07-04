#!/usr/bin/env bash
# check_regs_drift.sh - fail if the clean firmware register headers drift from
# the authoritative TLM models. Syntax-only compile of tools/check_regs_drift.cpp.
set -euo pipefail
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

CXX="${CXX:-/usr/bin/g++}"
SYSTEMC_INC="${SYSTEMC_INC:-/opt/systemc-2.3.4/include}"

"$CXX" -std=c++17 -fsyntax-only \
    -I"$SYSTEMC_INC" \
    -I"$repo/components/timer_tlm/include" \
    -I"$repo/components/i2c_tlm/include" \
    -I"$repo/components/dma_tlm/include" \
    -I"$repo/fw/common/include" \
    "$repo/tools/check_regs_drift.cpp"

echo "register drift check: PASS"
