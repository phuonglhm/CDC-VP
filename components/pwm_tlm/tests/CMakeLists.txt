add_executable(test_pwm_tlm
    tb.cpp)
target_link_libraries(test_pwm_tlm PRIVATE
    cdc::components::pwm_tlm
    SystemC::systemc)
add_test(NAME pwm_tlm COMMAND test_pwm_tlm)
