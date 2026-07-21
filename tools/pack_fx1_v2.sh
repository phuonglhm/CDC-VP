#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Package the CDC-VP NPU + FreeRTOS/TFLM handover into the internal fx1 tree.

set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
version="VP_FX1_V2.0"
fx1_root="${1:-/home/duyptt_HW/Desktop/VP_INTER/upgit/fx1}"
tmpl="${repo}/tools/fx1_v2_template"

source_dst="${fx1_root}/sw/bootloader/sources/${version}"
build_dst="${fx1_root}/sw/bootloader/build/${version}"
test_dst="${fx1_root}/sw/bootloader/test/${version}"
bin_dst="${fx1_root}/vp/bin/${version}"
doc_dst="${fx1_root}/vp/doc/${version}"

vp_pkg="${repo}/out/vp_fx1_full_soc"
fw_src="${repo}/fw/freertos_fx1"
tflm_root="${repo}/third_party/tflite-micro"
freertos_root="${repo}/third_party/FreeRTOS-Kernel"
tflm_lib="${tflm_root}/gen/riscv32_generic_rv32imac_zicsr_default_gcc/lib/libtensorflow-microlite.a"

die() { echo "ERROR: $*" >&2; exit 1; }
need_file() { [[ -f "$1" ]] || die "missing required file: $1"; }

[[ -d "${fx1_root}" ]] || die "fx1 root does not exist: ${fx1_root}"
worktree_state="clean"
if [[ -n "$(git -C "${repo}" status --porcelain --untracked-files=normal)" ]]; then
    worktree_state="dirty"
    echo "WARNING: CDC-VP working tree is dirty; BUILD_INFO will record this state" >&2
fi
[[ "$(sed -n 's/^CDC_ENABLE_SAURIA_NPU_V4:BOOL=//p' "${repo}/build-soc/CMakeCache.txt")" == "ON" ]] || \
    die "build-soc is not configured with CDC_ENABLE_SAURIA_NPU_V4=ON"

need_file "${vp_pkg}/vp_fx1_full_soc"
need_file "${vp_pkg}/configs/default.yaml"
need_file "${fw_src}/freertos_fx1.elf"
need_file "${fw_src}/freertos_fx1.dis"
need_file "${fw_src}/freertos_fx1.bin"
need_file "${tflm_lib}"

for dst in "${source_dst}" "${build_dst}" "${test_dst}" "${bin_dst}" "${doc_dst}"; do
    mkdir -p "${dst}"
    if find "${dst}" -mindepth 1 -print -quit | grep -q .; then
        die "destination is not empty: ${dst}"
    fi
done

stage="$(mktemp -d /tmp/fx1-v2-package.XXXXXX)"
trap 'rm -rf "${stage}"' EXIT
s_source="${stage}/sw/bootloader/sources/${version}"
s_build="${stage}/sw/bootloader/build/${version}"
s_test="${stage}/sw/bootloader/test/${version}"
s_bin="${stage}/vp/bin/${version}"
s_doc="${stage}/vp/doc/${version}"
mkdir -p "${s_source}" "${s_build}" "${s_test}" "${s_bin}" "${s_doc}"

echo ">> firmware and NPU integration sources"
for item in FreeRTOSConfig.h linker.ld README.md bsp include models src startup tests; do
    cp -a "${fw_src}/${item}" "${s_source}/"
done
cp "${tmpl}/Makefile" "${s_source}/Makefile"
mkdir -p "${s_source}/common/include"
cp -a "${repo}/fw/common/include/soc" "${s_source}/common/include/"

mkdir -p "${s_source}/vp_integration_reference"
cp -a "${repo}/components/npu_tlm_v4_model" \
    "${s_source}/vp_integration_reference/"
mkdir -p "${s_source}/vp_integration_reference/VP_FX1_Full_SoC"
cp -a "${repo}/platforms/VP_FX1_Full_SoC/CMakeLists.txt" \
      "${repo}/platforms/VP_FX1_Full_SoC/src" \
      "${repo}/platforms/VP_FX1_Full_SoC/configs" \
      "${s_source}/vp_integration_reference/VP_FX1_Full_SoC/"
cp "${repo}/CMakeLists.txt" \
    "${s_source}/vp_integration_reference/CDC-VP_ROOT_CMakeLists.txt"
cp "${repo}/components/CMakeLists.txt" \
    "${s_source}/vp_integration_reference/COMPONENTS_CMakeLists.txt"

