#Author: QuanNH107
#Verified by: HoangV11

add_executable(test_wdt_tlm
    test_wdt_tlm.cpp
    test.cpp)
target_link_libraries(test_wdt_tlm PRIVATE
    cdc::components::wdt_tlm
    SystemC::systemc)
target_include_directories(test_wdt_tlm PRIVATE ${CMAKE_SOURCE_DIR}/tests/support)
add_test(NAME wdt_tlm COMMAND test_wdt_tlm)
