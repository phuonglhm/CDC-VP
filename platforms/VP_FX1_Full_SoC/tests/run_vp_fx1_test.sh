#!/usr/bin/env bash
#
# Build-and-test driver for the VP_FX1 full SoC platform.
#
#   ./run_vp_fx1_test.sh                 # build + smoke + firmware test
#   BUILD_DIR=build-fx1 ./run_vp_fx1_test.sh
#   ./run_vp_fx1_test.sh --no-build      # skip configure/build, just run
#
# Exit code = number of failed checks (0 = all passed).

set -u

# ── Locate the repo root (this script lives in platforms/VP_FX1_Full_SoC/tests) ─
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
cd "${ROOT}"

BUILD_DIR="${BUILD_DIR:-build}"
CFG="platforms/VP_FX1_Full_SoC/configs/default.yaml"
EXE="${BUILD_DIR}/platforms/VP_FX1_Full_SoC/vp_fx1_full_soc"
FW_DIR="fw/hello_baremetal_riscv"
FW="${FW_DIR}/hello.elf"
NPU_FW_DIR="fw/npu_v4_irq_riscv"
NPU_FW="${NPU_FW_DIR}/npu_v4_irq.elf"
DO_BUILD=1
[ "${1:-}" = "--no-build" ] && DO_BUILD=0

# ── Pretty output ──────────────────────────────────────────────────────────────
GREEN='\033[1;32m'; RED='\033[1;31m'; YEL='\033[1;33m'; NC='\033[0m'
FAILS=0
pass() { echo -e "${GREEN}[PASS]${NC} $1"; }
fail() { echo -e "${RED}[FAIL]${NC} $1"; FAILS=$((FAILS + 1)); }
info() { echo -e "${YEL}[INFO]${NC} $1"; }

# ── Environment ────────────────────────────────────────────────────────────────
if [ -f tools/third_party/setup_env.sh ]; then
    # shellcheck disable=SC1091
    source tools/third_party/setup_env.sh >/dev/null 2>&1 || true
fi
export CC="${CC:-/usr/bin/gcc}"
export CXX="${CXX:-/usr/bin/g++}"
export PATH="/usr/bin:/bin:${PATH}"
NPU_ENABLED=0

echo "================ VP_FX1 Full SoC test ================"
info "repo : ${ROOT}"
info "build: ${BUILD_DIR}"
info "host compiler sanity"
"${CC}" -dumpfullversion
"${CXX}" --version | head -n 1

# ── 1. Configure + build ───────────────────────────────────────────────────────
if [ "${DO_BUILD}" = 1 ]; then
    NPU_CMAKE_ARGS=(-DCDC_ENABLE_SAURIA_NPU_V4=OFF)
    if [ -n "${SAURIA_NPU_ROOT:-}" ]; then
        if [ ! -f "${SAURIA_NPU_ROOT}/npu_top.h" ]; then
            fail "SAURIA_NPU_ROOT does not contain npu_top.h"
            exit "${FAILS}"
        fi
        NPU_CMAKE_ARGS=(
            -DCDC_ENABLE_SAURIA_NPU_V4=ON
            "-DSAURIA_NPU_ROOT=${SAURIA_NPU_ROOT}"
        )
    fi

    info "configuring + building (riscv_vp, Debug)..."
    if ! cmake -S . -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Debug \
            -DCDC_CPU_BACKEND=riscv_vp \
            -DCDC_BUILD_CUSTOM_SOC=ON -DCDC_BUILD_TESTS=OFF \
            -DCDC_BUILD_MINI_TLM=OFF -DCDC_BUILD_CPU_EVAL=OFF \
            "${NPU_CMAKE_ARGS[@]}" >/tmp/vp_fx1_cfg.log 2>&1; then
        fail "cmake configure (see /tmp/vp_fx1_cfg.log)"; echo; exit "${FAILS}"
    fi
    if cmake --build "${BUILD_DIR}" --target vp_fx1_full_soc -j"$(nproc)" >/tmp/vp_fx1_build.log 2>&1; then
        pass "build vp_fx1_full_soc"
    else
        fail "build vp_fx1_full_soc (see /tmp/vp_fx1_build.log)"
        grep -iE 'error:|fatal error' /tmp/vp_fx1_build.log | head -5
        exit "${FAILS}"
    fi
