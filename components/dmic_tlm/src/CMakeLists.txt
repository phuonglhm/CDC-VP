add_executable(test_dmic_tlm main.cpp testbench.cpp)
target_link_libraries(test_dmic_tlm PRIVATE
    cdc::components::dmic_tlm
    SystemC::systemc)
target_include_directories(test_dmic_tlm PRIVATE ${CMAKE_SOURCE_DIR}/tests/support)
add_test(NAME dmic_tlm COMMAND test_dmic_tlm)
