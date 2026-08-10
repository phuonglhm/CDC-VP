#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Fetches RISC-V VP++ at the audited pin and applies the approved F11 backport.
#
# This script is the *only* thing permitted to modify `third_party/riscv-vp-plusplus`.
# CMake verifies the result and refuses to build if it is wrong; it never patches
# anything itself, so a configure step can never silently change what was
# compiled.
#
# What it produces:
#
#   base revision  7a36fe859cae242f513ca6ad16ab8238f1e82977   (upstream tag 2025.09)
#   + backport     b710fa7be2643b42cee92f5bbcb8cead4c0ed282   (upstream master)
#   = effective source for cdc::cpu::riscv_vp_plusplus
#
# Why a backport rather than a newer pin: the fix lives on upstream master,
# which requires SystemC 3.0.1, and CDC-VP is built against 2.3.4 throughout.
# The commit itself touches two lines of `dbbcache.h` and depends on no SystemC
# API, so it is safe to carry alone. See components/TPU_V3/docs/TPU_V3_PHASE2_AUDIT.md
# finding F11.
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
readonly BACKPORT_REVISION="b710fa7be2643b42cee92f5bbcb8cead4c0ed282"
readonly PATCH_NAME="0001-b710fa7b-dbbcache-fixed-random-cycle-counting.patch"
readonly PATCH_SHA256="16758c959a534e71c23254599ac7229c0ff85d3b4e1dc67ee3c82cc96f23b8ad"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
checkout="${repo_root}/third_party/riscv-vp-plusplus"
patch_file="${script_dir}/patches/${PATCH_NAME}"

fail() { echo "FAIL: $*" >&2; exit 1; }

[[ -f "${patch_file}" ]] || fail "patch not found: ${patch_file}"

# The patch is identified by content, not by filename. A patch that changed
# under us would otherwise be applied without anyone noticing, which is exactly
# the "unrecorded patching" this whole mechanism exists to prevent.
actual_sha="$(sha256sum "${patch_file}" | cut -d' ' -f1)"
[[ "${actual_sha}" == "${PATCH_SHA256}" ]] \
    || fail "patch SHA256 is ${actual_sha}, expected ${PATCH_SHA256}"

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
    # former is acceptable, and only when it is exactly our patch.
    if git -C "${checkout}" apply --reverse --check "${patch_file}" 2>/dev/null \
       && [[ "$(git -C "${checkout}" rev-parse HEAD)" == "${BASE_REVISION}" ]]; then
        echo "already at ${BASE_REVISION} with the F11 backport applied; nothing to do"
        exit 0
    fi
    fail "the checkout has local modifications that are not the recorded F11 backport.
Inspect them, then reset:
    git -C ${checkout} checkout -- .
and re-run this script."
fi

echo "checking out base revision ${BASE_REVISION}"
git -C "${checkout}" checkout --quiet "${BASE_REVISION}"

# Applied only onto the exact base the patch was verified against. A different
# base could take the patch with different surrounding code and still "apply
# cleanly", which is how a backport quietly becomes something else.
head="$(git -C "${checkout}" rev-parse HEAD)"
[[ "${head}" == "${BASE_REVISION}" ]] \
    || fail "expected base ${BASE_REVISION}, got ${head}"

echo "applying F11 backport ${BACKPORT_REVISION}"
git -C "${checkout}" apply --check "${patch_file}" \
    || fail "the F11 backport does not apply cleanly to ${BASE_REVISION}"
git -C "${checkout}" apply "${patch_file}"

# Prove the result, rather than trusting that apply did what was intended.
git -C "${checkout}" apply --reverse --check "${patch_file}" \
    || fail "the backport does not appear applied after git apply"

echo
echo "RISC-V VP++ ready:"
echo "  base revision : ${BASE_REVISION} (upstream tag 2025.09)"
echo "  backport      : ${BACKPORT_REVISION} (F11, dbbcache cycle counter)"
echo "  patch sha256  : ${PATCH_SHA256}"
echo "  location      : ${checkout}"
