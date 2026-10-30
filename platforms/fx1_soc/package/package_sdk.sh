#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Assemble the VP_FX1_V3.0 SDK delivery (FX1 SoC VP + FreeRTOS CLI firmware)
# from a CDC-VP build that has target fx1_all built.
#
#   platforms/fx1_soc/package/package_sdk.sh --build-dir DIR --out DIR
#       [--tooldir DIR] [--sample NAME=RAW[,profile=FILE][,internal]] ... [--allow-dev-build]
#
# --out receives the tree in the SDK repository layout (vp/..., sw/...), so an
# export is a copy (export_to_sdk.sh). Nothing outside --out is written.
#
# What goes in, and where it comes from:
#   vp/bin/VP_FX1_V3.0       fx1_soc (RPATH $ORIGIN) + the libsystemc it needs
#   vp/doc/VP_FX1_V3.0       README, memory map, camera guide, licences, BUILD_MANIFEST.json,
#                            SHA256SUMS (tracked files only, see below)
#   sw/bootloader/sources/   the FreeRTOS CLI sources, BSP, ISP driver, the camera
#                            drivers (CSI-2 RX, I2C0, sensor) and examples, and the
#                            FreeRTOS-Kernel subset it compiles, with a
#                            stand-alone Makefile; copied, never rewritten
#   sw/bootloader/build/     outputs of that Makefile, run here by this script
#                            (the SDK repository does not track build/)
#   sw/bootloader/test/      run_vp.sh, console client, replay scripts, the ISP
#                            samples (job files + golden NV12 SHA-256 from the
#                            stand-alone ISP model)
#
# Default sample: d65, components/isp_tlm/input/D65_raw_2688x1520_5376.raw
# (cleared, ASSET_PROVENANCE.md). All samples use the checked-in shared full
# test profile by default; profile=FILE selects a different fixed profile.
#
# Exit status: 0 the tree is complete and self-consistent, 1 otherwise.
set -euo pipefail
export PATH="/usr/bin:/bin:${PATH}"

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
src="$(cd "${here}/../../.." && pwd)"
version="VP_FX1_V3.0"
build_dir="" out="" tooldir="" allow_dev=0
samples=()

die() { echo "package_sdk: $*" >&2; exit 1; }
log() { echo "[package] $*"; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir) build_dir="$(cd "$2" && pwd)"; shift 2 ;;
        --out) out="$2"; shift 2 ;;
        --tooldir) tooldir="$2"; shift 2 ;;
        --sample) samples+=("$2"); shift 2 ;;
        --allow-dev-build) allow_dev=1; shift ;;
        -h|--help) sed -n '3,30p' "$0"; exit 0 ;;
        *) die "unknown option $1" ;;
    esac
