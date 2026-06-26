add_executable(test_flash_nor_tlm test_flash_nor_tlm.cpp)
target_link_libraries(test_flash_nor_tlm PRIVATE
    cdc::components::flash_nor_tlm
    SystemC::systemc)
target_include_directories(test_flash_nor_tlm PRIVATE ${CMAKE_SOURCE_DIR}/tests/support)
add_test(NAME flash_nor_tlm COMMAND test_flash_nor_tlm)
