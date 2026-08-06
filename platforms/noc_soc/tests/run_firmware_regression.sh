#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Platform-level regression for `noc_soc`: build the SoC-map DMA firmware, run
# it over the cycle-stepped FlooNoC interconnect, and require it to complete.
#
# Why this exists. The component tests in `components/floo_noc_model` cannot
# cover a platform, and they demonstrably did not: the synthetic survey once
# wrote inside the firmware's `.text` and overwrote live instructions, and every
# component test still passed. A stale platform binary produces the same class
# of symptom. Both are caught here in seconds.
#
# Four runs, and they are **not** four firmware images:
#
#   1. Firmware over `--noc-timing detailed`, the calibration reference.
#   2. The same firmware over `--noc-timing fast`, the Step 11 LT path. Both
#      must reach `DMA PASS`, while the synthetic port reports zero traffic.
#   3. The platform's built-in survey over detailed timing.
#   4. The same survey over fast timing, proving its blocking probe consumes
#      the returned annotation. Both must reach `result all bytes match` and
#      report RAM, peripheral and DMA measurements.
#
# Runs 3/4 are a **substitute smoke path with reduced and different coverage**, not
# another firmware image. It exercises the interconnect and the memory path
# from the platform's own traffic generator, including its own DMA0 experiment,
# but has no CPU firmware workload or CPU interrupt-handling path. It is here
# because it is nearly free and catches gross interconnect breakage quickly.
#
# Reading the pair: a failure in 1 but not 2 is *evidence towards* a fault in
# the CPU, DMA or firmware-integration path rather than the interconnect, and a
# failure in both points at the interconnect or memory. That is a hint for
# where to look first, **not** a decision procedure — the two runs share almost
# all of the interconnect, so run 2 passing does not clear it. Anyone who wants
# a real separation needs a dedicated CPU alignment/readback image, which does
# not exist yet.
#
# Exit codes: 0 pass, 1 fail, 77 skipped (CTest's SKIP_RETURN_CODE) when the
# RISC-V toolchain is absent. Skipping is deliberate — a machine without the
# cross-compiler should not report a red test it cannot possibly run.

set -u -o pipefail

readonly SKIP=77

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
platform_dir="$(cd "${script_dir}/.." && pwd)"
repo_root="$(cd "${platform_dir}/../.." && pwd)"

noc_soc_bin="${NOC_SOC_BIN:-}"
fw_dir="${repo_root}/fw/dma_riscv"
config="${platform_dir}/configs/default.yaml"
# A unique directory per run unless one is named explicitly. A fixed default
# means two concurrent runs — a developer and a CI job, or two CTest jobs —
# overwrite each other's evidence, and the surviving log belongs to neither.
if [[ -n "${LOG_DIR:-}" ]]; then
    log_dir="${LOG_DIR}"
    mkdir -p "${log_dir}"
else
    log_dir="$(mktemp -d "${TMPDIR:-/tmp}/noc_soc_firmware_regression.XXXXXX")"
fi

# The firmware is built in a private copy of its sources, never in the
# repository. The Makefile builds in-source, so building here directly would
# race any concurrent run over the same `dma_test.elf`, and would leave the
# working tree dirty after a test.
fw_build_dir="${log_dir}/fw"

# `noc_soc` follows docs/peripheral_memory_map.md; `fw/dma_riscv` defaults to the
# VP_FX1_Full_SoC address, so the base must be overridden.
readonly dma_base=0x10060000u
# Derived, never written twice: two constants that must agree are two constants
# that will eventually disagree, and the resulting failure would read as "the
# override did not take effect" when the truth is that this file contradicts
# itself.
readonly dma_base_value="${dma_base%u}"

# Every run is bounded. A hang must fail with a message, never wedge the suite.
readonly run_timeout=300

fail() { echo "FAIL: $*" >&2; exit 1; }

# ── locate the binary ────────────────────────────────────────────────────────
if [[ -z "${noc_soc_bin}" ]]; then
  for candidate in \
      "${repo_root}/build/platforms/noc_soc/noc_soc" \
      "${platform_dir}/noc_soc"; do
    [[ -x "${candidate}" ]] && { noc_soc_bin="${candidate}"; break; }
  done
