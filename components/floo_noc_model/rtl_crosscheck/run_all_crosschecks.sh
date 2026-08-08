#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Run all twelve signed SystemC-to-RTL comparisons in one auditable command.
# Each leaf runner still owns its frozen-source/hash checks and evidence files;
# this wrapper only prevents a sign-off from accidentally running a subset.

set -u -o pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

runners=(
    run_route_select_crosscheck.sh
    run_stream_fifo_crosscheck.sh
    run_wormhole_arbiter_crosscheck.sh
    run_router_crosscheck.sh
    run_rob_crosscheck.sh
    run_chimney_req_crosscheck.sh
    run_chimney_timing_crosscheck.sh
    run_chimney_rsp_crosscheck.sh
    run_chimney_rsp_timing_crosscheck.sh
    run_chimney_mgr_rsp_crosscheck.sh
    run_mesh_crosscheck.sh
    run_axi_sizing_crosscheck.sh
)

passed=0
failed=()

for runner in "${runners[@]}"; do
    printf '\n==> [%02d/%02d] %s\n' \
        "$((passed + ${#failed[@]} + 1))" "${#runners[@]}" "$runner"
    if "${script_dir}/${runner}"; then
        passed=$((passed + 1))
    else
        failed+=("$runner")
    fi
done

printf '\nRTL cross-check summary: %d/%d passed\n' \
    "$passed" "${#runners[@]}"

if ((${#failed[@]} != 0)); then
    printf 'Failed runners:\n' >&2
    printf '  %s\n' "${failed[@]}" >&2
    exit 1
fi
