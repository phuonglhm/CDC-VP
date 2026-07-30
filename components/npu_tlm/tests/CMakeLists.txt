# SPDX-License-Identifier: Apache-2.0

add_executable(test_npu_tlm
    test_npu_tlm.cpp)

target_include_directories(test_npu_tlm PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/support)

target_link_libraries(test_npu_tlm PRIVATE
    cdc::components::npu_tlm
    cdc::components::bus_router
    cdc::components::memory_tlm
    SystemC::systemc)

add_test(NAME npu_tlm COMMAND test_npu_tlm)
