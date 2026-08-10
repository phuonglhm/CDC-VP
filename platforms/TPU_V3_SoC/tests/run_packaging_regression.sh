#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# TPU_V3 Phase 1 distribution gate.
#
# The plan's Phase 1 gate is "the package runs from out/tpu_v3_soc/ and nothing
# points back at the source tree". Checking that against the developer's own
# build proves less than it appears to: that build's artifacts sit next to the
# source, and the developer's shell has LD_LIBRARY_PATH pointing at SystemC.
#
# So this configures a *fresh* Release build with a private package root, then
# verifies:
#
#   * the package contains the binary, the SystemC runtime, configs, licences,
#     a firmware directory and a build manifest;
#   * the packaged executable has exactly RPATH=$ORIGIN and no source-tree path;
#   * libsystemc resolves from beside the binary with LD_LIBRARY_PATH unset;
#   * the binary runs every shipped configuration from a directory that has no
#     access to the source or build tree;
#   * the build manifest is well-formed JSON and records what was actually
#     linked, including that Spike is not.
#
# Exit codes: 0 pass, 1 fail, 77 skip. Set PACKAGING_KEEP_ARTIFACTS=1 to retain
# a successful temporary tree, or PACKAGING_SYSTEMC_HOME to select a non-default
# Accellera installation.

set -euo pipefail

readonly SKIP=77

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
platform_dir="$(cd "${script_dir}/.." && pwd)"
repo_root="$(cd "${platform_dir}/../.." && pwd)"

# Reproducibility beats the ambient login shell: the default PATH here puts a
# Synopsys g++ wrapper ahead of the system compiler.
export CC="${PACKAGING_CC:-/usr/bin/gcc}"
export CXX="${PACKAGING_CXX:-/usr/bin/g++}"
export PATH="/usr/bin:/bin:${PATH}"

systemc_home="${PACKAGING_SYSTEMC_HOME:-/opt/systemc-2.3.4}"

work_dir="$(mktemp -d "${TMPDIR:-/tmp}/tpu_v3_packaging.XXXXXX")"
build_dir="${work_dir}/build"
package_root="${work_dir}/package"
package_dir="${package_root}/tpu_v3_soc"
run_dir="${work_dir}/run"
evidence_dir="${work_dir}/evidence"
mkdir -p "${evidence_dir}" "${run_dir}"

cleanup()
{
    local status=$?
    trap - EXIT
    if [[ "${PACKAGING_KEEP_ARTIFACTS:-0}" != "1"
          && (${status} -eq 0 || ${status} -eq ${SKIP}) ]]; then
        rm -rf "${work_dir}"
    else
        echo "packaging evidence kept under ${work_dir}" >&2
    fi
    exit "${status}"
}
trap cleanup EXIT

fail()
{
    echo "FAIL: $*" >&2
    exit 1
}

require_command()
{
    command -v "$1" >/dev/null 2>&1 || fail "required command not found: $1"
}

match_file()
{
    local expected="$1" actual="$2"
    [[ -f "${actual}" ]] || fail "required package record missing: ${actual}"
    cmp -s "${expected}" "${actual}" \
        || fail "package record differs from source: ${actual}"
}

require_command cmake
require_command readelf
require_command ldd
require_command cmp
require_command python3
require_command timeout

[[ -d "${systemc_home}" ]] || {
    echo "SKIP: SystemC not found at ${systemc_home}" >&2
    exit "${SKIP}"
}

echo "packaging work directory: ${work_dir}"

cmake -S "${repo_root}" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DSYSTEMC_HOME="${systemc_home}" \
    -DCDC_BUILD_TPU_V3_SOC=ON \
    -DCDC_BUILD_TPU_V3_TESTS=OFF \
    -DCDC_BUILD_TESTS=OFF \
    -DCDC_BUILD_MINI_TLM=OFF \
    -DCDC_BUILD_CPU_EVAL=OFF \
    -DCDC_BUILD_CUSTOM_SOC=OFF \
    -DCDC_BUILD_NOC_SOC=OFF \
    -DCDC_PACKAGE_ROOT="${package_root}" \
    >"${evidence_dir}/configure.log" 2>&1 \
    || { cat "${evidence_dir}/configure.log" >&2; fail "configure failed"; }

