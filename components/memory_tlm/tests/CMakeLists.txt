add_executable(test_memory_tlm test_memory_tlm.cpp)
target_link_libraries(test_memory_tlm PRIVATE
    cdc::components::memory_tlm
    SystemC::systemc)
target_include_directories(test_memory_tlm PRIVATE ${CMAKE_SOURCE_DIR}/tests/support)
add_test(NAME memory_tlm COMMAND test_memory_tlm)
