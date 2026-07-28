#!/bin/bash
# SPDX-License-Identifier: SHL-0.51
#
# Cross-check of the SystemC chimney request-path flit assembly against the
# unmodified frozen `hw/floo_axi_chimney.sv`.
#
# Scope: **flit content and per-beat ordering**, not chimney timing. The
# stimulus issues one AXI beat at a time and the testbench drains the resulting
# flit before issuing the next, so the emitted order follows the beat order and
# does not depend on the chimney's internal request arbiter. Chimney timing,
# arbitration, back-pressure behaviour, and the response path are still
# uncompared.
#
# No shim is used: the RTL compile comes from the Bender-generated file list,
# so `floo_pkg`, the chimney, its reorder-buffer wrapper, the meta buffer, and
# every `common_cells`/`axi` dependency are the real frozen sources.

set -euo pipefail

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/home/duyptt_HW/.local/bin:/usr/bin:/bin:$PATH

"$CC" -dumpfullversion
"$CXX" --version | head -n 1

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
model_root="$(cd -- "$script_dir/.." && pwd)"
floo_rtl_root="${FLOONOC_RTL_ROOT:-/home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC}"
build_root="${BUILD_ROOT:-/tmp/floo_noc_chimney_req_crosscheck}"
systemc_build="$build_root/systemc"
verilator_build="$build_root/verilator"
stimulus="$model_root/tests/data/chimney_req_stimulus.csv"
model_trace="$build_root/chimney_req_model.csv"
rtl_trace="$build_root/chimney_req_rtl.csv"
rtl_source="$floo_rtl_root/hw/floo_axi_chimney.sv"
frozen_rtl_sha256="5f165d7f49cfa0512eef964c74d861337850b140d845395d64c971366b5e3a18"

if [[ ! -f "$rtl_source" ]]; then
  echo "FlooNoC chimney RTL not found: $rtl_source" >&2
  exit 2
fi

actual_rtl_sha256="$(sha256sum "$rtl_source" | cut -d ' ' -f 1)"
if [[ "$actual_rtl_sha256" != "$frozen_rtl_sha256" ]]; then
  echo "FlooNoC chimney RTL does not match frozen revision 9a6972a." >&2
  echo "actual SHA-256:   $actual_rtl_sha256" >&2
  echo "expected SHA-256: $frozen_rtl_sha256" >&2
  exit 2
fi

filelist_root="$build_root/filelist"
BUILD_ROOT="$filelist_root" "$script_dir/gen_rtl_filelist.sh"
filelist="$filelist_root/floo_verilator.f"

cmake -S "$model_root" -B "$systemc_build" --fresh \
  -DCMAKE_BUILD_TYPE=Debug \
  -DFLOO_NOC_MODEL_BUILD_TESTS=ON
cmake --build "$systemc_build" --target chimney_req_trace_sc --parallel
"$systemc_build/tests/chimney_req_trace_sc" "$stimulus" "$model_trace"

verilator --binary --timing -Wno-fatal \
  -Wno-DECLFILENAME \
  -Wno-PINCONNECTEMPTY \
  -Wno-UNUSEDSIGNAL \
  -Wno-UNUSEDPARAM \
  -Wno-WIDTHEXPAND \
  -Wno-WIDTHTRUNC \
  -Wno-UNSIGNED \
  --top-module tb_floo_axi_chimney_req_trace \
  --Mdir "$verilator_build" \
  -I"$floo_rtl_root/hw/include" \
  -f "$filelist" \
  "$script_dir/axi_chimney/tb_floo_axi_chimney_req_trace.sv"

run_log="$build_root/chimney_req_rtl_run.log"
"$verilator_build/Vtb_floo_axi_chimney_req_trace" \
  "+STIM_FILE=$stimulus" \
  "+TRACE_FILE=$rtl_trace" 2>&1 | tee "$run_log"

if grep -qiE "Assertion failed|%Error" "$run_log"; then
  echo "RTL reported an assertion or error during the run." >&2
  grep -iE "Assertion failed|%Error" "$run_log" | head -5 >&2
  exit 1
fi

python3 "$script_dir/compare_traces.py" \
  "$model_trace" "$rtl_trace" "chimney-request" "flits"
