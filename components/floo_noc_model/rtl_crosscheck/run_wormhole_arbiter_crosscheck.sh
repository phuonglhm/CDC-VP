#!/bin/bash
# SPDX-License-Identifier: SHL-0.51
#
# Cycle cross-check of the SystemC wormhole arbiter against the unmodified
# frozen FlooNoC `hw/floo_wormhole_arbiter.sv`, over the locked common_cells
# `rr_arb_tree`, `lzc`, and `cf_math_pkg`.
#
# No behavioural SystemVerilog replacement is used. The only local file in the
# compile is an intentionally empty `floo_pkg`, which satisfies the arbiter's
# unused import and supplies no declaration.
#
# Three configurations are covered:
#   NumRoutes 5 -> the five-port router configuration (non power-of-two tree)
#   NumRoutes 4 -> a full binary tree with no out-of-range leaves
#   NumRoutes 2 -> the single-level tree used by the standalone unit test

set -euo pipefail

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

"$CC" -dumpfullversion
"$CXX" --version | head -n 1

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
model_root="$(cd -- "$script_dir/.." && pwd)"
floo_rtl_root="${FLOONOC_RTL_ROOT:-/home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC}"
build_root="${BUILD_ROOT:-/tmp/floo_noc_wormhole_arbiter_crosscheck}"
systemc_build="$build_root/systemc"
stimulus="$model_root/tests/data/wormhole_arbiter_stimulus.csv"
rtl_source="$floo_rtl_root/hw/floo_wormhole_arbiter.sv"
frozen_rtl_sha256="ab4c2272e069cdf611b909b1dfbca4c55b13906c1bd4c12d6ae9ff12db1445b0"

if [[ ! -f "$rtl_source" ]]; then
  echo "FlooNoC wormhole-arbiter RTL not found: $rtl_source" >&2
  exit 2
fi

actual_rtl_sha256="$(sha256sum "$rtl_source" | cut -d ' ' -f 1)"
if [[ "$actual_rtl_sha256" != "$frozen_rtl_sha256" ]]; then
  echo "FlooNoC wormhole-arbiter RTL does not match frozen revision 9a6972a." >&2
  echo "actual SHA-256:   $actual_rtl_sha256" >&2
  echo "expected SHA-256: $frozen_rtl_sha256" >&2
  exit 2
fi

deps_output="$("$script_dir/fetch_rtl_deps.sh")"
echo "$deps_output"
common_cells_root="$(sed -n 's/^COMMON_CELLS_ROOT=//p' <<< "$deps_output")"
if [[ -z "$common_cells_root" ]]; then
  echo "fetch_rtl_deps.sh did not report COMMON_CELLS_ROOT" >&2
  exit 2
fi

cmake -S "$model_root" -B "$systemc_build" --fresh \
  -DCMAKE_BUILD_TYPE=Debug \
  -DFLOO_NOC_MODEL_BUILD_TESTS=ON
cmake --build "$systemc_build" --target arbiter_trace_sc --parallel

status=0
for routes in 5 4 2; do
  systemc_trace="$build_root/wormhole_arbiter_n${routes}_systemc_trace.csv"
  rtl_trace="$build_root/wormhole_arbiter_n${routes}_rtl_trace.csv"
  verilator_build="$build_root/verilator_n${routes}"

  "$systemc_build/tests/arbiter_trace_sc" "$stimulus" "$systemc_trace" "$routes"

  verilator --binary --timing -Wall -Wno-fatal \
    -Wno-DECLFILENAME \
    -Wno-PINCONNECTEMPTY \
    -Wno-UNUSEDSIGNAL \
    -Wno-UNUSEDPARAM \
    -GNumRoutes="$routes" \
    --top-module tb_wormhole_arbiter_trace \
    --Mdir "$verilator_build" \
    -I"$common_cells_root/include" \
    "$common_cells_root/src/cf_math_pkg.sv" \
    "$common_cells_root/src/lzc.sv" \
    "$common_cells_root/src/rr_arb_tree.sv" \
    "$script_dir/wormhole_arbiter/floo_pkg_empty.sv" \
    "$rtl_source" \
    "$script_dir/wormhole_arbiter/tb_wormhole_arbiter_trace.sv"

  "$verilator_build/Vtb_wormhole_arbiter_trace" \
    "+STIM_FILE=$stimulus" \
    "+TRACE_FILE=$rtl_trace"

  if ! python3 "$script_dir/compare_traces.py" \
      "$systemc_trace" "$rtl_trace" "wormhole-arbiter routes $routes"; then
    status=1
  fi
done

exit "$status"
