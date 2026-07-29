#!/bin/bash
# SPDX-License-Identifier: SHL-0.51
#
# Cycle cross-check of the SystemC `NoRoB` ordering gate against the unmodified
# frozen `hw/floo_rob_wrapper.sv`, over the locked axi `axi_demux_id_counters`
# and common_cells `delta_counter`.
#
# Unlike the two chimney cross-checks, which compare flit *content*, this one
# compares a handshake per cycle. `NoRoB` is an admission rule, so its only
# observable is the request-side `ready` it presents upstream, plus the counter
# bank behind it.
#
# `floo_rob_wrapper.sv` is a leaf module, so it is instantiated directly rather
# than through the chimney. That keeps the comparison free of the chimney's
# arbitration and cuts, which are a separate and still-unsigned concern.
#
# No shim is used: the RTL compile comes from the Bender-generated file list,
# so `floo_pkg` and every `common_cells`/`axi` dependency is the real frozen
# source.

set -euo pipefail

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/home/duyptt_HW/.local/bin:/usr/bin:/bin:$PATH

"$CC" -dumpfullversion
"$CXX" --version | head -n 1

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
model_root="$(cd -- "$script_dir/.." && pwd)"
floo_rtl_root="${FLOONOC_RTL_ROOT:-/home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC}"
build_root="${BUILD_ROOT:-/tmp/floo_noc_rob_crosscheck}"
systemc_build="$build_root/systemc"
verilator_build="$build_root/verilator"
stimulus="$model_root/tests/data/rob_stimulus.csv"
model_trace="$build_root/rob_model.csv"
rtl_trace="$build_root/rob_rtl.csv"

rtl_source="$floo_rtl_root/hw/floo_rob_wrapper.sv"
frozen_rtl_sha256="cb043909bcc69143cf743f810e384f84605df9d1542eb3f20bf4f065e141953e"

# The admission rule is only half in the wrapper; the counter semantics that
# decide `full_o` live in the axi dependency, so that file is pinned too.
counters_source="$floo_rtl_root/.bender/git/checkouts/axi-a612f580cfee89bc/src/axi_demux_simple.sv"
frozen_counters_sha256="e1a2fe30a2fa2f031198d3e317f32484913625d136d47d03ee05c63a774d597d"

check_frozen() {
  local path="$1"
  local expected="$2"
  local label="$3"

  if [[ ! -f "$path" ]]; then
    echo "$label RTL not found: $path" >&2
    exit 2
  fi
  local actual
  actual="$(sha256sum "$path" | cut -d ' ' -f 1)"
  if [[ "$actual" != "$expected" ]]; then
    echo "$label RTL does not match frozen revision 9a6972a." >&2
    echo "actual SHA-256:   $actual" >&2
    echo "expected SHA-256: $expected" >&2
    exit 2
  fi
}

check_frozen "$rtl_source" "$frozen_rtl_sha256" "FlooNoC rob-wrapper"
check_frozen "$counters_source" "$frozen_counters_sha256" "axi demux-counters"

filelist_root="$build_root/filelist"
BUILD_ROOT="$filelist_root" "$script_dir/gen_rtl_filelist.sh"
filelist="$filelist_root/floo_verilator.f"

cmake -S "$model_root" -B "$systemc_build" --fresh \
  -DCMAKE_BUILD_TYPE=Debug \
  -DFLOO_NOC_MODEL_BUILD_TESTS=ON
cmake --build "$systemc_build" --target rob_trace_sc --parallel
"$systemc_build/tests/rob_trace_sc" "$stimulus" "$model_trace"

verilator --binary --timing -Wno-fatal \
  -Wno-DECLFILENAME \
  -Wno-PINCONNECTEMPTY \
  -Wno-UNUSEDSIGNAL \
  -Wno-UNUSEDPARAM \
  -Wno-WIDTHEXPAND \
  -Wno-WIDTHTRUNC \
  -Wno-UNSIGNED \
  --top-module tb_floo_rob_wrapper_trace \
  --Mdir "$verilator_build" \
  -I"$floo_rtl_root/hw/include" \
  -f "$filelist" \
  "$script_dir/rob/tb_floo_rob_wrapper_trace.sv"

run_log="$build_root/rob_rtl_run.log"
"$verilator_build/Vtb_floo_rob_wrapper_trace" \
  "+STIM_FILE=$stimulus" \
  "+TRACE_FILE=$rtl_trace" 2>&1 | tee "$run_log"

# The counter bank carries an underflow assertion. A stimulus that answered a
# transaction that was never admitted would trip it, and that must fail the run
# rather than quietly produce a comparable trace.
if grep -qiE "Assertion failed|%Error" "$run_log"; then
  echo "RTL reported an assertion or error during the run." >&2
  grep -iE "Assertion failed|%Error" "$run_log" | head -5 >&2
  exit 1
fi

python3 "$script_dir/compare_traces.py" \
  "$model_trace" "$rtl_trace" "norob-ordering" "cycles"
