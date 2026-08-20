# SPDX-License-Identifier: Apache-2.0
#
# The Phase 8 chip-fabric gate. It needs neither a CPU backend nor an extracted
# accelerator, so it runs in every TPU_V3 test build rather than only in the
# full one. Timeouts everywhere: a contention test that hangs is a failure, not
# a suite that never finishes.

add_executable(test_tpu_v3_chip_fabric test_chip_local_fabric.cpp)
target_link_libraries(test_tpu_v3_chip_fabric PRIVATE
    cdc::components::tpu_v3_chip_fabric)
add_test(NAME tpu_v3_chip_fabric COMMAND test_tpu_v3_chip_fabric)
set_tests_properties(tpu_v3_chip_fabric PROPERTIES
    LABELS "tpu_v3;unit;fabric"
    TIMEOUT 300)
