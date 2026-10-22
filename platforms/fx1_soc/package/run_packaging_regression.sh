#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# VP_FX1_V3.0 distribution gate. Proves the package works away from CDC-VP:
#
#   1. a fresh Release build with every other platform off, then fx1_soc_package;
#   2. the package copied elsewhere, with the build tree made unreadable;
#   3. check_package.sh on the copy, including "no text file names CDC-VP";
#   4. with an empty environment (env -i, no LD_LIBRARY_PATH):
#      run_vp.sh version, smoke, isp for every sample; a sample with a wrong
#      golden SHA must FAIL (the check is not a rubber stamp);
#   5. the firmware rebuilt from the shipped sources with the shipped Makefile
#      must equal the packaged ELF byte for byte, and pass smoke again; the
#      DDR3 and camera examples built from them pass (camera: raw10, i2c_irq;
#      camera -> ISP: camera_isp_raw12);
#   6. a copy without build/ (what a clone of the SDK repository holds) must
#      pass `sha256sum -c SHA256SUMS`;
#   7. the one-terminal `run_vp.sh isp d65 interactive` flow through a
#      pseudo-terminal (when `script` and socat or nc exist).
#
#   platforms/fx1_soc/package/run_packaging_regression.sh
#   env: FX1_DEMO_RAW=<outdoor1 RAW> adds the internal sample, JOBS=N (default 1),
#        PACKAGING_KEEP_ARTIFACTS=1 keeps the temporary tree,
#        PACKAGING_SYSTEMC_HOME=<prefix> (default /opt/systemc-2.3.4). The
#        ambient SYSTEMC_HOME is deliberately ignored: a login shell may point
#        it at another SystemC (Fast Models ships one), and the gate must
#        build what is shipped.
#
# Exit status: 0 pass, 1 fail. There is no skip: without a toolchain or
# SystemC there is no package, and that is a failure.
set -uo pipefail
export PATH="/usr/bin:/bin:${PATH}"
export CC=/usr/bin/gcc CXX=/usr/bin/g++
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
src="$(cd "${here}/../../.." && pwd)"
version="VP_FX1_V3.0"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/fx1_pkgreg.XXXXXX")"
failures=0
step() { echo; echo "=== $*"; }
pass() { echo "  ok   $*"; }
bad() { echo "  FAIL $*"; failures=$((failures + 1)); }
finish() {
    chmod -R u+rwX "${tmp}/build" 2>/dev/null || true
    if [[ ${failures} -eq 0 && "${PACKAGING_KEEP_ARTIFACTS:-0}" != 1 ]]; then
        rm -rf "${tmp}"
    else
        echo "artifacts kept in ${tmp}"
    fi
}
trap finish EXIT

step "1. fresh Release build (FX1 only) and fx1_soc_package"
cfg=(-S "${src}" -B "${tmp}/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
     -DCDC_BUILD_FX1_SOC=ON -DFX1_SOC_BUILD_FIRMWARE=ON -DCDC_BUILD_TESTS=ON -DFX1_BUILD_MIPI_CSI2=ON
     -DCDC_BUILD_MINI_TLM=OFF -DCDC_BUILD_CPU_EVAL=OFF -DCDC_BUILD_CUSTOM_SOC=OFF -DCDC_BUILD_NOC_SOC=OFF
     -DCDC_BUILD_TPU_V3_SOC=OFF -DCDC_BUILD_RISCV_VPP_COMPILER_VP=OFF
     -DCDC_PACKAGE_ROOT="${tmp}/out")
cfg+=(-DSYSTEMC_HOME="${PACKAGING_SYSTEMC_HOME:-/opt/systemc-2.3.4}")
[[ -n "${FX1_DEMO_RAW:-}" ]] && cfg+=(-DFX1_DEMO_RAW="${FX1_DEMO_RAW}")
if cmake "${cfg[@]}" > "${tmp}/configure.log" 2>&1 \
        && cmake --build "${tmp}/build" --target fx1_soc_package -j "${JOBS:-1}" > "${tmp}/build.log" 2>&1; then
    pass "packaged into ${tmp}/out/${version}"
else
    tail -n 30 "${tmp}/configure.log" "${tmp}/build.log" 2>/dev/null
    bad "configure, build or packaging failed (logs in ${tmp})"
    exit 1
