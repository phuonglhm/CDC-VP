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
#                            drivers (CSI-2 RX, I2C0, sensor) and examples, the
#                            Ethernet driver (portable eth_drv + FX1 manager) and
#                            its bare-metal example, and the
#                            FreeRTOS-Kernel subset it compiles, with a
#                            stand-alone Makefile; copied, never rewritten
#   sw/bootloader/build/     outputs of that Makefile, run here by this script
#                            (the SDK repository does not track build/)
#   sw/bootloader/test/      run_vp.sh, console client, replay scripts, the ISP
#                            samples (job files + golden NV12 SHA-256 from the
#                            stand-alone ISP model), the NPU launcher and job
#                            fixtures (goldens from the firmware's C oracle)
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
"${sim}" --version 2>/dev/null | grep -q '^  NPU *SAURIA v4.7' \
    || die "the simulator has no NPU: configure ${build_dir} with -DFX1_SOC_ENABLE_NPU=ON (the default)"
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
         "${sdk}/include/fx1_csi2" "${sdk}/drivers/csi2" "${sdk}/drivers/camera" "${sdk}/camera" "${sdk}/camera_isp" \
         "${sdk}/drivers/eth" "${sdk}/eth" "${sdk}/include/fx1_npu" "${sdk}/drivers/npu" "${sdk}/npu_selftest" "${sdk}/npu_jobs"
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
# ISP RAW-sequence stream session ABI (`isp stream`, run_vp.sh isp-stream; VP-only host contract).
cp "${src}/components/FX1_Components/fx1_map/include/fx1/fx1_isp_stream.h" "${sdk}/include/fx1/"
# DDR3 controller register map (provisional encodings included) and the VP DDR_SYSCTRL glue.
cp "${src}/components/FX1_Components/fx1_map/include/fx1/fx1_ddr_sysctrl.h" "${sdk}/include/fx1/"
cp "${src}/components/FX1_Components/ddr3/include/fx1_ddr3/fx1_ddr3_regs.h" \
   "${src}/components/FX1_Components/ddr3/include/fx1_ddr3/provisional_encodings.h" "${sdk}/include/fx1_ddr3/"
cp "${src}/components/FX1_Components/dma/include/dma/fx1_dma_regs.h" "${sdk}/include/dma/"
# NPU (plan FX1_NPU_V47 N3): register ABI header, reference driver, job format,
# and the `npu selftest` cases + C oracle the CLI links. Launcher, fixtures and
# guide: below (N5).
cp "${src}/components/FX1_Components/npu/include/fx1_npu/fx1_npu_regs.h" "${sdk}/include/fx1_npu/"
cp "${fw}"/drivers/fx1_npu.[ch] "${fw}"/drivers/fx1_npu_build.c "${fw}"/drivers/fx1_npu_job.[ch] \
   "${fw}"/drivers/fx1_npu_job_run.c "${sdk}/drivers/npu/"
cp "${fw}"/tests/npu_common/npu_oracle.[ch] "${fw}"/tests/npu_common/npu_cases.[ch] "${sdk}/npu_selftest/"
cp "${fw}/host/npu_job_gen.c" "${sdk}/npu_jobs/"   # host tool: make npu-jobs
cp "${isp}/include/fx1_isp/fx1_isp_csr.h" "${isp}/driver/include/fx1_isp/fx1_isp_drv.h" "${sdk}/include/fx1_isp/"
cp "${isp}/driver/src/fx1_isp_drv.c" "${sdk}/drivers/isp/"
cp "${fw}"/tests/isp_stream/isp_common.c "${fw}"/tests/isp_stream/isp_common.h "${fw}"/tests/isp_stream/isp_fixture.h \
   "${sdk}/isp_selftest/"
