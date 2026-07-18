#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
THIRD_PARTY_DIR="${REPO_ROOT}/third_party"

RISCV_VP_URL="https://github.com/agra-uni-bremen/riscv-vp.git"
RISCV_VP_COMMIT="48b2f5877b2368cc466fb0da155db349e676c0b0"

FREERTOS_URL="https://github.com/FreeRTOS/FreeRTOS-Kernel.git"
FREERTOS_COMMIT="0adc196d4bd52a2d91102b525b0aafc1e14a2386"   # V11.2.0

die() {
    echo "ERROR: $*" >&2
    exit 1
}

ensure_clean_repo() {
    local dir="$1"
    local name="$2"

    if [[ ! -d "${dir}/.git" ]]; then
        die "${name} exists but is not a Git repository: ${dir}"
    fi

    if ! git -C "${dir}" diff --quiet || ! git -C "${dir}" diff --cached --quiet; then
        die "${name} has local changes. Commit/stash them before running this script."
    fi
}

setup_repo() {
    local name="$1"
    local url="$2"
    local commit="$3"
    local dir="$4"

    echo "==> ${name}"

    if [[ ! -d "${dir}" ]]; then
        echo "Cloning ${url} -> ${dir}"
        git clone "${url}" "${dir}"
    fi

    ensure_clean_repo "${dir}" "${name}"

    local current_url
    current_url="$(git -C "${dir}" remote get-url origin)"
    if [[ "${current_url}" != "${url}" ]]; then
        echo "WARNING: ${name} origin is ${current_url}, expected ${url}" >&2
    fi

    if git -C "${dir}" cat-file -e "${commit}^{commit}" 2>/dev/null; then
        :
    else
        echo "Fetching pinned commit ${commit}"
        git -C "${dir}" fetch --tags origin
    fi

    git -C "${dir}" checkout --detach "${commit}"
    echo "${name}: checked out $(git -C "${dir}" rev-parse --short=12 HEAD)"
}

mkdir -p "${THIRD_PARTY_DIR}"

setup_repo "Bremen riscv-vp" "${RISCV_VP_URL}" "${RISCV_VP_COMMIT}" \
    "${THIRD_PARTY_DIR}/riscv-vp"

setup_repo "FreeRTOS-Kernel" "${FREERTOS_URL}" "${FREERTOS_COMMIT}" \
    "${THIRD_PARTY_DIR}/FreeRTOS-Kernel"

echo
echo "Third-party dependencies are ready."
echo "Next:"
echo "  source ./tools/third_party/setup_env.sh"
echo "  cmake --preset debug"
echo "  cmake --build --preset debug"
