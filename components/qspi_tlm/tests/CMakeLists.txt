add_executable(test_qspi_tlm test_qspi_tlm.cpp)
target_link_libraries(test_qspi_tlm PRIVATE
    cdc::components::qspi_tlm
    cdc::components::flash_nor_tlm
    SystemC::systemc)
target_include_directories(test_qspi_tlm PRIVATE ${CMAKE_SOURCE_DIR}/tests/support)
add_test(NAME qspi_tlm COMMAND test_qspi_tlm)
