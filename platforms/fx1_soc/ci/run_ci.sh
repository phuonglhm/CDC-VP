#!/usr/bin/env bash
# FX1 SoC VP: configure, build and test in one go (plan P5).
#
#   platforms/fx1_soc/ci/run_ci.sh [--build-dir DIR] [--jobs N] [--smoke-only]
#
# Stages, all run even if an earlier test stage fails (a red stage does not
# hide the others' results):
#   1. configure  CDC_BUILD_FX1_SOC=ON, FX1_SOC_BUILD_FIRMWARE=ON (the
#                 acceptance configuration: a missing cross toolchain is a
#                 configure error), Release, Ninja. Every test option the gate
#                 depends on is set explicitly, so a reused build directory
#                 whose cache turned one off is put back, not trusted.
#   2. manifest   every test named in ci/required_tests.txt for a stage that
#                 runs is registered and selected by its stage; otherwise, or
#                 when the manifest is missing/unreadable/empty, stop with FAIL
#                 before building (a gate that silently runs fewer tests must
#                 not report PASS). FX1_CI_MANIFEST=<file> overrides the path.
#   3. build      target fx1_all only (see platforms/fx1_soc/CMakeLists.txt)
#   4. smoke      ctest -L "fx1_unit|fx1_smoke", plus the FX1 bus tests
#   5. regression ctest -L fx1_regression (skipped with --smoke-only); needs
#                 python3 with numpy (fx1_regression_isp_fixture_check)
#
# Every ctest stage runs with --no-tests=error and writes JUnit XML. Artifacts
# go to <build-dir>/fx1-ci-artifacts: junit-*.xml, ctest logs, every firmware
# UART log, a summary with tool versions and results. Exit status: 0 when
# every stage passed, 1 otherwise, 2 on usage errors.
#
# Host tools: /usr/bin is put first on PATH and CC/CXX default to /usr/bin/gcc
# and /usr/bin/g++, because some hosts carry vendor binutils wrappers earlier
# on PATH that must not be picked up. Override CC/CXX in the environment if
# your host compiler lives elsewhere.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BUILD="${ROOT}/build-fx1-ci"
JOBS=1
SMOKE_ONLY=0

while [ $# -gt 0 ]; do
    case "$1" in
        --build-dir) BUILD="$2"; shift 2 ;;
        --jobs) JOBS="$2"; shift 2 ;;
        --smoke-only) SMOKE_ONLY=1; shift ;;
        -h|--help) sed -n '2,/^set -uo/{/^set -uo/d;p}' "$0"; exit 0 ;;
        *) echo "run_ci.sh: unknown argument '$1'" >&2; exit 2 ;;
    esac
