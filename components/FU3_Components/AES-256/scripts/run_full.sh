#!/usr/bin/env bash
# Run from Linux/WSL. Build artifacts remain under the repository build/ tree.
set -euo pipefail
component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
repo=$(cd -- "$component/../../.." && pwd)
systemc_prefix=${SYSTEMC_HOME:-/opt/systemc-2.3.4}
jobs=${JOBS:-4}
test_status=0

run_suite() {
  local name=$1
  shift
  local build_dir="$repo/build/$name"
  cmake -S "$component" -B "$build_dir" \
    "-DSYSTEMC_HOME=$systemc_prefix" -DFU3_AES_BUILD_TESTS=ON \
    -DFU3_AES_BUS_TESTS=ON -DFU3_AES_ORACLE_TESTS=ON "$@"
  cmake --build "$build_dir" -j "$jobs"
  if [[ "$name" == fu3-aes ]]; then
    # Remove only the two generated counter files, so coverage belongs to this
    # execution rather than accumulating results from previous source versions.
    cmake -E rm -f "$build_dir/CMakeFiles/fu3_aes.dir/src/aes_core.cpp.gcda" \
      "$build_dir/CMakeFiles/fu3_aes.dir/src/aes_controller.cpp.gcda"
  fi
  ctest --test-dir "$build_dir" --output-on-failure || test_status=1
}

run_suite fu3-aes -DCMAKE_BUILD_TYPE=Debug -DFU3_AES_COVERAGE=ON \
  '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic -Werror'
run_suite fu3-aes-ubsan -DCMAKE_BUILD_TYPE=Debug -DFU3_AES_COVERAGE=OFF \
  '-DCMAKE_CXX_FLAGS=-fsanitize=undefined -fno-sanitize-recover=undefined -Wall -Wextra -Wpedantic -Werror'
run_suite fu3-aes-release -DCMAKE_BUILD_TYPE=Release -DFU3_AES_COVERAGE=OFF \
  '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic -Werror'

mkdir -p "$repo/build/fu3-aes/coverage"
cd -- "$repo/build/fu3-aes/coverage"
gcov -b -c ../CMakeFiles/fu3_aes.dir/src/aes_core.cpp.gcda \
  ../CMakeFiles/fu3_aes.dir/src/aes_controller.cpp.gcda > gcov.log
exit "$test_status"
