#!/bin/bash
# SPDX-License-Identifier: SHL-0.51
#
# Cycle cross-check of the SystemC five-port router against the unmodified
# frozen FlooNoC `hw/floo_router.sv`, in the parameter set frozen in
# `docs/P0_SCOPE.md`.
#
# Unlike the leaf harnesses this one uses no shim at all. The RTL compile comes
# from the Bender-generated file list produced by `gen_rtl_filelist.sh`, so
# `floo_pkg`, `floo_route_select`, `floo_wormhole_arbiter`, `floo_vc_arbiter`,
# and every `common_cells` dependency are the real frozen sources.
#
# `floo_router.sv`'s `StableValidIn` and `StableValidOut` assertions stay
# enabled: `INC_ASSERT` is gated on `SYNTHESIS`, and the generated list only
# defines `TARGET_SYNTHESIS`. Any assertion failure is reported as a stimulus
# defect rather than suppressed.

set -euo pipefail

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/home/duyptt_HW/.local/bin:/usr/bin:/bin:$PATH

"$CC" -dumpfullversion
"$CXX" --version | head -n 1

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
model_root="$(cd -- "$script_dir/.." && pwd)"
floo_rtl_root="${FLOONOC_RTL_ROOT:-/home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC}"
build_root="${BUILD_ROOT:-/tmp/floo_noc_router_crosscheck}"
systemc_build="$build_root/systemc"
verilator_build="$build_root/verilator"
stimulus="$model_root/tests/data/router_stimulus.csv"
systemc_trace="$build_root/router_systemc_trace.csv"
rtl_trace="$build_root/router_rtl_trace.csv"
rtl_source="$floo_rtl_root/hw/floo_router.sv"
frozen_rtl_sha256="fb291e78a0344dc40bfa19a4a0cebef9f6042088a39e4de06fb8f77739b0deaa"

if [[ ! -f "$rtl_source" ]]; then
  echo "FlooNoC router RTL not found: $rtl_source" >&2
  exit 2
fi

actual_rtl_sha256="$(sha256sum "$rtl_source" | cut -d ' ' -f 1)"
if [[ "$actual_rtl_sha256" != "$frozen_rtl_sha256" ]]; then
  echo "FlooNoC router RTL does not match frozen revision 9a6972a." >&2
  echo "actual SHA-256:   $actual_rtl_sha256" >&2
  echo "expected SHA-256: $frozen_rtl_sha256" >&2
  exit 2
fi

# The full file list is the source of truth for the RTL compile.
filelist_root="$build_root/filelist"
BUILD_ROOT="$filelist_root" "$script_dir/gen_rtl_filelist.sh"
filelist="$filelist_root/floo_verilator.f"

if [[ ! -f "$filelist" ]]; then
  echo "generated file list not found: $filelist" >&2
  exit 2
fi

cmake -S "$model_root" -B "$systemc_build" --fresh \
  -DCMAKE_BUILD_TYPE=Debug \
  -DFLOO_NOC_MODEL_BUILD_TESTS=ON
cmake --build "$systemc_build" --target router_trace_sc --parallel
"$systemc_build/tests/router_trace_sc" "$stimulus" "$systemc_trace"

verilator --binary --timing -Wno-fatal \
  -Wno-DECLFILENAME \
  -Wno-PINCONNECTEMPTY \
  -Wno-UNUSEDSIGNAL \
  -Wno-UNUSEDPARAM \
  -Wno-WIDTHEXPAND \
  -Wno-WIDTHTRUNC \
  -Wno-UNSIGNED \
  --top-module tb_floo_router_trace \
  --Mdir "$verilator_build" \
  -I"$floo_rtl_root/hw/include" \
  -f "$filelist" \
  "$script_dir/floo_router/tb_floo_router_trace.sv"

assertion_log="$build_root/router_rtl_run.log"
"$verilator_build/Vtb_floo_router_trace" \
  "+STIM_FILE=$stimulus" \
  "+TRACE_FILE=$rtl_trace" 2>&1 | tee "$assertion_log"

if grep -qiE "StableValid(In|Out)|Assertion failed" "$assertion_log"; then
  echo "RTL protocol assertion fired; the stimulus violates the router's" >&2
  echo "input or output handshake contract. Fix the stimulus, not the model." >&2
  grep -iE "StableValid(In|Out)|Assertion failed" "$assertion_log" | head -5 >&2
  exit 1
fi

python3 "$script_dir/compare_traces.py" \
  "$systemc_trace" "$rtl_trace" "floo-router"
