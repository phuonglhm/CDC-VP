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
#      golden SHA must FAIL and an empty golden be refused (the check is not a
#      rubber stamp); `isp FILE.raw` with d65's frame under an unquoted path
#      with spaces gives d65's golden, and a RAW of the wrong size is refused;
#      the VP-only post-Demosaic capture (--dump-demosaic, flag anywhere):
#      every sample and d65 auto equal their capture goldens, a wrong capture
#      golden FAILs, an empty one is refused, a user RAW is not compared, and
#      a host write error (file size limit) makes the run FAIL;
#   4d. the ISP RAW sequence sample (run_vp.sh isp-stream-sample, the system
#      FFmpeg): COMPLETE, every frame = isp_stream/expected_frames.txt, the
#      video checked; a wrong expected frame FAILs; a tuned copy of the profile
#      (BT.709), and the shared profile edited in place, make a BT.709 video
#      without the comparison; an unchanged copy elsewhere is still compared;
#      with no ffmpeg on
#      PATH the run is refused before the boot (PREFLIGHT_FAILED, install steps);
#   4e. the NPU (plan FX1_NPU_V47 N5): run_vp.sh npu-selftest; every shipped
#      fixture (run_vp.sh npu all, negative ones as declared); inherited model
#      knobs are removed and named, the result unchanged; a wrong golden FAILs
#      (exit 1), an unknown fixture is refused (exit 2); --print-command (one
#      fixture: the command, nothing run; `npu all --print-command` refused with
#      exit 2, no verdict, nothing run); the
#      fixture console flow (run_vp.sh npu long interactive + fx1_console.sh):
#      npu run, npu reset (recovery, soft reset, reopen), the job again: PASS;
#   5. the firmware rebuilt from the shipped sources with the shipped Makefile
#      must equal the packaged ELF byte for byte, and pass smoke again; the
#      DDR3 and camera examples built from them pass (camera: raw10, i2c_irq;
#      camera -> ISP: camera_isp_raw12); the scripted camera CLI sessions
#      (run_vp.sh camera-cli on the memory path, camera-direct-cli on the
#      direct CSI-2 -> ISP path, each frame against the golden; a direct
#      session fed a flat RAW file must FAIL); the camera console flow
#      (run_vp.sh interactive PORT --camera + fx1_console.sh: two direct-path
#      frames = golden; without --camera no camera; --camera-raw reaches the
#      VP; bad arguments refused), and the Ethernet example on every line
#      peer and the scripted Ethernet CLI session pass; `make npu-jobs` (host
#      compiler) gives the shipped NPU fixtures byte for byte, and one of them
#      runs from the generated directory; a second run keeps a user note and a
#      user fixture there; an NPUJOBS directory it did not make is refused
#      (exit status nonzero) with its files untouched, and so is a managed file
#      that is a link to a file outside NPUJOBS, with the link target untouched;
#   4c. a tuned shared profile with another write count (ISP-UPD-010): with
#      four more writes (NR curve used, two factory bad pixels; ISP-UPD-011:
#      and radial EE on; ISP-UPD-012: and the GTM manual curve) run_vp.sh isp
#      d65 gives the stand-alone model's NV12 for that profile (not the
#      golden); without the TUNE_* writes (fewer writes, reset values) it
#      gives the golden again;
#   6. a copy without build/ (what a clone of the SDK repository holds) must
#      pass `sha256sum -c SHA256SUMS`;
#   7. the one-terminal `run_vp.sh isp d65 interactive` flow through a
#      pseudo-terminal (when `script` and socat or nc exist), also with
#      --dump-demosaic (per-frame captures only).
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
# The stand-alone model, kept for step 4c's reference outputs.
cp "${tmp}/build/components/FX1_Components/isp/fx1_isp_run_raw" "${tmp}/ref_run_raw" 2>/dev/null \
    || bad "no fx1_isp_run_raw in the build (step 4c reference)"
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
# an empty golden is refused, never a PASS
: > "${test}/isp/tampered/expected_nv12.sha256"
run "${test}/run_vp.sh" isp tampered > "${tmp}/isp_empty_golden.log" 2>&1
rc=$?
[[ ${rc} -eq 2 ]] && grep -q "is not a SHA-256" "${tmp}/isp_empty_golden.log" \
    && pass "empty golden is refused (exit 2)" || bad "empty golden gave exit ${rc}"
rm -rf "${test}/isp/tampered" "${pkg}/sw/bootloader/build/${version}/isp/tampered"
# a user's RAW file: d65's frame under a path with spaces, passed unquoted
# (one word per argument), must give d65's golden without being compared
user_raw="${tmp}/elsewhere/my raw/d65 frame.raw"
mkdir -p "$(dirname "${user_raw}")"
cp "${src}/components/isp_tlm/input/D65_raw_2688x1520_5376.raw" "${user_raw}"
run "${test}/run_vp.sh" isp "${tmp}/elsewhere/my" "raw/d65" "frame.raw" > "${tmp}/isp_user_raw.log" 2>&1
rc=$?
user_nv12="${pkg}/sw/bootloader/build/${version}/isp/raw_d65_frame/platform.nv12"
[[ ${rc} -eq 0 ]] && grep -q "ISP DONE" "${tmp}/isp_user_raw.log" && [[ -s "${user_nv12}" ]] \
    && [[ "$(sha256sum "${user_nv12}" | cut -d' ' -f1)" == "$(cat "${test}/isp/d65/expected_nv12.sha256")" ]] \
    && pass "isp FILE.raw (unquoted path with spaces): d65's frame gives d65's golden, not compared" \
    || { tail -n 15 "${tmp}/isp_user_raw.log"; bad "isp FILE.raw gave exit ${rc}"; }
head -c 1000 "${user_raw}" > "${tmp}/elsewhere/short.raw"
run "${test}/run_vp.sh" isp "${tmp}/elsewhere/short.raw" > "${tmp}/isp_short_raw.log" 2>&1
rc=$?
[[ ${rc} -eq 2 ]] && grep -q "expected 8171520" "${tmp}/isp_short_raw.log" \
    && pass "isp FILE.raw of the wrong size is refused (exit 2)" || bad "short RAW gave exit ${rc}"
