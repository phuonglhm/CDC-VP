# SPDX-License-Identifier: Apache-2.0
#
# Unit gates for the benchmark definitions, the golden and the row schema.
# None of them elaborates SystemC: what they prove has to hold before a core is
# built around it.

add_executable(test_tpu_v3_benchmark_case test_benchmark_case.cpp)
target_link_libraries(test_tpu_v3_benchmark_case PRIVATE
    cdc::components::tpu_v3_microbench)
# The firmware's buffer map, so the frozen case tables can be checked against
# the memory they will actually be staged into.
target_include_directories(test_tpu_v3_benchmark_case PRIVATE
    "${CMAKE_SOURCE_DIR}/fw/TPU_V3_NEO_CORE_MICROBENCH/common")
add_test(NAME tpu_v3_benchmark_case COMMAND test_tpu_v3_benchmark_case)
set_tests_properties(tpu_v3_benchmark_case PROPERTIES
    LABELS "tpu_v3;unit;microbench;d27")

add_executable(test_tpu_v3_bench_result_row test_result_row.cpp)
target_link_libraries(test_tpu_v3_bench_result_row PRIVATE
    cdc::components::tpu_v3_microbench)
add_test(NAME tpu_v3_bench_result_row COMMAND test_tpu_v3_bench_result_row)
set_tests_properties(tpu_v3_bench_result_row PROPERTIES
    LABELS "tpu_v3;unit;microbench;d27")

add_executable(test_tpu_v3_bench_sha256 test_sha256.cpp)
target_link_libraries(test_tpu_v3_bench_sha256 PRIVATE
    cdc::components::tpu_v3_microbench)
add_test(NAME tpu_v3_bench_sha256
    COMMAND test_tpu_v3_bench_sha256 "${CMAKE_CURRENT_BINARY_DIR}")
set_tests_properties(tpu_v3_bench_sha256 PROPERTIES
    LABELS "tpu_v3;unit;microbench;d27")
