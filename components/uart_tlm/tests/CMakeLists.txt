add_executable(test_uart_tlm test_uart_tlm.cpp)
target_link_libraries(test_uart_tlm PRIVATE
    cdc::components::uart_tlm
    SystemC::systemc)
target_include_directories(test_uart_tlm PRIVATE ${CMAKE_SOURCE_DIR}/tests/support)
add_test(NAME uart_tlm COMMAND test_uart_tlm)
