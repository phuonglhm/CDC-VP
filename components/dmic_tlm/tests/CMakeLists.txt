add_executable(test_dmic_tlm
    test_dmic_tlm.cpp
    testbench.cpp)
target_include_directories(test_dmic_tlm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_dmic_tlm PRIVATE
    cdc::components::dmic_tlm
    SystemC::systemc)
add_test(NAME dmic_tlm COMMAND test_dmic_tlm)