fi
[[ -x "${noc_soc_bin:-}" ]] || fail "noc_soc binary not found; set NOC_SOC_BIN"

# ── locate the toolchain ─────────────────────────────────────────────────────
if ! command -v riscv-none-elf-gcc >/dev/null 2>&1; then
  if [[ -x /opt/toolchains/riscv-none-elf/bin/riscv-none-elf-gcc ]]; then
    export PATH="/opt/toolchains/riscv-none-elf/bin:${PATH}"
  else
    echo "SKIP: riscv-none-elf-gcc not found; cannot build the firmware." >&2
    exit "${SKIP}"
  fi
fi

mkdir -p "${log_dir}"

# ── build the firmware, in a private copy of its sources ────────────────────
#
# Never in the repository. The firmware Makefile builds in-source, so building
# there would race any concurrent run over the same `dma_test.elf` and would
# leave the working tree dirty after a test. Copying also removes the need for
# an in-source `make clean`, which was the previous way of guaranteeing the
# EXTRA_CFLAGS override actually retriggered the link.
build_log="${log_dir}/firmware_build.log"
mkdir -p "${fw_build_dir}"
cp -r "${fw_dir}/." "${fw_build_dir}/" 2>/dev/null \
  || fail "could not copy the firmware sources to ${fw_build_dir}"
# An ELF carried over from an earlier in-source build is linked against
# whatever base that build used, and is not what this run is testing.
rm -f "${fw_build_dir}/dma_test.elf" "${fw_build_dir}/dma_test.dis"

if ! make -C "${fw_build_dir}" EXTRA_CFLAGS="-DDMA_BASE=${dma_base}" \
      >"${build_log}" 2>&1; then
  echo "--- firmware build log ---" >&2
  cat "${build_log}" >&2
  fail "firmware build failed (log kept at ${build_log})"
fi

elf="${fw_build_dir}/dma_test.elf"
[[ -f "${elf}" ]] || fail "firmware build produced no ${elf}"

# Prove the override reached the binary, deterministically.
#
# The firmware exports `__fw_dma_base`, an absolute symbol whose address *is*
# the configured base, so `nm` answers the question exactly. An earlier version
# grepped the disassembly for the substring `10060`, which matches any
# incidental occurrence — a stack offset, an unrelated constant, a byte pair
# inside a larger literal — and proves nothing about which base the code uses.
#
# The proof is **mandatory**. It used to be wrapped in
# `if command -v riscv-none-elf-nm`, so a machine with the compiler but without
# `nm` ran the whole platform and reported a pass while the one deterministic
# piece of evidence had been skipped. Either the proof happens or the run does
# not claim to have made it.
if ! command -v riscv-none-elf-nm >/dev/null 2>&1; then
  echo "SKIP: riscv-none-elf-nm not found. The DMA-base proof cannot be made," >&2
  echo "      and this test does not run the platform without it: a pass that" >&2
  echo "      skipped its own evidence is worse than no result." >&2
  exit "${SKIP}"
fi
if ! command -v riscv-none-elf-objcopy >/dev/null 2>&1 \
   || ! command -v riscv-none-elf-readelf >/dev/null 2>&1; then
  echo "SKIP: riscv-none-elf-objcopy/readelf not found. Step 10.1 must prove" >&2
  echo "      that an overlapping PT_LOAD is rejected with its exact range." >&2
  exit "${SKIP}"
fi

actual_base="$(riscv-none-elf-nm "${elf}" \
  | awk '$3 == "__fw_dma_base" { print $1 }')"
if [[ -z "${actual_base}" ]]; then
  fail "the ELF exports no __fw_dma_base symbol; the firmware sources are" \
       "older than this test expects"
fi
expected_base="$(printf '%08x' "$((dma_base_value))")"
if [[ "${actual_base}" != "${expected_base}" ]]; then
  fail "the ELF was built for DMA base 0x${actual_base}, expected" \
       "0x${expected_base}: the EXTRA_CFLAGS override did not take effect"
fi
echo "  DMA base proved from the ELF: 0x${actual_base}"

