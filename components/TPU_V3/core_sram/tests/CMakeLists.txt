# SPDX-License-Identifier: Apache-2.0

add_executable(test_tpu_v3_core_sram test_core_sram.cpp)
target_link_libraries(test_tpu_v3_core_sram PRIVATE
    cdc::components::tpu_v3_core_sram)
add_test(NAME tpu_v3_core_sram COMMAND test_tpu_v3_core_sram)
set_tests_properties(tpu_v3_core_sram PROPERTIES
    LABELS "tpu_v3;unit"
    TIMEOUT 120)
