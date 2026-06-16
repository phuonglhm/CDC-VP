add_executable(test_plic_tlm test_plic_tlm.cpp)
target_link_libraries(test_plic_tlm PRIVATE
    cdc::components::plic_tlm
    cdc::components::memory_tlm
    SystemC::systemc)
target_include_directories(test_plic_tlm PRIVATE ${CMAKE_SOURCE_DIR}/tests/support)
add_test(NAME plic_tlm COMMAND test_plic_tlm)
