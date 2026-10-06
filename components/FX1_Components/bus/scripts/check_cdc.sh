#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${CDC_VP_SOURCE_DIR:?Set CDC_VP_SOURCE_DIR to the CDC-VP checkout}"
cmake -S tests/cdc_integration -B build-cdc -DCDC_VP_SOURCE_DIR="$CDC_VP_SOURCE_DIR"
cmake --build build-cdc --parallel 2
ctest --test-dir build-cdc --output-on-failure
cmake --install build-cdc --prefix "$PWD/build-cdc/install"
cmake -S tests/installed_consumer -B build-installed-consumer \
  -DCDC_COMPONENTS_PREFIX="$PWD/build-cdc/install"
cmake --build build-installed-consumer --parallel 2
ctest --test-dir build-installed-consumer --output-on-failure