echo ">> selected pinned FreeRTOS source"
fr_dst="${s_source}/third_party/FreeRTOS-Kernel"
mkdir -p "${fr_dst}/portable/MemMang" "${fr_dst}/portable/GCC"
cp "${freertos_root}/tasks.c" "${freertos_root}/list.c" \
   "${freertos_root}/queue.c" "${freertos_root}/LICENSE.md" "${fr_dst}/"
cp "${freertos_root}/portable/MemMang/heap_4.c" "${fr_dst}/portable/MemMang/"
cp -a "${freertos_root}/include" "${fr_dst}/"
cp -a "${freertos_root}/portable/GCC/RISC-V" "${fr_dst}/portable/GCC/"
git -C "${freertos_root}" rev-parse HEAD > "${fr_dst}/UPSTREAM_COMMIT"

echo ">> TFLM application header closure"
depfile="${stage}/tflm-app.deps"
export PATH="/opt/toolchains/riscv-none-elf/bin:${PATH}"
command -v riscv-none-elf-g++ >/dev/null || die "riscv-none-elf-g++ not found"
(
    cd "${fw_src}"
    riscv-none-elf-g++ -march=rv32imac_zicsr -mabi=ilp32 \
        -DDEMO_NPU=1 -DDEMO_TFLM=1 -DTF_LITE_STATIC_MEMORY \
        -DTF_LITE_STRIP_ERROR_STRINGS -DNDEBUG \
        -I. -Iinclude -Imodels -I../common/include \
        -I../../third_party/FreeRTOS-Kernel/include \
        -I../../third_party/FreeRTOS-Kernel/portable/GCC/RISC-V \
        -I../../third_party/FreeRTOS-Kernel/portable/GCC/RISC-V/chip_specific_extensions/RISCV_MTIME_CLINT_no_extensions \
        -I../../third_party/tflite-micro \
        -I../../third_party/tflite-micro/tensorflow/lite/micro/tools/make/downloads/flatbuffers/include \
        -I../../third_party/tflite-micro/tensorflow/lite/micro/tools/make/downloads/gemmlowp \
        -I../../third_party/tflite-micro/tensorflow/lite/micro/tools/make/downloads/ruy \
        -std=c++17 -MM src/tflm_cpu.cpp src/tflm_npu_conv.cpp
) > "${depfile}"