done
case "$BUILD" in /*) ;; *) BUILD="$(pwd)/$BUILD" ;; esac

export PATH="/usr/bin:/bin:${PATH}"
export CC="${CC:-/usr/bin/gcc}"
export CXX="${CXX:-/usr/bin/g++}"

ART="${BUILD}/fx1-ci-artifacts"
rm -rf "${ART}" && mkdir -p "${ART}" # fresh: no stale stage logs
SUMMARY="${ART}/summary.txt"
: > "${SUMMARY}"
log() { echo "[fx1-ci] $*" | tee -a "${SUMMARY}"; }

status=0
stage() { # stage <name> <command...>
    local name="$1"; shift
    log "stage ${name}: $*"
    "$@" > "${ART}/${name}.log" 2>&1
    local rc=$?
    tail -n 5 "${ART}/${name}.log" | sed 's/^/    /' | tee -a "${SUMMARY}"
    if [ ${rc} -ne 0 ]; then
        log "stage ${name}: FAILED (exit ${rc})"
        status=1
    else
        log "stage ${name}: ok"
    fi
    return ${rc}
}

log "repo ${ROOT}"
log "build ${BUILD}, jobs ${JOBS}"
log "CC $(${CC} -dumpfullversion 2>/dev/null || echo '?'), CXX $(${CXX} -dumpfullversion 2>/dev/null || echo '?')"
log "$(cmake --version | head -n 1)"
log "git $(git -C "${ROOT}" rev-parse --short HEAD 2>/dev/null || echo '?')$(git -C "${ROOT}" diff --quiet 2>/dev/null || echo ' (dirty)')"

stage configure cmake -S "${ROOT}" -B "${BUILD}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCDC_BUILD_FX1_SOC=ON -DFX1_SOC_BUILD_FIRMWARE=ON -DCDC_BUILD_TESTS=ON \
    -DFX1_DMA_BUILD_TESTS=ON -DFX1_DMA_BUILD_BUS_TESTS=ON -DBUS_SYSTEM_BUILD_TESTS=ON \
    || { log "configure failed: no build, no tests"; exit 1; }

SMOKE_SEL=(-L "fx1_unit|fx1_smoke")
BUS_SEL=(-R '^(bus_behavior|bus_config|bus_arbitration)$')
REGRESSION_SEL=(-L fx1_regression)
MANIFEST="${FX1_CI_MANIFEST:-${ROOT}/platforms/fx1_soc/ci/required_tests.txt}" # override: tests of the gate itself
# Fails (nonzero) whenever it cannot prove that every required test of the
# stages that will run is registered and selected by its stage: missing,
# unreadable or empty manifest, no entries, an entry outside a known section,
# a ctest listing that fails, or a required name that is not selected. With
# --smoke-only the regression section is not checked (that stage does not run;
# its isp_fixture_check test needs python3 with numpy).
check_manifest() {
    local -a lines=() stages=(smoke bus)
    [ ${SMOKE_ONLY} -eq 0 ] && stages+=(regression)
    if [ ! -f "${MANIFEST}" ] || [ ! -r "${MANIFEST}" ] || [ ! -s "${MANIFEST}" ]; then
        echo "manifest ${MANIFEST}: missing, unreadable or empty"
        return 1
    fi
    if ! mapfile -t lines < "${MANIFEST}"; then
        echo "manifest ${MANIFEST}: read failed"
        return 1
    fi
    local sel
    for sel in "${stages[@]}"; do
        local -n args="${sel^^}_SEL"
        if ! ctest --test-dir "${BUILD}" -N "${args[@]}" > "${ART}/ctest_n_${sel}.txt" 2>&1; then
            echo "ctest -N for stage ${sel} failed (see ctest_n_${sel}.txt)"
            return 1
        fi
        sed -n 's/^ *Test *#[0-9]*: //p' "${ART}/ctest_n_${sel}.txt" > "${ART}/registered_${sel}.txt"
    done
    local section="" line missing=0 checked=0 entries=0
    for line in "${lines[@]}"; do
        line="${line%$'\r'}"
        case "${line}" in
            ''|'#'*) continue ;;
            '['*']')
                section="${line:1:${#line}-2}"
                case "${section}" in smoke|bus|regression) ;; *)
                    echo "manifest: unknown section [${section}]"; return 1 ;;
                esac
                continue ;;
        esac
        if [ -z "${section}" ]; then
            echo "manifest: entry '${line}' before any [section]"
            return 1
        fi
        entries=$((entries + 1))
        [[ " ${stages[*]} " == *" ${section} "* ]] || continue # stage not run (--smoke-only)
        checked=$((checked + 1))
        if ! grep -qxF -- "${line}" "${ART}/registered_${section}.txt"; then
            echo "missing from stage ${section}: ${line}"
            missing=1
        fi
    done
    if [ ${entries} -eq 0 ] || [ ${checked} -eq 0 ]; then
        echo "manifest ${MANIFEST}: no required test entries for the stages that run"
        return 1
    fi
    [ ${missing} -eq 0 ] || return 1
    echo "all ${checked} required tests of stages ${stages[*]} registered (${entries} in the manifest)"
    return 0
}
stage manifest check_manifest \
    || { log "required tests missing (see manifest.log): not running a reduced gate"; exit 1; }
stage build cmake --build "${BUILD}" --target fx1_all -j "${JOBS}" \
    || { log "build failed: no tests"; exit 1; }

CTEST=(ctest --test-dir "${BUILD}" --no-tests=error -j "${JOBS}" --output-on-failure)
stage smoke "${CTEST[@]}" "${SMOKE_SEL[@]}" --output-junit "${ART}/junit-smoke.xml"
stage bus "${CTEST[@]}" "${BUS_SEL[@]}" --output-junit "${ART}/junit-bus.xml"
if [ ${SMOKE_ONLY} -eq 0 ]; then
    stage regression "${CTEST[@]}" "${REGRESSION_SEL[@]}" --output-junit "${ART}/junit-regression.xml"
else
    log "stage regression: skipped (--smoke-only)"
fi

# Artifacts: ctest's own logs and every firmware UART log (small text files).
cp -f "${BUILD}"/Testing/Temporary/LastTest*.log "${ART}/" 2>/dev/null
mkdir -p "${ART}/uart"
cp -f "${BUILD}"/fw/fx1_soc/*.uart.log "${ART}/uart/" 2>/dev/null
for name in smoke bus regression; do # failed tests, from each stage's own log
    [ -f "${ART}/${name}.log" ] || continue
    failed="$(sed -n '/The following tests FAILED:/,$p' "${ART}/${name}.log" | grep -E '^[[:space:]]+[0-9]+ - ')"
    [ -n "${failed}" ] && { log "failed in ${name}:"; echo "${failed}" | tee -a "${SUMMARY}"; }
done
log "artifacts in ${ART}"
log "result: $([ ${status} -eq 0 ] && echo PASS || echo FAIL)"
exit ${status}
