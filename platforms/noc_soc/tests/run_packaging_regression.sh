#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Reproducible Step 10.4 distribution gate.
#
# This runner deliberately creates a fresh parent build, install prefix,
# standalone installed-package consumer and binary package. It then verifies
# every sign-off claim rather than trusting a previously assembled artifact:
#
#   * the installed compiled noc_interconnect target is consumable;
#   * installed FlooNoC headers match the source set and carry known SPDX ids;
#   * development and binary-package licence/provenance files are byte exact;
#   * the packaged executable has exactly RPATH=$ORIGIN;
#   * libsystemc resolves from beside that executable with LD_LIBRARY_PATH unset;
#   * the packaged config runs a survey;
#   * the existing full firmware/survey regression passes against the package;
#   * FreeRTOS level 128 accepts UART replay and completes its CLI session.
#
# Exit codes: 0 pass, 1 fail, 77 skipped when the RISC-V firmware toolchain is
# unavailable. Set PACKAGING_KEEP_ARTIFACTS=1 to retain a successful temporary
# tree, PACKAGING_WORK_DIR to use and retain an explicit empty directory, or
# PACKAGING_SYSTEMC_HOME to select a non-default Accellera installation.

set -euo pipefail

readonly SKIP=77

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
platform_dir="$(cd "${script_dir}/.." && pwd)"
repo_root="$(cd "${platform_dir}/../.." && pwd)"

# Reproducibility takes precedence over ambient login-shell compiler variables.
# Explicit gate-specific overrides remain available for a deliberate toolchain
# qualification run.
export CC="${PACKAGING_CC:-/usr/bin/gcc}"
export CXX="${PACKAGING_CXX:-/usr/bin/g++}"
export PATH="/usr/bin:/bin:${PATH}"

# Do not inherit an unrelated Fast Models SYSTEMC_HOME from the developer's
# login shell. The signed model build uses this Accellera installation unless
# the packaging gate is overridden explicitly.
systemc_home="${PACKAGING_SYSTEMC_HOME:-/opt/systemc-2.3.4}"
owns_work_dir=0
if [[ -n "${PACKAGING_WORK_DIR:-}" ]]; then
    work_dir="${PACKAGING_WORK_DIR}"
    if [[ -e "${work_dir}" && ! -d "${work_dir}" ]]; then
        echo "FAIL: PACKAGING_WORK_DIR exists and is not a directory: ${work_dir}" >&2
        exit 1
    fi
    mkdir -p "${work_dir}"
    if find "${work_dir}" -mindepth 1 -print -quit | grep -q .; then
        echo "FAIL: PACKAGING_WORK_DIR must be empty: ${work_dir}" >&2
        exit 1
    fi
else
    work_dir="$(mktemp -d "${TMPDIR:-/tmp}/floo_noc_packaging_regression.XXXXXX")"
    owns_work_dir=1
fi

build_dir="${work_dir}/build"
install_prefix="${work_dir}/install"
consumer_build="${work_dir}/consumer"
package_root="${work_dir}/package"
package_dir="${package_root}/noc_soc"
evidence_dir="${work_dir}/evidence"
mkdir -p "${evidence_dir}"

cleanup()
{
    local status=$?
    trap - EXIT
    if [[ ${owns_work_dir} -eq 1
          && "${PACKAGING_KEEP_ARTIFACTS:-0}" != "1"
          && (${status} -eq 0 || ${status} -eq ${SKIP}) ]]; then
        "${CMAKE_COMMAND:-cmake}" -E remove_directory "${work_dir}"
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
    command -v "$1" >/dev/null 2>&1 \
        || fail "required command not found: $1"
}

match_file()
{
    local expected="$1"
    local actual="$2"
    [[ -f "${actual}" ]] || fail "required package record missing: ${actual}"
    cmp -s "${expected}" "${actual}" \
        || fail "package record differs from source: ${actual}"
}

require_command cmake
require_command readelf
require_command ldd
require_command cmp
require_command diff
require_command timeout
require_command find

echo "packaging work directory: ${work_dir}"
"${CC}" -dumpfullversion
"${CXX}" --version | head