# VP-only post-Demosaic capture: the job's final frame against the capture
# golden of the stand-alone model; NV12 golden unchanged by the option.
for d in "${test}"/isp/*/; do
    s="$(basename "${d}")"
    o="${pkg}/sw/bootloader/build/${version}/isp/${s}"
    if run "${test}/run_vp.sh" isp "${s}" --dump-demosaic > "${tmp}/dms_${s}.log" 2>&1 \
            && grep -q "ISP PASS" "${tmp}/dms_${s}.log" && grep -q "DEMOSAIC PASS" "${tmp}/dms_${s}.log" \
            && [[ "$(sha256sum "${o}/demosaic.rgb16" | cut -d' ' -f1)" == "$(cat "${d}expected_demosaic.sha256")" ]] \
            && run python3 "${test}/demosaic_validate.py" "${o}/demosaic" > /dev/null \
            && grep -q "VP debug tap: post-Demosaic, pre-CCM" "${o}/summary.txt"; then
        pass "isp ${s} --dump-demosaic: capture = golden $(cut -c1-12 "${d}expected_demosaic.sha256")..., NV12 golden kept"
    else
        tail -n 15 "${tmp}/dms_${s}.log"
        bad "isp ${s} --dump-demosaic"
    fi
done
o="${pkg}/sw/bootloader/build/${version}/isp/d65"
if run "${test}/run_vp.sh" isp --dump-demosaic d65 auto > "${tmp}/dms_auto.log" 2>&1 \
        && grep -q "ISP PASS" "${tmp}/dms_auto.log" && grep -q "DEMOSAIC PASS" "${tmp}/dms_auto.log" \
        && [[ "$(sha256sum "${o}/demosaic.rgb16" | cut -d' ' -f1)" == "$(cat "${test}/isp/d65/expected_auto_demosaic.sha256")" ]] \
        && grep -q "frame 3 of the job's 3" "${o}/summary.txt"; then
    pass "isp --dump-demosaic d65 auto: the last render frame (3 of 3, policy WB) = auto capture golden"
else
    tail -n 15 "${tmp}/dms_auto.log"
    bad "isp d65 auto --dump-demosaic"
fi
cp -a "${test}/isp/d65" "${test}/isp/tampered"
echo 0000000000000000000000000000000000000000000000000000000000000000 > "${test}/isp/tampered/expected_demosaic.sha256"
run "${test}/run_vp.sh" isp tampered --dump-demosaic > "${tmp}/dms_tampered.log" 2>&1
rc=$?
[[ ${rc} -eq 1 ]] && grep -q "ISP PASS" "${tmp}/dms_tampered.log" && grep -q "DEMOSAIC FAIL" "${tmp}/dms_tampered.log" \
    && pass "wrong capture golden is reported (exit 1, NV12 still PASS)" || bad "wrong capture golden gave exit ${rc}"
: > "${test}/isp/tampered/expected_demosaic.sha256"
run "${test}/run_vp.sh" isp tampered --dump-demosaic > "${tmp}/dms_empty_golden.log" 2>&1
rc=$?
[[ ${rc} -eq 2 ]] && grep -q "is not a SHA-256" "${tmp}/dms_empty_golden.log" \
    && pass "empty capture golden is refused (exit 2)" || bad "empty capture golden gave exit ${rc}"
rm -rf "${test}/isp/tampered" "${pkg}/sw/bootloader/build/${version}/isp/tampered"
run "${test}/run_vp.sh" isp "${tmp}/elsewhere/my" "raw/d65" "frame.raw" --dump-demosaic > "${tmp}/dms_user_raw.log" 2>&1
rc=$?
user_dms="${pkg}/sw/bootloader/build/${version}/isp/raw_d65_frame/demosaic.rgb16"
[[ ${rc} -eq 0 ]] && grep -q "DEMOSAIC DONE" "${tmp}/dms_user_raw.log" && [[ -s "${user_dms}" ]] \
    && [[ "$(sha256sum "${user_dms}" | cut -d' ' -f1)" == "$(cat "${test}/isp/d65/expected_demosaic.sha256")" ]] \
    && pass "isp FILE.raw --dump-demosaic: d65's frame gives d65's capture, not compared" \
    || { tail -n 15 "${tmp}/dms_user_raw.log"; bad "isp FILE.raw --dump-demosaic gave exit ${rc}"; }
# A host write error during the capture (8 MiB file size limit, SIGXFSZ
# ignored: the 23 MiB RGB16 cannot be written, the 4 MiB NV12 dumps can).
(trap '' XFSZ; ulimit -f 8192; run "${test}/run_vp.sh" isp d65 --dump-demosaic) > "${tmp}/dms_io.log" 2>&1
rc=$?
[[ ${rc} -eq 1 ]] && grep -q "Demosaic capture failed (host I/O)" "${tmp}/dms_io.log" \
    && pass "a host write error of the capture makes the run FAIL (fx1_soc exit 5)" \
    || { tail -n 15 "${tmp}/dms_io.log"; bad "capture write error gave exit ${rc}"; }
# The launcher's Python helpers write no bytecode into the shipped tree.
pyc="$(find "${pkg}/vp" "${pkg}/sw" -name __pycache__ ! -path "*/sw/bootloader/build/*" | head -n 3)"
[[ -z "${pyc}" ]] && pass "no __pycache__ in the shipped tree after the ISP runs" || bad "__pycache__ in the package: ${pyc}"
rm -rf "${tmp}/elsewhere/my raw" "${tmp}/elsewhere/short.raw" "${pkg}/sw/bootloader/build/${version}/isp/raw_"*

step "4b. csrw_to_bin.sh rebuilds every shipped profile.bin from its profile.csrw"
for csrw in "${test}"/profiles/*/profile.csrw; do
    d="${tmp}/csrw/$(basename "$(dirname "${csrw}")")"
    mkdir -p "${d}"
    cp "${csrw}" "${d}/profile.csrw"
    if run "${test}/csrw_to_bin.sh" "${d}/profile.csrw" > "${d}/log" 2>&1 \
            && cmp -s "${d}/profile.bin" "$(dirname "${csrw}")/profile.bin"; then
        pass "csrw_to_bin: $(basename "$(dirname "${csrw}")") profile.bin identical"
    else
        cat "${d}/log"
        bad "csrw_to_bin: $(basename "$(dirname "${csrw}")") profile.bin differs or the script failed"
    fi
