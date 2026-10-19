#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
source_dir=components/FU3_Components/ADC
systemc_prefix=${SYSTEMC_HOME:-/opt/systemc-2.3.4}
jobs=${JOBS:-4}
for profile in debug release ubsan; do
  case "$profile" in
    debug) build_dir=build/fu3-adc; build_type=Debug; flags=-Werror; coverage=ON ;;
    release) build_dir=build/fu3-adc-release; build_type=Release; flags=-Werror; coverage=OFF ;;
    ubsan) build_dir=build/fu3-adc-ubsan; build_type=Debug
      flags='-Werror -fsanitize=undefined -fno-sanitize-recover=all'; coverage=OFF ;;
  esac
  mkdir -p "$build_dir"
  cmake -S "$source_dir" -B "$build_dir" -DSYSTEMC_HOME="$systemc_prefix" \
    -DCMAKE_BUILD_TYPE="$build_type" -DCMAKE_CXX_FLAGS="$flags" \
    -DFU3_ADC_COVERAGE="$coverage" -DFU3_ADC_BUILD_TESTS=ON \
    -DFU3_ADC_BUS_TESTS=ON -DFU3_ADC_BUILD_EXAMPLE=ON 2>&1 | tee "$build_dir/configure.log"
  cmake --build "$build_dir" -j "$jobs" 2>&1 | tee "$build_dir/build.log"
  ctest --test-dir "$build_dir" --output-on-failure --output-junit results.xml 2>&1 | tee "$build_dir/tests.log"
done
(
  cd build/fu3-adc
  gcov -b -c CMakeFiles/fu3_adc.dir/src/adc.cpp.gcno > coverage-summary.txt
)
python3 "$source_dir/scripts/report.py" build/fu3-adc/results.xml \
  build/fu3-adc-release/results.xml build/fu3-adc-ubsan/results.xml \
  --output build/fu3-adc/verification-report.json
