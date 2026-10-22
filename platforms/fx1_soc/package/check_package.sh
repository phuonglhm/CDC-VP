#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Check a VP_FX1_V3.0 package tree (the --out of package_sdk.sh, a copy of it,
# or an SDK repository it was exported into; only the VP_FX1_V3.0 directories
# are looked at): contents, self-containment, licences, checksums, manifest.
# It does not run the VP; run_packaging_regression.sh does.
#
#   platforms/fx1_soc/package/check_package.sh PACKAGE_ROOT [--source CDC-VP]
#
# --source additionally rejects text files that name the CDC-VP tree (a
# package that only works next to the source is not a package).
# Exit status: 0 every check passed, 1 otherwise (all failures are listed).
set -uo pipefail
export PATH="/usr/bin:/bin:${PATH}"
version="VP_FX1_V3.0"
root="${1:?usage: check_package.sh PACKAGE_ROOT [--source DIR]}"
source_dir=""
[[ "${2:-}" == "--source" ]] && source_dir="${3:?--source DIR}"
root="$(cd "${root}" && pwd)" || exit 1
bin="vp/bin/${version}" doc="vp/doc/${version}" sdk="sw/bootloader/sources/${version}"
build="sw/bootloader/build/${version}" test="sw/bootloader/test/${version}"
failures=0
bad() { echo "  FAIL $*"; failures=$((failures + 1)); }
ok() { echo "  ok   $*"; }
cd "${root}" || exit 1