cmake --build "${build_dir}" --target tpu_v3_soc --parallel \
    >"${evidence_dir}/build.log" 2>&1 \
    || { cat "${evidence_dir}/build.log" >&2; fail "build failed"; }

cmake --build "${build_dir}" --target tpu_v3_soc_package --parallel \
    >"${evidence_dir}/package.log" 2>&1 \
    || { cat "${evidence_dir}/package.log" >&2; fail "packaging failed"; }

# ── contents ─────────────────────────────────────────────────────────────────

package_bin="${package_dir}/tpu_v3_soc"
[[ -x "${package_bin}" ]] || fail "packaged tpu_v3_soc is missing or not executable"

for config in single_core single_chip mesh_2x2 mesh_4x4; do
    [[ -f "${package_dir}/configs/${config}.yaml" ]] \
        || fail "packaged configs/${config}.yaml is missing"
done

[[ -d "${package_dir}/firmware" ]] || fail "packaged firmware/ is missing"
[[ -f "${package_dir}/README.md" ]] || fail "packaged README.md is missing"

match_file "${repo_root}/LICENSE"        "${package_dir}/licenses/Apache-2.0.txt"
match_file "${repo_root}/NOTICE"         "${package_dir}/licenses/CDC-VP-NOTICE.txt"
match_file "${repo_root}/THIRD_PARTY.md" "${package_dir}/licenses/THIRD_PARTY.md"
echo "licence records PASS"

compgen -G "${package_dir}/libsystemc.so*" >/dev/null \
    || fail "the SystemC runtime was not bundled"

# ── the executable must not point back at the build machine ──────────────────

mapfile -t dynamic_paths < <(
    readelf -d "${package_bin}" \
        | sed -n \
            -e 's/.*Library rpath: \[\(.*\)\]/RPATH:\1/p' \
            -e 's/.*Library runpath: \[\(.*\)\]/RUNPATH:\1/p'
)
if [[ ${#dynamic_paths[@]} -ne 1
      || "${dynamic_paths[0]}" != 'RPATH:$ORIGIN' ]]; then
    printf '%s\n' "${dynamic_paths[@]:-<none>}" >"${evidence_dir}/dynamic_paths.txt"
    fail "packaged executable must have exactly RPATH=\$ORIGIN, got: ${dynamic_paths[*]:-<none>}"
fi
echo "RPATH PASS: \$ORIGIN"

# A source-tree path anywhere in the dynamic section would defeat the point
# even if the RPATH itself looked right.
if readelf -d "${package_bin}" | grep -qF "${repo_root}"; then
    fail "the packaged executable's dynamic section references the source tree"
fi

env -u LD_LIBRARY_PATH ldd "${package_bin}" >"${evidence_dir}/ldd.txt"
grep -q 'not found' "${evidence_dir}/ldd.txt" \
    && fail "packaged executable has an unresolved shared library"
systemc_path="$(
    awk '$1 ~ /^libsystemc\.so/ && $2 == "=>" { print $3 }' "${evidence_dir}/ldd.txt"
)"
[[ -n "${systemc_path}" ]] || fail "ldd did not resolve libsystemc"
package_real="$(cd "${package_dir}" && pwd -P)"
systemc_real="$(cd "$(dirname "${systemc_path}")" && pwd -P)"
[[ "${systemc_real}" == "${package_real}" ]] \
    || fail "libsystemc resolved outside the package: ${systemc_path}"
echo "local SystemC resolution PASS"

# ── run it with no access to the source or build tree ────────────────────────
#
# The package is moved, not copied: a bundle that still works only because the
# original directory exists is not portable, and copying would not catch that.

mv "${package_dir}" "${run_dir}/bundle"
moved_bin="${run_dir}/bundle/tpu_v3_soc"

for config in single_core single_chip mesh_2x2 mesh_4x4; do
    log="${evidence_dir}/run_${config}.log"
    ( cd "${run_dir}/bundle" \
      && env -u LD_LIBRARY_PATH timeout --kill-after=5 60 \
             ./tpu_v3_soc --config "configs/${config}.yaml" ) \
        >"${log}" 2>&1 \
        || { cat "${log}" >&2; fail "packaged run of ${config} failed"; }
    grep -q 'status         : OK' "${log}" \
        || fail "packaged run of ${config} did not report OK"
    grep -q "'${config}'" "${log}" \
        || fail "packaged run of ${config} did not name its configuration"