# ── run one image and check it ───────────────────────────────────────────────
# $1 label, $2 log file, $3 the string that must be present, rest: extra args
run_image() {
  local label="$1" log="$2" required="$3"
  shift 3

  timeout --kill-after=10 "${run_timeout}" \
    "${noc_soc_bin}" -c "${config}" "$@" >"${log}" 2>&1
  local status=$?

  if [[ ${status} -eq 124 || ${status} -eq 137 ]]; then
    echo "--- ${label} log (tail) ---" >&2; tail -40 "${log}" >&2
    fail "${label}: timed out after ${run_timeout}s (log kept at ${log})"
  fi
  if [[ ${status} -ne 0 ]]; then
    echo "--- ${label} log (tail) ---" >&2; tail -40 "${log}" >&2
    fail "${label}: exited ${status} (log kept at ${log})"
  fi

  if ! grep -q "${required}" "${log}"; then
    echo "--- ${label} log (tail) ---" >&2; tail -40 "${log}" >&2
    fail "${label}: '${required}' not found (log kept at ${log})"
  fi

  # Symptoms that do not change the exit code but mean the run is worthless.
  # `\([EW][0-9]{3}\)` is SystemC's parenthesised report id, as in `Error: (E549)`.
  # Matching the bare digits instead would false-positive on any hex address
  # that happens to contain them.
  local pattern
  for pattern in 'Taking trap' '\[PC\] trapped' 'Error:' '\([EW][0-9]{3}\)' \
                 'mismatch' 'FAIL'; do
    if grep -Eq "${pattern}" "${log}"; then
      echo "--- ${label} log (tail) ---" >&2; tail -40 "${log}" >&2
      fail "${label}: log contains '${pattern}' (log kept at ${log})"
    fi
  done

  # A CPU parked at zero retired nothing useful, whatever else the log says.
  if grep -Eq 'CPU pc 0x0\b' "${log}"; then
    echo "--- ${label} log (tail) ---" >&2; tail -40 "${log}" >&2
    fail "${label}: CPU ended at PC 0 (log kept at ${log})"
  fi

  echo "  ${label}: PASS"
}

require_log() {
  local log="$1" text="$2" reason="$3"
  if ! grep -Fq -- "${text}" "${log}"; then
    echo "--- log tail ---" >&2; tail -40 "${log}" >&2
    fail "${reason}: '${text}' not found (log kept at ${log})"
  fi
}

reject_log() {
  local log="$1" text="$2" reason="$3"
  if grep -Fq -- "${text}" "${log}"; then
    echo "--- log tail ---" >&2; tail -40 "${log}" >&2
    fail "${reason}: forbidden '${text}' found (log kept at ${log})"
  fi
}

expect_rejected() {
  local label="$1" log="$2" required="$3"
  shift 3

  if "${noc_soc_bin}" "$@" >"${log}" 2>&1; then
    echo "--- ${label} log ---" >&2; cat "${log}" >&2
    fail "${label}: command unexpectedly succeeded"
  fi
  require_log "${log}" "${required}" "${label}"
  echo "  ${label}: PASS"
}

echo "noc_soc firmware regression"
echo "  binary:   ${noc_soc_bin}"
echo "  firmware: ${elf}"

# Host-input controls must be discoverable from the executable rather than
# existing only in a README. Help is intentionally valid without `--mode`.
help_log="${log_dir}/help.log"
"${noc_soc_bin}" --help >"${help_log}" 2>&1 \
    || fail "noc_soc --help failed"
for option in \
    "--uart0-socket PORT" \
    "--uart0-wait" \
    "--uart0-rx-file FILE" \
    "--uart0-rx-delay-us N"; do
    require_log "${help_log}" "${option}" "UART0 host option discoverability"
done
echo "  UART host options in --help: PASS"

# Mode selection is part of the platform contract, not an inference from
# whether `--fw` happened to be present.
expect_rejected "missing explicit mode" "${log_dir}/missing_mode.log" \
    "noc_soc: --mode is required: choose 'survey' or 'firmware'" \
    --sim-us 1
expect_rejected "firmware mode without ELF" \
    "${log_dir}/firmware_without_elf.log" \
    "noc_soc: firmware mode requires --fw <image.elf>" \
    --mode firmware --sim-us 1
expect_rejected "survey mode with ELF" \
    "${log_dir}/survey_with_elf.log" \
    "noc_soc: survey mode rejects --fw" \
    --mode survey --fw "${elf}" --sim-us 1