fi

tooldir="$(dirname "$(sed -n 's/^FX1_RISCV_PREFIX:[A-Z]*=//p' "${tmp}/build/CMakeCache.txt")gcc")"

step "2. move the package; make the build tree unreadable"
pkg="${tmp}/elsewhere/sdk"
mkdir -p "${tmp}/elsewhere"
cp -a "${tmp}/out/${version}" "${pkg}"
chmod -R a-rwx "${tmp}/build"
pass "package at ${pkg}"
test="${pkg}/sw/bootloader/test/${version}"
run() { env -i PATH=/usr/bin:/bin HOME="${tmp}/elsewhere" "$@"; }

step "3. check_package.sh"
"${here}/check_package.sh" "${pkg}" --source "${src}" || bad "check_package failed"

step "4. run from the copy with an empty environment"
out="$(run "${test}/run_vp.sh" version 2>&1)"
grep -q "fx1_soc ${version}" <<< "${out}" && grep -q "build type *Release" <<< "${out}" \
    && pass "version: Release ${version}" || bad "run_vp.sh version: ${out}"
run "${test}/run_vp.sh" smoke > "${tmp}/smoke.log" 2>&1 && grep -q "SMOKE PASS" "${tmp}/smoke.log" \
    && pass "smoke PASS" || { tail -n 15 "${tmp}/smoke.log"; bad "smoke"; }