done
[[ -n "${build_dir}" && -n "${out}" ]] || die "usage: package_sdk.sh --build-dir DIR --out DIR (see --help)"
[[ ${#samples[@]} -gt 0 ]] || samples=("d65=${src}/components/isp_tlm/input/D65_raw_2688x1520_5376.raw")

sim="${build_dir}/platforms/fx1_soc/fx1_soc"
ref_tool="${build_dir}/components/FX1_Components/isp/fx1_isp_run_raw"
isp="${src}/components/FX1_Components/isp"
shared_profile="${isp}/samples/profiles/shared_bggr_2688x1520_full.csrw"
fw="${src}/fw/fx1_soc"
kernel="${src}/third_party/FreeRTOS-Kernel"
vpp="${src}/third_party/riscv-vp-plusplus"
for f in "${sim}" "${ref_tool}"; do
    [[ -x "${f}" ]] || die "missing ${f}: build target fx1_all in ${build_dir} first"
done
if [[ -z "${tooldir}" ]]; then
    prefix="$(sed -n 's/^FX1_RISCV_PREFIX:[A-Z]*=//p' "${build_dir}/CMakeCache.txt")"
    [[ -n "${prefix}" ]] || die "no FX1_RISCV_PREFIX in ${build_dir}/CMakeCache.txt; pass --tooldir"
    tooldir="$(dirname "${prefix}")"
fi
[[ -x "${tooldir}/riscv-none-elf-gcc" ]] || die "no riscv-none-elf-gcc in ${tooldir}"
grep -q '^#define tskKERNEL_VERSION_NUMBER *"V11.2.0"' "${kernel}/include/task.h" \
    || die "FreeRTOS-Kernel V11.2.0 not found in ${kernel} (tools/third_party/setup_third_party.sh)"
build_type="$("${sim}" --version 2>/dev/null | sed -n 's/^  build type *\([^,]*\),.*/\1/p')"
if [[ "${build_type}" != "Release" && ${allow_dev} -eq 0 ]]; then
    die "the simulator is a '${build_type}' build; package a Release build (or pass --allow-dev-build)"
fi
"${sim}" --version 2>/dev/null | grep -q '^  camera *CSI-2 RX' \
    || die "the simulator has no camera: configure ${build_dir} with -DFX1_BUILD_MIPI_CSI2=ON"
csi2="${src}/components/FX1_Components/mipi-csi2/driver"

rm -rf "${out}"
mkdir -p "${out}"
out="$(cd "${out}" && pwd)"
bin="${out}/vp/bin/${version}"
doc="${out}/vp/doc/${version}"
sdk="${out}/sw/bootloader/sources/${version}"
fwout="${out}/sw/bootloader/build/${version}"
test="${out}/sw/bootloader/test/${version}"
mkdir -p "${bin}" "${doc}/licenses" "${sdk}" "${fwout}" "${test}/isp" "${test}/profiles"

# ── the VP ────────────────────────────────────────────────────────────────────
log "VP binary and runtime"
cp "${sim}" "${bin}/fx1_soc"
rpath="$(readelf -d "${bin}/fx1_soc" | sed -n 's/.*(R[UN]*PATH).*\[\(.*\)\]/\1/p')"
[[ "${rpath}" == '$ORIGIN' ]] || die "fx1_soc RPATH is '${rpath}', expected \$ORIGIN (cdc_make_portable)"
needed="$(readelf -d "${bin}/fx1_soc" | sed -n 's/.*NEEDED.*\[\(libsystemc[^]]*\)\]/\1/p')"
[[ -n "${needed}" && -f "${build_dir}/platforms/fx1_soc/${needed}" ]] || die "SystemC runtime ${needed} not beside ${sim}"
cp -L "${build_dir}/platforms/fx1_soc/${needed}" "${bin}/${needed}"

# ── firmware sources: the exact files the CDC-VP build compiles ──────────────
log "firmware sources"
mkdir -p "${sdk}/app" "${sdk}/bsp" "${sdk}/include/fx1" "${sdk}/include/dma" "${sdk}/include/fx1_isp" "${sdk}/include/fx1_ddr3" \
         "${sdk}/drivers/isp" "${sdk}/isp_selftest" "${sdk}/demo" "${sdk}/tests" "${sdk}/ddr3/examples" \
         "${sdk}/include/fx1_csi2" "${sdk}/drivers/csi2" "${sdk}/drivers/camera" "${sdk}/camera" "${sdk}/camera_isp"
cp "${here}/sdk/Makefile" "${here}/sdk/README.md" "${sdk}/"
cp "${fw}"/freertos/*.c "${fw}"/freertos/*.h "${fw}"/freertos/*.S "${sdk}/app/"
cp "${fw}/freertos/tests/isp_auto_policy_test.c" "${sdk}/tests/"
cp "${fw}"/common/crt0.S "${fw}"/common/fx1_fw.c "${fw}"/common/fx1_fw.h "${fw}"/common/fx1.ld.S "${sdk}/bsp/"
# DDR3: BootROM/SRAM start-up, reference bring-up code, firmware-init examples (make ddr3).
cp "${fw}"/common/crt0_rom.S "${fw}"/common/fx1_rom.ld.S "${sdk}/bsp/"
cp "${fw}"/ddr3/ddr3_fw.c "${fw}"/ddr3/ddr3_fw.h "${fw}"/ddr3/ddr3_probe.S "${fw}"/ddr3/ddr3_ref_1600.h "${sdk}/ddr3/"
cp "${here}/sdk/ddr3_README.md" "${sdk}/ddr3/README.md"
cp "${fw}"/tests/ddr3/fw_init.c "${fw}"/tests/ddr3/fault_recovery.c "${fw}"/tests/ddr3/power_recovery.c \
   "${fw}"/boot/boot_loader.c "${sdk}/ddr3/examples/"
cp "${src}/components/FX1_Components/fx1_map/include/fx1/fx1_memory_map.h" "${sdk}/include/fx1/"
# DDR3 controller register map (provisional encodings included) and the VP DDR_SYSCTRL glue.
cp "${src}/components/FX1_Components/fx1_map/include/fx1/fx1_ddr_sysctrl.h" "${sdk}/include/fx1/"
cp "${src}/components/FX1_Components/ddr3/include/fx1_ddr3/fx1_ddr3_regs.h" \
   "${src}/components/FX1_Components/ddr3/include/fx1_ddr3/provisional_encodings.h" "${sdk}/include/fx1_ddr3/"
cp "${src}/components/FX1_Components/dma/include/dma/fx1_dma_regs.h" "${sdk}/include/dma/"
cp "${isp}/include/fx1_isp/fx1_isp_csr.h" "${isp}/driver/include/fx1_isp/fx1_isp_drv.h" "${sdk}/include/fx1_isp/"
cp "${isp}/driver/src/fx1_isp_drv.c" "${sdk}/drivers/isp/"
cp "${fw}"/tests/isp_stream/isp_common.c "${fw}"/tests/isp_stream/isp_common.h "${fw}"/tests/isp_stream/isp_fixture.h \
   "${sdk}/isp_selftest/"
cp "${fw}/demo/isp_raw_frame/isp_demo_job.h" "${fw}/demo/isp_raw_frame/isp_demo_setup.h" "${sdk}/demo/"
# Camera (CSI-2 M5b): reference CSI-2 RX driver, BSP drivers, examples (make camera).
cp "${csi2}/include/fx1_csi2/fx1_csi2_drv.h" "${csi2}/include/fx1_csi2/fx1_csi2_regs.h" "${sdk}/include/fx1_csi2/"
cp "${csi2}/src/fx1_csi2_drv.c" "${sdk}/drivers/csi2/"
cp "${fw}"/drivers/fx1_i2c.[ch] "${fw}"/drivers/fx1_ccs_sensor.[ch] "${fw}"/drivers/fx1_csi2_hal_mmio.[ch] "${sdk}/drivers/camera/"
cp "${fw}"/tests/camera/*.[ch] "${sdk}/camera/"
# Camera manager and camera -> ISP examples (CSI-2 M6a/M6b): the manager is
# also part of the FreeRTOS CLI (`camera` commands); the golden header holds
# the ISP profiles and the expected NV12 CRCs (gen_golden.py, not shipped).
cp "${fw}"/drivers/fx1_camera.[ch] "${fw}"/drivers/fx1_isp_hal_mmio.[ch] "${sdk}/drivers/camera/"
cp "${fw}"/tests/camera_isp/cam_isp.[ch] "${fw}"/tests/camera_isp/stream.c "${fw}"/tests/camera_isp/camera_isp_golden.h \
   "${sdk}/camera_isp/"
k="${sdk}/third_party/FreeRTOS-Kernel"
mkdir -p "${k}/include" "${k}/portable/MemMang" "${k}/portable/GCC/RISC-V"
cp "${kernel}/LICENSE.md" "${kernel}/tasks.c" "${kernel}/queue.c" "${kernel}/list.c" "${k}/"
cp "${kernel}"/include/*.h "${k}/include/"
cp "${kernel}/portable/MemMang/heap_4.c" "${k}/portable/MemMang/"
cp "${kernel}"/portable/GCC/RISC-V/{port.c,portASM.S,portContext.h,portmacro.h} "${k}/portable/GCC/RISC-V/"

# ── firmware outputs, from the shipped Makefile ──────────────────────────────
log "firmware build with the SDK Makefile (${tooldir})"
make -s -C "${sdk}" TOOLDIR="${tooldir}" OUT="${fwout}" all > "${out}/.make.log" 2>&1 \
    || { cat "${out}/.make.log" >&2; die "the SDK Makefile failed"; }
rm -f "${out}/.make.log"
# The DDR3 examples must build from the shipped sources too (outputs not shipped).
d3check="$(mktemp -d "${TMPDIR:-/tmp}/fx1_ddr3_check.XXXXXX")"
make -s -C "${sdk}" TOOLDIR="${tooldir}" OUT="${d3check}" ddr3 > "${out}/.make.log" 2>&1 \
    || { cat "${out}/.make.log" >&2; rm -rf "${d3check}"; die "the SDK Makefile failed on the DDR3 examples"; }
rm -rf "${d3check}" "${out}/.make.log"
# The camera examples must build from the shipped sources too (outputs not shipped).
camcheck="$(mktemp -d "${TMPDIR:-/tmp}/fx1_camera_check.XXXXXX")"
make -s -C "${sdk}" TOOLDIR="${tooldir}" OUT="${camcheck}" camera > "${out}/.make.log" 2>&1 \
    || { cat "${out}/.make.log" >&2; rm -rf "${camcheck}"; die "the SDK Makefile failed on the camera examples"; }
rm -rf "${camcheck}" "${out}/.make.log"

# ── test launchers and ISP samples ───────────────────────────────────────────
log "launchers"
cp "${here}/test/run_vp.sh" "${here}/test/isp_run.txt" "${test}/"
cp "${fw}/freertos/tests/cli_smoke.txt" "${fw}/freertos/tests/cli_camera.txt" "${test}/"
cp "${src}/platforms/fx1_soc/tools/fx1_console.sh" "${test}/"
cp "${src}/platforms/fx1_soc/demo/isp_pack.py" "${test}/"
cp "${here}/test/isp_auto.py" "${test}/"
chmod +x "${test}/run_vp.sh" "${test}/fx1_console.sh" "${test}/isp_pack.py"

scratch="$(mktemp -d "${TMPDIR:-/tmp}/fx1_package.XXXXXX")"
trap 'rm -rf "${scratch}"' EXIT
sample_json="${scratch}/samples.json"
echo "[" > "${sample_json}"
first=1
for spec in "${samples[@]}"; do
    name="${spec%%=*}"
    rest="${spec#*=}"
    raw="${rest%%,*}"
    profile="" internal=0
    IFS=',' read -r -a opts <<< "${rest#"${raw}"}"
    for o in "${opts[@]}"; do
        case "${o}" in
            "") ;;
            profile=*) profile="${o#profile=}" ;;
            internal) internal=1 ;;
            *) die "sample ${name}: unknown option ${o}" ;;
        esac
    done
    [[ "${name}" =~ ^[a-z0-9_]+$ ]] || die "sample name '${name}': use [a-z0-9_]"
    [[ -f "${raw}" ]] || die "sample ${name}: RAW ${raw} not found"
    log "ISP sample ${name}: $(basename "${raw}")"
    w="${scratch}/${name}"
    mkdir -p "${w}"
    [[ -n "${profile}" ]] || profile="${shared_profile}"
    [[ -f "${profile}" ]] || die "sample ${name}: profile ${profile} not found"
    profile_id="$(basename "${profile}" .csrw)"
    [[ "${profile_id}" =~ ^[a-z0-9_]+$ ]] || die "profile ID '${profile_id}': use [a-z0-9_]"
    profile_dir="${test}/profiles/${profile_id}"
    d="${test}/isp/${name}"
    mkdir -p "${d}"
    python3 "${src}/platforms/fx1_soc/demo/run_isp_demo.py" --raw "${raw}" --profile "${profile}" \
        --build-dir "${build_dir}" --out "${w}/job" --frames 2 --prepare-only --job-env "${d}/job.env" > "${w}/prepare.log"
    cp "${w}/job/input.isp16" "${w}/job/job.bin" "${d}/"
    printf '\nPROFILE_ID=%s\n' "${profile_id}" >> "${d}/job.env"
    if [[ -d "${profile_dir}" ]]; then
        cmp -s "${profile}" "${profile_dir}/profile.csrw" \
            && cmp -s "${w}/job/profile.bin" "${profile_dir}/profile.bin" \
            || die "profile ID '${profile_id}' names different profile contents"
    else
        mkdir -p "${profile_dir}"
        cp "${profile}" "${profile_dir}/profile.csrw"
        cp "${w}/job/profile.bin" "${profile_dir}/"
    fi
    profile_sha="$(sha256sum "${profile_dir}/profile.csrw" | cut -d' ' -f1)"
    profile_bin_sha="$(sha256sum "${profile_dir}/profile.bin" | cut -d' ' -f1)"
    # Golden: the stand-alone ISP model, same input and profile (not the VP run).
    mkdir -p "${w}/ref"
    "${ref_tool}" --input "${w}/job/input.isp16" --width 2688 --height 1520 --profile "${profile}" --frames 2 \
        --out "${w}/ref/frame" > "${w}/ref.log" 2>&1 && [[ -s "${w}/ref/frame.nv12" ]] \
        || { cat "${w}/ref.log" >&2; die "sample ${name}: the reference ISP model produced no NV12"; }
    golden="$(sha256sum "${w}/ref/frame.nv12" | cut -d' ' -f1)"
    echo "${golden}" > "${d}/expected_nv12.sha256"
    printf '%s, shared profile %s\n' "$(basename "${raw}")" "${profile_id}" > "${d}/SAMPLE.txt"
    auto_json=""
    if [[ "${profile_id}" == "shared_bggr_2688x1520_full" ]]; then
        # Firmware selects controls from AWB on the CPU. Generate a golden
        # independently with those controls in the stand-alone ISP, two render
        # frames from cold temporal state. The probe is not in this reference.
        "${test}/run_vp.sh" isp "${name}" auto > "${w}/auto.log" 2>&1 \
            || { cat "${w}/auto.log" >&2; die "sample ${name}: auto firmware failed"; }
        "${ref_tool}" --input "${d}/input.isp16" --width 2688 --height 1520 \
            --profile "${fwout}/isp/${name}/resolved_profile.csrw" --frames 2 \
            --out "${w}/ref/auto" > "${w}/auto_ref.log" 2>&1 \
            || { cat "${w}/auto_ref.log" >&2; die "sample ${name}: auto reference failed"; }
        auto_golden="$(sha256sum "${w}/ref/auto.nv12" | cut -d' ' -f1)"
        [[ "$(sha256sum "${fwout}/isp/${name}/platform.nv12" | cut -d' ' -f1)" == "${auto_golden}" ]] \
            || die "sample ${name}: auto output differs from the stand-alone ISP"
        echo "${auto_golden}" > "${d}/expected_auto_nv12.sha256"
        auto_json=", \"auto_policy\": \"neutral-v1\", \"auto_frames\": 3, \"expected_auto_nv12_sha256\": \"${auto_golden}\""
    fi
    raw_sha="$(sha256sum "${raw}" | cut -d' ' -f1)"
    {
        if [[ ${internal} -eq 1 ]]; then
            echo "$(basename "${raw}"), 2688x1520 BGGR 10-bit -- INTERNAL USE ONLY, not cleared for public release"
        else
            echo "$(basename "${raw}"), 2688x1520 BGGR 10-bit -- Apache-2.0 project asset (CDC-VP ASSET_PROVENANCE.md)"
        fi
        echo "RAW SHA-256       ${raw_sha}"
        echo "format            le16_raw10_msb, converted to the ISP 16-bit container (input.isp16)"
        echo "profile           profiles/${profile_id}/profile.csrw ($(grep -c -v -e '^#' -e '^\s*$' "${profile}") writes)"
        echo "                  fixed test profile shared by samples selecting this ID; not a sensor calibration"
        echo "frames            2 (first frame warms up GTM auto; output is the second frame)"
        echo "expected NV12     ${golden}"
        echo "                  from the stand-alone ISP model (fx1_isp_run_raw), same input and profile"
    } > "${d}/SAMPLE.txt"
    [[ ${first} -eq 1 ]] || echo "," >> "${sample_json}"
    first=0
    printf '{"name": "%s", "raw": "%s", "raw_sha256": "%s", "profile_id": "%s", "profile": "%s", "profile_sha256": "%s", "profile_bin_sha256": "%s", "expected_nv12_sha256": "%s", "frames": 2, "internal_only": %s%s}\n' \
        "${name}" "$(basename "${raw}")" "${raw_sha}" "${profile_id}" "profiles/${profile_id}/profile.csrw" \
        "${profile_sha}" "${profile_bin_sha}" "${golden}" \
        "$([[ ${internal} -eq 1 ]] && echo true || echo false)" "${auto_json}" >> "${sample_json}"
done
echo "]" >> "${sample_json}"
[[ -d "${test}/isp/d65" ]] || die "the default sample d65 is missing (run_vp.sh isp uses it)"

# ── documentation and licences ───────────────────────────────────────────────
log "documentation and licences"
cp "${here}/docs/README.md" "${here}/docs/MEMORY_MAP.md" "${doc}/"
mkdir -p "${doc}/camera"
cp "${here}/docs/CAMERA_FW_GUIDE.md" "${doc}/camera/README.md"
# DDR3 documents for driver developers. Links to documents that stay in the VP
# source tree become plain text, so the package has no dead link.
mkdir -p "${doc}/ddr3"
cp "${here}/docs/DDR3_FW_GUIDE.md" "${doc}/ddr3/README.md"
ddr3_docs="${src}/components/FX1_Components/ddr3/docs"
cp "${ddr3_docs}/programming_guide.md" "${doc}/ddr3/programming_guide.md"
python3 - "${doc}/ddr3" <<'PYEOF'
import os, re, sys
d = sys.argv[1]
names = set(os.listdir(d))
for f in names:
    p = os.path.join(d, f)
    original = text = open(p).read()
    # A parenthesized reference to a document that is not shipped, e.g. "([x.md](x.md) L-14)", goes.
    text = re.sub(r"\s*\(\[[^\]]+\]\(([^)#\s]+)\)(\s+L-\d+)?\)",
                  lambda m: m.group(0) if m.group(1) in names else "", text)
    fixed = re.sub(r"\[([^\]]+)\]\(([^)#\s]+)(#[^)]*)?\)",
                   lambda m: m.group(0) if m.group(2) in names or "://" in m.group(2) else m.group(1), text)
    if fixed != original:
        open(p, "w").write(fixed)
PYEOF
cp "${src}/LICENSE" "${doc}/licenses/Apache-2.0.txt"
cp "${src}/NOTICE" "${doc}/licenses/CDC-VP-NOTICE.txt"
cp "${src}/THIRD_PARTY.md" "${doc}/licenses/THIRD_PARTY.md"
cp "${vpp}/LICENSE" "${doc}/licenses/RISCV-VP-PLUSPLUS.MIT.txt"
cmake -DSOURCE_DIR="${src}" -DSOFTFLOAT_SOURCE="${vpp}/vp/src/vendor/softfloat/f32_add.c" \
      -DOUTPUT="${doc}/licenses/BERKELEY-SOFTFLOAT-3d.BSD-3-Clause.txt" \
      -P "${src}/platforms/riscv_vpp_compiler_vp/cmake/write_softfloat_notice.cmake" > /dev/null
systemc_home="$(sed -n 's/^SYSTEMC_HOME:[A-Z]*=//p' "${build_dir}/CMakeCache.txt")"
[[ -f "${systemc_home}/share/doc/systemc/LICENSE" ]] || die "no SystemC LICENSE under ${systemc_home}"
cp "${systemc_home}/share/doc/systemc/LICENSE" "${doc}/licenses/SYSTEMC.Apache-2.0.txt"
cp "${kernel}/LICENSE.md" "${doc}/licenses/FREERTOS-KERNEL.MIT.txt"

# ── manifest and checksums ───────────────────────────────────────────────────
log "manifest and checksums"
python3 "${here}/write_manifest.py" --out "${out}" --source "${src}" --build-dir "${build_dir}" \
    --tooldir "${tooldir}" --samples "${sample_json}" --manifest "${doc}/BUILD_MANIFEST.json"
# SHA256SUMS covers what the SDK repository tracks: not sw/bootloader/build/,
# which it ignores and every user regenerates (its hashes are in the manifest).
(cd "${out}" && find vp sw -type f ! -path "sw/bootloader/build/*" ! -name SHA256SUMS -print0 \
    | LC_ALL=C sort -z | xargs -0 sha256sum) > "${doc}/SHA256SUMS"

"${here}/check_package.sh" "${out}"
log "done: ${out}"