expect_rejected "invalid mode value" "${log_dir}/invalid_mode.log" \
    "noc_soc: --mode must be 'survey' or 'firmware'" \
    --mode mixed --sim-us 1
expect_rejected "invalid NoC timing value" \
    "${log_dir}/invalid_noc_timing.log" \
    "noc_soc: --noc-timing must be 'detailed' or 'fast'" \
    --mode survey --noc-timing mixed --sim-us 1
expect_rejected "UART wait without socket" \
    "${log_dir}/uart_wait_without_socket.log" \
    "noc_soc: --uart0-wait requires --uart0-socket" \
    --mode survey --uart0-wait --sim-us 1
expect_rejected "UART replay delay without file" \
    "${log_dir}/uart_delay_without_file.log" \
    "noc_soc: --uart0-rx-delay-us requires --uart0-rx-file" \
    --mode survey --uart0-rx-delay-us 1 --sim-us 1
expect_rejected "UART host input in survey mode" \
    "${log_dir}/uart_input_in_survey.log" \
    "noc_soc: UART0 host input requires --mode firmware" \
    --mode survey --uart0-rx-file unused --sim-us 1
expect_rejected "UART socket above TCP port range" \
    "${log_dir}/uart_socket_out_of_range.log" \
    "noc_soc: --uart0-socket is out of range" \
    --mode survey --uart0-socket 65536 --sim-us 1
expect_rejected "non-decimal UART socket" \
    "${log_dir}/uart_socket_not_decimal.log" \
    "noc_soc: --uart0-socket must be an unsigned decimal number" \
    --mode survey --uart0-socket 12x --sim-us 1

# Move the real firmware's PT_LOAD into the final RAM page. The image need not
# execute: the platform must reject it before `sc_start()` and print both the
# exact ELF range and the reserved range.
overlap_elf="${log_dir}/firmware_overlaps_survey_scratch.elf"
cp "${elf}" "${overlap_elf}" \
  || fail "could not create overlap-test ELF"
read -r original_first original_size < <(
  riscv-none-elf-readelf -lW "${elf}" \
    | awk '$1 == "LOAD" { print $4, $6; exit }')
[[ -n "${original_first:-}" && -n "${original_size:-}" ]] \
  || fail "could not read the firmware ELF PT_LOAD range"
readonly scratch_first=0x80fff000
relocation_delta="$((scratch_first - original_first))"
riscv-none-elf-objcopy --change-addresses "${relocation_delta}" "${overlap_elf}" \
  || fail "could not relocate overlap-test ELF"
read -r overlap_first overlap_size < <(
  riscv-none-elf-readelf -lW "${overlap_elf}" \
    | awk '$1 == "LOAD" { print $4, $6; exit }')
[[ -n "${overlap_first:-}" && -n "${overlap_size:-}" ]] \
  || fail "could not read the overlap-test ELF PT_LOAD range"
overlap_last="$(printf '0x%x' "$((overlap_first + overlap_size))")"
overlap_message="noc_soc: firmware PT_LOAD [${overlap_first}, ${overlap_last}) overlaps reserved survey scratch [0x80fff000, 0x81000000)"
expect_rejected "reserved scratch overlap" "${log_dir}/overlap.log" \
    "${overlap_message}" \
    --mode firmware --fw "${overlap_elf}" --sim-us 1
reject_log "${log_dir}/overlap.log" "noc_soc config:" \
    "reserved scratch overlap must fail before internal SoC construction"

# 2000 µs, not 500. The workload needs about 563 µs of modelled time since the
# wrapper began spending the caller's annotated delay instead of discarding it
# (see the delay contract in `noc_interconnect.h`); before that it fitted in
# ~331 µs. The margin is deliberate — this bound exists to stop a hang, not to
# assert a performance figure, and a bound tight enough to fail on a timing
# change is a bound that will keep failing for the wrong reason.
run_image "firmware mode, detailed NoC (DMA transfer)" \
          "${log_dir}/firmware_detailed.log" \
          'DMA PASS' --mode firmware --noc-timing detailed \
          --fw "${elf}" --sim-us 2000

require_log "${log_dir}/firmware_detailed.log" "noc_soc mode: firmware" \
    "firmware mode selection"
