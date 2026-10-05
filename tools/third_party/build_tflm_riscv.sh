#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Build the pinned TFLite-Micro microlite archive for the FX1 RV32IMAC
# firmware. Run setup_third_party.sh first.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
TFLM_ROOT="${REPO_ROOT}/third_party/tflite-micro"
TFLM_COMMIT="096563546742ba81adb6f012ab718d196a48e02d"
TFLM_GENDIR="${TFLM_ROOT}/gen/riscv32_generic_rv32imac_zicsr_default_gcc"
TFLM_LIB="${TFLM_GENDIR}/lib/libtensorflow-microlite.a"

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:${PATH}

# shellcheck disable=SC1091
source "${SCRIPT_DIR}/setup_env.sh" >/dev/null

echo "host gcc: $(${CC} -dumpfullversion)"
echo "host g++: $(${CXX} --version | head -n 1)"
echo "riscv g++: $(riscv-none-elf-g++ --version | head -n 1)"

if [[ ! -d "${TFLM_ROOT}/.git" ]]; then
    echo "ERROR: ${TFLM_ROOT} is missing; run setup_third_party.sh first" >&2
    exit 1
fi

actual_commit="$(git -C "${TFLM_ROOT}" rev-parse HEAD)"
if [[ "${actual_commit}" != "${TFLM_COMMIT}" ]]; then
    echo "ERROR: TFLite-Micro is at ${actual_commit}, expected ${TFLM_COMMIT}" >&2
    exit 1
fi

# Do not use upstream's riscv32_generic target include: it hard-codes an old
# SiFive compiler download and an external printf package. CDC-VP already pins
# its xPack compiler, and firmware emits its own deterministic UART diagnostics.
#
# GCC 15's libstdc++ intentionally rejects <string>/<vector> under
# -ffreestanding, while generated FlatBuffers schema headers include them even
# though the microlite runtime path does not allocate STL containers. Compile
# the library with hosted headers, then link it into the -nostdlib firmware.
tflm_cflags=(
    -march=rv32imac_zicsr
    -mabi=ilp32
    -mcmodel=medany
    -Os
    -fno-pic
    -fno-builtin
    -ffunction-sections
    -fdata-sections
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -DTF_LITE_STATIC_MEMORY
    -DTF_LITE_DISABLE_X86_NEON
    -DTF_LITE_STRIP_ERROR_STRINGS
    -DNDEBUG
    -funsigned-char
)
tflm_cxxflags=(
    "${tflm_cflags[@]}"
    -std=c++17
    -fno-rtti
    -fno-exceptions
    -fno-threadsafe-statics
    -fno-use-cxa-atexit
    -Wno-register
)

make -C "${TFLM_ROOT}" -j"${JOBS:-$(nproc)}" \
    -f tensorflow/lite/micro/tools/make/Makefile \
    TARGET=riscv32_generic \
    TARGETS_WITHOUT_MAKEFILES=riscv32_generic \
    TARGET_ARCH=rv32imac_zicsr \
    TARGET_TOOLCHAIN_ROOT="${RISCV_HOME}/bin/" \
    TARGET_TOOLCHAIN_PREFIX=riscv-none-elf- \
    CXXFLAGS="${tflm_cxxflags[*]}" \
    CCFLAGS="${tflm_cflags[*]}" \
    microlite

if [[ ! -f "${TFLM_LIB}" ]]; then
    echo "ERROR: microlite archive was not produced: ${TFLM_LIB}" >&2
    exit 1
fi

echo "TFLite-Micro RV32 archive: ${TFLM_LIB}"
riscv-none-elf-size -A "${TFLM_LIB}" 2>/dev/null | tail -n 1 || true