done

step "4c. a tuned shared profile with another write count: the job follows profile.bin (ISP-UPD-010)"
sp="${test}/profiles/shared_bggr_2688x1520_full"
d65_out="${pkg}/sw/bootloader/build/${version}/isp/d65"
cp -a "${sp}" "${tmp}/shared_shipped"
python3 - "${sp}/profile.csrw" <<'PY'
import sys
p = sys.argv[1]
t = open(p).read()
for a, b in (("TUNE_NR_CTRL MODE=0 MEDIAN_EN=1\n", "TUNE_NR_CTRL MODE=2 MEDIAN_EN=1\n"),
             ("TUNE_BPC_CTRL COLOR_THRESH_EN=0 FACTORY_EN=0\n", "TUNE_BPC_CTRL COLOR_THRESH_EN=0 FACTORY_EN=1\n"),
             ("TUNE_BPC_COUNT VALUE=0\n", "TUNE_BPC_COUNT VALUE=2\nTUNE_BPC_ADDR VALUE=0\nTUNE_BPC_DATA X=100 Y=20\n"
              "TUNE_BPC_ADDR VALUE=1\nTUNE_BPC_DATA X=7 Y=21\n"),
             ("EE_FEATURE_EN activity_en=0 contrast_en=0 radial_en=0\n",
              "EE_FEATURE_EN activity_en=0 contrast_en=0 radial_en=1\n"),
             ("EE_RADIAL_GAIN_23 gain2=0x8000 gain3=0x8000\n", "EE_RADIAL_GAIN_23 gain2=0xA000 gain3=0xC000\n"),
             ("GTM_CTRL en=1 manual=0\n", "GTM_CTRL en=1 manual=1\n")):
    assert t.count(a) == 1, a
    t = t.replace(a, b)
open(p, "w").write(t)
PY
run "${test}/csrw_to_bin.sh" "${sp}/profile.csrw" > "${tmp}/tuned_c2b.log" 2>&1
n=$(( $(stat -c %s "${sp}/profile.bin") / 8 ))
run "${test}/run_vp.sh" isp d65 > "${tmp}/isp_tuned.log" 2>&1
rc=$?
"${tmp}/ref_run_raw" --input "${test}/isp/d65/input.isp16" --width 2688 --height 1520 --frames 2 --profile-mode \
    --profile "${sp}/profile.csrw" --out "${tmp}/tuned_ref" > "${tmp}/tuned_ref.log" 2>&1
ref="$(sha256sum "${tmp}/tuned_ref.nv12" 2>/dev/null | cut -d' ' -f1)"
got="$(sha256sum "${d65_out}/platform.nv12" 2>/dev/null | cut -d' ' -f1)"
[[ ${rc} -eq 1 ]] && grep -q "ISP FAIL: NV12 differs" "${tmp}/isp_tuned.log" && grep -q "the job uses ${n}" "${tmp}/isp_tuned.log" \
    && [[ -n "${ref}" && "${got}" == "${ref}" && "${ref}" != "$(cat "${test}/isp/d65/expected_nv12.sha256")" ]] \
    && [[ -s "${d65_out}/platform.png" ]] \
    && pass "tuned profile, 4 more writes (NR curve MODE 2, 2 factory pixels, radial EE, GTM manual): job uses ${n}, NV12 = the model's ${ref:0:12}... (not the golden), PNG written" \
    || { tail -n 15 "${tmp}/isp_tuned.log" "${tmp}/tuned_ref.log"; bad "tuned profile with more writes: exit ${rc}, NV12 ${got:0:12} vs model ${ref:0:12}"; }
python3 - "${tmp}/shared_shipped/profile.csrw" "${sp}/profile.csrw" <<'PY'
import sys
t = open(sys.argv[1]).read().splitlines(True)
assert sum(l.startswith("TUNE_") for l in t) > 40
open(sys.argv[2], "w").write("".join(l for l in t if not l.startswith("TUNE_")))
PY
run "${test}/csrw_to_bin.sh" "${sp}/profile.csrw" > "${tmp}/untuned_c2b.log" 2>&1
m=$(( $(stat -c %s "${sp}/profile.bin") / 8 ))
run "${test}/run_vp.sh" isp d65 > "${tmp}/isp_untuned.log" 2>&1 && grep -q "ISP PASS" "${tmp}/isp_untuned.log" \
    && grep -q "the job uses ${m}" "${tmp}/isp_untuned.log" \
    && pass "profile without its TUNE_* writes (${m} writes, fewer): job uses ${m}, NV12 = golden" \
    || { tail -n 15 "${tmp}/isp_untuned.log"; bad "profile with fewer writes"; }
rm -rf "${sp}" "${d65_out}"
cp -a "${tmp}/shared_shipped" "${sp}"
cmp -s "${sp}/profile.bin" "${tmp}/shared_shipped/profile.bin" || bad "the shipped shared profile was not restored"

