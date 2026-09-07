#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "usage: $0 <build-dir> <firmware.elf>" >&2
    exit 2
fi

build_dir=$1
firmware=$2

# A hung model must fail the run instead of blocking CI forever.
vp_timeout=${VP_TIMEOUT:-300}

ctest --test-dir "${build_dir}" -R '^tutorial_edu_timer_unit$' \
      --output-on-failure --timeout "${vp_timeout}"

output=$(timeout "${vp_timeout}" \
         "${build_dir}/vp_timer_tutorial/mini_soc_edu_timer" \
         --fw "${firmware}" --sim-us 1000)
printf '%s\n' "${output}"

grep -q '^FW: ID PASS$' <<<"${output}"
grep -q '^FW: RW PASS$' <<<"${output}"
grep -q '^FW: IRQ PASS$' <<<"${output}"
grep -q '^FW: ALL PASS$' <<<"${output}"
grep -q '^SIM: completed 1000 us$' <<<"${output}"

echo "REGRESSION: ALL PASS"