done
echo "packaged configuration runs PASS"

( cd "${run_dir}/bundle" \
  && env -u LD_LIBRARY_PATH ./tpu_v3_soc --config configs/mesh_4x4.yaml \
         --print-address-map ) >"${evidence_dir}/address_map.log" 2>&1 \
    || fail "packaged --print-address-map failed"
grep -q 'chip7.core1.mxu1_control' "${evidence_dir}/address_map.log" \
    || fail "packaged address map is incomplete"

env -u LD_LIBRARY_PATH "${moved_bin}" --version \
    >"${evidence_dir}/version.log" 2>&1 \
    || fail "packaged --version failed"
grep -q 'build type      : Release' "${evidence_dir}/version.log" \
    || fail "packaged binary does not report the Release build type"

# ── build manifest ───────────────────────────────────────────────────────────

manifest="${run_dir}/bundle/BUILD_MANIFEST.json"
[[ -f "${manifest}" ]] || fail "BUILD_MANIFEST.json is missing"

version_log="${evidence_dir}/version.log"

python3 - "${manifest}" "${version_log}" <<'PY' || fail "build manifest check failed"
import json
import re
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    manifest = json.load(handle)
version_text = open(sys.argv[2], encoding="utf-8").read()

def require(condition, message):
    if not condition:
        print(f"FAIL: {message}", file=sys.stderr)
        raise SystemExit(1)

require(manifest["schema"] == "cdc-vp/build-manifest/2", "unexpected schema")
require(manifest["platform"] == "tpu_v3_soc", "unexpected platform")
require(manifest["build"]["type"] == "Release", "build type is not Release")
require(manifest["build"]["cxx_compiler"], "no compiler recorded")
require(manifest["runtime"]["bundled_systemc"] is True,
        "SystemC not declared bundled")

# Two revisions, because they are captured at two different moments: the
# binary's is compiled in when the platform is configured, the package's is
# read when the bundle is assembled. Recording one and calling it "the
# revision" would let the manifest name a commit the binary was never built
# from.
source = manifest["source"]
require(source["revision_available"] is True,
        "this build is a Git checkout, so a revision must be recorded")
require(isinstance(source["build_revision"], str)
        and len(source["build_revision"]) == 40,
        "build_revision is not a full SHA")
require(isinstance(source["package_revision"], str)
        and len(source["package_revision"]) == 40,
        "package_revision is not a full SHA")
require(isinstance(source["revision_matches_binary"], bool),
        "revision_matches_binary is not a bool")
require(isinstance(source["dirty_at_package_time"], bool),
        "dirty_at_package_time is not a bool")

# The binary must agree with the manifest about which build it is.
match = re.search(r"CDC-VP revision : (\S+)", version_text)
require(match is not None, "--version does not report a revision")
require(match.group(1) == source["build_revision"],
        f"--version says {match.group(1)}, manifest says {source['build_revision']}")

# The MXU backend is chosen at build time and the binary refuses any other, so
# the manifest cannot name a backend that did not run.
backend = manifest["configuration"]["mxu_backend"]
require(backend == "fast", f"unexpected MXU backend {backend!r}")
match = re.search(r"MXU backend     : (\S+)", version_text)
require(match is not None, "--version does not report the MXU backend")
require(match.group(1) == backend,
        f"--version says backend {match.group(1)}, manifest says {backend}")
require("BF16" in manifest["configuration"]["mxu_arithmetic"],
        "the MXU arithmetic contract is not recorded")

# The manifest must not claim a dependency the binary does not contain: Spike
# is pinned but not linked until Phase 2, and saying otherwise would put a
# licence obligation and an accuracy claim on a binary that has neither.
spike = manifest["upstream"]["spike"]
require(spike["linked"] is False, "Spike is declared linked at Phase 1")
require(len(spike["pinned_revision"]) == 40, "Spike revision is not a full SHA")
require(spike["license"] == "BSD-3-Clause", "Spike licence is not recorded")
require(spike["pin_status"] == "candidate",
        "the Spike pin is not marked as a candidate (decision record D3)")