tflm_dst="${s_source}/third_party/tflite-micro"
mkdir -p "${tflm_dst}"
deps_flat="$(tr '\\\n' '  ' < "${depfile}")"
for token in ${deps_flat}; do
    case "${token}" in
        *third_party/tflite-micro/*)
            abs="$(cd "${fw_src}" && realpath -m "${token}")"
            [[ "${abs}" == "${tflm_root}/"* ]] || continue
            [[ -f "${abs}" ]] || die "TFLM dependency does not exist: ${abs}"
            rel="${abs#${tflm_root}/}"
            mkdir -p "${tflm_dst}/$(dirname "${rel}")"
            cp "${abs}" "${tflm_dst}/${rel}"
            ;;
    esac
done
tflm_model_rel="tensorflow/lite/micro/examples/hello_world/models/hello_world_int8.tflite"
mkdir -p "${tflm_dst}/$(dirname "${tflm_model_rel}")"
cp "${tflm_root}/${tflm_model_rel}" "${tflm_dst}/${tflm_model_rel}"
cp "${tflm_root}/LICENSE" "${tflm_dst}/LICENSE"
git -C "${tflm_root}" rev-parse HEAD > "${tflm_dst}/UPSTREAM_COMMIT"
mkdir -p "${tflm_dst}/prebuilt"
cp "${tflm_lib}" "${tflm_dst}/prebuilt/libtensorflow-microlite.a"

echo ">> firmware build outputs"
cp "${fw_src}/freertos_fx1.elf" "${fw_src}/freertos_fx1.dis" \
   "${fw_src}/freertos_fx1.bin" "${s_build}/"
riscv-none-elf-objdump -h -t "${fw_src}/freertos_fx1.elf" \
    > "${s_build}/freertos_fx1.sections.txt"

echo ">> VP runtime and test runner"
cp "${vp_pkg}/vp_fx1_full_soc" "${s_bin}/"
cp "${vp_pkg}"/libsystemc.so* "${s_bin}/"
cp "${repo}/platforms/VP_FX1_Full_SoC/configs/default.yaml" "${s_bin}/"
cp "${tmpl}/run_vp.sh" "${s_test}/run_vp.sh"
cp "${fw_src}/tests/cli_smoke.txt" "${s_test}/"
chmod +x "${s_test}/run_vp.sh" "${s_bin}/vp_fx1_full_soc"

echo ">> documentation and build identity"
cp "${tmpl}/PACKAGE_README.md" "${s_doc}/README.md"
cp "${fw_src}/README.md" "${s_doc}/FREERTOS_NPU_TFLM.md"
cp "${repo}/components/npu_tlm_v4_model/README.md" "${s_doc}/NPU_TLM_V4_MODEL.md"
cp "${repo}/docs/CDC-VP_VIRTUAL_SoC_PLATFORM.md" "${s_doc}/VP_PLATFORM.md"
cp "${repo}/docs/peripheral_memory_map.md" "${s_doc}/PERIPHERAL_MEMORY_MAP.md"
cp "${repo}/docs/interrupt_modeling_policy.md" "${s_doc}/INTERRUPT_MODELING_POLICY.md"

commit="$(git -C "${repo}" rev-parse HEAD)"
built_utc="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
host_gcc="$(/usr/bin/gcc -dumpfullversion)"
host_gxx="$(/usr/bin/g++ --version | head -n 1)"
riscv_gcc="$(riscv-none-elf-gcc --version | head -n 1)"
freertos_commit="$(git -C "${freertos_root}" rev-parse HEAD)"
tflm_commit="$(git -C "${tflm_root}" rev-parse HEAD)"
cat > "${s_doc}/BUILD_INFO.txt" <<EOF
Package             : ${version}
Built UTC           : ${built_utc}
CDC-VP commit       : ${commit}
CDC-VP worktree     : ${worktree_state}
VP build type       : $(sed -n 's/^CMAKE_BUILD_TYPE:STRING=//p' "${repo}/build-soc/CMakeCache.txt")
NPU v4              : enabled
Host GCC            : ${host_gcc}
Host G++            : ${host_gxx}
RISC-V GCC          : ${riscv_gcc}
FreeRTOS commit     : ${freertos_commit}
TFLM commit         : ${tflm_commit}
Firmware options    : NPU=1 TFLM=1, rv32imac_zicsr/ilp32
VP SHA-256          : $(sha256sum "${s_bin}/vp_fx1_full_soc" | awk '{print $1}')
Firmware SHA-256    : $(sha256sum "${s_build}/freertos_fx1.elf" | awk '{print $1}')
TFLM lib SHA-256    : $(sha256sum "${tflm_dst}/prebuilt/libtensorflow-microlite.a" | awk '{print $1}')
EOF

(
    cd "${stage}"
    find sw vp -type f ! -path "vp/doc/${version}/SHA256SUMS" -print0 \
        | sort -z | xargs -0 sha256sum > "vp/doc/${version}/SHA256SUMS"
    sha256sum -c "vp/doc/${version}/SHA256SUMS" >/dev/null
)

echo ">> installing staged package into ${fx1_root}"
cp -a "${s_source}/." "${source_dst}/"
cp -a "${s_build}/." "${build_dst}/"
cp -a "${s_test}/." "${test_dst}/"
cp -a "${s_bin}/." "${bin_dst}/"
cp -a "${s_doc}/." "${doc_dst}/"

echo ">> verifying installed manifest"
(
    cd "${fx1_root}"
    sha256sum -c "vp/doc/${version}/SHA256SUMS" >/dev/null
)

if git -C "${fx1_root}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    for trackable in \
        "sw/bootloader/sources/${version}/Makefile" \
        "sw/bootloader/build/${version}/freertos_fx1.elf" \
        "sw/bootloader/sources/${version}/third_party/tflite-micro/prebuilt/libtensorflow-microlite.a" \
        "sw/bootloader/test/${version}/run_vp.sh" \
        "vp/bin/${version}/vp_fx1_full_soc" \
        "vp/doc/${version}/README.md"; do
        if git -C "${fx1_root}" check-ignore -q "${trackable}"; then
            die "packaged file is hidden by fx1 .gitignore: ${trackable}"
        fi
    done
fi

echo ">> package complete"
echo "   source: ${source_dst}"
echo "   build : ${build_dst}"
echo "   test  : ${test_dst}/run_vp.sh"
echo "   bin   : ${bin_dst}"
echo "   doc   : ${doc_dst}"
