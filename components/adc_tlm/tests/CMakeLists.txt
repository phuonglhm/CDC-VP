add_executable(test_adc_tlm test_adc_tlm.cpp)
target_link_libraries(test_adc_tlm PRIVATE
    cdc::components::adc_tlm
    SystemC::systemc)
target_include_directories(test_adc_tlm PRIVATE ${CMAKE_SOURCE_DIR}/tests/support)
add_test(NAME adc_tlm COMMAND test_adc_tlm)
