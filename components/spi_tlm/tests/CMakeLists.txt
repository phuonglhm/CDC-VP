add_executable(test_spi_tlm test_spi_tlm.cpp)
target_link_libraries(test_spi_tlm PRIVATE
    cdc::components::spi_tlm
    SystemC::systemc)
target_include_directories(test_spi_tlm PRIVATE ${CMAKE_SOURCE_DIR}/tests/support)
add_test(NAME spi_tlm COMMAND test_spi_tlm)
