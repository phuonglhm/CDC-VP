add_executable(test_clint_tlm test_clint_tlm.cpp)
target_link_libraries(test_clint_tlm PRIVATE
    cdc::components::clint_tlm
    cdc::components::memory_tlm
    SystemC::systemc)
target_include_directories(test_clint_tlm PRIVATE ${CMAKE_SOURCE_DIR}/tests/support)
add_test(NAME clint_tlm COMMAND test_clint_tlm)