step "4d. ISP RAW sequence sample (run_vp.sh isp-stream-sample)"
if command -v ffmpeg > /dev/null && command -v ffprobe > /dev/null; then
    sdir="${tmp}/elsewhere/stream out"
    mkdir -p "${sdir}"
    nseq=$(grep -c -v '^#' "${test}/isp_stream/expected_frames.txt")
    run "${test}/run_vp.sh" isp-stream-sample --out "${sdir}/sample" > "${tmp}/stream_sample.log" 2>&1
    rc=$?
    st="$(python3 -c 'import json,sys; s=json.load(open(sys.argv[1])); print(s["status"], s["captured"], s.get("psnr_min_yuv"))' \
        "${sdir}/sample/summary.json" 2>/dev/null)"
    [[ ${rc} -eq 0 ]] && grep -q "SEQUENCE PASS: ${nseq} frame(s)" "${tmp}/stream_sample.log" \
        && [[ "${st}" == "COMPLETE ${nseq} "* && -s "${sdir}/sample/output.mp4" ]] \
        && pass "isp-stream-sample: COMPLETE, ${nseq} frames = expected_frames.txt, video verified (Y/U/V ${st##* })" \
        || { tail -n 15 "${tmp}/stream_sample.log"; bad "isp-stream-sample (exit ${rc}, ${st})"; }
    cp "${test}/isp_stream/expected_frames.txt" "${tmp}/expected_frames.txt"
    sed -i '0,/^1 /s/^\(1 [a-z0-9_]* [0-9]\) ..../\1 0000/' "${test}/isp_stream/expected_frames.txt"
    run "${test}/run_vp.sh" isp-stream-sample --out "${sdir}/wrong" > "${tmp}/stream_wrong.log" 2>&1
    rc=$?
    [[ ${rc} -eq 1 ]] && grep -q "SEQUENCE FAIL: frame(s) \[1\]" "${tmp}/stream_wrong.log" \
        && pass "a wrong expected frame (1) FAILs (not a rubber stamp)" \
        || { tail -n 5 "${tmp}/stream_wrong.log"; bad "a wrong expected frame was not caught (exit ${rc})"; }
    cp "${tmp}/expected_frames.txt" "${test}/isp_stream/expected_frames.txt"
    cp "${test}/profiles/shared_bggr_2688x1520_full/profile.csrw" "${sdir}/tuned profile.csrw"
    echo "CSC_CTRL std=1" >> "${sdir}/tuned profile.csrw"
    run "${test}/run_vp.sh" isp-stream-sample --out "${sdir}/tuned" --profile "${sdir}/tuned profile.csrw" \
        > "${tmp}/stream_tuned.log" 2>&1
    rc=$?
    [[ ${rc} -eq 0 ]] && grep -q "SEQUENCE NOT COMPARED: the session's profile" "${tmp}/stream_tuned.log" \
        && grep -q "colorimetry *full range, BT.709" "${sdir}/tuned/summary.txt" && [[ -s "${sdir}/tuned/output.mp4" ]] \
        && pass "a tuned copy of the profile (CSC_CTRL std=1): BT.709 video, no golden comparison" \
        || { tail -n 5 "${tmp}/stream_tuned.log"; bad "tuned profile run (exit ${rc})"; }
    # The tuning flow in place: the shipped shared profile itself edited (S4 review R1).
    shp="${test}/profiles/shared_bggr_2688x1520_full/profile.csrw"
    cp "${shp}" "${tmp}/shared_profile.csrw"
    echo "CSC_CTRL std=1" >> "${shp}"
    run "${test}/run_vp.sh" isp-stream-sample --out "${sdir}/inplace" --frames 2 > "${tmp}/stream_inplace.log" 2>&1
    rc=$?
    cp "${tmp}/shared_profile.csrw" "${shp}"
    [[ ${rc} -eq 0 ]] && grep -q "SEQUENCE NOT COMPARED: the session's profile" "${tmp}/stream_inplace.log" \
        && grep -q "colorimetry *full range, BT.709" "${sdir}/inplace/summary.txt" && [[ -s "${sdir}/inplace/output.mp4" ]] \
        && pass "the shared profile edited in place: BT.709 video, exit 0, not compared with the shipped frames" \
        || { tail -n 5 "${tmp}/stream_inplace.log"; bad "in-place tuned profile (exit ${rc})"; }
    (cd "${test}" && sha256sum -c --quiet isp_stream/profile.sha256) || bad "the shared profile was not restored"
    # An unchanged copy elsewhere (--profile=FILE form) is still the shipped profile: compared.
    cp "${shp}" "${sdir}/same profile.csrw"
    run "${test}/run_vp.sh" isp-stream-sample --out "${sdir}/same" --frames 2 --profile="${sdir}/same profile.csrw" \
        > "${tmp}/stream_same.log" 2>&1
    rc=$?
    [[ ${rc} -eq 0 ]] && grep -q "SEQUENCE PASS: 2 frame(s)" "${tmp}/stream_same.log" \
        && pass "an unchanged copy of the profile at another path (--profile=FILE): compared, SEQUENCE PASS" \
        || { tail -n 5 "${tmp}/stream_same.log"; bad "unchanged profile copy (exit ${rc})"; }
    # No FFmpeg on PATH: every other tool of /usr/bin, ffmpeg and ffprobe left out.
    nob="${tmp}/no_ffmpeg_bin"
    mkdir -p "${nob}"
    for f in /usr/bin/*; do
        case "$(basename "${f}")" in ffmpeg|ffprobe) ;; *) ln -s "${f}" "${nob}/" ;; esac
    done
    env -i PATH="${nob}" HOME="${tmp}/elsewhere" "${test}/run_vp.sh" isp-stream-sample --out "${sdir}/noff" \
        > "${tmp}/stream_noff.log" 2>&1
    rc=$?
    [[ ${rc} -eq 5 ]] && grep -q "PREFLIGHT_FAILED" "${tmp}/stream_noff.log" && grep -q "dnf install -y ffmpeg" "${tmp}/stream_noff.log" \
        && [[ ! -e "${sdir}/noff" ]] \
        && pass "no ffmpeg on PATH: PREFLIGHT_FAILED (exit 5) before the boot, install steps printed, DIR not made" \
        || { tail -n 8 "${tmp}/stream_noff.log"; bad "the run without ffmpeg (exit ${rc})"; }
    rm -rf "${sdir}"
else
    bad "ffmpeg/ffprobe (with libx264) are needed for the isp-stream checks (package/docs/README.md)"
fi

step "4e. NPU: self-test, every fixture, knobs, negative controls, console"
run "${test}/run_vp.sh" npu-selftest > "${tmp}/npu_selftest.log" 2>&1 && grep -q "NPU SELFTEST PASS" "${tmp}/npu_selftest.log" \
    && pass "npu-selftest: $(grep -c 'npu selftest: .* 0 of' "${tmp}/npu_selftest.log") cases equal the C oracle, PASS" \
    || { tail -n 15 "${tmp}/npu_selftest.log"; bad "run_vp.sh npu-selftest"; }
nfx="$(ls -d "${test}"/npu/*/ | wc -l)"
run "${test}/run_vp.sh" npu all > "${tmp}/npu_all.log" 2>&1 && grep -q "NPU FIXTURES PASS (${nfx}/${nfx}" "${tmp}/npu_all.log" \
    && pass "npu all: ${nfx}/${nfx} fixtures as declared ($(grep -c 'PASS: as declared (OK)' "${tmp}/npu_all.log") OK, $(grep -c 'PASS: as declared ([A-Z_]*)' "${tmp}/npu_all.log") total)" \
    || { cat "${tmp}/npu_all.log"; bad "run_vp.sh npu all"; }
