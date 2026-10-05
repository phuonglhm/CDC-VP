#!/usr/bin/env bash
# Source this file in every new terminal before building cdc-vp:
#
#   source ./tools/third_party/setup_env.sh
#
# It intentionally modifies the current shell environment.

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    echo "ERROR: setup_env.sh must be sourced, not executed." >&2
    echo "Use: source ./tools/third_party/setup_env.sh" >&2
    exit 1
fi

_cdc_tp_script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export CDC_VP_ROOT="$(cd "${_cdc_tp_script_dir}/../.." && pwd)"

# Avoid recursive shell-level issues seen on this host.
export SHLVL=1

# Force native host compilers before EDA toolchain wrappers in PATH.
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++

# RISC-V bare-metal toolchain used by fw/* Makefiles.
export RISCV_HOME="${RISCV_HOME:-/opt/toolchains/riscv-none-elf}"

case ":${PATH}:" in
    *":${RISCV_HOME}/bin:"*) ;;
    *) export PATH="${RISCV_HOME}/bin:${PATH}" ;;
esac

# Keep /usr/bin and /bin before vendor EDA paths for cmake/compiler discovery.
export PATH="/usr/bin:/bin:${PATH}"

echo "CDC_VP_ROOT=${CDC_VP_ROOT}"
echo "CC=${CC}"
echo "CXX=${CXX}"
echo "RISCV_HOME=${RISCV_HOME}"
echo "cmake=$(command -v cmake || true)"
echo "riscv gcc=$(command -v riscv-none-elf-gcc || true)"