cp "${fw}/demo/isp_raw_frame/isp_demo_job.h" "${fw}/demo/isp_raw_frame/isp_demo_setup.h" "${sdk}/demo/"
# Camera (CSI-2 M5b): reference CSI-2 RX driver, BSP drivers, examples (make camera).
cp "${csi2}/include/fx1_csi2/fx1_csi2_drv.h" "${csi2}/include/fx1_csi2/fx1_csi2_regs.h" "${sdk}/include/fx1_csi2/"
cp "${csi2}/src/fx1_csi2_drv.c" "${sdk}/drivers/csi2/"
# Ethernet (plan M5): the portable driver and its register header (the files the
# CDC-VP build compiles, components/FX1_Components/ethernet/driver), the FX1
# manager (drivers/fx1_eth.[ch]) and the bare-metal wiring example (make eth).
eth_drv="${src}/components/FX1_Components/ethernet/driver"
cp "${eth_drv}/include/eth_drv.h" "${eth_drv}/include/eth_regs.h" "${eth_drv}/src/eth_drv.c" \
   "${fw}"/drivers/fx1_eth.[ch] "${sdk}/drivers/eth/"
cp "${fw}/tests/eth_wiring/main.c" "${sdk}/eth/eth_wiring.c"
cp "${fw}/tests/atomic/atomic_common.h" "${sdk}/eth/"
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
# The Ethernet example must build from the shipped sources too (outputs not shipped).
ethcheck="$(mktemp -d "${TMPDIR:-/tmp}/fx1_eth_check.XXXXXX")"
make -s -C "${sdk}" TOOLDIR="${tooldir}" OUT="${ethcheck}" eth > "${out}/.make.log" 2>&1 \
    || { cat "${out}/.make.log" >&2; rm -rf "${ethcheck}"; die "the SDK Makefile failed on the Ethernet example"; }
rm -rf "${ethcheck}" "${out}/.make.log"
# The NPU fixtures must come out of the shipped generator (host compiler) byte for
# byte as the build's fixtures that are shipped (outputs not shipped: test/npu/ is).
npucheck="$(mktemp -d "${TMPDIR:-/tmp}/fx1_npu_check.XXXXXX")"
make -s -C "${sdk}" OUT="${npucheck}" npu-jobs > "${out}/.make.log" 2>&1 \
    || { cat "${out}/.make.log" >&2; rm -rf "${npucheck}"; die "the SDK Makefile failed on the NPU job generator"; }
diff -r "${npucheck}/npu_jobs" "${build_dir}/fw/fx1_soc/npu_jobs" -x .stamp > "${out}/.make.log" 2>&1 \
    || { cat "${out}/.make.log" >&2; rm -rf "${npucheck}"; die "make npu-jobs differs from the build's NPU fixtures"; }
rm -rf "${npucheck}" "${out}/.make.log"