run HAS_DUMMY_KNOB=1 SAURIA_CORE_RTL_PROFILE=legacy NPU_AXI_DRAM_LAT=500 "${test}/run_vp.sh" npu chain --out "${tmp}/npu_knobs" \
    > "${tmp}/npu_knobs.log" 2>&1
rc=$?
[[ ${rc} -eq 0 ]] && grep -q "NPU model knobs removed from the environment: .*SAURIA_CORE_RTL_PROFILE" "${tmp}/npu_knobs.log" \
    && python3 -c 'import json,sys;a=json.load(open(sys.argv[1]));b=json.load(open(sys.argv[2]));sys.exit(0 if a["model_config"]["env"]=="none" and [o["sha256"] for o in a["outputs"]]==[o["sha256"] for o in b["outputs"]] else 1)' \
        "${tmp}/npu_knobs/summary.json" "${pkg}/sw/bootloader/build/${version}/npu/chain/summary.json" \
    && pass "inherited model knobs removed and named; outputs as the clean run (env=none)" \
    || { tail -n 8 "${tmp}/npu_knobs.log"; bad "npu chain with inherited knobs (exit ${rc})"; }
cp -a "${test}/npu/chain" "${tmp}/npu_tampered"
python3 -c 'import sys;p=sys.argv[1];b=bytearray(open(p,"rb").read());b[0]^=1;open(p,"wb").write(b)' "${tmp}/npu_tampered/golden_10.bin"
run "${test}/run_vp.sh" npu "${tmp}/npu_tampered" --out "${tmp}/npu_tampered_out" > "${tmp}/npu_tampered.log" 2>&1
rc=$?
[[ ${rc} -eq 1 ]] && grep -q "NPU chain FAIL" "${tmp}/npu_tampered.log" && grep -q "1 differ from the golden" "${tmp}/npu_tampered.log" \
    && pass "a wrong NPU golden FAILs (exit 1, 1 byte differs)" || { tail -n 8 "${tmp}/npu_tampered.log"; bad "tampered NPU golden gave exit ${rc}"; }
run "${test}/run_vp.sh" npu no_such_job > "${tmp}/npu_unknown.log" 2>&1
rc=$?
[[ ${rc} -eq 2 ]] && grep -q "no fixture 'no_such_job'" "${tmp}/npu_unknown.log" && pass "an unknown fixture is refused (exit 2)" \
    || bad "unknown fixture gave exit ${rc}"
run "${test}/run_vp.sh" npu chain --out "${tmp}/npu_print" --print-command > "${tmp}/npu_print.log" 2>&1 \
    && grep -q "^env -i .*fx1_soc --fw .*--load 0x80650000:" "${tmp}/npu_print.log" && [[ ! -e "${tmp}/npu_print/uart.log" ]] \
    && pass "--print-command: the clean fx1_soc command, nothing run" || bad "npu --print-command"
run "${test}/run_vp.sh" npu all --print-command --out "${tmp}/npu_print_all" > "${tmp}/npu_print_all.log" 2>&1
rc=$?
[[ ${rc} -eq 2 ]] && ! grep -q "FIXTURES PASS" "${tmp}/npu_print_all.log" && grep -q "no verdict" "${tmp}/npu_print_all.log" \
    && [[ -z "$(find "${tmp}/npu_print_all" -name result.bin 2>/dev/null)" ]] \
    && pass "npu all --print-command: refused (exit 2), no verdict, nothing run" \
    || { cat "${tmp}/npu_print_all.log"; bad "npu all --print-command gave exit ${rc}"; }
# The console flow with the client that needs nothing installed (bash /dev/tcp).
port=$((20000 + RANDOM % 20000))
run "${test}/run_vp.sh" npu long interactive "${port}" > "${tmp}/npu_console_vp.log" 2>&1 &
vp_pid=$!
for _ in $(seq 50); do grep -q "waits for the UART console" "${tmp}/npu_console_vp.log" 2>/dev/null && break; sleep 0.2; done
sleep 1
(sleep 3; printf 'npu run\r'; sleep 1; printf 'npu reset\r'; sleep 4; printf 'npu run\r'; sleep 2; printf 'npu wait\r'; sleep 8
 printf 'poweroff\r'; sleep 6) \
    | timeout 90 env -i PATH=/usr/bin:/bin FX1_CONSOLE_CLIENT=bash "${test}/fx1_console.sh" "${port}" > "${tmp}/npu_console.log" 2>&1
# The VP ends with poweroff; one that does not (a failed console) is stopped by its PID.
for _ in $(seq 150); do kill -0 "${vp_pid}" 2>/dev/null || break; sleep 0.2; done
kill "${vp_pid}" 2>/dev/null && bad "the interactive NPU VP did not end with poweroff (stopped)"
wait "${vp_pid}"
rc=$?
tr -d '\r' < "${tmp}/npu_console.log" | grep -q "npu reset: recovery OK, soft reset OK, open OK" \
    && tr -d '\r' < "${tmp}/npu_console.log" | grep -q "npu run: long: OK, 8/8 ops" \
    && tr -d '\r' < "${tmp}/npu_console.log" | grep -q "npu run: PASS" && [[ ${rc} -eq 0 ]] \
    && pass "npu long interactive: npu run, npu reset ($(tr -d '\r' < "${tmp}/npu_console.log" | grep -o 'job long stopped: ABORTED at op [0-9]*' || echo 'idle')), the job again PASS, poweroff (bash client)" \
    || { tr -d '\r' < "${tmp}/npu_console.log" | tail -n 8; bad "npu interactive flow (VP exit ${rc})"; }