require_log "${log_dir}/firmware_detailed.log" \
    "interconnect timing: detailed cycle-stepped FlooNoC" \
    "detailed NoC timing selection"
require_log "${log_dir}/firmware_detailed.log" \
    "noc_soc: firmware mode; synthetic survey disabled" \
    "firmware ownership"
require_log "${log_dir}/firmware_detailed.log" "  transactions 0" \
    "firmware synthetic-transaction ownership"
require_log "${log_dir}/firmware_detailed.log" "  RAM writes 0" \
    "firmware RAM ownership"
require_log "${log_dir}/firmware_detailed.log" "  DMA register writes 0" \
    "firmware DMA ownership"
reject_log "${log_dir}/firmware_detailed.log" "noc_soc: RAM at" \
    "firmware mode must not run the RAM survey"
reject_log "${log_dir}/firmware_detailed.log" \
    "noc_soc: DMA memory-to-memory transfer, 512 bytes" \
    "firmware mode must not program DMA0 from the probe port"

run_image "firmware mode, fast NoC (DMA transfer)" \
          "${log_dir}/firmware_fast.log" \
          'DMA PASS' --mode firmware --noc-timing fast \
          --fw "${elf}" --sim-us 2000
require_log "${log_dir}/firmware_fast.log" \
    "interconnect timing: fast approximately-timed FlooNoC" \
    "fast NoC timing selection"
require_log "${log_dir}/firmware_fast.log" \
    "noc_soc: firmware mode; synthetic survey disabled" \
    "fast firmware ownership"
require_log "${log_dir}/firmware_fast.log" "  transactions 0" \
    "fast firmware synthetic-transaction ownership"
require_log "${log_dir}/firmware_fast.log" "  RAM writes 0" \
    "fast firmware RAM ownership"
require_log "${log_dir}/firmware_fast.log" "  DMA register writes 0" \
    "fast firmware DMA ownership"
require_log "${log_dir}/firmware_fast.log" \
    "cycle-stepped mesh bypassed in fast mode" \
    "fast backend isolation"
require_log "${log_dir}/firmware_fast.log" \
    "through the fast NoC estimate" \
    "fast firmware modeled-time report"
reject_log "${log_dir}/firmware_fast.log" "noc_soc: RAM at" \
    "fast firmware mode must not run the RAM survey"

run_image "synthetic survey mode" "${log_dir}/survey.log" \
          'result   all bytes match' --mode survey --noc-timing detailed \
          --sim-us 200

require_log "${log_dir}/survey.log" "noc_soc mode: survey" \
    "survey mode selection"
require_log "${log_dir}/survey.log" "noc_soc: RAM at" \
    "survey RAM measurements"
require_log "${log_dir}/survey.log" \
    "noc_soc: one 32-bit register read per peripheral" \
    "survey peripheral measurements"
require_log "${log_dir}/survey.log" \
    "noc_soc: DMA memory-to-memory transfer, 512 bytes" \
    "survey DMA measurements"
if ! grep -Eq '^  RAM writes [1-9][0-9]*$' "${log_dir}/survey.log"; then
  fail "survey mode issued no synthetic RAM write"
fi
if ! grep -Eq '^  DMA register writes [1-9][0-9]*$' \
      "${log_dir}/survey.log"; then
  fail "survey mode did not program DMA0"
fi

run_image "synthetic survey mode, fast NoC" \
          "${log_dir}/survey_fast.log" \
          'result   all bytes match' --mode survey --noc-timing fast \
          --sim-us 200
require_log "${log_dir}/survey_fast.log" \
    "interconnect timing: fast approximately-timed FlooNoC" \
    "fast survey timing selection"
require_log "${log_dir}/survey_fast.log" "noc_soc: RAM at" \
    "fast survey RAM measurements"
require_log "${log_dir}/survey_fast.log" \
    "noc_soc: one 32-bit register read per peripheral" \
    "fast survey peripheral measurements"
require_log "${log_dir}/survey_fast.log" \
    "noc_soc: DMA memory-to-memory transfer, 512 bytes" \
    "fast survey DMA measurements"
require_log "${log_dir}/survey_fast.log" \
    "cycle-stepped mesh bypassed in fast mode" \
    "fast survey backend isolation"

echo "noc_soc firmware regression PASS"
