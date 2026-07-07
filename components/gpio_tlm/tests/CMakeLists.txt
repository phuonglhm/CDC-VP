add_executable(test_gpio_tlm test_gpio_tlm.cpp)
target_link_libraries(test_gpio_tlm PRIVATE
    cdc::components::gpio_tlm
    SystemC::systemc)
add_test(NAME gpio_tlm COMMAND test_gpio_tlm)
