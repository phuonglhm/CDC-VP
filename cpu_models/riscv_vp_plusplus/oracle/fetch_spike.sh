#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Fetches and builds Spike (`riscv-isa-sim`) at the audited pin, as a
# **standalone** differential oracle.
#
#   pin  16c0b60119f65a648643cf5d41e4e38e871f0bad   (decision record D3, P0-1)
#
# Spike is never linked into the TPU_V3 platform or into a portable package
# (D3). It is built here as a separate executable that the differential test
# invokes as a child process. Nothing in `platforms/` or `cdc_package_platform`
# refers to it, and the package has no dependency on this directory existing.
#
# Why a child process rather than a library: an oracle that shares a process
# with the model under test shares its allocator, its global state and its
# build flags. Berkeley SoftFloat alone would be linked twice under one set of
# process-global rounding-mode variables — the exact hazard finding F5 exists
# to guard. A separate address space makes the independence structural rather
# than a claim.
#
# Unlike `fetch_riscv_vp_plusplus.sh` there is no patch step: Spike is used
# exactly as upstream published it. If it needed patching to agree with us, it
# would no longer be an independent reference.

set -euo pipefail

readonly UPSTREAM="https://github.com/riscv-software-src/riscv-isa-sim.git"
readonly PIN="16c0b60119f65a648643cf5d41e4e38e871f0bad"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../.." && pwd)"
checkout="${repo_root}/third_party/riscv-isa-sim"
build_dir="${checkout}/build"
install_dir="${checkout}/install"

fail() { echo "FAIL: $*" >&2; exit 1; }

# The default PATH on this machine puts a Synopsys g++ wrapper ahead of the
# system compiler; it cannot build Spike. Pin the toolchain rather than
# inheriting whatever is first.
export CC="${CC:-/usr/bin/gcc}"
export CXX="${CXX:-/usr/bin/g++}"

if [[ ! -d "${checkout}/.git" ]]; then
    echo "cloning ${UPSTREAM}"
    git clone --quiet "${UPSTREAM}" "${checkout}"
fi

if ! git -C "${checkout}" cat-file -e "${PIN}^{commit}" 2>/dev/null; then
    git -C "${checkout}" fetch --quiet origin
fi

if [[ -n "$(git -C "${checkout}" status --porcelain --untracked-files=no)" ]]; then
    fail "the Spike checkout has local modifications.
An oracle that has been edited is not an independent reference. Inspect them, then:
    git -C ${checkout} checkout -- .
and re-run this script."
fi

git -C "${checkout}" checkout --quiet "${PIN}"
head="$(git -C "${checkout}" rev-parse HEAD)"
[[ "${head}" == "${PIN}" ]] || fail "expected pin ${PIN}, got ${head}"

mkdir -p "${build_dir}"
if [[ ! -f "${build_dir}/config.status" ]]; then
    echo "configuring Spike"
    # --enable-commitlog: costs nothing until `--log-commits` is passed, and it
    # is the only practical way to localise a differential mismatch to an
    # instruction. Diagnosing one without it means rebuilding the oracle, which
    # is the moment you least want to change it.
    (cd "${build_dir}" && "${checkout}/configure" \
        --prefix="${install_dir}" \
        --enable-commitlog >/dev/null) \
        || fail "Spike configure failed"
fi

echo "building Spike (this takes a few minutes)"
make -C "${build_dir}" -j"$(nproc)" >/dev/null || fail "Spike build failed"
make -C "${build_dir}" install >/dev/null || fail "Spike install failed"

spike_bin="${install_dir}/bin/spike"
[[ -x "${spike_bin}" ]] || fail "no spike binary at ${spike_bin}"

echo
echo "Spike oracle ready:"
echo "  pin      : ${PIN}"
echo "  binary   : ${spike_bin}"
"${spike_bin}" --help 2>&1 | head -1 || true
