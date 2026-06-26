add_executable(test_uart2_tlm
    test_uart2_tlm.cpp
    master_tb.cpp
    top.cpp)
target_include_directories(test_uart2_tlm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_uart2_tlm PRIVATE
    cdc::components::uart2_tlm
    SystemC::systemc)
add_test(NAME uart2_tlm COMMAND test_uart2_tlm)