fi

if grep -q '^CDC_ENABLE_SAURIA_NPU_V4:BOOL=ON$' \
        "${BUILD_DIR}/CMakeCache.txt" 2>/dev/null; then
    NPU_ENABLED=1
    info "optional SAURIA NPU v4: enabled"
else
    info "optional SAURIA NPU v4: disabled"
fi

if [ ! -x "${EXE}" ]; then
    fail "executable not found: ${EXE}"; echo; exit "${FAILS}"
fi

# ── 2. Smoke test: elaboration only (no firmware) ──────────────────────────────
info "smoke test (--sim-ms 0)..."
SMOKE="$(timeout 60s "${EXE}" -c "${CFG}" --sim-ms 0 2>&1)"
SRC=$?
if [ "${SRC}" -eq 0 ] && ! echo "${SMOKE}" | grep -qiE 'Error:|not bound|more than one driver'; then
    pass "elaboration clean (all bindings OK)"
else
    fail "elaboration"
    echo "${SMOKE}" | grep -iE 'Error:|not bound|driver' | head -5
fi

# ── 3. Firmware test: boot hello.elf, expect UART console output ────────────────
export PATH="${RISCV_HOME:-/opt/toolchains/riscv-none-elf}/bin:/usr/bin:/bin:${PATH}"
info "building firmware (${FW_DIR})..."
if ! make -C "${FW_DIR}" >/tmp/vp_fx1_fw.log 2>&1; then
    fail "firmware build (see /tmp/vp_fx1_fw.log)"
elif [ ! -f "${FW}" ]; then
    fail "firmware ELF not produced: ${FW}"
else
    pass "firmware build (${FW})"
    info "running firmware (--sim-ms 2)..."
    OUT="$(timeout 60s "${EXE}" -c "${CFG}" --fw "${FW}" --sim-ms 2 2>&1)"
    if echo "${OUT}" | grep -q "Hello from RISC-V"; then
        pass "firmware console output: 'Hello from RISC-V'"
    else
        fail "firmware console output (expected 'Hello from RISC-V')"
        echo "${OUT}" | tail -6
    fi
fi

# ── 4. Optional NPU E2E: RAM master + SAURIA core + PLIC source 17 ───────────
if [ "${NPU_ENABLED}" = 1 ]; then
    info "building NPU firmware (${NPU_FW_DIR})..."
    if ! make -C "${NPU_FW_DIR}" >/tmp/vp_fx1_npu_fw.log 2>&1; then
        fail "NPU firmware build (see /tmp/vp_fx1_npu_fw.log)"
    elif [ ! -f "${NPU_FW}" ]; then
        fail "NPU firmware ELF not produced: ${NPU_FW}"
    else
        pass "NPU firmware build (${NPU_FW})"
        info "running NPU firmware (--sim-ms 2)..."
        NPU_OUT="$(timeout 60s "${EXE}" -c "${CFG}" --fw "${NPU_FW}" --sim-ms 2 2>&1)"
        if echo "${NPU_OUT}" | grep -q "NPU IRQ17" &&
           echo "${NPU_OUT}" | grep -q "NPU PASS"; then
            pass "NPU GEMM DMA + PLIC IRQ17 end-to-end"
        else
            fail "NPU end-to-end output (expected IRQ17 and PASS)"
            echo "${NPU_OUT}" | tail -12
        fi
    fi
else
    info "skipping NPU E2E (public build has optional NPU disabled)"
fi

# ── Summary ────────────────────────────────────────────────────────────────────
echo "-----------------------------------------------------"
if [ "${FAILS}" -eq 0 ]; then
    echo -e "${GREEN}ALL CHECKS PASSED${NC}"
else
    echo -e "${RED}${FAILS} CHECK(S) FAILED${NC}"
fi
echo "====================================================="
exit "${FAILS}"
