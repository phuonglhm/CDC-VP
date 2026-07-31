#!/bin/bash
# SPDX-License-Identifier: SHL-0.51
#
# Cycle cross-check of the chimney's **manager-side response path** against the
# unmodified frozen `hw/floo_axi_chimney.sv`. This is Step A-1: the fourth and
# last chimney quadrant, and the twelfth cross-check.
#
# Why it is separate from the other three chimney runners: all of them hold the
# manager response link idle. `tb_floo_axi_chimney_rsp_timing_trace.sv` states
# it outright — `floo_rsp_in.valid = 1'b0`. So `floo_rsp_i` had never been
# driven against RTL, and nothing downstream of it was signed:
#
#   * the B/R channel decode off `hdr.axi_ch`;
#   * `floo_rsp_o.ready = axi_ready_out[hdr.axi_ch]`, the per-channel ready
#     select;
#   * the reorder-buffer counter release, and specifically that a multi-beat
#     read burst releases it **once**, on `RLAST`.
#
# That last one is not hypothetical. The model popped the counter on every R
# beat, and three signed cross-checks could not see it, because none of them
# ever sent an R beat. `rlast-ignored` in `run_negative_controls.sh` reinjects
# it.
#
# The model side is a composition of `axi_chimney_request` (which owns the
# counter bank) and `axi_chimney_manager_response` (which decodes the flit and
# drives the release). Signing the composition is the point — the counter bank
# on its own is already signed by `run_rob_crosscheck.sh`.
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
floo_rtl_root="${FLOONOC_RTL_ROOT:-/home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC}"
build_root="${BUILD_ROOT:-/tmp/floo_noc_chimney_mgr_rsp_crosscheck}"
systemc_build="$build_root/systemc"
verilator_build="$build_root/verilator"
stimulus="$model_root/tests/data/chimney_mgr_rsp_stimulus.csv"
model_trace="$build_root/chimney_mgr_rsp_model.csv"
rtl_trace="$build_root/chimney_mgr_rsp_rtl.csv"
rtl_source="$floo_rtl_root/hw/floo_axi_chimney.sv"
rob_source="$floo_rtl_root/hw/floo_rob_wrapper.sv"
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

# The counter release this cross-check is about lives in the `NoRoB` branch of
# the reorder-buffer wrapper, not in the chimney file, so that file is guarded
# too. Guarding only `floo_axi_chimney.sv` would let the behaviour under test
# change without the runner noticing.
if [[ ! -f "$rob_source" ]]; then
  echo "FlooNoC RoB wrapper RTL not found: $rob_source" >&2
  exit 2
fi
frozen_rob_sha256="cb043909bcc69143cf743f810e384f84605df9d1542eb3f20bf4f065e141953e"
actual_rob_sha256="$(sha256sum "$rob_source" | cut -d ' ' -f 1)"
if [[ "$actual_rob_sha256" != "$frozen_rob_sha256" ]]; then
  echo "FlooNoC RoB wrapper RTL does not match frozen revision 9a6972a." >&2
  echo "actual SHA-256:   $actual_rob_sha256" >&2
  echo "expected SHA-256: $frozen_rob_sha256" >&2
  exit 2
fi

filelist_root="$build_root/filelist"
BUILD_ROOT="$filelist_root" "$script_dir/gen_rtl_filelist.sh"
filelist="$filelist_root/floo_verilator.f"

cmake -S "$model_root" -B "$systemc_build" --fresh \
  -DCMAKE_BUILD_TYPE=Debug \
  -DFLOO_NOC_MODEL_BUILD_TESTS=ON
cmake --build "$systemc_build" --target chimney_mgr_rsp_trace_sc --parallel
"$systemc_build/tests/chimney_mgr_rsp_trace_sc" "$stimulus" "$model_trace"

verilator --binary --timing -Wno-fatal \
  -Wno-DECLFILENAME \
  -Wno-PINCONNECTEMPTY \
  -Wno-UNUSEDSIGNAL \
  -Wno-UNUSEDPARAM \
  -Wno-WIDTHEXPAND \
  -Wno-WIDTHTRUNC \
  -Wno-UNSIGNED \
  --top-module tb_floo_axi_chimney_mgr_rsp_trace \
  --Mdir "$verilator_build" \
  -I"$floo_rtl_root/hw/include" \
  -f "$filelist" \
  "$script_dir/axi_chimney/tb_floo_axi_chimney_mgr_rsp_trace.sv"

run_log="$build_root/chimney_mgr_rsp_rtl_run.log"
"$verilator_build/Vtb_floo_axi_chimney_mgr_rsp_trace" \
  "+STIM_FILE=$stimulus" \
  "+TRACE_FILE=$rtl_trace" 2>&1 | tee "$run_log"

if grep -qiE "Assertion failed|%Error" "$run_log"; then
  echo "RTL reported an assertion or error during the run." >&2
  grep -iE "Assertion failed|%Error" "$run_log" | head -5 >&2
  exit 1
fi

# The transaction budget, checked directly against what the stimulus documents:
# two outstanding reads on id 1, one on id 2, one write on id 1, and every one
# of them answered exactly once.
#
# This replaces a `counter >= MaxTxnsPerId` test that claimed to detect a wrap
# and could not. `CounterWidth = $clog2(MaxTxnsPerId) = 5`, so `in_flight` is
# five bits and never reaches 32; an underflow from zero lands on 31. The check
# was dead code. Comparing the maxima and the final values catches an underflow,
# an extra push and a missed pop alike, and says what the stimulus means rather
# than guessing at a range.
#
# It matters because both sides replay the same vector: a stimulus that pushes
# more transactions than it intends still compares exactly while proving a
# different scenario than the one written down. An earlier version held
# `AxVALID` for six cycles — six transactions, not one — and ended with 10 and 5
# outstanding against a comment claiming 2 and 1.
expected_peak="2 1 1"
expected_final="0 0 0"
actual_peak="$(
  awk -F, 'NR > 1 {
             if ($(NF-2)+0 > a) a = $(NF-2)+0
             if ($(NF-1)+0 > b) b = $(NF-1)+0
             if ($NF+0     > c) c = $NF+0
           } END { print a+0, b+0, c+0 }' "$rtl_trace"
)"
actual_final="$(tail -n 1 "$rtl_trace" | awk -F, '{print $(NF-2)+0, $(NF-1)+0, $NF+0}')"

if [[ "$actual_peak" != "$expected_peak" ]]; then
  echo "reorder-buffer peak occupancy (r_cnt1 r_cnt2 b_cnt1) is" >&2
  echo "  ${actual_peak}, expected ${expected_peak}." >&2
  echo "The stimulus is not issuing the transaction budget it documents." >&2
  exit 1
fi
if [[ "$actual_final" != "$expected_final" ]]; then
  echo "reorder-buffer counters did not drain: final (r_cnt1 r_cnt2 b_cnt1) =" >&2
  echo "  ${actual_final}, expected ${expected_final}." >&2
  echo "Every pushed transaction must be answered exactly once." >&2
  exit 1
fi

python3 "$script_dir/compare_traces.py" \
  "$model_trace" "$rtl_trace" "chimney-mgr-rsp" "cycles"
