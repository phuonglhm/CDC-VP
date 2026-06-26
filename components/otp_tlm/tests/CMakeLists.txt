add_executable(test_otp_tlm
    tb.cpp)
target_link_libraries(test_otp_tlm PRIVATE
    cdc::components::otp_tlm
    SystemC::systemc)
add_test(NAME otp_tlm COMMAND test_otp_tlm)
