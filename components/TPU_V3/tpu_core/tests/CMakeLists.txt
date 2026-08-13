# SPDX-License-Identifier: Apache-2.0
#
# The Phase 3 gate for the D15 interconnect split. Each test has a timeout:
# a concurrency test that hangs is a failure, not a suite that never finishes.

add_executable(test_tpu_v3_local_sram_fabric test_neo_local_sram_fabric.cpp)
target_link_libraries(test_tpu_v3_local_sram_fabric PRIVATE
    cdc::components::tpu_v3_tpu_core)
add_test(NAME tpu_v3_local_sram_fabric COMMAND test_tpu_v3_local_sram_fabric)
set_tests_properties(tpu_v3_local_sram_fabric PROPERTIES
    LABELS "tpu_v3;unit;fabric"
    TIMEOUT 300)

add_executable(test_tpu_v3_control_fabric test_neo_control_fabric.cpp)
target_link_libraries(test_tpu_v3_control_fabric PRIVATE
    cdc::components::tpu_v3_tpu_core)
add_test(NAME tpu_v3_control_fabric COMMAND test_tpu_v3_control_fabric)
set_tests_properties(tpu_v3_control_fabric PROPERTIES
    LABELS "tpu_v3;unit;fabric"
    TIMEOUT 120)

add_executable(test_tpu_v3_external_bridge test_neo_external_bridge.cpp)
target_link_libraries(test_tpu_v3_external_bridge PRIVATE
    cdc::components::tpu_v3_tpu_core)
add_test(NAME tpu_v3_external_bridge COMMAND test_tpu_v3_external_bridge)
set_tests_properties(tpu_v3_external_bridge PROPERTIES
    LABELS "tpu_v3;unit;fabric"
    TIMEOUT 300)
