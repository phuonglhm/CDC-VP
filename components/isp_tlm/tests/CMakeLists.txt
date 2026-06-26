add_executable(test_isp_tlm test_isp_tlm.cpp)
target_link_libraries(test_isp_tlm PRIVATE
    cdc::components::isp_tlm
    SystemC::systemc)
target_include_directories(test_isp_tlm PRIVATE ${CMAKE_SOURCE_DIR}/tests/support)
add_test(NAME isp_tlm COMMAND test_isp_tlm)
