add_executable(test_npu_tlm_v4_model
    test_npu_tlm_v4_model.cpp)

target_include_directories(test_npu_tlm_v4_model PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/support)

target_link_libraries(test_npu_tlm_v4_model PRIVATE
    cdc::components::npu_tlm_v4_model
    cdc::components::bus_router
    cdc::components::memory_tlm
    SystemC::systemc)

add_test(NAME npu_tlm_v4_model COMMAND test_npu_tlm_v4_model)
