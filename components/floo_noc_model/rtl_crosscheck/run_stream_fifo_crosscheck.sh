#!/bin/bash
# SPDX-License-Identifier: SHL-0.51
#
# Cycle cross-check of the SystemC `stream_fifo_optimal_wrap` mirror against the
# unmodified common_cells RTL that FlooNoC `hw/floo_router.sv` instantiates.
#
# Both wrap branches are covered:
#   Depth 2 -> spill_register_flushable
#   Depth 4 -> stream_fifo -> fifo_v3
#
# No behavioural SystemVerilog replacement is used. The RTL comes from the
# revision locked in the frozen FlooNoC `Bender.lock`, materialised by
# `fetch_rtl_deps.sh`.

set -euo pipefail

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

"$CC" -dumpfullversion
"$CXX" --version | head -n 1

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
model_root="$(cd -- "$script_dir/.." && pwd)"
build_root="${BUILD_ROOT:-/tmp/floo_noc_stream_fifo_crosscheck}"
systemc_build="$build_root/systemc"
stimulus="$model_root/tests/data/stream_fifo_stimulus.csv"

# Materialise and verify the locked dependency revision.
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
cmake --build "$systemc_build" --target fifo_trace_sc --parallel

status=0
for depth in 2 4; do
  systemc_trace="$build_root/stream_fifo_d${depth}_systemc_trace.csv"
  rtl_trace="$build_root/stream_fifo_d${depth}_rtl_trace.csv"
  verilator_build="$build_root/verilator_d${depth}"

  "$systemc_build/tests/fifo_trace_sc" "$stimulus" "$systemc_trace" "$depth"

  verilator --binary --timing -Wall -Wno-fatal \
    -Wno-DECLFILENAME \
    -Wno-PINCONNECTEMPTY \
    -Wno-UNUSEDSIGNAL \
    -Wno-UNUSEDPARAM \
    -GDepth="$depth" \
    --top-module tb_stream_fifo_trace \
    --Mdir "$verilator_build" \
    -I"$common_cells_root/include" \
    "$common_cells_root/src/fifo_v3.sv" \
    "$common_cells_root/src/stream_fifo.sv" \
    "$common_cells_root/src/spill_register_flushable.sv" \
    "$common_cells_root/src/stream_fifo_optimal_wrap.sv" \
    "$script_dir/stream_fifo/tb_stream_fifo_trace.sv"

  "$verilator_build/Vtb_stream_fifo_trace" \
    "+STIM_FILE=$stimulus" \
    "+TRACE_FILE=$rtl_trace"

  if ! python3 "$script_dir/compare_traces.py" \
      "$systemc_trace" "$rtl_trace" "stream-fifo depth $depth"; then
    status=1
  fi
done

exit "$status"