cmake -S "${repo_root}" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DSYSTEMC_HOME="${systemc_home}" \
    -DCDC_BUILD_NOC_SOC=ON \
    -DCDC_BUILD_TESTS=OFF \
    -DCDC_BUILD_MINI_TLM=OFF \
    -DCDC_BUILD_CPU_EVAL=ON \
    -DCDC_BUILD_CUSTOM_SOC=OFF \
    -DCDC_PACKAGE_ROOT="${package_root}"
cmake --build "${build_dir}" --parallel
cmake --install "${build_dir}" --prefix "${install_prefix}"
cmake --build "${build_dir}" --target noc_soc_package --parallel

# The installed consumer is a separate source tree. Its only connection to the
# project is find_package(cdc-components) through this run's fresh prefix.
cmake -S "${repo_root}/components/floo_noc_model/tests/installed_consumer" \
    -B "${consumer_build}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${install_prefix}" \
    -DSYSTEMC_HOME="${systemc_home}"
cmake --build "${consumer_build}" --parallel
consumer_output="$("${consumer_build}/floo_noc_installed_consumer" 2>&1)"
grep -q '^installed noc_interconnect consumer PASS$' <<<"${consumer_output}" \
    || fail "installed compiled-target consumer did not report PASS"
echo "installed consumer PASS"

# Compare the installed header manifest to the source manifest, then check every
# installed FlooNoC header's licence identifier. Scope this to floo_noc_model:
# the parent development prefix contains unrelated component headers too.
source_headers="${evidence_dir}/source_headers.txt"
installed_headers="${evidence_dir}/installed_headers.txt"
(
    cd "${repo_root}/components/floo_noc_model/include/floo_noc_model"
    find . -type f -printf '%P\n' | sort
) >"${source_headers}"
(
    cd "${install_prefix}/include/floo_noc_model"
    find . -type f -printf '%P\n' | sort
) >"${installed_headers}"
diff -u "${source_headers}" "${installed_headers}" \
    >"${evidence_dir}/header_manifest.diff" \
    || fail "installed FlooNoC header manifest differs from source"

header_count=0
while IFS= read -r relative; do
    [[ -n "${relative}" ]] || continue
    header="${install_prefix}/include/floo_noc_model/${relative}"
    grep -Eq 'SPDX-License-Identifier: (Apache-2.0|SHL-0.51)' "${header}" \
        || fail "installed header has no expected SPDX identifier: ${relative}"
    header_count=$((header_count + 1))
done <"${installed_headers}"
[[ ${header_count} -gt 0 ]] || fail "installed FlooNoC header set is empty"
echo "installed header SPDX PASS: ${header_count}/${header_count}"

dev_licenses="${install_prefix}/share/licenses/cdc-components/floo_noc_model"
match_file "${repo_root}/LICENSE" \
    "${dev_licenses}/Apache-2.0.txt"
match_file "${repo_root}/NOTICE" \
    "${dev_licenses}/NOTICE.txt"
match_file "${repo_root}/components/floo_noc_model/LICENSES/SHL-0.51.txt" \
    "${dev_licenses}/SHL-0.51.txt"
match_file "${repo_root}/components/floo_noc_model/PROVENANCE.md" \
    "${dev_licenses}/PROVENANCE.md"

binary_licenses="${package_dir}/licenses"
match_file "${repo_root}/LICENSE" \
    "${binary_licenses}/Apache-2.0.txt"
match_file "${repo_root}/NOTICE" \
    "${binary_licenses}/CDC-VP-NOTICE.txt"
match_file "${repo_root}/THIRD_PARTY.md" \
    "${binary_licenses}/THIRD_PARTY.md"
match_file "${repo_root}/third_party/riscv-vp/LICENSE" \
    "${binary_licenses}/RISC-V-VP-MIT.txt"
match_file "${repo_root}/components/floo_noc_model/LICENSES/SHL-0.51.txt" \
    "${binary_licenses}/SHL-0.51.txt"
match_file "${repo_root}/components/floo_noc_model/PROVENANCE.md" \
    "${binary_licenses}/FlooNoC-PROVENANCE.md"
echo "licence and provenance records PASS"