echo "check_package: ${root}"
required=(
    "${bin}/fx1_soc" "${bin}/libsystemc.so.2.3"
    "${doc}/README.md" "${doc}/MEMORY_MAP.md" "${doc}/BUILD_MANIFEST.json" "${doc}/SHA256SUMS"
    "${doc}/licenses/Apache-2.0.txt" "${doc}/licenses/CDC-VP-NOTICE.txt" "${doc}/licenses/THIRD_PARTY.md"
    "${doc}/licenses/RISCV-VP-PLUSPLUS.MIT.txt" "${doc}/licenses/BERKELEY-SOFTFLOAT-3d.BSD-3-Clause.txt"
    "${doc}/licenses/SYSTEMC.Apache-2.0.txt" "${doc}/licenses/FREERTOS-KERNEL.MIT.txt"
    "${sdk}/Makefile" "${sdk}/README.md" "${sdk}/app/main.c" "${sdk}/app/cli.c" "${sdk}/app/probe.S"
    "${sdk}/app/FreeRTOSConfig.h" "${sdk}/app/freertos_risc_v_chip_specific_extensions.h"
    "${sdk}/bsp/crt0.S" "${sdk}/bsp/fx1_fw.c" "${sdk}/bsp/fx1.ld.S" "${sdk}/include/fx1/fx1_memory_map.h"
    "${sdk}/include/dma/fx1_dma_regs.h" "${sdk}/include/fx1_isp/fx1_isp_csr.h" "${sdk}/include/fx1_isp/fx1_isp_drv.h"
    "${sdk}/include/fx1/fx1_ddr_sysctrl.h" "${sdk}/include/fx1_ddr3/fx1_ddr3_regs.h" "${sdk}/include/fx1_ddr3/provisional_encodings.h"
    "${sdk}/bsp/crt0_rom.S" "${sdk}/bsp/fx1_rom.ld.S" "${sdk}/ddr3/README.md" "${sdk}/ddr3/ddr3_fw.c" "${sdk}/ddr3/ddr3_fw.h"
    "${sdk}/ddr3/ddr3_probe.S" "${sdk}/ddr3/ddr3_ref_1600.h" "${sdk}/ddr3/examples/fw_init.c"
    "${sdk}/ddr3/examples/fault_recovery.c" "${sdk}/ddr3/examples/power_recovery.c" "${sdk}/ddr3/examples/boot_loader.c"
    "${doc}/ddr3/README.md" "${doc}/ddr3/programming_guide.md"
    "${sdk}/drivers/isp/fx1_isp_drv.c" "${sdk}/isp_selftest/isp_common.c" "${sdk}/demo/isp_demo_job.h"
    "${sdk}/include/fx1_csi2/fx1_csi2_drv.h" "${sdk}/include/fx1_csi2/fx1_csi2_regs.h" "${sdk}/drivers/csi2/fx1_csi2_drv.c"
    "${sdk}/drivers/camera/fx1_i2c.c" "${sdk}/drivers/camera/fx1_i2c.h" "${sdk}/drivers/camera/fx1_ccs_sensor.c"
    "${sdk}/drivers/camera/fx1_ccs_sensor.h" "${sdk}/drivers/camera/fx1_csi2_hal_mmio.c" "${sdk}/drivers/camera/fx1_csi2_hal_mmio.h"
    "${sdk}/camera/camera.c" "${sdk}/camera/camera.h" "${sdk}/camera/raw.c" "${sdk}/camera/recover.c"
    "${sdk}/camera/decerr.c" "${sdk}/camera/slverr.c" "${sdk}/camera/i2c_irq.c" "${sdk}/camera/i2c_timeout.c"
    "${sdk}/drivers/camera/fx1_camera.c" "${sdk}/drivers/camera/fx1_camera.h" "${sdk}/drivers/camera/fx1_isp_hal_mmio.c"
    "${sdk}/drivers/camera/fx1_isp_hal_mmio.h" "${sdk}/app/cmd_camera.c" "${sdk}/camera_isp/cam_isp.c" "${sdk}/camera_isp/cam_isp.h"
    "${sdk}/camera_isp/stream.c" "${sdk}/camera_isp/camera_isp_golden.h" "${test}/cli_camera.txt"
    "${doc}/camera/README.md"
    "${sdk}/third_party/FreeRTOS-Kernel/LICENSE.md" "${sdk}/third_party/FreeRTOS-Kernel/tasks.c"
    "${sdk}/third_party/FreeRTOS-Kernel/portable/GCC/RISC-V/portASM.S"
    "${build}/freertos_cli.elf" "${build}/freertos_cli.dis" "${build}/freertos_cli.map"
    "${test}/run_vp.sh" "${test}/fx1_console.sh" "${test}/isp_pack.py" "${test}/isp_auto.py" "${test}/cli_smoke.txt" "${test}/isp_run.txt"
)
for s in "${test}"/isp/*/; do
    for f in SAMPLE.txt job.env job.bin input.isp16 expected_nv12.sha256; do
        required+=("${s}${f}")
    done
    profile_id="$(sed -n 's/^PROFILE_ID=\([a-z0-9_][a-z0-9_]*\)$/\1/p' "${s}job.env" 2>/dev/null)"
    if [[ "${profile_id}" =~ ^[a-z0-9_]+$ ]]; then
        required+=("${test}/profiles/${profile_id}/profile.csrw" "${test}/profiles/${profile_id}/profile.bin")
    else
        bad "${s}job.env has no valid PROFILE_ID"
    fi
done
missing=0
for f in "${required[@]}"; do
    [[ -s "${f}" ]] || { bad "missing or empty: ${f}"; missing=$((missing + 1)); }
done
[[ -d "${test}/isp/d65" ]] || bad "default ISP sample d65 missing"
[[ ${missing} -eq 0 ]] && ok "${#required[@]} required files present"
for f in "${test}/run_vp.sh" "${test}/fx1_console.sh" "${test}/isp_pack.py" "${bin}/fx1_soc"; do
    [[ -x "${f}" ]] || bad "not executable: ${f}"
done

# ── self-contained executable ────────────────────────────────────────────────
rpath="$(readelf -d "${bin}/fx1_soc" | sed -n 's/.*(R[UN]*PATH).*\[\(.*\)\]/\1/p')"
[[ "${rpath}" == '$ORIGIN' ]] && ok "RPATH \$ORIGIN" || bad "RPATH is '${rpath}', expected \$ORIGIN"
resolved="$(env -i PATH=/usr/bin:/bin ldd "${bin}/fx1_soc" 2>&1)"
if grep -q "not found" <<< "${resolved}"; then
    bad "unresolved libraries: $(grep 'not found' <<< "${resolved}" | tr -s ' \t' ' ')"
elif grep -q "libsystemc.so.2.3 => ${root}/${bin}/libsystemc.so.2.3" <<< "${resolved}"; then
    ok "libsystemc resolves beside the binary with LD_LIBRARY_PATH unset"
else
    bad "libsystemc does not resolve from ${bin}: $(grep libsystemc <<< "${resolved}")"
fi
[[ -L "${bin}/libsystemc.so.2.3" ]] && bad "${bin}/libsystemc.so.2.3 is a symlink (copy the file)"

# ── licences: the text, not only the file ────────────────────────────────────
lic="${doc}/licenses"
grep -q "Apache License" "${lic}/Apache-2.0.txt" || bad "Apache-2.0.txt has no Apache licence text"
grep -q "Apache License" "${lic}/SYSTEMC.Apache-2.0.txt" || bad "SYSTEMC.Apache-2.0.txt has no Apache licence text"
grep -q "Permission is hereby granted" "${lic}/RISCV-VP-PLUSPLUS.MIT.txt" || bad "VP++ licence is not the MIT text"
grep -q "Permission is hereby granted" "${lic}/FREERTOS-KERNEL.MIT.txt" || bad "FreeRTOS licence is not the MIT text"
grep -q "Regents of the University of" "${lic}/BERKELEY-SOFTFLOAT-3d.BSD-3-Clause.txt" \
    || bad "SoftFloat notice lacks its copyright line"
ok "licence texts checked"
# The D-PHY RX model in the camera build: its record must be there; its
# provenance is open and reported, not a failure (user decision, CSI-2 M5b).
if grep -q "mipirxphy" "${lic}/THIRD_PARTY.md" && grep -q "provenance pending" "${lic}/THIRD_PARTY.md"; then
    echo "  note mipirxphy (MIPI D-PHY RX model in fx1_soc): provenance pending -- supplier, owner and original licence not confirmed (THIRD_PARTY.md)"
else
    bad "THIRD_PARTY.md has no mipirxphy record with its provenance status"
fi
"${bin}/fx1_soc" --version 2>/dev/null | grep -q '^  camera *CSI-2 RX' && ok "fx1_soc contains the camera (--camera)" \
    || bad "fx1_soc --version does not report the camera"

# ── checksums: every tracked file, nothing from build/ ───────────────────────
if sha256sum --quiet -c "${doc}/SHA256SUMS" > /dev/null 2>&1; then
    listed="$(wc -l < "${doc}/SHA256SUMS")"
    present="$(find vp sw -type f -path "*/${version}/*" ! -path "sw/bootloader/build/*" ! -name SHA256SUMS | wc -l)"
    if [[ "${listed}" -ne "${present}" ]]; then
        bad "SHA256SUMS lists ${listed} files, the tree has ${present} outside build/"
    elif grep -q " sw/bootloader/build/" "${doc}/SHA256SUMS"; then
        bad "SHA256SUMS lists build/ outputs, which the SDK repository does not track"
    else
        ok "SHA256SUMS verifies, ${listed} files"
    fi
else
    bad "sha256sum -c ${doc}/SHA256SUMS fails: $(sha256sum --quiet -c "${doc}/SHA256SUMS" 2>&1 | head -n 3 | tr '\n' ' ')"
fi

# ── manifest agrees with the files and the binary ────────────────────────────
python3 - "${root}" "${doc}/BUILD_MANIFEST.json" <<'EOF' || failures=$((failures + 1))
import hashlib, json, os, re, struct, subprocess, sys
root, path = sys.argv[1], sys.argv[2]
try:
    m = json.load(open(path))
except ValueError as e:
    print("  FAIL BUILD_MANIFEST.json is not JSON: %s" % e); sys.exit(1)
errors = []
for rel, a in m["artefacts"].items():
    p = os.path.join(root, rel)
    if not os.path.isfile(p):
        errors.append("manifest lists %s, not in the package" % rel)
    elif hashlib.sha256(open(p, "rb").read()).hexdigest() != a["sha256"]:
        errors.append("%s does not match its manifest SHA-256" % rel)
ver = subprocess.run([os.path.join(root, "vp/bin/VP_FX1_V3.0/fx1_soc"), "--version"], stdout=subprocess.PIPE,
                     stderr=subprocess.DEVNULL, text=True).stdout
if m["source"]["binary_revision"] not in ver:
    errors.append("manifest binary_revision is not what fx1_soc --version reports")
if m["build"]["vp_build_type"] != "Release":
    errors.append("VP build type is %s, not Release" % m["build"]["vp_build_type"])
for s in m["isp_samples"]:
    d = os.path.join(root, "sw/bootloader/test/VP_FX1_V3.0/isp", s["name"])
    if open(os.path.join(d, "expected_nv12.sha256")).read().strip() != s["expected_nv12_sha256"]:
        errors.append("sample %s: expected_nv12.sha256 differs from the manifest" % s["name"])
    if "expected_auto_nv12_sha256" in s:
        p = os.path.join(d, "expected_auto_nv12.sha256")
        if not os.path.isfile(p) or open(p).read().strip() != s["expected_auto_nv12_sha256"]:
            errors.append("sample %s: adaptive golden differs from the manifest" % s["name"])
        if s.get("auto_policy") != "neutral-v1" or s.get("auto_frames") != max(2,s.get("frames",1))+1:
            errors.append("sample %s: adaptive policy/frames differs from the contract" % s["name"])
    env = open(os.path.join(d, "job.env")).read()
    ids = re.findall(r"^PROFILE_ID=([a-z0-9_]+)$", env, re.M)
    pid = s.get("profile_id", "")
    if ids != [pid] or not re.fullmatch(r"[a-z0-9_]+", pid):
        errors.append("sample %s: PROFILE_ID differs from the manifest or is invalid" % s["name"])
        continue
    rel = "profiles/%s/profile.csrw" % pid
    if s.get("profile") != rel:
        errors.append("sample %s: profile path differs from its ID" % s["name"])
    pdir = os.path.join(root, "sw/bootloader/test/VP_FX1_V3.0/profiles", pid)
    for file, key in (("profile.csrw", "profile_sha256"), ("profile.bin", "profile_bin_sha256")):
        p = os.path.join(pdir, file)
        if not os.path.isfile(p) or hashlib.sha256(open(p, "rb").read()).hexdigest() != s.get(key):
            errors.append("sample %s: shared %s differs from the manifest" % (s["name"], file))
    job = open(os.path.join(d, "job.bin"), "rb").read()
    p = os.path.join(pdir, "profile.bin")
    if len(job) != 64 or not os.path.isfile(p) or os.path.getsize(p) != struct.unpack("<16I", job)[10] * 8:
        errors.append("sample %s: job profile_count differs from the shared profile" % s["name"])
    if "frames" in s and (len(job) != 64 or s["frames"] != struct.unpack("<16I", job)[11]):
        errors.append("sample %s: job frames differs from the manifest" % s["name"])
for e in errors:
    print("  FAIL " + e)
if not errors:
    print("  ok   manifest: %d artefacts, revision %s (%s), %d sample(s)"
          % (len(m["artefacts"]), m["source"]["package_revision"][:12],
             "dirty" if m["source"]["dirty_at_package_time"] else "clean", len(m["isp_samples"])))
sys.exit(1 if errors else 0)
EOF

# ── nothing points back at the source tree ───────────────────────────────────
if [[ -n "${source_dir}" ]]; then
    hits="$(find vp sw -type f -path "*/${version}/*" ! -path "sw/bootloader/build/*" -print0 \
        | xargs -0 grep -IlF "${source_dir}" 2>/dev/null || true)"
    [[ -z "${hits}" ]] && ok "no text file names ${source_dir}" || bad "text files name the source tree: ${hits}"
fi

if [[ ${failures} -eq 0 ]]; then
    echo "check_package: PASS"
else
    echo "check_package: FAIL (${failures})"
fi
[[ ${failures} -eq 0 ]]
