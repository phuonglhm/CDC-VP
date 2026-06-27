add_executable(test_rtc_tlm test_rtc_tlm.cpp)
target_link_libraries(test_rtc_tlm PRIVATE
    cdc::components::rtc_tlm
    SystemC::systemc)
target_include_directories(test_rtc_tlm PRIVATE ${CMAKE_SOURCE_DIR}/tests/support)
add_test(NAME rtc_tlm COMMAND test_rtc_tlm)