# A repository-wide Sauria option is not evidence that this binary contains or
# exposes Sauria. These fields must remain separate for licensing and result
# provenance.
sauria = manifest["upstream"]["sauria"]
require(sauria["global_option_enabled"] is False,
        "unexpected Sauria global option state in the child build")
require(sauria["linked"] is False,
        "Sauria is declared linked into the Phase 1 binary")
require(sauria["selectable"] is False,
        "Sauria is declared selectable in the Phase 1 binary")

require(manifest["configuration"]["max_chips"] == 8, "max_chips is not 8")
print("build manifest PASS")
PY

# ── negative control: a manifest generated outside a Git checkout ────────────
#
# This is where the manifest used to emit a bare `unknown` and produce invalid
# JSON — so every consumer failed on exactly the builds whose provenance most
# needed reading. An exported tarball or a container without `git` hits it.

nogit_dir="${work_dir}/nogit"
mkdir -p "${nogit_dir}"
cmake \
    -DMANIFEST_PATH="${nogit_dir}/BUILD_MANIFEST.json" \
    -DSOURCE_DIR="${nogit_dir}" \
    -DBUILD_REVISION=unknown \
    -DPLATFORM=tpu_v3_soc \
    -DBUILD_TYPE=Release \
    -DCXX_COMPILER=/usr/bin/g++ \
    -DCXX_COMPILER_ID=GNU \
    -DCXX_COMPILER_VERSION=11.5.0 \
    -DCMAKE_VERSION_USED=3.31.8 \
    -DSYSTEMC_HOME="${systemc_home}" \
    -DMXU_BACKEND=fast \
    -DSPIKE_REVISION=16c0b60119f65a648643cf5d41e4e38e871f0bad \
    -DSPIKE_LINKED=FALSE \
    -DSAURIA_OPTION_ENABLED=TRUE \
    -DSAURIA_LINKED=FALSE \
    -DSAURIA_SELECTABLE=FALSE \
    -P "${platform_dir}/cmake/write_build_manifest.cmake" \
    >"${evidence_dir}/nogit_manifest.log" 2>&1 \
    || fail "manifest generation failed outside a Git checkout"

python3 - "${nogit_dir}/BUILD_MANIFEST.json" <<'PY' \
    || fail "the manifest generated outside a Git checkout is not usable"
import json
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    manifest = json.load(handle)      # invalid JSON fails here, which is the point

source = manifest["source"]
assert source["revision_available"] is False, source
assert source["build_revision"] is None, source
assert source["package_revision"] is None, source
assert source["revision_matches_binary"] is None, source
assert source["dirty_at_package_time"] is None, source
sauria = manifest["upstream"]["sauria"]
assert sauria["global_option_enabled"] is True, sauria
assert sauria["linked"] is False, sauria
assert sauria["selectable"] is False, sauria
print("no-git manifest PASS")
PY

# ── negative control: an invalid MXU backend must not configure ──────────────
#
# `set_property(... STRINGS ...)` only fills a GUI drop-down; without an
# explicit check a typo configured happily and was copied verbatim into the
# manifest, naming a backend the binary does not contain.

for bad_backend in not_a_backend sauria; do
    if cmake -S "${repo_root}" -B "${work_dir}/badcfg_${bad_backend}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DSYSTEMC_HOME="${systemc_home}" \
        -DCDC_BUILD_TPU_V3_SOC=ON \
        -DCDC_BUILD_TPU_V3_TESTS=OFF \
        -DCDC_BUILD_TESTS=OFF \
        -DCDC_BUILD_MINI_TLM=OFF \
        -DCDC_BUILD_CPU_EVAL=OFF \
        -DCDC_BUILD_CUSTOM_SOC=OFF \
        -DCDC_BUILD_NOC_SOC=OFF \
        -DTPU_V3_MXU_BACKEND="${bad_backend}" \
        >"${evidence_dir}/badcfg_${bad_backend}.log" 2>&1; then
        fail "TPU_V3_MXU_BACKEND=${bad_backend} configured successfully"
    fi
    grep -q "TPU_V3_MXU_BACKEND" "${evidence_dir}/badcfg_${bad_backend}.log" \
        || fail "the ${bad_backend} refusal does not name TPU_V3_MXU_BACKEND"
done
echo "MXU backend validation PASS"

echo "tpu_v3_soc packaging regression PASS"
