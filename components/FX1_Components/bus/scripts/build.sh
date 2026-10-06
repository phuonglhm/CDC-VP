#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
args=(-S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUS_SYSTEM_BUILD_TESTS=ON)
if [[ -n "${SYSTEMC_HOME:-}" ]]; then
    args+=("-DCMAKE_PREFIX_PATH=$SYSTEMC_HOME")
fi
cmake "${args[@]}"
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