rm -rf "${pkg}/sw/bootloader/build/${version}/npu"

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

step "5c2. camera CLI sessions, memory and direct path (plan DIRECT_CSI_ISP D5); a wrong frame must FAIL"
# The scripted sessions as shipped. The exit status is the firmware's verdict
# (every frame against the golden); the counts also make sure the direct
# session really ran on the direct path (6 direct frames: no RAW in DDR, then
# 2 zero-copy frames back on the memory path) and the memory one on memory.
cam_count() { tr -d '\r' < "$1" | grep -a -c -E "$2" || true; }
golden_crc=' crc 0x[0-9a-f]+ = golden frame [0-9]+$'
run "${test}/run_vp.sh" camera-cli > "${tmp}/camera_cli.log" 2>&1; rc=$?
log="${pkg}/sw/bootloader/build/${version}/camera-cli.log"
n_mem="$(cam_count "${log}" "^  capture [0-9]+ FRAME_ID [0-9]+ side [01] .*raw ok${golden_crc}")"
n_dir="$(cam_count "${log}" " direct out ")"
[[ ${rc} -eq 0 && ${n_mem} -eq 6 && ${n_dir} -eq 0 ]] && grep -q "CAMERA CLI PASS" "${tmp}/camera_cli.log" \
    && pass "run_vp.sh camera-cli: PASS, 6 memory-path frames = golden" \
    || { cat "${tmp}/camera_cli.log"; bad "run_vp.sh camera-cli (exit ${rc}, ${n_mem} golden memory frames, ${n_dir} direct)"; }
run "${test}/run_vp.sh" camera-direct-cli > "${tmp}/camera_direct_cli.log" 2>&1; rc=$?
log="${pkg}/sw/bootloader/build/${version}/camera-direct-cli.log"
n_dir="$(cam_count "${log}" "^  capture [0-9]+ FRAME_ID [0-9]+ epoch [0-9]+ isp frame [1-9][0-9]* direct out [0-3] 64x32 raw n/a${golden_crc}")"
n_mem="$(cam_count "${log}" "^  capture [0-9]+ FRAME_ID [0-9]+ side [01] .* zero-copy .*raw ok${golden_crc}")"
n_ok="$(cam_count "${log}" "^  direct ok [1-9][0-9]*, reset 0, csi_dropped 0, csi_missing_fe 0, invalid 0, isp_failed 0")"
[[ ${rc} -eq 0 && ${n_dir} -eq 6 && ${n_mem} -eq 2 && ${n_ok} -eq 1 ]] && grep -q "CAMERA CLI PASS" "${tmp}/camera_direct_cli.log" \
    && pass "run_vp.sh camera-direct-cli: PASS, 6 direct frames + 2 memory-path frames = golden" \
    || { cat "${tmp}/camera_direct_cli.log"; bad "run_vp.sh camera-direct-cli (exit ${rc}, ${n_dir} direct / ${n_mem} memory golden frames, status ${n_ok})"; }
# Negative: the direct path fed from a RAW file (64x32 RAW12, every sample
# 0x123) instead of the sensor pattern: the NV12 must miss the golden and the
# session FAIL (the verdict is not a rubber stamp).
cdir="${tmp}/camera_neg"; mkdir -p "${cdir}"
printf 'camera probe\ncamera path direct\ncamera mode 64 32 12 4\ncamera start\ncamera capture 2\ncamera stop\npoweroff\n' \
    > "${cdir}/cli_direct_bad.txt"
/usr/bin/python3 -c 'import sys; open(sys.argv[1], "wb").write(bytes([0x23, 0x01]) * (64 * 32))' "${cdir}/flat.raw"
run "${pkg}/vp/bin/${version}/fx1_soc" --fw "${pkg}/sw/bootloader/build/${version}/freertos_cli.elf" --camera \
    --camera-raw "${cdir}/flat.raw" --timeout-ms 3000 --uart0-rx-file "${cdir}/cli_direct_bad.txt" \
    --uart0-rx-delay-us 60000 --uart0-rx-byte-us 87 > "${cdir}/run.log" 2>&1; rc=$?
n_bad="$(cam_count "${cdir}/run.log" " direct out [0-3] 64x32 raw n/a crc 0x[0-9a-f]+ GOLDEN MISMATCH")"
[[ ${rc} -ne 0 && ${n_bad} -ge 1 ]] && grep -a -q "RESULT FAIL" "${cdir}/run.log" \
    && pass "direct CLI from a flat RAW file: FAIL as required (exit ${rc}, ${n_bad} golden mismatch(es))" \
    || { tail -n 30 "${cdir}/run.log"; bad "direct CLI from a flat RAW file was not refused (exit ${rc}, ${n_bad} mismatches)"; }