# ── test launchers and ISP samples ───────────────────────────────────────────
log "launchers"
cp "${here}/test/run_vp.sh" "${here}/test/isp_run.txt" "${test}/"
cp "${fw}/freertos/tests/cli_smoke.txt" "${fw}/freertos/tests/cli_camera.txt" "${test}/"
cp "${fw}/freertos/tests/cli_eth.txt" "${fw}/freertos/tests/cli_eth_load.txt" "${test}/"
cp "${src}/platforms/fx1_soc/tools/fx1_console.sh" "${test}/"
cp "${src}/platforms/fx1_soc/demo/isp_pack.py" "${test}/"
cp "${here}/test/isp_auto.py" "${here}/test/csrw_to_bin.sh" "${test}/"
# VP-only post-Demosaic capture (run_vp.sh isp ... --dump-demosaic): selection
# and validation, Python standard library only.
cp "${isp}/tools/demosaic_select.py" "${isp}/tools/demosaic_validate.py" "${test}/"
# Profile codec (ISP-UPD-008): register or field syntax -> ordered CSR writes;
# csrw_to_bin.sh and isp_auto.py import it, with the shipped CSR header.
cp "${isp}/tools/csrw_codec.py" "${test}/"
# ISP RAW sequence -> video (run_vp.sh isp-stream): the launcher and the session preparation
# (Python standard library only; FFmpeg is the system's, not shipped).
cp "${src}/platforms/fx1_soc/demo/isp_stream.py" "${src}/platforms/fx1_soc/demo/isp_stream_prep.py" "${test}/"
# NPU (plan FX1_NPU_V47 N5): the job launcher (Python standard library only), the
# self-test console script, and every job fixture of the build (fx1_npu_jobs:
# fw/fx1_soc/host/npu_job_gen.c, goldens from the firmware's C oracle).
cp "${src}/platforms/fx1_soc/demo/npu_run.py" "${here}/test/cli_npu_selftest.txt" "${test}/"
npu_jobs="${build_dir}/fw/fx1_soc/npu_jobs"
[[ -f "${npu_jobs}/chain/fixture.json" ]] || die "no NPU fixtures in ${npu_jobs}: build target fx1_all (fx1_npu_jobs)"
mkdir -p "${test}/npu"
for jd in "${npu_jobs}"/*/; do
    n="$(basename "${jd}")"
    mkdir -p "${test}/npu/${n}"
    cp "${jd}fixture.json" "${jd}job.bin" "${jd}window.bin" "${test}/npu/${n}/"
    for g in "${jd}"golden_*.bin; do [[ -e "${g}" ]] && cp "${g}" "${test}/npu/${n}/"; done
    python3 - "${test}/npu/${n}" <<'PYEOF' || die "NPU fixture ${n}: inconsistent"
import hashlib, json, os, sys, zlib
d = sys.argv[1]
s = json.load(open(os.path.join(d, "fixture.json")))
assert os.path.getsize(os.path.join(d, s["job_file"])) == s["job_bytes"], "job.bin size"
assert os.path.getsize(os.path.join(d, s["window_file"])) == s["load_bytes"], "window.bin size"
outs = [t for t in s["tensors"] if t["role"] == "OUTPUT"]
for t in outs:
    if t["golden"]:
        g = open(os.path.join(d, t["golden"]), "rb").read()
        assert len(g) == t["bytes"] and zlib.crc32(g) == t["crc32"], "golden %s: size or CRC-32" % t["golden"]
xchecked = s["name"] in ("chain", "dfc", "mixed", "large", "long", "busy")
neg = s["expect"]["status"] != "OK"
lines = [s["description"],
         "expected outcome  %s%s" % (s["expect"]["status"], "" if not neg else " (negative fixture: the firmware must refuse it as declared)"),
         "operations        %s" % (" ".join(s["ops"]) or "-"),
         "arena             %s, window 0x%08x (%d bytes, %d loaded), job 0x%08x, execution budget %d ms"
         % (s["arena"], s["window_addr"], s["window_size"], s["load_bytes"], s["job_addr"], s["timeout_ms"]),
         "outputs           %s" % (", ".join("%s (tensor %d, %d bytes)" % (t["name"], t["index"], t["bytes"]) for t in outs) or "-"),
         "provenance        generated by npu_job_gen (sources/VP_FX1_V3.0/npu_jobs/npu_job_gen.c, make npu-jobs): synthetic data",
         "                  from fixed seeds (no network, weights or images); goldens from the firmware's C oracle",
         "                  (npu_selftest/npu_oracle.c), not from the model;%s" % (
             " its rich / DFC operations cross-checked byte" if xchecked else " no NPU operation runs" if neg else ""),
         "                  %s" % ("for byte with the drop's numpy reference before delivery" if xchecked
                                  else "(firmware checks only)" if neg else "GEMM bank: C oracle, compared with the model before delivery"),
         "run               ./run_vp.sh npu %s      (summary: sw/bootloader/build/VP_FX1_V3.0/npu/%s/summary.txt)" % (s["name"], s["name"])]
open(os.path.join(d, "FIXTURE.txt"), "w").write("\n".join(lines) + "\n")
PYEOF
done
chmod +x "${test}/run_vp.sh" "${test}/fx1_console.sh" "${test}/isp_pack.py" "${test}/csrw_to_bin.sh" "${test}/npu_run.py"

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
        --out "${w}/ref/frame" --dump-demosaic > "${w}/ref.log" 2>&1 && [[ -s "${w}/ref/frame.nv12" ]] \
        && [[ -s "${w}/ref/frame.demosaic.rgb16" ]] \
        || { cat "${w}/ref.log" >&2; die "sample ${name}: the reference ISP model produced no NV12 / Demosaic capture"; }
    golden="$(sha256sum "${w}/ref/frame.nv12" | cut -d' ' -f1)"
    echo "${golden}" > "${d}/expected_nv12.sha256"
    # The VP-only post-Demosaic capture of the same (final) frame.
    python3 "${isp}/tools/demosaic_validate.py" "${w}/ref/frame.demosaic" > /dev/null \
        || die "sample ${name}: the reference Demosaic capture is not valid"
    dgolden="$(sha256sum "${w}/ref/frame.demosaic.rgb16" | cut -d' ' -f1)"
    echo "${dgolden}" > "${d}/expected_demosaic.sha256"
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
            --out "${w}/ref/auto" --dump-demosaic > "${w}/auto_ref.log" 2>&1 \
            || { cat "${w}/auto_ref.log" >&2; die "sample ${name}: auto reference failed"; }
        auto_golden="$(sha256sum "${w}/ref/auto.nv12" | cut -d' ' -f1)"
        [[ "$(sha256sum "${fwout}/isp/${name}/platform.nv12" | cut -d' ' -f1)" == "${auto_golden}" ]] \
            || die "sample ${name}: auto output differs from the stand-alone ISP"
        echo "${auto_golden}" > "${d}/expected_auto_nv12.sha256"
        python3 "${isp}/tools/demosaic_validate.py" "${w}/ref/auto.demosaic" > /dev/null \
            || die "sample ${name}: the auto reference Demosaic capture is not valid"
        auto_dgolden="$(sha256sum "${w}/ref/auto.demosaic.rgb16" | cut -d' ' -f1)"
        echo "${auto_dgolden}" > "${d}/expected_auto_demosaic.sha256"
        auto_json=", \"auto_policy\": \"neutral-v1\", \"auto_frames\": 3, \"expected_auto_nv12_sha256\": \"${auto_golden}\", \"expected_auto_demosaic_sha256\": \"${auto_dgolden}\""
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
        echo "expected Demosaic ${dgolden}"
        echo "                  demosaic.rgb16 of run_vp.sh isp ... --dump-demosaic (VP debug tap, post-Demosaic,"
        echo "                  pre-CCM, RGB12 in LE16, 2686x1518), same frame, same stand-alone model"
    } > "${d}/SAMPLE.txt"
    [[ ${first} -eq 1 ]] || echo "," >> "${sample_json}"
    first=0
    printf '{"name": "%s", "raw": "%s", "raw_sha256": "%s", "profile_id": "%s", "profile": "%s", "profile_sha256": "%s", "profile_bin_sha256": "%s", "expected_nv12_sha256": "%s", "expected_demosaic_sha256": "%s", "frames": 2, "internal_only": %s%s}\n' \
        "${name}" "$(basename "${raw}")" "${raw_sha}" "${profile_id}" "profiles/${profile_id}/profile.csrw" \
        "${profile_sha}" "${profile_bin_sha}" "${golden}" "${dgolden}" \
        "$([[ ${internal} -eq 1 ]] && echo true || echo false)" "${auto_json}" >> "${sample_json}"
done
echo "]" >> "${sample_json}"
[[ -d "${test}/isp/d65" ]] || die "the default sample d65 is missing (run_vp.sh isp uses it)"

# ── ISP RAW sequence sample (run_vp.sh isp-stream-sample) ────────────────────
# d65 -> outdoor1 -> d65 from the samples' own ISP containers (no RAW is added);
# without the internal outdoor1, d65 -> d65. The oracle of every frame (repeat 2,
# the shipped profile) is the stand-alone ISP model over the same session; the
# second frame of each source must also equal that sample's golden (the GTM
# history reaches one frame back).
log "ISP RAW sequence sample (isp_stream/)"
seq_names=()
for n in d65 outdoor1 d65; do [[ -d "${test}/isp/${n}" ]] && seq_names+=("${n}"); done
seqd="${test}/isp_stream"
mkdir -p "${seqd}"
for n in "${seq_names[@]}"; do
    grep -qx "PROFILE_ID=shared_bggr_2688x1520_full" "${test}/isp/${n}/job.env" \
        || die "sequence sample ${n} does not use the shared profile"
done
python3 - "${seqd}/sequence.json" "${seq_names[@]}" <<'PYEOF'
import json, sys
# input.isp16 is already the ISP's 12-bit container (RAW10 in bits [11:2]) and the
# shared profile keeps COMMON_INPUT_FORMAT at RAW12, as `run_vp.sh isp NAME` runs it.
m = {"version": 1, "width": 2688, "height": 1520, "bayer": "BGGR", "source_format": "le16_raw12_has",
     "input_depth": "raw12", "frames": ["../isp/%s/input.isp16" % n for n in sys.argv[2:]]}
open(sys.argv[1], "w").write(json.dumps(m, indent=2) + "\n")
PYEOF
python3 "${src}/platforms/fx1_soc/demo/isp_stream_prep.py" prepare --out "${scratch}/sequence" \
    --manifest "${seqd}/sequence.json" --profile "${test}/profiles/shared_bggr_2688x1520_full/profile.csrw" \
    --repeat 2 > "${scratch}/sequence.log" 2>&1 \
    && python3 "${src}/platforms/fx1_soc/demo/isp_stream_prep.py" reference --session "${scratch}/sequence" \
        --run-raw "${ref_tool}" >> "${scratch}/sequence.log" 2>&1 \
    || { cat "${scratch}/sequence.log" >&2; die "the sequence sample: prepare/reference failed"; }
python3 - "${scratch}/sequence/reference.json" "${seqd}/expected_frames.txt" "${test}/isp" "${seq_names[@]}" <<'PYEOF' \
    || die "the sequence sample: the model's frames do not give the samples' goldens"
import json, os, sys
ref, out, isp, names = json.load(open(sys.argv[1])), sys.argv[2], sys.argv[3], sys.argv[4:]
frames = ref["frames"]
assert len(frames) == 2 * len(names), "reference: %d frames for %d sources x 2" % (len(frames), len(names))
with open(out, "w") as f:
    f.write("# ordinal source repeat sha256 (NV12, run_vp.sh isp-stream-sample: repeat 2, the shipped profile)\n")
    for fr in frames:
        n = names[fr["source_index"]]
        if fr["repeat_index"] == 1:
            golden = open(os.path.join(isp, n, "expected_nv12.sha256")).read().strip()
            if fr["sha256"] != golden:
                sys.exit("sequence frame %d (%s, second): %s, the sample's golden is %s" % (fr["ordinal"], n, fr["sha256"], golden))
        f.write("%d %s %d %s\n" % (fr["ordinal"], n, fr["repeat_index"] + 1, fr["sha256"]))
PYEOF
# The profile the oracle was made with, as shipped: run_vp.sh isp-stream-sample applies the
# oracle only when the session's profile snapshot has this hash (a profile edited in place
# is then not compared with frames it was never made for).
(cd "${test}" && sha256sum profiles/shared_bggr_2688x1520_full/profile.csrw) > "${seqd}/profile.sha256"
{
    echo "ISP RAW sequence sample: $(printf '%s -> ' "${seq_names[@]}" | sed 's/ -> $//'), repeat 2 ($(( 2 * ${#seq_names[@]} )) ISP frames, one session)"
    echo "sources           ../isp/<sample>/input.isp16 (the samples' ISP containers; no other RAW)"
    if [[ " ${seq_names[*]} " == *" outdoor1 "* ]]; then
        echo "                  outdoor1 is INTERNAL USE ONLY, not cleared for public release (isp/outdoor1/SAMPLE.txt)"
    fi
    echo "profile           profiles/shared_bggr_2688x1520_full/profile.csrw, applied once for the whole session"
    echo "run               ./run_vp.sh isp-stream-sample --out DIR      (DIR/output.mp4, DIR/summary.txt)"
    echo "expected frames   expected_frames.txt: every frame's NV12 SHA-256 from the stand-alone ISP model"
    echo "                  (same session); each source's second frame equals its sample's golden"
    echo "                  (isp/<sample>/expected_nv12.sha256). They hold for the profile as shipped"
    echo "                  (profile.sha256) in profile mode: a run whose profile differs (another"
    echo "                  --profile, or the shared one edited in place) makes its video without the"
    echo "                  comparison."
} > "${seqd}/SEQUENCE.txt"

# ── NPU gate: the shipped launcher on the shipped fixtures ────────────────────
# The self-test and every fixture through run_vp.sh as a user runs them; their
# outcomes, the banner (model, knobs) and the identity the firmware reads go
# into the manifest. Outputs stay in the scratch directory.
log "NPU: run_vp.sh npu-selftest, npu all"
"${test}/run_vp.sh" npu-selftest > "${scratch}/npu_selftest.out" 2>&1 \
    || { cat "${scratch}/npu_selftest.out" >&2; die "run_vp.sh npu-selftest failed"; }
cp "${fwout}/npu/selftest.log" "${scratch}/npu_selftest.log"
rm -rf "${fwout:?}/npu"
"${test}/run_vp.sh" npu all --out "${scratch}/npu" > "${scratch}/npu_all.out" 2>&1 \
    || { cat "${scratch}/npu_all.out" >&2; die "run_vp.sh npu all failed"; }
python3 - "${scratch}" "${test}/npu" "${scratch}/npu.json" <<'PYEOF' || die "NPU gate summary"
import hashlib, json, os, re, sys
scratch, fixtures, out = sys.argv[1:4]
log = open(os.path.join(scratch, "npu_selftest.log"), errors="replace").read().replace("\r", "")
grab = lambda rx: (re.search(rx, log, re.M) or [None, None])[1]
banner = grab(r"^\[fx1_soc\] npu: (at .*)$")
cfg = re.search(r"profile=(\S+) host=(\S+) env=([^,\s]+)", banner or "")
ident = re.search(r"VP_ID (0x[0-9a-f]+) ABI (0x[0-9a-f]+) model hash (0x[0-9a-f]+)", log)
feat = re.search(r"npu: features (0x[0-9a-f]+) ([^,]+)", log)
assert banner and cfg and ident and feat, "self-test log lacks the banner or npu info"
res = {"banner": banner, "model_config": {"profile": cfg.group(1), "host": cfg.group(2), "env": cfg.group(3)},
       "vp_id": ident.group(1), "vp_abi": ident.group(2), "vp_model_hash": ident.group(3),
       "features": feat.group(1), "feature_names": feat.group(2).split(),
       "selftest": [l.strip() for l in log.splitlines() if l.strip().startswith("npu selftest:")], "fixtures": []}
assert any(l.endswith("PASS") for l in res["selftest"]), "self-test did not pass"
for n in sorted(os.listdir(fixtures)):
    d = os.path.join(fixtures, n)
    spec = json.load(open(os.path.join(d, "fixture.json")))
    summ = json.load(open(os.path.join(scratch, "npu", n, "summary.json")))
    assert summ["outcome"] == "PASS", "fixture %s: %s" % (n, summ.get("reason"))
    files = {f: hashlib.sha256(open(os.path.join(d, f), "rb").read()).hexdigest()
             for f in sorted(os.listdir(d)) if f.endswith((".bin", ".json"))}
    r = summ.get("result", {})
    res["fixtures"].append({"name": n, "description": spec["description"], "expected": spec["expect"]["status"],
                            "arena": spec["arena"], "ops": spec["ops"], "files_sha256": files,
                            "package_run": {"outcome": summ["outcome"], "status": r.get("status"),
                                            "outputs_matching": r.get("outputs_ok_fw"),
                                            "model_exec_cycles": r.get("model_exec_cycles"),
                                            "output_sha256": [o["sha256"] for o in summ.get("outputs", [])]}})
json.dump(res, open(out, "w"), indent=2)
PYEOF

# ── documentation and licences ───────────────────────────────────────────────
log "documentation and licences"
cp "${here}/docs/README.md" "${here}/docs/MEMORY_MAP.md" "${doc}/"
cp "${src}/platforms/fx1_soc/docs/ISP_STREAM_CONTRACT.md" "${doc}/"   # run_vp.sh isp-stream
mkdir -p "${doc}/camera"
cp "${here}/docs/CAMERA_FW_GUIDE.md" "${doc}/camera/README.md"
mkdir -p "${doc}/eth"
cp "${here}/docs/ETHERNET_FW_GUIDE.md" "${doc}/eth/README.md"
# NPU: the firmware guide (delivery, running, job format, driver, limits) as
# npu/README.md, and the register-level ABI as npu/register_abi.md: the SDK
# adaptation of the component README (same tables and headings, checked by
# fx1_unit_npu_abi_doc_sync; SDK paths, no CDC-VP source references).
mkdir -p "${doc}/npu"
cp "${here}/docs/NPU_FW_GUIDE.md" "${doc}/npu/README.md"
cp "${here}/docs/NPU_REGISTER_ABI.md" "${doc}/npu/register_abi.md"
cp "${here}/docs/NPU_MODEL_REGISTER_REFERENCE.md" "${doc}/npu/model_registers_reference.md"
# DDR3 documents for driver developers. Links to documents that stay in the VP
# source tree become plain text, so the package has no dead link.
mkdir -p "${doc}/ddr3"
cp "${here}/docs/DDR3_FW_GUIDE.md" "${doc}/ddr3/README.md"
ddr3_docs="${src}/components/FX1_Components/ddr3/docs"
cp "${ddr3_docs}/programming_guide.md" "${doc}/ddr3/programming_guide.md"
for linkdir in "${doc}/ddr3" "${doc}/npu"; do
python3 - "${linkdir}" <<'PYEOF'
import os, re, sys
d = sys.argv[1]
names = set(os.listdir(d))
for f in names:
    # The archival model README is kept verbatim after its FX1 mapping preface.
    if f == "model_registers_reference.md":
        continue
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
done
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
# NPU: fx1_soc links the SAURIA v4.7 model drop (its SystemC port of the SAURIA
# design). The upstream licence and provenance, and the drop's own notice
# (which also lists drop data this package does not contain: README.md says what).
cp "${src}/licenses/SAURIA.SHL-2.1" "${doc}/licenses/SAURIA.SHL-2.1.txt"
cp "${src}/licenses/SAURIA.PROVENANCE.md" "${doc}/licenses/SAURIA.PROVENANCE.md"
npu_root="$(sed -n 's/^FX1_NPU_MODEL_ROOT:[A-Z]*=//p' "${build_dir}/CMakeCache.txt")"
[[ -f "${npu_root}/THIRD_PARTY_NOTICES.md" ]] || die "no THIRD_PARTY_NOTICES.md in the NPU model drop ${npu_root}"
cp "${npu_root}/THIRD_PARTY_NOTICES.md" "${doc}/licenses/SAURIA-V4.7-DROP.THIRD_PARTY_NOTICES.md"
# Beside it: which of the notice's items this delivery contains (the notice's
# "Shipped" is the drop's own scope).
cp "${here}/docs/SAURIA_DROP_SDK_SCOPE.md" "${doc}/licenses/SAURIA-V4.7-DROP.SDK_SCOPE.md"

# ── manifest and checksums ───────────────────────────────────────────────────
log "manifest and checksums"
python3 "${here}/write_manifest.py" --out "${out}" --source "${src}" --build-dir "${build_dir}" \
    --tooldir "${tooldir}" --samples "${sample_json}" --npu "${scratch}/npu.json" --manifest "${doc}/BUILD_MANIFEST.json"
# SHA256SUMS covers what the SDK repository tracks: not sw/bootloader/build/,
# which it ignores and every user regenerates (its hashes are in the manifest).
(cd "${out}" && find vp sw -type f ! -path "sw/bootloader/build/*" ! -name SHA256SUMS -print0 \
    | LC_ALL=C sort -z | xargs -0 sha256sum) > "${doc}/SHA256SUMS"

"${here}/check_package.sh" "${out}"
log "done: ${out}"
