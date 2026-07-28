#!/bin/bash
# SPDX-License-Identifier: SHL-0.51
#
# Cross-check of the SystemC AXI sizing arithmetic against the unmodified
# frozen `floo_pkg` functions, which in turn call the locked `axi_pkg`.
#
# This is not a cycle comparison: the functions under test are pure. Both sides
# evaluate the same configuration list and the results are compared exactly.
# It exists because two details of the FlooNoC sizing are easy to transcribe
# wrongly and silently: `get_max_axi_payload_bits` adds one spare bit, and the
# channel widths use `InIdWidth` rather than `OutIdWidth`.
#
# No shim is used: the RTL compile comes from the Bender-generated file list.

set -euo pipefail

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/home/duyptt_HW/.local/bin:/usr/bin:/bin:$PATH

"$CC" -dumpfullversion
"$CXX" --version | head -n 1

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
model_root="$(cd -- "$script_dir/.." && pwd)"
build_root="${BUILD_ROOT:-/tmp/floo_noc_axi_sizing_crosscheck}"
systemc_build="$build_root/systemc"
verilator_build="$build_root/verilator"
configs="$model_root/tests/data/axi_sizing_configs.csv"
model_trace="$build_root/axi_sizing_model.csv"
rtl_trace="$build_root/axi_sizing_rtl.csv"

filelist_root="$build_root/filelist"
BUILD_ROOT="$filelist_root" "$script_dir/gen_rtl_filelist.sh"
filelist="$filelist_root/floo_verilator.f"

cmake -S "$model_root" -B "$systemc_build" --fresh \
  -DCMAKE_BUILD_TYPE=Debug \
  -DFLOO_NOC_MODEL_BUILD_TESTS=ON
cmake --build "$systemc_build" --target axi_sizing_trace --parallel
"$systemc_build/tests/axi_sizing_trace" "$configs" "$model_trace"

verilator --binary --timing -Wno-fatal \
  -Wno-DECLFILENAME \
  -Wno-PINCONNECTEMPTY \
  -Wno-UNUSEDSIGNAL \
  -Wno-UNUSEDPARAM \
  -Wno-WIDTHEXPAND \
  -Wno-WIDTHTRUNC \
  -Wno-UNSIGNED \
  --top-module tb_axi_sizing_trace \
  --Mdir "$verilator_build" \
  -f "$filelist" \
  "$script_dir/axi_sizing/tb_axi_sizing_trace.sv"

"$verilator_build/Vtb_axi_sizing_trace" \
  "+CONFIG_FILE=$configs" \
  "+TRACE_FILE=$rtl_trace"

python3 "$script_dir/compare_traces.py" \
  "$model_trace" "$rtl_trace" "axi-sizing" "configurations"