# The console flow with the camera (run_vp.sh interactive PORT --camera), with
# the client that needs nothing installed (bash /dev/tcp): on the direct path
# two frames must equal the golden and the direct counters stay clean; the same
# console without --camera must find no camera; --camera-raw must reach the VP;
# bad arguments are refused before a VP starts. Each command waits for the
# CLI's answer to the previous one (a capture takes host seconds: detailed D-PHY).
cam_open() {   # cam_open STEM [interactive arguments after the port]
    local stem="$1"; shift
    cam_port=$((20000 + RANDOM % 20000))
    cam_log="${tmp}/${stem}_console.log" cam_vplog="${tmp}/${stem}_vp.log"
    rm -f "${tmp}/${stem}.in"; mkfifo "${tmp}/${stem}.in"
    run "${test}/run_vp.sh" interactive "${cam_port}" "$@" > "${cam_vplog}" 2>&1 &
    cam_vp=$!
    for _ in $(seq 50); do grep -q "waits for the UART console" "${cam_vplog}" 2>/dev/null && break; sleep 0.2; done
    sleep 1
    # Read-write open: never blocks, and a client that ends early gives no SIGPIPE here.
    exec {cam_fd}<> "${tmp}/${stem}.in"
    timeout 900 env -i PATH=/usr/bin:/bin FX1_CONSOLE_CLIENT=bash "${test}/fx1_console.sh" "${cam_port}" \
        < "${tmp}/${stem}.in" > "${cam_log}" 2>&1 &
    cam_client=$!
}
cam_wait() {   # cam_wait REGEX SECONDS: the console log has a line matching REGEX
    local i
    for ((i = 0; i < $2 * 5; i++)); do
        tr -d '\r' 2>/dev/null < "${cam_log}" | grep -a -q -E "$1" && return 0
        sleep 0.2
    done
    return 1
}
cam_say() { printf '%s\r' "$1" >&"${cam_fd}"; cam_wait "$2" "$3"; }   # cam_say COMMAND REGEX SECONDS
cam_close() {  # poweroff; the VP must end by itself; sets cam_rc (VP exit status)
    printf 'poweroff\r' >&"${cam_fd}"
    for _ in $(seq 300); do kill -0 "${cam_vp}" 2>/dev/null || break; sleep 0.2; done
    kill "${cam_vp}" 2>/dev/null && bad "an interactive camera VP did not end with poweroff (stopped)"
    wait "${cam_vp}"; cam_rc=$?
    exec {cam_fd}>&-
    wait "${cam_client}" 2>/dev/null || true
}
prompt='root@fx1:~# '
cam_open cam_ia --camera
cam_wait "${prompt}" 120 \
    && cam_say "camera probe" "camera probe: PASS" 60 \
    && cam_say "camera path direct" "golden yes, path direct" 30 \
    && cam_say "camera start" "camera start: (PASS|FAIL)" 120 \
    && cam_say "camera capture 2" "camera capture: (PASS|FAIL)" 600 \
    && cam_say "camera status" "^  direct ok " 60 \
    && cam_say "camera stop" "^camera stop: " 60
cam_close
n_dir="$(cam_count "${cam_log}" "^  capture [0-9]+ FRAME_ID [0-9]+ epoch [0-9]+ isp frame [1-9][0-9]* direct out [0-3] 64x32 raw n/a${golden_crc}")"
n_ok="$(cam_count "${cam_log}" "^  direct ok [1-9][0-9]*, reset 0, csi_dropped 0, csi_missing_fe 0, invalid 0, isp_failed 0")"
[[ ${cam_rc} -eq 0 && ${n_dir} -eq 2 && ${n_ok} -eq 1 ]] && grep -q "^Camera: --camera " "${cam_vplog}" \
    && tr -d '\r' < "${cam_log}" | grep -a -q "^camera stop: OK" \
    && tr -d '\r' < "${cam_log}" | grep -a -q "^poweroff: 0 failed check(s)" \
    && pass "interactive --camera: console on the direct path, 2 frames = golden, direct counters clean, poweroff (bash client)" \
    || { tr -d '\r' < "${cam_log}" | tail -n 25; cat "${cam_vplog}"; bad "interactive --camera flow (VP exit ${cam_rc}, ${n_dir} golden direct frames, status ${n_ok})"; }
cam_open cam_ia_none
cam_wait "${prompt}" 120 && cam_say "camera probe" "^camera: absent" 60
cam_close
[[ ${cam_rc} -eq 0 ]] && ! grep -q "^Camera:" "${cam_vplog}" \
    && tr -d '\r' < "${cam_log}" | grep -a -q "^camera: absent (no CSI-2 RX: fx1_soc runs without --camera)" \
    && pass "interactive without --camera: camera probe finds no camera" \
    || { tr -d '\r' < "${cam_log}" | tail -n 10; bad "interactive without --camera (VP exit ${cam_rc})"; }
cam_open cam_ia_raw --camera-raw "${cdir}/flat.raw"
cam_wait "${prompt}" 120 && cam_say "camera probe" "camera probe: (PASS|FAIL)" 60
cam_close
[[ ${cam_rc} -eq 0 ]] && grep -a -q "^\[fx1_soc\] camera: .*frames from ${cdir}/flat.raw" "${cam_vplog}" \
    && tr -d '\r' < "${cam_log}" | grep -a -q "camera probe: PASS" \
    && pass "interactive --camera-raw FILE: the VP's sensor takes its frames from FILE (implies --camera)" \
    || { cat "${cam_vplog}"; bad "interactive --camera-raw (VP exit ${cam_rc})"; }
run "${test}/run_vp.sh" interactive 5000 --camera-raw "${cdir}/no_such.raw" > "${tmp}/cam_ia_bad1.log" 2>&1; rc1=$?
run "${test}/run_vp.sh" interactive --cam > "${tmp}/cam_ia_bad2.log" 2>&1; rc2=$?
[[ ${rc1} -eq 2 && ${rc2} -eq 2 ]] && grep -q "no such file" "${tmp}/cam_ia_bad1.log" && grep -q "unknown argument '--cam'" "${tmp}/cam_ia_bad2.log" \
    && pass "interactive: a missing --camera-raw file and an unknown argument are refused (exit 2)" \
    || bad "interactive argument refusal (exit ${rc1} / ${rc2})"
rm -f "${pkg}/sw/bootloader/build/${version}/camera-cli.log" "${pkg}/sw/bootloader/build/${version}/camera-direct-cli.log"

step "5d. Ethernet example, built from the shipped sources, on every line peer; scripted eth CLI session"
if run make -s -C "${pkg}/sw/bootloader/sources/${version}" TOOLDIR="${tooldir}" eth > "${tmp}/make_eth.log" 2>&1; then
    run "${test}/run_vp.sh" eth > "${tmp}/eth.log" 2>&1 && grep -q "ETH EXAMPLES PASS" "${tmp}/eth.log" \
        && pass "run_vp.sh eth: $(grep -c '  PASS ' "${tmp}/eth.log") line peers PASS" \
        || { cat "${tmp}/eth.log"; bad "run_vp.sh eth"; }
    rm -rf "${pkg}/sw/bootloader/build/${version}/eth"
else
    cat "${tmp}/make_eth.log"
    bad "make eth with the shipped Makefile failed"
fi
run "${test}/run_vp.sh" eth-cli > "${tmp}/eth_cli.log" 2>&1 && grep -q "ETH CLI PASS" "${tmp}/eth_cli.log" \
    && pass "run_vp.sh eth-cli PASS" || { cat "${tmp}/eth_cli.log"; bad "run_vp.sh eth-cli"; }
