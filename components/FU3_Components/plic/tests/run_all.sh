#!/usr/bin/env bash
# Build + chạy toàn bộ test PLIC: static và shared, mỗi bản gồm behavior, integration (qua bus FU3) và installed consumer (find_package fu3-plic).
# Dùng: tests/run_all.sh   (đổi SystemC: SYSTEMC_HOME=/path tests/run_all.sh)
set -euo pipefail

test_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source_dir="$(cd -- "${test_dir}/.." && pwd)"
build_root="${FU3_PLIC_BUILD_DIR:-${source_dir}/build}"
systemc_home="${SYSTEMC_HOME:-/usr/local/systemc-2.3.4}"
jobs="${FU3_PLIC_JOBS:-8}"

for mode in static shared; do
    shared=OFF
    if [ "${mode}" = shared ]; then shared=ON; fi
    build_dir="${build_root}/${mode}"
    echo "===== ${mode}"
    cmake -S "${source_dir}" -B "${build_dir}" -DSYSTEMC_HOME="${systemc_home}" \
        -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS="${shared}" -DBUILD_TESTING=ON
    cmake --build "${build_dir}" --parallel "${jobs}"
    ctest --test-dir "${build_dir}" --output-on-failure
    "${build_dir}/plic_behavior_tests"
    "${build_dir}/plic_integration_tests"
    "${build_dir}/intmap_tests"


    cmake --install "${build_dir}" --prefix "${build_dir}/install"
    cmake -S "${source_dir}/tests/consumer" -B "${build_dir}/consumer" \
        -DCMAKE_PREFIX_PATH="${build_dir}/install;${systemc_home}"
    cmake --build "${build_dir}/consumer" --parallel "${jobs}"
    ctest --test-dir "${build_dir}/consumer" --output-on-failure
done
echo "===== PLIC: all tests passed"