for d in "${test}"/isp/*/; do
    s="$(basename "${d}")"
    if run "${test}/run_vp.sh" isp "${s}" > "${tmp}/isp_${s}.log" 2>&1 && grep -q "ISP PASS" "${tmp}/isp_${s}.log" \
            && [[ -s "${pkg}/sw/bootloader/build/${version}/isp/${s}/platform.png" ]]; then
        pass "isp ${s}: NV12 = golden $(cut -c1-12 "${d}expected_nv12.sha256")..., PNG written"
    else
        tail -n 15 "${tmp}/isp_${s}.log"
        bad "isp ${s}"
    fi
done
# negative control: a sample whose golden is wrong must fail
cp -a "${test}/isp/d65" "${test}/isp/tampered"
echo 0000000000000000000000000000000000000000000000000000000000000000 > "${test}/isp/tampered/expected_nv12.sha256"
run "${test}/run_vp.sh" isp tampered > "${tmp}/isp_tampered.log" 2>&1
rc=$?
[[ ${rc} -eq 1 ]] && grep -q "ISP FAIL: NV12 differs" "${tmp}/isp_tampered.log" \
    && pass "wrong golden is reported (exit 1)" || bad "wrong golden gave exit ${rc}"
rm -rf "${test}/isp/tampered" "${pkg}/sw/bootloader/build/${version}/isp/tampered"

step "5. firmware rebuilt from the shipped sources"
want="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["artefacts"]["sw/bootloader/build/VP_FX1_V3.0/freertos_cli.elf"]["sha256"])' \
    "${pkg}/vp/doc/${version}/BUILD_MANIFEST.json")"
if run make -s -C "${pkg}/sw/bootloader/sources/${version}" TOOLDIR="${tooldir}" clean all > "${tmp}/make.log" 2>&1; then
    got="$(sha256sum "${pkg}/sw/bootloader/build/${version}/freertos_cli.elf" | cut -d' ' -f1)"
    [[ "${got}" == "${want}" ]] && pass "rebuilt ELF equals the packaged one (${got:0:12}...)" \
        || bad "rebuilt ELF ${got:0:12} differs from the packaged ${want:0:12}"
    run "${test}/run_vp.sh" smoke > "${tmp}/smoke2.log" 2>&1 && pass "smoke PASS with the rebuilt ELF" \
        || bad "smoke with the rebuilt ELF"
    # Not only the ELF: every packaged firmware output (.dis, .map, .sections.txt)
    # must come out identical from a rebuild at another path.
    "${here}/check_package.sh" "${pkg}" > "${tmp}/check_rebuilt.log" 2>&1 \
        && pass "every firmware output of the rebuild matches the manifest" \
        || { cat "${tmp}/check_rebuilt.log"; bad "a rebuilt firmware output differs from the manifest"; }
else
    cat "${tmp}/make.log"
    bad "make with the shipped Makefile failed"
fi

step "5b. DDR3 firmware-init examples, built and run from the shipped sources"
if run make -s -C "${pkg}/sw/bootloader/sources/${version}" TOOLDIR="${tooldir}" ddr3 > "${tmp}/make_ddr3.log" 2>&1; then
    run "${test}/run_vp.sh" ddr3 > "${tmp}/ddr3.log" 2>&1 && grep -q "DDR3 EXAMPLES PASS" "${tmp}/ddr3.log" \
        && pass "run_vp.sh ddr3: $(grep -c '  PASS ' "${tmp}/ddr3.log") examples PASS" \
        || { cat "${tmp}/ddr3.log"; bad "run_vp.sh ddr3"; }
    rm -rf "${pkg}/sw/bootloader/build/${version}/ddr3"
else
    cat "${tmp}/make_ddr3.log"
    bad "make ddr3 with the shipped Makefile failed"
fi

step "5c. camera examples, built from the shipped sources, three run (fx1_soc --camera)"
if run make -s -C "${pkg}/sw/bootloader/sources/${version}" TOOLDIR="${tooldir}" camera > "${tmp}/make_camera.log" 2>&1; then
    run "${test}/run_vp.sh" camera camera_raw10 camera_i2c_irq > "${tmp}/camera.log" 2>&1 \
        && grep -q "CAMERA EXAMPLES PASS" "${tmp}/camera.log" \
        && pass "run_vp.sh camera: $(grep -c '  PASS ' "${tmp}/camera.log") examples PASS" \
        || { cat "${tmp}/camera.log"; bad "run_vp.sh camera"; }
    # Camera -> ISP through the camera manager (CSI-2 M6b): RAW12 zero-copy.
    run "${test}/run_vp.sh" camera-isp camera_isp_raw12 > "${tmp}/camera_isp.log" 2>&1 \
        && grep -q "CAMERA ISP EXAMPLES PASS" "${tmp}/camera_isp.log" \
        && pass "run_vp.sh camera-isp: camera_isp_raw12 PASS" \
        || { cat "${tmp}/camera_isp.log"; bad "run_vp.sh camera-isp"; }
    rm -rf "${pkg}/sw/bootloader/build/${version}/camera"
else
    cat "${tmp}/make_camera.log"
    bad "make camera with the shipped Makefile failed"
fi

step "6. SHA256SUMS on a copy without build/ (an SDK repository clone)"
clone="${tmp}/clone"
mkdir -p "${clone}"
(cd "${pkg}" && tar --exclude="sw/bootloader/build" -cf - vp sw) | (cd "${clone}" && tar -xf -)
(cd "${clone}" && sha256sum --quiet -c "vp/doc/${version}/SHA256SUMS") && pass "sha256sum -c passes without build/" \
    || bad "sha256sum -c fails on a clone"

step "7. one-terminal interactive ISP flow (pseudo-terminal)"
if command -v script > /dev/null && { command -v socat > /dev/null || command -v nc > /dev/null; }; then
    port=$((20000 + RANDOM % 20000))
    (sleep 4; printf 'isp run\r'; sleep 12; printf 'poweroff\r'; sleep 30) \
        | timeout 120 script -q -e -c "env -i PATH=/usr/bin:/bin HOME=${tmp}/elsewhere ${test}/run_vp.sh isp d65 interactive ${port}" \
            "${tmp}/interactive.ts" > /dev/null 2>&1
    rc=$?
    tr -d '\r' < "${tmp}/interactive.ts" | grep -q "ISP PASS" && [[ ${rc} -eq 0 ]] \
        && pass "isp d65 interactive: isp run, poweroff, ISP PASS" || bad "interactive flow (exit ${rc}, ${tmp}/interactive.ts)"
    pgrep -f "[f]x1_soc .*--uart0-socket ${port}" > /dev/null && bad "a VP is still running on port ${port}"
else
    bad "need script(1) and socat or nc for the interactive check"
fi

echo
if [[ ${failures} -eq 0 ]]; then
    echo "fx1 packaging regression: PASS"
else
    echo "fx1 packaging regression: FAIL (${failures})"
fi
[[ ${failures} -eq 0 ]]