rm -f "${pkg}/sw/bootloader/build/${version}/eth-cli.log"

step "5e. NPU job fixtures regenerated from the shipped sources (make npu-jobs, host compiler)"
gen="${pkg}/sw/bootloader/build/${version}/npu_jobs"
if run make -s -C "${pkg}/sw/bootloader/sources/${version}" npu-jobs > "${tmp}/make_npu_jobs.log" 2>&1; then
    diff -r "${gen}" "${test}/npu" -x FIXTURE.txt -x .npu_job_gen > "${tmp}/npu_jobs_diff.log" 2>&1 \
        && pass "make npu-jobs: $(ls -d "${gen}"/*/ | wc -l) fixtures, byte for byte the shipped ones" \
        || { head -n 10 "${tmp}/npu_jobs_diff.log"; bad "make npu-jobs differs from test/npu/"; }
    # Review N5 R1: a second run keeps what is not the generator's.
    echo "my notes" > "${gen}/my_notes.txt"
    echo "my tuning" > "${gen}/chain/tuning_notes.txt"
    mkdir -p "${gen}/my_job" && echo "my job" > "${gen}/my_job/job.bin"
    run make -s -C "${pkg}/sw/bootloader/sources/${version}" npu-jobs > "${tmp}/make_npu_jobs2.log" 2>&1 \
        && [[ "$(cat "${gen}/my_notes.txt" "${gen}/chain/tuning_notes.txt" "${gen}/my_job/job.bin")" == "my notes"$'\n'"my tuning"$'\n'"my job" ]] \
        && diff -r "${gen}" "${test}/npu" -x FIXTURE.txt -x .npu_job_gen -x my_notes.txt -x tuning_notes.txt -x my_job > /dev/null 2>&1 \
        && pass "make npu-jobs again: user notes and a user fixture kept, the fixtures byte for byte" \
        || { cat "${tmp}/make_npu_jobs2.log"; bad "make npu-jobs into its own directory with user files"; }
    foreign="${tmp}/npu_foreign"
    mkdir -p "${foreign}/chain" && echo "user's" > "${foreign}/user_notes.txt" && echo "user's" > "${foreign}/chain/job.bin"
    run make -s -C "${pkg}/sw/bootloader/sources/${version}" npu-jobs NPUJOBS="${foreign}" > "${tmp}/make_npu_foreign.log" 2>&1
    rc=$?
    [[ ${rc} -ne 0 ]] && grep -q "not made by npu_job_gen" "${tmp}/make_npu_foreign.log" \
        && [[ "$(cat "${foreign}/user_notes.txt" "${foreign}/chain/job.bin")" == "user's"$'\n'"user's" ]] \
        && [[ "$(find "${foreign}" -type f | wc -l)" -eq 2 ]] \
        && pass "make npu-jobs NPUJOBS=<a directory it did not make>: refused (make exit ${rc}), its files untouched" \
        || { cat "${tmp}/make_npu_foreign.log"; bad "make npu-jobs into a foreign directory (exit ${rc})"; }
    # Review N5 R3: never written through a link to a file outside NPUJOBS.
    echo "user backup" > "${tmp}/npu_outside_job.bin"
    cp "${gen}/chain/job.bin" "${tmp}/npu_chain_job.keep"
    ln -sf "${tmp}/npu_outside_job.bin" "${gen}/chain/job.bin"
    run make -s -C "${pkg}/sw/bootloader/sources/${version}" npu-jobs > "${tmp}/make_npu_link.log" 2>&1
    rc=$?
    [[ ${rc} -ne 0 ]] && grep -q "not write through links" "${tmp}/make_npu_link.log" \
        && [[ "$(cat "${tmp}/npu_outside_job.bin")" == "user backup" && -L "${gen}/chain/job.bin" ]] \
        && pass "make npu-jobs with chain/job.bin a link outside NPUJOBS: refused (make exit ${rc}), the link target untouched" \
        || { cat "${tmp}/make_npu_link.log"; bad "make npu-jobs wrote through a link (exit ${rc})"; }
    rm -f "${gen}/chain/job.bin" && cp "${tmp}/npu_chain_job.keep" "${gen}/chain/job.bin"
    run "${test}/run_vp.sh" npu "${gen}/mixed" --out "${tmp}/npu_gen_mixed" > "${tmp}/npu_gen_mixed.log" 2>&1 \
        && grep -q "NPU mixed PASS" "${tmp}/npu_gen_mixed.log" && pass "a generated fixture runs (mixed PASS)" \
        || { tail -n 8 "${tmp}/npu_gen_mixed.log"; bad "run_vp.sh npu on a generated fixture"; }
else
    cat "${tmp}/make_npu_jobs.log"
    bad "make npu-jobs with the shipped Makefile failed"
fi
rm -rf "${gen}" "${pkg}/sw/bootloader/build/${version}/host"

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
    port=$((20000 + RANDOM % 20000))
    (sleep 4; printf 'isp run\r'; sleep 12; printf 'poweroff\r'; sleep 30) \
        | timeout 120 script -q -e -c "env -i PATH=/usr/bin:/bin HOME=${tmp}/elsewhere ${test}/run_vp.sh isp d65 interactive ${port} --dump-demosaic" \
            "${tmp}/interactive_dms.ts" > /dev/null 2>&1
    rc=$?
    idx="${pkg}/sw/bootloader/build/${version}/isp/d65/demosaic.d/index.jsonl"
    tr -d '\r' < "${tmp}/interactive_dms.ts" | grep -q "DEMOSAIC: per-frame captures" && [[ ${rc} -eq 0 ]] \
        && [[ "$(grep -c '"frame_completed"' "${idx}" 2>/dev/null)" -ge 2 ]] \
        && [[ ! -e "${pkg}/sw/bootloader/build/${version}/isp/d65/demosaic.rgb16" ]] \
        && pass "isp d65 interactive --dump-demosaic: per-frame captures, none selected" \
        || bad "interactive --dump-demosaic flow (exit ${rc}, ${tmp}/interactive_dms.ts)"
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