package_bin="${package_dir}/noc_soc"
package_config="${package_dir}/configs/default.yaml"
[[ -x "${package_bin}" ]] || fail "packaged noc_soc is missing or not executable"
[[ -f "${package_config}" ]] || fail "packaged default.yaml is missing"

mapfile -t dynamic_paths < <(
    readelf -d "${package_bin}" \
        | sed -n \
            -e 's/.*Library rpath: \[\(.*\)\]/RPATH:\1/p' \
            -e 's/.*Library runpath: \[\(.*\)\]/RUNPATH:\1/p'
)
if [[ ${#dynamic_paths[@]} -ne 1
      || "${dynamic_paths[0]}" != 'RPATH:$ORIGIN' ]]; then
    printf '%s\n' "${dynamic_paths[@]}" \
        >"${evidence_dir}/dynamic_paths.txt"
    fail "packaged executable must have exactly RPATH=\\$ORIGIN"
fi
echo "RPATH PASS: \$ORIGIN"

env -u LD_LIBRARY_PATH ldd "${package_bin}" \
    >"${evidence_dir}/ldd.txt"
grep -q 'not found' "${evidence_dir}/ldd.txt" \
    && fail "packaged executable has an unresolved shared library"
systemc_path="$(
    awk '$1 == "libsystemc.so.2.3" && $2 == "=>" { print $3 }' \
        "${evidence_dir}/ldd.txt"
)"
[[ -n "${systemc_path}" ]] \
    || fail "ldd did not resolve libsystemc.so.2.3"
package_real="$(cd "${package_dir}" && pwd -P)"
systemc_real="$(cd "$(dirname "${systemc_path}")" && pwd -P)"
[[ "${systemc_real}" == "${package_real}" ]] \
    || fail "libsystemc resolved outside the package: ${systemc_path}"
echo "local SystemC resolution PASS"

survey_log="${evidence_dir}/packaged_survey.log"
env -u LD_LIBRARY_PATH timeout --kill-after=5 120 \
    "${package_bin}" -c "${package_config}" \
    --mode survey --noc-timing fast --sim-us 20 \
    >"${survey_log}" 2>&1 \
    || fail "packaged survey failed; see ${survey_log}"
grep -q 'result   all bytes match' "${survey_log}" \
    || fail "packaged survey did not report all bytes match"
grep -Eq 'Taking trap|\[PC\] trapped|Error:|\([EW][0-9]{3}\)|mismatch|FAIL' \
    "${survey_log}" \
    && fail "packaged survey log contains a failure symptom"
echo "packaged survey PASS"

# Reuse the signed platform-level workload rather than maintaining a second
# firmware oracle. It also checks detailed and fast timing plus ownership.
set +e
env -u LD_LIBRARY_PATH \
    NOC_SOC_BIN="${package_bin}" \
    LOG_DIR="${evidence_dir}/firmware" \
    "${script_dir}/run_firmware_regression.sh"
firmware_status=$?
set -e
if [[ ${firmware_status} -eq ${SKIP} ]]; then
    echo "SKIP: packaged firmware gate needs the RISC-V toolchain" >&2
    exit "${SKIP}"
fi
[[ ${firmware_status} -eq 0 ]] \
    || fail "packaged firmware regression failed"

# The RTOS and UART CLI must also run on the package consumer, not only on the
# development binary. Step 12.8 retains every level-124 marker and adds the
# host-bridge/file-replay path, so it is the strongest single packaged image.
# Independent reproduction of the earlier stages remains the responsibility of
# `noc_soc_freertos_regression`.
set +e
env -u LD_LIBRARY_PATH \
    NOC_SOC_BIN="${package_bin}" \
    FREERTOS_STEPS="12.8" \
    LOG_DIR="${evidence_dir}/freertos" \
    "${script_dir}/run_freertos_regression.sh"
freertos_status=$?
set -e
if [[ ${freertos_status} -eq ${SKIP} ]]; then
    echo "SKIP: packaged FreeRTOS gate needs the RISC-V toolchain" >&2
    exit "${SKIP}"
fi
[[ ${freertos_status} -eq 0 ]] \
    || fail "packaged FreeRTOS regression failed"

echo "noc_soc packaging regression PASS"
