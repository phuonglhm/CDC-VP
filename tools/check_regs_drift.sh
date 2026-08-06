#!/usr/bin/env bash
# check_regs_drift.sh - fail if the clean firmware register headers drift from
# the authoritative TLM models. Syntax-only compile of tools/check_regs_drift.cpp.
set -euo pipefail
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

CXX="${CXX:-/usr/bin/g++}"
SYSTEMC_INC="${SYSTEMC_INC:-/opt/systemc-2.3.4/include}"
drift_flags=()
if [[ "${CHECK_REGS_SKIP_NPU:-0}" == "1" ]]; then
    drift_flags+=(-DCDC_CHECK_REGS_SKIP_NPU=1)
fi

"$CXX" -std=c++17 -fsyntax-only \
    "${drift_flags[@]}" \
    -I"$SYSTEMC_INC" \
    -I"$repo/components/timer_tlm/include" \
    -I"$repo/components/i2c_tlm/include" \
    -I"$repo/components/dma_tlm/include" \
    -I"$repo/components/gpio_tlm/include" \
    -I"$repo/components/npu_tlm_v4_model/include" \
    -I"$repo/fw/common/include" \
    "$repo/tools/check_regs_drift.cpp"

if [[ "${CHECK_REGS_SKIP_NPU:-0}" == "1" ]]; then
    echo "register drift check: PASS (optional NPU scope explicitly skipped)"
else
    echo "register drift check: PASS"
fi
