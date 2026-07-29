#!/bin/bash
# SPDX-License-Identifier: SHL-0.51
#
# Cycle cross-check of the model's two-network mesh against a grid of the
# unmodified frozen `hw/floo_axi_router.sv`.
#
# This closes the last timing gap. The chimney is signed in both directions and
# the routers individually, but until now nothing compared what happens
# *between* nodes: route turns, packet routes held across several routers,
# contention for a shared output, and eject back-pressure propagating back into
# the network.
#
# It deliberately does **not** go through the FlooGen-generated top. That module
# exposes only AXI ports per endpoint, so comparing against it would need a
# full chimney at every node and would fold chimney behaviour into a mesh
# measurement. `floo_axi_router` has clean flit-level ports, so the grid is the
# isolatable unit. The generated netlist is still the authority for the wiring
# rule, which the testbench reproduces.
#
# No shim: the RTL compile comes from the Bender-generated file list.

set -euo pipefail

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/home/duyptt_HW/.local/bin:/usr/bin:/bin:$PATH

"$CC" -dumpfullversion
"$CXX" --version | head -n 1

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
model_root="$(cd -- "$script_dir/.." && pwd)"
floo_rtl_root="${FLOONOC_RTL_ROOT:-/home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC}"
build_root="${BUILD_ROOT:-/tmp/floo_noc_mesh_crosscheck}"
systemc_build="$build_root/systemc"
verilator_build="$build_root/verilator"
stimulus="$model_root/tests/data/mesh_stimulus.csv"
model_trace="$build_root/mesh_model.csv"
rtl_trace="$build_root/mesh_rtl.csv"

rtl_source="$floo_rtl_root/hw/floo_axi_router.sv"
frozen_rtl_sha256="087626ffc8859939f857e4ea6e68ef337c03eb09442d0d46c6ae5cac2004a0db"

if [[ ! -f "$rtl_source" ]]; then
  echo "FlooNoC axi-router RTL not found: $rtl_source" >&2
  exit 2
fi
actual_rtl_sha256="$(sha256sum "$rtl_source" | cut -d ' ' -f 1)"
if [[ "$actual_rtl_sha256" != "$frozen_rtl_sha256" ]]; then
  echo "FlooNoC axi-router RTL does not match frozen revision 9a6972a." >&2
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
cmake --build "$systemc_build" --target mesh_trace_sc --parallel
"$systemc_build/tests/mesh_trace_sc" "$stimulus" "$model_trace"

verilator --binary --timing -Wno-fatal \
  -Wno-DECLFILENAME \
  -Wno-PINCONNECTEMPTY \
  -Wno-UNUSEDSIGNAL \
  -Wno-UNUSEDPARAM \
  -Wno-WIDTHEXPAND \
  -Wno-WIDTHTRUNC \
  -Wno-UNSIGNED \
  --top-module tb_floo_mesh_trace \
  --Mdir "$verilator_build" \
  -I"$floo_rtl_root/hw/include" \
  -f "$filelist" \
  "$script_dir/mesh/tb_floo_mesh_trace.sv"

run_log="$build_root/mesh_rtl_run.log"
"$verilator_build/Vtb_floo_mesh_trace" \
  "+STIM_FILE=$stimulus" \
  "+TRACE_FILE=$rtl_trace" 2>&1 | tee "$run_log"

# `floo_router` keeps its own `StableValidIn`/`StableValidOut` assertions
# enabled; a stimulus that violated the protocol must fail the run rather than
# produce a comparable trace.
if grep -qiE "Assertion failed|%Error" "$run_log"; then
  echo "RTL reported an assertion or error during the run." >&2
  grep -iE "Assertion failed|%Error" "$run_log" | head -5 >&2
  exit 1
fi

python3 "$script_dir/compare_traces.py" \
  "$model_trace" "$rtl_trace" "mesh" "node-cycles"
