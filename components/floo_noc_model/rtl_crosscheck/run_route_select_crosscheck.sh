#!/bin/bash
# SPDX-License-Identifier: SHL-0.51

set -euo pipefail

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

"$CC" -dumpfullversion
"$CXX" --version | head -n 1

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
model_root="$(cd -- "$script_dir/.." && pwd)"
floo_rtl_root="${FLOONOC_RTL_ROOT:-/home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC}"
build_root="${BUILD_ROOT:-/tmp/floo_noc_route_crosscheck}"
systemc_build="$build_root/systemc"
verilator_build="$build_root/verilator"
stimulus="$model_root/tests/data/route_select_stimulus.csv"
systemc_trace="$build_root/route_select_systemc_trace.csv"
rtl_trace="$build_root/route_select_rtl_trace.csv"
rtl_source="$floo_rtl_root/hw/floo_route_select.sv"
frozen_rtl_sha256="234fefcb0cee853ee93299b16caee0811e7f142237f205ee399d3f9c53073f1e"

if [[ ! -f "$rtl_source" ]]; then
  echo "FlooNoC route-selector RTL not found: $rtl_source" >&2
  exit 2
fi

actual_rtl_sha256="$(sha256sum "$rtl_source" | cut -d ' ' -f 1)"
if [[ "$actual_rtl_sha256" != "$frozen_rtl_sha256" ]]; then
  echo "FlooNoC route-selector RTL does not match frozen revision 9a6972a." >&2
  echo "actual SHA-256:   $actual_rtl_sha256" >&2
  echo "expected SHA-256: $frozen_rtl_sha256" >&2
  exit 2
fi

cmake -S "$model_root" -B "$systemc_build" --fresh \
  -DCMAKE_BUILD_TYPE=Debug \
  -DFLOO_NOC_MODEL_BUILD_TESTS=ON
cmake --build "$systemc_build" --target route_trace_sc --parallel
"$systemc_build/tests/route_trace_sc" "$stimulus" "$systemc_trace"

verilator --binary --timing -Wall -Wno-fatal \
  -Wno-DECLFILENAME \
  -Wno-PINCONNECTEMPTY \
  -Wno-UNUSEDSIGNAL \
  -Wno-UNUSEDPARAM \
  -DTARGET_SYNTHESIS \
  --top-module tb_route_select_trace \
  --Mdir "$verilator_build" \
  -I"$script_dir/route_select/shim" \
  "$script_dir/route_select/floo_pkg.sv" \
  "$rtl_source" \
  "$script_dir/route_select/tb_route_select_trace.sv"

"$verilator_build/Vtb_route_select_trace" \
  "+STIM_FILE=$stimulus" \
  "+TRACE_FILE=$rtl_trace"

python3 "$script_dir/compare_traces.py" "$systemc_trace" "$rtl_trace"
