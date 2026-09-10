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

# Decision record D6's host-backing policy. It is a plain-C++ test on purpose:
# the property it proves — that a 1 GiB window costs a few pages until it is
# written — must hold before any SystemC target is wrapped around it.
add_executable(test_tpu_v3_sparse_memory test_sparse_memory.cpp)
target_link_libraries(test_tpu_v3_sparse_memory PRIVATE
    cdc::components::tpu_v3_common)
add_test(NAME tpu_v3_sparse_memory COMMAND test_tpu_v3_sparse_memory)
set_tests_properties(tpu_v3_sparse_memory PROPERTIES LABELS "tpu_v3;unit")

# D28's two profiles, and the gate that stops a profile label outrunning the
# machine that carries it. Plain C++: the profile is a value object, so its
# contract must hold before any component is built around it.
add_executable(test_tpu_v3_neo_lite_profile test_neo_lite_profile.cpp)
target_link_libraries(test_tpu_v3_neo_lite_profile PRIVATE
    cdc::components::tpu_v3_common)
add_test(NAME tpu_v3_neo_lite_profile COMMAND test_tpu_v3_neo_lite_profile)
set_tests_properties(tpu_v3_neo_lite_profile PROPERTIES
    LABELS "tpu_v3;unit;d28;g5;wp1")
