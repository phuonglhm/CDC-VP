# SPDX-License-Identifier: Apache-2.0

add_executable(test_tpu_v3_address_map test_address_map.cpp)
target_link_libraries(test_tpu_v3_address_map PRIVATE
    cdc::components::tpu_v3_common)
add_test(NAME tpu_v3_address_map COMMAND test_tpu_v3_address_map)
set_tests_properties(tpu_v3_address_map PROPERTIES LABELS "tpu_v3;unit")

add_executable(test_tpu_v3_architecture_config test_architecture_config.cpp)
target_link_libraries(test_tpu_v3_architecture_config PRIVATE
    cdc::components::tpu_v3_common)
add_test(NAME tpu_v3_architecture_config
    COMMAND test_tpu_v3_architecture_config)
set_tests_properties(tpu_v3_architecture_config PROPERTIES LABELS "tpu_v3;unit")
