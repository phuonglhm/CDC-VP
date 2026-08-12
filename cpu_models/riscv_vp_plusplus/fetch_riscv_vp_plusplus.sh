#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Fetches RISC-V VP++ at the audited pin and applies the approved patch series.
#
# This script is the *only* thing permitted to modify `third_party/riscv-vp-plusplus`.
# CMake verifies the result and refuses to build if it is wrong; it never patches
# anything itself, so a configure step can never silently change what was
# compiled.
#
# What it produces:
#
#   base revision  7a36fe859cae242f513ca6ad16ab8238f1e82977   (upstream tag 2025.09)
#   + 0001         upstream backport b710fa7b                 (F11, D8)
#   + 0002         downstream conformance patch               (F12, D12)
#   + 0003         downstream conformance patch               (F13, D13)
#   = effective source for cdc::cpu::riscv_vp_plusplus
#
# Two kinds of patch, and the difference matters more than it looks:
#
#   *upstream backport* — the commit exists upstream. It is carried back only
#     because upstream master requires SystemC 3.0.1 and CDC-VP is built against
#     2.3.4 throughout. It disappears the day the pin moves past it.
#
#   *downstream conformance patch* — there is no upstream commit. These were
#     written here, against the specification, for defects the Phase 2
#     differential corpus found. They do **not** disappear when the pin moves:
#     each has to be re-checked against the new base, and each should be offered
#     upstream. Calling them "backports" would hide exactly that.
#
# See components/TPU_V3/docs/TPU_V3_PHASE2_AUDIT.md findings F11, F12 and F13,
# and decision records D8, D12 and D13.
#
# Deliberately *not* bundled with this backport, each for its own reason:
#
#   7a936cce  "fixed fast quantum"      — a larger change to quantum accounting;
#                                         needs its own audit before Phase 7.
#   52d376d4  AMO atomicity / bus lock  — needs a multi-hart AMO contention test
#                                         before Phase 6. A single-threaded
#                                         differential run against Spike cannot
#                                         demonstrate atomicity between harts.
#
# If upstream publishes an official backport on a SystemC 2.3-compatible branch,
# move the pin to that commit, delete the local patch, and re-run the whole
# Phase 2 gate.

set -euo pipefail

readonly UPSTREAM="https://github.com/ics-jku/riscv-vp-plusplus.git"
readonly BASE_REVISION="7a36fe859cae242f513ca6ad16ab8238f1e82977"

# Applied in order. Must stay in step with RISCV_VP_PLUSPLUS_PATCH_SERIES in
# CMakeLists.txt, which verifies the same hashes and refuses to build if the
# result is not exactly this.
readonly PATCH_SERIES=(
    "0001-b710fa7b-dbbcache-fixed-random-cycle-counting.patch 16758c959a534e71c23254599ac7229c0ff85d3b4e1dc67ee3c82cc96f23b8ad upstream-backport"
    "0002-d12-rv32-index-eew64-illegal.patch                   de147236d885b6a76584ad1ac83da6f4d6cab975a0464b40acdb9c619090a319 downstream-conformance"
    "0003-d13-bus-error-is-an-access-fault.patch               c3535b21b849581af340b755ad1d22c33647c88dc3b9659072b75fd7fefce84b downstream-conformance"
)

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
checkout="${repo_root}/third_party/riscv-vp-plusplus"
patches_dir="${script_dir}/patches"

fail() { echo "FAIL: $*" >&2; exit 1; }

# Each patch is identified by content, not by filename. A patch that changed
# under us would otherwise be applied without anyone noticing, which is exactly
# the "unrecorded patching" this whole mechanism exists to prevent.
verify_hashes() {
    local record file want got
    for record in "${PATCH_SERIES[@]}"; do
        read -r file want _ <<<"${record}"
        [[ -f "${patches_dir}/${file}" ]] || fail "patch not found: ${patches_dir}/${file}"
        got="$(sha256sum "${patches_dir}/${file}" | cut -d' ' -f1)"
        [[ "${got}" == "${want}" ]] \
            || fail "patch ${file} has SHA256 ${got}, expected ${want}"
    done
}

# The series is applied, so "already done" means *every* patch reverse-applies.
# Checking only the last one would accept a tree missing an earlier patch that a
# later one happened not to conflict with.
series_fully_applied() {
    local record file
    for record in "${PATCH_SERIES[@]}"; do
        read -r file _ _ <<<"${record}"
        git -C "${checkout}" apply --reverse --check "${patches_dir}/${file}" 2>/dev/null \
            || return 1
    done
    return 0
}

verify_hashes

if [[ ! -d "${checkout}/.git" ]]; then
    echo "cloning ${UPSTREAM}"
    git clone --quiet "${UPSTREAM}" "${checkout}"
fi

# Fetch only if the pin is not already present, so a working checkout does not
# need network access.
if ! git -C "${checkout}" cat-file -e "${BASE_REVISION}^{commit}" 2>/dev/null; then
    git -C "${checkout}" fetch --quiet origin
fi

if [[ -n "$(git -C "${checkout}" status --porcelain)" ]]; then
    # Distinguish "already patched" from "someone edited the tree". Only the
    # former is acceptable, and only when it is exactly this series.
    if series_fully_applied \
       && [[ "$(git -C "${checkout}" rev-parse HEAD)" == "${BASE_REVISION}" ]]; then
        echo "already at ${BASE_REVISION} with the full patch series applied; nothing to do"
        exit 0
    fi
    fail "the checkout has local modifications that are not the recorded patch series.
Inspect them, then reset:
    git -C ${checkout} checkout -- .
and re-run this script."
fi

echo "checking out base revision ${BASE_REVISION}"
git -C "${checkout}" checkout --quiet "${BASE_REVISION}"

# Applied only onto the exact base each patch was verified against. A different
# base could take a patch with different surrounding code and still "apply
# cleanly", which is how a fix quietly becomes something else.
head="$(git -C "${checkout}" rev-parse HEAD)"
[[ "${head}" == "${BASE_REVISION}" ]] \
    || fail "expected base ${BASE_REVISION}, got ${head}"

for record in "${PATCH_SERIES[@]}"; do
    read -r file _ kind <<<"${record}"
    echo "applying ${kind}: ${file}"
    git -C "${checkout}" apply --check "${patches_dir}/${file}" \
        || fail "${file} does not apply cleanly onto ${BASE_REVISION}"
    git -C "${checkout}" apply "${patches_dir}/${file}"
done

# Prove the result, rather than trusting that apply did what was intended.
series_fully_applied || fail "the patch series does not appear applied after git apply"

echo
echo "RISC-V VP++ ready:"
echo "  base revision : ${BASE_REVISION} (upstream tag 2025.09)"
for record in "${PATCH_SERIES[@]}"; do
    read -r file sha kind <<<"${record}"
    printf '  %-22s %s\n' "${kind}" "${file}"
    printf '  %-22s %s\n' "" "${sha}"
done
echo "  location      : ${checkout}"
