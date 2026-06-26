add_executable(test_timer_tlm
    test_timer_tlm.cpp)
target_link_libraries(test_timer_tlm PRIVATE
    cdc::components::timer_tlm
    SystemC::systemc)
add_test(NAME timer_tlm COMMAND test_timer_tlm)
